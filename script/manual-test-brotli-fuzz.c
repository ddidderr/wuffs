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
//   -nohq        (-enc only) no dictionaries at qualities 10 and 11 (by
//                default, 10% of the q10-11 configs get small inputs and
//                dictionaries).
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
//   gcc -O2 -std=c99 -I$BROTLI/c/include -I$BROTLI/c
//       script/manual-test-brotli-fuzz.c
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
// libbrotlienc's internal state, to recognize a known C bug (see
// c_state_corrupted). This needs -I$BROTLI/c as well as -I$BROTLI/c/include.
#include "enc/state.h"

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
static bool g_hq_dicts = true;

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
  if ((wlen > ENC_WB_CAP) && !c->short_workbuf && (r.min_incl <= ENC_WB_CAP)) {
    wlen = ENC_WB_CAP;
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
// g_c_state_corrupted is encode_c's error when libbrotlienc's
// BrotliEncoderCompressStream fails because of a C bug: in ProcessMetadata,
// when dst has no room, the metadata body goes through tiny_buf_ (16 bytes)
// and pushing it out leaves next_out_ pointing just past tiny_buf_, at
// remaining_metadata_bytes_. Back in the processing state, qualities 0 and 1
// compress in place (into the caller's dst) without resetting next_out_, so
// a later flush's InjectBytePaddingBlock writes the padding bits through the
// stale pointer, over remaining_metadata_bytes_. Its output is still right,
// but the next BrotliEncoderCompressStream call sees a metadata block in
// progress and fails. A C build that sets next_out_ to NULL when the
// metadata workflow ends produces the same bytes as Wuffs. The corruption
// shows as the processing state with remaining_metadata_bytes_ not
// BROTLI_UINT32_MAX, which never happens otherwise.
static const char g_c_state_corrupted[] =
    "C state corrupted (stale next_out_ after metadata)";

static bool  //
c_state_corrupted(BrotliEncoderState* state) {
  return (state->stream_state_ == BROTLI_STREAM_PROCESSING) &&
         (state->remaining_metadata_bytes_ != 0xFFFFFFFFu);
}

// g_c_zero_alloc makes encode_c give libbrotlienc zeroed memory (see
// g_enc_uninit_tail).
static bool g_c_zero_alloc;

static void*  //
zero_alloc(void* opaque, size_t size) {
  (void)opaque;
  return calloc(1, size);
}

static void  //
zero_free(void* opaque, void* address) {
  (void)opaque;
  free(address);
}

static const char*  //
encode_c(const enc_config* c, const dict_set* dicts, const uint8_t* src_ptr,
         uint8_t* out, size_t out_cap, size_t* out_len) {
  *out_len = 0;
  BrotliEncoderPreparedDictionary* prepared[MAX_DICTS] = {0};
  size_t num_prepared = 0;
  BrotliEncoderState* state =
      g_c_zero_alloc ? BrotliEncoderCreateInstance(zero_alloc, zero_free, NULL)
                     : BrotliEncoderCreateInstance(NULL, NULL, NULL);
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
        ret = c_state_corrupted(state) ? g_c_state_corrupted
                                       : "BrotliEncoderCompressStream failed";
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
static bool g_trace = false;
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
    if (g_trace) {
      printf("wuffs call %" PRIu64 ": lin=%zu lout=%zu -> ri=%zu wi=%zu %s\n",
             res.calls, lin, lout, ri, wi, status.repr ? status.repr : "ok");
    }
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
    if (g_trace) {
      printf("c     call %" PRIu64 ": lin=%zu lout=%zu -> ri=%zu wi=%zu r=%d\n",
             res.calls, lin, lout, ri, wi, (int)r);
    }
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
static uint64_t g_dec_known_block_length;
static uint64_t g_dec_known_saved;

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

// known_block_length_deviation returns whether the results differ only in
// the documented way (see "Status Mapping" in std/brotli/README.md): with
// chunked input, libbrotlidec can report BROTLI_DECODER_ERROR_FORMAT_BLOCK_
// LENGTH_1 for an overrun metablock where Wuffs reports "#bad metablock
// length" (BLOCK_LENGTH_2), because libbrotlidec's internal input buffer
// (BrotliDecoderState.buffer, used after it rolls back a partially read
// command, distance or block switch) makes it run out of input, and check
// the metablock length, at points where Wuffs does not.
static bool  //
known_block_length_deviation(const chunking* ch, const dec_result* w,
                             const dec_result* c, const uint8_t* wout,
                             const uint8_t* cout) {
  if (ch->one_shot || (w->cls != -10) || (c->cls != -9)) {
    return false;
  }
  size_t m = (w->out_n < c->out_n) ? w->out_n : c->out_n;
  return !m || !memcmp(wout, cout, m);
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
        if (why && known_block_length_deviation(&ch, &w, &c, wout, cout)) {
          why = NULL;
          g_dec_known_block_length++;
          if (g_dec_known_saved < 20) {
            g_dec_known_saved++;
            char desc[4096];
            describe_dec(desc, sizeof desc, "known BLOCK_LENGTH deviation", s,
                         &dc, &ch, &w, &c, n);
            save_failure("known", desc, m, n, &dc.dicts);
            g_num_failures--;
          }
        }
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
  printf("  known BLOCK_LENGTH_1/2 deviations (chunked)=%" PRIu64 "\n",
         g_dec_known_block_length);
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

// replay_dec re-runs a saved decoder failure (see save_failure), printing
// every transform_io and BrotliDecoderDecompressStream call.
static int  //
replay_dec(const char* path) {
  g_dec_wb = big_alloc(DEC_WB_CAP);
  g_dec = (wuffs_brotli__decoder*)xmalloc(sizeof__wuffs_brotli__decoder());
  size_t n = 0;
  uint8_t* m = read_file(path, &n, 64 << 20);
  char tpath[1100];
  snprintf(tpath, sizeof tpath, "%s.txt", path);
  size_t tn = 0;
  char* t = (char*)read_file(tpath, &tn, 1 << 20);
  if (!m || !t) {
    fprintf(stderr, "could not read %s or its .txt\n", path);
    return 2;
  }
  t[tn] = 0;
  dec_config dc;
  memset(&dc, 0, sizeof dc);
  chunking ch;
  memset(&ch, 0, sizeof ch);
  int lw = 0, ebw = 0, un = 0, os = 0;
  char* q = strstr(t, " lw=");
  if (!q || (sscanf(q, " lw=%d", &lw) != 1)) {
    return 2;
  }
  q = strstr(t, " wb_len=");
  if (!q || (sscanf(q,
                    " wb_len=%" SCNu64 " expect_bad_wb=%d uninit=%d out_cap=%zu "
                    "one_shot=%d in_seed=0x%" SCNx64 " in_style=%d out_seed=0x%" SCNx64
                    " out_style=%d",
                    &dc.wb_len, &ebw, &un, &dc.out_cap, &os, &ch.in_seed,
                    &ch.in_style, &ch.out_seed, &ch.out_style) != 9)) {
    fprintf(stderr, "could not parse %s\n", tpath);
    return 2;
  }
  dc.large_window = lw;
  dc.expect_bad_wb = ebw;
  dc.uninit = un;
  ch.one_shot = os;
  for (size_t i = 0; i < MAX_DICTS; i++) {
    for (uint32_t type = 0; type < 2; type++) {
      char dpath[1200];
      snprintf(dpath, sizeof dpath, "%s.dict%zu.%s", path, i,
               type ? "sd" : "raw");
      size_t dn = 0;
      uint8_t* d = read_file(dpath, &dn, 1 << 30);
      if (d) {
        dict_set_add(&dc.dicts, type, d, dn, false);
      }
    }
  }
  uint8_t* wout = xmalloc(dc.out_cap + 1);
  uint8_t* cout = xmalloc(dc.out_cap + 1);
  g_trace = true;
  size_t att = 0;
  dec_result w = wuffs_decode(&dc, &ch, m, n, wout);
  dec_result c = c_decode(&dc, &ch, m, n, cout, &att);
  printf("wuffs: cls=%d status=%s out=%zu in=%zu\nc:     cls=%d out=%zu in=%zu\n",
         w.cls, w.wuffs_status ? w.wuffs_status : "ok", w.out_n, w.in_used,
         c.cls, c.out_n, c.in_used);
  const char* why = compare_dec(&w, &c, wout, cout, dc.out_cap);
  printf("verdict: %s\n", why ? why : "agree");
  return why ? 1 : 0;
}

// ---------------- Encoder campaign

static int run_enc(uint64_t iters, double secs);
static int run_api(uint64_t iters, double secs);

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
    } else if (!strcmp(argv[i], "-nohq")) {
      g_hq_dicts = false;
    } else {
      fprintf(stderr, "unknown option %s\n", argv[i]);
      return 2;
    }
  }
  g_seed = seed;
  // Hash the seed: SplitMix64 adds 0x9E3779B97F4A7C15 to its state per
  // number, so a starting state of (seed * 0x9E3779B97F4A7C15) would make
  // seed N+1's sequence that of seed N shifted by one number, and shards with
  // consecutive seeds would soon generate the same cases.
  g_rng_state = seed;
  g_rng_state = r64();
  if (!strncmp(argv[1], "-replay=", 8)) {
    return replay_dec(argv[1] + 8);
  } else if (!strcmp(argv[1], "-dec")) {
    return run_dec(iters, secs);
  } else if (!strcmp(argv[1], "-enc")) {
    return run_enc(iters, secs);
  } else if (!strcmp(argv[1], "-api")) {
    return run_api(iters, secs);
  }
  fprintf(stderr, "unknown mode %s\n", argv[1]);
  return 2;
}

// pick_quality picks a quality in [g_qmin ..= g_qmax], weighted towards the
// faster qualities.
static uint32_t  //
pick_quality(void) {
  for (int tries = 0; tries < 100; tries++) {
    uint64_t r = rn(1000);
    uint32_t q;
    if (r < 550) {
      q = (uint32_t)rn(5);
    } else if (r < 930) {
      q = 5 + (uint32_t)rn(5);
    } else {
      q = 10 + (uint32_t)rn(2);
    }
    if ((g_qmin <= q) && (q <= g_qmax)) {
      return q;
    }
  }
  return g_qmin + (uint32_t)rn(g_qmax - g_qmin + 1);
}

static uint32_t  //
weird_u32(void) {
  static const uint32_t v[] = {0, 1, 2, 9, 31, 32, 255, 0x7FFFFFFF, 0x80000000,
                               0xFFFFFFFE, 0xFFFFFFFF};
  if (chance(500)) {
    return v[rn(sizeof v / sizeof v[0])];
  }
  return (uint32_t)r64();
}

static void  //
shuffle_params(enc_config* c) {
  for (size_t i = c->num_params; i > 1; i--) {
    size_t j = (size_t)rn(i);
    uint32_t k = c->keys[i - 1];
    uint32_t v = c->values[i - 1];
    c->keys[i - 1] = c->keys[j];
    c->values[i - 1] = c->values[j];
    c->keys[j] = k;
    c->values[j] = v;
  }
}

static uint64_t g_enc_configs;
static uint64_t g_enc_by_quality[12];
static uint64_t g_enc_bytes_in;
static uint64_t g_enc_both_rejected;
static uint64_t g_enc_both_flint_failed;
static uint64_t g_enc_c_corrupted;
// g_enc_uninit_tail counts output mismatches that go away when both encoders
// get zeroed memory. libbrotlienc's RingBufferWrite allocates only the first
// write's bytes (when it is shorter than the tail) and, when it grows the ring
// buffer, copies them but leaves the tail (the copy of the buffer's start, past
// its end) uninitialized, which match finding can then read. Wuffs reads the
// corresponding workbuf bytes, so each output depends on what that memory
// held (malloc'd memory for C, the caller's workbuf for Wuffs), and the two
// agree when the memory does. Both outputs must still decode correctly.
static uint64_t g_enc_uninit_tail;
static uint64_t g_enc_skipped;
static uint64_t g_enc_roundtrips;
static uint64_t g_enc_with_dicts;
static uint64_t g_enc_hq_with_dicts;
static uint64_t g_enc_lw;
static uint64_t g_enc_short_checks;
static uint64_t g_enc_wb_extra;
static uint64_t g_enc_metadata;
static uint64_t g_enc_flushes;
static uint64_t g_enc_dst_limited;
static uint64_t g_enc_stream_offset;
static uint64_t g_enc_len_hist[5];
static double g_enc_time[3];  // C encoder, Wuffs encoder, round trips.

static double  //
now_secs(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (double)t.tv_sec + 1e-9 * (double)t.tv_nsec;
}  // <=64Ki, <=256Ki, <=1Mi, <=4Mi, more.

static void  //
describe_enc(char* buf, size_t cap, const char* what, const enc_config* c,
             const dict_set* ds, size_t input_len, size_t src_len) {
  int k = snprintf(buf, cap, "%s: input_len=%zu src_len=%zu params:", what,
                   input_len, src_len);
  for (size_t j = 0; j < c->num_params; j++) {
    k += snprintf(buf + k, cap - (size_t)k, " p%u=%u", c->keys[j], c->values[j]);
  }
  k += snprintf(buf + k, cap - (size_t)k,
                " dst_chunk=%zu%s dst_seed=0x%" PRIX64 " workbuf_extra=%" PRIu64
                " short_workbuf=%d dicts=%zu:",
                c->dst_chunk, c->dst_chunk_rand ? "(rand)" : "", c->dst_seed,
                c->workbuf_extra, (int)c->short_workbuf, ds->n);
  for (size_t j = 0; (j < ds->n) && ((size_t)k < (cap - 64)); j++) {
    k += snprintf(buf + k, cap - (size_t)k, " %zu", ds->d[j].len);
  }
  k += snprintf(buf + k, cap - (size_t)k, " steps=%zu:", c->num_steps);
  for (size_t j = 0; (j < c->num_steps) && ((size_t)k < (cap - 64)); j++) {
    k += snprintf(buf + k, cap - (size_t)k, " %c%zu", "PFXM"[c->step_ops[j]],
                  c->step_lens[j]);
  }
  snprintf(buf + k, cap - (size_t)k, "\n");
}

// save_enc_failure saves the input (the plan's source, including any
// metadata), the dictionaries and a description.
static void  //
save_enc_failure(const char* what, const enc_config* c, const dict_set* ds,
                 const uint8_t* src, size_t src_len, size_t input_len) {
  static char desc[1 << 16];
  describe_enc(desc, sizeof desc, what, c, ds, input_len, src_len);
  save_failure("enc", desc, src, src_len, ds);
}

// roundtrip_dec decodes stream with both decoders and checks that the result
// is want.
static const char*  //
roundtrip_dec(const uint8_t* stream, size_t stream_len, const uint8_t* want,
              size_t want_len, bool lw, const dict_set* ds, uint8_t* out) {
  dec_config dc;
  memset(&dc, 0, sizeof dc);
  dc.large_window = lw;
  dc.dicts = *ds;  // Not owned.
  dc.out_cap = want_len;
  dc.uninit = chance(500);
  chunking ch;
  memset(&ch, 0, sizeof ch);
  ch.one_shot = chance(800);
  ch.in_seed = r64();
  ch.out_seed = r64();
  ch.in_style = 2 + (int)rn(2);
  ch.out_style = 2 + (int)rn(2);
  size_t attached = 0;
  dec_result c = c_decode(&dc, &ch, stream, stream_len, out, &attached);
  if (c.cls != CLS_OK) {
    return "libbrotlidec round trip failed";
  } else if ((c.out_n != want_len) || (want_len && memcmp(out, want, want_len))) {
    return "libbrotlidec round trip mismatch";
  } else if (c.in_used != stream_len) {
    return "libbrotlidec did not consume the whole stream";
  }
  size_t n_ok = 0;
  if (wuffs_attach(g_dec, &dc, &n_ok)) {
    return "Wuffs add_dictionary failed";
  }
  dc.wb_len = wuffs_brotli__decoder__workbuf_len(g_dec).max_incl;
  if (dc.wb_len > DEC_WB_CAP) {
    return "decoder workbuf_len is too large";
  }
  dec_result w = wuffs_decode(&dc, &ch, stream, stream_len, out);
  if (w.cls != CLS_OK) {
    return "Wuffs decoder round trip failed";
  } else if ((w.out_n != want_len) || (want_len && memcmp(out, want, want_len))) {
    return "Wuffs decoder round trip mismatch";
  } else if (w.in_used != stream_len) {
    return "Wuffs decoder did not consume the whole stream";
  }
  return NULL;
}

static int  //
run_enc(uint64_t iters, double secs) {
  load_corpus();
  load_dicts();
  g_dec_wb = big_alloc(DEC_WB_CAP);
  g_enc_wb = big_alloc(ENC_WB_CAP);
  g_dec = (wuffs_brotli__decoder*)xmalloc(sizeof__wuffs_brotli__decoder());
  const size_t in_cap = (4 << 20) + (40 << 20);
  uint8_t* input = xmalloc(in_cap);
  uint8_t* src = xmalloc(in_cap);
  const size_t out_cap = in_cap + (1 << 20);
  uint8_t* have = xmalloc(out_cap);
  uint8_t* want = xmalloc(out_cap);
  uint8_t* dec_out = xmalloc(in_cap);
  enc_config c;
  memset(&c, 0, sizeof c);

  struct timespec t0;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  uint64_t it = 0;
  for (; it < iters; it++) {
    if ((it & 15) == 0) {
      struct timespec t1;
      clock_gettime(CLOCK_MONOTONIC, &t1);
      double elapsed = (double)(t1.tv_sec - t0.tv_sec) +
                       1e-9 * (double)(t1.tv_nsec - t0.tv_nsec);
      if ((secs > 0) && (elapsed > secs)) {
        break;
      }
    }
    if (g_enc_wb_touched > ((size_t)512 << 20)) {
      big_release(g_enc_wb, ENC_WB_CAP);
      big_release(g_dec_wb, DEC_WB_CAP);
      g_enc_wb_touched = 0;
    }

    // Parameters.
    c.num_params = 0;
    c.num_steps = 0;
    uint32_t q = pick_quality();
    bool hq = q >= 10;
    if (chance(990) || (q != 11)) {
      enc_add_param(&c, 1, q);
    }  // Otherwise the default quality, 11.
    bool lw = chance(200);
    if (lw) {
      enc_add_param(&c, 6, chance(900) ? 1 : (uint32_t)(1 + rn(0xFFFFFFFE)));
    }
    uint32_t lgwin = 22;
    {
      uint64_t r = rn(1000);
      if (r < 700) {
        lgwin = 10 + (uint32_t)rn(15);
        enc_add_param(&c, 2, lgwin);
      } else if (r < 850) {
        lgwin = 10 + (uint32_t)rn(hq ? 18 : 21);
        enc_add_param(&c, 2, lgwin);
      } else if (r < 900) {
        lgwin = weird_u32();
        if (hq && lw && (lgwin > 27) && (lgwin < 0x80000000)) {
          lgwin = 27;
        }
        enc_add_param(&c, 2, lgwin);
      }  // Otherwise the default lgwin, 22.
    }
    if (chance(400)) {
      enc_add_param(&c, 3, chance(800) ? 16 + (uint32_t)rn(9)
                                       : (chance(500) ? (uint32_t)rn(31)
                                                      : weird_u32()));
    }
    if (chance(300)) {
      enc_add_param(&c, 0, chance(950) ? (uint32_t)rn(3) : weird_u32());
    }
    if (chance(500)) {
      uint64_t r = rn(3);
      enc_add_param(&c, 5, (r == 0) ? 0 : (r == 1) ? 0xFFFFFFFF : weird_u32());
    }
    if (chance(300)) {
      uint32_t np = chance(950) ? (uint32_t)rn(4) : (uint32_t)rn(8);
      uint32_t nd = chance(900) ? ((uint32_t)rn(16) << np) : (uint32_t)rn(200);
      enc_add_param(&c, 7, np);
      enc_add_param(&c, 8, nd);
    }
    if (chance(200)) {
      enc_add_param(&c, 4, (uint32_t)rn(2));
    }
    bool stream_offset = false;
    if (chance(80)) {
      uint32_t v = chance(500) ? (uint32_t)rn(100000)
                               : (uint32_t)rn((1u << 30) + 1);
      if (chance(100)) {
        v = (1u << 30) + (uint32_t)rn(2);  // 1 << 30 is OK, one more is not.
      }
      enc_add_param(&c, 9, v);
      stream_offset = v != 0;
    }
    shuffle_params(&c);

    // Input.
    // At q10-11, dictionaries go with small inputs (the Zopfli encoder is
    // slow), and only in 10% of the configs.
    bool with_dicts = hq ? (g_hq_dicts && chance(100)) : chance(250);
    size_t len = pick_len((hq && with_dicts) ? (chance(950) ? (16 << 10) : (64 << 10))
                          : chance(800)        ? (64 << 10)
                          : hq                 ? (chance(900) ? (256 << 10) : (1 << 20))
                                               : (4 << 20));
    gen_input(input, len);
    // Exact size hints.
    for (size_t j = 0; j < c.num_params; j++) {
      if ((c.keys[j] == 5) && (c.values[j] == 0xFFFFFFFF) && chance(900)) {
        c.values[j] = (uint32_t)len;
      }
    }

    // Dictionaries.
    dict_set ds = {0};
    if (with_dicts) {
      size_t k = pick_num_dicts();
      k = (k == 0) ? 1 : (k > 15) ? 15 : k;
      random_raw_dicts(&ds, k, input, len);
    }

    // The plan.
    c.dst_chunk = 0;
    c.dst_chunk_rand = false;
    c.dst_seed = r64();
    {
      uint64_t r = rn(1000);
      if ((r < 100) && (len <= (256 << 10))) {
        c.dst_chunk = 1 + rn(7);
      } else if (r < 200) {
        c.dst_chunk = 1 + rn(chance(500) ? 64 : 70000);
        c.dst_chunk_rand = chance(500);
      }
    }
    int chunk_style = (int)rn(6);
    if ((chunk_style == 1) && (len > 4096)) {
      chunk_style = 2;
    }
    if ((chunk_style == 2) && (len > (256 << 10))) {
      chunk_style = 3;
    }
    bool flush_every = chance(50);
    uint32_t flush_rate = chance(300) ? (uint32_t)rn(300) : 0;
    uint32_t meta_rate = chance(150) ? (uint32_t)rn(100) : 0;
    size_t src_len = 0;
    size_t i = 0;
    bool any_meta = false;
    bool any_flush = false;
    while (true) {
      if (meta_rate && chance(meta_rate)) {
        uint64_t r = rn(1000);
        size_t m = (r < 300) ? 0
                   : (r < 500) ? 1
                   : (r < 900) ? rn(300)
                   : (r < 995) ? rn(70000)
                               : ((1 << 24) - 1 + rn(2));
        if ((src_len + m + (len - i)) > in_cap) {
          m = 0;
        }
        for (size_t j = 0; j < m; j++) {
          src[src_len + j] = (uint8_t)r64();
        }
        src_len += m;
        enc_add_step(&c, OP_EMIT_METADATA, m);
        any_meta = true;
      }
      size_t k;
      switch (chunk_style) {
        case 0:
          k = len - i;
          break;
        case 1:
          k = 1;
          break;
        case 2:
          k = 1 + rn(7);
          break;
        case 3:
          k = 1 + rn(chance(900) ? 300 : 70000);
          break;
        case 4:
          k = rn(chance(900) ? 5000 : (1 << 20));  // Can be 0.
          break;
        default:
          k = rn(len + 1);
          break;
      }
      if (k > (len - i)) {
        k = len - i;
      }
      memcpy(src + src_len, input + i, k);
      src_len += k;
      i += k;
      bool last = (i == len) && (chance(800) || (c.num_steps > 100000));
      if (last) {
        enc_add_step(&c, OP_FINISH, k);
        break;
      } else if (flush_every || (flush_rate && chance(flush_rate))) {
        enc_add_step(&c, OP_FLUSH, k);
        any_flush = true;
      } else {
        enc_add_step(&c, OP_PROCESS, k);
      }
    }
    if (hq && chance(500)) {
      uint64_t r = rn(3);
      c.workbuf_extra = (r == 0) ? rn(1 << 20)
                        : (r == 1) ? rn((uint64_t)1 << 32)
                                   : UINT64_MAX;
    } else {
      c.workbuf_extra = 0;
    }
    c.short_workbuf = (q >= 2) && chance(50);

    g_enc_configs++;
    g_enc_by_quality[q]++;
    g_enc_bytes_in += len;
    g_enc_len_hist[(len <= (64 << 10))    ? 0
                   : (len <= (256 << 10)) ? 1
                   : (len <= (1 << 20))   ? 2
                   : (len <= (4 << 20))   ? 3
                                          : 4]++;
    g_enc_with_dicts += (ds.n > 0);
    g_enc_hq_with_dicts += (hq && (ds.n > 0));
    g_enc_lw += lw;
    g_enc_metadata += any_meta;
    g_enc_flushes += any_flush;
    g_enc_dst_limited += (c.dst_chunk > 0);
    g_enc_stream_offset += stream_offset;
    g_enc_wb_extra += (c.workbuf_extra > 0);

    size_t have_len = 0;
    size_t want_len = 0;
    uint64_t wlen = 0;
    double tt0 = now_secs();
    const char* want_err = encode_c(&c, &ds, src, want, out_cap, &want_len);
    double tt1 = now_secs();
    const char* have_err = encode_wuffs(&c, &ds, src, src_len, len, have,
                                        out_cap, &have_len, 0, &wlen);
    double tt2 = now_secs();
    g_enc_time[0] += tt1 - tt0;
    g_enc_time[1] += tt2 - tt1;
    if (have_err == g_too_large) {
      g_enc_skipped++;
      if (getenv("BFUZZ_VERBOSE")) {
        static char desc[1 << 16];
        describe_enc(desc, sizeof desc, "skipped", &c, &ds, len, src_len);
        printf("%s", desc);
      }
      dict_set_free(&ds);
      continue;
    }
    if (have_err || want_err) {
      bool both_rejected =
          have_err && want_err &&
          (have_err == wuffs_base__error__bad_argument) &&
          !strcmp(want_err, "BrotliEncoderSetParameter failed");
      // With a stream offset, the C code's "emit flint" flush can return
      // early with input left while a flush is pending (when dst is too
      // short to take the flush's output), and the next call then fails
      // (BrotliEncoderCompressStream returns BROTLI_FALSE when the stream is
      // not in its processing state and there is input). Wuffs mirrors this
      // with "#base: bad call sequence". Their outputs before then must
      // agree.
      bool both_flint_failed =
          stream_offset && have_err && want_err &&
          (have_err == wuffs_base__error__bad_call_sequence) &&
          !strcmp(want_err, "BrotliEncoderCompressStream failed") &&
          (have_len == want_len) && !memcmp(have, want, have_len);
      // See g_c_state_corrupted: C's output must be a prefix of Wuffs'.
      bool c_corrupted = !have_err && (want_err == g_c_state_corrupted) &&
                         (want_len <= have_len) &&
                         !memcmp(have, want, want_len);
      if (both_rejected) {
        g_enc_both_rejected++;
      } else if (both_flint_failed) {
        g_enc_both_flint_failed++;
      } else if (c_corrupted) {
        g_enc_c_corrupted++;
        if (g_enc_c_corrupted <= 5) {
          static char desc[1 << 16];
          describe_enc(desc, sizeof desc, g_c_state_corrupted, &c, &ds, len,
                       src_len);
          save_failure("enc-known", desc, src, src_len, &ds);
          g_num_failures--;
        }
      } else {
        char what[512];
        snprintf(what, sizeof what, "errors: wuffs \"%s\", C \"%s\"",
                 have_err ? have_err : "", want_err ? want_err : "");
        save_enc_failure(what, &c, &ds, src, src_len, len);
      }
      dict_set_free(&ds);
      continue;
    }
    if ((have_len != want_len) || memcmp(have, want, have_len)) {
      // Check that Wuffs' output decodes, then rerun both encoders with
      // zeroed memory (see g_enc_uninit_tail).
      const char* rt_err =
          stream_offset ? NULL
                        : roundtrip_dec(have, have_len, input, len, lw, &ds,
                                        dec_out);
      size_t want2_len = 0;
      size_t have2_len = 0;
      uint64_t wlen3 = 0;
      g_c_zero_alloc = true;
      const char* e1 = encode_c(&c, &ds, src, want, out_cap, &want2_len);
      g_c_zero_alloc = false;
      memset(g_enc_wb, 0, wlen);
      const char* e2 = encode_wuffs(&c, &ds, src, src_len, len, have, out_cap,
                                    &have2_len, 0, &wlen3);
      if (!rt_err && !e1 && !e2 && (have2_len == want2_len) &&
          !memcmp(have, want, have2_len)) {
        g_enc_uninit_tail++;
        if (g_enc_uninit_tail <= 5) {
          static char desc[1 << 16];
          describe_enc(desc, sizeof desc,
                       "outputs differ only with non-zeroed memory", &c, &ds,
                       len, src_len);
          save_failure("enc-known", desc, src, src_len, &ds);
          g_num_failures--;
        }
        dict_set_free(&ds);
        continue;
      }
      have_len = have2_len;
      want_len = want2_len;
      size_t j = 0;
      while ((j < have_len) && (j < want_len) && (have[j] == want[j])) {
        j++;
      }
      char what[512];
      snprintf(what, sizeof what,
               "output mismatch: lengths %zu (wuffs) vs %zu (C), first "
               "difference at %zu",
               have_len, want_len, j);
      save_enc_failure(what, &c, &ds, src, src_len, len);
      dict_set_free(&ds);
      continue;
    }
    if (c.short_workbuf) {
      // One byte less than the shortest workbuf must fail.
      size_t n2 = 0;
      uint64_t wlen2 = 0;
      const char* err2 = encode_wuffs(&c, &ds, src, src_len, len, have,
                                      out_cap, &n2, 1, &wlen2);
      if (err2 != (const char*)"no short workbuf") {
        g_enc_short_checks++;
        if (err2 != wuffs_base__error__bad_workbuf_length) {
          char what[512];
          snprintf(what, sizeof what,
                   "a workbuf of %" PRIu64 " bytes did not fail: %s", wlen2,
                   err2 ? err2 : "ok");
          save_enc_failure(what, &c, &ds, src, src_len, len);
        }
      }
    }
    if (!stream_offset) {
      g_enc_roundtrips++;
      double tt3 = now_secs();
      const char* err = roundtrip_dec(want, want_len, input, len, lw, &ds,
                                      dec_out);
      g_enc_time[2] += now_secs() - tt3;
      if (err) {
        save_enc_failure(err, &c, &ds, src, src_len, len);
      }
    }
    dict_set_free(&ds);
  }

  printf("encoder campaign: seed=%" PRIu64 " q=%u-%u configs=%" PRIu64
         " failures=%" PRIu64 " skipped(workbuf>3GiB)=%" PRIu64
         " both_rejected=%" PRIu64 " both_flint_failed=%" PRIu64
         " c_state_corrupted=%" PRIu64 " uninit_tail=%" PRIu64
         " roundtrips=%" PRIu64 "\n",
         g_seed, g_qmin, g_qmax, g_enc_configs, g_num_failures, g_enc_skipped,
         g_enc_both_rejected, g_enc_both_flint_failed, g_enc_c_corrupted,
         g_enc_uninit_tail, g_enc_roundtrips);
  printf("  by quality:");
  for (int q = 0; q < 12; q++) {
    printf(" q%d=%" PRIu64, q, g_enc_by_quality[q]);
  }
  printf("\n  input bytes=%" PRIu64 " lengths: <=64Ki=%" PRIu64
         " <=256Ki=%" PRIu64 " <=1Mi=%" PRIu64 " <=4Mi=%" PRIu64 "\n",
         g_enc_bytes_in, g_enc_len_hist[0], g_enc_len_hist[1],
         g_enc_len_hist[2], g_enc_len_hist[3]);
  printf("  with dicts=%" PRIu64 " (q10-11: %" PRIu64 ") large window=%" PRIu64 " metadata=%" PRIu64
         " flushes=%" PRIu64 " dst limited=%" PRIu64 " stream offset=%" PRIu64
         " workbuf above min=%" PRIu64 " short workbuf checks=%" PRIu64 "\n",
         g_enc_with_dicts, g_enc_hq_with_dicts, g_enc_lw, g_enc_metadata, g_enc_flushes,
         g_enc_dst_limited, g_enc_stream_offset, g_enc_wb_extra,
         g_enc_short_checks);
  printf("  seconds: C encoder %.1f, Wuffs encoder %.1f, round trips %.1f\n",
         g_enc_time[0], g_enc_time[1], g_enc_time[2]);
  return g_num_failures ? 1 : 0;
}

// ---------------- API edge cases

// run_steps_c and run_steps_wuffs run an arbitrary sequence of steps (unlike
// a plan, a FINISH step can be anywhere and be followed by other steps) and
// return the index of the first step that fails (or num_steps). A C step
// fails if BrotliEncoderCompressStream returns BROTLI_FALSE, a Wuffs step if
// request_flush, request_emit_metadata or transform_io returns an error.
static bool g_steps_c_corrupted;

static size_t  //
run_steps_c(const enc_config* c, const uint8_t* src, uint8_t* out,
            size_t out_cap, size_t* out_len) {
  *out_len = 0;
  g_steps_c_corrupted = false;
  BrotliEncoderState* state = BrotliEncoderCreateInstance(NULL, NULL, NULL);
  for (size_t j = 0; j < c->num_params; j++) {
    BrotliEncoderSetParameter(state, (BrotliEncoderParameter)(c->keys[j]),
                              c->values[j]);
  }
  const uint8_t* next_in = src;
  size_t i = 0;
  for (; i < c->num_steps; i++) {
    size_t avail_in = c->step_lens[i];
    BrotliEncoderOperation op = (BrotliEncoderOperation)(c->step_ops[i]);
    bool failed = false;
    while (true) {
      size_t avail_out = out_cap - *out_len;
      if (c->dst_chunk && (avail_out > c->dst_chunk)) {
        avail_out = c->dst_chunk;
      }
      size_t avail_out0 = avail_out;
      uint8_t* next_out = out + *out_len;
      if (!BrotliEncoderCompressStream(state, op, &avail_in, &next_in,
                                       &avail_out, &next_out, NULL)) {
        g_steps_c_corrupted = c_state_corrupted(state);
        failed = true;
        break;
      }
      *out_len += avail_out0 - avail_out;
      if (op == BROTLI_OPERATION_FINISH) {
        if (BrotliEncoderIsFinished(state)) {
          break;
        }
      } else if ((avail_in == 0) && !BrotliEncoderHasMoreOutput(state)) {
        break;
      }
    }
    if (failed) {
      break;
    }
    next_in += avail_in;  // Skip any unconsumed input (there is none).
  }
  BrotliEncoderDestroyInstance(state);
  return i;
}

static const char* g_steps_wuffs_err;

static size_t  //
run_steps_wuffs(const enc_config* c, const uint8_t* src_ptr, size_t src_len,
                uint8_t* out, size_t out_cap, size_t* out_len,
                uint64_t zero_dst_rate) {
  *out_len = 0;
  g_steps_wuffs_err = NULL;
  wuffs_brotli__encoder* enc = wuffs_brotli__encoder__alloc();
  for (size_t j = 0; j < c->num_params; j++) {
    wuffs_brotli__encoder__set_quirk(enc, quirk_key(c->keys[j]), c->values[j]);
  }
  uint64_t wlen = wuffs_brotli__encoder__workbuf_len(enc).min_incl;
  if (wlen > ENC_WB_CAP) {
    free(enc);
    g_steps_wuffs_err = g_too_large;
    return 0;
  }
  if (wlen > g_enc_wb_touched) {
    g_enc_wb_touched = wlen;
  }
  wuffs_base__slice_u8 workbuf = wuffs_base__make_slice_u8(g_enc_wb, wlen);
  wuffs_base__io_buffer dst = wuffs_base__ptr_u8__writer(out, out_cap);
  wuffs_base__io_buffer src =
      wuffs_base__ptr_u8__reader((uint8_t*)src_ptr, src_len, false);
  src.meta.wi = 0;
  size_t end = 0;
  size_t i = 0;
  for (; i < c->num_steps; i++) {
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
      g_steps_wuffs_err = status.repr;
      break;
    }
    bool failed = false;
    for (uint64_t calls = 0;; calls++) {
      if (calls > (out_cap + src_len + 1000)) {
        g_steps_wuffs_err = "too many transform_io calls";
        failed = true;
        break;
      }
      wuffs_base__io_buffer d = dst;
      if (c->dst_chunk && ((d.data.len - d.meta.wi) > c->dst_chunk)) {
        d.data.len = d.meta.wi + c->dst_chunk;
      }
      if (zero_dst_rate && chance(zero_dst_rate)) {
        d.data.len = d.meta.wi;
      }
      wuffs_base__io_buffer s = src;
      s.meta.wi = end;
      s.meta.closed = (op == OP_FINISH);
      status = wuffs_brotli__encoder__transform_io(enc, &d, &s, workbuf);
      dst.meta.wi = d.meta.wi;
      src.meta.ri = s.meta.ri;
      if (status.repr == NULL) {
        break;
      } else if (status.repr == wuffs_base__suspension__short_write) {
        continue;
      } else if ((status.repr == wuffs_base__suspension__short_read) &&
                 (op != OP_FINISH) && (src.meta.ri == end)) {
        break;
      }
      g_steps_wuffs_err = status.repr;
      failed = true;
      break;
    }
    if (failed) {
      break;
    }
    src.meta.ri = end;
  }
  *out_len = dst.meta.wi;
  free(enc);
  return i;
}

static uint64_t g_api_checks;
static uint64_t g_api_both_failed_meta;  // Too-long metadata.
static uint64_t g_api_both_failed_seq;   // Bad call sequence.
static uint64_t g_api_c_corrupted;       // See g_c_state_corrupted.

static void  //
api_fail(const char* fmt, const char* a, uint64_t x, uint64_t y) {
  char desc[1024];
  snprintf(desc, sizeof desc, fmt, a, x, y);
  strncat(desc, "\n", sizeof desc - strlen(desc) - 1);
  save_failure("api", desc, NULL, 0, NULL);
}

// api_param_acceptance compares set_quirk's and BrotliEncoderSetParameter's
// acceptance of every parameter value (and of setting a parameter after the
// first call).
static void  //
api_param_acceptance(void) {
  static const uint32_t values[] = {0,          1,          2,          3,
                                    4,          9,          10,         11,
                                    12,         15,         16,         24,
                                    25,         30,         31,         120,
                                    121,        255,        1u << 24,   1u << 30,
                                    (1u << 30) + 1, 0x7FFFFFFF, 0x80000000,
                                    0xFFFFFFFE, 0xFFFFFFFF};
  for (uint32_t param = 0; param <= 9; param++) {
    for (size_t k = 0; k < sizeof values / sizeof values[0]; k++) {
      g_api_checks++;
      BrotliEncoderState* st = BrotliEncoderCreateInstance(NULL, NULL, NULL);
      bool c_ok = BrotliEncoderSetParameter(st, (BrotliEncoderParameter)param,
                                            values[k]);
      BrotliEncoderDestroyInstance(st);
      wuffs_brotli__encoder* enc = wuffs_brotli__encoder__alloc();
      wuffs_base__status status =
          wuffs_brotli__encoder__set_quirk(enc, quirk_key(param), values[k]);
      bool w_ok = status.repr == NULL;
      if (!w_ok && (status.repr != wuffs_base__error__bad_argument)) {
        api_fail("set_quirk(param %s%" PRIu64 ", %" PRIu64 "): unexpected status",
                 "", param, values[k]);
      }
      if (c_ok != w_ok) {
        api_fail("set_quirk(param %s%" PRIu64 ", %" PRIu64 "): acceptance differs",
                 "", param, values[k]);
      }
      // Values that do not fit in a u32.
      status = wuffs_brotli__encoder__set_quirk(
          enc, quirk_key(param), ((uint64_t)1 << 32) + values[k]);
      if (status.repr != wuffs_base__error__bad_argument) {
        api_fail("set_quirk(param %s%" PRIu64 ", (1<<32) + %" PRIu64
                 "): not rejected", "", param, values[k]);
      }
      free(enc);
    }
    // After the first call.
    g_api_checks++;
    BrotliEncoderState* st = BrotliEncoderCreateInstance(NULL, NULL, NULL);
    size_t ai = 0, ao = 0;
    const uint8_t* ni = NULL;
    uint8_t* no = NULL;
    BrotliEncoderCompressStream(st, BROTLI_OPERATION_PROCESS, &ai, &ni, &ao,
                                &no, NULL);
    bool c_ok = BrotliEncoderSetParameter(st, (BrotliEncoderParameter)param, 0);
    BrotliEncoderDestroyInstance(st);
    wuffs_brotli__encoder* enc = wuffs_brotli__encoder__alloc();
    wuffs_brotli__encoder__set_quirk(enc, WUFFS_BROTLI__QUIRK_ENCODER_QUALITY,
                                     5);
    wuffs_brotli__encoder__set_quirk(enc, WUFFS_BROTLI__QUIRK_LGWIN, 10);
    uint64_t wlen = wuffs_brotli__encoder__workbuf_len(enc).max_incl;
    uint8_t one = 0;
    wuffs_base__io_buffer d = wuffs_base__ptr_u8__writer(&one, 0);
    wuffs_base__io_buffer s = wuffs_base__ptr_u8__reader(&one, 0, false);
    wuffs_brotli__encoder__transform_io(enc, &d, &s,
                                        wuffs_base__make_slice_u8(g_enc_wb, wlen));
    wuffs_base__status status =
        wuffs_brotli__encoder__set_quirk(enc, quirk_key(param), 0);
    if (c_ok || (status.repr != wuffs_base__error__bad_call_sequence)) {
      api_fail("set_quirk after the first call (param %s%" PRIu64
               "): C %" PRIu64 ", Wuffs differs", "", param, c_ok);
    }
    free(enc);
  }
  // An unknown parameter.
  g_api_checks++;
  BrotliEncoderState* st = BrotliEncoderCreateInstance(NULL, NULL, NULL);
  bool c_ok = BrotliEncoderSetParameter(st, (BrotliEncoderParameter)10, 0);
  BrotliEncoderDestroyInstance(st);
  wuffs_brotli__encoder* enc = wuffs_brotli__encoder__alloc();
  wuffs_base__status status = wuffs_brotli__encoder__set_quirk(
      enc, WUFFS_BROTLI__QUIRK_STREAM_OFFSET + 1, 0);
  if (c_ok || (status.repr != wuffs_base__error__unsupported_option)) {
    api_fail("unknown parameter: %sC %" PRIu64 " %" PRIu64, "", c_ok, 0);
  }
  free(enc);
}

// api_compare_steps runs c's steps with both libraries and compares the
// failing step and the output before it.
static void  //
api_compare_steps(const char* what, const enc_config* c, const uint8_t* src,
                  size_t src_len, uint8_t* have, uint8_t* want, size_t cap,
                  uint64_t zero_dst_rate) {
  g_api_checks++;
  size_t want_len = 0;
  size_t have_len = 0;
  size_t cf = run_steps_c(c, src, want, cap, &want_len);
  size_t wf = run_steps_wuffs(c, src, src_len, have, cap, &have_len,
                              zero_dst_rate);
  if (g_steps_wuffs_err == g_too_large) {
    return;
  }
  bool bad = (cf != wf);
  if (!bad && (cf < c->num_steps)) {
    // Which error Wuffs returns.
    uint32_t op = c->step_ops[cf];
    const char* e = g_steps_wuffs_err;
    bool meta_too_long = (op == OP_EMIT_METADATA) &&
                         (c->step_lens[cf] > (1u << 24));
    if (meta_too_long) {
      g_api_both_failed_meta++;
    } else {
      g_api_both_failed_seq++;
    }
    if (meta_too_long ? (e != wuffs_base__error__bad_argument)
                      : (e != wuffs_base__error__bad_call_sequence)) {
      bad = true;
    }
  }
  if (!bad) {
    // C's output on failure is whatever it wrote before the failing call.
    if ((have_len != want_len) || memcmp(have, want, have_len)) {
      bad = true;
    }
  }
  if (bad && g_steps_c_corrupted && (cf < wf) && (want_len <= have_len) &&
      !memcmp(have, want, want_len)) {
    // See g_c_state_corrupted.
    g_api_c_corrupted++;
    return;
  }
  if (bad) {
    static char desc[1 << 16];
    char what2[512];
    snprintf(what2, sizeof what2,
             "%s: failing step C %zu, Wuffs %zu (%s); output lengths C %zu, "
             "Wuffs %zu",
             what, cf, wf, g_steps_wuffs_err ? g_steps_wuffs_err : "",
             want_len, have_len);
    dict_set ds = {0};
    describe_enc(desc, sizeof desc, what2, c, &ds, 0, src_len);
    save_failure("api", desc, src, src_len < (1 << 20) ? src_len : 0, NULL);
  }
}

// api_metadata checks metadata blocks of 0, 1, (1 << 24) - 1, 1 << 24 and
// (1 << 24) + 1 bytes (the last must be rejected), at the start, after some
// data, after a flush and after the end of the stream.
static void  //
api_metadata(uint8_t* src, uint8_t* have, uint8_t* want, size_t cap) {
  static const size_t lens[] = {0, 1, (1u << 24) - 1, 1u << 24,
                                (1u << 24) + 1};
  static const uint32_t qs[] = {0, 1, 2, 4, 5, 9, 10, 11};
  for (size_t qi = 0; qi < sizeof qs / sizeof qs[0]; qi++) {
    for (size_t li = 0; li < 5; li++) {
      for (int where = 0; where < 4; where++) {
        enc_config c;
        memset(&c, 0, sizeof c);
        enc_add_param(&c, 1, qs[qi]);
        enc_add_param(&c, 2, 16);
        size_t n = 0;
        size_t data = 3000;
        gen_segment(src, data, 0, 3);
        n += data;
        if (where == 0) {
          enc_add_step(&c, OP_EMIT_METADATA, lens[li]);
          enc_add_step(&c, OP_FINISH, data);
        } else if (where == 1) {
          enc_add_step(&c, OP_PROCESS, data);
          enc_add_step(&c, OP_EMIT_METADATA, lens[li]);
          enc_add_step(&c, OP_FINISH, 0);
        } else if (where == 2) {
          enc_add_step(&c, OP_FLUSH, data);
          enc_add_step(&c, OP_EMIT_METADATA, lens[li]);
          enc_add_step(&c, OP_FINISH, 0);
        } else {
          enc_add_step(&c, OP_FINISH, data);
          enc_add_step(&c, OP_EMIT_METADATA, lens[li]);
        }
        size_t at = (where == 0) ? 0 : data;
        memmove(src + at + lens[li], src + at, n - at);
        memset(src + at, 0xA5, lens[li]);
        n += lens[li];
        char what[128];
        snprintf(what, sizeof what, "metadata q=%u len=%zu where=%d", qs[qi],
                 lens[li], where);
        api_compare_steps(what, &c, src, n, have, want, cap, 0);
        free(c.step_ops);
        free(c.step_lens);
      }
    }
  }
}

// api_dictionaries checks add_dictionary's edge cases against
// BrotliEncoderPrepareDictionary and BrotliEncoderAttachPreparedDictionary.
static void  //
api_dictionaries(void) {
  static const uint8_t bytes[] = "some dictionary bytes, some dictionary bytes";
  // Up to 16 dictionaries (the 16th must be rejected by both), of various
  // types.
  for (uint32_t type = 0; type < 4; type++) {
    g_api_checks++;
    BrotliEncoderState* st = BrotliEncoderCreateInstance(NULL, NULL, NULL);
    wuffs_brotli__encoder* enc = wuffs_brotli__encoder__alloc();
    BrotliEncoderPreparedDictionary* pd[17] = {0};
    for (int k = 0; k < 17; k++) {
      size_t len = (k * 7) % (sizeof bytes);
      uint32_t t = (k == 3) ? type : 0;
      pd[k] = BrotliEncoderPrepareDictionary((BrotliSharedDictionaryType)t,
                                             len, bytes, 11, NULL, NULL, NULL);
      bool c_ok = pd[k] && BrotliEncoderAttachPreparedDictionary(st, pd[k]);
      uint64_t wl =
          wuffs_brotli__encoder__add_dictionary_workbuf_len(enc, len);
      wuffs_base__status status = wuffs_brotli__encoder__add_dictionary(
          enc, t, wuffs_base__make_slice_u8((uint8_t*)bytes, len),
          wuffs_base__make_slice_u8(g_enc_wb, wl));
      const char* want_status =
          c_ok              ? NULL
          : (t == 1)        ? wuffs_brotli__error__unsupported_serialized_encoder_dictionary
          : (t > 1)         ? wuffs_base__error__bad_argument
                            : wuffs_brotli__error__too_many_compound_dictionaries;
      if (status.repr != want_status) {
        api_fail("add_dictionary #%s%" PRIu64 " (type %" PRIu64 "): differs", "",
                 (uint64_t)k, t);
      }
      if (wl > g_enc_wb_touched) {
        g_enc_wb_touched = wl;
      }
    }
    // A too-short workbuf.
    {
      wuffs_brotli__encoder* e2 = wuffs_brotli__encoder__alloc();
      uint64_t wl = wuffs_brotli__encoder__add_dictionary_workbuf_len(e2, 10);
      wuffs_base__status status = wuffs_brotli__encoder__add_dictionary(
          e2, 0, wuffs_base__make_slice_u8((uint8_t*)bytes, 10),
          wuffs_base__make_slice_u8(g_enc_wb, wl - 1));
      if (status.repr != wuffs_base__error__bad_workbuf_length) {
        api_fail("add_dictionary with a short workbuf: %s%" PRIu64 "%" PRIu64,
                 status.repr ? status.repr : "ok", 0, 0);
      }
      free(e2);
    }
    BrotliEncoderDestroyInstance(st);
    for (int k = 0; k < 17; k++) {
      if (pd[k]) {
        BrotliEncoderDestroyPreparedDictionary(pd[k]);
      }
    }
    free(enc);
  }
  // A transform_io workbuf shorter than the dictionaries.
  g_api_checks++;
  {
    wuffs_brotli__encoder* enc = wuffs_brotli__encoder__alloc();
    uint64_t wl = wuffs_brotli__encoder__add_dictionary_workbuf_len(enc, 40);
    wuffs_brotli__encoder__add_dictionary(
        enc, 0, wuffs_base__make_slice_u8((uint8_t*)bytes, 40),
        wuffs_base__make_slice_u8(g_enc_wb, wl));
    uint64_t d_end = wuffs_brotli__encoder__workbuf_len(enc).min_incl;
    uint8_t one = 0;
    wuffs_base__io_buffer d = wuffs_base__ptr_u8__writer(&one, 1);
    wuffs_base__io_buffer s = wuffs_base__ptr_u8__reader(&one, 0, true);
    (void)d_end;
    wuffs_base__status status = wuffs_brotli__encoder__transform_io(
        enc, &d, &s, wuffs_base__make_slice_u8(g_enc_wb, 50));
    if (status.repr != wuffs_base__error__bad_workbuf_length) {
      api_fail("transform_io with a workbuf shorter than the dictionaries: %s%" PRIu64
               "%" PRIu64, status.repr ? status.repr : "ok", 0, 0);
    }
    // add_dictionary after transform_io (the C code accepts this, see the
    // README).
    status = wuffs_brotli__encoder__add_dictionary(
        enc, 0, wuffs_base__make_slice_u8((uint8_t*)bytes, 4),
        wuffs_base__make_slice_u8(g_enc_wb, wl * 2));
    if (status.repr != NULL) {
      // The failed transform_io did not start compressing.
    }
    free(enc);
  }
}

// api_short_workbuf_q01 checks that, at qualities 0 and 1, an input block of
// n bytes needs exactly (2 * n) + 519 bytes of workbuf.
static void  //
api_short_workbuf_q01(uint8_t* src, uint8_t* out, size_t cap) {
  static const size_t ns[] = {0, 1, 2, 100, 1000, 65536, 100000};
  for (uint32_t q = 0; q <= 1; q++) {
    for (size_t k = 0; k < sizeof ns / sizeof ns[0]; k++) {
      size_t n = ns[k];
      gen_segment(src, n, 0, 3);
      for (int delta = -1; delta <= 0; delta++) {
        g_api_checks++;
        wuffs_brotli__encoder* enc = wuffs_brotli__encoder__alloc();
        wuffs_brotli__encoder__set_quirk(enc, WUFFS_BROTLI__QUIRK_ENCODER_QUALITY,
                                         q);
        uint64_t wl = (2 * n) + 519 + delta;
        wuffs_base__io_buffer d = wuffs_base__ptr_u8__writer(out, cap);
        wuffs_base__io_buffer s = wuffs_base__ptr_u8__reader(src, n, true);
        wuffs_base__status status = wuffs_base__make_status(NULL);
        for (int calls = 0; calls < 100; calls++) {
          status = wuffs_brotli__encoder__transform_io(
              enc, &d, &s, wuffs_base__make_slice_u8(g_enc_wb, wl));
          if (status.repr != wuffs_base__suspension__short_write) {
            break;
          }
        }
        bool want_ok = delta == 0;
        if (want_ok ? (status.repr != NULL)
                    : (status.repr != wuffs_base__error__bad_workbuf_length)) {
          api_fail("q0/1 workbuf for a block of n bytes: %s (n=%" PRIu64
                   " delta=%" PRIu64 ")",
                   status.repr ? status.repr : "ok", n, (uint64_t)(-delta));
        }
        free(enc);
      }
    }
  }
}

// random_steps makes a random sequence of steps, where FINISH can be
// anywhere and metadata can be of odd lengths.
static size_t  //
random_steps(enc_config* c, uint8_t* src, size_t cap) {
  size_t n = 0;
  size_t k = 1 + rn(chance(800) ? 6 : 30);
  for (size_t i = 0; i < k; i++) {
    uint32_t op = (uint32_t)rn(4);
    size_t len = 0;
    if (op == OP_EMIT_METADATA) {
      uint64_t r = rn(1000);
      len = (r < 300) ? 0 : (r < 500) ? 1 : (r < 990) ? rn(300)
                                                       : ((1 << 24) - 1 + rn(3));
    } else if (!chance(300)) {
      len = rn(chance(900) ? 200 : 70000);
    }
    if ((n + len) > cap) {
      len = 0;
    }
    gen_input(src + n, len);
    n += len;
    enc_add_step(c, op, len);
  }
  return n;
}

static int  //
run_api(uint64_t iters, double secs) {
  load_corpus();
  g_enc_wb = big_alloc(ENC_WB_CAP);
  const size_t cap = (48 << 20);
  uint8_t* src = xmalloc(cap);
  uint8_t* have = xmalloc(cap + (1 << 20));
  uint8_t* want = xmalloc(cap + (1 << 20));

  api_param_acceptance();
  api_dictionaries();
  api_short_workbuf_q01(src, have, cap);
  api_metadata(src, have, want, cap);
  uint64_t fixed = g_api_checks;

  // Empty input (an immediately closed src) at every quality and some
  // lgwins, with and without large window.
  for (uint32_t q = 0; q <= 11; q++) {
    for (uint32_t lgwin = 10; lgwin <= 30; lgwin += 4) {
      for (uint32_t lw = 0; lw <= 1; lw++) {
        enc_config c;
        memset(&c, 0, sizeof c);
        enc_add_param(&c, 1, q);
        enc_add_param(&c, 2, lgwin);
        enc_add_param(&c, 6, lw);
        enc_add_step(&c, OP_FINISH, 0);
        api_compare_steps("empty input", &c, src, 0, have, want, cap, 300);
        free(c.step_ops);
        free(c.step_lens);
      }
    }
  }

  // Random sequences of steps.
  struct timespec t0;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  uint64_t it = 0;
  for (; it < iters; it++) {
    if ((it & 15) == 0) {
      struct timespec t1;
      clock_gettime(CLOCK_MONOTONIC, &t1);
      if ((secs > 0) && (((double)(t1.tv_sec - t0.tv_sec)) > secs)) {
        break;
      }
    }
    if (g_enc_wb_touched > ((size_t)512 << 20)) {
      big_release(g_enc_wb, ENC_WB_CAP);
      g_enc_wb_touched = 0;
    }
    enc_config c;
    memset(&c, 0, sizeof c);
    uint32_t q = pick_quality();
    enc_add_param(&c, 1, q);
    enc_add_param(&c, 2, 10 + (uint32_t)rn(15));
    bool stream_offset = chance(200);
    if (stream_offset) {
      enc_add_param(&c, 9, (uint32_t)rn(1000));
    }
    c.dst_chunk = chance(300) ? 1 + rn(100) : 0;
    size_t n = random_steps(&c, src, cap);
    // The zero-length dst calls are Wuffs only (run_steps_c gives every C
    // call some dst). A Wuffs call with no dst room while the stream offset
    // flush is pending is like a C call with avail_out == 0 there, after
    // which the next call with input fails in both libraries (see "Encoder
    // Operation Mapping" in std/brotli/README.md), so they are not used with
    // a stream offset.
    uint64_t zero_dst_rate = chance(300) ? 300 : 0;
    api_compare_steps("random steps", &c, src, n, have, want, cap,
                      stream_offset ? 0 : zero_dst_rate);
    free(c.step_ops);
    free(c.step_lens);
  }
  printf("api campaign: seed=%" PRIu64 " fixed checks=%" PRIu64
         " random step sequences=%" PRIu64 " total checks=%" PRIu64
         " failures=%" PRIu64 "\n",
         g_seed, fixed, it, g_api_checks, g_num_failures);
  printf("  both rejected: too-long metadata=%" PRIu64
         ", bad call sequence=%" PRIu64 "; C state corrupted=%" PRIu64 "\n",
         g_api_both_failed_meta, g_api_both_failed_seq, g_api_c_corrupted);
  return g_num_failures ? 1 : 0;
}
