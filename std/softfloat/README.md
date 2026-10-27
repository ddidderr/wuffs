# Soft Float

`std/softfloat` implements IEEE 754 binary64 (`f64`, C's `double`) and binary32
(`f32`, C's `float`) arithmetic using only integer operations. Wuffs the
Language has no floating point types, but some file formats' reference
implementations make decisions (e.g. a compressor choosing between encodings)
with floating point arithmetic. Porting them to Wuffs with byte-for-byte
identical output needs bit-exact floating point results. The motivating user is
a Wuffs port of the brotli encoder, whose cost models use `double` and `float`
arithmetic and libm's `log2`.

Values are passed around as their bit patterns: an `f64` is a `base.u64` and an
`f32` is a `base.u32`, the same bits that C's `memcpy` from a `double` or
`float` would give. All of the functions are pure methods on the `arith`
struct, which has no state (other than Wuffs' usual initialization check).

    // C example. The bits of 0.1 and 0.2 are 0x3FB999999999999A and
    // 0x3FC999999999999A. Their sum is 0x3FD3333333333334 (0.30000000000000004).
    wuffs_softfloat__arith sf;
    wuffs_softfloat__arith__initialize(&sf, sizeof sf, WUFFS_VERSION, 0);
    uint64_t sum = wuffs_softfloat__arith__f64_add(
        &sf, 0x3FB999999999999A, 0x3FC999999999999A);

    // Wuffs example, with a "sf : softfloat.arith" field in this struct.
    sum = this.sf.f64_add(a: 0x3FB9_9999_9999_999A, b: 0x3FC9_9999_9999_999A)


## API

binary64:

- `f64_add(a, b)`, `f64_sub(a, b)`, `f64_mul(a, b)`, `f64_div(a, b)`: C's `a +
  b`, `a - b`, `a * b` and `a / b`.
- `f64_fma(a, b, c)`: C's `fma(a, b, c)`, i.e. `(a * b) + c` with a single
  rounding.
- `f64_neg(a)`, `f64_abs(a)`: flip or clear the sign bit (C's `-a` and
  `fabs(a)`).
- `f64_is_nan(a)`: 1 if `a` is a NaN, else 0.
- `f64_cmp(a, b)`: 0 if `a < b`, 1 if `a == b`, 2 if `a > b`, 3 if unordered
  (either is a NaN).
- `f64_eq`, `f64_ne`, `f64_lt`, `f64_le`, `f64_gt`, `f64_ge`: C's `==`, `!=`,
  `<`, `<=`, `>` and `>=`, returning 1 (true) or 0 (false). Any comparison
  involving a NaN is false (except `!=`, which is true) and `-0 == +0`. These
  return `base.u32` instead of `base.bool` because the Wuffs-to-C code
  generator cannot (yet) emit public functions that return a `base.bool`.
- `f64_min(a, b)`, `f64_max(a, b)`: exactly brotli's `BROTLI_MIN(double, a, b)`
  and `BROTLI_MAX(double, a, b)`, which are `(a < b) ? a : b` and `(a > b) ? a :
  b`. These are not IEEE 754 `minNum` / `maxNum`: given a NaN, or given `-0`
  and `+0`, they return the second argument.
- `f64_from_u64(a)`, `f64_from_u32(a)`, `f64_from_i64(a)`: C's `(double)a`,
  rounding to nearest (exact for `u32`, and for 64-bit integers whose magnitude
  is at most `1 << 53`).
- `f64_to_u64_trunc(a)`: C's `(uint64_t)a`, truncating towards zero. C only
  defines this for `a` in the range `(-1, 1 << 64)`. Out of that range, this
  function returns 0 for NaNs and negative values and `0xFFFF_FFFF_FFFF_FFFF`
  for large values, but callers should not rely on that.
- `f64_to_f32(a)`: C's `(float)a`, rounding to nearest (ties to even),
  overflowing to infinity and underflowing to subnormals or zero.
- `log2_u64(v)`: glibc's `log2((double)v)`, see below. `log2_u64(0)` is `-inf`.
- `log2_f64(x)`: glibc's `log2(x)`, for any `x` (including subnormals, zeroes,
  negative numbers, infinities and NaNs).

binary32:

- `f32_add`, `f32_sub`, `f32_mul`, `f32_div`: C's `a + b`, `a - b`, `a * b` and
  `a / b` on `float`s (evaluated in `float` precision, i.e. C's
  `FLT_EVAL_METHOD == 0`, as on x86-64).
- `f32_neg`, `f32_abs`, `f32_is_nan`, `f32_cmp`, `f32_eq`, `f32_ne`, `f32_lt`,
  `f32_le`, `f32_gt`, `f32_ge`, `f32_min`, `f32_max`: like their `f64`
  counterparts.
- `f32_to_f64(a)`: C's `(double)a`, which is exact.
- `f32_from_u32(a)`, `f32_from_i32(a)`, `f32_from_u64(a)`: C's `(float)a`,
  rounding to nearest.

All arithmetic rounds to nearest, ties to even (the default IEEE 754 rounding
mode, and the only one implemented), and handles signed zeroes, subnormals
(gradual underflow, no flush-to-zero), infinities and NaNs as IEEE 754
requires. Exception flags (inexact, overflow, etc.) are not reported.

NaNs: whenever IEEE 754 (and x86 hardware) produces a NaN, so does this
package, but the NaN's payload and sign bit are not guaranteed to match any
particular hardware. Invalid operations like `0 / 0` produce `0xFFF8...` (x86's
"default NaN") and NaN operands are propagated (quietened), preferring the
first NaN operand.


## Implementation

The `f64` operations follow the structure of John Hauser's Berkeley SoftFloat
(release 3): unpack the sign, exponent and significand, handle special cases,
compute an exact or sticky-bit-augmented integer significand (with 10 extra
low bits for guard, round and sticky), then normalize and round in one place
(`round_pack_f64`). Multiplication and fused multiply-add compute exact 128-bit
products using 32-bit limbs (Wuffs has no 128-bit integers). Division is exact
integer long division, 11 quotient bits per (64-bit integer) division step,
with the final remainder as the sticky bit. Leading zero counts use a 256-entry
table.

`f64_add` and `f64_sub` have a fast path for the common case where both
operands are normal (`f64_add_normal`): it orders the operands by magnitude,
aligns the significands with a sticky bit, adds or subtracts them and rounds
inline. Only a result that is subnormal falls back to the general code
(`f64_add_mags`, `f64_sub_mags`). Adding (or subtracting) a zero and a finite
non-zero value also returns early.

Likewise, `f32_add`, `f32_sub` and `f32_mul` compute the result directly in
binary32 (with 32-bit significands for add and sub and an exact 48-bit
product for mul) when both operands are normal and the result is normal (or,
for add and mul, overflows to infinity), and add and sub return early for a
zero plus a finite non-zero value. `f32_from_u32` converts integers below
`1 << 24` (which are exact) directly. Otherwise (zeroes, subnormal
operands or results, infinities and NaNs), they, and `f32_div` always,
convert their operands exactly to `f64`, perform the `f64` operation and then
round to `f32`. This double rounding is innocuous: binary64's 53 bits of
precision are more than `(2 * 24) + 2`, which suffices for `+`, `-`, `*` and
`/` (S. A. Figueroa, "When is double rounding innocuous?", ACM SIGNUM
Newsletter, 1995). The tests confirm this against hardware, including for
subnormal `f32` results. The fast paths never produce a NaN, so NaN results
(and their payloads) are exactly those of the "via `f64`" computation.


## log2

The encoder being ported calls libm's `log2((double)v)` for integers `v`. glibc's
`log2` is not correctly rounded (its documented maximum error is about 0.55
ULP), so a correctly rounded `log2` would not match it for every input.
Instead, `log2_f64` is a bit-exact port of the glibc function itself. (Note
that brotli's `FastLog2(v)` only calls `log2` for `v >= 256`. For smaller `v`
it reads `kBrotliLog2Table`, whose entries are `float` literals widened to
`double`, which a port should copy verbatim rather than compute.)

Specifically, it ports glibc 2.39's `__log2_fma`, the variant of
`sysdeps/ieee754/dbl-64/e_log2.c` (by Szabolcs Nagy, from ARM's
optimized-routines) that glibc's x86-64 `log2` IFUNC resolves to on CPUs with
FMA and AVX2. Compiling that C code with `-mfma` lets the compiler contract
some (but not all) `a * b + c` expressions into fused multiply-adds, which
changes the rounding, so the port follows the machine code of the shipped
Ubuntu glibc 2.39 `libm.so.6`, not just the C source. The exact sequence of
operations (9 `fma`s, 3 multiplications and 6 additions / subtractions on the
main path; 13 `fma`s, 3 multiplications and 4 additions / subtractions on the
separate near-1.0 path) was transcribed from its disassembly, and the
constants (`InvLn2hi`, `InvLn2lo`, the two polynomials and the 64-entry
`invc`/`logc` table) were read from its `__log2_data` and confirmed against
glibc's `e_log2_data.c`. Since every softfloat operation is correctly rounded,
the port then matches glibc by construction.

Other libm implementations (e.g. glibc's non-FMA variant on older x86-64 CPUs,
musl, macOS or MSVC) can produce different `log2` results for some inputs. The
port matches the configuration listed above, which is the reference
configuration for the brotli port.


## Verification

`test/c/std/softfloat.c` (run by `wuffs test std/softfloat`, under both gcc and
clang) checks every operation against the C compiler's own `double` and `float`
arithmetic on every pair of 34 (f64) or 23 (f32) edge-case values (±0,
±min/max subnormal, ±min normal, ±1, ±max, ±inf, NaNs, f32 rounding
boundaries, etc.), on those edge cases paired with pseudo-random values, and on
250,000 pseudo-random (deterministic xorshift64*) operand pairs biased towards
subnormals, huge values, nearby exponents (cancellation) and sparse or dense
fractions (exact results and rounding ties). It also checks brotli-specific
patterns such as `(size_t)(x + 0.5)`, `(float)FastLog2(n) + 2` and the
`literal_costs` Kahan summation. `fma` and `log2` results are checked against
golden checksums; with `wuffs test -mimic std/softfloat` the same checksums are
also recomputed from libm's `fma` and `log2`.

`script/manual-test-softfloat-ops.c` runs much longer randomized comparisons
(by default 10⁸ iterations per group of operations) against the hardware and
libm. Result, on an x86-64 machine (gcc 13 `-O2`, no `-mfma`, no
`-ffast-math`):

    f64_add             100000000 iterations,        0 mismatches,     8.0 seconds
    f64_sub             100000000 iterations,        0 mismatches,     8.1 seconds
    f64_mul             100000000 iterations,        0 mismatches,     8.0 seconds
    f64_div             100000000 iterations,        0 mismatches,    11.7 seconds
    f64_fma             100000000 iterations,        0 mismatches,    13.0 seconds
    f64_cmp             100000000 iterations,        0 mismatches,     7.5 seconds
    f64_predicates      100000000 iterations,        0 mismatches,     7.9 seconds
    f64_min_max         100000000 iterations,        0 mismatches,     7.7 seconds
    f32_add             100000000 iterations,        0 mismatches,    10.3 seconds
    f32_sub             100000000 iterations,        0 mismatches,    10.3 seconds
    f32_mul             100000000 iterations,        0 mismatches,    10.0 seconds
    f32_div             100000000 iterations,        0 mismatches,    14.3 seconds
    f32_cmp             100000000 iterations,        0 mismatches,     8.3 seconds
    f32_predicates      100000000 iterations,        0 mismatches,     8.5 seconds
    f32_min_max         100000000 iterations,        0 mismatches,     8.3 seconds
    conversions         100000000 iterations,        0 mismatches,    23.3 seconds
    brotli_patterns      25000000 iterations,        0 mismatches,    14.4 seconds
    log2_f64             25000000 iterations,        0 mismatches,     8.0 seconds
    PASS: 0 mismatches in total
    (3 minutes 9 seconds in total, single-threaded)

`script/manual-test-softfloat-log2.c` compares `log2_u64(v)` to glibc's
`log2((double)v)` for every `v` in `[1, 1 << 32)` and for 10⁷ pseudo-random `v`
in `[1 << 32, 1 << 53)`. Result (glibc 2.39, x86-64 with FMA and AVX2, 4
threads):

    exhaustive: 4294967295 values checked, 0 mismatches, 376 seconds
    random: 10000000 values checked, 0 mismatches, 379 seconds in total
    PASS: 0 mismatches in total
    (6 minutes 20 seconds wall clock in total)

No exception table is needed.

`script/manual-test-softfloat-sweep.c` systematically checks the `f32` add,
sub, mul and div operations over every pair of operand exponents (`[0 ..=
255]` squared), both signs and 40 x 40 mantissa patterns (exact results, ties,
carries, cancellation, subnormal results, overflow, NaN payloads): each result
must equal the hardware's (any NaN matching any NaN) and, bit-for-bit
including NaNs, the "via `f64`" composition `f64_to_f32(f64_op(f32_to_f64(a),
f32_to_f64(b)))`. It does the same for the `f64` add, sub, mul and div
operations (every pair of exponents in `[0 ..= 2047]`, 9 x 9 mantissa
patterns, NaN results checked bit-for-bit against the NaN rule above), and
checks `f64_to_f32` (326 exponents times 2²⁰ fraction patterns times both
signs), `f32_to_f64` (every `f32`), `f32_from_u32` and `f32_from_i32` (every
32-bit integer) and `f32_from_u64` (2³² structured values). Result (x86-64,
gcc 13 `-O2`, 4 threads):

    f32_add            419430400 operand pairs,        0 mismatches
    f32_sub            419430400 operand pairs,        0 mismatches
    f32_mul            419430400 operand pairs,        0 mismatches
    f32_div            419430400 operand pairs,        0 mismatches
    f64_add           1358954496 operand pairs,        0 mismatches
    f64_sub           1358954496 operand pairs,        0 mismatches
    f64_mul           1358954496 operand pairs,        0 mismatches
    f64_div           1358954496 operand pairs,        0 mismatches
    f64_to_f32         341835776 operands,             0 mismatches
    f32_to_f64        4294967296 operands,             0 mismatches
    f32_from_u32      4294967296 operands,             0 mismatches
    f32_from_i32      4294967296 operands,             0 mismatches
    f32_from_u64      4294967296 operands,             0 mismatches
    PASS: 0 mismatches in total
