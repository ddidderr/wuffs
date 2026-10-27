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

// ---------------- Main

int  //
main(int argc, char** argv) {
  bool quick = (argc > 1) && !strcmp(argv[1], "-quick");
  int i;
  g_tester = wuffs_brotli__metablock_tester__alloc();
  if (!g_tester) {
    printf("could not allocate the tester\n");
    return 1;
  }

  test_bit_cost(quick ? 20000 : 200000);

  for (i = 0; i < 64; i++) {
    if (g_count_names[i]) {
      printf("%-32s %10" PRIu64 " configurations\n", g_count_names[i],
             g_counts[i]);
    }
  }
  printf("%" PRIu64 " checks, %" PRIu64 " mismatches\n", g_num_checks,
         g_num_mismatches);
  free(g_tester);
  return g_num_mismatches ? 1 : 0;
}
