// Copyright 2026 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ----------------

// manual-test-brotli-fuzz.c is a differential fuzzer for std/brotli against
// the reference implementation (libbrotlidec and libbrotlienc v1.2.1). Every
// parameter is drawn from a pseudo-random number generator.
//
// Modes (the first argument):
//
//   -dec  decodes randomly mutated brotli streams with both libbrotlidec and
//         Wuffs' decoder and checks that they produce the same output bytes,
//         consume the same input (on success) and agree on the result: ok,
//         truncated input, a full dst (needs more output) or the specific
//         BROTLI_DECODER_ERROR_FORMAT_* code (see the "Status Mapping" in
//         std/brotli/README.md). Seeds are the test/data/brotli files and
//         streams that libbrotlienc or Wuffs' encoder produce (at random
//         qualities, window sizes, large window and raw dictionaries) from
//         random inputs. Mutations are bit flips, byte substitutions,
//         truncations, insertions, deletions, splices of two streams and
//         random bytes. Each stream is decoded one-shot (by both) and with
//         random I/O chunking (the same absolute chunk boundaries for both,
//         mostly 1 to 7 bytes), with a random large-window quirk, random
//         (raw and serialized) dictionaries and a random workbuf length (at
//         or above workbuf_len, or exactly the stream's minimum, or one byte
//         less than that, which must fail).
//
//   -enc  encodes random inputs with random parameters (quality, lgwin,
//         large window, lgblock, mode, size hint, npostfix, ndirect,
//         disabling literal context modeling, stream offset, raw
//         dictionaries) and random plans of PROCESS, FLUSH, EMIT_METADATA and
//         FINISH steps with random chunk sizes and dst limits, with both
//         libbrotlienc and Wuffs' encoder, and checks that the output is
//         byte-exact and that both decoders round-trip it.
//
// Options (after the mode):
//   -seed=S      the PRNG seed (default 1).
//   -n=N         the number of iterations (default 1000).
//   -secs=T      stop after about T seconds.
//   -q=LO-HI     (-enc only) restrict the quality to LO ..= HI.
//   -hq          (-enc only) allow dictionaries at qualities 10 and 11.
//   -out=DIR     where to save failing cases (default ".").
//   -data=DIR    the test/data directory (default "test/data").
//
// The libbrotlidec that it is linked with should be built with
// BROTLI_EXPERIMENTAL (so that it accepts serialized dictionaries, like
// Wuffs' decoder). Only c/common/shared_dictionary.c depends on it, so
// libbrotlienc can be the default build. For example, from the Wuffs root
// directory (as one command line):
//
//   for f in $BROTLI/c/common/*.c $BROTLI/c/dec/*.c; do
//       gcc -O2 -DBROTLI_EXPERIMENTAL -I$BROTLI/c/include -c $f
//           -o /tmp/bd/$(basename $(dirname $f))_$(basename $f .c).o; done
//   ar rcs /tmp/bd/libbrotlidecexp.a /tmp/bd/*.o
//   gcc -O2 -std=c99 -I$BROTLI/c/include script/manual-test-brotli-fuzz.c
//       $BROTLI_BUILD/libbrotlienc.a /tmp/bd/libbrotlidecexp.a -lm
//       -o /tmp/brotli-fuzz
//   /tmp/brotli-fuzz -dec -seed=1 -n=100000
//
// It prints a summary and exits with a non-zero status if there were any
// divergences. Each divergence is saved (the stream, its dictionaries and a
// description) under the -out directory.

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>

#include "brotli/decode.h"
#include "brotli/encode.h"
#include "brotli/shared_dictionary.h"

#define WUFFS_IMPLEMENTATION
#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__BROTLI
#define WUFFS_CONFIG__MODULE__SOFTFLOAT
#include "../release/c/wuffs-unsupported-snapshot.c"

// ---------------- PRNG

static uint64_t g_rng_state = 1;
static uint64_t g_seed = 1;

static uint64_t  //
r64(void) {
  // SplitMix64.
  uint64_t z = (g_rng_state += 0x9E3779B97F4A7C15ull);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

// rn returns a pseudo-random number in [0 .. n), or 0 if n is 0.
static uint64_t  //
rn(uint64_t n) {
  return n ? (r64() % n) : 0;
}

// chance returns true with probability (k / 1000).
static bool  //
chance(uint64_t k) {
  return rn(1000) < k;
}

static uint64_t  //
split64(uint64_t* state) {
  uint64_t z = (*state += 0x9E3779B97F4A7C15ull);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

// ---------------- Globals

static const char* g_out_dir = ".";
static const char* g_data_dir = "test/data";
static uint64_t g_num_failures = 0;
static uint32_t g_qmin = 0;
static uint32_t g_qmax = 11;
static bool g_hq_dicts = false;

static uint8_t*  //
xmalloc(size_t n) {
  uint8_t* p = malloc(n ? n : 1);
  if (!p) {
    fprintf(stderr, "out of memory (%zu bytes)\n", n);
    exit(2);
  }
  return p;
}

// big_alloc reserves (but does not touch) n bytes.
static uint8_t*  //
big_alloc(size_t n) {
  void* p = mmap(NULL, n, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  if (p == MAP_FAILED) {
    fprintf(stderr, "mmap of %zu bytes failed\n", n);
    exit(2);
  }
  return (uint8_t*)p;
}

// big_release gives back the physical memory of a big_alloc region.
static void  //
big_release(uint8_t* p, size_t n) {
  madvise(p, n, MADV_DONTNEED);
}

static uint8_t*  //
read_file(const char* filename, size_t* len, size_t max_len) {
  FILE* f = fopen(filename, "rb");
  if (!f) {
    return NULL;
  }
  struct stat st;
  if ((fstat(fileno(f), &st) != 0) || !S_ISREG(st.st_mode) ||
      ((size_t)st.st_size > max_len)) {
    fclose(f);
    return NULL;
  }
  size_t n = (size_t)st.st_size;
  uint8_t* p = xmalloc(n + 1);
  if (fread(p, 1, n, f) != n) {
    fclose(f);
    free(p);
    return NULL;
  }
  fclose(f);
  *len = n;
  return p;
}

static void  //
write_file(const char* filename, const uint8_t* p, size_t n) {
  FILE* f = fopen(filename, "wb");
  if (!f) {
    fprintf(stderr, "could not write %s\n", filename);
    return;
  }
  if (n) {
    fwrite(p, 1, n, f);
  }
  fclose(f);
}

// ---------------- Corpus (for inputs and dictionaries)

typedef struct {
  char name[256];
  uint8_t* ptr;
  size_t len;
} blob;

#define MAX_CORPUS 512
static blob g_corpus[MAX_CORPUS];
static size_t g_num_corpus;

static int  //
cmp_names(const void* a, const void* b) {
  return strcmp(*(const char* const*)a, *(const char* const*)b);
}

// list_dir returns the sorted names of the regular files in dir.
static size_t  //
list_dir(const char* dir, char** names, size_t max) {
  DIR* d = opendir(dir);
  if (!d) {
    return 0;
  }
  size_t n = 0;
  struct dirent* e;
  while ((e = readdir(d)) && (n < max)) {
    if (e->d_name[0] == '.') {
      continue;
    }
    names[n++] = strdup(e->d_name);
  }
  closedir(d);
  qsort(names, n, sizeof names[0], cmp_names);
  return n;
}

static void  //
load_corpus(void) {
  static const char* subdirs[] = {"", "/brotli"};
  for (int s = 0; s < 2; s++) {
    char dir[512];
    snprintf(dir, sizeof dir, "%s%s", g_data_dir, subdirs[s]);
    char* names[1024];
    size_t n = list_dir(dir, names, 1024);
    for (size_t i = 0; i < n; i++) {
      size_t k = strlen(names[i]);
      bool compressed = ((k > 3) && !strcmp(names[i] + k - 3, ".br")) ||
                        strstr(names[i], ".compressed") ||
                        strstr(names[i], ".bin");
      if ((s == 1) && compressed) {
        free(names[i]);
        continue;
      }
      char path[1024];
      snprintf(path, sizeof path, "%s/%s", dir, names[i]);
      size_t len = 0;
      uint8_t* p = read_file(path, &len, 4 << 20);
      if (p && (len > 0) && (g_num_corpus < MAX_CORPUS)) {
        blob* b = &g_corpus[g_num_corpus++];
        snprintf(b->name, sizeof b->name, "%s", path);
        b->ptr = p;
        b->len = len;
      } else {
        free(p);
      }
      free(names[i]);
    }
  }
  if (g_num_corpus == 0) {
    fprintf(stderr, "no corpus files in %s (use -data=DIR)\n", g_data_dir);
    exit(2);
  }
}

// ---------------- Input generators

// pick_len returns a pseudo-random length up to max, with an emphasis on
// short lengths.
static size_t  //
pick_len(size_t max) {
  uint64_t r = rn(1000);
  size_t n;
  if (r < 30) {
    n = rn(4);
  } else if (r < 300) {
    n = rn(256);
  } else if (r < 550) {
    n = rn(4096);
  } else if (r < 850) {
    n = rn(65536 + 1);
  } else if (r < 970) {
    n = rn(1 << 20);
  } else {
    n = rn(4 << 20);
  }
  return (n < max) ? n : max;
}

static void  //
put_utf8(uint8_t* p, size_t* i, size_t n, uint32_t c) {
  uint8_t b[4];
  size_t k;
  if (c < 0x80) {
    b[0] = (uint8_t)c;
    k = 1;
  } else if (c < 0x800) {
    b[0] = (uint8_t)(0xC0 | (c >> 6));
    b[1] = (uint8_t)(0x80 | (c & 0x3F));
    k = 2;
  } else if (c < 0x10000) {
    b[0] = (uint8_t)(0xE0 | (c >> 12));
    b[1] = (uint8_t)(0x80 | ((c >> 6) & 0x3F));
    b[2] = (uint8_t)(0x80 | (c & 0x3F));
    k = 3;
  } else {
    b[0] = (uint8_t)(0xF0 | (c >> 18));
    b[1] = (uint8_t)(0x80 | ((c >> 12) & 0x3F));
    b[2] = (uint8_t)(0x80 | ((c >> 6) & 0x3F));
    b[3] = (uint8_t)(0x80 | (c & 0x3F));
    k = 4;
  }
  for (size_t j = 0; (j < k) && (*i < n); j++) {
    p[(*i)++] = b[j];
  }
}

// gen_segment fills p[0 .. n] with one kind of data. p[-dist] (for dist up to
// prefix) may be read, for repeats of earlier data.
static int  //
gen_segment(uint8_t* p, size_t n, size_t prefix, int kind) {
  switch (kind) {
    case 0: {  // Random bytes.
      for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)r64();
      }
      break;
    }
    case 1: {  // A slice of a corpus file (text, images, binaries).
      const blob* b = &g_corpus[rn(g_num_corpus)];
      size_t off = rn(b->len);
      for (size_t i = 0; i < n; i++) {
        p[i] = b->ptr[(off + i) % b->len];
      }
      break;
    }
    case 2: {  // A repeated pattern.
      size_t period = 1 + rn(chance(500) ? 8 : 400);
      for (size_t i = 0; i < n; i++) {
        p[i] = (i < period) ? (uint8_t)r64() : p[i - period];
      }
      break;
    }
    case 3: {  // Text from a small vocabulary, runs and noise.
      static const char* words[] = {"the ",   "brotli ", "wuffs ",  "of ",
                                    "and ",   "data\n",  "time ",   "<a>",
                                    "encoder", ", ",     "zebra ",  "1234",
                                    "\xC3\xA9t\xC3\xA9 ", "\t", "{\"k\": ",
                                    "}\n"};
      size_t i = 0;
      while (i < n) {
        uint64_t r = r64();
        if ((r & 0xFF) < 200) {
          const char* w = words[(r >> 8) % 16];
          while (*w && (i < n)) {
            p[i++] = (uint8_t)(*w++);
          }
        } else if ((r & 0xFF) < 230) {
          size_t k = (size_t)((r >> 8) % 300);
          while (k-- && (i < n)) {
            p[i++] = (uint8_t)(r >> 40);
          }
        } else {
          size_t k = (size_t)((r >> 8) % 2000);
          while (k-- && (i < n)) {
            p[i++] = (uint8_t)r64();
          }
        }
      }
      break;
    }
    case 4: {  // Repeats of earlier data (near and far), with changes.
      if (prefix == 0) {
        return gen_segment(p, n, prefix, 3);
      }
      size_t i = 0;
      while (i < n) {
        size_t avail = prefix + i;
        size_t dist = 1 + (chance(300) ? rn(avail) : rn(avail < 64 ? avail : 64));
        size_t k = 4 + rn(chance(100) ? 70000 : 300);
        for (size_t j = 0; (j < k) && (i < n); j++, i++) {
          p[i] = p[(ptrdiff_t)i - (ptrdiff_t)dist];
        }
        if (i < n) {
          p[i++] = (uint8_t)r64();
        }
      }
      break;
    }
    case 5: {  // UTF-8 text (valid, sometimes invalid).
      size_t i = 0;
      while (i < n) {
        uint64_t r = r64();
        uint32_t c;
        switch (r & 7) {
          case 0:
          case 1:
          case 2:
            c = 0x20 + (uint32_t)((r >> 8) % 0x5F);
            break;
          case 3:
          case 4:
            c = 0x80 + (uint32_t)((r >> 8) % 0x780);
            break;
          case 5:
            c = 0x800 + (uint32_t)((r >> 8) % 0xD000);
            break;
          case 6:
            c = 0x10000 + (uint32_t)((r >> 8) % 0x100000);
            break;
          default:
            p[i++] = (uint8_t)(r >> 8);
            continue;
        }
        put_utf8(p, &i, n, c);
      }
      break;
    }
    case 6: {  // A small alphabet.
      uint32_t k = 1 + (uint32_t)rn(4);
      uint8_t base = (uint8_t)r64();
      for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)(base + (uint8_t)rn(k));
      }
      break;
    }
    default: {  // Zeroes.
      memset(p, 0, n);
      break;
    }
  }
  return kind;
}

// gen_input fills p[0 .. n] with a mixture of segments.
static void  //
gen_input(uint8_t* p, size_t n) {
  if (chance(400)) {
    gen_segment(p, n, 0, (int)rn(8));
    return;
  }
  size_t i = 0;
  while (i < n) {
    size_t k = 1 + rn(chance(200) ? (n - i) : 4096);
    if (k > (n - i)) {
      k = n - i;
    }
    gen_segment(p + i, k, i, (int)rn(8));
    i += k;
  }
}

// ---------------- Dictionaries

typedef struct {
  uint32_t type;  // 0 raw, 1 serialized.
  const uint8_t* ptr;
  size_t len;
  bool owned;
} dict;

#define MAX_DICTS 16

typedef struct {
  dict d[MAX_DICTS];
  size_t n;
} dict_set;

static void  //
dict_set_free(dict_set* s) {
  for (size_t i = 0; i < s->n; i++) {
    if (s->d[i].owned) {
      free((void*)s->d[i].ptr);
    }
  }
  s->n = 0;
}

static void  //
dict_set_add(dict_set* s, uint32_t type, const uint8_t* ptr, size_t len,
             bool copy) {
  if (s->n >= MAX_DICTS) {
    return;
  }
  dict* d = &s->d[s->n++];
  d->type = type;
  d->len = len;
  d->owned = copy;
  if (copy) {
    uint8_t* q = xmalloc(len);
    if (len) {
      memcpy(q, ptr, len);
    }
    d->ptr = q;
  } else {
    d->ptr = ptr;
  }
}

static void  //
dict_set_copy(dict_set* dst, const dict_set* src) {
  dst->n = 0;
  for (size_t i = 0; i < src->n; i++) {
    dict_set_add(dst, src->d[i].type, src->d[i].ptr, src->d[i].len, true);
  }
}

static size_t  //
dict_set_total(const dict_set* s) {
  size_t t = 0;
  for (size_t i = 0; i < s->n; i++) {
    t += s->d[i].len;
  }
  return t;
}

// Well-known dictionaries (see test/data/brotli/README.md).
static uint8_t* g_romeo;
static size_t g_romeo_len;
static uint8_t* g_alice;
static size_t g_alice_len;
static uint8_t* g_sd[4];  // full, words, prefix, utf8.
static size_t g_sd_len[4];

static uint8_t*  //
must_read(const char* rel, size_t* len) {
  char path[1024];
  snprintf(path, sizeof path, "%s/%s", g_data_dir, rel);
  uint8_t* p = read_file(path, len, 64 << 20);
  if (!p) {
    fprintf(stderr, "could not read %s\n", path);
    exit(2);
  }
  return p;
}

static void  //
load_dicts(void) {
  g_romeo = must_read("romeo.txt", &g_romeo_len);
  g_alice = must_read("brotli/alice29.txt", &g_alice_len);
  g_sd[0] = must_read("brotli/shared-dictionary.full.bin", &g_sd_len[0]);
  g_sd[1] = must_read("brotli/shared-dictionary.words.bin", &g_sd_len[1]);
  g_sd[2] = must_read("brotli/shared-dictionary.prefix.bin", &g_sd_len[2]);
  g_sd[3] = must_read("brotli/shared-dictionary.utf8.bin", &g_sd_len[3]);
}

// named_dict_set fills s with the dictionaries that the named test/data/brotli
// file was compressed with (and sets *lw for large-window files).
static void  //
named_dict_set(const char* name, dict_set* s, bool* lw) {
  s->n = 0;
  *lw = strstr(name, "lw") != NULL;
  if (strstr(name, "raw-romeo")) {
    dict_set_add(s, 0, g_romeo, g_romeo_len, false);
  } else if (strstr(name, "alice29.txt.") && strstr(name, "raw-chunks")) {
    dict_set_add(s, 0, g_alice, 20000, false);
    dict_set_add(s, 0, g_alice + 20000, 1, false);
    dict_set_add(s, 0, g_alice + 20001, 0, false);
    dict_set_add(s, 0, g_alice + 20001, 40000 - 1, false);
  } else if (strstr(name, "raw-chunks") || strstr(name, "invalid.raw-")) {
    dict_set_add(s, 0, g_romeo, 300, false);
    dict_set_add(s, 0, g_romeo + 300, 200, false);
    dict_set_add(s, 0, g_romeo + 500, g_romeo_len - 500, false);
  } else if (strstr(name, "sd-full") || strstr(name, "invalid.sd-")) {
    dict_set_add(s, 1, g_sd[0], g_sd_len[0], false);
  } else if (strstr(name, "sd-words")) {
    dict_set_add(s, 1, g_sd[1], g_sd_len[1], false);
  } else if (strstr(name, "sd-prefix-raw")) {
    dict_set_add(s, 1, g_sd[2], g_sd_len[2], false);
    dict_set_add(s, 0, g_romeo, 300, false);
  } else if (strstr(name, "sd-utf8")) {
    dict_set_add(s, 1, g_sd[3], g_sd_len[3], false);
  }
}

// random_dict_bytes makes a random raw dictionary of len bytes, sometimes
// based on hint (e.g. the input that will be compressed).
static uint8_t*  //
random_dict_bytes(size_t len, const uint8_t* hint, size_t hint_len) {
  uint8_t* p = xmalloc(len);
  if (hint && hint_len && chance(600)) {
    size_t off = rn(hint_len);
    for (size_t i = 0; i < len; i++) {
      p[i] = hint[(off + i) % hint_len];
    }
    // Sparse changes.
    if (len && chance(500)) {
      for (size_t k = rn(8); k > 0; k--) {
        p[rn(len)] ^= (uint8_t)(1 + rn(255));
      }
    }
  } else {
    gen_input(p, len);
  }
  return p;
}

static size_t  //
pick_dict_len(void) {
  uint64_t r = rn(1000);
  if (r < 50) {
    return 0;
  } else if (r < 400) {
    return 1 + rn(64);
  } else if (r < 800) {
    return rn(4096);
  } else if (r < 985) {
    return rn(256 << 10);
  } else if (r < 995) {
    // Around the 2 MiB hash table size boundary.
    return (2 << 20) - 8 + rn(17);
  }
  return rn(3 << 20);
}

// random_raw_dicts fills s with k random raw dictionaries.
static void  //
random_raw_dicts(dict_set* s, size_t k, const uint8_t* hint, size_t hint_len) {
  s->n = 0;
  for (size_t i = 0; i < k; i++) {
    size_t len = pick_dict_len();
    uint8_t* p = random_dict_bytes(len, hint, hint_len);
    dict_set_add(s, 0, p, len, false);
    s->d[s->n - 1].owned = true;
  }
}

static size_t  //
pick_num_dicts(void) {
  uint64_t r = rn(1000);
  if (r < 400) {
    return 0;
  } else if (r < 700) {
    return 1;
  } else if (r < 850) {
    return 2 + rn(3);
  } else if (r < 950) {
    return 5 + rn(10);
  }
  return 15 + rn(2);  // 15 or 16 (the 16th must be rejected).
}

// ---------------- Wuffs encoder and libbrotlienc drivers

#define OP_PROCESS 0
#define OP_FLUSH 1
#define OP_FINISH 2
#define OP_EMIT_METADATA 3

#define MAX_PARAMS 16

typedef struct {
  uint32_t keys[MAX_PARAMS];  // BrotliEncoderParameter values.
  uint32_t values[MAX_PARAMS];
  size_t num_params;
  size_t num_steps;
  uint32_t* step_ops;
  size_t* step_lens;
  size_t steps_cap;
  size_t dst_chunk;     // 0 means unlimited.
  bool dst_chunk_rand;  // Each call gets 1 + rn(dst_chunk) bytes of dst.
  uint64_t dst_seed;
  uint64_t workbuf_extra;  // Added to workbuf_len().min_incl (q10-11).
  bool short_workbuf;      // Use the shortest workbuf (q2-11, see below).
} enc_config;

static void  //
enc_add_param(enc_config* c, uint32_t key, uint32_t value) {
  if (c->num_params < MAX_PARAMS) {
    c->keys[c->num_params] = key;
    c->values[c->num_params] = value;
    c->num_params++;
  }
}

static uint32_t  //
enc_get_param(const enc_config* c, uint32_t key, uint32_t dflt) {
  uint32_t v = dflt;
  for (size_t j = 0; j < c->num_params; j++) {
    if (c->keys[j] == key) {
      v = c->values[j];
    }
  }
  return v;
}

static void  //
enc_add_step(enc_config* c, uint32_t op, size_t len) {
  if (c->num_steps == c->steps_cap) {
    c->steps_cap = c->steps_cap ? (2 * c->steps_cap) : 64;
    c->step_ops = realloc(c->step_ops, c->steps_cap * sizeof c->step_ops[0]);
    c->step_lens =
        realloc(c->step_lens, c->steps_cap * sizeof c->step_lens[0]);
    if (!c->step_ops || !c->step_lens) {
      fprintf(stderr, "out of memory\n");
      exit(2);
    }
  }
  c->step_ops[c->num_steps] = op;
  c->step_lens[c->num_steps] = len;
  c->num_steps++;
}

static uint32_t  //
quirk_key(uint32_t param) {
  switch (param) {
    case 0:
      return WUFFS_BROTLI__QUIRK_MODE;
    case 1:
      return WUFFS_BROTLI__QUIRK_ENCODER_QUALITY;
    case 2:
      return WUFFS_BROTLI__QUIRK_LGWIN;
    case 3:
      return WUFFS_BROTLI__QUIRK_LGBLOCK;
    case 4:
      return WUFFS_BROTLI__QUIRK_DISABLE_LITERAL_CONTEXT_MODELING;
    case 5:
      return WUFFS_BROTLI__QUIRK_SIZE_HINT;
    case 6:
      return WUFFS_BROTLI__QUIRK_LARGE_WINDOW;
    case 7:
      return WUFFS_BROTLI__QUIRK_NPOSTFIX;
    case 8:
      return WUFFS_BROTLI__QUIRK_NDIRECT;
    case 9:
      return WUFFS_BROTLI__QUIRK_STREAM_OFFSET;
  }
  return 0;
}

static uint8_t* g_enc_wb;  // The Wuffs encoder's workbuf (big_alloc).
#define ENC_WB_CAP ((size_t)3 << 30)
static size_t g_enc_wb_touched;
static const char g_too_large[] = "workbuf_len is too large";

static size_t  //
next_dst_room(const enc_config* c, uint64_t* st) {
  if (c->dst_chunk == 0) {
    return SIZE_MAX;
  } else if (c->dst_chunk_rand) {
    return 1 + (size_t)(split64(st) % c->dst_chunk);
  }
  return c->dst_chunk;
}

// short_workbuf_len mirrors the parity script's: the shortest workbuf (at
// qualities 2 to 11) that should work for an input of len bytes, given that
// workbuf_len().min_incl is wlen, or 0 if there is none.
static uint64_t  //
short_workbuf_len(const enc_config* c, size_t len, uint64_t wlen) {
  int32_t quality = (int32_t)enc_get_param(c, 1, 11);
  int32_t lgwin = (int32_t)enc_get_param(c, 2, 22);
  int32_t lgblock = (int32_t)enc_get_param(c, 3, 0);
  bool large_window = enc_get_param(c, 6, 0) != 0;
  quality = (quality < 0) ? 0 : (quality > 11) ? 11 : quality;
  if (quality < 2) {
    return 0;
  }
  large_window = large_window && (quality > 2);
  int32_t max_lgwin = large_window ? 30 : 24;
  lgwin = (lgwin < 10) ? 10 : (lgwin > max_lgwin) ? max_lgwin : lgwin;
  if (quality < 4) {
    lgblock = 14;
  } else if (lgblock == 0) {
    lgblock = 16;
    if ((quality >= 9) && (lgwin > lgblock)) {
      lgblock = (lgwin < 18) ? lgwin : 18;
    }
  } else {
    lgblock = (lgblock < 16) ? 16 : (lgblock > 24) ? 24 : lgblock;
  }
  int32_t rb_bits = 1 + ((lgwin > lgblock) ? lgwin : lgblock);
  uint64_t rb_size = (uint64_t)1 << rb_bits;
  uint64_t rb_region = 2 + rb_size + ((uint64_t)1 << lgblock) + 7;
  if (((len + 16) > rb_size) || (rb_region > wlen)) {
    return 0;
  }
  uint64_t n = wlen - rb_region + 2 + len + 16;
  return (n < wlen) ? n : 0;
}

// encode_wuffs runs the plan with Wuffs' encoder. *wlen_out is the workbuf
// length used. short_delta (0 or 1) is subtracted from the short workbuf
// length.
static const char*  //
encode_wuffs(const enc_config* c, const dict_set* dicts, const uint8_t* src_ptr,
             size_t src_len, size_t input_len, uint8_t* out, size_t out_cap,
             size_t* out_len, uint64_t short_delta, uint64_t* wlen_out) {
  *out_len = 0;
  wuffs_brotli__encoder* enc = wuffs_brotli__encoder__alloc();
  if (!enc) {
    return "alloc failed";
  }
  const char* ret = NULL;
  for (size_t j = 0; j < dicts->n; j++) {
    uint64_t n = wuffs_brotli__encoder__add_dictionary_workbuf_len(
        enc, dicts->d[j].len);
    if (n > ENC_WB_CAP) {
      ret = g_too_large;
      goto cleanup;
    }
    if (n > g_enc_wb_touched) {
      g_enc_wb_touched = n;
    }
    wuffs_base__status status = wuffs_brotli__encoder__add_dictionary(
        enc, dicts->d[j].type,
        wuffs_base__make_slice_u8((uint8_t*)dicts->d[j].ptr, dicts->d[j].len),
        wuffs_base__make_slice_u8(g_enc_wb, (size_t)n));
    if (status.repr) {
      ret = status.repr;
      goto cleanup;
    }
  }
  for (size_t j = 0; j < c->num_params; j++) {
    wuffs_base__status status = wuffs_brotli__encoder__set_quirk(
        enc, quirk_key(c->keys[j]), c->values[j]);
    if (status.repr) {
      ret = status.repr;
      goto cleanup;
    }
  }
  wuffs_base__range_ii_u64 r = wuffs_brotli__encoder__workbuf_len(enc);
  uint64_t wlen = r.min_incl;
  if (c->workbuf_extra && (r.max_incl > r.min_incl)) {
    uint64_t room = r.max_incl - r.min_incl;
    wlen += (c->workbuf_extra < room) ? c->workbuf_extra : room;
  }
  if (c->short_workbuf) {
    uint64_t s = short_workbuf_len(c, input_len, r.min_incl);
    if (s > 0) {
      wlen = s - short_delta;
    } else if (short_delta) {
      ret = "no short workbuf";
      goto cleanup;
    }
  }
  if (wlen > ENC_WB_CAP) {
    ret = g_too_large;
    goto cleanup;
  }
  *wlen_out = wlen;
  if (wlen > g_enc_wb_touched) {
    g_enc_wb_touched = wlen;
  }
  wuffs_base__slice_u8 workbuf =
      wuffs_base__make_slice_u8(g_enc_wb, (size_t)wlen);

  wuffs_base__io_buffer dst = wuffs_base__ptr_u8__writer(out, out_cap);
  wuffs_base__io_buffer src =
      wuffs_base__ptr_u8__reader((uint8_t*)src_ptr, src_len, false);
  src.meta.wi = 0;
  uint64_t dst_st = c->dst_seed;

  size_t end = 0;
  for (size_t i = 0; i < c->num_steps; i++) {
    end += c->step_lens[i];
    uint32_t op = c->step_ops[i];
    wuffs_base__status status = wuffs_base__make_status(NULL);
    if (op == OP_FLUSH) {
      status = wuffs_brotli__encoder__request_flush(enc);
    } else if (op == OP_EMIT_METADATA) {
      status = wuffs_brotli__encoder__request_emit_metadata(enc,
                                                            c->step_lens[i]);
    }
    if (status.repr) {
      ret = status.repr;
      goto cleanup;
    }
    uint64_t calls = 0;
    while (true) {
      if (++calls > (out_cap + src_len + 1000)) {
        ret = "too many transform_io calls";
        goto cleanup;
      }
      wuffs_base__io_buffer d = dst;
      size_t room = next_dst_room(c, &dst_st);
      if ((d.data.len - d.meta.wi) > room) {
        d.data.len = d.meta.wi + room;
      }
      wuffs_base__io_buffer s = src;
      s.meta.wi = end;
      s.meta.closed = (op == OP_FINISH);
      status = wuffs_brotli__encoder__transform_io(enc, &d, &s, workbuf);
      dst.meta.wi = d.meta.wi;
      src.meta.ri = s.meta.ri;
      if (status.repr == NULL) {
        if (op != OP_FINISH) {
          ret = "unexpected ok";
          goto cleanup;
        }
        break;
      } else if (status.repr == wuffs_base__suspension__short_write) {
        if (dst.meta.wi == dst.data.len) {
          ret = "dst is full";
          goto cleanup;
        }
        continue;
      } else if ((status.repr == wuffs_base__suspension__short_read) &&
                 (op != OP_FINISH) && (src.meta.ri == end)) {
        break;
      }
      ret = status.repr;
      goto cleanup;
    }
  }

cleanup:
  *out_len = dst.meta.wi;
  free(enc);
  return ret;
}

// encode_c runs the plan with libbrotlienc.
static const char*  //
encode_c(const enc_config* c, const dict_set* dicts, const uint8_t* src_ptr,
         uint8_t* out, size_t out_cap, size_t* out_len) {
  *out_len = 0;
  BrotliEncoderPreparedDictionary* prepared[MAX_DICTS] = {0};
  size_t num_prepared = 0;
  BrotliEncoderState* state = BrotliEncoderCreateInstance(NULL, NULL, NULL);
  if (!state) {
    return "BrotliEncoderCreateInstance failed";
  }
  const char* ret = NULL;
  for (size_t j = 0; j < c->num_params; j++) {
    if (!BrotliEncoderSetParameter(state, (BrotliEncoderParameter)(c->keys[j]),
                                   c->values[j])) {
      ret = "BrotliEncoderSetParameter failed";
      goto cleanup;
    }
  }
  for (size_t j = 0; j < dicts->n; j++) {
    prepared[j] = BrotliEncoderPrepareDictionary(
        (BrotliSharedDictionaryType)dicts->d[j].type, dicts->d[j].len,
        dicts->d[j].ptr, 11, NULL, NULL, NULL);
    if (!prepared[j]) {
      ret = "BrotliEncoderPrepareDictionary failed";
      goto cleanup;
    }
    num_prepared++;
    if (!BrotliEncoderAttachPreparedDictionary(state, prepared[j])) {
      ret = "BrotliEncoderAttachPreparedDictionary failed";
      goto cleanup;
    }
  }
  const uint8_t* next_in = src_ptr;
  uint64_t dst_st = c->dst_seed;
  for (size_t i = 0; i < c->num_steps; i++) {
    size_t avail_in = c->step_lens[i];
    BrotliEncoderOperation op = (BrotliEncoderOperation)(c->step_ops[i]);
    uint64_t calls = 0;
    while (true) {
      if (++calls > (out_cap + c->step_lens[i] + 1000)) {
        ret = "too many BrotliEncoderCompressStream calls";
        goto cleanup;
      }
      size_t avail_out = out_cap - *out_len;
      size_t room = next_dst_room(c, &dst_st);
      if (avail_out > room) {
        avail_out = room;
      }
      size_t avail_out0 = avail_out;
      uint8_t* next_out = out + *out_len;
      if (!BrotliEncoderCompressStream(state, op, &avail_in, &next_in,
                                       &avail_out, &next_out, NULL)) {
        ret = "BrotliEncoderCompressStream failed";
        goto cleanup;
      }
      *out_len += avail_out0 - avail_out;
      if (op == BROTLI_OPERATION_FINISH) {
        if (BrotliEncoderIsFinished(state)) {
          break;
        }
      } else if ((avail_in == 0) && !BrotliEncoderHasMoreOutput(state)) {
        break;
      }
      if (*out_len == out_cap) {
        ret = "dst is full";
        goto cleanup;
      }
    }
  }

cleanup:
  BrotliEncoderDestroyInstance(state);
  for (size_t j = 0; j < num_prepared; j++) {
    BrotliEncoderDestroyPreparedDictionary(prepared[j]);
  }
  return ret;
}

// ---------------- Decoder drivers

// Result classes.
#define CLS_OK 1
#define CLS_NEEDS_MORE_INPUT 2
#define CLS_NEEDS_MORE_OUTPUT 3
#define CLS_BUG (-1000)
// Error classes are the (negative) BrotliDecoderErrorCode values.

typedef struct {
  int cls;
  size_t out_n;
  size_t in_used;
  const char* wuffs_status;
  uint64_t calls;
} dec_result;

static int  //
wuffs_status_class(const char* s) {
  static const struct {
    const char* status;
    int code;
  } table[] = {
      {wuffs_brotli__error__bad_exuberant_nibble, -1},
      {wuffs_brotli__error__bad_reserved_bit, -2},
      {wuffs_brotli__error__bad_exuberant_metadata_nibble, -3},
      {wuffs_brotli__error__bad_simple_huffman_alphabet, -4},
      {wuffs_brotli__error__bad_simple_huffman_symbol, -5},
      {wuffs_brotli__error__bad_code_length_code_lengths, -6},
      {wuffs_brotli__error__bad_huffman_space, -7},
      {wuffs_brotli__error__bad_context_map_repeat, -8},
      {wuffs_brotli__error__bad_block_length, -9},
      {wuffs_brotli__error__bad_metablock_length, -10},
      {wuffs_brotli__error__bad_transform, -11},
      {wuffs_brotli__error__bad_dictionary, -12},
      {wuffs_brotli__error__bad_window_bits, -13},
      {wuffs_brotli__error__unsupported_large_window, -13},
      {wuffs_brotli__error__bad_padding, -14},
      {wuffs_brotli__error__bad_final_padding, -15},
      {wuffs_brotli__error__bad_distance, -16},
      {wuffs_brotli__error__bad_block_switch, -17},
      {wuffs_brotli__error__bad_compound_dictionary, -18},
      {wuffs_brotli__error__truncated_input, CLS_NEEDS_MORE_INPUT},
  };
  if (!s) {
    return CLS_OK;
  }
  for (size_t i = 0; i < sizeof table / sizeof table[0]; i++) {
    if (table[i].status == s) {
      return table[i].code;
    }
  }
  return CLS_BUG;
}

// A chunking schedule: when a decoder runs out of input (or output room),
// the next chunk boundary is the previous one plus next_chunk. Both decoders
// use the same sequence of boundaries.
typedef struct {
  bool one_shot;
  uint64_t in_seed;
  uint64_t out_seed;
  int in_style;
  int out_style;
} chunking;

static size_t  //
next_chunk(uint64_t* st, int style) {
  uint64_t r = split64(st);
  switch (style) {
    case 0:  // 1 to 7 bytes.
      return 1 + (size_t)((r >> 8) % 7);
    case 1:  // Exactly 1 byte.
      return 1;
    case 2:  // Mostly small, sometimes large.
      if ((r & 0xFF) < 0xE0) {
        return 1 + (size_t)((r >> 8) % 7);
      }
      return 1 + (size_t)((r >> 8) % 70000);
    default:  // 1 to 300 bytes.
      return 1 + (size_t)((r >> 8) % 300);
  }
}

typedef struct {
  bool large_window;
  dict_set dicts;     // The dictionaries attached to both decoders.
  uint64_t wb_len;    // The Wuffs workbuf length.
  bool expect_bad_wb;  // wb_len is one byte too short for the stream.
  bool uninit;        // WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED.
  size_t out_cap;
} dec_config;

static uint8_t* g_dec_wb;  // big_alloc.
#define DEC_WB_CAP (((size_t)1 << 30) + ((size_t)64 << 20))
static wuffs_brotli__decoder* g_dec;

static const char*  //
wuffs_attach(wuffs_brotli__decoder* dec, const dec_config* dc, size_t* n_ok) {
  wuffs_base__status status = wuffs_brotli__decoder__initialize(
      dec, sizeof__wuffs_brotli__decoder(), WUFFS_VERSION,
      dc->uninit ? WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED : 0);
  if (status.repr) {
    return status.repr;
  }
  if (dc->large_window) {
    status = wuffs_brotli__decoder__set_quirk(
        dec, WUFFS_BROTLI__QUIRK_LARGE_WINDOW, 1);
    if (status.repr) {
      return status.repr;
    }
  }
  *n_ok = 0;
  for (size_t i = 0; i < dc->dicts.n; i++) {
    status = wuffs_brotli__decoder__add_dictionary(
        dec, dc->dicts.d[i].type,
        wuffs_base__make_slice_u8((uint8_t*)dc->dicts.d[i].ptr,
                                  dc->dicts.d[i].len),
        wuffs_base__make_slice_u8(g_dec_wb, DEC_WB_CAP));
    if (status.repr) {
      return status.repr;
    }
    (*n_ok)++;
  }
  return NULL;
}

static dec_result  //
wuffs_decode(const dec_config* dc,
             const chunking* ch,
             const uint8_t* in,
             size_t n,
             uint8_t* out) {
  dec_result res = {0};
  size_t n_ok = 0;
  const char* err = wuffs_attach(g_dec, dc, &n_ok);
  if (err) {
    res.cls = CLS_BUG;
    res.wuffs_status = err;
    return res;
  }
  wuffs_base__slice_u8 wb =
      wuffs_base__make_slice_u8(g_dec_wb, (size_t)dc->wb_len);
  uint64_t in_st = ch->in_seed;
  uint64_t out_st = ch->out_seed;
  size_t lin = ch->one_shot ? n : next_chunk(&in_st, ch->in_style);
  size_t lout = ch->one_shot ? dc->out_cap : next_chunk(&out_st, ch->out_style);
  lin = (lin < n) ? lin : n;
  lout = (lout < dc->out_cap) ? lout : dc->out_cap;
  size_t ri = 0;
  size_t wi = 0;
  uint64_t max_calls = 3 * ((uint64_t)n + dc->out_cap) + 100;
  while (true) {
    if (++res.calls > max_calls) {
      res.cls = CLS_BUG;
      res.wuffs_status = "too many transform_io calls (a loop?)";
      break;
    }
    wuffs_base__io_buffer src =
        wuffs_base__ptr_u8__reader((uint8_t*)in, lin, lin == n);
    src.meta.ri = ri;
    wuffs_base__io_buffer dst = wuffs_base__ptr_u8__writer(out, lout);
    dst.meta.wi = wi;
    wuffs_base__status status =
        wuffs_brotli__decoder__transform_io(g_dec, &dst, &src, wb);
    if ((src.meta.ri < ri) || (dst.meta.wi < wi)) {
      res.cls = CLS_BUG;
      res.wuffs_status = "went backwards";
      break;
    }
    ri = src.meta.ri;
    wi = dst.meta.wi;
    res.wuffs_status = status.repr;
    if (status.repr == wuffs_base__suspension__short_read) {
      if (lin < n) {
        lin += next_chunk(&in_st, ch->in_style);
        lin = (lin < n) ? lin : n;
        continue;
      }
      res.cls = CLS_BUG;  // A short read with a closed src.
      break;
    } else if (status.repr == wuffs_base__suspension__short_write) {
      if (lout < dc->out_cap) {
        lout += next_chunk(&out_st, ch->out_style);
        lout = (lout < dc->out_cap) ? lout : dc->out_cap;
        continue;
      }
      res.cls = CLS_NEEDS_MORE_OUTPUT;
      break;
    }
    if (status.repr == wuffs_base__error__bad_workbuf_length) {
      res.cls = -100;
    } else {
      res.cls = wuffs_status_class(status.repr);
    }
    break;
  }
  res.out_n = wi;
  res.in_used = ri;
  return res;
}

static dec_result  //
c_decode(const dec_config* dc,
         const chunking* ch,
         const uint8_t* in,
         size_t n,
         uint8_t* out,
         size_t* c_attached) {
  dec_result res = {0};
  BrotliDecoderState* s = BrotliDecoderCreateInstance(NULL, NULL, NULL);
  if (!s) {
    res.cls = CLS_BUG;
    return res;
  }
  if (dc->large_window) {
    BrotliDecoderSetParameter(s, BROTLI_DECODER_PARAM_LARGE_WINDOW, 1);
  }
  *c_attached = 0;
  for (size_t i = 0; i < dc->dicts.n; i++) {
    if (!BrotliDecoderAttachDictionary(
            s, (BrotliSharedDictionaryType)dc->dicts.d[i].type,
            dc->dicts.d[i].len, dc->dicts.d[i].ptr)) {
      break;
    }
    (*c_attached)++;
  }
  if (*c_attached != dc->dicts.n) {
    BrotliDecoderDestroyInstance(s);
    res.cls = CLS_BUG;
    return res;
  }
  uint64_t in_st = ch->in_seed;
  uint64_t out_st = ch->out_seed;
  size_t lin = ch->one_shot ? n : next_chunk(&in_st, ch->in_style);
  size_t lout = ch->one_shot ? dc->out_cap : next_chunk(&out_st, ch->out_style);
  lin = (lin < n) ? lin : n;
  lout = (lout < dc->out_cap) ? lout : dc->out_cap;
  size_t ri = 0;
  size_t wi = 0;
  while (true) {
    res.calls++;
    size_t avail_in = lin - ri;
    const uint8_t* next_in = in + ri;
    size_t avail_out = lout - wi;
    uint8_t* next_out = out + wi;
    BrotliDecoderResult r = BrotliDecoderDecompressStream(
        s, &avail_in, &next_in, &avail_out, &next_out, NULL);
    ri = lin - avail_in;
    wi = lout - avail_out;
    if (r == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT) {
      if (lin < n) {
        lin += next_chunk(&in_st, ch->in_style);
        lin = (lin < n) ? lin : n;
        continue;
      }
      res.cls = CLS_NEEDS_MORE_INPUT;
    } else if (r == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT) {
      if (lout < dc->out_cap) {
        lout += next_chunk(&out_st, ch->out_style);
        lout = (lout < dc->out_cap) ? lout : dc->out_cap;
        continue;
      }
      res.cls = CLS_NEEDS_MORE_OUTPUT;
    } else if (r == BROTLI_DECODER_RESULT_SUCCESS) {
      res.cls = CLS_OK;
    } else {
      res.cls = (int)BrotliDecoderGetErrorCode(s);
    }
    break;
  }
  BrotliDecoderDestroyInstance(s);
  res.out_n = wi;
  res.in_used = ri;
  return res;
}

// stream_window_bits parses a brotli stream header's WBITS, returning 0 if
// it is invalid or truncated. It mirrors DecodeWindowBits in decode.c.
static uint32_t  //
stream_window_bits(const uint8_t* p, size_t n, bool large_window) {
  uint64_t bits = 0;
  for (size_t i = 0; (i < 8) && (i < n); i++) {
    bits |= ((uint64_t)p[i]) << (8 * i);
  }
  size_t nbits = (n < 8) ? (8 * n) : 64;
  if (nbits < 1) {
    return 0;
  } else if ((bits & 1) == 0) {
    return 16;
  } else if (nbits < 4) {
    return 0;
  }
  uint32_t k = (uint32_t)((bits >> 1) & 7);
  if (k != 0) {
    return 17 + k;
  } else if (nbits < 7) {
    return 0;
  }
  k = (uint32_t)((bits >> 4) & 7);
  if (k == 1) {
    if (!large_window || (nbits < 14) || ((bits >> 7) & 1)) {
      return 0;
    }
    k = (uint32_t)((bits >> 8) & 63);
    return ((k < 10) || (k > 30)) ? 0 : k;
  } else if (k != 0) {
    return 8 + k;
  }
  return 17;
}

// ---------------- Failure reporting

static void  //
save_failure(const char* kind,
             const char* desc,
             const uint8_t* stream,
             size_t stream_len,
             const dict_set* dicts) {
  g_num_failures++;
  char path[1024];
  snprintf(path, sizeof path, "%s/fail-%s-%" PRIu64 "-%d.br", g_out_dir, kind,
           g_rng_state, (int)g_num_failures);
  write_file(path, stream, stream_len);
  char dpath[1100];
  for (size_t i = 0; dicts && (i < dicts->n); i++) {
    snprintf(dpath, sizeof dpath, "%s.dict%zu.%s", path, i,
             dicts->d[i].type ? "sd" : "raw");
    write_file(dpath, dicts->d[i].ptr, dicts->d[i].len);
  }
  snprintf(dpath, sizeof dpath, "%s.txt", path);
  write_file(dpath, (const uint8_t*)desc, strlen(desc));
  printf("FAILURE (%s) %s\n  saved %s\n", kind, desc, path);
  fflush(stdout);
}

// ---------------- Decoder campaign

typedef struct {
  char name[256];
  uint8_t* ptr;
  size_t len;
  dict_set dicts;
  bool large_window;
} seed;

#define MAX_FILE_SEEDS 256
#define MAX_GEN_SEEDS 256
static seed g_file_seeds[MAX_FILE_SEEDS];
static size_t g_num_file_seeds;
static seed g_gen_seeds[MAX_GEN_SEEDS];
static size_t g_num_gen_seeds;

static uint64_t g_dec_inputs;
static uint64_t g_dec_decodes;
static uint64_t g_dec_cls_counts[2][32];  // [one-shot, chunked][class].
static uint64_t g_dec_bad_wb_checks;
static uint64_t g_dec_exact_wb;
static uint64_t g_dec_dict_rejections;
static uint64_t g_dec_lw;
static uint64_t g_dec_with_dicts;
static uint64_t g_dec_gen_streams;
static uint64_t g_dec_gen_wuffs;
static uint64_t g_dec_wuffs_shorter_output;

static int  //
cls_index(int cls) {
  if (cls >= 1 && cls <= 3) {
    return cls;
  } else if (cls <= -1 && cls >= -18) {
    return 3 - cls;  // 4 ..= 21.
  } else if (cls == -100) {
    return 22;
  }
  return 0;
}

static void  //
load_file_seeds(void) {
  char dir[512];
  snprintf(dir, sizeof dir, "%s/brotli", g_data_dir);
  char* names[1024];
  size_t n = list_dir(dir, names, 1024);
  for (size_t i = 0; i < n; i++) {
    size_t k = strlen(names[i]);
    bool want = ((k > 3) && !strcmp(names[i] + k - 3, ".br")) ||
                strstr(names[i], ".compressed");
    if (want && (g_num_file_seeds < MAX_FILE_SEEDS)) {
      char path[1024];
      snprintf(path, sizeof path, "%s/%s", dir, names[i]);
      size_t len = 0;
      uint8_t* p = read_file(path, &len, 16 << 20);
      if (p) {
        seed* s = &g_file_seeds[g_num_file_seeds++];
        snprintf(s->name, sizeof s->name, "%s", names[i]);
        s->ptr = p;
        s->len = len;
        named_dict_set(names[i], &s->dicts, &s->large_window);
      }
    }
    free(names[i]);
  }
}

// make_gen_seed compresses a random input with random parameters (with
// libbrotlienc or, sometimes, Wuffs' encoder) and checks that libbrotlidec
// decodes it.
static void  //
make_gen_seed(seed* s, uint8_t* tmp_in, uint8_t* tmp_out, size_t tmp_cap) {
  free(s->ptr);
  dict_set_free(&s->dicts);
  size_t len = pick_len(256 << 10);
  gen_input(tmp_in, len);

  enc_config c;
  memset(&c, 0, sizeof c);
  uint32_t q = (uint32_t)rn(12);
  if ((q >= 10) && (len > (64 << 10))) {
    q = (uint32_t)rn(10);
  }
  bool lw = (q >= 3) && chance(300);
  uint32_t lgwin = 10 + (uint32_t)rn(lw ? 21 : 15);
  enc_add_param(&c, 1, q);
  enc_add_param(&c, 2, lgwin);
  if (lw) {
    enc_add_param(&c, 6, 1);
  }
  if (chance(300)) {
    enc_add_param(&c, 7, (uint32_t)rn(4));
    enc_add_param(&c, 8, (uint32_t)rn(16) << (uint32_t)rn(4));
  }
  dict_set ds = {0};
  if ((q < 10) && chance(400)) {
    random_raw_dicts(&ds, 1 + rn(chance(100) ? 15 : 3), tmp_in, len);
  }
  // A plan: random PROCESS chunks with occasional flushes and metadata.
  size_t meta_total = 0;
  size_t i = 0;
  while (true) {
    if (chance(30)) {
      size_t m = rn(chance(100) ? 70000 : 64);
      if ((len + meta_total + m) > tmp_cap) {
        m = 0;
      }
      enc_add_step(&c, OP_EMIT_METADATA, m);
      memmove(tmp_in + i + m, tmp_in + i, len + meta_total - i);
      for (size_t j = 0; j < m; j++) {
        tmp_in[i + j] = (uint8_t)r64();
      }
      i += m;
      meta_total += m;
    }
    size_t k = chance(300) ? (len + meta_total - i) : (1 + rn(5000));
    if (k > (len + meta_total - i)) {
      k = len + meta_total - i;
    }
    i += k;
    if (i == (len + meta_total)) {
      enc_add_step(&c, OP_FINISH, k);
      break;
    }
    enc_add_step(&c, chance(100) ? OP_FLUSH : OP_PROCESS, k);
  }

  size_t out_len = 0;
  const char* err;
  bool use_wuffs = (q < 10) && chance(250);
  if (use_wuffs) {
    uint64_t wlen = 0;
    err = encode_wuffs(&c, &ds, tmp_in, len + meta_total, len, tmp_out,
                       tmp_cap, &out_len, 0, &wlen);
    g_dec_gen_wuffs++;
  } else {
    err = encode_c(&c, &ds, tmp_in, tmp_out, tmp_cap, &out_len);
  }
  free(c.step_ops);
  free(c.step_lens);
  if (err) {
    fprintf(stderr, "make_gen_seed: %s encoder: %s\n", use_wuffs ? "Wuffs" : "C",
            err);
    exit(2);
  }
  g_dec_gen_streams++;
  s->ptr = xmalloc(out_len);
  memcpy(s->ptr, tmp_out, out_len);
  s->len = out_len;
  s->large_window = lw;
  s->dicts = ds;
  snprintf(s->name, sizeof s->name, "gen(q=%u lgwin=%u lw=%d dicts=%zu)", q,
           lgwin, (int)lw, ds.n);
}

static size_t  //
mutate(uint8_t* m, size_t n, size_t cap, const seed* other) {
  int k = 1 + (int)rn(chance(800) ? 3 : 12);
  for (int j = 0; j < k; j++) {
    switch (rn(10)) {
      case 0:
      case 1:
      case 2:  // Bit flip.
        if (n) {
          m[rn(n)] ^= (uint8_t)(1u << rn(8));
        }
        break;
      case 3:  // Byte substitution.
        if (n) {
          m[rn(n)] = (uint8_t)r64();
        }
        break;
      case 4:  // Truncation.
        if (n) {
          n = rn(n + 1);
        }
        break;
      case 5: {  // Insertion.
        size_t len = 1 + rn(chance(800) ? 4 : 64);
        if ((n + len) > cap) {
          break;
        }
        size_t p = rn(n + 1);
        memmove(m + p + len, m + p, n - p);
        for (size_t i = 0; i < len; i++) {
          m[p + i] = (uint8_t)r64();
        }
        n += len;
        break;
      }
      case 6: {  // Deletion.
        if (!n) {
          break;
        }
        size_t p = rn(n);
        size_t len = 1 + rn(chance(800) ? 4 : 64);
        if (len > (n - p)) {
          len = n - p;
        }
        memmove(m + p, m + p + len, n - p - len);
        n -= len;
        break;
      }
      case 7: {  // Splice: our prefix and the other stream's suffix.
        if (!other || !other->len) {
          break;
        }
        size_t p = rn(n + 1);
        size_t q = rn(other->len);
        size_t len = other->len - q;
        if ((p + len) > cap) {
          len = cap - p;
        }
        memcpy(m + p, other->ptr + q, len);
        n = p + len;
        break;
      }
      case 8: {  // Overwrite a range with random bytes.
        if (!n) {
          break;
        }
        size_t p = rn(n);
        size_t len = 1 + rn(16);
        for (size_t i = 0; (i < len) && ((p + i) < n); i++) {
          m[p + i] = (uint8_t)r64();
        }
        break;
      }
      default: {  // Small arithmetic change.
        if (n) {
          m[rn(n)] += (uint8_t)(rn(2) ? 1 : 0xFF);
        }
        break;
      }
    }
  }
  return n;
}

// pick_dec_dicts chooses the dictionaries to attach: usually the seed's own,
// sometimes random ones (raw and serialized, possibly mutated).
static void  //
pick_dec_dicts(dict_set* out, const seed* s) {
  out->n = 0;
  if (chance(700)) {
    dict_set_copy(out, &s->dicts);
    return;
  }
  size_t k = pick_num_dicts();
  for (size_t i = 0; i < k; i++) {
    if (chance(250)) {
      size_t which = rn(4);
      size_t len = g_sd_len[which];
      uint8_t* p = xmalloc(len + 64);
      memcpy(p, g_sd[which], len);
      if (chance(300)) {
        len = mutate(p, len, len + 64, NULL);
      }
      dict_set_add(out, 1, p, len, false);
      out->d[out->n - 1].owned = true;
    } else if (chance(300)) {
      size_t off = rn(g_romeo_len);
      size_t len = rn(g_romeo_len - off + 1);
      dict_set_add(out, 0, g_romeo + off, len, true);
    } else {
      size_t len = pick_dict_len();
      if (len > (256 << 10)) {
        len = rn(256 << 10);
      }
      uint8_t* p = random_dict_bytes(len, NULL, 0);
      dict_set_add(out, 0, p, len, false);
      out->d[out->n - 1].owned = true;
    }
  }
}

static void  //
describe_dec(char* buf, size_t cap, const char* what, const seed* s,
             const dec_config* dc, const chunking* ch, const dec_result* w,
             const dec_result* c, size_t n) {
  snprintf(buf, cap,
           "%s: seed=%s stream_len=%zu lw=%d dicts=%zu (total %zu) wb_len=%" PRIu64
           " expect_bad_wb=%d uninit=%d out_cap=%zu one_shot=%d in_seed=0x%" PRIX64
           " in_style=%d out_seed=0x%" PRIX64 " out_style=%d | wuffs: cls=%d "
           "status=%s out=%zu in=%zu calls=%" PRIu64 " | c: cls=%d out=%zu in=%zu\n",
           what, s->name, n, (int)dc->large_window, dc->dicts.n,
           dict_set_total(&dc->dicts), dc->wb_len, (int)dc->expect_bad_wb,
           (int)dc->uninit, dc->out_cap, (int)ch->one_shot, ch->in_seed,
           ch->in_style, ch->out_seed, ch->out_style, w ? w->cls : 0,
           (w && w->wuffs_status) ? w->wuffs_status : "ok", w ? w->out_n : 0,
           w ? w->in_used : 0, w ? w->calls : 0, c ? c->cls : 0,
           c ? c->out_n : 0, c ? c->in_used : 0);
}

// compare_dec returns NULL if the two results agree.
static const char*  //
compare_dec(const dec_result* w, const dec_result* c, const uint8_t* wout,
            const uint8_t* cout, size_t out_cap) {
  if (w->cls == CLS_BUG) {
    return "Wuffs returned an unexpected status";
  } else if (c->cls == CLS_BUG) {
    return "libbrotlidec setup failed";
  } else if (w->cls != c->cls) {
    return "different result classes";
  }
  if ((w->cls == CLS_OK) || (w->cls == CLS_NEEDS_MORE_OUTPUT)) {
    if (w->out_n != c->out_n) {
      return "different output lengths";
    } else if (w->out_n && memcmp(wout, cout, w->out_n)) {
      return "different output bytes";
    } else if ((w->cls == CLS_OK) && (w->in_used != c->in_used)) {
      return "different input consumption";
    } else if ((w->cls == CLS_NEEDS_MORE_OUTPUT) && (w->out_n != out_cap)) {
      return "needs more output but dst is not full";
    }
    return NULL;
  }
  size_t m = (w->out_n < c->out_n) ? w->out_n : c->out_n;
  if (m && memcmp(wout, cout, m)) {
    return "different output bytes (common prefix)";
  }
  if (w->out_n < c->out_n) {
    g_dec_wuffs_shorter_output++;
  }
  return NULL;
}

static int  //
run_dec(uint64_t iters, double secs) {
  load_corpus();
  load_dicts();
  load_file_seeds();
  g_dec_wb = big_alloc(DEC_WB_CAP);
  g_enc_wb = big_alloc(ENC_WB_CAP);
  g_dec = (wuffs_brotli__decoder*)xmalloc(sizeof__wuffs_brotli__decoder());
  const size_t tmp_cap = (8 << 20);
  uint8_t* tmp_in = xmalloc(tmp_cap);
  uint8_t* tmp_out = xmalloc(tmp_cap);
  const size_t max_out = (2 << 20) + 4096;
  uint8_t* wout = xmalloc(max_out);
  uint8_t* cout = xmalloc(max_out);
  const size_t mcap = (1 << 20);
  uint8_t* m = xmalloc(mcap);

  // Fill the generated seed pool.
  for (size_t i = 0; i < 32; i++) {
    make_gen_seed(&g_gen_seeds[g_num_gen_seeds++], tmp_in, tmp_out, tmp_cap);
  }

  struct timespec t0;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  uint64_t it = 0;
  for (; it < iters; it++) {
    if ((it & 63) == 0) {
      struct timespec t1;
      clock_gettime(CLOCK_MONOTONIC, &t1);
      double elapsed =
          (double)(t1.tv_sec - t0.tv_sec) + 1e-9 * (double)(t1.tv_nsec - t0.tv_nsec);
      if ((secs > 0) && (elapsed > secs)) {
        break;
      }
      if (g_dec_wb && ((it & 4095) == 0)) {
        big_release(g_dec_wb, DEC_WB_CAP);
        big_release(g_enc_wb, ENC_WB_CAP);
      }
    }
    if ((it % 16) == 0) {
      size_t slot = (g_num_gen_seeds < MAX_GEN_SEEDS) ? g_num_gen_seeds++
                                                      : (size_t)rn(MAX_GEN_SEEDS);
      make_gen_seed(&g_gen_seeds[slot], tmp_in, tmp_out, tmp_cap);
    }

    // Pick a seed and mutate it.
    const seed* s = chance(500) ? &g_file_seeds[rn(g_num_file_seeds)]
                                : &g_gen_seeds[rn(g_num_gen_seeds)];
    const seed* other = chance(500) ? &g_file_seeds[rn(g_num_file_seeds)]
                                    : &g_gen_seeds[rn(g_num_gen_seeds)];
    size_t n;
    if (chance(20)) {
      n = rn(chance(500) ? 32 : 4096);
      for (size_t i = 0; i < n; i++) {
        m[i] = (uint8_t)r64();
      }
    } else {
      n = (s->len < mcap) ? s->len : mcap;
      memcpy(m, s->ptr, n);
      if (!chance(30)) {  // 3% are unmutated.
        n = mutate(m, n, mcap, other);
      }
    }
    g_dec_inputs++;

    dec_config dc;
    memset(&dc, 0, sizeof dc);
    dc.large_window = chance(800) ? s->large_window : chance(500);
    g_dec_lw += dc.large_window;
    pick_dec_dicts(&dc.dicts, s);
    g_dec_with_dicts += (dc.dicts.n > 0);
    dc.uninit = chance(500);
    dc.out_cap = chance(900) ? ((size_t)1 << 20) + rn(1 << 20) : rn(4096);

    // Attach the dictionaries to both decoders, checking that they agree on
    // which ones are accepted. libbrotlidec can be left in a modified state
    // after a failure (see std/brotli/README.md), so only the dictionaries
    // before the first failure are used.
    {
      size_t w_ok = 0;
      const char* werr = wuffs_attach(g_dec, &dc, &w_ok);
      BrotliDecoderState* cs = BrotliDecoderCreateInstance(NULL, NULL, NULL);
      size_t c_ok = 0;
      for (size_t i = 0; i < dc.dicts.n; i++) {
        if (!BrotliDecoderAttachDictionary(
                cs, (BrotliSharedDictionaryType)dc.dicts.d[i].type,
                dc.dicts.d[i].len, dc.dicts.d[i].ptr)) {
          break;
        }
        c_ok++;
      }
      BrotliDecoderDestroyInstance(cs);
      if (w_ok != c_ok) {
        char desc[2048];
        snprintf(desc, sizeof desc,
                 "add_dictionary: Wuffs accepted %zu (then \"%s\"), "
                 "libbrotlidec accepted %zu, of %zu dictionaries (seed %s)\n",
                 w_ok, werr ? werr : "", c_ok, dc.dicts.n, s->name);
        save_failure("dict", desc, m, n, &dc.dicts);
      }
      if (w_ok < dc.dicts.n) {
        g_dec_dict_rejections++;
        size_t keep = (w_ok < c_ok) ? w_ok : c_ok;
        for (size_t i = keep; i < dc.dicts.n; i++) {
          if (dc.dicts.d[i].owned) {
            free((void*)dc.dicts.d[i].ptr);
          }
        }
        dc.dicts.n = keep;
      }
    }

    // The workbuf length.
    {
      size_t w_ok = 0;
      wuffs_attach(g_dec, &dc, &w_ok);
      uint64_t wlen = wuffs_brotli__decoder__workbuf_len(g_dec).max_incl;
      uint32_t wbits = stream_window_bits(m, n, dc.large_window);
      uint64_t exact = wbits ? (dict_set_total(&dc.dicts) +
                                ((uint64_t)1 << wbits) + 542)
                             : 0;
      uint64_t r = rn(1000);
      if (exact && (r < 100)) {
        dc.wb_len = exact;
        g_dec_exact_wb++;
      } else if (exact && (r < 130)) {
        dc.wb_len = exact - 1;
        dc.expect_bad_wb = true;
        g_dec_bad_wb_checks++;
      } else if (r < 600) {
        dc.wb_len = wlen;
      } else {
        dc.wb_len = wlen + rn(65536);
      }
      if (dc.wb_len > DEC_WB_CAP) {
        dc.wb_len = wlen;
        dc.expect_bad_wb = false;
      }
    }

    for (int pass = 0; pass < 2; pass++) {
      chunking ch;
      memset(&ch, 0, sizeof ch);
      ch.one_shot = (pass == 0);
      ch.in_seed = r64();
      ch.out_seed = r64();
      ch.in_style = (int)rn(4);
      ch.out_style = (int)rn(4);
      size_t c_attached = 0;
      dec_result w = wuffs_decode(&dc, &ch, m, n, wout);
      dec_result c = c_decode(&dc, &ch, m, n, cout, &c_attached);
      g_dec_decodes += 2;
      g_dec_cls_counts[pass][cls_index(c.cls)]++;
      const char* why = NULL;
      if (dc.expect_bad_wb) {
        // The workbuf is one byte too short for the window. Wuffs must fail
        // with "#bad workbuf length" unless the stream fails (or is
        // truncated) before the window size is known, which can only be the
        // case for a stream with an invalid or truncated header. Since
        // stream_window_bits succeeded, the header is valid and complete.
        if (w.cls != -100) {
          why = "a too-short workbuf did not fail";
        }
      } else {
        why = compare_dec(&w, &c, wout, cout, dc.out_cap);
      }
      if (why) {
        char desc[4096];
        describe_dec(desc, sizeof desc, why, s, &dc, &ch, &w, &c, n);
        save_failure("dec", desc, m, n, &dc.dicts);
      }
    }
    dict_set_free(&dc.dicts);
  }

  printf("decoder campaign: seed=%" PRIu64 " inputs=%" PRIu64 " decodes=%" PRIu64
         " failures=%" PRIu64 "\n",
         g_seed, g_dec_inputs, g_dec_decodes, g_num_failures);
  printf("  generated seed streams=%" PRIu64 " (%" PRIu64
         " by Wuffs' encoder), large window=%" PRIu64 ", with dicts=%" PRIu64
         ", dict rejections=%" PRIu64 ", exact workbuf=%" PRIu64
         ", too-short workbuf=%" PRIu64 ", wuffs shorter output on error=%" PRIu64
         "\n",
         g_dec_gen_streams, g_dec_gen_wuffs, g_dec_lw, g_dec_with_dicts,
         g_dec_dict_rejections, g_dec_exact_wb, g_dec_bad_wb_checks,
         g_dec_wuffs_shorter_output);
  static const char* names[23] = {
      "other",     "ok",          "needs-more-input", "needs-more-output",
      "-1",        "-2",          "-3",               "-4",
      "-5",        "-6",          "-7",               "-8",
      "-9",        "-10",         "-11",              "-12",
      "-13",       "-14",         "-15",              "-16",
      "-17",       "-18",         "bad-workbuf"};
  for (int pass = 0; pass < 2; pass++) {
    printf("  %s classes (C):", pass ? "chunked" : "one-shot");
    for (int i = 0; i < 23; i++) {
      if (g_dec_cls_counts[pass][i]) {
        printf(" %s=%" PRIu64, names[i], g_dec_cls_counts[pass][i]);
      }
    }
    printf("\n");
  }
  return g_num_failures ? 1 : 0;
}

// ---------------- Encoder campaign

static int run_enc(uint64_t iters, double secs);

int  //
main(int argc, char** argv) {
  if (BrotliEncoderVersion() != 0x1002001 ||
      BrotliDecoderVersion() != 0x1002001) {
    fprintf(stderr, "want libbrotli v1.2.1, have enc 0x%x dec 0x%x\n",
            BrotliEncoderVersion(), BrotliDecoderVersion());
    return 2;
  }
  if (argc < 2) {
    fprintf(stderr, "usage: %s -dec|-enc [options]\n", argv[0]);
    return 2;
  }
  uint64_t seed = 1;
  uint64_t iters = 1000;
  double secs = 0;
  for (int i = 2; i < argc; i++) {
    if (!strncmp(argv[i], "-seed=", 6)) {
      seed = strtoull(argv[i] + 6, NULL, 0);
    } else if (!strncmp(argv[i], "-n=", 3)) {
      iters = strtoull(argv[i] + 3, NULL, 0);
    } else if (!strncmp(argv[i], "-secs=", 6)) {
      secs = atof(argv[i] + 6);
    } else if (!strncmp(argv[i], "-out=", 5)) {
      g_out_dir = argv[i] + 5;
    } else if (!strncmp(argv[i], "-data=", 6)) {
      g_data_dir = argv[i] + 6;
    } else if (!strncmp(argv[i], "-q=", 3)) {
      unsigned lo = 0, hi = 0;
      if (sscanf(argv[i] + 3, "%u-%u", &lo, &hi) != 2 || lo > hi || hi > 11) {
        fprintf(stderr, "bad -q option\n");
        return 2;
      }
      g_qmin = lo;
      g_qmax = hi;
    } else if (!strcmp(argv[i], "-hq")) {
      g_hq_dicts = true;
    } else {
      fprintf(stderr, "unknown option %s\n", argv[i]);
      return 2;
    }
  }
  g_seed = seed;
  g_rng_state = seed * 0x9E3779B97F4A7C15ull;
  if (!strcmp(argv[1], "-dec")) {
    return run_dec(iters, secs);
  } else if (!strcmp(argv[1], "-enc")) {
    return run_enc(iters, secs);
  }
  fprintf(stderr, "unknown mode %s\n", argv[1]);
  return 2;
}

static int  //
run_enc(uint64_t iters, double secs) {
  (void)iters;
  (void)secs;
  return 2;
}
