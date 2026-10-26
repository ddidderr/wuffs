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

// manual-test-brotli-encoder-parity.c checks that Wuffs' std/brotli encoder
// produces exactly the same bytes as libbrotlienc v1.2.1 (at qualities 0 and
// 1) over the full parameter grid: every test input times
//   - quality {0, 1}
//   - lgwin {10, 16, 18, 22, 24}
//   - mode {0, 1, 2}
//   - input chunking {single, 1-byte (inputs up to 4 KiB only), 7-byte,
//     4096-byte, pseudo-random}
//   - flush {none, every chunk, middle}
// plus metadata blocks (of 0, 1, 100 and 70000 bytes, at the start, middle
// and end), size_hint {0, exact, 1 << 30}, other parameters (which have no
// effect at qualities 0 and 1) and limited dst lengths.
//
// The test inputs are the files named on the command line (or, by default,
// every file in test/data/brotli plus test/data/{256.bytes,enwik5,romeo.txt})
// plus some synthetic inputs (1 MiB of zeroes, 256 KiB of pseudo-random
// bytes, a repeated 100-byte pattern and pseudo-random lengths 0 to 40).
//
// The test/c/std/brotli.c test program (run by "wuffs test -mimic
// std/brotli") checks a subset of this grid.
//
// Run it from the Wuffs root directory with:
//
//   gcc -O2 -std=c99 script/manual-test-brotli-encoder-parity.c
//       -lbrotlienc -lbrotlicommon -lm -o /tmp/brotli-enc-parity
//
// (as one command line).
//   /tmp/brotli-enc-parity
//
// It prints each mismatching configuration (and the first differing offset)
// and exits with a non-zero status if there were any mismatches.

#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "brotli/encode.h"

#define WUFFS_IMPLEMENTATION
#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__BROTLI
#define WUFFS_CONFIG__MODULE__SOFTFLOAT
#include "../release/c/wuffs-unsupported-snapshot.c"

#define OP_PROCESS 0
#define OP_FLUSH 1
#define OP_FINISH 2
#define OP_EMIT_METADATA 3

#define MAX_STEPS (1 << 22)
#define MAX_PARAMS 16
#define MAX_INPUT (16 << 20)
#define MAX_OUTPUT (64 << 20)

static uint32_t g_step_ops[MAX_STEPS];
static size_t g_step_lens[MAX_STEPS];
static size_t g_num_steps;

static uint8_t* g_src;      // The plan's source: input plus any metadata.
static size_t g_src_len;
static uint8_t* g_have;     // Wuffs' output.
static uint8_t* g_want;     // libbrotlienc's output.
static uint8_t* g_workbuf;  // Wuffs' workbuf.
static size_t g_workbuf_len;

static uint64_t g_num_configs;
static uint64_t g_num_mismatches;

typedef struct {
  uint32_t keys[MAX_PARAMS];
  uint32_t values[MAX_PARAMS];
  size_t num_params;
  uint32_t chunking;  // 0 single, 1 1-byte, 2 7-byte, 3 4096-byte, 4 random.
  uint32_t flush;     // 0 none, 1 every chunk, 2 middle.
  uint32_t metadata_position;  // 0 none, 1 start, 2 middle, 3 end.
  size_t metadata_len;
  size_t dst_chunk;  // 0 means unlimited.
  uint64_t seed;
} config;

static void  //
add_param(config* c, uint32_t key, uint32_t value) {
  if (c->num_params < MAX_PARAMS) {
    c->keys[c->num_params] = key;
    c->values[c->num_params] = value;
    c->num_params++;
  }
}

static uint64_t  //
rand64(uint64_t* state) {
  // SplitMix64.
  uint64_t z = (*state += 0x9E3779B97F4A7C15ull);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

static void  //
fill_random(uint8_t* p, size_t n, uint64_t seed) {
  uint64_t state = seed;
  for (size_t i = 0; i < n; i++) {
    p[i] = (uint8_t)(rand64(&state) >> 56);
  }
}

static bool  //
add_step(uint32_t op, size_t len) {
  if (g_num_steps >= MAX_STEPS) {
    return false;
  }
  g_step_ops[g_num_steps] = op;
  g_step_lens[g_num_steps] = len;
  g_num_steps++;
  return true;
}

// make_plan mirrors make_brotli_enc_plan in test/c/std/brotli.c.
static const char*  //
make_plan(const config* c, const uint8_t* data, size_t data_len) {
  g_num_steps = 0;
  g_src_len = 0;
  if ((data_len + c->metadata_len) > MAX_INPUT) {
    return "input is too long";
  }
  uint64_t rng = c->seed;
  size_t metadata_at = SIZE_MAX;
  if (c->metadata_position == 1) {
    metadata_at = 0;
  } else if (c->metadata_position == 2) {
    metadata_at = data_len / 2;
  } else if (c->metadata_position == 3) {
    metadata_at = data_len;
  }
  bool flushed_middle = false;

  size_t i = 0;
  while (true) {
    if (i == metadata_at) {
      fill_random(g_src + g_src_len, c->metadata_len, c->seed ^ 0x5A5A);
      g_src_len += c->metadata_len;
      if (!add_step(OP_EMIT_METADATA, c->metadata_len)) {
        return "too many steps";
      }
      metadata_at = SIZE_MAX;
    }

    size_t n = data_len - i;
    switch (c->chunking) {
      case 1:
        n = 1;
        break;
      case 2:
        n = 7;
        break;
      case 3:
        n = 4096;
        break;
      case 4: {
        uint64_t r = rand64(&rng);
        n = (size_t)(1 + ((r & 0xFF) < 0xF0 ? ((r >> 8) % 300)
                                             : ((r >> 8) % 70000)));
        break;
      }
    }
    if ((metadata_at != SIZE_MAX) && (metadata_at > i)) {
      n = (n < (metadata_at - i)) ? n : (metadata_at - i);
    }
    if (n > (data_len - i)) {
      n = data_len - i;
    }

    memcpy(g_src + g_src_len, data + i, n);
    g_src_len += n;
    i += n;
    bool last = (i == data_len) && (metadata_at == SIZE_MAX);

    bool ok = true;
    if (last) {
      ok = add_step(OP_FINISH, n);
    } else if (c->flush == 1) {
      ok = add_step(OP_FLUSH, n);
    } else {
      ok = add_step(OP_PROCESS, n);
      if ((c->flush == 2) && !flushed_middle && ((2 * i) >= data_len)) {
        flushed_middle = true;
        ok = ok && add_step(OP_FLUSH, 0);
      }
    }
    if (!ok) {
      return "too many steps";
    }
    if (last) {
      break;
    }
  }
  return NULL;
}

// brotli_enc_quirk_key maps a BrotliEncoderParameter value to the
// corresponding std/brotli quirk key (or 0 if there is none).
static uint32_t  //
brotli_enc_quirk_key(uint32_t param) {
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

// encode_wuffs mirrors wuffs_brotli_encode_plan in test/c/std/brotli.c.
static const char*  //
encode_wuffs(const config* c, size_t* have_len) {
  wuffs_brotli__encoder* enc = wuffs_brotli__encoder__alloc();
  if (!enc) {
    return "alloc failed";
  }
  const char* ret = NULL;
  wuffs_base__io_buffer dst = wuffs_base__ptr_u8__writer(g_have, MAX_OUTPUT);
  wuffs_base__io_buffer src = wuffs_base__ptr_u8__reader(g_src, g_src_len,
                                                         false);
  src.meta.wi = 0;

  for (size_t j = 0; j < c->num_params; j++) {
    wuffs_base__status status = wuffs_brotli__encoder__set_quirk(
        enc, brotli_enc_quirk_key(c->keys[j]), c->values[j]);
    if (status.repr) {
      ret = status.repr;
      goto cleanup;
    }
  }
  uint64_t wlen = wuffs_brotli__encoder__workbuf_len(enc).max_incl;
  if (wlen > g_workbuf_len) {
    ret = "workbuf_len is too large";
    goto cleanup;
  }
  wuffs_base__slice_u8 workbuf =
      wuffs_base__make_slice_u8(g_workbuf, (size_t)wlen);

  size_t end = 0;
  for (size_t i = 0; i < g_num_steps; i++) {
    end += g_step_lens[i];
    uint32_t op = g_step_ops[i];
    wuffs_base__status status = wuffs_base__make_status(NULL);
    if (op == OP_FLUSH) {
      status = wuffs_brotli__encoder__request_flush(enc);
    } else if (op == OP_EMIT_METADATA) {
      status = wuffs_brotli__encoder__request_emit_metadata(enc, g_step_lens[i]);
    }
    if (status.repr) {
      ret = status.repr;
      goto cleanup;
    }
    while (true) {
      wuffs_base__io_buffer d = dst;
      if ((c->dst_chunk > 0) && ((d.data.len - d.meta.wi) > c->dst_chunk)) {
        d.data.len = d.meta.wi + c->dst_chunk;
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
  *have_len = dst.meta.wi;
  free(enc);
  return ret;
}

// encode_mimic mirrors mimic_brotli_encode in test/c/mimiclib/brotli.c.
static const char*  //
encode_mimic(const config* c, size_t* want_len) {
  *want_len = 0;
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
  const uint8_t* next_in = g_src;
  for (size_t i = 0; i < g_num_steps; i++) {
    size_t avail_in = g_step_lens[i];
    BrotliEncoderOperation op = (BrotliEncoderOperation)(g_step_ops[i]);
    while (true) {
      size_t avail_out = MAX_OUTPUT - *want_len;
      if ((c->dst_chunk > 0) && (avail_out > c->dst_chunk)) {
        avail_out = c->dst_chunk;
      }
      size_t avail_out0 = avail_out;
      uint8_t* next_out = g_want + *want_len;
      if (!BrotliEncoderCompressStream(state, op, &avail_in, &next_in,
                                       &avail_out, &next_out, NULL)) {
        ret = "BrotliEncoderCompressStream failed";
        goto cleanup;
      }
      *want_len += avail_out0 - avail_out;
      if (op == BROTLI_OPERATION_FINISH) {
        if (BrotliEncoderIsFinished(state)) {
          break;
        }
      } else if ((avail_in == 0) && !BrotliEncoderHasMoreOutput(state)) {
        break;
      }
      if (*want_len == MAX_OUTPUT) {
        ret = "dst is full";
        goto cleanup;
      }
    }
  }

cleanup:
  BrotliEncoderDestroyInstance(state);
  return ret;
}

static void  //
describe(char* buf, size_t n, const char* name, const config* c) {
  int k = snprintf(buf, n, "%s:", name);
  for (size_t j = 0; (j < c->num_params) && (k > 0) && ((size_t)k < n); j++) {
    k += snprintf(buf + k, n - (size_t)k, " p%u=%u", c->keys[j], c->values[j]);
  }
  if ((k > 0) && ((size_t)k < n)) {
    snprintf(buf + k, n - (size_t)k,
             " chunking=%u flush=%u metadata=%u/%zu dst_chunk=%zu seed=0x%" PRIX64,
             c->chunking, c->flush, c->metadata_position, c->metadata_len,
             c->dst_chunk, c->seed);
  }
}

static void  //
check(const char* name, const config* c, const uint8_t* data, size_t len) {
  char desc[512];
  g_num_configs++;
  const char* err = make_plan(c, data, len);
  if (err) {
    describe(desc, sizeof desc, name, c);
    printf("SKIP %s: %s\n", desc, err);
    return;
  }
  size_t have_len = 0;
  size_t want_len = 0;
  const char* have_err = encode_wuffs(c, &have_len);
  const char* want_err = encode_mimic(c, &want_len);
  if (have_err || want_err) {
    g_num_mismatches++;
    describe(desc, sizeof desc, name, c);
    printf("MISMATCH %s: wuffs error \"%s\", mimic error \"%s\"\n", desc,
           have_err ? have_err : "", want_err ? want_err : "");
    fflush(stdout);
    return;
  }
  size_t n = (have_len < want_len) ? have_len : want_len;
  size_t i = 0;
  while ((i < n) && (g_have[i] == g_want[i])) {
    i++;
  }
  if ((i < n) || (have_len != want_len)) {
    g_num_mismatches++;
    describe(desc, sizeof desc, name, c);
    printf("MISMATCH %s: lengths %zu (wuffs) vs %zu (mimic), first difference "
           "at offset %zu\n",
           desc, have_len, want_len, i);
    fflush(stdout);
  }
}

static config  //
make_config(uint32_t quality, uint32_t lgwin) {
  config c;
  memset(&c, 0, sizeof c);
  add_param(&c, 1, quality);  // BROTLI_PARAM_QUALITY.
  add_param(&c, 2, lgwin);    // BROTLI_PARAM_LGWIN.
  return c;
}

static void  //
run_input(const char* name, const uint8_t* data, size_t len) {
  static const uint32_t lgwins[] = {10, 16, 18, 22, 24};
  static const size_t metadata_lens[] = {0, 1, 100, 70000};
  uint64_t configs0 = g_num_configs;
  uint64_t mismatches0 = g_num_mismatches;
  time_t start = time(NULL);

  // The main grid.
  for (uint32_t q = 0; q <= 1; q++) {
    for (size_t w = 0; w < 5; w++) {
      for (uint32_t mode = 0; mode <= 2; mode++) {
        for (uint32_t ch = 0; ch < 5; ch++) {
          if ((ch == 1) && (len > 4096)) {
            continue;
          }
          for (uint32_t fl = 0; fl < 3; fl++) {
            config c = make_config(q, lgwins[w]);
            add_param(&c, 0, mode);  // BROTLI_PARAM_MODE.
            c.chunking = ch;
            c.flush = fl;
            c.seed = (w << 8) | (mode << 4) | fl | ((uint64_t)len << 16);
            check(name, &c, data, len);
          }
        }
      }
    }
  }

  // Metadata, size_hint, other parameters and limited dst lengths.
  for (uint32_t q = 0; q <= 1; q++) {
    for (size_t i = 0; i < 4; i++) {
      for (uint32_t pos = 1; pos <= 3; pos++) {
        for (uint32_t fl = 0; fl < 3; fl++) {
          config c = make_config(q, 16);
          c.chunking = 4;
          c.flush = fl;
          c.metadata_position = pos;
          c.metadata_len = metadata_lens[i];
          c.dst_chunk = (fl == 2) ? 5 : 0;
          c.seed = (i << 8) | pos;
          check(name, &c, data, len);
        }
      }
    }
    static const uint32_t extras[][2] = {
        {5, 0},                    // BROTLI_PARAM_SIZE_HINT.
        {5, 0xFFFFFFFF},           // Replaced by the exact size.
        {5, 1u << 30},             //
        {3, 16},                   // BROTLI_PARAM_LGBLOCK.
        {3, 24},                   //
        {4, 1},                    // BROTLI_PARAM_DISABLE_LITERAL_CONTEXT_MODELING.
        {6, 1},                    // BROTLI_PARAM_LARGE_WINDOW.
        {7, 2},                    // BROTLI_PARAM_NPOSTFIX.
        {8, 12},                   // BROTLI_PARAM_NDIRECT.
        {9, 1},                    // BROTLI_PARAM_STREAM_OFFSET.
        {9, 1000},                 //
        {9, 1u << 30},             //
        {2, 0xFFFFFFFF},           // LGWIN (int)-1.
        {2, 30},                   //
        {1, 0xFFFFFFFF},           // QUALITY (int)-1, so quality 0.
    };
    for (size_t i = 0; i < sizeof extras / sizeof extras[0]; i++) {
      for (uint32_t ch = 0; ch < 5; ch += 4) {
        config c = make_config(q, 22);
        uint32_t v = extras[i][1];
        if ((extras[i][0] == 5) && (v == 0xFFFFFFFF)) {
          v = (uint32_t)len;
        }
        add_param(&c, extras[i][0], v);
        c.chunking = ch;
        c.flush = 2;
        c.seed = i;
        check(name, &c, data, len);
      }
    }
    if (len <= (256 << 10)) {
      static const size_t dst_chunks[] = {1, 2, 3, 7, 64, 4096};
      for (size_t i = 0; i < 6; i++) {
        for (uint32_t fl = 0; fl < 3; fl++) {
          config c = make_config(q, (i & 1) ? 10 : 24);
          c.chunking = (fl == 0) ? 0 : 4;
          c.flush = fl;
          c.dst_chunk = dst_chunks[i];
          c.seed = i;
          check(name, &c, data, len);
        }
      }
    }
  }

  printf("%-50s %8zu bytes: %6" PRIu64 " configs, %" PRIu64
         " mismatches (%lds)\n",
         name, len, g_num_configs - configs0, g_num_mismatches - mismatches0,
         (long)(time(NULL) - start));
  fflush(stdout);
}

static uint8_t*  //
read_whole_file(const char* path, size_t* len) {
  FILE* f = fopen(path, "rb");
  if (!f) {
    return NULL;
  }
  uint8_t* p = malloc(MAX_INPUT);
  if (p) {
    *len = fread(p, 1, MAX_INPUT, f);
  }
  fclose(f);
  return p;
}

static int  //
compare_strings(const void* a, const void* b) {
  return strcmp(*(const char* const*)a, *(const char* const*)b);
}

int  //
main(int argc, char** argv) {
  if (BrotliEncoderVersion() != 0x1002001) {
    fprintf(stderr, "libbrotlienc version: have 0x%X, want 0x1002001\n",
            BrotliEncoderVersion());
    return 1;
  }
  g_src = malloc(MAX_INPUT);
  g_have = malloc(MAX_OUTPUT);
  g_want = malloc(MAX_OUTPUT);
  g_workbuf_len = (2u << 24) + 503 + 16;
  g_workbuf = malloc(g_workbuf_len);
  if (!g_src || !g_have || !g_want || !g_workbuf) {
    fprintf(stderr, "malloc failed\n");
    return 1;
  }

  char* names[1024];
  int num_names = 0;
  if (argc > 1) {
    for (int i = 1; (i < argc) && (num_names < 1024); i++) {
      names[num_names++] = strdup(argv[i]);
    }
  } else {
    names[num_names++] = strdup("test/data/256.bytes");
    names[num_names++] = strdup("test/data/enwik5");
    names[num_names++] = strdup("test/data/romeo.txt");
    DIR* d = opendir("test/data/brotli");
    if (!d) {
      fprintf(stderr, "could not open test/data/brotli (run this program "
                      "from the Wuffs root directory)\n");
      return 1;
    }
    struct dirent* e;
    int first = num_names;
    while ((e = readdir(d)) && (num_names < 1024)) {
      if (e->d_name[0] == '.') {
        continue;
      }
      char path[1024];
      snprintf(path, sizeof path, "test/data/brotli/%s", e->d_name);
      names[num_names++] = strdup(path);
    }
    closedir(d);
    qsort(names + first, (size_t)(num_names - first), sizeof names[0],
          compare_strings);
  }

  for (int i = 0; i < num_names; i++) {
    size_t len = 0;
    uint8_t* data = read_whole_file(names[i], &len);
    if (!data) {
      fprintf(stderr, "could not read %s\n", names[i]);
      return 1;
    }
    run_input(names[i], data, len);
    free(data);
  }

  if (argc <= 1) {
    // Synthetic inputs.
    uint8_t* p = malloc(1 << 20);
    if (!p) {
      return 1;
    }
    memset(p, 0, 1 << 20);
    run_input("synthetic: 1 MiB of zeroes", p, 1 << 20);
    fill_random(p, 256 << 10, 0x1234);
    run_input("synthetic: 256 KiB of pseudo-random bytes", p, 256 << 10);
    uint8_t pattern[100];
    fill_random(pattern, 100, 0x5678);
    for (size_t i = 0; i < (64 << 10); i++) {
      p[i] = pattern[i % 100];
    }
    run_input("synthetic: a repeated 100-byte pattern", p, 64 << 10);
    for (size_t n = 0; n <= 40; n++) {
      fill_random(p, n, 0x9999 + n);
      char name[64];
      snprintf(name, sizeof name, "synthetic: %zu pseudo-random bytes", n);
      run_input(name, p, n);
    }
    free(p);
  }

  printf("TOTAL: %" PRIu64 " configs, %" PRIu64 " mismatches\n", g_num_configs,
         g_num_mismatches);
  return g_num_mismatches ? 1 : 0;
}
