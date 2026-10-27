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
// produces exactly the same bytes as libbrotlienc v1.2.1 (at qualities 0 to
// 9) over a large parameter grid. For each test input and quality:
//   - lgwin {10, 16, 18, 22, 24}
//   - mode {0, 1, 2}
//   - input chunking {single, 1-byte (inputs up to 4 KiB only), 7-byte,
//     4096-byte, pseudo-random}
//   - flush {none, every chunk, middle}
// plus metadata blocks (of 0, 1, 100 and 70000 bytes, at the start, middle
// and end), size_hint {0, exact, 1 << 30, etc}, the other parameters
// (lgblock, disable literal context modeling, npostfix and ndirect, large
// window, stream offset) and limited dst lengths. At qualities 2 and above,
// inputs longer than 512 KiB use a smaller grid.
//
// The test inputs are the files named on the command line (or, by default,
// every file in test/data/brotli plus test/data/{256.bytes,enwik5,romeo.txt,
// pi.txt,nobel-prizes.json,archive.iso,hibiscus.regular.bmp}) plus some
// synthetic inputs (1 MiB of zeroes, 256 KiB of pseudo-random bytes, a
// repeated 100-byte pattern, a 600 KiB mixture of text and binary data and
// pseudo-random lengths 0 to 40).
//
// Options (before any file names):
//   -q=LO-HI   only check qualities LO to HI (default 0-9).
//   -shard=K/N only check the inputs whose index i satisfies (i % N) == K,
//              so that N processes can split the work.
//   -lw        only run the large-window grid (BROTLI_PARAM_LARGE_WINDOW
//              with lgwin 10 to 30, see run_input_lw), at qualities 2 and
//              above.
//   -big       instead of the usual inputs, check synthetic inputs of 3 to
//              12 MiB (with the usual or, with -lw, the large-window grid)
//              and of 24 to 70 MiB (with a few large-window configurations
//              each), with repeats at distances of up to 40 MiB.
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

#include "brotli/decode.h"
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
#define MAX_INPUT (80 << 20)
#define MAX_OUTPUT (96 << 20)

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
static uint64_t g_num_short_checks;
static size_t g_short_workbuf_len;  // 0 means use workbuf_len.
static size_t g_last_want_len;
static uint64_t g_num_mismatches;

static uint32_t g_qmin = 0;
static uint32_t g_qmax = 9;
static bool g_lw_only = false;
static bool g_dict_only = false;

// The attached raw dictionaries (see set_dicts), for both encoders: the C
// code's prepared dictionaries and a Wuffs encoder (g_dict_template) that has
// them added, with the dictionaries in g_workbuf[.. g_dict_end]. For a small
// total dictionary size, encode_wuffs adds them to a fresh encoder instead.
#define MAX_DICTS 16
static const uint8_t* g_dict_ptrs[MAX_DICTS];
static size_t g_dict_lens[MAX_DICTS];
static size_t g_num_dicts;
static size_t g_dict_total;
static BrotliEncoderPreparedDictionary* g_dict_prepared[MAX_DICTS];
static wuffs_brotli__encoder* g_dict_template;
static uint64_t g_dict_end;
static uint64_t g_num_roundtrips;
static uint64_t g_last_wlen;  // encode_wuffs' workbuf_len.
static char g_dict_desc[160];
static uint8_t* g_foreign_dict;  // test/data/enwik5.
static size_t g_foreign_dict_len;
static uint64_t g_num_smaller_checks;

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
  // try_short is whether to also check (at qualities 2 to 9) Wuffs' output
  // with the shortest workbuf that should work: see short_workbuf_len.
  bool try_short;
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

// fill_mixed fills p[0 .. n] with a mixture of text (from a small
// vocabulary, so that there are many matches), runs and binary noise.
static void  //
fill_mixed(uint8_t* p, size_t n_max, uint64_t seed) {
  static const char* words[] = {"the ",    "brotli ", "wuffs ", "of ",
                                "and ",    "data\n",  "time ",  "<a>",
                                "encoder", ", ",       "zebra ", "1234"};
  uint64_t state = seed;
  size_t n = 0;
  while (n < n_max) {
    uint64_t r = rand64(&state);
    if ((r & 0xFF) < 200) {
      const char* w = words[(r >> 8) % 12];
      size_t k = strlen(w);
      while (k-- && (n < n_max)) {
        p[n++] = (uint8_t)(*w++);
      }
    } else if ((r & 0xFF) < 230) {
      size_t k = (size_t)((r >> 8) % 300);
      while (k-- && (n < n_max)) {
        p[n++] = (uint8_t)(r >> 40);
      }
    } else {
      size_t k = (size_t)((r >> 8) % 2000);
      while (k-- && (n < n_max)) {
        p[n++] = (uint8_t)(rand64(&state) >> 56);
      }
    }
  }
}

// fill_far_repeats fills p[0 .. n] with 1 MiB blocks: most are fresh
// pseudo-random bytes or fill_mixed data (with a per-block seed), but some
// repeat (with sparse changes) the block that is near or far
// (near_mib or far_mib MiB) before them, so that encoders with a large
// enough window find matches at those distances. When far_mib is a multiple
// of 4 and near_mib is not 2 modulo 4, the far copies' sources are fresh
// blocks that are not also copied (or copies) nearer by.
static void  //
fill_far_repeats(uint8_t* p, size_t n, size_t near_mib, size_t far_mib,
                 uint64_t seed) {
  const size_t mib = 1 << 20;
  for (size_t b = 0; (b * mib) < n; b++) {
    uint8_t* q = p + (b * mib);
    size_t k = ((n - (b * mib)) < mib) ? (n - (b * mib)) : mib;
    size_t from = SIZE_MAX;
    if ((b >= near_mib) && ((b % 4) == 1)) {
      from = b - near_mib;
    } else if ((b >= far_mib) && ((b % 4) == 3)) {
      from = b - far_mib;
    }
    if (from != SIZE_MAX) {
      // A shifted copy: the distance is not a multiple of 1 MiB (but, for
      // odd b, it is a multiple of 4, which the HROLLING_FAST hasher, used
      // by H35 and H55, needs to find it).
      size_t shift = (size_t)(seed % 250) + (b * 7);
      if (b & 1) {
        shift *= 4;
      }
      memcpy(q, p + (from * mib) + shift, k);
      for (size_t i = 4093; i < k; i += 65521) {
        q[i] ^= 0x55;
      }
    } else if ((b % 3) == 0) {
      fill_random(q, k, seed + b);
    } else {
      fill_mixed(q, k, seed + b);
    }
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

// grow_workbuf makes g_workbuf at least n bytes long, keeping its contents
// (and so any dictionaries at its start).
static bool  //
grow_workbuf(uint64_t n) {
  if (n <= g_workbuf_len) {
    return true;
  } else if (n > ((uint64_t)3 << 30)) {
    return false;
  }
  uint8_t* p = realloc(g_workbuf, (size_t)n);
  if (!p) {
    return false;
  }
  g_workbuf = p;
  g_workbuf_len = (size_t)n;
  return true;
}

// set_dicts sets the attached dictionaries for the following checks (none if
// n is 0), preparing them for both encoders.
static const char*  //
set_dicts(size_t n, const uint8_t* const* ptrs, const size_t* lens) {
  for (size_t j = 0; j < g_num_dicts; j++) {
    BrotliEncoderDestroyPreparedDictionary(g_dict_prepared[j]);
    g_dict_prepared[j] = NULL;
  }
  free(g_dict_template);
  g_dict_template = NULL;
  g_num_dicts = 0;
  g_dict_total = 0;
  g_dict_end = 0;
  for (size_t j = 0; (j < n) && (j < MAX_DICTS); j++) {
    g_dict_ptrs[j] = ptrs[j];
    g_dict_lens[j] = lens[j];
    g_dict_total += lens[j];
    // The quality argument does not matter for raw dictionaries.
    g_dict_prepared[j] = BrotliEncoderPrepareDictionary(
        BROTLI_SHARED_DICTIONARY_RAW, lens[j], ptrs[j], 11, NULL, NULL, NULL);
    g_num_dicts = j + 1;
    if (!g_dict_prepared[j]) {
      return "BrotliEncoderPrepareDictionary failed";
    }
  }
  if (g_dict_total <= (256 << 10)) {
    return NULL;
  }
  g_dict_template = wuffs_brotli__encoder__alloc();
  if (!g_dict_template) {
    return "alloc failed";
  }
  for (size_t j = 0; j < g_num_dicts; j++) {
    uint64_t wlen = wuffs_brotli__encoder__add_dictionary_workbuf_len(
        g_dict_template, g_dict_lens[j]);
    if (!grow_workbuf(wlen)) {
      return "malloc failed";
    }
    wuffs_base__status status = wuffs_brotli__encoder__add_dictionary(
        g_dict_template, WUFFS_BROTLI__DICTIONARY_TYPE_RAW,
        wuffs_base__make_slice_u8((uint8_t*)g_dict_ptrs[j], g_dict_lens[j]),
        wuffs_base__make_slice_u8(g_workbuf, (size_t)wlen));
    if (status.repr) {
      return status.repr;
    }
  }
  g_dict_end = wuffs_brotli__encoder__workbuf_len(g_dict_template).max_incl;
  return NULL;
}

// encode_wuffs mirrors wuffs_brotli_encode_plan in test/c/std/brotli.c.
static const char*  //
encode_wuffs(const config* c, size_t* have_len) {
  wuffs_brotli__encoder* enc = wuffs_brotli__encoder__alloc();
  if (!enc) {
    return "alloc failed";
  }
  const char* ret = NULL;
  if (g_dict_template) {
    memcpy(enc, g_dict_template, sizeof__wuffs_brotli__encoder());
  } else if (g_num_dicts > 0) {
    for (size_t j = 0; j < g_num_dicts; j++) {
      uint64_t n = wuffs_brotli__encoder__add_dictionary_workbuf_len(
          enc, g_dict_lens[j]);
      if (!grow_workbuf(n)) {
        ret = "malloc failed";
        goto cleanup;
      }
      wuffs_base__status status = wuffs_brotli__encoder__add_dictionary(
          enc, WUFFS_BROTLI__DICTIONARY_TYPE_RAW,
          wuffs_base__make_slice_u8((uint8_t*)g_dict_ptrs[j], g_dict_lens[j]),
          wuffs_base__make_slice_u8(g_workbuf, (size_t)n));
      if (status.repr) {
        ret = status.repr;
        goto cleanup;
      }
    }
  }
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
  if (wlen > ((uint64_t)3 << 30)) {
    ret = "workbuf_len is too large";
    goto cleanup;
  } else if (!grow_workbuf(wlen)) {
    ret = "malloc failed";
    goto cleanup;
  }
  g_last_wlen = wlen;
  if ((g_short_workbuf_len > 0) && (g_short_workbuf_len < wlen)) {
    wlen = g_short_workbuf_len;
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
  for (size_t j = 0; j < g_num_dicts; j++) {
    if (!BrotliEncoderAttachPreparedDictionary(state, g_dict_prepared[j])) {
      ret = "BrotliEncoderAttachPreparedDictionary failed";
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
  k = (int)strlen(buf);
  if ((g_num_dicts > 0) && (k > 0) && ((size_t)k < n)) {
    snprintf(buf + k, n - (size_t)k, " dicts=%s", g_dict_desc);
  }
}

// short_workbuf_len returns the shortest workbuf (at qualities 2 to 9) that
// should work for the config c and an input of len bytes, given that
// workbuf_len is wlen, or 0 if it is not shorter than wlen. The ring buffer
// is the workbuf's last region, of (2 + (1 << rb_bits) + (1 << lgblock) + 7)
// bytes, and a shorter workbuf works if it holds (2 + len + 16) bytes of it
// and (len + 16) is at most (1 << rb_bits). This mirrors SanitizeParams,
// ComputeLgBlock and ComputeRbBits in quality.h.
static size_t  //
short_workbuf_len(const config* c, size_t len, uint64_t wlen) {
  int32_t quality = 11;
  int32_t lgwin = 22;
  int32_t lgblock = 0;
  bool large_window = false;
  for (size_t j = 0; j < c->num_params; j++) {
    int32_t v = (int32_t)(c->values[j]);
    switch (c->keys[j]) {
      case 1:
        quality = v;
        break;
      case 2:
        lgwin = v;
        break;
      case 3:
        lgblock = v;
        break;
      case 6:
        large_window = v != 0;
        break;
    }
  }
  quality = (quality < 0) ? 0 : (quality > 11) ? 11 : quality;
  if ((quality < 2) || (quality > 9)) {
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
  return (n < wlen) ? (size_t)n : 0;
}

static const char*  //
roundtrip(const config* c, const uint8_t* data, size_t len, size_t want_len);

static void  //
check(const char* name, const config* c, const uint8_t* data, size_t len) {
  char desc[768];
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
  g_last_want_len = want_len;
  if (c->try_short && !have_err && !want_err) {
    // Also check the shortest workbuf that should work, and that one byte
    // less fails.
    uint64_t wlen = g_last_wlen;
    size_t short_len = short_workbuf_len(c, len, wlen);
    if (short_len > 0) {
      g_num_short_checks++;
      g_short_workbuf_len = short_len - 1;
      size_t too_short_len = 0;
      const char* too_short_err = encode_wuffs(c, &too_short_len);
      g_short_workbuf_len = short_len;
      have_err = encode_wuffs(c, &have_len);
      g_short_workbuf_len = 0;
      if (!too_short_err ||
          (too_short_err != wuffs_base__error__bad_workbuf_length)) {
        g_num_mismatches++;
        describe(desc, sizeof desc, name, c);
        printf("MISMATCH %s: a workbuf of %zu bytes did not fail: %s\n",
               desc, short_len - 1, too_short_err ? too_short_err : "ok");
        fflush(stdout);
        return;
      } else if (have_err) {
        have_err = "short workbuf failed";
      }
    }
  }
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
    return;
  }
  if (g_num_dicts > 0) {
    // A stream offset makes the output a continuation of another stream, so
    // it cannot be decoded on its own.
    bool stream_offset = false;
    for (size_t j = 0; j < c->num_params; j++) {
      if ((c->keys[j] == 9) && c->values[j]) {
        stream_offset = true;
      }
    }
    if (!stream_offset) {
      g_num_roundtrips++;
      const char* rt_err = roundtrip(c, data, len, want_len);
      if (rt_err) {
        g_num_mismatches++;
        describe(desc, sizeof desc, name, c);
        printf("ROUNDTRIP FAILURE %s: %s\n", desc, rt_err);
        fflush(stdout);
      }
    }
  }
}

// roundtrip checks that both libbrotlidec and Wuffs' decoder (with the same
// dictionaries attached) decode g_want (of want_len bytes) to data.
static const char*  //
roundtrip(const config* c, const uint8_t* data, size_t len, size_t want_len) {
  static uint8_t* out = NULL;
  static uint8_t* dec_workbuf = NULL;
  static size_t dec_workbuf_len = 0;
  if (!out) {
    out = malloc(MAX_INPUT);
    if (!out) {
      return "malloc failed";
    }
  }
  bool lw = false;
  for (size_t j = 0; j < c->num_params; j++) {
    if (c->keys[j] == 6) {
      lw = c->values[j] != 0;
    }
  }

  // libbrotlidec.
  BrotliDecoderState* d = BrotliDecoderCreateInstance(NULL, NULL, NULL);
  if (!d) {
    return "BrotliDecoderCreateInstance failed";
  }
  BrotliDecoderSetParameter(d, BROTLI_DECODER_PARAM_LARGE_WINDOW, lw ? 1 : 0);
  for (size_t j = 0; j < g_num_dicts; j++) {
    if (!BrotliDecoderAttachDictionary(d, BROTLI_SHARED_DICTIONARY_RAW,
                                       g_dict_lens[j], g_dict_ptrs[j])) {
      BrotliDecoderDestroyInstance(d);
      return "BrotliDecoderAttachDictionary failed";
    }
  }
  size_t avail_in = want_len;
  const uint8_t* next_in = g_want;
  size_t avail_out = MAX_INPUT;
  uint8_t* next_out = out;
  BrotliDecoderResult r = BrotliDecoderDecompressStream(
      d, &avail_in, &next_in, &avail_out, &next_out, NULL);
  BrotliDecoderDestroyInstance(d);
  if (r != BROTLI_DECODER_RESULT_SUCCESS) {
    return "libbrotlidec failed";
  } else if (((MAX_INPUT - avail_out) != len) || (len && memcmp(out, data, len))) {
    return "libbrotlidec round trip mismatch";
  }

  // Wuffs' decoder.
  wuffs_brotli__decoder* dec = wuffs_brotli__decoder__alloc();
  if (!dec) {
    return "alloc failed";
  }
  const char* ret = NULL;
  wuffs_brotli__decoder__set_quirk(dec, WUFFS_BROTLI__QUIRK_LARGE_WINDOW,
                                   lw ? 1 : 0);
  size_t need = g_dict_total + ((size_t)1 << 30) + 542;
  if (need > dec_workbuf_len) {
    free(dec_workbuf);
    dec_workbuf = malloc(need);
    dec_workbuf_len = dec_workbuf ? need : 0;
    if (!dec_workbuf) {
      free(dec);
      return "malloc failed";
    }
  }
  wuffs_base__slice_u8 wb = wuffs_base__make_slice_u8(dec_workbuf, dec_workbuf_len);
  for (size_t j = 0; j < g_num_dicts; j++) {
    wuffs_base__status status = wuffs_brotli__decoder__add_dictionary(
        dec, WUFFS_BROTLI__DICTIONARY_TYPE_RAW,
        wuffs_base__make_slice_u8((uint8_t*)g_dict_ptrs[j], g_dict_lens[j]), wb);
    if (status.repr) {
      ret = status.repr;
      goto cleanup;
    }
  }
  uint64_t wlen = wuffs_brotli__decoder__workbuf_len(dec).max_incl;
  if (wlen > dec_workbuf_len) {
    ret = "decoder workbuf_len is too large";
    goto cleanup;
  }
  wuffs_base__io_buffer dst = wuffs_base__ptr_u8__writer(out, MAX_INPUT);
  wuffs_base__io_buffer src = wuffs_base__ptr_u8__reader(g_want, want_len, true);
  wuffs_base__status status = wuffs_brotli__decoder__transform_io(
      dec, &dst, &src, wuffs_base__make_slice_u8(dec_workbuf, (size_t)wlen));
  if (status.repr) {
    ret = status.repr;
  } else if ((dst.meta.wi != len) || (len && memcmp(out, data, len))) {
    ret = "Wuffs decoder round trip mismatch";
  }
cleanup:
  free(dec);
  return ret;
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
run_input_q01(const char* name, const uint8_t* data, size_t len, uint32_t q) {
  static const uint32_t lgwins[] = {10, 16, 18, 22, 24};
  static const size_t metadata_lens[] = {0, 1, 100, 70000};

  // The main grid.
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

  // Metadata, size_hint, other parameters and limited dst lengths.
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
      {5, 0},           // BROTLI_PARAM_SIZE_HINT.
      {5, 0xFFFFFFFF},  // Replaced by the exact size.
      {5, 1u << 30},    //
      {3, 16},          // BROTLI_PARAM_LGBLOCK.
      {3, 24},          //
      {4, 1},           // BROTLI_PARAM_DISABLE_LITERAL_CONTEXT_MODELING.
      {6, 1},           // BROTLI_PARAM_LARGE_WINDOW.
      {7, 2},           // BROTLI_PARAM_NPOSTFIX.
      {8, 12},          // BROTLI_PARAM_NDIRECT.
      {9, 1},           // BROTLI_PARAM_STREAM_OFFSET.
      {9, 1000},        //
      {9, 1u << 30},    //
      {2, 0xFFFFFFFF},  // LGWIN (int)-1.
      {2, 30},          //
      {1, 0xFFFFFFFF},  // QUALITY (int)-1, so quality 0.
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

// run_input_lw runs the large-window (BROTLI_PARAM_LARGE_WINDOW) grid for a
// quality from 2 to 9. At quality 2 (and below), the C code ignores the
// large-window parameter. At qualities 3 to 9, lgwin can be up to 30 and lgwin
// above 24 selects the composite hashers (H35, H55 and H65) instead of H3,
// H54, H6 and H68 (and only those: H5, H58 and H40 to H42 stay as they are).
static void  //
run_input_lw(const char* name, const uint8_t* data, size_t len, uint32_t q) {
  static const size_t metadata_lens[] = {0, 1, 100, 70000};
  bool big = len > (512 << 10);

  // lgwin and chunking.
  static const uint32_t lgwins[] = {10, 18, 24, 25, 26, 27, 28, 29, 30};
  for (size_t w = 0; w < 9; w++) {
    if (big && (lgwins[w] != 24) && (lgwins[w] != 25) && (lgwins[w] != 28) &&
        (lgwins[w] != 30)) {
      continue;
    }
    for (uint32_t ch = 0; ch < 5; ch += 4) {
      for (uint32_t fl = 0; fl < 3; fl += 2) {
        if (big && (fl == 0)) {
          continue;
        }
        config c = make_config(q, lgwins[w]);
        add_param(&c, 6, 1);  // BROTLI_PARAM_LARGE_WINDOW.
        c.chunking = ch;
        c.flush = fl;
        c.seed = 0x10000 | (w << 8) | (ch << 4) | fl | (q << 12);
        c.try_short = true;
        check(name, &c, data, len);
      }
    }
  }

  // Other parameters, alone and in combination.
  static const uint32_t extras[][6] = {
      // Pairs of (BrotliEncoderParameter, value). A zero key ends the list.
      {5, 1u << 20},             // BROTLI_PARAM_SIZE_HINT: H55 and H65.
      {5, 0xFFFFFFFF},           // Replaced by the exact size.
      {5, 1u << 30},             //
      {3, 16},                   // BROTLI_PARAM_LGBLOCK.
      {3, 18},                   //
      {3, 24, 5, 1u << 21},      // ... and a big size hint.
      {4, 1, 5, 1u << 20},       // DISABLE_LITERAL_CONTEXT_MODELING.
      {7, 3, 8, 120},            // BROTLI_PARAM_NPOSTFIX and NDIRECT.
      {7, 1, 8, 4, 5, 1u << 20}, //
      {0, 2},                    // FONT mode.
      {0, 1, 5, 1u << 22},       // TEXT mode and a big size hint.
      {9, 1000},                 // BROTLI_PARAM_STREAM_OFFSET.
      {9, 1u << 30, 5, 1u << 20},
      {2, 0xFFFFFFFF},           // LGWIN (int)-1, so 10.
      {2, 31},                   // LGWIN 31, so 30.
  };
  for (size_t i = 0; i < sizeof extras / sizeof extras[0]; i++) {
    for (size_t w = 0; w < 2; w++) {
      static const uint32_t ws[] = {25, 30};
      if (big && (w != 0)) {
        continue;
      }
      config c = make_config(q, ws[w]);
      add_param(&c, 6, 1);  // BROTLI_PARAM_LARGE_WINDOW.
      for (size_t k = 0; (k < 6) && ((k == 0) || extras[i][k]); k += 2) {
        uint32_t v = extras[i][k + 1];
        if ((extras[i][k] == 5) && (v == 0xFFFFFFFF)) {
          v = (uint32_t)len;
        }
        add_param(&c, extras[i][k], v);
      }
      c.chunking = 4;
      c.flush = 2;
      c.seed = 0x20000 | i | (w << 8) | (q << 12);
      c.try_short = true;
      check(name, &c, data, len);
    }
  }

  // Metadata blocks and limited dst lengths.
  for (size_t i = 0; i < 4; i += 3) {
    config c = make_config(q, 26);
    add_param(&c, 6, 1);  // BROTLI_PARAM_LARGE_WINDOW.
    c.chunking = 4;
    c.flush = 2;
    c.metadata_position = 2;
    c.metadata_len = metadata_lens[i];
    c.dst_chunk = (len <= (32 << 10)) ? 3 : 4096;
    c.seed = 0x30000 | i | (q << 12);
    check(name, &c, data, len);
  }
}

// run_input_q2 runs the grid for a quality from 2 to 9.
static void  //
run_input_q2(const char* name, const uint8_t* data, size_t len, uint32_t q) {
  static const uint32_t lgwins[] = {10, 16, 18, 22, 24};
  static const size_t metadata_lens[] = {0, 1, 100, 70000};
  bool big = len > (512 << 10);

  // The main grid.
  for (size_t w = 0; w < 5; w++) {
    for (uint32_t mode = 0; mode <= 2; mode++) {
      if (big && (mode == 1)) {
        continue;
      }
      for (uint32_t ch = 0; ch < 5; ch++) {
        if ((ch == 1) && (len > 4096)) {
          continue;
        } else if (big && ((ch == 2) || (ch == 3))) {
          continue;
        }
        for (uint32_t fl = 0; fl < 3; fl++) {
          if (big && (fl == 1)) {
            continue;
          }
          config c = make_config(q, lgwins[w]);
          add_param(&c, 0, mode);  // BROTLI_PARAM_MODE.
          c.chunking = ch;
          c.flush = fl;
          c.seed = (w << 8) | (mode << 4) | fl | ((uint64_t)len << 16) | q;
          check(name, &c, data, len);
        }
      }
    }
  }

  // Metadata blocks and limited dst lengths.
  for (size_t i = 0; i < 4; i++) {
    for (uint32_t pos = 1; pos <= 3; pos++) {
      for (uint32_t fl = 0; fl < 3; fl++) {
        if (big && (fl != 2)) {
          continue;
        }
        config c = make_config(q, (pos == 2) ? 18 : 16);
        c.chunking = (pos == 3) ? 0 : 4;
        c.flush = fl;
        c.metadata_position = pos;
        c.metadata_len = metadata_lens[i];
        c.dst_chunk = (fl == 2) ? 5 : 0;
        c.seed = (i << 8) | pos | (q << 12);
        check(name, &c, data, len);
      }
    }
  }

  // Other parameters, alone and in combination.
  static const uint32_t extras[][6] = {
      // Pairs of (BrotliEncoderParameter, value). A zero key ends the list.
      {5, 0},                    // BROTLI_PARAM_SIZE_HINT.
      {5, 0xFFFFFFFF},           // Replaced by the exact size.
      {5, 1},                    //
      {5, 1u << 20},             //
      {5, 1u << 30},             //
      {3, 16},                   // BROTLI_PARAM_LGBLOCK.
      {3, 17},                   //
      {3, 20},                   //
      {3, 24},                   //
      {3, 0xFFFFFFFF},           // LGBLOCK (int)-1.
      {4, 1},                    // DISABLE_LITERAL_CONTEXT_MODELING.
      {4, 1, 0, 1},              // ... and TEXT mode.
      {7, 1},                    // BROTLI_PARAM_NPOSTFIX.
      {7, 2, 8, 12},             // ... and BROTLI_PARAM_NDIRECT.
      {7, 3, 8, 120},            //
      {7, 0, 8, 15},             //
      {7, 3, 8, 0},              //
      {7, 1, 8, 4},              //
      {0, 2},                    // FONT mode.
      {0, 2, 7, 3, 8, 30},       // FONT mode with explicit distance params.
      {9, 1},                    // BROTLI_PARAM_STREAM_OFFSET.
      {9, 1000},                 //
      {9, 1u << 30},             //
      {2, 0xFFFFFFFF},           // LGWIN (int)-1.
      {2, 30},                   //
      {5, 0xFFFFFFFF, 3, 18},    // Exact size hint and LGBLOCK 18.
      {5, 1u << 21, 4, 1},       // Big size hint, no context modeling.
  };
  for (size_t i = 0; i < sizeof extras / sizeof extras[0]; i++) {
    for (uint32_t ch = 0; ch < 5; ch += 4) {
      for (size_t w = 0; w < 3; w++) {
        static const uint32_t ws[] = {16, 22, 24};
        if (big && (w != 1)) {
          continue;
        }
        config c = make_config(q, ws[w]);
        for (size_t k = 0; (k < 6) && ((k == 0) || extras[i][k]); k += 2) {
          uint32_t v = extras[i][k + 1];
          if ((extras[i][k] == 5) && (v == 0xFFFFFFFF)) {
            v = (uint32_t)len;
          }
          add_param(&c, extras[i][k], v);
        }
        c.chunking = ch;
        c.flush = 2;
        c.seed = i | (w << 8) | (q << 12);
        check(name, &c, data, len);
      }
    }
  }
  run_input_lw(name, data, len, q);
  if (len <= (256 << 10)) {
    static const size_t dst_chunks[] = {1, 2, 3, 7, 64, 4096};
    for (size_t i = 0; i < 6; i++) {
      for (uint32_t fl = 0; fl < 3; fl++) {
        if ((dst_chunks[i] < 7) && (len > (32 << 10))) {
          continue;
        }
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

// run_dict_configs runs the dictionary grid's configurations for one
// quality and the current dictionaries. level 0 is the full grid, level 1 a
// smaller one and level 2 a much smaller one (for long inputs).
static void  //
run_dict_configs(const char* name, const uint8_t* data, size_t len, uint32_t q,
                 int level) {
  static const uint32_t lgwins[] = {10, 16, 22, 24};
  for (size_t w = 0; w < 4; w++) {
    for (uint32_t ch = 0; ch < 5; ch += 4) {
      for (uint32_t fl = 0; fl < 3; fl += 2) {
        if ((level >= 1) && ((w == 1) || ((ch == 0) && (fl == 2)))) {
          continue;
        } else if ((level >= 2) && ((w == 0) || (ch != fl * 2))) {
          continue;
        }
        config c = make_config(q, lgwins[w]);
        c.chunking = ch;
        c.flush = fl;
        c.seed = 0x60000 | (w << 8) | (ch << 4) | fl | (q << 12);
        check(name, &c, data, len);
      }
    }
  }

  static const uint32_t extras[][6] = {
      // Pairs of (BrotliEncoderParameter, value). A zero key ends the list.
      {5, 0xFFFFFFFF},            // BROTLI_PARAM_SIZE_HINT: the exact size.
      {5, 1u << 20},              // H54 (quality 4), H6 or H68.
      {5, 1},                     //
      {9, 1},                     // BROTLI_PARAM_STREAM_OFFSET.
      {9, 1000},                  //
      {9, 1u << 30},              //
      {6, 1, 2, 18},              // BROTLI_PARAM_LARGE_WINDOW.
      {6, 1, 2, 30},              //
      {6, 1, 2, 25, 5, 1u << 20}, // H35, H55 or H65.
      {9, 1000, 6, 1, 2, 26},     // Stream offset and large window.
      {3, 16},                    // BROTLI_PARAM_LGBLOCK.
      {3, 24, 5, 1u << 21},       //
      {7, 2, 8, 12},              // BROTLI_PARAM_NPOSTFIX and NDIRECT.
      {0, 2},                     // FONT mode.
      {4, 1},                     // DISABLE_LITERAL_CONTEXT_MODELING.
  };
  for (size_t i = 0; i < sizeof extras / sizeof extras[0]; i++) {
    for (uint32_t ch = 0; ch < 5; ch += 4) {
      if ((level >= 1) && (ch == 0)) {
        continue;
      } else if ((level >= 2) && (i != 1) && (i != 4) && (i != 8)) {
        continue;
      }
      config c = make_config(q, 22);
      for (size_t k = 0; (k < 6) && ((k == 0) || extras[i][k]); k += 2) {
        uint32_t v = extras[i][k + 1];
        if ((extras[i][k] == 5) && (v == 0xFFFFFFFF)) {
          v = (uint32_t)len;
        }
        add_param(&c, extras[i][k], v);
      }
      c.chunking = ch;
      c.flush = 2;
      c.seed = 0x70000 | i | (ch << 8) | (q << 12);
      check(name, &c, data, len);
    }
  }
  if (level >= 2) {
    return;
  }

  // Metadata blocks, limited dst lengths and a flush after every chunk.
  for (uint32_t pos = 1; pos <= 2; pos++) {
    config c = make_config(q, 18);
    c.chunking = 4;
    c.flush = 2;
    c.metadata_position = pos;
    c.metadata_len = (pos == 1) ? 0 : 100;
    c.seed = 0x80000 | pos | (q << 12);
    check(name, &c, data, len);
  }
  {
    config c = make_config(q, 16);
    c.chunking = 4;
    c.flush = 0;
    c.dst_chunk = (len <= (32 << 10)) ? 3 : 4096;
    c.seed = 0x90000 | (q << 12);
    check(name, &c, data, len);
  }
  if (len <= (8 << 10)) {
    config c = make_config(q, 22);
    c.chunking = 2;
    c.flush = 1;
    c.seed = 0xA0000 | (q << 12);
    check(name, &c, data, len);
  }
}

// check_smaller checks that, with the current dictionaries (which should
// hold much of data), libbrotlienc's output (which Wuffs' output matches) is
// smaller than without them (unless that is already tiny).
static void  //
check_smaller(const char* name, const uint8_t* data, size_t len, uint32_t q) {
  config c = make_config(q, 22);
  check(name, &c, data, len);
  size_t with_len = g_last_want_len;
  size_t saved = g_num_dicts;
  size_t without_len = 0;
  g_num_dicts = 0;
  const char* err = make_plan(&c, data, len);
  if (!err) {
    err = encode_mimic(&c, &without_len);
  }
  g_num_dicts = saved;
  g_num_smaller_checks++;
  // Tiny outputs (e.g. for long runs of zeroes) cannot shrink.
  if (err || (with_len > without_len) ||
      ((without_len >= 64) && (with_len >= without_len))) {
    g_num_mismatches++;
    printf("NOT SMALLER %s: q=%u dicts=%s: %zu bytes with, %zu without: %s\n",
           name, q, g_dict_desc, with_len, without_len, err ? err : "");
    fflush(stdout);
  }
}

// run_input_dict runs the dictionary grid (several sets of raw dictionaries,
// mostly made from the input, so that there are matches) for an input.
static void  //
run_input_dict(const char* name, const uint8_t* data, size_t len) {
  uint64_t configs0 = g_num_configs;
  uint64_t mismatches0 = g_num_mismatches;
  time_t start = time(NULL);
  static uint8_t noise[1000];
  fill_random(noise, sizeof noise, 0xD1C7);
  static const char text[] = "the quick brown fox jumps over the lazy dog";
  bool big = len > (256 << 10);

  for (int set = 0; set < 7; set++) {
    const uint8_t* ptrs[MAX_DICTS];
    size_t lens[MAX_DICTS];
    size_t n = 0;
    int level = big ? 2 : 1;
    switch (set) {
      case 0:  // A prefix of the input.
        ptrs[n] = data;
        lens[n++] = len / 2;
        snprintf(g_dict_desc, sizeof g_dict_desc, "prefix");
        level = big ? 2 : 0;
        break;
      case 1:  // The whole input.
        ptrs[n] = data;
        lens[n++] = len;
        snprintf(g_dict_desc, sizeof g_dict_desc, "whole");
        level = big ? 2 : 0;
        break;
      case 2:  // Another file.
        ptrs[n] = g_foreign_dict;
        lens[n++] = g_foreign_dict_len;
        snprintf(g_dict_desc, sizeof g_dict_desc, "enwik5");
        break;
      case 3:  // Noise and the input's middle third.
        ptrs[n] = noise;
        lens[n++] = sizeof noise;
        ptrs[n] = data + (len / 3);
        lens[n++] = (len / 3);
        snprintf(g_dict_desc, sizeof g_dict_desc, "noise+middle");
        break;
      case 4: {  // 15 (the maximum number of) pieces of the input.
        if (len == 0) {
          continue;
        }
        size_t k = (len + 14) / 15;
        for (size_t j = 0; j < 15; j++) {
          size_t at = (j * k) % len;
          ptrs[n] = data + at;
          lens[n++] = ((len - at) < k) ? (len - at) : k;
        }
        snprintf(g_dict_desc, sizeof g_dict_desc, "15 pieces of %zu", k);
        break;
      }
      case 5:  // Tiny dictionaries (and an empty one, first).
        ptrs[n] = (const uint8_t*)text;
        lens[n++] = 0;
        ptrs[n] = (const uint8_t*)text;
        lens[n++] = 1;
        ptrs[n] = (const uint8_t*)text + 4;
        lens[n++] = 7;
        ptrs[n] = (const uint8_t*)text + 10;
        lens[n++] = 8;
        ptrs[n] = (len >= 9) ? data + (len - 9) : (const uint8_t*)text;
        lens[n++] = 9;
        snprintf(g_dict_desc, sizeof g_dict_desc, "tiny");
        break;
      case 6:  // The input's two halves, swapped.
        ptrs[n] = data + (len / 2);
        lens[n++] = len - (len / 2);
        ptrs[n] = data;
        lens[n++] = len / 2;
        ptrs[n] = (const uint8_t*)text;
        lens[n++] = sizeof text - 1;
        snprintf(g_dict_desc, sizeof g_dict_desc, "swapped halves+text");
        break;
    }
    const char* err = set_dicts(n, ptrs, lens);
    if (err) {
      g_num_mismatches++;
      printf("FAILURE %s: set_dicts(%s): %s\n", name, g_dict_desc, err);
      continue;
    }
    for (uint32_t q = g_qmin; q <= g_qmax; q++) {
      run_dict_configs(name, data, len, q, level);
      if ((set == 1) && (len >= 1000) && (q >= 5)) {
        check_smaller(name, data, len, q);
      }
    }
  }
  set_dicts(0, NULL, NULL);
  printf("%-50s %8zu bytes: %6" PRIu64 " configs, %" PRIu64
         " mismatches (%lds)\n",
         name, len, g_num_configs - configs0, g_num_mismatches - mismatches0,
         (long)(time(NULL) - start));
  fflush(stdout);
}

// run_big_dicts checks dictionaries of 2 to 40 MiB (every hash table size
// class of CreatePreparedDictionary), with pieces of a 1 MiB input copied
// into them at various offsets.
static void  //
run_big_dicts(size_t which) {
  static const size_t sizes[] = {
      (2 << 20),     (2 << 20) + 1, (3 << 20) + 77, (5 << 20) + 3,
      (9 << 20) + 5, (17 << 20) + 1, (40 << 20) + 9,
  };
  if (which >= sizeof sizes / sizeof sizes[0]) {
    return;
  }
  uint64_t configs0 = g_num_configs;
  uint64_t mismatches0 = g_num_mismatches;
  time_t start = time(NULL);
  size_t dlen = sizes[which];
  size_t len = 1 << 20;
  uint8_t* input = malloc(len);
  uint8_t* dict = malloc(dlen);
  if (!input || !dict) {
    printf("FAILURE: malloc failed\n");
    g_num_mismatches++;
    free(input);
    free(dict);
    return;
  }
  fill_mixed(input, len, 0xB16 + which);
  fill_mixed(dict, dlen, 0xD1C + which);
  // Copy 64 KiB pieces of the input to the dictionary's start, middle and
  // end (with sparse changes).
  for (size_t j = 0; j < 3; j++) {
    size_t at = (j == 0) ? 1234 : (j == 1) ? (dlen / 2) : (dlen - (70 << 10));
    memcpy(dict + at, input + (j * (300 << 10)), 64 << 10);
    for (size_t i = 777; i < (64 << 10); i += 4099) {
      dict[at + i] ^= 0x33;
    }
  }
  char name[96];
  snprintf(name, sizeof name, "synthetic: 1 MiB with a %zu byte dictionary",
           dlen);
  const uint8_t* ptrs[2] = {dict, input + (900 << 10)};
  size_t lens[2] = {dlen, 1000};
  snprintf(g_dict_desc, sizeof g_dict_desc, "big %zu + 1000", dlen);
  const char* err = set_dicts(2, ptrs, lens);
  if (err) {
    g_num_mismatches++;
    printf("FAILURE %s: set_dicts: %s\n", name, err);
  } else {
    for (uint32_t q = (g_qmin > 2) ? g_qmin : 2; q <= g_qmax; q++) {
      static const uint32_t lgwins[] = {16, 24};
      for (size_t w = 0; w < 2; w++) {
        config c = make_config(q, lgwins[w]);
        c.seed = 0xB0000 | (w << 8) | (q << 12);
        check(name, &c, input, len);
        c.chunking = 4;
        c.flush = 2;
        check(name, &c, input, len);
      }
      static const uint32_t extras[][6] = {
          {6, 1, 2, 30, 5, 1u << 20},
          {9, 1000},
          {5, 1u << 20},
      };
      for (size_t i = 0; i < 3; i++) {
        config c = make_config(q, 22);
        for (size_t k = 0; (k < 6) && ((k == 0) || extras[i][k]); k += 2) {
          add_param(&c, extras[i][k], extras[i][k + 1]);
        }
        c.seed = 0xC0000 | i | (q << 12);
        check(name, &c, input, len);
      }
      if (q >= 5) {
        check_smaller(name, input, len, q);
      }
    }
  }
  set_dicts(0, NULL, NULL);
  free(input);
  free(dict);
  printf("%-50s %8zu bytes: %6" PRIu64 " configs, %" PRIu64
         " mismatches (%lds)\n",
         name, len, g_num_configs - configs0, g_num_mismatches - mismatches0,
         (long)(time(NULL) - start));
  fflush(stdout);
}

static void  //
run_input(const char* name, const uint8_t* data, size_t len) {
  if (g_dict_only) {
    run_input_dict(name, data, len);
    return;
  }
  uint64_t configs0 = g_num_configs;
  uint64_t mismatches0 = g_num_mismatches;
  time_t start = time(NULL);
  for (uint32_t q = g_qmin; q <= g_qmax; q++) {
    if (g_lw_only) {
      if (q >= 2) {
        run_input_lw(name, data, len, q);
      }
    } else if (q <= 1) {
      run_input_q01(name, data, len, q);
    } else {
      run_input_q2(name, data, len, q);
    }
  }
  printf("%-50s %8zu bytes: %6" PRIu64 " configs, %" PRIu64
         " mismatches (%lds)\n",
         name, len, g_num_configs - configs0, g_num_mismatches - mismatches0,
         (long)(time(NULL) - start));
  fflush(stdout);
}

// run_input_far checks a few large-window configurations (one-shot and
// streaming) for a very long input.
static void  //
run_input_far(const char* name, const uint8_t* data, size_t len) {
  uint64_t configs0 = g_num_configs;
  uint64_t mismatches0 = g_num_mismatches;
  time_t start = time(NULL);
  for (uint32_t q = (g_qmin > 2) ? g_qmin : 2; q <= g_qmax; q++) {
    static const uint32_t lgwins[] = {24, 25, 26, 30};
    for (size_t w = 0; w < 4; w++) {
      config c = make_config(q, lgwins[w]);
      add_param(&c, 6, 1);  // BROTLI_PARAM_LARGE_WINDOW.
      c.seed = 0x40000 | (w << 8) | (q << 12);
      c.try_short = true;
      check(name, &c, data, len);
      printf("  q=%u lgwin=%u: %zu compressed bytes\n", q, lgwins[w],
             g_last_want_len);
    }
    // Streaming, with a flush and a size hint below 1 MiB (so H35 at
    // quality 3 and H58 or H5 at qualities 5 to 9, but H3 at quality 4).
    config c = make_config(q, 30);
    add_param(&c, 6, 1);       // BROTLI_PARAM_LARGE_WINDOW.
    add_param(&c, 5, 12345);  // BROTLI_PARAM_SIZE_HINT.
    c.chunking = 4;
    c.flush = 2;
    c.seed = 0x50000 | (q << 12);
    check(name, &c, data, len);
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
  g_workbuf = malloc(g_workbuf_len);  // It grows as needed.
  if (!g_src || !g_have || !g_want || !g_workbuf) {
    fprintf(stderr, "malloc failed\n");
    return 1;
  }

  int shard_k = 0;
  int shard_n = 1;
  bool big = false;
  int argi = 1;
  for (; argi < argc; argi++) {
    unsigned a = 0;
    unsigned b = 0;
    if (sscanf(argv[argi], "-q=%u-%u", &a, &b) == 2) {
      g_qmin = a;
      g_qmax = (b < 9) ? b : 9;
    } else if (strcmp(argv[argi], "-lw") == 0) {
      g_lw_only = true;
    } else if (strcmp(argv[argi], "-dict") == 0) {
      g_dict_only = true;
    } else if (strcmp(argv[argi], "-big") == 0) {
      big = true;
    } else if (sscanf(argv[argi], "-shard=%u/%u", &a, &b) == 2) {
      shard_k = (int)a;
      shard_n = (b > 0) ? (int)b : 1;
    } else {
      break;
    }
  }

  char* names[1024];
  int num_names = 0;
  if (argi < argc) {
    for (int i = argi; (i < argc) && (num_names < 1024); i++) {
      names[num_names++] = strdup(argv[i]);
    }
  } else {
    names[num_names++] = strdup("test/data/256.bytes");
    names[num_names++] = strdup("test/data/enwik5");
    names[num_names++] = strdup("test/data/romeo.txt");
    names[num_names++] = strdup("test/data/pi.txt");
    names[num_names++] = strdup("test/data/nobel-prizes.json");
    names[num_names++] = strdup("test/data/archive.iso");
    names[num_names++] = strdup("test/data/hibiscus.regular.bmp");
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

  g_foreign_dict = read_whole_file("test/data/enwik5", &g_foreign_dict_len);
  if (!g_foreign_dict) {
    fprintf(stderr, "could not read test/data/enwik5\n");
    return 1;
  }

  int input_index = 0;
  if (g_dict_only && (argi >= argc)) {
    for (size_t j = 0; j < 7; j++) {
      if (((input_index++) % shard_n) == shard_k) {
        run_big_dicts(j);
      }
    }
  }
  if (big) {
    // Synthetic inputs of several MiB (with the usual grid) and of 24 to 70
    // MiB (with run_input_far's configurations), with repeats at distances
    // above 16 MiB (1 << 24, the largest non-large-window window).
    num_names = 0;
    uint8_t* p = malloc(70 << 20);
    if (!p) {
      return 1;
    }
    static const struct {
      size_t mib;
      size_t near_mib;
      size_t far_mib;
      bool far;
    } bigs[] = {
        {3, 2, 2, false},     {6, 2, 5, false},    {12, 3, 10, false},
        {24, 3, 20, true},    {40, 3, 32, true},   {70, 5, 40, true},
    };
    for (size_t j = 0; j < sizeof bigs / sizeof bigs[0]; j++) {
      if (((input_index++) % shard_n) != shard_k) {
        continue;
      }
      size_t n = (bigs[j].mib << 20) - 12345;
      fill_far_repeats(p, n, bigs[j].near_mib, bigs[j].far_mib, 0x777 + j);
      char name[96];
      snprintf(name, sizeof name,
               "synthetic: %zu MiB with repeats at %zu and %zu MiB",
               bigs[j].mib, bigs[j].near_mib, bigs[j].far_mib);
      if (bigs[j].far) {
        run_input_far(name, p, n);
      } else {
        run_input(name, p, n);
      }
    }
    free(p);
  }
  for (int i = 0; i < num_names; i++) {
    if (((input_index++) % shard_n) != shard_k) {
      continue;
    }
    size_t len = 0;
    uint8_t* data = read_whole_file(names[i], &len);
    if (!data) {
      fprintf(stderr, "could not read %s\n", names[i]);
      return 1;
    }
    run_input(names[i], data, len);
    free(data);
  }

  if ((argi >= argc) && !big) {
    // Synthetic inputs.
    uint8_t* p = malloc(1 << 20);
    if (!p) {
      return 1;
    }
    if (((input_index++) % shard_n) == shard_k) {
      memset(p, 0, 1 << 20);
      run_input("synthetic: 1 MiB of zeroes", p, 1 << 20);
    }
    if (((input_index++) % shard_n) == shard_k) {
      fill_random(p, 256 << 10, 0x1234);
      run_input("synthetic: 256 KiB of pseudo-random bytes", p, 256 << 10);
    }
    if (((input_index++) % shard_n) == shard_k) {
      uint8_t pattern[100];
      fill_random(pattern, 100, 0x5678);
      for (size_t i = 0; i < (64 << 10); i++) {
        p[i] = pattern[i % 100];
      }
      run_input("synthetic: a repeated 100-byte pattern", p, 64 << 10);
    }
    if (((input_index++) % shard_n) == shard_k) {
      fill_mixed(p, 600 << 10, 0xABCD);
      run_input("synthetic: 600 KiB of mixed text and binary", p, 600 << 10);
    }
    for (size_t n = 0; n <= 40; n++) {
      if (((input_index++) % shard_n) != shard_k) {
        continue;
      }
      fill_random(p, n, 0x9999 + n);
      char name[64];
      snprintf(name, sizeof name, "synthetic: %zu pseudo-random bytes", n);
      run_input(name, p, n);
    }
    free(p);
  }

  printf("TOTAL: %" PRIu64 " configs (%" PRIu64
         " also with a short workbuf, %" PRIu64 " round trips, %" PRIu64
         " smaller-with-dictionary checks), %" PRIu64 " mismatches\n",
         g_num_configs, g_num_short_checks, g_num_roundtrips,
         g_num_smaller_checks, g_num_mismatches);
  return g_num_mismatches ? 1 : 0;
}
