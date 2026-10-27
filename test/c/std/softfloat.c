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

/*
This test program is typically run indirectly, by the "wuffs test" or "wuffs
bench" commands. These commands take an optional "-mimic" flag to check that
Wuffs' output mimics (i.e. exactly matches) other libraries' output, such as
giflib for GIF, libpng for PNG, etc.

To manually run this test:

for CC in clang gcc; do
  $CC -std=c99 -Wall -Werror softfloat.c && ./a.out
  rm -f a.out
done

Each edition should print "PASS", amongst other information, and exit(0).

Add the "wuffs mimic cflags" (everything after the colon below) to the C
compiler flags (after the .c file) to run the mimic tests.

The reference results come from the C compiler's own double and float
arithmetic, so do NOT compile this with -ffast-math, -mfma or -march=native.
The fma and log2 reference results come from golden checksums, computed by the
mimic tests (which call libm's fma and log2 directly). The log2 golden values
are those of glibc 2.39 on x86-64 with FMA (see std/softfloat/README.md), so
the log2 mimic tests are only expected to pass on such systems.

For much longer (randomized and exhaustive) tests, see
script/manual-test-softfloat-ops.c and script/manual-test-softfloat-log2.c.
*/

// ¿ wuffs mimic cflags: -DWUFFS_MIMIC -lm

// Wuffs ships as a "single file C library" or "header file library" as per
// https://github.com/nothings/stb/blob/master/docs/stb_howto.txt
//
// To use that single file as a "foo.c"-like implementation, instead of a
// "foo.h"-like header, #define WUFFS_IMPLEMENTATION before #include'ing or
// compiling it.
#define WUFFS_IMPLEMENTATION

// Defining the WUFFS_CONFIG__MODULE* macros are optional, but it lets users of
// release/c/etc.c choose which parts of Wuffs to build. That file contains the
// entire Wuffs standard library, implementing a variety of codecs and file
// formats. Without this macro definition, an optimizing compiler or linker may
// very well discard Wuffs code for unused codecs, but listing the Wuffs
// modules we use makes that process explicit. Preprocessing means that such
// code simply isn't compiled.
#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__SOFTFLOAT

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../../../release/c/wuffs-unsupported-snapshot.c"
#include "../testlib/testlib.c"
#ifdef WUFFS_MIMIC
#include <math.h>
#endif

#if defined(__FAST_MATH__) || defined(__FMA__)
#error "Build without -ffast-math and without -mfma / -march=native"
#endif

// ---------------- Helpers

static double  //
sf_d_of(uint64_t u) {
  double d;
  memcpy(&d, &u, 8);
  return d;
}

static uint64_t  //
sf_u_of_d(double d) {
  uint64_t u;
  memcpy(&u, &d, 8);
  return u;
}

static float  //
sf_f_of(uint32_t u) {
  float f;
  memcpy(&f, &u, 4);
  return f;
}

static uint32_t  //
sf_u_of_f(float f) {
  uint32_t u;
  memcpy(&u, &f, 4);
  return u;
}

static bool  //
sf_is_nan64(uint64_t u) {
  return (u & 0x7FFFFFFFFFFFFFFFull) > 0x7FF0000000000000ull;
}

static bool  //
sf_is_nan32(uint32_t u) {
  return (u & 0x7FFFFFFFu) > 0x7F800000u;
}

// sf_same64 and sf_same32 return whether have matches want. NaN payloads and
// signs are not compared: a NaN only has to match a NaN.
static bool  //
sf_same64(uint64_t have, uint64_t want) {
  return sf_is_nan64(want) ? sf_is_nan64(have) : (have == want);
}

static bool  //
sf_same32(uint32_t have, uint32_t want) {
  return sf_is_nan32(want) ? sf_is_nan32(have) : (have == want);
}

// sf_hash mixes x into the running hash h.
static uint64_t  //
sf_hash(uint64_t h, uint64_t x) {
  h ^= x;
  h *= 0x100000001B3ull;
  return h ^ (h >> 29);
}

// sf_canonical_nan64 maps every NaN to the same bit pattern, so that hashes
// do not depend on NaN payloads.
static uint64_t  //
sf_canonical_nan64(uint64_t u) {
  return sf_is_nan64(u) ? 0x7FF8000000000000ull : u;
}

// ---------------- PRNG (xorshift64*), deterministic.

static uint64_t g_sf_rng;

static uint64_t  //
sf_rnd() {
  g_sf_rng ^= g_sf_rng >> 12;
  g_sf_rng ^= g_sf_rng << 25;
  g_sf_rng ^= g_sf_rng >> 27;
  return g_sf_rng * 0x2545F4914F6CDD1Dull;
}

static const uint64_t g_sf_f64_edges[] = {
    0x0000000000000000ull,  // +0
    0x8000000000000000ull,  // -0
    0x0000000000000001ull,  // +min subnormal
    0x8000000000000001ull,  // -min subnormal
    0x000FFFFFFFFFFFFFull,  // +max subnormal
    0x800FFFFFFFFFFFFFull,  // -max subnormal
    0x0010000000000000ull,  // +min normal
    0x8010000000000000ull,  // -min normal
    0x3FF0000000000000ull,  // +1
    0xBFF0000000000000ull,  // -1
    0x7FEFFFFFFFFFFFFFull,  // +max
    0xFFEFFFFFFFFFFFFFull,  // -max
    0x7FF0000000000000ull,  // +inf
    0xFFF0000000000000ull,  // -inf
    0x7FF8000000000000ull,  // qNaN
    0xFFF8000000000000ull,  // -qNaN
    0x7FF0000000000001ull,  // sNaN
    0x3FE0000000000000ull,  // 0.5
    0x4000000000000000ull,  // 2
    0x3FEFFFFFFFFFFFFFull,  // 1 - ulp
    0x3FF0000000000001ull,  // 1 + ulp
    0x3CA0000000000000ull,  // 2**-53
    0x4340000000000000ull,  // 2**53
    0x7FE0000000000000ull,  // 2**1023
    0x0008000000000000ull,  // 2**-1023 (subnormal)
    0x400921FB54442D18ull,  // pi
    0xC00921FB54442D18ull,  // -pi
    0x3FB999999999999Aull,  // 0.1
    0x547D42AEA2879F2Eull,  // 1e99
    0x47EFFFFFE0000000ull,  // FLT_MAX
    0x47EFFFFFF0000000ull,  // FLT_MAX + half ULP (rounds to inf as f32)
    0x36A0000000000000ull,  // 2**-149 (f32 min subnormal)
    0x3690000000000000ull,  // 2**-150 (ties to f32 zero)
    0x3698000000000000ull,  // 1.5 * 2**-150 (rounds up to f32 min subnormal)
};

static const uint32_t g_sf_f32_edges[] = {
    0x00000000u,  // +0
    0x80000000u,  // -0
    0x00000001u,  // +min subnormal
    0x80000001u,  // -min subnormal
    0x007FFFFFu,  // +max subnormal
    0x807FFFFFu,  // -max subnormal
    0x00800000u,  // +min normal
    0x80800000u,  // -min normal
    0x3F800000u,  // +1
    0xBF800000u,  // -1
    0x7F7FFFFFu,  // +max
    0xFF7FFFFFu,  // -max
    0x7F800000u,  // +inf
    0xFF800000u,  // -inf
    0x7FC00000u,  // qNaN
    0x7F800001u,  // sNaN
    0x3F000000u,  // 0.5
    0x40000000u,  // 2
    0x3F7FFFFFu,  // 1 - ulp
    0x3F800001u,  // 1 + ulp
    0x7EFFC99Eu,  // 1.7e38f (brotli's kInfinity)
    0x4B800000u,  // 2**24
    0x33800000u,  // 2**-24
};

#define SF_N_F64_EDGES \
  (sizeof(g_sf_f64_edges) / sizeof(g_sf_f64_edges[0]))
#define SF_N_F32_EDGES \
  (sizeof(g_sf_f32_edges) / sizeof(g_sf_f32_edges[0]))

// sf_gen_f64 returns a pseudo-random f64 bit pattern, biased towards edge
// cases: subnormals, huge values, sparse and dense fractions (which produce
// exact results and rounding ties), etc.
static uint64_t  //
sf_gen_f64() {
  uint64_t sign = (sf_rnd() & 1) << 63;
  uint64_t exp = 0;
  uint64_t frac = sf_rnd() & 0x000FFFFFFFFFFFFFull;
  switch (sf_rnd() & 3) {
    case 0:
      frac &= 0x000FFFFFFFFFFFFFull << (40 + (sf_rnd() % 13));
      break;
    case 1:
      frac &= (1ull << (sf_rnd() % 13)) - 1;
      break;
    case 2:
      frac = 0x000FFFFFFFFFFFFFull ^ (frac & ((1ull << (sf_rnd() % 13)) - 1));
      break;
  }
  frac &= 0x000FFFFFFFFFFFFFull;
  switch (sf_rnd() & 15) {
    case 0:
      return sf_rnd();
    case 1:
      return g_sf_f64_edges[sf_rnd() % SF_N_F64_EDGES];
    case 2:
      exp = sf_rnd() % 3;
      break;
    case 3:
      exp = 0x7FC + (sf_rnd() % 4);
      break;
    case 4:
      exp = 0x3F0 + (sf_rnd() % 0x20);
      break;
    case 5:
      return sign | sf_u_of_d((double)(sf_rnd() % 2000));
    case 6:
      exp = (sf_rnd() & 1) ? (0x47E + (sf_rnd() % 3))
                           : (0x368 + (sf_rnd() % 0x1A));
      break;
    default:
      exp = 1 + (sf_rnd() % 0x7FE);
      break;
  }
  return sign | (exp << 52) | frac;
}

// sf_gen_f64_near returns an f64 related to a (nearby exponent or a few ULPs
// away), so that add and sub exercise alignment and cancellation.
static uint64_t  //
sf_gen_f64_near(uint64_t a) {
  uint64_t sign = (sf_rnd() & 1) << 63;
  switch (sf_rnd() & 3) {
    case 0: {
      int64_t exp =
          (int64_t)((a >> 52) & 0x7FF) + (int64_t)(sf_rnd() % 121) - 60;
      exp = (exp < 0) ? 0 : (exp > 0x7FE) ? 0x7FE : exp;
      return sign | ((uint64_t)exp << 52) |
             (sf_gen_f64() & 0x000FFFFFFFFFFFFFull);
    }
    case 1:
      return (a + (sf_rnd() % 9) - 4) ^ sign;
  }
  return sf_gen_f64();
}

static uint32_t  //
sf_gen_f32() {
  switch (sf_rnd() & 15) {
    case 0:
      return (uint32_t)sf_rnd();
    case 1:
      return g_sf_f32_edges[sf_rnd() % SF_N_F32_EDGES];
    case 2:  // Subnormal or tiny.
      return ((uint32_t)(sf_rnd() & 1) << 31) |
             (uint32_t)((sf_rnd() % 3) << 23) |
             (uint32_t)(sf_gen_f64() & 0x7FFFFFu);
  }
  uint32_t u = sf_u_of_f((float)sf_d_of(sf_gen_f64()));
  return sf_is_nan32(u) ? 0x7FC00000u : u;
}

static uint32_t  //
sf_gen_f32_near(uint32_t a) {
  uint32_t sign = (uint32_t)(sf_rnd() & 1) << 31;
  switch (sf_rnd() & 3) {
    case 0: {
      int32_t exp = (int32_t)((a >> 23) & 0xFF) + (int32_t)(sf_rnd() % 61) - 30;
      exp = (exp < 0) ? 0 : (exp > 0xFE) ? 0xFE : exp;
      return sign | ((uint32_t)exp << 23) | (uint32_t)(sf_rnd() & 0x7FFFFFu);
    }
    case 1:
      return (a + (uint32_t)(sf_rnd() % 9) - 4) ^ sign;
  }
  return sf_gen_f32();
}

// ---------------- Checkers

static wuffs_softfloat__arith g_sf;

static const char*  //
sf_init() {
  return wuffs_softfloat__arith__initialize(
             &g_sf, sizeof g_sf, WUFFS_VERSION,
             WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED)
      .repr;
}

#define SF(f) wuffs_softfloat__arith__##f

// sf_check_f64_pair checks every binary f64 operation (other than fma) on
// (a, b) against the hardware.
static const char*  //
sf_check_f64_pair(uint64_t a, uint64_t b) {
  double da = sf_d_of(a);
  double db = sf_d_of(b);
  struct {
    const char* name;
    uint64_t have;
    uint64_t want;
  } results[] = {
      {"add", SF(f64_add)(&g_sf, a, b), sf_u_of_d(da + db)},
      {"sub", SF(f64_sub)(&g_sf, a, b), sf_u_of_d(da - db)},
      {"mul", SF(f64_mul)(&g_sf, a, b), sf_u_of_d(da * db)},
      {"div", SF(f64_div)(&g_sf, a, b), sf_u_of_d(da / db)},
      {"eq", SF(f64_eq)(&g_sf, a, b), da == db},
      {"ne", SF(f64_ne)(&g_sf, a, b), da != db},
      {"lt", SF(f64_lt)(&g_sf, a, b), da < db},
      {"le", SF(f64_le)(&g_sf, a, b), da <= db},
      {"gt", SF(f64_gt)(&g_sf, a, b), da > db},
      {"ge", SF(f64_ge)(&g_sf, a, b), da >= db},
      {"cmp", SF(f64_cmp)(&g_sf, a, b),
       (da < db) ? 0 : (da == db) ? 1 : (da > db) ? 2 : 3},
  };
  for (size_t i = 0; i < WUFFS_TESTLIB_ARRAY_SIZE(results); i++) {
    if (!sf_same64(results[i].have, results[i].want)) {
      RETURN_FAIL("f64_%s(0x%016" PRIX64 ", 0x%016" PRIX64 "): have 0x%016" PRIX64
                  ", want 0x%016" PRIX64,
                  results[i].name, a, b, results[i].have, results[i].want);
    }
  }
  // BROTLI_MIN and BROTLI_MAX select an argument, so compare them exactly.
  uint64_t have_min = SF(f64_min)(&g_sf, a, b);
  uint64_t want_min = sf_u_of_d((da < db) ? da : db);
  uint64_t have_max = SF(f64_max)(&g_sf, a, b);
  uint64_t want_max = sf_u_of_d((da > db) ? da : db);
  if ((have_min != want_min) || (have_max != want_max)) {
    RETURN_FAIL("f64_min/max(0x%016" PRIX64 ", 0x%016" PRIX64 ")", a, b);
  }
  return NULL;
}

static const char*  //
sf_check_f32_pair(uint32_t a, uint32_t b) {
  float fa = sf_f_of(a);
  float fb = sf_f_of(b);
  struct {
    const char* name;
    uint32_t have;
    uint32_t want;
  } results[] = {
      {"add", SF(f32_add)(&g_sf, a, b), sf_u_of_f(fa + fb)},
      {"sub", SF(f32_sub)(&g_sf, a, b), sf_u_of_f(fa - fb)},
      {"mul", SF(f32_mul)(&g_sf, a, b), sf_u_of_f(fa * fb)},
      {"div", SF(f32_div)(&g_sf, a, b), sf_u_of_f(fa / fb)},
      {"eq", SF(f32_eq)(&g_sf, a, b), fa == fb},
      {"ne", SF(f32_ne)(&g_sf, a, b), fa != fb},
      {"lt", SF(f32_lt)(&g_sf, a, b), fa < fb},
      {"le", SF(f32_le)(&g_sf, a, b), fa <= fb},
      {"gt", SF(f32_gt)(&g_sf, a, b), fa > fb},
      {"ge", SF(f32_ge)(&g_sf, a, b), fa >= fb},
      {"cmp", SF(f32_cmp)(&g_sf, a, b),
       (fa < fb) ? 0u : (fa == fb) ? 1u : (fa > fb) ? 2u : 3u},
  };
  for (size_t i = 0; i < WUFFS_TESTLIB_ARRAY_SIZE(results); i++) {
    if (!sf_same32(results[i].have, results[i].want)) {
      RETURN_FAIL("f32_%s(0x%08" PRIX32 ", 0x%08" PRIX32 "): have 0x%08" PRIX32
                  ", want 0x%08" PRIX32,
                  results[i].name, a, b, results[i].have, results[i].want);
    }
  }
  uint32_t have_min = SF(f32_min)(&g_sf, a, b);
  uint32_t want_min = sf_u_of_f((fa < fb) ? fa : fb);
  uint32_t have_max = SF(f32_max)(&g_sf, a, b);
  uint32_t want_max = sf_u_of_f((fa > fb) ? fa : fb);
  if ((have_min != want_min) || (have_max != want_max)) {
    RETURN_FAIL("f32_min/max(0x%08" PRIX32 ", 0x%08" PRIX32 ")", a, b);
  }
  return NULL;
}

// sf_check_f64_unary checks the f64 conversions of a (and of a's low bits,
// interpreted as integers) against the hardware.
static const char*  //
sf_check_f64_unary(uint64_t a) {
  double da = sf_d_of(a);

  uint64_t have = SF(f64_to_f32)(&g_sf, a);
  uint64_t want = sf_u_of_f((float)da);
  if (!sf_same32((uint32_t)have, (uint32_t)want)) {
    RETURN_FAIL("f64_to_f32(0x%016" PRIX64 "): have 0x%08" PRIX64
                ", want 0x%08" PRIX64,
                a, have, want);
  }

  if ((da > -1.0) && (da < 18446744073709551616.0)) {
    have = SF(f64_to_u64_trunc)(&g_sf, a);
    want = (uint64_t)da;
    if (have != want) {
      RETURN_FAIL("f64_to_u64_trunc(0x%016" PRIX64 "): have %" PRIu64
                  ", want %" PRIu64,
                  a, have, want);
    }
  }

  have = SF(f64_neg)(&g_sf, a);
  want = sf_u_of_d(-da);
  if (have != want) {
    RETURN_FAIL("f64_neg(0x%016" PRIX64 ")", a);
  }
  have = SF(f64_abs)(&g_sf, a);
  want = a & 0x7FFFFFFFFFFFFFFFull;
  if (have != want) {
    RETURN_FAIL("f64_abs(0x%016" PRIX64 ")", a);
  }
  if (SF(f64_is_nan)(&g_sf, a) != (uint32_t)(da != da)) {
    RETURN_FAIL("f64_is_nan(0x%016" PRIX64 ")", a);
  }

  // Integer conversions, using a's bits as an integer (with varying widths).
  uint64_t u = a >> (a & 63);
  have = SF(f64_from_u64)(&g_sf, u);
  want = sf_u_of_d((double)u);
  if (have != want) {
    RETURN_FAIL("f64_from_u64(%" PRIu64 "): have 0x%016" PRIX64
                ", want 0x%016" PRIX64,
                u, have, want);
  }
  have = SF(f64_from_u32)(&g_sf, (uint32_t)u);
  want = sf_u_of_d((double)(uint32_t)u);
  if (have != want) {
    RETURN_FAIL("f64_from_u32(%" PRIu32 ")", (uint32_t)u);
  }
  have = SF(f64_from_i64)(&g_sf, (int64_t)a);
  want = sf_u_of_d((double)(int64_t)a);
  if (have != want) {
    RETURN_FAIL("f64_from_i64(%" PRId64 ")", (int64_t)a);
  }
  have = SF(f32_from_u64)(&g_sf, u);
  want = sf_u_of_f((float)u);
  if (have != want) {
    RETURN_FAIL("f32_from_u64(%" PRIu64 "): have 0x%08" PRIX64
                ", want 0x%08" PRIX64,
                u, have, want);
  }
  have = SF(f32_from_u32)(&g_sf, (uint32_t)u);
  want = sf_u_of_f((float)(uint32_t)u);
  if (have != want) {
    RETURN_FAIL("f32_from_u32(%" PRIu32 ")", (uint32_t)u);
  }
  have = SF(f32_from_i32)(&g_sf, (int32_t)a);
  want = sf_u_of_f((float)(int32_t)a);
  if (have != want) {
    RETURN_FAIL("f32_from_i32(%" PRId32 ")", (int32_t)a);
  }
  return NULL;
}

static const char*  //
sf_check_f32_unary(uint32_t a) {
  float fa = sf_f_of(a);
  uint64_t have = SF(f32_to_f64)(&g_sf, a);
  uint64_t want = sf_u_of_d((double)fa);
  if (!sf_same64(have, want)) {
    RETURN_FAIL("f32_to_f64(0x%08" PRIX32 "): have 0x%016" PRIX64
                ", want 0x%016" PRIX64,
                a, have, want);
  }
  if ((SF(f32_neg)(&g_sf, a) != sf_u_of_f(-fa)) ||
      (SF(f32_abs)(&g_sf, a) != (a & 0x7FFFFFFFu)) ||
      (SF(f32_is_nan)(&g_sf, a) != (uint32_t)(fa != fa))) {
    RETURN_FAIL("f32_neg/abs/is_nan(0x%08" PRIX32 ")", a);
  }
  return NULL;
}

// ---------------- Tests

const char*  //
test_wuffs_softfloat_f64_edge_cases() {
  CHECK_FOCUS(__func__);
  CHECK_STRING(sf_init());
  g_sf_rng = 0x1234567890ABCDEFull;

  // Every pair from the edge-case list, plus each edge case paired with
  // pseudo-random values.
  for (size_t i = 0; i < SF_N_F64_EDGES; i++) {
    uint64_t a = g_sf_f64_edges[i];
    CHECK_STRING(sf_check_f64_unary(a));
    for (size_t j = 0; j < SF_N_F64_EDGES; j++) {
      CHECK_STRING(sf_check_f64_pair(a, g_sf_f64_edges[j]));
    }
    for (int j = 0; j < 100; j++) {
      uint64_t b = sf_gen_f64();
      CHECK_STRING(sf_check_f64_pair(a, b));
      CHECK_STRING(sf_check_f64_pair(b, a));
    }
  }
  return NULL;
}

const char*  //
test_wuffs_softfloat_f64_random() {
  CHECK_FOCUS(__func__);
  CHECK_STRING(sf_init());
  g_sf_rng = 0x0123456789ABCDEFull;

  for (int i = 0; i < 250000; i++) {
    uint64_t a = sf_gen_f64();
    uint64_t b = (i & 1) ? sf_gen_f64_near(a) : sf_gen_f64();
    CHECK_STRING(sf_check_f64_pair(a, b));
    CHECK_STRING(sf_check_f64_unary(a));
  }
  return NULL;
}

const char*  //
test_wuffs_softfloat_f32_edge_cases() {
  CHECK_FOCUS(__func__);
  CHECK_STRING(sf_init());
  g_sf_rng = 0x1234567890ABCDEFull;

  for (size_t i = 0; i < SF_N_F32_EDGES; i++) {
    uint32_t a = g_sf_f32_edges[i];
    CHECK_STRING(sf_check_f32_unary(a));
    for (size_t j = 0; j < SF_N_F32_EDGES; j++) {
      CHECK_STRING(sf_check_f32_pair(a, g_sf_f32_edges[j]));
    }
    for (int j = 0; j < 100; j++) {
      uint32_t b = sf_gen_f32();
      CHECK_STRING(sf_check_f32_pair(a, b));
      CHECK_STRING(sf_check_f32_pair(b, a));
    }
  }
  return NULL;
}

const char*  //
test_wuffs_softfloat_f32_random() {
  CHECK_FOCUS(__func__);
  CHECK_STRING(sf_init());
  g_sf_rng = 0x0123456789ABCDEFull;

  for (int i = 0; i < 250000; i++) {
    uint32_t a = sf_gen_f32();
    uint32_t b = (i & 1) ? sf_gen_f32_near(a) : sf_gen_f32();
    CHECK_STRING(sf_check_f32_pair(a, b));
    CHECK_STRING(sf_check_f32_unary(a));
  }
  return NULL;
}

const char*  //
test_wuffs_softfloat_brotli_patterns() {
  CHECK_FOCUS(__func__);
  CHECK_STRING(sf_init());
  g_sf_rng = 0xFEDCBA9876543210ull;

  for (int i = 0; i < 40000; i++) {
    // log2 values come from the softfloat library itself (it is checked
    // against golden values separately); everything else is checked against
    // the hardware.
    uint64_t v = (sf_rnd() >> (sf_rnd() % 64)) | 1;
    uint64_t w = (sf_rnd() >> (sf_rnd() % 64)) | 1;
    uint64_t lv = SF(log2_u64)(&g_sf, v);
    uint64_t lw = SF(log2_u64)(&g_sf, w);
    double dv = sf_d_of(lv);
    double dw = sf_d_of(lw);

    // backward_references_hq.c: (float)FastLog2(n) + 2 and
    // log2sum - (float)FastLog2(histogram[i]), with "if (cost < 1) cost = 1".
    uint32_t have32 =
        SF(f32_add)(&g_sf, SF(f64_to_f32)(&g_sf, lv), SF(f32_from_u32)(&g_sf, 2));
    uint32_t want32 = sf_u_of_f((float)dv + 2);
    if (have32 != want32) {
      RETURN_FAIL("(float)FastLog2(%" PRIu64 ") + 2", v);
    }
    have32 = SF(f32_sub)(&g_sf, SF(f64_to_f32)(&g_sf, lv),
                         SF(f64_to_f32)(&g_sf, lw));
    if (SF(f32_lt)(&g_sf, have32, 0x3F800000u)) {
      have32 = 0x3F800000u;
    }
    float cost = (float)dv - (float)dw;
    if (cost < 1) {
      cost = 1;
    }
    if (have32 != sf_u_of_f(cost)) {
      RETURN_FAIL("log2sum - (float)FastLog2(%" PRIu64 ")", w);
    }

    // backward_references_hq.c: the Kahan-style literal cost accumulation.
    {
      float carry = 0.0f;
      float costs[2] = {0.0f, 0.0f};
      uint32_t s_carry = 0;
      uint32_t s_costs[2] = {0, 0};
      for (int j = 0; j < 4; j++) {
        uint32_t x = sf_gen_f32() & 0x7FFFFFFFu;
        if (sf_is_nan32(x) || (x == 0x7F800000u)) {
          x = 0x3F800000u;
        }
        carry += sf_f_of(x);
        costs[1] = costs[0] + carry;
        carry -= costs[1] - costs[0];
        costs[0] = costs[1];
        s_carry = SF(f32_add)(&g_sf, s_carry, x);
        s_costs[1] = SF(f32_add)(&g_sf, s_costs[0], s_carry);
        s_carry = SF(f32_sub)(&g_sf, s_carry,
                              SF(f32_sub)(&g_sf, s_costs[1], s_costs[0]));
        s_costs[0] = s_costs[1];
      }
      if ((s_costs[0] != sf_u_of_f(costs[0])) ||
          !sf_same32(s_carry, sf_u_of_f(carry))) {
        RETURN_FAIL("literal_costs accumulation (i=%d)", i);
      }
    }

    // bit_cost_inc.h: depth = (size_t)(log2p + 0.5) and
    // bits += histogram->data_[i] * log2p.
    if (v > w) {
      uint64_t t = v;
      v = w;
      w = t;
      t = lv;
      lv = lw;
      lw = t;
      dv = sf_d_of(lv);
      dw = sf_d_of(lw);
    }
    double log2p = dw - dv;
    uint64_t s_log2p = SF(f64_sub)(&g_sf, lw, lv);
    uint64_t have64 = SF(f64_to_u64_trunc)(
        &g_sf, SF(f64_add)(&g_sf, s_log2p, 0x3FE0000000000000ull));
    if (have64 != (uint64_t)(log2p + 0.5)) {
      RETURN_FAIL("(size_t)(log2p + 0.5) for v=%" PRIu64 ", w=%" PRIu64, v, w);
    }
    uint32_t count = (uint32_t)v;
    have64 = SF(f64_mul)(&g_sf, SF(f64_from_u32)(&g_sf, count), s_log2p);
    if (have64 != sf_u_of_d(count * log2p)) {
      RETURN_FAIL("count * log2p for v=%" PRIu64 ", w=%" PRIu64, v, w);
    }

    // literal_cost.c: lit_cost and (float)lit_cost.
    double lit_cost = dv - dw;
    lit_cost += 0.02905;
    if (lit_cost < 1.0) {
      lit_cost *= 0.5;
      lit_cost += 0.5;
    }
    lit_cost += 0.35 + (0.35 / 2000) * (double)(i % 2000);
    uint64_t s_lit_cost = SF(f64_sub)(&g_sf, lv, lw);
    s_lit_cost = SF(f64_add)(&g_sf, s_lit_cost, sf_u_of_d(0.02905));
    if (SF(f64_lt)(&g_sf, s_lit_cost, 0x3FF0000000000000ull)) {
      s_lit_cost = SF(f64_mul)(&g_sf, s_lit_cost, 0x3FE0000000000000ull);
      s_lit_cost = SF(f64_add)(&g_sf, s_lit_cost, 0x3FE0000000000000ull);
    }
    s_lit_cost = SF(f64_add)(
        &g_sf, s_lit_cost,
        SF(f64_add)(&g_sf, sf_u_of_d(0.35),
                    SF(f64_mul)(&g_sf, sf_u_of_d(0.35 / 2000),
                                SF(f64_from_u64)(&g_sf, (uint64_t)(i % 2000)))));
    if (SF(f64_to_f32)(&g_sf, s_lit_cost) != sf_u_of_f((float)lit_cost)) {
      RETURN_FAIL("(float)lit_cost for v=%" PRIu64 ", w=%" PRIu64, v, w);
    }

    // encode.c: 1.0 / (double)total and entropy * that.
    double inv = 1.0 / (double)v;
    uint64_t s_inv =
        SF(f64_div)(&g_sf, 0x3FF0000000000000ull, SF(f64_from_u64)(&g_sf, v));
    if ((s_inv != sf_u_of_d(inv)) ||
        (SF(f64_mul)(&g_sf, lw, s_inv) != sf_u_of_d(dw * inv))) {
      RETURN_FAIL("1.0 / (double)%" PRIu64, v);
    }
  }
  return NULL;
}

// ---------------- Golden tests (fma and log2)

// The fma and log2 golden checksums below were computed by the corresponding
// mimic tests, which call libm (glibc 2.39, x86-64 with FMA) directly.

#define SF_FMA_GOLDEN_COUNT 50000
static const uint64_t g_sf_fma_golden_hash = 0xB310EF683B324F25ull;

const char*  //
do_test_xxxxx_softfloat_fma(bool mimic) {
  CHECK_STRING(sf_init());
  g_sf_rng = 0x5555AAAA5555AAAAull;
  uint64_t h = 0;
  for (int i = 0; i < SF_FMA_GOLDEN_COUNT; i++) {
    uint64_t a = sf_gen_f64();
    uint64_t b = (i & 1) ? sf_gen_f64_near(a) : sf_gen_f64();
    uint64_t c;
    switch (sf_rnd() & 3) {
      case 0:  // c is near -(a * b), for cancellation.
        c = sf_u_of_d(-(sf_d_of(a) * sf_d_of(b)));
        c = (sf_rnd() & 1) ? (c + (sf_rnd() % 5) - 2) : sf_gen_f64_near(c);
        break;
      case 1:
        c = sf_gen_f64_near(a);
        break;
      default:
        c = sf_gen_f64();
        break;
    }
    uint64_t z = 0;
    if (mimic) {
#ifdef WUFFS_MIMIC
      z = sf_u_of_d(fma(sf_d_of(a), sf_d_of(b), sf_d_of(c)));
#endif  // WUFFS_MIMIC
    } else {
      z = SF(f64_fma)(&g_sf, a, b, c);
    }
    h = sf_hash(h, sf_canonical_nan64(z));
  }
  if (h != g_sf_fma_golden_hash) {
    RETURN_FAIL("hash: have 0x%016" PRIX64 ", want 0x%016" PRIX64, h,
                g_sf_fma_golden_hash);
  }
  return NULL;
}

// Selected log2 values: {v, glibc's log2((double)v)}.
static const uint64_t g_sf_log2_golden[][2] = {
    {0, 0xFFF0000000000000ull},
    {1, 0x0000000000000000ull},
    {2, 0x3FF0000000000000ull},
    {3, 0x3FF95C01A39FBD68ull},
    {255, 0x401FFA37C98FE55Full},
    {256, 0x4020000000000000ull},
    {257, 0x402002E1388DAF56ull},
    {1000, 0x4023EE7B471B3A95ull},
    {65535, 0x402FFFFD1D55A205ull},
    {123456789, 0x403AE12262B7B9FCull},
    {0xFFFFFFFFull, 0x403FFFFFFFFE8EACull},
    {0x100000000ull, 0x4040000000000000ull},
    {0x1FFFFFFFFFFFFFull, 0x404A800000000000ull},
    {0xFFFFFFFFFFFFFFFFull, 0x4050000000000000ull},
};

static const uint64_t g_sf_log2_golden_hash_small = 0xEC8C15638B7A6941ull;
static const uint64_t g_sf_log2_golden_hash_large = 0xBB1E14F4071E6A38ull;
static const uint64_t g_sf_log2_golden_hash_f64 = 0x05249CC766C886DBull;

static uint64_t  //
sf_log2_u64(bool mimic, uint64_t v) {
  if (mimic) {
#ifdef WUFFS_MIMIC
    return sf_u_of_d(log2((double)v));
#endif  // WUFFS_MIMIC
  }
  return SF(log2_u64)(&g_sf, v);
}

static uint64_t  //
sf_log2_f64(bool mimic, uint64_t x) {
  if (mimic) {
#ifdef WUFFS_MIMIC
    return sf_u_of_d(log2(sf_d_of(x)));
#endif  // WUFFS_MIMIC
  }
  return SF(log2_f64)(&g_sf, x);
}

const char*  //
do_test_xxxxx_softfloat_log2(bool mimic) {
  CHECK_STRING(sf_init());

  for (size_t i = 0; i < WUFFS_TESTLIB_ARRAY_SIZE(g_sf_log2_golden); i++) {
    uint64_t v = g_sf_log2_golden[i][0];
    uint64_t have = sf_log2_u64(mimic, v);
    if (have != g_sf_log2_golden[i][1]) {
      RETURN_FAIL("log2(%" PRIu64 "): have 0x%016" PRIX64 ", want 0x%016" PRIX64,
                  v, have, g_sf_log2_golden[i][1]);
    }
  }

  // Every v in [1, 65536]. Brotli's FastLog2 calls log2 for v >= 256.
  uint64_t h = 0;
  for (uint64_t v = 1; v <= 65536; v++) {
    h = sf_hash(h, sf_log2_u64(mimic, v));
  }
  if (h != g_sf_log2_golden_hash_small) {
    RETURN_FAIL("small hash: have 0x%016" PRIX64 ", want 0x%016" PRIX64, h,
                g_sf_log2_golden_hash_small);
  }

  // Pseudo-random v, with random bit lengths up to 64.
  g_sf_rng = 0x0F0F0F0F0F0F0F0Full;
  h = 0;
  for (int i = 0; i < 50000; i++) {
    uint64_t v = sf_rnd() >> (sf_rnd() % 64);
    h = sf_hash(h, sf_log2_u64(mimic, v));
  }
  if (h != g_sf_log2_golden_hash_large) {
    RETURN_FAIL("large hash: have 0x%016" PRIX64 ", want 0x%016" PRIX64, h,
                g_sf_log2_golden_hash_large);
  }

  // Pseudo-random f64 x (including near 1.0, subnormals, negatives, NaNs).
  h = 0;
  for (int i = 0; i < 50000; i++) {
    uint64_t x = (i & 1) ? (0x3FF0000000000000ull +
                            (sf_rnd() % 0x0001000000000000ull) -
                            0x0000800000000000ull)
                         : sf_gen_f64();
    h = sf_hash(h, sf_canonical_nan64(sf_log2_f64(mimic, x)));
  }
  if (h != g_sf_log2_golden_hash_f64) {
    RETURN_FAIL("f64 hash: have 0x%016" PRIX64 ", want 0x%016" PRIX64, h,
                g_sf_log2_golden_hash_f64);
  }
  return NULL;
}

const char*  //
test_wuffs_softfloat_fma() {
  CHECK_FOCUS(__func__);
  return do_test_xxxxx_softfloat_fma(false);
}

const char*  //
test_wuffs_softfloat_log2() {
  CHECK_FOCUS(__func__);
  return do_test_xxxxx_softfloat_log2(false);
}

#ifdef WUFFS_MIMIC

const char*  //
test_mimic_softfloat_fma() {
  CHECK_FOCUS(__func__);
  return do_test_xxxxx_softfloat_fma(true);
}

const char*  //
test_mimic_softfloat_log2() {
  CHECK_FOCUS(__func__);
  return do_test_xxxxx_softfloat_log2(true);
}

#endif  // WUFFS_MIMIC

// ---------------- Manifest

proc g_tests[] = {

    test_wuffs_softfloat_brotli_patterns,
    test_wuffs_softfloat_f32_edge_cases,
    test_wuffs_softfloat_f32_random,
    test_wuffs_softfloat_f64_edge_cases,
    test_wuffs_softfloat_f64_random,
    test_wuffs_softfloat_fma,
    test_wuffs_softfloat_log2,

#ifdef WUFFS_MIMIC

    test_mimic_softfloat_fma,
    test_mimic_softfloat_log2,

#endif  // WUFFS_MIMIC

    NULL,
};

proc g_benches[] = {
    NULL,
};

int  //
main(int argc, char** argv) {
  g_proc_package_name = "std/softfloat";
  return test_main(argc, argv, g_tests, g_benches);
}
