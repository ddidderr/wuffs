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

// manual-test-softfloat-log2.c checks that Wuffs' std/softfloat log2_u64(v)
// function produces exactly the same bits as the C library's log2((double)v)
// for EVERY v in [1, 1 << 32) and for 10 million pseudo-random v in [1 << 32,
// 1 << 53). The softfloat implementation is a port of glibc's (x86-64, FMA)
// log2, so this should only pass when linking against such a libm (e.g.
// glibc 2.28 or later on an x86-64 CPU with FMA and AVX2). Run it with:
//
//   gcc -O2 -std=c99 manual-test-softfloat-log2.c -lm -lpthread -o /tmp/sflog2
//   /tmp/sflog2
//
// An optional argument (e.g. "/tmp/sflog2 0x10000000") limits the exhaustive
// part to v in [1, that). An optional second argument sets the number of
// threads (default 4).
//
// It prints progress, the first 20 mismatches (if any) and exits with a
// non-zero status if there were any mismatches.

#include <inttypes.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WUFFS_IMPLEMENTATION
#define WUFFS_CONFIG__STATIC_FUNCTIONS
#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__SOFTFLOAT
#include "../release/c/wuffs-unsupported-snapshot.c"

#define MAX_THREADS 64
#define CHUNK_SIZE 0x1000000

static wuffs_softfloat__arith g_sf;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint64_t g_next_chunk = 1;  // Guarded by g_mutex.
static uint64_t g_end = 0;
static uint64_t g_num_mismatches = 0;  // Guarded by g_mutex.
static uint64_t g_num_checked = 0;     // Guarded by g_mutex.
static time_t g_start_time;

static uint64_t  //
u_of_d(double d) {
  uint64_t u;
  memcpy(&u, &d, 8);
  return u;
}

static void  //
report_mismatch(uint64_t v, uint64_t have, uint64_t want) {
  pthread_mutex_lock(&g_mutex);
  if (g_num_mismatches++ < 20) {
    printf("MISMATCH v=%" PRIu64 " (0x%" PRIX64 "): have 0x%016" PRIX64
           ", want 0x%016" PRIX64 "\n",
           v, v, have, want);
    fflush(stdout);
  }
  pthread_mutex_unlock(&g_mutex);
}

static void*  //
worker(void* arg) {
  (void)arg;
  while (1) {
    pthread_mutex_lock(&g_mutex);
    uint64_t lo = g_next_chunk;
    g_next_chunk = (lo + CHUNK_SIZE) & ~(uint64_t)(CHUNK_SIZE - 1);
    pthread_mutex_unlock(&g_mutex);
    if (lo >= g_end) {
      return NULL;
    }
    uint64_t hi = (lo + CHUNK_SIZE) & ~(uint64_t)(CHUNK_SIZE - 1);
    if (hi > g_end) {
      hi = g_end;
    }
    for (uint64_t v = lo; v < hi; v++) {
      uint64_t have = wuffs_softfloat__arith__log2_u64(&g_sf, v);
      uint64_t want = u_of_d(log2((double)v));
      if (have != want) {
        report_mismatch(v, have, want);
      }
    }
    pthread_mutex_lock(&g_mutex);
    g_num_checked += hi - lo;
    if ((hi % 0x10000000) == 0) {
      printf("progress: v up to 0x%09" PRIX64 " (%3d%%), %" PRIu64
             " mismatches so far, %ld seconds\n",
             hi, (int)((100 * hi) / g_end), g_num_mismatches,
             (long)(time(NULL) - g_start_time));
      fflush(stdout);
    }
    pthread_mutex_unlock(&g_mutex);
  }
}

int  //
main(int argc, char** argv) {
  g_end = (argc > 1) ? strtoull(argv[1], NULL, 0) : 0x100000000ull;
  int num_threads = (argc > 2) ? atoi(argv[2]) : 4;
  if ((num_threads < 1) || (num_threads > MAX_THREADS)) {
    num_threads = 4;
  }

  wuffs_base__status status = wuffs_softfloat__arith__initialize(
      &g_sf, sizeof g_sf, WUFFS_VERSION, 0);
  if (status.repr) {
    fprintf(stderr, "initialize: %s\n", status.repr);
    return 1;
  }

  // Sanity check a few values.
  if ((wuffs_softfloat__arith__log2_u64(&g_sf, 0) != 0xFFF0000000000000ull) ||
      (wuffs_softfloat__arith__log2_u64(&g_sf, 1) != 0) ||
      (wuffs_softfloat__arith__log2_u64(&g_sf, 1024) !=
       0x4024000000000000ull)) {
    printf("FAIL: sanity check\n");
    return 1;
  }

  // Exhaustive part.
  g_start_time = time(NULL);
  printf("exhaustive: v in [1, 0x%" PRIX64 "), %d threads\n", g_end,
         num_threads);
  fflush(stdout);
  pthread_t threads[MAX_THREADS];
  for (int i = 0; i < num_threads; i++) {
    pthread_create(&threads[i], NULL, worker, NULL);
  }
  for (int i = 0; i < num_threads; i++) {
    pthread_join(threads[i], NULL);
  }
  printf("exhaustive: %" PRIu64 " values checked, %" PRIu64
         " mismatches, %ld seconds\n",
         g_num_checked, g_num_mismatches, (long)(time(NULL) - g_start_time));

  // Random part: v in [1 << 32, 1 << 53), with a deterministic xorshift64*
  // PRNG, half of them uniform in log scale (random bit length) and half
  // uniform.
  uint64_t rng = 0x9E3779B97F4A7C15ull;
  uint64_t num_random = 10000000;
  uint64_t random_mismatches = 0;
  for (uint64_t i = 0; i < num_random; i++) {
    rng ^= rng >> 12;
    rng ^= rng << 25;
    rng ^= rng >> 27;
    uint64_t r = rng * 0x2545F4914F6CDD1Dull;
    uint64_t v;
    if (i & 1) {
      v = (r >> 11) | (1ull << 32);  // Uniform-ish in [1 << 32, 1 << 53).
    } else {
      int bits = 33 + (int)((r >> 58) % 21);  // Bit length in [33, 53].
      v = (r & ((1ull << (bits - 1)) - 1)) | (1ull << (bits - 1));
    }
    uint64_t have = wuffs_softfloat__arith__log2_u64(&g_sf, v);
    uint64_t want = u_of_d(log2((double)v));
    if (have != want) {
      random_mismatches++;
      report_mismatch(v, have, want);
    }
  }
  printf("random: %" PRIu64 " values checked, %" PRIu64
         " mismatches, %ld seconds in total\n",
         num_random, random_mismatches, (long)(time(NULL) - g_start_time));

  printf("%s: %" PRIu64 " mismatches in total\n",
         g_num_mismatches ? "FAIL" : "PASS", g_num_mismatches);
  return g_num_mismatches ? 1 : 0;
}
