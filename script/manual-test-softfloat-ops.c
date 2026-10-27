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

// manual-test-softfloat-ops.c compares Wuffs' std/softfloat arithmetic
// against the C compiler's native (hardware) double and float arithmetic (and
// libm's fma and log2) for many pseudo-random operands.
//
// Build it WITHOUT -ffast-math and WITHOUT -mfma / -march=native (so that the
// reference arithmetic is strict IEEE 754, with no FMA contraction):
//
//   gcc -O2 -std=c99 manual-test-softfloat-ops.c -lm -o /tmp/sfops
//   /tmp/sfops            # Default: 100 million operations per op.
//   /tmp/sfops 1000000    # Or pass the number of operations per op.
//   /tmp/sfops 1000000 f64_fma   # Or test only the named op.
//
// It prints one line per op and the first few mismatches (if any), and exits
// with a non-zero status if there were any mismatches. NaN results only need
// to be NaN (on both sides): NaN payloads and signs are not compared.

#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
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

#if defined(__FAST_MATH__) || defined(__FMA__)
#error "Build without -ffast-math and without -mfma / -march=native"
#endif

static wuffs_softfloat__arith g_sf;

// ---------------- Bit casts.

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

// ---------------- PRNG (xorshift64*).

static uint64_t g_rng = 0x2545F4914F6CDD1Dull;

static uint64_t  //
rnd() {
  g_rng ^= g_rng >> 12;
  g_rng ^= g_rng << 25;
  g_rng ^= g_rng >> 27;
  return g_rng * 0x2545F4914F6CDD1Dull;
}

// ---------------- Operand generators.

static const uint64_t g_f64_edges[] = {
    0x0000000000000000ull,  // +0
    0x8000000000000000ull,  // -0
    0x0000000000000001ull,  // min subnormal
    0x8000000000000001ull,  //
    0x000FFFFFFFFFFFFFull,  // max subnormal
    0x800FFFFFFFFFFFFFull,  //
    0x0010000000000000ull,  // min normal
    0x8010000000000000ull,  //
    0x3FF0000000000000ull,  // 1
    0xBFF0000000000000ull,  // -1
    0x7FEFFFFFFFFFFFFFull,  // max
    0xFFEFFFFFFFFFFFFFull,  //
    0x7FF0000000000000ull,  // +inf
    0xFFF0000000000000ull,  // -inf
    0x7FF8000000000000ull,  // qNaN
    0x7FF0000000000001ull,  // sNaN
    0x3FE0000000000000ull,  // 0.5
    0x4000000000000000ull,  // 2
    0x3CA0000000000000ull,  // 2**-53
    0x4340000000000000ull,  // 2**53
    0x43F0000000000000ull,  // 2**64
    0x7FE0000000000000ull,  // 2**1023
    0x0020000000000000ull,  // 2**-1021
    0x3FEFFFFFFFFFFFFFull,  // 1 - ulp
    0x3FF0000000000001ull,  // 1 + ulp
};

static const uint32_t g_f32_edges[] = {
    0x00000000u, 0x80000000u, 0x00000001u, 0x80000001u, 0x007FFFFFu,
    0x807FFFFFu, 0x00800000u, 0x80800000u, 0x3F800000u, 0xBF800000u,
    0x7F7FFFFFu, 0xFF7FFFFFu, 0x7F800000u, 0xFF800000u, 0x7FC00000u,
    0x7F800001u, 0x3F000000u, 0x40000000u, 0x33800000u, 0x4B800000u,
    0x7EFFC99Eu, 0x3F7FFFFFu, 0x3F800001u,
};

#define N_F64_EDGES (sizeof(g_f64_edges) / sizeof(g_f64_edges[0]))
#define N_F32_EDGES (sizeof(g_f32_edges) / sizeof(g_f32_edges[0]))

// gen_frac returns 52 fraction bits, biased towards patterns that produce
// exact results and ties (few set bits, all bits set) as well as random ones.
static uint64_t  //
gen_frac() {
  uint64_t r = rnd();
  switch (rnd() & 7) {
    case 0:  // Only the top few bits set.
      return r & (0x000FFFFFFFFFFFFFull << (40 + (rnd() % 13))) &
             0x000FFFFFFFFFFFFFull;
    case 1:  // Only the bottom few bits set.
      return r & ((1ull << (rnd() % 13)) - 1);
    case 2:  // Nearly all bits set.
      return 0x000FFFFFFFFFFFFFull ^ (r & ((1ull << (rnd() % 13)) - 1));
    case 3:  // Sparse.
      return r & rnd() & rnd() & 0x000FFFFFFFFFFFFFull;
  }
  return r & 0x000FFFFFFFFFFFFFull;
}

static uint64_t  //
gen_f64() {
  uint64_t sign = (rnd() & 1) << 63;
  uint64_t exp;
  switch (rnd() & 15) {
    case 0:
      return rnd();
    case 1:
      return g_f64_edges[rnd() % N_F64_EDGES];
    case 2:  // Subnormal or tiny.
      exp = rnd() % 3;
      break;
    case 3:  // Huge.
      exp = 0x7FC + (rnd() % 4);
      break;
    case 4:  // Moderate.
      exp = 0x3F0 + (rnd() % 0x20);
      break;
    case 5:  // Small integers.
      return sign | u_of_d((double)(rnd() % 2000));
    case 6:  // Just above or below the f32 range boundaries.
      exp = (rnd() & 1) ? (0x47E + (rnd() % 3)) : (0x368 + (rnd() % 0x1A));
      break;
    default:
      exp = 1 + (rnd() % 0x7FE);
      break;
  }
  return sign | (exp << 52) | gen_frac();
}

// gen_f64_near returns an f64 related to a: with a nearby exponent, or a few
// ULPs away, or a simple multiple, so that add/sub/div exercise cancellation,
// alignment and exact results.
static uint64_t  //
gen_f64_near(uint64_t a) {
  uint64_t sign = (rnd() & 1) << 63;
  switch (rnd() & 7) {
    case 0:
    case 1: {
      int64_t exp = (int64_t)((a >> 52) & 0x7FF) + (int64_t)(rnd() % 121) - 60;
      if (exp < 0) {
        exp = 0;
      } else if (exp > 0x7FE) {
        exp = 0x7FE;
      }
      return sign | ((uint64_t)exp << 52) | gen_frac();
    }
    case 2:
      return (a + (rnd() % 9) - 4) ^ sign;
    case 3:
      return u_of_d(d_of(a) * (double)(rnd() % 20)) ^ sign;
  }
  return gen_f64();
}

static uint32_t  //
gen_f32() {
  uint32_t sign = (uint32_t)(rnd() & 1) << 31;
  uint32_t exp;
  uint32_t frac = (uint32_t)(gen_frac() >> 29);
  switch (rnd() & 15) {
    case 0:
      return (uint32_t)rnd();
    case 1:
      return g_f32_edges[rnd() % N_F32_EDGES];
    case 2:
      exp = (uint32_t)(rnd() % 3);
      break;
    case 3:
      exp = 0xFC + (uint32_t)(rnd() % 4);
      break;
    case 4:
      exp = 0x70 + (uint32_t)(rnd() % 0x20);
      break;
    case 5:
      return sign | u_of_f((float)(rnd() % 2000));
    default:
      exp = 1 + (uint32_t)(rnd() % 0xFE);
      break;
  }
  return sign | (exp << 23) | frac;
}

static uint32_t  //
gen_f32_near(uint32_t a) {
  uint32_t sign = (uint32_t)(rnd() & 1) << 31;
  switch (rnd() & 7) {
    case 0:
    case 1: {
      int32_t exp = (int32_t)((a >> 23) & 0xFF) + (int32_t)(rnd() % 61) - 30;
      if (exp < 0) {
        exp = 0;
      } else if (exp > 0xFE) {
        exp = 0xFE;
      }
      return sign | ((uint32_t)exp << 23) | (uint32_t)(gen_frac() >> 29);
    }
    case 2:
      return (a + (uint32_t)(rnd() % 9) - 4) ^ sign;
  }
  return gen_f32();
}

static uint64_t  //
gen_u64() {
  uint64_t r = rnd();
  switch (rnd() & 3) {
    case 0:
      return r;
    case 1:
      return r >> (rnd() % 64);
    case 2:  // Near a power of 2, or with few bits.
      return (r & rnd() & rnd()) >> (rnd() % 64);
  }
  return ((1ull << (rnd() % 64)) + (rnd() % 5) - 2);
}

// ---------------- Checks.

static uint64_t g_num_mismatches = 0;
static uint64_t g_op_mismatches = 0;

static bool  //
same64(uint64_t have, uint64_t want) {
  if (isnan(d_of(want))) {
    return isnan(d_of(have));
  }
  return have == want;
}

static bool  //
same32(uint32_t have, uint32_t want) {
  if (isnan(f_of(want))) {
    return isnan(f_of(have));
  }
  return have == want;
}

static void  //
mismatch3(const char* op,
          uint64_t a,
          uint64_t b,
          uint64_t c,
          uint64_t have,
          uint64_t want) {
  g_num_mismatches++;
  if (g_op_mismatches++ < 10) {
    printf("  MISMATCH %s(0x%016" PRIX64 ", 0x%016" PRIX64 ", 0x%016" PRIX64
           "): have 0x%016" PRIX64 ", want 0x%016" PRIX64 "\n",
           op, a, b, c, have, want);
  }
}

// ---------------- Per-op test loops.

typedef void (*test_func)(uint64_t n);

#define LOOP_F64_BINOP(name, expr_have, expr_want)          \
  static void test_##name(uint64_t n) {                     \
    for (uint64_t i = 0; i < n; i++) {                      \
      uint64_t a = gen_f64();                               \
      uint64_t b = (rnd() & 1) ? gen_f64_near(a) : gen_f64(); \
      if (rnd() & 1) {                                      \
        uint64_t t = a;                                     \
        a = b;                                              \
        b = t;                                              \
      }                                                     \
      uint64_t have = (expr_have);                          \
      uint64_t want = (expr_want);                          \
      if (!same64(have, want)) {                            \
        mismatch3(#name, a, b, 0, have, want);              \
      }                                                     \
    }                                                       \
  }

#define LOOP_F32_BINOP(name, expr_have, expr_want)          \
  static void test_##name(uint64_t n) {                     \
    for (uint64_t i = 0; i < n; i++) {                      \
      uint32_t a = gen_f32();                               \
      uint32_t b = (rnd() & 1) ? gen_f32_near(a) : gen_f32(); \
      if (rnd() & 1) {                                      \
        uint32_t t = a;                                     \
        a = b;                                              \
        b = t;                                              \
      }                                                     \
      uint32_t have = (expr_have);                          \
      uint32_t want = (expr_want);                          \
      if (!same32(have, want)) {                            \
        mismatch3(#name, a, b, 0, have, want);              \
      }                                                     \
    }                                                       \
  }

#define SF(f) wuffs_softfloat__arith__##f

LOOP_F64_BINOP(f64_add,
               SF(f64_add)(&g_sf, a, b),
               u_of_d(d_of(a) + d_of(b)))
LOOP_F64_BINOP(f64_sub,
               SF(f64_sub)(&g_sf, a, b),
               u_of_d(d_of(a) - d_of(b)))
LOOP_F64_BINOP(f64_mul,
               SF(f64_mul)(&g_sf, a, b),
               u_of_d(d_of(a) * d_of(b)))
LOOP_F64_BINOP(f64_div,
               SF(f64_div)(&g_sf, a, b),
               u_of_d(d_of(a) / d_of(b)))
LOOP_F64_BINOP(f64_cmp,
               SF(f64_cmp)(&g_sf, a, b),
               (isnan(d_of(a)) || isnan(d_of(b)))
                   ? 3
                   : ((d_of(a) < d_of(b)) ? 0 : (d_of(a) == d_of(b)) ? 1 : 2))
LOOP_F64_BINOP(f64_predicates,
               (SF(f64_eq)(&g_sf, a, b) << 0) |
                   (SF(f64_ne)(&g_sf, a, b) << 1) |
                   (SF(f64_lt)(&g_sf, a, b) << 2) |
                   (SF(f64_le)(&g_sf, a, b) << 3) |
                   (SF(f64_gt)(&g_sf, a, b) << 4) |
                   (SF(f64_ge)(&g_sf, a, b) << 5),
               ((uint64_t)(d_of(a) == d_of(b)) << 0) |
                   ((uint64_t)(d_of(a) != d_of(b)) << 1) |
                   ((uint64_t)(d_of(a) < d_of(b)) << 2) |
                   ((uint64_t)(d_of(a) <= d_of(b)) << 3) |
                   ((uint64_t)(d_of(a) > d_of(b)) << 4) |
                   ((uint64_t)(d_of(a) >= d_of(b)) << 5))
// min and max are compared bit-exactly (including NaN payloads), since they
// just select one of their arguments.
LOOP_F64_BINOP(f64_min_max,
               SF(f64_min)(&g_sf, a, b) ^ (SF(f64_max)(&g_sf, a, b) * 3),
               u_of_d((d_of(a) < d_of(b)) ? d_of(a) : d_of(b)) ^
                   (u_of_d((d_of(a) > d_of(b)) ? d_of(a) : d_of(b)) * 3))

LOOP_F32_BINOP(f32_add,
               SF(f32_add)(&g_sf, a, b),
               u_of_f(f_of(a) + f_of(b)))
LOOP_F32_BINOP(f32_sub,
               SF(f32_sub)(&g_sf, a, b),
               u_of_f(f_of(a) - f_of(b)))
LOOP_F32_BINOP(f32_mul,
               SF(f32_mul)(&g_sf, a, b),
               u_of_f(f_of(a) * f_of(b)))
LOOP_F32_BINOP(f32_div,
               SF(f32_div)(&g_sf, a, b),
               u_of_f(f_of(a) / f_of(b)))
LOOP_F32_BINOP(f32_cmp,
               SF(f32_cmp)(&g_sf, a, b),
               (isnan(f_of(a)) || isnan(f_of(b)))
                   ? 3
                   : ((f_of(a) < f_of(b)) ? 0 : (f_of(a) == f_of(b)) ? 1 : 2))
LOOP_F32_BINOP(f32_predicates,
               (SF(f32_eq)(&g_sf, a, b) << 0) |
                   (SF(f32_ne)(&g_sf, a, b) << 1) |
                   (SF(f32_lt)(&g_sf, a, b) << 2) |
                   (SF(f32_le)(&g_sf, a, b) << 3) |
                   (SF(f32_gt)(&g_sf, a, b) << 4) |
                   (SF(f32_ge)(&g_sf, a, b) << 5),
               ((uint32_t)(f_of(a) == f_of(b)) << 0) |
                   ((uint32_t)(f_of(a) != f_of(b)) << 1) |
                   ((uint32_t)(f_of(a) < f_of(b)) << 2) |
                   ((uint32_t)(f_of(a) <= f_of(b)) << 3) |
                   ((uint32_t)(f_of(a) > f_of(b)) << 4) |
                   ((uint32_t)(f_of(a) >= f_of(b)) << 5))
LOOP_F32_BINOP(f32_min_max,
               SF(f32_min)(&g_sf, a, b) ^ (SF(f32_max)(&g_sf, a, b) * 3),
               u_of_f((f_of(a) < f_of(b)) ? f_of(a) : f_of(b)) ^
                   (u_of_f((f_of(a) > f_of(b)) ? f_of(a) : f_of(b)) * 3))

static void  //
test_f64_fma(uint64_t n) {
  for (uint64_t i = 0; i < n; i++) {
    uint64_t a = gen_f64();
    uint64_t b = (rnd() & 1) ? gen_f64_near(a) : gen_f64();
    uint64_t c;
    switch (rnd() & 3) {
      case 0:  // c is near (a * b), for cancellation.
        c = u_of_d(-(d_of(a) * d_of(b)));
        c = (rnd() & 1) ? (c + (rnd() % 5) - 2) : gen_f64_near(c);
        break;
      case 1:
        c = gen_f64_near(a);
        break;
      default:
        c = gen_f64();
        break;
    }
    uint64_t have = SF(f64_fma)(&g_sf, a, b, c);
    uint64_t want = u_of_d(fma(d_of(a), d_of(b), d_of(c)));
    if (!same64(have, want)) {
      mismatch3("f64_fma", a, b, c, have, want);
    }
  }
}

static void  //
test_f64_conversions(uint64_t n) {
  for (uint64_t i = 0; i < n; i++) {
    uint64_t u = gen_u64();
    uint64_t have = SF(f64_from_u64)(&g_sf, u);
    uint64_t want = u_of_d((double)u);
    if (!same64(have, want)) {
      mismatch3("f64_from_u64", u, 0, 0, have, want);
    }

    have = SF(f64_from_u32)(&g_sf, (uint32_t)u);
    want = u_of_d((double)(uint32_t)u);
    if (!same64(have, want)) {
      mismatch3("f64_from_u32", (uint32_t)u, 0, 0, have, want);
    }

    have = SF(f64_from_i64)(&g_sf, (int64_t)u);
    want = u_of_d((double)(int64_t)u);
    if (!same64(have, want)) {
      mismatch3("f64_from_i64", u, 0, 0, have, want);
    }

    uint64_t x = gen_f64();
    double dx = d_of(x);
    if ((dx > -1.0) && (dx < 18446744073709551616.0)) {
      have = SF(f64_to_u64_trunc)(&g_sf, x);
      want = (uint64_t)dx;
      if (have != want) {
        mismatch3("f64_to_u64_trunc", x, 0, 0, have, want);
      }
    }
    // Also exercise (size_t)(x + 0.5) for x in [0, 64), as in brotli's
    // bit_cost_inc.h.
    x = u_of_d(d_of((rnd() & 0x000FFFFFFFFFFFFFull) | 0x3FF0000000000000ull) *
               (double)(1 << (rnd() % 6)) -
               1.0);
    have = SF(f64_to_u64_trunc)(&g_sf,
                                SF(f64_add)(&g_sf, x, 0x3FE0000000000000ull));
    want = (uint64_t)(d_of(x) + 0.5);
    if (have != want) {
      mismatch3("f64_to_u64_trunc(x + 0.5)", x, 0, 0, have, want);
    }

    have = SF(f64_to_f32)(&g_sf, x = gen_f64());
    want = u_of_f((float)d_of(x));
    if (!same32((uint32_t)have, (uint32_t)want)) {
      mismatch3("f64_to_f32", x, 0, 0, have, want);
    }

    uint32_t f = gen_f32();
    have = SF(f32_to_f64)(&g_sf, f);
    want = u_of_d((double)f_of(f));
    if (!same64(have, want)) {
      mismatch3("f32_to_f64", f, 0, 0, have, want);
    }

    have = SF(f32_from_u32)(&g_sf, (uint32_t)u);
    want = u_of_f((float)(uint32_t)u);
    if (have != want) {
      mismatch3("f32_from_u32", (uint32_t)u, 0, 0, have, want);
    }

    have = SF(f32_from_i32)(&g_sf, (int32_t)u);
    want = u_of_f((float)(int32_t)u);
    if (have != want) {
      mismatch3("f32_from_i32", (uint32_t)u, 0, 0, have, want);
    }

    have = SF(f32_from_u64)(&g_sf, u);
    want = u_of_f((float)u);
    if (have != want) {
      mismatch3("f32_from_u64", u, 0, 0, have, want);
    }

    uint64_t n = rnd() >> (rnd() & 63);
    have = SF(f64_neg)(&g_sf, n) ^ (SF(f64_abs)(&g_sf, n) * 3) ^
           ((uint64_t)SF(f64_is_nan)(&g_sf, n) << 7);
    want = u_of_d(-d_of(n)) ^ (u_of_d(fabs(d_of(n))) * 3) ^
           ((uint64_t)(isnan(d_of(n)) ? 1 : 0) << 7);
    if (have != want) {
      mismatch3("f64_neg_abs_is_nan", n, 0, 0, have, want);
    }
    uint32_t m = (uint32_t)n;
    have = SF(f32_neg)(&g_sf, m) ^ (SF(f32_abs)(&g_sf, m) * 3) ^
           ((uint32_t)SF(f32_is_nan)(&g_sf, m) << 7);
    want = u_of_f(-f_of(m)) ^ (u_of_f(fabsf(f_of(m))) * 3) ^
           ((uint32_t)(isnan(f_of(m)) ? 1 : 0) << 7);
    if (have != want) {
      mismatch3("f32_neg_abs_is_nan", m, 0, 0, have, want);
    }
  }
}

// Brotli-specific patterns: (float)FastLog2(n) + 2, log2sum - (float)x,
// (float)lit_cost and similar.
static void  //
test_brotli_patterns(uint64_t n) {
  for (uint64_t i = 0; i < n; i++) {
    uint64_t v = (rnd() >> (rnd() % 64)) | 1;
    double lg = log2((double)v);
    uint64_t lg_have = SF(log2_u64)(&g_sf, v);
    if (lg_have != u_of_d(lg)) {
      mismatch3("log2_u64", v, 0, 0, lg_have, u_of_d(lg));
    }
    uint32_t have =
        SF(f32_add)(&g_sf, SF(f64_to_f32)(&g_sf, lg_have), 0x40000000u);
    uint32_t want = u_of_f((float)lg + 2);
    if (have != want) {
      mismatch3("(float)FastLog2(n) + 2", v, 0, 0, have, want);
    }
    uint64_t w = (rnd() >> (rnd() % 64)) | 1;
    double lw = log2((double)w);
    have = SF(f32_sub)(&g_sf, SF(f64_to_f32)(&g_sf, lg_have),
                       SF(f64_to_f32)(&g_sf, u_of_d(lw)));
    want = u_of_f((float)lg - (float)lw);
    if (have != want) {
      mismatch3("(float)log2(v) - (float)log2(w)", v, w, 0, have, want);
    }
    // lit_cost = log2(v) - log2(w); lit_cost += 0.029; if (lit_cost < 1.0)
    // { lit_cost *= 0.5; lit_cost += 0.5; } ... (float)lit_cost.
    double lc = lg - lw;
    lc += 0.029;
    if (lc < 1.0) {
      lc *= 0.5;
      lc += 0.5;
    }
    lc += 0.35 + (0.35 / 2000) * (double)(i % 2000);
    uint64_t sc = SF(f64_sub)(&g_sf, lg_have, u_of_d(lw));
    sc = SF(f64_add)(&g_sf, sc, u_of_d(0.029));
    if (SF(f64_lt)(&g_sf, sc, u_of_d(1.0))) {
      sc = SF(f64_mul)(&g_sf, sc, u_of_d(0.5));
      sc = SF(f64_add)(&g_sf, sc, u_of_d(0.5));
    }
    sc = SF(f64_add)(
        &g_sf, sc,
        SF(f64_add)(&g_sf, u_of_d(0.35),
                    SF(f64_mul)(&g_sf, u_of_d(0.35 / 2000),
                                SF(f64_from_u64)(&g_sf, i % 2000))));
    if (SF(f64_to_f32)(&g_sf, sc) != u_of_f((float)lc)) {
      mismatch3("lit_cost", v, w, i, SF(f64_to_f32)(&g_sf, sc),
                u_of_f((float)lc));
    }
    // (size_t)(log2p + 0.5) and bits += data * log2p.
    double log2p = lw - lg;
    uint64_t s_log2p = SF(f64_sub)(&g_sf, u_of_d(lw), lg_have);
    if (log2p >= 0) {
      if (SF(f64_to_u64_trunc)(&g_sf, SF(f64_add)(&g_sf, s_log2p,
                                                  u_of_d(0.5))) !=
          (uint64_t)(log2p + 0.5)) {
        mismatch3("(size_t)(log2p + 0.5)", v, w, 0, 0, 0);
      }
    }
    uint32_t cnt = (uint32_t)rnd();
    if (SF(f64_mul)(&g_sf, SF(f64_from_u32)(&g_sf, cnt), s_log2p) !=
        u_of_d(cnt * log2p)) {
      mismatch3("data * log2p", v, w, cnt, 0, 0);
    }
  }
}

static void  //
test_log2_f64(uint64_t n) {
  for (uint64_t i = 0; i < n; i++) {
    uint64_t x;
    switch (rnd() & 3) {
      case 0:  // Near 1.0.
        x = 0x3FF0000000000000ull + (rnd() % 0x0001000000000000ull) -
            0x0000800000000000ull;
        break;
      case 1:  // Positive.
        x = gen_f64() & 0x7FFFFFFFFFFFFFFFull;
        break;
      default:
        x = gen_f64();
        break;
    }
    uint64_t have = SF(log2_f64)(&g_sf, x);
    uint64_t want = u_of_d(log2(d_of(x)));
    if (!same64(have, want)) {
      mismatch3("log2_f64", x, 0, 0, have, want);
    }
  }
}

// ---------------- Main.

static struct {
  const char* name;
  test_func func;
  uint64_t scale;  // Divides the number of operations.
} g_ops[] = {
    {"f64_add", test_f64_add, 1},
    {"f64_sub", test_f64_sub, 1},
    {"f64_mul", test_f64_mul, 1},
    {"f64_div", test_f64_div, 1},
    {"f64_fma", test_f64_fma, 1},
    {"f64_cmp", test_f64_cmp, 1},
    {"f64_predicates", test_f64_predicates, 1},
    {"f64_min_max", test_f64_min_max, 1},
    {"f32_add", test_f32_add, 1},
    {"f32_sub", test_f32_sub, 1},
    {"f32_mul", test_f32_mul, 1},
    {"f32_div", test_f32_div, 1},
    {"f32_cmp", test_f32_cmp, 1},
    {"f32_predicates", test_f32_predicates, 1},
    {"f32_min_max", test_f32_min_max, 1},
    {"conversions", test_f64_conversions, 1},
    {"brotli_patterns", test_brotli_patterns, 4},
    {"log2_f64", test_log2_f64, 4},
};

int  //
main(int argc, char** argv) {
  uint64_t n = 100000000;
  if (argc > 1) {
    n = strtoull(argv[1], NULL, 10);
  }
  const char* only = (argc > 2) ? argv[2] : NULL;

  wuffs_base__status status = wuffs_softfloat__arith__initialize(
      &g_sf, sizeof g_sf, WUFFS_VERSION, 0);
  if (status.repr) {
    fprintf(stderr, "initialize: %s\n", status.repr);
    return 1;
  }

  for (size_t i = 0; i < sizeof(g_ops) / sizeof(g_ops[0]); i++) {
    if (only && strcmp(only, g_ops[i].name)) {
      continue;
    }
    uint64_t m = n / g_ops[i].scale;
    g_op_mismatches = 0;
    clock_t t0 = clock();
    (*g_ops[i].func)(m);
    double secs = (double)(clock() - t0) / CLOCKS_PER_SEC;
    printf("%-16s %12" PRIu64 " iterations, %8" PRIu64
           " mismatches, %7.1f seconds\n",
           g_ops[i].name, m, g_op_mismatches, secs);
    fflush(stdout);
  }
  printf("%s: %" PRIu64 " mismatches in total\n",
         g_num_mismatches ? "FAIL" : "PASS", g_num_mismatches);
  return g_num_mismatches ? 1 : 0;
}
