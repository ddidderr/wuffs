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

// manual-test-brotli-float.c compares std/brotli's package-private softfloat
// fast paths (the bit_cost methods in std/brotli/encode_float.wuffs) against
// the C compiler's native (hardware) float and double arithmetic, for
// pseudo-random operands that are biased towards the fast paths' edge cases:
// every exponent difference, near-ties, cancellation, results near the
// subnormal and overflow thresholds, zeroes, subnormals, infinities and NaNs.
// Being static functions, they are called directly (this program #include's
// the Wuffs C file with WUFFS_CONFIG__STATIC_FUNCTIONS).
//
// Build it WITHOUT -ffast-math and WITHOUT -mfma / -march=native:
//   gcc -O2 -std=c99 manual-test-brotli-float.c -o /tmp/brfloat
//   /tmp/brfloat            # Default: 100 million operations per op.
//   /tmp/brfloat 1000000    # Or pass the number of operations per op.
// It prints one line per op and the first few mismatches (if any), and exits
// with a non-zero status if there were any mismatches. NaN results only need
// to be NaN (on both sides).

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WUFFS_IMPLEMENTATION
#define WUFFS_CONFIG__STATIC_FUNCTIONS
#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__BROTLI
#define WUFFS_CONFIG__MODULE__SOFTFLOAT
#include "../release/c/wuffs-unsupported-snapshot.c"

#if defined(__FAST_MATH__) || defined(__FMA__)
#error "Build without -ffast-math and without -mfma / -march=native"
#endif

static wuffs_brotli__bit_cost* g_bc;

static uint64_t g_rng = 0x123456789ABCDEF1ull;

static uint64_t  //
rnd(void) {
  // xorshift64*.
  g_rng ^= g_rng >> 12;
  g_rng ^= g_rng << 25;
  g_rng ^= g_rng >> 27;
  return g_rng * 0x2545F4914F6CDD1Dull;
}

static double  //
d_of(uint64_t u) {
  double d;
  memcpy(&d, &u, 8);
  return d;
}

static uint64_t  //
u_of_d(double d) {
  uint64_t u;
  memcpy(&u, &d, 8);
  return u;
}

static float  //
f_of(uint32_t u) {
  float f;
  memcpy(&f, &u, 4);
  return f;
}

static uint32_t  //
u_of_f(float f) {
  uint32_t u;
  memcpy(&u, &f, 4);
  return u;
}

// rnd_f32 returns a random f32 bit pattern. Usually it is a normal number
// whose exponent is near ref's (so that every exponent difference and lots
// of cancellation is exercised) and whose low fraction bits are often all
// zeroes or all ones (giving near-ties).
static uint32_t  //
rnd_f32(uint32_t ref) {
  uint64_t r = rnd();
  uint32_t sign = (uint32_t)(r & 1) << 31;
  uint32_t frac = (uint32_t)(r >> 8) & 0x7FFFFF;
  switch ((r >> 1) & 7) {
    case 0:
      frac &= ~0u << ((r >> 32) % 24);
      break;
    case 1:
      frac |= ~(~0u << ((r >> 32) % 24)) & 0x7FFFFF;
      break;
  }
  uint32_t exp;
  switch ((r >> 4) & 15) {
    case 0:
      return (uint32_t)rnd();  // Anything, including NaNs.
    case 1:
      return sign | (frac & ((r >> 40) & 1 ? 0x7FFFFF : 0));  // Subnormal.
    case 2:
      return sign | 0x7F800000 | ((r >> 41) & 1 ? frac : 0);  // Inf or NaN.
    case 3:
      exp = 1 + ((r >> 42) % 4);  // Near the subnormal threshold.
      break;
    case 4:
      exp = 250 + ((r >> 42) % 5);  // Near the overflow threshold.
      break;
    case 5:
      exp = 1 + ((r >> 42) % 254);
      break;
    default: {
      int32_t e = (int32_t)((ref >> 23) & 0xFF) - 30 + (int32_t)((r >> 42) % 61);
      exp = (e < 1) ? 1 : (e > 254) ? 254 : (uint32_t)e;
      break;
    }
  }
  return sign | (exp << 23) | frac;
}

// rnd_f64 is like rnd_f32 but for f64.
static uint64_t  //
rnd_f64(uint64_t ref) {
  uint64_t r = rnd();
  uint64_t r2 = rnd();
  uint64_t sign = (r & 1) << 63;
  uint64_t frac = r2 & 0xFFFFFFFFFFFFFull;
  switch ((r >> 1) & 7) {
    case 0:
      frac &= ~0ull << ((r >> 32) % 53);
      break;
    case 1:
      frac |= ~(~0ull << ((r >> 32) % 53)) & 0xFFFFFFFFFFFFFull;
      break;
  }
  uint64_t exp;
  switch ((r >> 4) & 15) {
    case 0:
      return rnd();  // Anything, including NaNs.
    case 1:
      return sign | (frac & ((r >> 40) & 1 ? 0xFFFFFFFFFFFFFull : 0));
    case 2:
      return sign | 0x7FF0000000000000ull | ((r >> 41) & 1 ? frac : 0);
    case 3:
      exp = 1 + ((r >> 42) % 4);
      break;
    case 4:
      exp = 2042 + ((r >> 42) % 5);
      break;
    case 5:
      exp = 1 + ((r >> 42) % 2046);
      break;
    default: {
      int64_t e = (int64_t)((ref >> 52) & 0x7FF) - 60 + (int64_t)((r >> 42) % 121);
      exp = (e < 1) ? 1 : (e > 2046) ? 2046 : (uint64_t)e;
      break;
    }
  }
  return sign | (exp << 52) | frac;
}

static uint64_t g_total_mismatches;

#define CHECK32(name, have_expr, want_expr, is_cmp)                          \
  static void check_##name(uint64_t n) {                                     \
    uint64_t mismatches = 0;                                                 \
    for (uint64_t i = 0; i < n; i++) {                                       \
      uint32_t a = rnd_f32((uint32_t)rnd());                                 \
      uint32_t b = (rnd() & 3) ? rnd_f32(a) : a ^ ((rnd() & 1) << 31);       \
      if (rnd() & 1) {                                                       \
        uint32_t t = a;                                                      \
        a = b;                                                               \
        b = t;                                                               \
      }                                                                      \
      uint32_t have = (have_expr);                                           \
      uint32_t want = (want_expr);                                           \
      bool ok = (have == want) ||                                            \
                (!is_cmp && ((have & 0x7FFFFFFF) > 0x7F800000) &&            \
                 ((want & 0x7FFFFFFF) > 0x7F800000));                        \
      if (!ok && (mismatches++ < 5)) {                                       \
        printf("%s(0x%08" PRIX32 ", 0x%08" PRIX32 "): have 0x%08" PRIX32     \
               ", want 0x%08" PRIX32 "\n",                                   \
               #name, a, b, have, want);                                     \
      }                                                                      \
    }                                                                        \
    printf("%-8s %12" PRIu64 " iterations, %8" PRIu64 " mismatches\n",       \
           #name, n, mismatches);                                            \
    g_total_mismatches += mismatches;                                        \
  }

#define CHECK64(name, have_expr, want_expr, is_cmp)                          \
  static void check_##name(uint64_t n) {                                     \
    uint64_t mismatches = 0;                                                 \
    for (uint64_t i = 0; i < n; i++) {                                       \
      uint64_t a = rnd_f64(rnd());                                           \
      uint64_t b = (rnd() & 3) ? rnd_f64(a) : a ^ ((rnd() & 1) << 63);       \
      if (rnd() & 1) {                                                       \
        uint64_t t = a;                                                      \
        a = b;                                                               \
        b = t;                                                               \
      }                                                                      \
      uint64_t have = (have_expr);                                           \
      uint64_t want = (want_expr);                                           \
      bool ok = (have == want) ||                                            \
                (!is_cmp &&                                                  \
                 ((have & 0x7FFFFFFFFFFFFFFFull) > 0x7FF0000000000000ull) && \
                 ((want & 0x7FFFFFFFFFFFFFFFull) > 0x7FF0000000000000ull));  \
      if (!ok && (mismatches++ < 5)) {                                       \
        printf("%s(0x%016" PRIX64 ", 0x%016" PRIX64 "): have 0x%016" PRIX64  \
               ", want 0x%016" PRIX64 "\n",                                  \
               #name, a, b, have, want);                                     \
      }                                                                      \
    }                                                                        \
    printf("%-8s %12" PRIu64 " iterations, %8" PRIu64 " mismatches\n",       \
           #name, n, mismatches);                                            \
    g_total_mismatches += mismatches;                                        \
  }

CHECK32(f32_add,
        wuffs_brotli__bit_cost__f32_add(g_bc, a, b),
        u_of_f(f_of(a) + f_of(b)),
        false)
CHECK32(f32_sub,
        wuffs_brotli__bit_cost__f32_sub(g_bc, a, b),
        u_of_f(f_of(a) - f_of(b)),
        false)
CHECK32(f32_lt,
        wuffs_brotli__bit_cost__f32_lt(g_bc, a, b),
        f_of(a) < f_of(b),
        true)
CHECK32(f32_le,
        wuffs_brotli__bit_cost__f32_le(g_bc, a, b),
        f_of(a) <= f_of(b),
        true)
CHECK32(f32_gt,
        wuffs_brotli__bit_cost__f32_gt(g_bc, a, b),
        f_of(a) > f_of(b),
        true)
CHECK64(f64_add,
        wuffs_brotli__bit_cost__f64_add(g_bc, a, b),
        u_of_d(d_of(a) + d_of(b)),
        false)
CHECK64(f64_sub,
        wuffs_brotli__bit_cost__f64_sub(g_bc, a, b),
        u_of_d(d_of(a) - d_of(b)),
        false)
CHECK64(f64_mul,
        wuffs_brotli__bit_cost__f64_mul(g_bc, a, b),
        u_of_d(d_of(a) * d_of(b)),
        false)
CHECK64(f64_lt,
        wuffs_brotli__bit_cost__f64_lt(g_bc, a, b),
        d_of(a) < d_of(b),
        true)
CHECK64(f64_gt,
        wuffs_brotli__bit_cost__f64_gt(g_bc, a, b),
        d_of(a) > d_of(b),
        true)
CHECK64(f64_ge,
        wuffs_brotli__bit_cost__f64_ge(g_bc, a, b),
        d_of(a) >= d_of(b),
        true)

// rnd_int returns a random integer, with a random number of significant
// bits (and often with many trailing zeroes).
static uint64_t  //
rnd_int(void) {
  uint64_t r = rnd();
  uint64_t x = rnd() >> (r % 64);
  if (r & 0x100) {
    x &= ~0ull << ((r >> 9) % 64);
  }
  return x;
}

static void  //
check_conversions(uint64_t n) {
  uint64_t mismatches = 0;
  for (uint64_t i = 0; i < n; i++) {
    uint64_t x = rnd_int();
    uint32_t x32 = (uint32_t)x;
    uint64_t have = wuffs_brotli__bit_cost__f64_from_u64(g_bc, x);
    uint64_t want = u_of_d((double)x);
    if ((have != want) && (mismatches++ < 5)) {
      printf("f64_from_u64(0x%016" PRIX64 "): have 0x%016" PRIX64
             ", want 0x%016" PRIX64 "\n", x, have, want);
    }
    have = wuffs_brotli__bit_cost__f64_from_u32(g_bc, x32);
    want = u_of_d((double)x32);
    if ((have != want) && (mismatches++ < 5)) {
      printf("f64_from_u32(0x%08" PRIX32 "): have 0x%016" PRIX64
             ", want 0x%016" PRIX64 "\n", x32, have, want);
    }
    have = wuffs_brotli__bit_cost__f32_from_u32(g_bc, x32);
    want = u_of_f((float)x32);
    if ((have != want) && (mismatches++ < 5)) {
      printf("f32_from_u32(0x%08" PRIX32 "): have 0x%08" PRIX32
             ", want 0x%08" PRIX32 "\n", x32, (uint32_t)have, (uint32_t)want);
    }
    // f64_to_u64_trunc, for (non-negative) values that fit in a u64.
    uint64_t a = rnd_f64(rnd()) & 0x7FFFFFFFFFFFFFFFull;
    if ((rnd() & 3) == 0) {
      a = u_of_d((double)x) + (rnd() % 3) - 1;
    }
    if (d_of(a) < 18446744073709551616.0) {
      have = wuffs_brotli__bit_cost__f64_to_u64_trunc(g_bc, a);
      want = (uint64_t)d_of(a);
      if ((have != want) && (mismatches++ < 5)) {
        printf("f64_to_u64_trunc(0x%016" PRIX64 "): have 0x%016" PRIX64
               ", want 0x%016" PRIX64 "\n", a, have, want);
      }
    }
  }
  for (uint32_t v = 0; v < 32; v++) {
    if ((WUFFS_BROTLI__F32_OF_SMALL_INT[v] != u_of_f((float)v)) &&
        (mismatches++ < 5)) {
      printf("F32_OF_SMALL_INT[%" PRIu32 "] is wrong\n", v);
    }
  }
  printf("%-8s %12" PRIu64 " iterations, %8" PRIu64 " mismatches\n", "convert",
         n, mismatches);
  g_total_mismatches += mismatches;
}

int  //
main(int argc, char** argv) {
  uint64_t n = 100000000;
  if (argc > 1) {
    n = strtoull(argv[1], NULL, 10);
  }
  wuffs_brotli__encoder* enc = wuffs_brotli__encoder__alloc();
  if (!enc) {
    printf("alloc failed\n");
    return 1;
  }
  g_bc = &enc->private_data.f_bc;

  check_f32_add(n);
  check_f32_sub(n);
  check_f32_lt(n);
  check_f32_le(n);
  check_f32_gt(n);
  check_f64_add(n);
  check_f64_sub(n);
  check_f64_mul(n);
  check_f64_lt(n);
  check_f64_gt(n);
  check_f64_ge(n);
  check_conversions(n);

  free(enc);
  if (g_total_mismatches) {
    printf("FAIL: %" PRIu64 " mismatches in total\n", g_total_mismatches);
    return 1;
  }
  printf("PASS: 0 mismatches in total\n");
  return 0;
}
