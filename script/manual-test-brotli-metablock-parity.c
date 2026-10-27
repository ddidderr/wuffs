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

// manual-test-brotli-metablock-parity.c checks that std/brotli's ports of
// libbrotlienc v1.2.1's bit cost, histogram, block splitter, clustering and
// metablock functions (exposed for testing by the brotli.metablock_tester
// struct) produce bit-for-bit identical results to the C functions
// (BrotliBitsEntropy, BrotliPopulationCost*, BrotliSplitBlock,
// BrotliClusterHistograms*, BrotliBuildMetaBlock, BrotliOptimizeHistograms,
// etc.), which are BROTLI_INTERNAL but linkable from the static libraries.
//
// Run it from the Wuffs root directory with (as one command line):
//
//   gcc -O2 -std=c99 -I/path/to/brotli/c -I/path/to/brotli/c/include
//       script/manual-test-brotli-metablock-parity.c
//       /path/to/build/libbrotlienc.a /path/to/build/libbrotlicommon.a
//       -lm -o /tmp/brotli-metablock-parity
//   /tmp/brotli-metablock-parity [-quick]
//
// where /path/to/brotli is a checkout of the brotli repository at tag v1.2.1
// (the internal headers under c/enc are needed) and /path/to/build is a CMake
// build of it with BUILD_SHARED_LIBS=OFF. The C code's doubles are compared
// as their bit patterns, so the reference must be built for x86-64 with the
// default flags (SSE2, no FMA contraction) and linked against glibc's log2.
//
// It prints each mismatch and a summary, and exits with a non-zero status if
// there were any mismatches.

#define _POSIX_C_SOURCE 200809L

#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "enc/bit_cost.h"
#include "enc/block_splitter.h"
#include "enc/cluster.h"
#include "enc/command.h"
#include "enc/entropy_encode.h"
#include "enc/histogram.h"
#include "enc/memory.h"
#include "enc/metablock.h"
#include "enc/params.h"

#define WUFFS_IMPLEMENTATION
#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__BROTLI
#define WUFFS_CONFIG__MODULE__SOFTFLOAT
#include "../release/c/wuffs-unsupported-snapshot.c"

// ---------------- Utilities

static uint64_t g_rng_state = 0x123456789ABCDEF1ull;

static uint64_t  //
rng64(void) {
  // xorshift64*.
  g_rng_state ^= g_rng_state >> 12;
  g_rng_state ^= g_rng_state << 25;
  g_rng_state ^= g_rng_state >> 27;
  return g_rng_state * 0x2545F4914F6CDD1Dull;
}

static uint32_t  //
rng_below(uint32_t n) {
  return n ? (uint32_t)((rng64() >> 32) % n) : 0;
}

static uint64_t  //
f64_bits(double d) {
  uint64_t u;
  memcpy(&u, &d, 8);
  return u;
}

static uint64_t g_num_checks = 0;
static uint64_t g_num_mismatches = 0;
static uint64_t g_counts[64];
static const char* g_count_names[64];

static void  //
count(int i, const char* name) {
  g_counts[i]++;
  g_count_names[i] = name;
}

#define CHECK_EQ(what, c_value, w_value)                                     \
  do {                                                                       \
    uint64_t c_v_ = (uint64_t)(c_value);                                     \
    uint64_t w_v_ = (uint64_t)(w_value);                                     \
    g_num_checks++;                                                          \
    if (c_v_ != w_v_) {                                                      \
      g_num_mismatches++;                                                    \
      if (g_num_mismatches < 50) {                                           \
        printf("MISMATCH %s (%s): C 0x%016" PRIX64 " vs Wuffs 0x%016" PRIX64 \
               "\n",                                                         \
               what, g_config_name, c_v_, w_v_);                             \
      }                                                                      \
    }                                                                        \
  } while (0)

static char g_config_name[256];

static wuffs_brotli__metablock_tester* g_tester = NULL;

// ---------------- BitsEntropy and PopulationCost

// fill_counts fills n u32 counts with a random distribution of the given
// style.
static void  //
fill_counts(uint32_t* counts, size_t n, int style) {
  size_t i;
  memset(counts, 0, n * sizeof(uint32_t));
  switch (style) {
    case 0:  // Uniform small.
      for (i = 0; i < n; i++) {
        counts[i] = rng_below(10);
      }
      break;
    case 1:  // Sparse.
      for (i = 0; i < n; i++) {
        counts[i] = (rng_below(8) == 0) ? rng_below(1000) : 0;
      }
      break;
    case 2: {  // 1 to 5 non-zero symbols.
      size_t k = 1 + rng_below(5);
      for (i = 0; i < k; i++) {
        counts[rng_below((uint32_t)n)] += 1 + rng_below(100000);
      }
      break;
    }
    case 3:  // Large counts.
      for (i = 0; i < n; i++) {
        counts[i] = (rng_below(3) == 0) ? (uint32_t)(rng64() >> 40) : 0;
      }
      break;
    case 4:  // Zipf-ish.
      for (i = 0; i < n; i++) {
        counts[i] = (uint32_t)(100000 / (1 + i + rng_below(3)));
      }
      break;
    case 5:  // Runs of zeros of various lengths.
      for (i = 0; i < n;) {
        size_t run = 1 + rng_below(40);
        uint32_t v = rng_below(2) ? 0 : 1 + rng_below(50);
        for (; run && (i < n); run--, i++) {
          counts[i] = v;
        }
      }
      break;
    case 6:  // All zeros.
      break;
    default: {  // Huge counts (u32 overflow in the 2 * (...) expressions).
      size_t k = 2 + rng_below(4);
      for (i = 0; i < k; i++) {
        counts[rng_below((uint32_t)n)] = 0x7FFFFFFFu + rng_below(0x7FFFFFFF);
      }
      break;
    }
  }
}

static void  //
test_bit_cost(int iters) {
  static uint32_t counts[1024];
  static uint8_t hbuf[2832];
  int it;
  for (it = 0; it < iters; it++) {
    int style = (int)rng_below(8);
    size_t size = 1 + rng_below(704);
    snprintf(g_config_name, sizeof(g_config_name),
             "bit_cost iter=%d style=%d size=%zu", it, style, size);
    fill_counts(counts, size, style);
    {
      double c = BrotliBitsEntropy(counts, size);
      uint64_t w = wuffs_brotli__metablock_tester__bits_entropy(
          g_tester, wuffs_base__make_slice_u8((uint8_t*)counts, size * 4),
          size);
      CHECK_EQ("BitsEntropy", f64_bits(c), w);
      count(0, "BitsEntropy");
    }
    if (style == 7) {
      // Huge counts make the C code's total_count_ inconsistent (and its
      // (size_t)(log2p + 0.5) undefined), so skip PopulationCost.
      continue;
    }
    {
      HistogramLiteral h;
      size_t i;
      fill_counts(h.data_, 256, style);
      h.total_count_ = 0;
      for (i = 0; i < 256; i++) {
        h.total_count_ += h.data_[i];
      }
      h.bit_cost_ = 0;
      memcpy(hbuf, &h, sizeof(h));
      CHECK_EQ("PopulationCostLiteral", f64_bits(BrotliPopulationCostLiteral(&h)),
               wuffs_brotli__metablock_tester__population_cost(
                   g_tester, wuffs_base__make_slice_u8(hbuf, sizeof(h)), 256));
      count(1, "PopulationCostLiteral");
    }
    {
      HistogramCommand h;
      size_t i;
      fill_counts(h.data_, 704, style);
      h.total_count_ = 0;
      for (i = 0; i < 704; i++) {
        h.total_count_ += h.data_[i];
      }
      h.bit_cost_ = 0;
      memcpy(hbuf, &h, sizeof(h));
      CHECK_EQ("PopulationCostCommand", f64_bits(BrotliPopulationCostCommand(&h)),
               wuffs_brotli__metablock_tester__population_cost(
                   g_tester, wuffs_base__make_slice_u8(hbuf, sizeof(h)), 704));
      count(2, "PopulationCostCommand");
    }
    {
      HistogramDistance h;
      size_t i;
      fill_counts(h.data_, 544, style);
      h.total_count_ = 0;
      for (i = 0; i < 544; i++) {
        h.total_count_ += h.data_[i];
      }
      h.bit_cost_ = 0;
      memcpy(hbuf, &h, sizeof(h));
      CHECK_EQ("PopulationCostDistance",
               f64_bits(BrotliPopulationCostDistance(&h)),
               wuffs_brotli__metablock_tester__population_cost(
                   g_tester, wuffs_base__make_slice_u8(hbuf, sizeof(h)), 544));
      count(3, "PopulationCostDistance");
    }
  }
}

// ---------------- InitDistanceParams

static void  //
test_init_distance_params(void) {
  uint32_t npostfix, ndirect_msb, lw;
  for (lw = 0; lw < 2; lw++) {
    for (npostfix = 0; npostfix <= 3; npostfix++) {
      for (ndirect_msb = 0; ndirect_msb < 16; ndirect_msb++) {
        uint32_t ndirect = ndirect_msb << npostfix;
        BrotliDistanceParams c;
        snprintf(g_config_name, sizeof(g_config_name),
                 "InitDistanceParams npostfix=%u ndirect=%u lw=%u", npostfix,
                 ndirect, lw);
        BrotliInitDistanceParams(&c, npostfix, ndirect, lw ? BROTLI_TRUE : 0);
        wuffs_brotli__metablock_tester__init_distance_params(g_tester, npostfix,
                                                             ndirect, lw);
        CHECK_EQ("distance_postfix_bits", c.distance_postfix_bits,
                 wuffs_brotli__metablock_tester__get(g_tester, 24));
        CHECK_EQ("num_direct_distance_codes", c.num_direct_distance_codes,
                 wuffs_brotli__metablock_tester__get(g_tester, 25));
        CHECK_EQ("alphabet_size_max", c.alphabet_size_max,
                 wuffs_brotli__metablock_tester__get(g_tester, 26));
        CHECK_EQ("alphabet_size_limit", c.alphabet_size_limit,
                 wuffs_brotli__metablock_tester__get(g_tester, 27));
        CHECK_EQ("max_distance", c.max_distance,
                 wuffs_brotli__metablock_tester__get(g_tester, 28));
        count(4, "InitDistanceParams");
      }
    }
  }
}

// ---------------- Test inputs

typedef struct {
  const char* name;
  uint8_t* data;
  size_t len;
} file_t;

#define MAX_FILES 64
static file_t g_files[MAX_FILES];
static size_t g_num_files = 0;

static void  //
load_file(const char* path) {
  FILE* f;
  long n;
  if (g_num_files >= MAX_FILES) {
    return;
  }
  f = fopen(path, "rb");
  if (!f) {
    return;
  }
  fseek(f, 0, SEEK_END);
  n = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (n > 0) {
    uint8_t* d = malloc((size_t)n);
    if (d && (fread(d, 1, (size_t)n, f) == (size_t)n)) {
      g_files[g_num_files].name = path;
      g_files[g_num_files].data = d;
      g_files[g_num_files].len = (size_t)n;
      g_num_files++;
    } else {
      free(d);
    }
  }
  fclose(f);
}

// Data styles.
enum {
  STYLE_RANDOM = 0,
  STYLE_IDENTICAL,
  STYLE_FILE,
  STYLE_UTF8,
  STYLE_FEW_SYMBOLS,
  STYLE_SHIFTING,  // The byte distribution changes every few KiB.
  STYLE_BINARY,    // Structured binary (little-endian integers).
  NUM_STYLES
};

static const char* g_style_names[NUM_STYLES] = {
    "random", "identical", "file", "utf8", "few", "shifting", "binary",
};

static const char* g_file_name = "";
static uint32_t g_shift_max = 8192;  // STYLE_SHIFTING's maximum segment size.
static int g_dist_style = 0;
static uint32_t g_dist_shift = 0;
static uint32_t g_dist_residue = 0;

static void  //
fill_data(uint8_t* d, size_t n, int style) {
  size_t i;
  g_file_name = "";
  switch (style) {
    case STYLE_RANDOM:
      for (i = 0; i < n; i++) {
        d[i] = (uint8_t)rng64();
      }
      break;
    case STYLE_IDENTICAL: {
      uint8_t b = (uint8_t)rng64();
      memset(d, b, n);
      break;
    }
    case STYLE_FILE: {
      file_t* f = &g_files[rng_below((uint32_t)g_num_files)];
      size_t off = rng_below((uint32_t)f->len);
      g_file_name = f->name;
      for (i = 0; i < n; i++) {
        d[i] = f->data[(off + i) % f->len];
      }
      break;
    }
    case STYLE_UTF8:
      for (i = 0; i < n;) {
        uint32_t r = rng_below(10);
        if ((r < 5) || (i + 4 > n)) {
          d[i++] = (uint8_t)(0x20 + rng_below(0x5F));
        } else if (r < 7) {
          d[i++] = (uint8_t)(0xC2 + rng_below(0x1E));
          d[i++] = (uint8_t)(0x80 + rng_below(0x40));
        } else if (r < 9) {
          d[i++] = (uint8_t)(0xE0 + rng_below(0x10));
          d[i++] = (uint8_t)(0x80 + rng_below(0x40));
          d[i++] = (uint8_t)(0x80 + rng_below(0x40));
        } else {
          d[i++] = (uint8_t)(0xF0 + rng_below(0x5));
          d[i++] = (uint8_t)(0x80 + rng_below(0x40));
          d[i++] = (uint8_t)(0x80 + rng_below(0x40));
          d[i++] = (uint8_t)(0x80 + rng_below(0x40));
        }
      }
      break;
    case STYLE_FEW_SYMBOLS: {
      uint32_t k = 2 + rng_below(6);
      uint8_t base = (uint8_t)rng64();
      for (i = 0; i < n; i++) {
        d[i] = (uint8_t)(base + rng_below(k) * 3);
      }
      break;
    }
    case STYLE_SHIFTING: {
      uint32_t lo = 0, span = 1;
      size_t next = 0;
      for (i = 0; i < n; i++) {
        if (i == next) {
          lo = rng_below(256);
          span = 1 + rng_below(64);
          next = i + 64 + rng_below(g_shift_max);
        }
        d[i] = (uint8_t)(lo + rng_below(span));
      }
      break;
    }
    default: {
      uint32_t v = (uint32_t)rng64();
      for (i = 0; i + 4 <= n; i += 4) {
        v += rng_below(300);
        memcpy(d + i, &v, 4);
      }
      for (; i < n; i++) {
        d[i] = (uint8_t)rng64();
      }
      break;
    }
  }
}

// ---------------- Commands

typedef struct {
  uint8_t* rb;  // The ring buffer, of rb_size = mask + 1 bytes (plus slack).
  size_t mask;
  size_t pos;
  size_t total;  // The metablock length.
  Command* cmds;
  size_t num_cmds;
  size_t cmds_cap;
  uint8_t prev_byte;
  uint8_t prev_byte2;
  BrotliDistanceParams dist;
  bool large_window;
} block_t;

static void  //
push_cmd(block_t* b, size_t insert_len, size_t copy_len, int delta,
         size_t distance_code) {
  if (b->num_cmds == b->cmds_cap) {
    b->cmds_cap = b->cmds_cap ? 2 * b->cmds_cap : 1024;
    b->cmds = realloc(b->cmds, b->cmds_cap * sizeof(Command));
  }
  if (copy_len == 0) {
    InitInsertCommand(&b->cmds[b->num_cmds++], insert_len);
  } else {
    InitCommand(&b->cmds[b->num_cmds++], &b->dist, insert_len, copy_len, delta,
                distance_code);
  }
}

static size_t  //
random_distance(block_t* b) {
  uint32_t r = rng_below(16);
  size_t max = b->large_window ? 0x7FFFFFFC : ((1u << 24) - 16);
  if ((g_dist_style == 1) && (r < 12)) {
    // Distances with a fixed residue modulo (1 << g_dist_shift), which
    // favors npostfix > 0.
    return ((size_t)(1 + rng_below(r < 8 ? 64 : 100000)) << g_dist_shift) +
           g_dist_residue;
  } else if ((g_dist_style == 2) && (r < 12)) {
    // Small distances, which favor ndirect > 0.
    return 1 + rng_below(4 + rng_below(130));
  }
  if (r < 6) {
    return 1 + rng_below(64);
  } else if (r < 10) {
    return 1 + rng_below(4096);
  } else if (r < 13) {
    return 1 + rng_below(1u << 18);
  } else if (r < 15) {
    return 1 + rng_below((uint32_t)max);
  }
  return max - rng_below(1000);
}

static size_t  //
random_insert_len(size_t remaining, int mode) {
  uint32_t r = rng_below(16);
  size_t n;
  if (mode == 1) {  // Long inserts.
    n = rng_below(r < 8 ? 3000 : 70000);
  } else if (r < 8) {
    n = rng_below(6);
  } else if (r < 13) {
    n = rng_below(130);
  } else if (r < 15) {
    n = rng_below(2200);
  } else {
    n = rng_below(30000);
  }
  return n < remaining ? n : remaining;
}

static size_t  //
random_copy_len(int mode) {
  uint32_t r = rng_below(16);
  (void)mode;
  if (r < 8) {
    return 2 + rng_below(8);
  } else if (r < 13) {
    return 2 + rng_below(134);
  } else if (r < 15) {
    return 2 + rng_below(2200);
  }
  return 2 + rng_below(100000);
}

// make_synthetic_commands fills b->cmds with random (but valid) commands
// whose insert and copy lengths add up to b->total.
static void  //
make_synthetic_commands(block_t* b, int mode) {
  size_t remaining = b->total;
  b->num_cmds = 0;
  while (remaining > 0) {
    size_t ins = random_insert_len(remaining, mode);
    size_t copy;
    int delta = 0;
    size_t dcode;
    remaining -= ins;
    if (remaining < 2) {
      // A final insert-only command.
      push_cmd(b, ins + remaining, 0, 0, 0);
      remaining = 0;
      break;
    }
    copy = random_copy_len(mode);
    if (copy > remaining) {
      copy = remaining;
    }
    if ((rng_below(16) == 0) && (copy >= 4) && (copy <= 24)) {
      // A dictionary-like copy whose length code differs from its length.
      delta = (int)rng_below(9) - 4;
      if ((int)copy + delta < 2) {
        delta = 0;
      }
    }
    if (rng_below(4) == 0) {
      dcode = rng_below(16);  // A short (distance cache) code.
    } else {
      dcode = random_distance(b) + 15;
    }
    push_cmd(b, ins, copy, delta, dcode);
    remaining -= copy;
  }
}

// make_lz77_commands fills b->cmds with the commands of a simple greedy LZ77
// parse of the metablock's bytes (with a 1-entry distance cache).
static void  //
make_lz77_commands(block_t* b) {
  static uint32_t table[1 << 16];
  size_t i = 0, ins = 0, last_dist = 0;
  memset(table, 0xFF, sizeof(table));
  b->num_cmds = 0;
  while (i < b->total) {
    size_t cand_len = 0, cand_dist = 0;
    if (i + 4 <= b->total) {
      uint32_t v = 0;
      uint32_t h;
      size_t k;
      for (k = 0; k < 4; k++) {
        v |= (uint32_t)b->rb[(b->pos + i + k) & b->mask] << (8 * k);
      }
      h = (v * 0x1E35A7BDu) >> 16;
      if (table[h] != 0xFFFFFFFFu) {
        size_t j = table[h];
        size_t len = 0;
        while ((i + len < b->total) && (len < 1000) &&
               (b->rb[(b->pos + j + len) & b->mask] ==
                b->rb[(b->pos + i + len) & b->mask])) {
          len++;
        }
        if (len >= 4) {
          cand_len = len;
          cand_dist = i - j;
        }
      }
      table[h] = (uint32_t)i;
    }
    if (cand_len) {
      size_t dcode = (cand_dist == last_dist) ? 0 : cand_dist + 15;
      push_cmd(b, ins, cand_len, 0, dcode);
      last_dist = cand_dist;
      ins = 0;
      i += cand_len;
    } else {
      ins++;
      i++;
    }
  }
  if (ins > 0) {
    push_cmd(b, ins, 0, 0, 0);
  }
}

// make_short_commands fills b->cmds with many short commands.
static void  //
make_short_commands(block_t* b) {
  size_t remaining = b->total;
  b->num_cmds = 0;
  while (remaining > 0) {
    size_t ins = rng_below(4);
    size_t copy = 2 + rng_below(6);
    if (ins > remaining) {
      ins = remaining;
    }
    remaining -= ins;
    if (remaining < copy) {
      push_cmd(b, ins + remaining, 0, 0, 0);
      break;
    }
    push_cmd(b, ins, copy,  0,
             rng_below(3) ? rng_below(16) : random_distance(b) + 15);
    remaining -= copy;
  }
}

// make_block sets up the ring buffer (with the metablock's total bytes at
// position pos, possibly wrapping around) and its commands.
static void  //
make_block(block_t* b, size_t total, int style, int cmd_mode, bool wrap) {
  size_t rb_size = 1024;
  uint8_t* tmp;
  size_t i;
  while (rb_size < total + (wrap ? 0 : 1)) {
    rb_size *= 2;
  }
  if (!wrap && (rb_size < 2 * total)) {
    rb_size *= 2;
  }
  free(b->rb);
  b->rb = malloc(rb_size + 64);
  memset(b->rb, 0, rb_size + 64);
  b->mask = rb_size - 1;
  b->pos = wrap ? (rb_size - rng_below((uint32_t)total + 1)) & b->mask
                : rng_below((uint32_t)(rb_size - total));
  b->total = total;
  tmp = malloc(total + 1);
  fill_data(tmp, total, style);
  for (i = 0; i < total; i++) {
    b->rb[(b->pos + i) & b->mask] = tmp[i];
  }
  free(tmp);
  b->prev_byte = (uint8_t)rng64();
  b->prev_byte2 = (uint8_t)rng64();
  if (cmd_mode == 2) {
    make_lz77_commands(b);
  } else if (cmd_mode == 3) {
    make_short_commands(b);
  } else {
    make_synthetic_commands(b, cmd_mode);
  }
}

// Coverage statistics.
static uint64_t g_dist_params_seen[2][4][16];
static uint64_t g_chosen_dist_params_seen[4][16];
static uint64_t g_context_modes_seen[4];
static uint64_t g_max_num_types[3];
static uint64_t g_max_num_blocks[3];
static uint64_t g_max_literals;
static uint64_t g_max_commands;

// ---------------- Workbuf

static uint8_t* g_workbuf = NULL;
static size_t g_workbuf_len = 0;
static uint64_t g_max_workbuf_used = 0;

static void  //
ensure_workbuf(size_t n) {
  if (g_workbuf_len < n) {
    free(g_workbuf);
    g_workbuf = malloc(n);
    g_workbuf_len = n;
    if (!g_workbuf) {
      printf("out of memory\n");
      exit(1);
    }
  }
}

static uint32_t  //
ld32(const uint8_t* p) {
  uint32_t v;
  memcpy(&v, p, 4);
  return v;
}

// ---------------- SplitBlock

static void  //
compare_block_split(const char* what, const BlockSplit* c, int cat) {
  size_t n;
  size_t i;
  uint64_t num_types = wuffs_brotli__metablock_tester__get(g_tester, 4 * cat);
  uint64_t num_blocks =
      wuffs_brotli__metablock_tester__get(g_tester, 4 * cat + 1);
  uint64_t types_off =
      wuffs_brotli__metablock_tester__get(g_tester, 4 * cat + 2);
  uint64_t lengths_off =
      wuffs_brotli__metablock_tester__get(g_tester, 4 * cat + 3);
  char buf[128];
  snprintf(buf, sizeof(buf), "%s.num_types", what);
  CHECK_EQ(buf, c->num_types, num_types);
  snprintf(buf, sizeof(buf), "%s.num_blocks", what);
  CHECK_EQ(buf, c->num_blocks, num_blocks);
  n = c->num_blocks < num_blocks ? c->num_blocks : (size_t)num_blocks;
  for (i = 0; i < n; i++) {
    snprintf(buf, sizeof(buf), "%s.types[%zu]", what, i);
    CHECK_EQ(buf, c->types[i], g_workbuf[types_off + i]);
    snprintf(buf, sizeof(buf), "%s.lengths[%zu]", what, i);
    CHECK_EQ(buf, c->lengths[i], ld32(g_workbuf + lengths_off + 4 * i));
    if (g_num_mismatches > 1000) {
      break;
    }
  }
}

static size_t  //
workbuf_guess(const block_t* b) {
  return ((size_t)64 << 20) + 64 * b->total + 64 * b->num_cmds;
}

static void  //
test_split_block(block_t* b, int quality) {
  MemoryManager m;
  BrotliEncoderParams params;
  BlockSplit ls, cs, ds;
  wuffs_base__status st;
  size_t wlen = workbuf_guess(b);

  memset(&params, 0, sizeof(params));
  params.quality = quality;
  BrotliInitMemoryManager(&m, 0, 0, 0);
  BrotliInitBlockSplit(&ls);
  BrotliInitBlockSplit(&cs);
  BrotliInitBlockSplit(&ds);
  BrotliSplitBlock(&m, b->cmds, b->num_cmds, b->rb, b->pos, b->mask, &params,
                   &ls, &cs, &ds);

  while (true) {
    ensure_workbuf(wlen);
    st = wuffs_brotli__metablock_tester__split_block(
        g_tester,
        wuffs_base__make_slice_u8((uint8_t*)b->cmds,
                                  b->num_cmds * sizeof(Command)),
        b->num_cmds, wuffs_base__make_slice_u8(b->rb, b->mask + 1), b->pos,
        b->mask, (uint32_t)quality,
        wuffs_base__make_slice_u8(g_workbuf, g_workbuf_len));
    if (st.repr != wuffs_base__error__bad_workbuf_length) {
      break;
    }
    wlen *= 2;
  }
  if (st.repr) {
    printf("split_block: %s (%s)\n", st.repr, g_config_name);
    g_num_mismatches++;
  } else {
    uint64_t used = wuffs_brotli__metablock_tester__get(g_tester, 29);
    if (g_max_workbuf_used < used) {
      g_max_workbuf_used = used;
    }
    compare_block_split("literal_split", &ls, 0);
    compare_block_split("command_split", &cs, 1);
    compare_block_split("distance_split", &ds, 2);
  }
  BrotliDestroyBlockSplit(&m, &ls);
  BrotliDestroyBlockSplit(&m, &cs);
  BrotliDestroyBlockSplit(&m, &ds);
}

// ---------------- BuildMetaBlock and OptimizeHistograms

static void  //
compare_histograms(const char* what, const void* c_histos, size_t hsize,
                   uint64_t c_n, int cat) {
  uint64_t off = wuffs_brotli__metablock_tester__get(g_tester, 18 + cat);
  uint64_t n = wuffs_brotli__metablock_tester__get(g_tester, 21 + cat);
  size_t i;
  char buf[128];
  snprintf(buf, sizeof(buf), "%s.size", what);
  CHECK_EQ(buf, c_n, n);
  if (c_n != n) {
    return;
  }
  for (i = 0; i < n; i++) {
    const uint8_t* cp = (const uint8_t*)c_histos + i * hsize;
    const uint8_t* wp = g_workbuf + off + i * hsize;
    size_t j;
    g_num_checks++;
    if (memcmp(cp, wp, hsize)) {
      for (j = 0; j < hsize; j += 4) {
        if (memcmp(cp + j, wp + j, 4)) {
          break;
        }
      }
      snprintf(buf, sizeof(buf), "%s[%zu] at byte %zu", what, i, j);
      CHECK_EQ(buf, ld32(cp + j), ld32(wp + j));
      g_num_checks--;
      return;
    }
  }
}

static void  //
compare_context_map(const char* what, const uint32_t* c_map, size_t c_n,
                    int cat) {
  uint64_t off = wuffs_brotli__metablock_tester__get(g_tester, 12 + cat);
  uint64_t n = wuffs_brotli__metablock_tester__get(g_tester, 15 + cat);
  size_t i;
  char buf[128];
  snprintf(buf, sizeof(buf), "%s.size", what);
  CHECK_EQ(buf, c_n, n);
  if (c_n != n) {
    return;
  }
  g_num_checks++;
  for (i = 0; i < n; i++) {
    if (c_map[i] != ld32(g_workbuf + off + 4 * i)) {
      snprintf(buf, sizeof(buf), "%s[%zu]", what, i);
      CHECK_EQ(buf, c_map[i], ld32(g_workbuf + off + 4 * i));
      return;
    }
  }
}

static void  //
compare_meta_block_split(const MetaBlockSplit* mb, const char* suffix) {
  char buf[64];
  snprintf(buf, sizeof(buf), "literal_histograms%s", suffix);
  compare_histograms(buf, mb->literal_histograms, sizeof(HistogramLiteral),
                     mb->literal_histograms_size, 0);
  snprintf(buf, sizeof(buf), "command_histograms%s", suffix);
  compare_histograms(buf, mb->command_histograms, sizeof(HistogramCommand),
                     mb->command_histograms_size, 1);
  snprintf(buf, sizeof(buf), "distance_histograms%s", suffix);
  compare_histograms(buf, mb->distance_histograms, sizeof(HistogramDistance),
                     mb->distance_histograms_size, 2);
}

static void  //
test_build_meta_block(block_t* b, int quality, bool disable_lcm,
                      int context_mode) {
  MemoryManager m;
  BrotliEncoderParams params;
  MetaBlockSplit mb;
  wuffs_base__status st;
  size_t wlen = workbuf_guess(b);
  size_t cmds_size = b->num_cmds * sizeof(Command);
  Command* c_cmds = malloc(cmds_size + 1);
  Command* w_cmds = malloc(cmds_size + 1);
  size_t i;

  memcpy(c_cmds, b->cmds, cmds_size);
  memset(&params, 0, sizeof(params));
  params.quality = quality;
  params.large_window = b->large_window ? BROTLI_TRUE : BROTLI_FALSE;
  params.disable_literal_context_modeling =
      disable_lcm ? BROTLI_TRUE : BROTLI_FALSE;
  params.dist = b->dist;
  BrotliInitMemoryManager(&m, 0, 0, 0);
  InitMetaBlockSplit(&mb);
  BrotliBuildMetaBlock(&m, b->rb, b->pos, b->mask, &params, b->prev_byte,
                       b->prev_byte2, c_cmds, b->num_cmds,
                       (ContextType)context_mode, &mb);

  while (true) {
    memcpy(w_cmds, b->cmds, cmds_size);
    ensure_workbuf(wlen);
    st = wuffs_brotli__metablock_tester__build_meta_block(
        g_tester, wuffs_base__make_slice_u8(g_workbuf, g_workbuf_len),
        wuffs_base__make_slice_u8(b->rb, b->mask + 1), b->pos, b->mask,
        b->prev_byte, b->prev_byte2,
        wuffs_base__make_slice_u8((uint8_t*)w_cmds, cmds_size), b->num_cmds,
        (uint32_t)context_mode, (uint32_t)quality, b->large_window ? 1 : 0,
        disable_lcm ? 1 : 0, b->dist.distance_postfix_bits,
        b->dist.num_direct_distance_codes);
    if (st.repr != wuffs_base__error__bad_workbuf_length) {
      break;
    }
    wlen *= 2;
  }
  if (st.repr) {
    printf("build_meta_block: %s (%s)\n", st.repr, g_config_name);
    g_num_mismatches++;
    goto done;
  }
  {
    uint64_t used = wuffs_brotli__metablock_tester__get(g_tester, 29);
    if (g_max_workbuf_used < used) {
      g_max_workbuf_used = used;
    }
  }

  CHECK_EQ("dist.distance_postfix_bits", params.dist.distance_postfix_bits,
           wuffs_brotli__metablock_tester__get(g_tester, 24));
  CHECK_EQ("dist.num_direct_distance_codes",
           params.dist.num_direct_distance_codes,
           wuffs_brotli__metablock_tester__get(g_tester, 25));
  CHECK_EQ("dist.alphabet_size_max", params.dist.alphabet_size_max,
           wuffs_brotli__metablock_tester__get(g_tester, 26));
  CHECK_EQ("dist.alphabet_size_limit", params.dist.alphabet_size_limit,
           wuffs_brotli__metablock_tester__get(g_tester, 27));
  CHECK_EQ("dist.max_distance", params.dist.max_distance,
           wuffs_brotli__metablock_tester__get(g_tester, 28));
  g_num_checks++;
  if (memcmp(c_cmds, w_cmds, cmds_size)) {
    for (i = 0; i < b->num_cmds; i++) {
      if (memcmp(&c_cmds[i], &w_cmds[i], sizeof(Command))) {
        char buf[64];
        snprintf(buf, sizeof(buf), "cmds[%zu].dist_prefix_", i);
        CHECK_EQ(buf, c_cmds[i].dist_prefix_, w_cmds[i].dist_prefix_);
        snprintf(buf, sizeof(buf), "cmds[%zu].dist_extra_", i);
        CHECK_EQ(buf, c_cmds[i].dist_extra_, w_cmds[i].dist_extra_);
        break;
      }
    }
  }
  {
    g_chosen_dist_params_seen[params.dist.distance_postfix_bits & 3]
                             [(params.dist.num_direct_distance_codes >>
                               params.dist.distance_postfix_bits) & 15]++;
    g_context_modes_seen[context_mode & 3]++;
    if (g_max_num_types[0] < mb.literal_split.num_types) {
      g_max_num_types[0] = mb.literal_split.num_types;
    }
    if (g_max_num_types[1] < mb.command_split.num_types) {
      g_max_num_types[1] = mb.command_split.num_types;
    }
    if (g_max_num_types[2] < mb.distance_split.num_types) {
      g_max_num_types[2] = mb.distance_split.num_types;
    }
    if (g_max_num_blocks[0] < mb.literal_split.num_blocks) {
      g_max_num_blocks[0] = mb.literal_split.num_blocks;
    }
    if (g_max_num_blocks[1] < mb.command_split.num_blocks) {
      g_max_num_blocks[1] = mb.command_split.num_blocks;
    }
    if (g_max_num_blocks[2] < mb.distance_split.num_blocks) {
      g_max_num_blocks[2] = mb.distance_split.num_blocks;
    }
    if (g_max_commands < b->num_cmds) {
      g_max_commands = b->num_cmds;
    }
    {
      uint64_t nl = 0;
      for (i = 0; i < b->num_cmds; i++) {
        nl += b->cmds[i].insert_len_;
      }
      if (g_max_literals < nl) {
        g_max_literals = nl;
      }
    }
  }
  compare_block_split("mb.literal_split", &mb.literal_split, 0);
  compare_block_split("mb.command_split", &mb.command_split, 1);
  compare_block_split("mb.distance_split", &mb.distance_split, 2);
  compare_context_map("literal_context_map", mb.literal_context_map,
                      mb.literal_context_map_size, 0);
  compare_context_map("distance_context_map", mb.distance_context_map,
                      mb.distance_context_map_size, 2);
  compare_meta_block_split(&mb, "");

  BrotliOptimizeHistograms(params.dist.alphabet_size_limit, &mb);
  wuffs_brotli__metablock_tester__optimize_histograms(
      g_tester, wuffs_base__make_slice_u8(g_workbuf, g_workbuf_len),
      params.dist.alphabet_size_limit);
  compare_meta_block_split(&mb, " (optimized)");

done:
  DestroyMetaBlockSplit(&m, &mb);
  free(c_cmds);
  free(w_cmds);
}

// ---------------- ClusterHistograms

static void  //
test_cluster_histograms(int iters) {
  int it;
  for (it = 0; it < iters; it++) {
    int kind = (int)rng_below(3);
    size_t data_size = (kind == 0) ? 256 : (kind == 1) ? 704 : 544;
    size_t hsize = (kind == 0)   ? sizeof(HistogramLiteral)
                   : (kind == 1) ? sizeof(HistogramCommand)
                                 : sizeof(HistogramDistance);
    uint32_t r = rng_below(8);
    size_t in_size = (r < 4)   ? 1 + rng_below(16)
                     : (r < 7) ? 1 + rng_below(200)
                               : 1 + rng_below(1100);
    size_t max_histograms = rng_below(4) ? 256 : 1 + rng_below(300);
    int style = (int)rng_below(7);
    int sharing = (int)rng_below(3);
    uint8_t* in = malloc(in_size * hsize);
    uint8_t* out = malloc(in_size * hsize);
    uint32_t* symbols = malloc(in_size * sizeof(uint32_t));
    size_t c_out_size = 0;
    uint64_t w_out_size;
    MemoryManager m;
    size_t i, j;
    snprintf(g_config_name, sizeof(g_config_name),
             "ClusterHistograms iter=%d kind=%d in_size=%zu max=%zu "
             "style=%d sharing=%d",
             it, kind, in_size, max_histograms, style, sharing);
    for (i = 0; i < in_size; i++) {
      uint32_t* data = (uint32_t*)(in + i * hsize);
      uint64_t total = 0;
      if ((sharing > 0) && (i > 0) && (rng_below(3) != 0)) {
        // Copy (and perturb) an earlier histogram, so that clustering has
        // something to merge.
        memcpy(data, in + rng_below((uint32_t)i) * hsize, data_size * 4);
        if (sharing == 2) {
          data[rng_below((uint32_t)data_size)] += rng_below(5);
        }
      } else if (rng_below(10) == 0) {
        memset(data, 0, data_size * 4);  // An empty histogram.
      } else {
        fill_counts(data, data_size, style);
      }
      for (j = 0; j < data_size; j++) {
        total += data[j];
      }
      memcpy(in + i * hsize + data_size * 4, &total, 8);
      memset(in + i * hsize + data_size * 4 + 8, 0, 8);
    }
    BrotliInitMemoryManager(&m, 0, 0, 0);
    if (kind == 0) {
      BrotliClusterHistogramsLiteral(&m, (HistogramLiteral*)in, in_size,
                                     max_histograms, (HistogramLiteral*)out,
                                     &c_out_size, symbols);
    } else if (kind == 1) {
      BrotliClusterHistogramsCommand(&m, (HistogramCommand*)in, in_size,
                                     max_histograms, (HistogramCommand*)out,
                                     &c_out_size, symbols);
    } else {
      BrotliClusterHistogramsDistance(&m, (HistogramDistance*)in, in_size,
                                      max_histograms, (HistogramDistance*)out,
                                      &c_out_size, symbols);
    }
    ensure_workbuf(4 * in_size * hsize + ((size_t)16 << 20));
    w_out_size = wuffs_brotli__metablock_tester__cluster_histograms(
        g_tester, (uint32_t)data_size,
        wuffs_base__make_slice_u8(in, in_size * hsize), in_size,
        max_histograms, wuffs_base__make_slice_u8(g_workbuf, g_workbuf_len));
    CHECK_EQ("out_size", c_out_size, w_out_size);
    if (c_out_size == w_out_size) {
      uint64_t out_off = wuffs_brotli__metablock_tester__get(g_tester, 18);
      uint64_t sym_off = wuffs_brotli__metablock_tester__get(g_tester, 12);
      g_num_checks++;
      for (i = 0; i < c_out_size; i++) {
        if (memcmp(out + i * hsize, g_workbuf + out_off + i * hsize, hsize)) {
          char buf[64];
          snprintf(buf, sizeof(buf), "out[%zu]", i);
          CHECK_EQ(buf, 0, 1);
          break;
        }
      }
      for (i = 0; i < in_size; i++) {
        if (symbols[i] != ld32(g_workbuf + sym_off + 4 * i)) {
          char buf[64];
          snprintf(buf, sizeof(buf), "histogram_symbols[%zu]", i);
          CHECK_EQ(buf, symbols[i], ld32(g_workbuf + sym_off + 4 * i));
          break;
        }
      }
    }
    count(5 + kind, kind == 0   ? "ClusterHistogramsLiteral"
                    : kind == 1 ? "ClusterHistogramsCommand"
                                : "ClusterHistogramsDistance");
    free(in);
    free(out);
    free(symbols);
  }
}

// ---------------- Configurations

static block_t g_block;


static void  //
random_dist_params(block_t* b) {
  uint32_t npostfix = rng_below(4);
  uint32_t ndirect = rng_below(16) << npostfix;
  if (rng_below(3) == 0) {
    npostfix = 0;
    ndirect = 0;
  }
  b->large_window = rng_below(4) == 0;
  BrotliInitDistanceParams(&b->dist, npostfix, ndirect,
                           b->large_window ? BROTLI_TRUE : BROTLI_FALSE);
}

static void  //
run_config(size_t total, int style, int cmd_mode, bool do_split,
           bool do_build) {
  bool wrap = rng_below(4) == 0;
  int quality = (rng_below(2) == 0) ? 10 : 11;
  bool disable_lcm = rng_below(4) == 0;
  int context_mode = (int)rng_below(4);
  if (rng_below(8) == 0) {
    quality = 4 + (int)rng_below(6);
  }
  g_shift_max = rng_below(2) ? 8192 : 64 + rng_below(2000);
  g_dist_style = (int)rng_below(3);
  g_dist_shift = 1 + rng_below(3);
  g_dist_residue = rng_below(1u << g_dist_shift);
  random_dist_params(&g_block);
  g_dist_params_seen[g_block.large_window ? 1 : 0]
                    [g_block.dist.distance_postfix_bits]
                    [g_block.dist.num_direct_distance_codes >>
                     g_block.dist.distance_postfix_bits]++;
  make_block(&g_block, total, style, cmd_mode, wrap);
  snprintf(g_config_name, sizeof(g_config_name),
           "total=%zu style=%s%s%s cmds=%d(n=%zu) wrap=%d q=%d lw=%d "
           "npostfix=%u ndirect=%u nolcm=%d mode=%d rng=%016" PRIX64,
           total, g_style_names[style], *g_file_name ? ":" : "", g_file_name,
           cmd_mode, g_block.num_cmds, wrap, quality, g_block.large_window,
           g_block.dist.distance_postfix_bits,
           g_block.dist.num_direct_distance_codes, disable_lcm, context_mode,
           g_rng_state);
  if (do_split) {
    test_split_block(&g_block, quality);
    count(8, "SplitBlock");
  }
  if (do_build) {
    test_build_meta_block(&g_block, quality, disable_lcm, context_mode);
    count(9, "BuildMetaBlock+OptimizeHistograms");
  }
}

static size_t  //
random_total(void) {
  uint32_t r = rng_below(16);
  if (r < 3) {
    return rng_below(130);
  } else if (r < 10) {
    return 130 + rng_below(3870);
  } else if (r < 15) {
    return 4000 + rng_below(36000);
  }
  return 40000 + rng_below(110000);
}

// ---------------- Main

int  //
main(int argc, char** argv) {
  bool quick = false;
  int n_bitcost = 200000, n_clusters = 3000, n_configs = 20000, n_large = 8;
  int i;
  for (i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-quick")) {
      quick = true;
      n_bitcost = 20000;
      n_clusters = 300;
      n_configs = 200;
      n_large = 1;
    } else if (!strncmp(argv[i], "-bitcost=", 9)) {
      n_bitcost = atoi(argv[i] + 9);
    } else if (!strncmp(argv[i], "-clusters=", 10)) {
      n_clusters = atoi(argv[i] + 10);
    } else if (!strncmp(argv[i], "-configs=", 9)) {
      n_configs = atoi(argv[i] + 9);
    } else if (!strncmp(argv[i], "-large=", 7)) {
      n_large = atoi(argv[i] + 7);
    } else if (!strncmp(argv[i], "-seed=", 6)) {
      g_rng_state = strtoull(argv[i] + 6, NULL, 0) | 1;
    }
  }
  (void)quick;
  g_tester = wuffs_brotli__metablock_tester__alloc();
  if (!g_tester) {
    printf("could not allocate the tester\n");
    return 1;
  }

  load_file("test/data/romeo.txt");
  load_file("test/data/midsummer.txt");
  load_file("test/data/pi.txt");
  load_file("test/data/abraca.txt");
  load_file("test/data/enwik5");
  load_file("test/data/archive.tar");
  load_file("test/data/hippopotamus.pgm");
  load_file("test/data/github-tags.json");
  load_file("test/data/json-things.formatted.json");
  load_file("test/data/256.bytes");
  if (g_num_files == 0) {
    printf("no test files (run from the Wuffs root directory)\n");
    return 1;
  }

  test_bit_cost(n_bitcost);
  test_init_distance_params();
  test_cluster_histograms(n_clusters);
  // Edge cases: no commands, one command, one literal, all identical bytes,
  // in every literal context mode.
  for (i = 0; i < 64; i++) {
    static const size_t totals[8] = {0, 1, 2, 5, 127, 128, 129, 3000};
    run_config(totals[i & 7], (i & 8) ? STYLE_IDENTICAL : STYLE_RANDOM,
               (i >> 4) % 3, true, true);
  }
  // Large configurations: at least 1 MiB of literals, and at least 100k
  // commands.
  for (i = 0; i < n_large; i++) {
    static const int styles[4] = {STYLE_FILE, STYLE_RANDOM, STYLE_SHIFTING,
                                  STYLE_UTF8};
    if (i & 1) {
      // Many short commands.
      run_config((size_t)1 << 20, STYLE_FEW_SYMBOLS, 3, false, true);
    } else {
      run_config(((size_t)3 << 19) + rng_below(1 << 18), styles[(i / 2) & 3],
                 1, false, true);
    }
    printf("large configuration %d done, %" PRIu64 " mismatches so far\n",
           i, g_num_mismatches);
    fflush(stdout);
  }
  {
    int n = n_configs;
    for (i = 0; i < n; i++) {
      int style = (int)rng_below(NUM_STYLES);
      int cmd_mode = (int)rng_below(3);
      run_config(random_total(), style, cmd_mode, rng_below(2) == 0, true);
      if ((i % 1000) == 999) {
        printf("%d configurations, %" PRIu64 " mismatches so far\n", i + 1,
               g_num_mismatches);
        fflush(stdout);
      }
    }
  }

  for (i = 0; i < 64; i++) {
    if (g_count_names[i]) {
      printf("%-32s %10" PRIu64 " configurations\n", g_count_names[i],
             g_counts[i]);
    }
  }
  {
    int lw, np, nd, seen = 0, chosen = 0;
    for (lw = 0; lw < 2; lw++) {
      for (np = 0; np < 4; np++) {
        for (nd = 0; nd < 16; nd++) {
          seen += g_dist_params_seen[lw][np][nd] ? 1 : 0;
          if (lw == 0) {
            chosen += g_chosen_dist_params_seen[np][nd] ? 1 : 0;
          }
        }
      }
    }
    printf("input (large_window, npostfix, ndirect) combinations: %d of 128\n",
           seen);
    printf("chosen (npostfix, ndirect) combinations: %d of 64\n", chosen);
    printf("context modes: %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 "\n",
           g_context_modes_seen[0], g_context_modes_seen[1],
           g_context_modes_seen[2], g_context_modes_seen[3]);
    printf("max num_types (lit, cmd, dist): %" PRIu64 " %" PRIu64 " %" PRIu64
           "\n",
           g_max_num_types[0], g_max_num_types[1], g_max_num_types[2]);
    printf("max num_blocks (lit, cmd, dist): %" PRIu64 " %" PRIu64 " %" PRIu64
           "\n",
           g_max_num_blocks[0], g_max_num_blocks[1], g_max_num_blocks[2]);
    printf("max literals: %" PRIu64 ", max commands: %" PRIu64 "\n",
           g_max_literals, g_max_commands);
  }
  printf("max workbuf used: %" PRIu64 " bytes\n", g_max_workbuf_used);
  printf("%" PRIu64 " checks, %" PRIu64 " mismatches\n", g_num_checks,
         g_num_mismatches);
  free(g_tester);
  return g_num_mismatches ? 1 : 0;
}
