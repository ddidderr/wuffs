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

// manual-test-softfloat-sweep.c systematically checks Wuffs' std/softfloat
// binary32 operations (and the f64 to / from f32 conversions) over every
// combination of operand exponents, signs and mantissa patterns, plus
// exhaustive sweeps of the integer conversions. Each f32 result is compared
// against:
//
//  - the C compiler's native (hardware) float arithmetic: the bits must be
//    identical, except that a NaN only has to match a NaN, and
//  - the "via f64" definition, f64_to_f32(f64_op(f32_to_f64(a),
//    f32_to_f64(b))), computed with the package's own (public) f64 functions:
//    here the bits must be identical even for NaNs, so that the NaN payloads
//    and signs produced by the f32 fast paths' slow path fallbacks are
//    exactly those of the via-f64 implementation.
//
// The operands for the binary operations are every (exponent_a, exponent_b)
// pair in [0 ..= 255] x [0 ..= 255], times both signs for each operand, times
// NUM_MANTISSAS x NUM_MANTISSAS mantissa patterns (zero, one, all ones, only
// top / bottom bits, half-way patterns and pseudo-random ones, so that exact
// results, rounding ties, carries, cancellation, subnormal results, overflow
// to infinity and NaN payloads are all covered).
//
// The f64 add, sub, mul and div operations are checked the same way (every
// exponent pair in [0 ..= 2047] x [0 ..= 2047], both signs, NUM_MANTISSAS64
// x NUM_MANTISSAS64 mantissa patterns) against the hardware, with NaN
// results checked bit-for-bit against the package's documented NaN rule:
// the first NaN operand, quietened, else the default NaN 0xFFF8_0000_0000_0000.
//
// Build it WITHOUT -ffast-math and WITHOUT -mfma / -march=native:
//
//   gcc -O2 -std=c99 script/manual-test-softfloat-sweep.c -lm -lpthread
//       -o /tmp/sfsweep
//
// (as one command line), then run /tmp/sfsweep (or /tmp/sfsweep OPNAME to run
// only one op: f32_add, f32_sub, f32_mul, f32_div, f64_to_f32, f32_to_f64,
// f32_from_u32, f32_from_i32 or f32_from_u64). It exits with a non-zero
// status if there were any mismatches.

#define _POSIX_C_SOURCE 200809L

#include <inttypes.h>
#include <pthread.h>
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

#define NUM_THREADS 4

static wuffs_softfloat__arith g_sf;

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

static bool  //
is_nan32(uint32_t u) {
  return (u & 0x7FFFFFFFu) > 0x7F800000u;
}

static bool  //
is_nan64(uint64_t u) {
  return (u & 0x7FFFFFFFFFFFFFFFull) > 0x7FF0000000000000ull;
}

// ---------------- Mantissa patterns.

#define NUM_MANTISSAS 40

static uint32_t g_mantissas[NUM_MANTISSAS];

static void  //
init_mantissas() {
  static const uint32_t fixed[] = {
      0x000000, 0x000001, 0x000002, 0x000003, 0x7FFFFF, 0x7FFFFE, 0x7FFFFD,
      0x400000, 0x400001, 0x3FFFFF, 0x200000, 0x600000, 0x000100, 0x0000FF,
      0x100001, 0x080000, 0x7C0000, 0x000040, 0x00003F, 0x555555, 0x2AAAAA,
      0x7FFF80, 0x000080, 0x400080, 0x3FFF80,
  };
  size_t n = sizeof(fixed) / sizeof(fixed[0]);
  memcpy(g_mantissas, fixed, sizeof(fixed));
  uint64_t x = 0x9E3779B97F4A7C15ull;
  for (size_t i = n; i < NUM_MANTISSAS; i++) {
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    uint32_t r = (uint32_t)((x * 0x2545F4914F6CDD1Dull) >> 41) & 0x7FFFFF;
    if (i & 1) {
      r &= 0x7FF000 | (r & 0xF);  // Sparse.
    }
    g_mantissas[i] = r;
  }
}

#define NUM_MANTISSAS64 9

static uint64_t g_mantissas64[NUM_MANTISSAS64];

static void  //
init_mantissas64() {
  static const uint64_t fixed[] = {
      0x0000000000000ull, 0x0000000000001ull, 0xFFFFFFFFFFFFFull,
      0x8000000000000ull, 0x8000000000001ull, 0x0000000000200ull,
  };
  size_t n = sizeof(fixed) / sizeof(fixed[0]);
  memcpy(g_mantissas64, fixed, sizeof(fixed));
  uint64_t x = 0x0123456789ABCDEFull;
  for (size_t i = n; i < NUM_MANTISSAS64; i++) {
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    uint64_t r = (x * 0x2545F4914F6CDD1Dull) >> 12;
    if (i & 1) {
      r &= 0xFFF00000000FFull;  // Sparse.
    }
    g_mantissas64[i] = r;
  }
}

// ---------------- Binary ops.

typedef uint32_t (*wuffs_op)(const wuffs_softfloat__arith*, uint32_t, uint32_t);
typedef uint64_t (*wuffs_op64)(const wuffs_softfloat__arith*,
                               uint64_t,
                               uint64_t);

static float  //
hw_add(float a, float b) {
  return a + b;
}
static float  //
hw_sub(float a, float b) {
  return a - b;
}
static float  //
hw_mul(float a, float b) {
  return a * b;
}
static float  //
hw_div(float a, float b) {
  return a / b;
}

static struct {
  const char* name;
  wuffs_op op;
  wuffs_op64 op64;
  float (*hw)(float, float);
} g_binops[] = {
    {"f32_add", wuffs_softfloat__arith__f32_add, wuffs_softfloat__arith__f64_add,
     hw_add},
    {"f32_sub", wuffs_softfloat__arith__f32_sub, wuffs_softfloat__arith__f64_sub,
     hw_sub},
    {"f32_mul", wuffs_softfloat__arith__f32_mul, wuffs_softfloat__arith__f64_mul,
     hw_mul},
    {"f32_div", wuffs_softfloat__arith__f32_div, wuffs_softfloat__arith__f64_div,
     hw_div},
};

static double  //
hw_add64(double a, double b) {
  return a + b;
}
static double  //
hw_sub64(double a, double b) {
  return a - b;
}
static double  //
hw_mul64(double a, double b) {
  return a * b;
}
static double  //
hw_div64(double a, double b) {
  return a / b;
}

static struct {
  const char* name;
  wuffs_op64 op64;
  double (*hw)(double, double);
} g_binops64[] = {
    {"f64_add", wuffs_softfloat__arith__f64_add, hw_add64},
    {"f64_sub", wuffs_softfloat__arith__f64_sub, hw_sub64},
    {"f64_mul", wuffs_softfloat__arith__f64_mul, hw_mul64},
    {"f64_div", wuffs_softfloat__arith__f64_div, hw_div64},
};

typedef struct {
  int which;
  uint32_t ea_lo;
  uint32_t ea_hi;
  uint64_t count;
  uint64_t mismatches;
} job;

static pthread_mutex_t g_print_mutex = PTHREAD_MUTEX_INITIALIZER;

static void  //
report(job* j, const char* name, uint32_t a, uint32_t b, uint32_t have,
       uint32_t want_hw, uint32_t want_via) {
  j->mismatches++;
  if (j->mismatches <= 5) {
    pthread_mutex_lock(&g_print_mutex);
    printf("MISMATCH %s(0x%08" PRIX32 ", 0x%08" PRIX32 "): have 0x%08" PRIX32
           ", hardware 0x%08" PRIX32 ", via f64 0x%08" PRIX32 "\n",
           name, a, b, have, want_hw, want_via);
    pthread_mutex_unlock(&g_print_mutex);
  }
}

static void*  //
run_binop(void* arg) {
  job* j = (job*)arg;
  wuffs_op op = g_binops[j->which].op;
  wuffs_op64 op64 = g_binops[j->which].op64;
  float (*hw)(float, float) = g_binops[j->which].hw;
  const char* name = g_binops[j->which].name;
  for (uint32_t ea = j->ea_lo; ea < j->ea_hi; ea++) {
    for (uint32_t eb = 0; eb < 256; eb++) {
      for (uint32_t ma = 0; ma < NUM_MANTISSAS; ma++) {
        for (uint32_t mb = 0; mb < NUM_MANTISSAS; mb++) {
          for (uint32_t signs = 0; signs < 4; signs++) {
            uint32_t a = ((signs & 1) << 31) | (ea << 23) | g_mantissas[ma];
            uint32_t b = ((signs >> 1) << 31) | (eb << 23) | g_mantissas[mb];
            uint32_t have = op(&g_sf, a, b);
            uint32_t want_hw = u_of_f(hw(f_of(a), f_of(b)));
            uint32_t want_via = wuffs_softfloat__arith__f64_to_f32(
                &g_sf, op64(&g_sf, wuffs_softfloat__arith__f32_to_f64(&g_sf, a),
                            wuffs_softfloat__arith__f32_to_f64(&g_sf, b)));
            j->count++;
            if (have != want_via) {
              report(j, name, a, b, have, want_hw, want_via);
            } else if (is_nan32(have) ? !is_nan32(want_hw)
                                      : (have != want_hw)) {
              report(j, name, a, b, have, want_hw, want_via);
            }
          }
        }
      }
    }
  }
  return NULL;
}

static void*  //
run_binop64(void* arg) {
  job* j = (job*)arg;
  wuffs_op64 op64 = g_binops64[j->which].op64;
  double (*hw)(double, double) = g_binops64[j->which].hw;
  const char* name = g_binops64[j->which].name;
  for (uint64_t ea = j->ea_lo; ea < j->ea_hi; ea++) {
    for (uint64_t eb = 0; eb < 2048; eb++) {
      for (uint32_t ma = 0; ma < NUM_MANTISSAS64; ma++) {
        for (uint32_t mb = 0; mb < NUM_MANTISSAS64; mb++) {
          for (uint64_t signs = 0; signs < 4; signs++) {
            uint64_t a = ((signs & 1) << 63) | (ea << 52) | g_mantissas64[ma];
            uint64_t b = ((signs >> 1) << 63) | (eb << 52) | g_mantissas64[mb];
            uint64_t have = op64(&g_sf, a, b);
            uint64_t want = u_of_d(hw(d_of(a), d_of(b)));
            j->count++;
            if (is_nan64(want)) {
              want = is_nan64(a)   ? (a | 0x0008000000000000ull)
                     : is_nan64(b) ? (b | 0x0008000000000000ull)
                                   : 0xFFF8000000000000ull;
            }
            if (have != want) {
              j->mismatches++;
              if (j->mismatches <= 5) {
                pthread_mutex_lock(&g_print_mutex);
                printf("MISMATCH %s(0x%016" PRIX64 ", 0x%016" PRIX64
                       "): have 0x%016" PRIX64 ", want 0x%016" PRIX64 "\n",
                       name, a, b, have, want);
                pthread_mutex_unlock(&g_print_mutex);
              }
            }
          }
        }
      }
    }
  }
  return NULL;
}

// ---------------- Conversions.

typedef struct {
  int which;
  uint64_t lo;
  uint64_t hi;
  uint64_t count;
  uint64_t mismatches;
} cjob;

static void  //
creport(cjob* j, const char* name, uint64_t a, uint64_t have, uint64_t want) {
  j->mismatches++;
  if (j->mismatches <= 5) {
    pthread_mutex_lock(&g_print_mutex);
    printf("MISMATCH %s(0x%016" PRIX64 "): have 0x%016" PRIX64
           ", want 0x%016" PRIX64 "\n",
           name, a, have, want);
    pthread_mutex_unlock(&g_print_mutex);
  }
}

// f32_from_u64 operands: (1 << top) plus, for top <= 26, every pattern of
// the bits below it or, for larger top, every pattern of the 13 bits just
// below it (where the rounding happens) and of the 13 bottom bits (sticky).
static uint64_t  //
u64_operand(uint64_t i) {
  uint32_t top = (uint32_t)(i >> 26) & 63;
  uint64_t low = i & 0x3FFFFFF;
  uint64_t below = low;
  if (top > 26) {
    below = ((low >> 13) << (top - 13)) | (low & 0x1FFF);
  }
  below &= (((uint64_t)1) << top) - 1;
  return (((uint64_t)1) << top) | below;
}

// f64_to_f32_exponent maps [0 ..= 325] to the f64 biased exponents 0, 1, 2,
// 0x340 ..= 0x47F (the f32 normal and subnormal range, and a little more on
// either side) and 0x7FD, 0x7FE and 0x7FF.
static uint64_t  //
f64_to_f32_exponent(uint32_t i) {
  if (i < 3) {
    return i;
  } else if (i < 323) {
    return 0x340 + (i - 3);
  }
  return 0x7FD + (i - 323);
}

static void*  //
run_conv(void* arg) {
  cjob* j = (cjob*)arg;
  for (uint64_t i = j->lo; i < j->hi; i++) {
    j->count++;
    switch (j->which) {
      case 0: {  // f64_to_f32: exponents near the f32 range, many fractions.
        uint64_t e = f64_to_f32_exponent((uint32_t)(i >> 20));
        uint64_t f = i & 0xFFFFF;
        // 10 bits at the top of the fraction, 10 around the rounding point
        // (bit 29) and 6 (repeated) at the bottom.
        uint64_t frac = ((f >> 10) << 42) | ((f & 0x3FF) << 24) |
                        ((f & 0x3F) * 0x41);
        for (int s = 0; s < 2; s++) {
          uint64_t a = ((uint64_t)s << 63) | (e << 52) | frac;
          uint32_t have = wuffs_softfloat__arith__f64_to_f32(&g_sf, a);
          uint32_t want = u_of_f((float)d_of(a));
          if (is_nan32(have) ? !is_nan32(want) : (have != want)) {
            creport(j, "f64_to_f32", a, have, want);
          } else if (is_nan32(have) &&
                     (have != (((uint32_t)(a >> 32) & 0x80000000u) |
                               0x7FC00000u |
                               (uint32_t)((a & 0xFFFFFFFFFFFFFull) >> 29)))) {
            creport(j, "f64_to_f32 (NaN payload)", a, have, want);
          }
        }
        break;
      }
      case 1: {  // f32_to_f64: every f32.
        uint32_t a = (uint32_t)i;
        uint64_t have = wuffs_softfloat__arith__f32_to_f64(&g_sf, a);
        uint64_t want = u_of_d((double)f_of(a));
        if (is_nan64(have) ? !is_nan64(want) : (have != want)) {
          creport(j, "f32_to_f64", a, have, want);
        } else if (is_nan64(have) &&
                   (have != (((uint64_t)(a >> 31) << 63) |
                             0x7FF8000000000000ull |
                             ((uint64_t)(a & 0x7FFFFF) << 29)))) {
          creport(j, "f32_to_f64 (NaN payload)", a, have, want);
        }
        break;
      }
      case 2: {  // f32_from_u32: every u32.
        uint32_t a = (uint32_t)i;
        uint32_t have = wuffs_softfloat__arith__f32_from_u32(&g_sf, a);
        uint32_t want = u_of_f((float)a);
        if (have != want) {
          creport(j, "f32_from_u32", a, have, want);
        }
        break;
      }
      case 3: {  // f32_from_i32: every i32.
        int32_t a = (int32_t)(uint32_t)i;
        uint32_t have = wuffs_softfloat__arith__f32_from_i32(&g_sf, a);
        uint32_t want = u_of_f((float)a);
        if (have != want) {
          creport(j, "f32_from_i32", (uint32_t)a, have, want);
        }
        break;
      }
      case 4: {  // f32_from_u64.
        uint64_t a = u64_operand(i);
        uint32_t have = wuffs_softfloat__arith__f32_from_u64(&g_sf, a);
        uint32_t want = u_of_f((float)a);
        if (have != want) {
          creport(j, "f32_from_u64", a, have, want);
        }
        break;
      }
    }
  }
  return NULL;
}

static const struct {
  const char* name;
  uint64_t n;
} g_convs[] = {
    {"f64_to_f32", ((uint64_t)326) << 20},
    {"f32_to_f64", ((uint64_t)1) << 32},
    {"f32_from_u32", ((uint64_t)1) << 32},
    {"f32_from_i32", ((uint64_t)1) << 32},
    {"f32_from_u64", ((uint64_t)64) << 26},
};

static double  //
now() {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (double)t.tv_sec + (1e-9 * (double)t.tv_nsec);
}

int  //
main(int argc, char** argv) {
  const char* only = (argc > 1) ? argv[1] : NULL;
  wuffs_base__status status = wuffs_softfloat__arith__initialize(
      &g_sf, sizeof g_sf, WUFFS_VERSION, 0);
  if (status.repr) {
    printf("initialize: %s\n", status.repr);
    return 1;
  }
  init_mantissas();
  init_mantissas64();
  uint64_t total_mismatches = 0;
  double t_start = now();

  for (int w = 0; w < (int)(sizeof(g_binops) / sizeof(g_binops[0])); w++) {
    if (only && strcmp(only, g_binops[w].name)) {
      continue;
    }
    double t0 = now();
    job jobs[NUM_THREADS];
    pthread_t threads[NUM_THREADS];
    for (int t = 0; t < NUM_THREADS; t++) {
      jobs[t] = (job){w, (uint32_t)(t * 256 / NUM_THREADS),
                      (uint32_t)((t + 1) * 256 / NUM_THREADS), 0, 0};
      pthread_create(&threads[t], NULL, run_binop, &jobs[t]);
    }
    uint64_t count = 0, mismatches = 0;
    for (int t = 0; t < NUM_THREADS; t++) {
      pthread_join(threads[t], NULL);
      count += jobs[t].count;
      mismatches += jobs[t].mismatches;
    }
    total_mismatches += mismatches;
    printf("%-14s %12" PRIu64 " operand pairs, %8" PRIu64
           " mismatches, %6.1f seconds\n",
           g_binops[w].name, count, mismatches, now() - t0);
    fflush(stdout);
  }

  for (int w = 0; w < (int)(sizeof(g_binops64) / sizeof(g_binops64[0]));
       w++) {
    if (only && strcmp(only, g_binops64[w].name)) {
      continue;
    }
    double t0 = now();
    job jobs[NUM_THREADS];
    pthread_t threads[NUM_THREADS];
    for (int t = 0; t < NUM_THREADS; t++) {
      jobs[t] = (job){w, (uint32_t)(t * 2048 / NUM_THREADS),
                      (uint32_t)((t + 1) * 2048 / NUM_THREADS), 0, 0};
      pthread_create(&threads[t], NULL, run_binop64, &jobs[t]);
    }
    uint64_t count = 0, mismatches = 0;
    for (int t = 0; t < NUM_THREADS; t++) {
      pthread_join(threads[t], NULL);
      count += jobs[t].count;
      mismatches += jobs[t].mismatches;
    }
    total_mismatches += mismatches;
    printf("%-14s %12" PRIu64 " operand pairs, %8" PRIu64
           " mismatches, %6.1f seconds\n",
           g_binops64[w].name, count, mismatches, now() - t0);
    fflush(stdout);
  }

  for (int w = 0; w < (int)(sizeof(g_convs) / sizeof(g_convs[0])); w++) {
    if (only && strcmp(only, g_convs[w].name)) {
      continue;
    }
    double t0 = now();
    cjob jobs[NUM_THREADS];
    pthread_t threads[NUM_THREADS];
    uint64_t n = g_convs[w].n;
    for (int t = 0; t < NUM_THREADS; t++) {
      jobs[t] = (cjob){w, (n / NUM_THREADS) * (uint64_t)t,
                       (t == NUM_THREADS - 1) ? n
                                              : (n / NUM_THREADS) * (t + 1),
                       0, 0};
      pthread_create(&threads[t], NULL, run_conv, &jobs[t]);
    }
    uint64_t count = 0, mismatches = 0;
    for (int t = 0; t < NUM_THREADS; t++) {
      pthread_join(threads[t], NULL);
      count += jobs[t].count;
      mismatches += jobs[t].mismatches;
    }
    total_mismatches += mismatches;
    printf("%-14s %12" PRIu64 " operands,      %8" PRIu64
           " mismatches, %6.1f seconds\n",
           g_convs[w].name, count, mismatches, now() - t0);
    fflush(stdout);
  }

  printf("%s: %" PRIu64 " mismatches in total (%.0f seconds)\n",
         total_mismatches ? "FAIL" : "PASS", total_mismatches,
         now() - t_start);
  return total_mismatches ? 1 : 0;
}
