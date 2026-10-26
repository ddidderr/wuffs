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
  $CC -std=c99 -Wall -Werror brotli.c && ./a.out
  rm -f a.out
done

Each edition should print "PASS", amongst other information, and exit(0).

Add the "wuffs mimic cflags" (everything after the colon below) to the C
compiler flags (after the .c file) to run the mimic tests.

To manually run the benchmarks, replace "-Wall -Werror" with "-O3" and replace
the first "./a.out" with "./a.out -bench". Combine these changes with the
"wuffs mimic cflags" to run the mimic benchmarks.
*/

// ¿ wuffs mimic cflags: -DWUFFS_MIMIC -lbrotlienc -lbrotlidec -lbrotlicommon -lm

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
#define WUFFS_CONFIG__MODULE__BROTLI
#define WUFFS_CONFIG__MODULE__SOFTFLOAT

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../../../release/c/wuffs-unsupported-snapshot.c"
#include "../testlib/testlib.c"
#ifdef WUFFS_MIMIC
#include "../mimiclib/brotli.c"
#endif

// ---------------- Golden Tests

golden_test g_brotli_alice29_gt = {
    .want_filename = "test/data/brotli/alice29.txt",
    .src_filename = "test/data/brotli/alice29.txt.compressed",
};

golden_test g_brotli_alice29_q11w10_gt = {
    .want_filename = "test/data/brotli/alice29.txt",
    .src_filename = "test/data/brotli/alice29.txt.q11w10.br",
};

golden_test g_brotli_romeo_gt = {
    .want_filename = "test/data/romeo.txt",
    .src_filename = "test/data/brotli/romeo.txt.q11w22.br",
};

// g_brotli_vectors lists every (compressed, decompressed) pair of test files.
// See test/data/brotli/README.md for where they came from.
struct {
  const char* src_filename;
  const char* want_filename;
} g_brotli_vectors[] = {
    {"test/data/brotli/10x10y.compressed", "test/data/brotli/10x10y"},
    {"test/data/brotli/256.bytes.q0w10.br", "test/data/256.bytes"},
    {"test/data/brotli/256.bytes.q0w22.br", "test/data/256.bytes"},
    {"test/data/brotli/256.bytes.q0w24.br", "test/data/256.bytes"},
    {"test/data/brotli/256.bytes.q11w10.br", "test/data/256.bytes"},
    {"test/data/brotli/256.bytes.q11w22.br", "test/data/256.bytes"},
    {"test/data/brotli/256.bytes.q11w24.br", "test/data/256.bytes"},
    {"test/data/brotli/256.bytes.q5w10.br", "test/data/256.bytes"},
    {"test/data/brotli/256.bytes.q5w22.br", "test/data/256.bytes"},
    {"test/data/brotli/256.bytes.q5w24.br", "test/data/256.bytes"},
    {"test/data/brotli/64x.compressed", "test/data/brotli/64x"},
    {"test/data/brotli/alice29.txt.compressed", "test/data/brotli/alice29.txt"},
    {"test/data/brotli/alice29.txt.q11w10.br", "test/data/brotli/alice29.txt"},
    {"test/data/brotli/alice29.txt.q1w10.br", "test/data/brotli/alice29.txt"},
    {"test/data/brotli/alice29.txt.q5w10.br", "test/data/brotli/alice29.txt"},
    {"test/data/brotli/alice29.txt.q9w16.br", "test/data/brotli/alice29.txt"},
    {"test/data/brotli/asyoulik.txt.compressed",
     "test/data/brotli/asyoulik.txt"},
    {"test/data/brotli/backward65536.compressed",
     "test/data/brotli/backward65536"},
    {"test/data/brotli/compressed_file.compressed",
     "test/data/brotli/compressed_file"},
    {"test/data/brotli/compressed_repeated.compressed",
     "test/data/brotli/compressed_repeated"},
    {"test/data/brotli/cp1251-utf16le.compressed",
     "test/data/brotli/cp1251-utf16le"},
    {"test/data/brotli/cp852-utf8.compressed", "test/data/brotli/cp852-utf8"},
    {"test/data/brotli/empty.compressed", "test/data/brotli/empty"},
    {"test/data/brotli/empty.compressed.00", "test/data/brotli/empty"},
    {"test/data/brotli/empty.compressed.01", "test/data/brotli/empty"},
    {"test/data/brotli/empty.compressed.02", "test/data/brotli/empty"},
    {"test/data/brotli/empty.compressed.03", "test/data/brotli/empty"},
    {"test/data/brotli/empty.compressed.04", "test/data/brotli/empty"},
    {"test/data/brotli/empty.compressed.05", "test/data/brotli/empty"},
    {"test/data/brotli/empty.compressed.06", "test/data/brotli/empty"},
    {"test/data/brotli/empty.compressed.07", "test/data/brotli/empty"},
    {"test/data/brotli/empty.compressed.08", "test/data/brotli/empty"},
    {"test/data/brotli/empty.compressed.09", "test/data/brotli/empty"},
    {"test/data/brotli/empty.compressed.10", "test/data/brotli/empty"},
    {"test/data/brotli/empty.compressed.11", "test/data/brotli/empty"},
    {"test/data/brotli/empty.compressed.12", "test/data/brotli/empty"},
    {"test/data/brotli/empty.compressed.13", "test/data/brotli/empty"},
    {"test/data/brotli/empty.compressed.14", "test/data/brotli/empty"},
    {"test/data/brotli/empty.compressed.15", "test/data/brotli/empty"},
    {"test/data/brotli/empty.compressed.16", "test/data/brotli/empty"},
    {"test/data/brotli/empty.compressed.17", "test/data/brotli/empty"},
    {"test/data/brotli/empty.compressed.18", "test/data/brotli/empty"},
    {"test/data/brotli/monkey.compressed", "test/data/brotli/monkey"},
    {"test/data/brotli/quickfox.compressed", "test/data/brotli/quickfox"},
    {"test/data/brotli/quickfox_repeated.compressed",
     "test/data/brotli/quickfox_repeated"},
    {"test/data/brotli/random_org_10k.bin.compressed",
     "test/data/brotli/random_org_10k.bin"},
    {"test/data/brotli/romeo.txt.q0w10.br", "test/data/romeo.txt"},
    {"test/data/brotli/romeo.txt.q0w22.br", "test/data/romeo.txt"},
    {"test/data/brotli/romeo.txt.q0w24.br", "test/data/romeo.txt"},
    {"test/data/brotli/romeo.txt.q11w10.br", "test/data/romeo.txt"},
    {"test/data/brotli/romeo.txt.q11w22.br", "test/data/romeo.txt"},
    {"test/data/brotli/romeo.txt.q11w24.br", "test/data/romeo.txt"},
    {"test/data/brotli/romeo.txt.q5w10.br", "test/data/romeo.txt"},
    {"test/data/brotli/romeo.txt.q5w22.br", "test/data/romeo.txt"},
    {"test/data/brotli/romeo.txt.q5w24.br", "test/data/romeo.txt"},
    {"test/data/brotli/ukkonooa.compressed", "test/data/brotli/ukkonooa"},
    {"test/data/brotli/x.compressed", "test/data/brotli/x"},
    {"test/data/brotli/x.compressed.00", "test/data/brotli/x"},
    {"test/data/brotli/x.compressed.01", "test/data/brotli/x"},
    {"test/data/brotli/x.compressed.02", "test/data/brotli/x"},
    {"test/data/brotli/x.compressed.03", "test/data/brotli/x"},
    {"test/data/brotli/xyzzy.compressed", "test/data/brotli/xyzzy"},
    {"test/data/brotli/zeros.compressed", "test/data/brotli/zeros"},
};

// g_brotli_malformed lists invalid (or truncated) inputs. Each want_status is
// the Wuffs status that the C code's BrotliDecoderErrorCode (in the comment)
// maps to. See std/brotli/README.md for that mapping. Each input is either a
// file (if src_filename is non-NULL) or inline data.
//
// The test/data/brotli/invalid.*.br files were found by mutating valid
// test/data/brotli files and keeping the smallest input that libbrotlidec
// v1.2.1 rejects with each error code.
//
// BROTLI_DECODER_ERROR_FORMAT_BLOCK_SWITCH is not covered: triggering it needs
// more than (1 << 24) commands in one metablock.
struct {
  const char* src_filename;
  const char* data;
  size_t len;
  const char* want_status;
} g_brotli_malformed[] = {
    // BROTLI_DECODER_ERROR_FORMAT_EXUBERANT_NIBBLE.
    {"test/data/brotli/invalid.exuberant-nibble.br", NULL, 0,
     wuffs_brotli__error__bad_exuberant_nibble},
    // BROTLI_DECODER_ERROR_FORMAT_RESERVED: a metadata block's reserved bit.
    // The inline data's bits (LSB first) are: WBITS=0 (16), ISLAST=0,
    // MNIBBLES=11 (metadata) and a reserved bit of 1.
    {NULL, "\x1C", 1, wuffs_brotli__error__bad_reserved_bit},
    {"test/data/brotli/invalid.reserved.br", NULL, 0,
     wuffs_brotli__error__bad_reserved_bit},
    // BROTLI_DECODER_ERROR_FORMAT_EXUBERANT_META_NIBBLE.
    {"test/data/brotli/invalid.exuberant-meta-nibble.br", NULL, 0,
     wuffs_brotli__error__bad_exuberant_metadata_nibble},
    // BROTLI_DECODER_ERROR_FORMAT_SIMPLE_HUFFMAN_ALPHABET.
    {"test/data/brotli/invalid.simple-huffman-alphabet.br", NULL, 0,
     wuffs_brotli__error__bad_simple_huffman_alphabet},
    // BROTLI_DECODER_ERROR_FORMAT_SIMPLE_HUFFMAN_SAME.
    {"test/data/brotli/invalid.simple-huffman-same.br", NULL, 0,
     wuffs_brotli__error__bad_simple_huffman_symbol},
    // BROTLI_DECODER_ERROR_FORMAT_CL_SPACE.
    {"test/data/brotli/invalid.cl-space.br", NULL, 0,
     wuffs_brotli__error__bad_code_length_code_lengths},
    // BROTLI_DECODER_ERROR_FORMAT_HUFFMAN_SPACE.
    {"test/data/brotli/invalid.huffman-space.br", NULL, 0,
     wuffs_brotli__error__bad_huffman_space},
    // BROTLI_DECODER_ERROR_FORMAT_CONTEXT_MAP_REPEAT.
    {"test/data/brotli/invalid.context-map-repeat.br", NULL, 0,
     wuffs_brotli__error__bad_context_map_repeat},
    // BROTLI_DECODER_ERROR_FORMAT_BLOCK_LENGTH_1.
    {"test/data/brotli/invalid.block-length-1.br", NULL, 0,
     wuffs_brotli__error__bad_block_length},
    // BROTLI_DECODER_ERROR_FORMAT_BLOCK_LENGTH_2.
    {"test/data/brotli/invalid.block-length-2.br", NULL, 0,
     wuffs_brotli__error__bad_metablock_length},
    // BROTLI_DECODER_ERROR_FORMAT_TRANSFORM.
    {"test/data/brotli/invalid.transform.br", NULL, 0,
     wuffs_brotli__error__bad_transform},
    // BROTLI_DECODER_ERROR_FORMAT_DICTIONARY.
    {"test/data/brotli/invalid.dictionary.br", NULL, 0,
     wuffs_brotli__error__bad_dictionary},
    // BROTLI_DECODER_ERROR_FORMAT_WINDOW_BITS: the large-window escape
    // (WBITS = 0b0010001) is invalid unless large-window brotli is enabled.
    {NULL, "\x11", 1, wuffs_brotli__error__unsupported_large_window},
    // BROTLI_DECODER_ERROR_FORMAT_PADDING_1: an empty metadata block followed
    // by a non-zero padding bit.
    {NULL, "\x8C\x00", 2, wuffs_brotli__error__bad_padding},
    // BROTLI_DECODER_ERROR_FORMAT_PADDING_2: an ISLAST and ISLASTEMPTY
    // metablock followed by a non-zero padding bit.
    {NULL, "\x0E", 1, wuffs_brotli__error__bad_final_padding},
    // BROTLI_DECODER_ERROR_FORMAT_DISTANCE: a distance that is too far back.
    {"test/data/brotli/invalid.distance.br", NULL, 0,
     wuffs_brotli__error__bad_distance},
    // BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT: an empty input.
    {NULL, "", 0, wuffs_brotli__error__truncated_input},
    // BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT: a truncated input (the first 20
    // bytes of romeo.txt.q11w22.br).
    {"test/data/brotli/invalid.truncated.br", NULL, 0,
     wuffs_brotli__error__truncated_input},
};

const char*  //
read_brotli_malformed(wuffs_base__io_buffer* src, size_t i) {
  if (g_brotli_malformed[i].src_filename) {
    return read_file(src, g_brotli_malformed[i].src_filename);
  } else if (g_brotli_malformed[i].len > src->data.len) {
    return "read_brotli_malformed: data is too long";
  }
  if (g_brotli_malformed[i].len > 0) {
    memcpy(src->data.ptr, g_brotli_malformed[i].data,
           g_brotli_malformed[i].len);
  }
  src->meta.wi = g_brotli_malformed[i].len;
  src->meta.ri = 0;
  src->meta.closed = true;
  return NULL;
}

// ---------------- Brotli Tests

const char*  //
test_wuffs_brotli_decode_interface() {
  CHECK_FOCUS(__func__);
  wuffs_brotli__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_brotli__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
  return do_test__wuffs_base__io_transformer(
      wuffs_brotli__decoder__upcast_as__wuffs_base__io_transformer(&dec),
      "test/data/brotli/romeo.txt.q5w22.br", 0, SIZE_MAX, 942, 0x0A);
}

const char*  //
wuffs_brotli_decode(wuffs_base__io_buffer* dst,
                    wuffs_base__io_buffer* src,
                    uint32_t wuffs_initialize_flags,
                    uint64_t wlimit,
                    uint64_t rlimit) {
  wuffs_brotli__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_brotli__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION, wuffs_initialize_flags));

  while (true) {
    wuffs_base__io_buffer limited_dst = make_limited_writer(*dst, wlimit);
    wuffs_base__io_buffer limited_src = make_limited_reader(*src, rlimit);

    wuffs_base__status status = wuffs_brotli__decoder__transform_io(
        &dec, &limited_dst, &limited_src, g_work_slice_u8);

    dst->meta.wi += limited_dst.meta.wi;
    src->meta.ri += limited_src.meta.ri;

    if (((wlimit < UINT64_MAX) &&
         (status.repr == wuffs_base__suspension__short_write)) ||
        ((rlimit < UINT64_MAX) &&
         (status.repr == wuffs_base__suspension__short_read))) {
      continue;
    }
    return status.repr;
  }
}

const char*  //
do_test_brotli_decode_all(const char* (*codec_func)(wuffs_base__io_buffer*,
                                                    wuffs_base__io_buffer*,
                                                    uint32_t,
                                                    uint64_t,
                                                    uint64_t),
                          uint64_t wlimit,
                          uint64_t rlimit) {
  size_t n = WUFFS_TESTLIB_ARRAY_SIZE(g_brotli_vectors);
  for (size_t i = 0; i < n; i++) {
    golden_test gt = ((golden_test){
        .want_filename = g_brotli_vectors[i].want_filename,
        .src_filename = g_brotli_vectors[i].src_filename,
    });
    const char* status = do_test_io_buffers(codec_func, &gt, wlimit, rlimit);
    if (status) {
      RETURN_FAIL("%s: %s", gt.src_filename, status);
    }
  }
  return NULL;
}

const char*  //
test_wuffs_brotli_decode_all() {
  CHECK_FOCUS(__func__);
  return do_test_brotli_decode_all(wuffs_brotli_decode, UINT64_MAX, UINT64_MAX);
}

const char*  //
test_wuffs_brotli_decode_all_with_io_limits() {
  CHECK_FOCUS(__func__);
  return do_test_brotli_decode_all(wuffs_brotli_decode, 1, 1);
}

const char*  //
test_wuffs_brotli_decode_alice29() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(wuffs_brotli_decode, &g_brotli_alice29_gt,
                            UINT64_MAX, UINT64_MAX);
}

const char*  //
test_wuffs_brotli_decode_alice29_q11w10() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(wuffs_brotli_decode, &g_brotli_alice29_q11w10_gt,
                            UINT64_MAX, UINT64_MAX);
}

const char*  //
test_wuffs_brotli_decode_romeo() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(wuffs_brotli_decode, &g_brotli_romeo_gt, UINT64_MAX,
                            UINT64_MAX);
}

const char*  //
test_wuffs_brotli_decode_malformed() {
  CHECK_FOCUS(__func__);
  size_t n = WUFFS_TESTLIB_ARRAY_SIZE(g_brotli_malformed);
  for (size_t i = 0; i < n; i++) {
    for (int j = 0; j < 2; j++) {
      wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
          .data = g_src_slice_u8,
      });
      wuffs_base__io_buffer have = ((wuffs_base__io_buffer){
          .data = g_have_slice_u8,
      });
      CHECK_STRING(read_brotli_malformed(&src, i));
      uint64_t limit = (j == 0) ? UINT64_MAX : 1;
      const char* have_status = wuffs_brotli_decode(
          &have, &src, WUFFS_INITIALIZE__DEFAULT_OPTIONS, limit, limit);
      // With an I/O limit, running out of (limited) input can make the C
      // code's WriteRingBuffer reject an overrun metablock earlier, as
      // BLOCK_LENGTH_1 instead of BLOCK_LENGTH_2.
      const char* want_status = g_brotli_malformed[i].want_status;
      if ((j == 1) &&
          (want_status == wuffs_brotli__error__bad_metablock_length) &&
          (have_status == wuffs_brotli__error__bad_block_length)) {
        continue;
      }
      if (have_status != want_status) {
        RETURN_FAIL("i=%zu, j=%d: have \"%s\", want \"%s\"", i, j, have_status,
                    want_status);
      }
    }
  }
  return NULL;
}

const char*  //
test_wuffs_brotli_decode_truncated() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  wuffs_base__io_buffer have = ((wuffs_base__io_buffer){
      .data = g_have_slice_u8,
  });
  CHECK_STRING(read_file(&src, "test/data/brotli/romeo.txt.q11w22.br"));
  if (src.meta.wi < 2) {
    RETURN_FAIL("source file was too short");
  }
  for (size_t n = 0; n < src.meta.wi; n++) {
    wuffs_base__io_buffer s = src;
    s.meta.wi = n;
    s.meta.closed = true;
    have.meta.wi = 0;
    const char* have_status = wuffs_brotli_decode(
        &have, &s, WUFFS_INITIALIZE__DEFAULT_OPTIONS, UINT64_MAX, UINT64_MAX);
    if (have_status != wuffs_brotli__error__truncated_input) {
      RETURN_FAIL("n=%zu: have \"%s\", want \"%s\"", n, have_status,
                  wuffs_brotli__error__truncated_input);
    }
  }
  return NULL;
}

const char*  //
test_wuffs_brotli_decode_trailing_data() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  wuffs_base__io_buffer have = ((wuffs_base__io_buffer){
      .data = g_have_slice_u8,
  });
  wuffs_base__io_buffer want = ((wuffs_base__io_buffer){
      .data = g_want_slice_u8,
  });
  CHECK_STRING(read_file(&src, "test/data/brotli/romeo.txt.q5w22.br"));
  CHECK_STRING(read_file(&want, "test/data/romeo.txt"));
  size_t n = src.meta.wi;
  if ((n + 3) > src.data.len) {
    RETURN_FAIL("source file was too long");
  }
  // Append some trailing data that the decoder should not read.
  memcpy(src.data.ptr + n, "XYZ", 3);
  src.meta.wi += 3;

  for (int i = 0; i < 2; i++) {
    uint64_t limit = (i == 0) ? UINT64_MAX : 1;
    src.meta.ri = 0;
    have.meta.wi = 0;
    CHECK_STRING(wuffs_brotli_decode(
        &have, &src, WUFFS_INITIALIZE__DEFAULT_OPTIONS, limit, limit));
    CHECK_STRING(check_io_buffers_equal("", &have, &want));
    if (src.meta.ri != n) {
      RETURN_FAIL("i=%d: ri: have %zu, want %zu", i, src.meta.ri, n);
    }
  }
  return NULL;
}

const char*  //
test_wuffs_brotli_decode_workbuf_length() {
  CHECK_FOCUS(__func__);
  // romeo.txt.q5w22.br has a (1 << 22) byte window. The workbuf also needs
  // 542 bytes of slack.
  for (int i = 0; i < 3; i++) {
    wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
        .data = g_src_slice_u8,
    });
    wuffs_base__io_buffer have = ((wuffs_base__io_buffer){
        .data = g_have_slice_u8,
    });
    CHECK_STRING(read_file(&src, "test/data/brotli/romeo.txt.q5w22.br"));
    wuffs_brotli__decoder dec;
    CHECK_STATUS("initialize", wuffs_brotli__decoder__initialize(
                                   &dec, sizeof dec, WUFFS_VERSION,
                                   WUFFS_INITIALIZE__DEFAULT_OPTIONS));
    wuffs_base__slice_u8 workbuf = g_work_slice_u8;
    const char* want = NULL;
    if (i == 0) {
      workbuf.len = 1000;
      want = wuffs_base__error__bad_workbuf_length;
    } else if (i == 1) {
      workbuf.len = (1 << 22) + 541;
      want = wuffs_base__error__bad_workbuf_length;
    } else {
      workbuf.len = (1 << 22) + 542;
    }
    wuffs_base__status status =
        wuffs_brotli__decoder__transform_io(&dec, &have, &src, workbuf);
    if (status.repr != want) {
      RETURN_FAIL("i=%d: have \"%s\", want \"%s\"", i, status.repr, want);
    }
  }
  return NULL;
}

// ---------------- Mimic Tests

#ifdef WUFFS_MIMIC

const char*  //
test_mimic_brotli_decode_all() {
  CHECK_FOCUS(__func__);
  return do_test_brotli_decode_all(mimic_brotli_decode, UINT64_MAX, UINT64_MAX);
}

const char*  //
test_mimic_brotli_decode_alice29() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(mimic_brotli_decode, &g_brotli_alice29_gt,
                            UINT64_MAX, UINT64_MAX);
}

const char*  //
test_mimic_brotli_decode_romeo() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(mimic_brotli_decode, &g_brotli_romeo_gt, UINT64_MAX,
                            UINT64_MAX);
}

// brotli_c_error_code returns the libbrotlidec BrotliDecoderErrorCode that
// corresponds to a Wuffs status. A truncated input maps to
// BROTLI_DECODER_NEEDS_MORE_INPUT (2).
int  //
brotli_c_error_code(const char* status) {
  static const struct {
    const char* status;
    int c_error_code;
  } table[] = {
      {wuffs_brotli__error__bad_exuberant_nibble, -1},
      {wuffs_brotli__error__bad_reserved_bit, -2},
      {wuffs_brotli__error__bad_exuberant_metadata_nibble, -3},
      {wuffs_brotli__error__bad_simple_huffman_alphabet, -4},
      {wuffs_brotli__error__bad_simple_huffman_symbol, -5},
      {wuffs_brotli__error__bad_code_length_code_lengths, -6},
      {wuffs_brotli__error__bad_huffman_space, -7},
      {wuffs_brotli__error__bad_context_map_repeat, -8},
      {wuffs_brotli__error__bad_block_length, -9},
      {wuffs_brotli__error__bad_metablock_length, -10},
      {wuffs_brotli__error__bad_transform, -11},
      {wuffs_brotli__error__bad_dictionary, -12},
      {wuffs_brotli__error__bad_window_bits, -13},
      {wuffs_brotli__error__unsupported_large_window, -13},
      {wuffs_brotli__error__bad_padding, -14},
      {wuffs_brotli__error__bad_final_padding, -15},
      {wuffs_brotli__error__bad_distance, -16},
      {wuffs_brotli__error__bad_block_switch, -17},
      {wuffs_brotli__error__truncated_input, 2},
  };
  if (!status) {
    return 1;  // BROTLI_DECODER_SUCCESS.
  }
  for (size_t i = 0; i < WUFFS_TESTLIB_ARRAY_SIZE(table); i++) {
    if (table[i].status == status) {
      return table[i].c_error_code;
    }
  }
  return 0;
}

// do_test_mimic_brotli_differential decodes src with both Wuffs and
// libbrotlidec and checks that they produce the same output and that they
// either both succeed or both fail.
const char*  //
do_test_mimic_brotli_differential(const char* name,
                                  wuffs_base__io_buffer* src,
                                  bool want_error) {
  wuffs_base__io_buffer have = ((wuffs_base__io_buffer){
      .data = g_have_slice_u8,
  });
  wuffs_base__io_buffer want = ((wuffs_base__io_buffer){
      .data = g_want_slice_u8,
  });
  wuffs_base__io_buffer src0 = *src;
  const char* have_status = wuffs_brotli_decode(
      &have, &src0, WUFFS_INITIALIZE__DEFAULT_OPTIONS, UINT64_MAX, UINT64_MAX);
  wuffs_base__io_buffer src1 = *src;
  int error_code = 0;
  const char* want_status =
      mimic_brotli_decode_error_code(&want, &src1, &error_code);
  if ((have_status == NULL) != (want_status == NULL)) {
    RETURN_FAIL("%s: have \"%s\", want \"%s\" (error code %d)", name,
                have_status, want_status, error_code);
  } else if ((have_status != NULL) &&
             (brotli_c_error_code(have_status) != error_code)) {
    RETURN_FAIL("%s: have \"%s\", want error code %d", name, have_status,
                error_code);
  } else if ((have_status != NULL) != want_error) {
    RETURN_FAIL("%s: have \"%s\", want_error %d", name, have_status,
                (int)want_error);
  } else if (!want_error && (src0.meta.ri != src1.meta.ri)) {
    RETURN_FAIL("%s: ri: have %zu, want %zu", name, src0.meta.ri, src1.meta.ri);
  }
  // On error, libbrotlidec only guarantees to have written the bytes that it
  // flushed so far, so compare only the common prefix.
  if (want_error) {
    if (have.meta.wi > want.meta.wi) {
      have.meta.wi = want.meta.wi;
    } else {
      want.meta.wi = have.meta.wi;
    }
  }
  const char* status = check_io_buffers_equal("", &have, &want);
  if (status) {
    RETURN_FAIL("%s: %s", name, status);
  }
  return NULL;
}

const char*  //
test_mimic_brotli_decode_differential() {
  CHECK_FOCUS(__func__);
  size_t n = WUFFS_TESTLIB_ARRAY_SIZE(g_brotli_vectors);
  for (size_t i = 0; i < n; i++) {
    wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
        .data = g_src_slice_u8,
    });
    CHECK_STRING(read_file(&src, g_brotli_vectors[i].src_filename));
    CHECK_STRING(do_test_mimic_brotli_differential(
        g_brotli_vectors[i].src_filename, &src, false));
  }
  return NULL;
}

const char*  //
test_mimic_brotli_decode_malformed() {
  CHECK_FOCUS(__func__);
  size_t n = WUFFS_TESTLIB_ARRAY_SIZE(g_brotli_malformed);
  for (size_t i = 0; i < n; i++) {
    wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
        .data = g_src_slice_u8,
    });
    CHECK_STRING(read_brotli_malformed(&src, i));
    char name[64];
    snprintf(name, sizeof name, "g_brotli_malformed[%zu]", i);
    CHECK_STRING(do_test_mimic_brotli_differential(name, &src, true));
  }
  return NULL;
}

#endif  // WUFFS_MIMIC

// ---------------- Encoder Tests

// The encoder tests describe each encoding as a "plan": a sequence of steps.
// Step i applies the BrotliEncoderOperation g_brotli_enc_step_ops[i] (0 =
// PROCESS, 1 = FLUSH, 2 = FINISH, 3 = EMIT_METADATA) to the next
// g_brotli_enc_step_lens[i] bytes of the source. The wuffs_brotli_encode_plan
// and mimic_brotli_encode functions run a plan with Wuffs' std/brotli encoder
// and libbrotlienc, respectively, so that their outputs can be compared.

#define BROTLI_ENC_OP_PROCESS 0
#define BROTLI_ENC_OP_FLUSH 1
#define BROTLI_ENC_OP_FINISH 2
#define BROTLI_ENC_OP_EMIT_METADATA 3

// The BrotliEncoderParameter values.
#define BROTLI_ENC_PARAM_MODE 0
#define BROTLI_ENC_PARAM_QUALITY 1
#define BROTLI_ENC_PARAM_LGWIN 2
#define BROTLI_ENC_PARAM_LGBLOCK 3
#define BROTLI_ENC_PARAM_DISABLE_LITERAL_CONTEXT_MODELING 4
#define BROTLI_ENC_PARAM_SIZE_HINT 5
#define BROTLI_ENC_PARAM_LARGE_WINDOW 6
#define BROTLI_ENC_PARAM_NPOSTFIX 7
#define BROTLI_ENC_PARAM_NDIRECT 8
#define BROTLI_ENC_PARAM_STREAM_OFFSET 9

#define BROTLI_ENC_MAX_STEPS (1 << 19)
#define BROTLI_ENC_MAX_PARAMS 16

uint32_t g_brotli_enc_step_ops[BROTLI_ENC_MAX_STEPS];
size_t g_brotli_enc_step_lens[BROTLI_ENC_MAX_STEPS];
size_t g_brotli_enc_num_steps;

// The chunkings (how the input is split into steps).
#define BROTLI_ENC_CHUNK_SINGLE 0
#define BROTLI_ENC_CHUNK_1 1
#define BROTLI_ENC_CHUNK_7 2
#define BROTLI_ENC_CHUNK_4096 3
#define BROTLI_ENC_CHUNK_RANDOM 4
#define BROTLI_ENC_NUM_CHUNKINGS 5

// The flush modes.
#define BROTLI_ENC_FLUSH_NONE 0
#define BROTLI_ENC_FLUSH_EVERY_CHUNK 1
#define BROTLI_ENC_FLUSH_MIDDLE 2
#define BROTLI_ENC_NUM_FLUSHES 3

// The metadata positions.
#define BROTLI_ENC_METADATA_NONE 0
#define BROTLI_ENC_METADATA_START 1
#define BROTLI_ENC_METADATA_MIDDLE 2
#define BROTLI_ENC_METADATA_END 3

typedef struct {
  uint32_t param_keys[BROTLI_ENC_MAX_PARAMS];
  uint32_t param_values[BROTLI_ENC_MAX_PARAMS];
  size_t num_params;
  uint32_t chunking;
  uint32_t flush;
  uint32_t metadata_position;
  size_t metadata_len;
  size_t dst_chunk;
  uint64_t seed;
} brotli_enc_config;

void  //
brotli_enc_config__add_param(brotli_enc_config* c,
                             uint32_t key,
                             uint32_t value) {
  if (c->num_params < BROTLI_ENC_MAX_PARAMS) {
    c->param_keys[c->num_params] = key;
    c->param_values[c->num_params] = value;
    c->num_params++;
  }
}

uint64_t  //
brotli_enc_rand(uint64_t* state) {
  // SplitMix64.
  uint64_t z = (*state += 0x9E3779B97F4A7C15ull);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

void  //
brotli_enc_fill_random(uint8_t* p, size_t n, uint64_t seed) {
  uint64_t state = seed;
  for (size_t i = 0; i < n; i++) {
    p[i] = (uint8_t)(brotli_enc_rand(&state) >> 56);
  }
}

const char*  //
brotli_enc_add_step(uint32_t op, size_t len) {
  if (g_brotli_enc_num_steps >= BROTLI_ENC_MAX_STEPS) {
    return "too many steps";
  }
  g_brotli_enc_step_ops[g_brotli_enc_num_steps] = op;
  g_brotli_enc_step_lens[g_brotli_enc_num_steps] = len;
  g_brotli_enc_num_steps++;
  return NULL;
}

// make_brotli_enc_plan sets the g_brotli_enc_step_* plan for encoding the
// data[:data_len] input with the config c. It also writes the plan's source
// (the input, with any metadata bytes interleaved) to src.
const char*  //
make_brotli_enc_plan(wuffs_base__io_buffer* src,
                     const brotli_enc_config* c,
                     const uint8_t* data,
                     size_t data_len) {
  g_brotli_enc_num_steps = 0;
  src->meta.ri = 0;
  src->meta.wi = 0;
  src->meta.pos = 0;
  src->meta.closed = true;
  if ((data_len + c->metadata_len) > src->data.len) {
    return "make_brotli_enc_plan: src is too short";
  }
  uint64_t rng = c->seed;
  size_t metadata_at = SIZE_MAX;
  if (c->metadata_position == BROTLI_ENC_METADATA_START) {
    metadata_at = 0;
  } else if (c->metadata_position == BROTLI_ENC_METADATA_MIDDLE) {
    metadata_at = data_len / 2;
  } else if (c->metadata_position == BROTLI_ENC_METADATA_END) {
    metadata_at = data_len;
  }
  bool flushed_middle = false;

  size_t i = 0;
  while (true) {
    if (i == metadata_at) {
      brotli_enc_fill_random(src->data.ptr + src->meta.wi, c->metadata_len,
                             c->seed ^ 0x5A5A);
      src->meta.wi += c->metadata_len;
      CHECK_STRING(
          brotli_enc_add_step(BROTLI_ENC_OP_EMIT_METADATA, c->metadata_len));
      metadata_at = SIZE_MAX;
    }

    size_t n = data_len - i;
    switch (c->chunking) {
      case BROTLI_ENC_CHUNK_1:
        n = 1;
        break;
      case BROTLI_ENC_CHUNK_7:
        n = 7;
        break;
      case BROTLI_ENC_CHUNK_4096:
        n = 4096;
        break;
      case BROTLI_ENC_CHUNK_RANDOM: {
        uint64_t r = brotli_enc_rand(&rng);
        // Mostly small chunks, with occasional large ones.
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

    memcpy(src->data.ptr + src->meta.wi, data + i, n);
    src->meta.wi += n;
    i += n;
    bool last = (i == data_len) && (metadata_at == SIZE_MAX);

    if (last) {
      CHECK_STRING(brotli_enc_add_step(BROTLI_ENC_OP_FINISH, n));
      break;
    } else if (c->flush == BROTLI_ENC_FLUSH_EVERY_CHUNK) {
      CHECK_STRING(brotli_enc_add_step(BROTLI_ENC_OP_FLUSH, n));
    } else {
      CHECK_STRING(brotli_enc_add_step(BROTLI_ENC_OP_PROCESS, n));
      if ((c->flush == BROTLI_ENC_FLUSH_MIDDLE) && !flushed_middle &&
          ((2 * i) >= data_len)) {
        flushed_middle = true;
        CHECK_STRING(brotli_enc_add_step(BROTLI_ENC_OP_FLUSH, 0));
      }
    }
  }
  return NULL;
}

// brotli_enc_quirk_key maps a BrotliEncoderParameter value to the
// corresponding std/brotli quirk key (or 0 if there is none).
uint32_t  //
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

// wuffs_brotli_encode_plan is the Wuffs equivalent of mimic_brotli_encode
// (in test/c/mimiclib/brotli.c).
const char*  //
wuffs_brotli_encode_plan(wuffs_base__io_buffer* dst,
                         wuffs_base__io_buffer* src,
                         const uint32_t* param_keys,
                         const uint32_t* param_values,
                         size_t num_params,
                         const uint32_t* step_ops,
                         const size_t* step_lens,
                         size_t num_steps,
                         size_t dst_chunk) {
  // The encoder struct is large (more than 1 MiB), mostly arrays that do not
  // need zero-initializing, so don't use wuffs_brotli__encoder__alloc.
  wuffs_brotli__encoder* enc = malloc(sizeof__wuffs_brotli__encoder());
  if (!enc) {
    return "malloc failed";
  }
  const char* ret = wuffs_brotli__encoder__initialize(
                        enc, sizeof__wuffs_brotli__encoder(), WUFFS_VERSION,
                        WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED)
                        .repr;
  if (ret) {
    goto cleanup;
  }
  for (size_t j = 0; j < num_params; j++) {
    wuffs_base__status status = wuffs_brotli__encoder__set_quirk(
        enc, brotli_enc_quirk_key(param_keys[j]),
        param_values[j]);
    if (status.repr) {
      ret = status.repr;
      goto cleanup;
    }
  }
  wuffs_base__range_ii_u64 workbuf_len =
      wuffs_brotli__encoder__workbuf_len(enc);
  if (workbuf_len.max_incl > g_work_slice_u8.len) {
    ret = "workbuf_len is too large";
    goto cleanup;
  }
  wuffs_base__slice_u8 workbuf = wuffs_base__make_slice_u8(
      g_work_slice_u8.ptr, (size_t)workbuf_len.max_incl);

  for (size_t i = 0; i < num_steps; i++) {
    size_t end = src->meta.ri + step_lens[i];
    if (end > src->meta.wi) {
      ret = "wuffs_brotli_encode_plan: step length exceeds the input";
      goto cleanup;
    }
    uint32_t op = step_ops[i];
    wuffs_base__status status = wuffs_base__make_status(NULL);
    if (op == BROTLI_ENC_OP_FLUSH) {
      status = wuffs_brotli__encoder__request_flush(enc);
    } else if (op == BROTLI_ENC_OP_EMIT_METADATA) {
      status = wuffs_brotli__encoder__request_emit_metadata(enc, step_lens[i]);
    }
    if (status.repr) {
      ret = status.repr;
      goto cleanup;
    }

    while (true) {
      wuffs_base__io_buffer d = *dst;
      if ((dst_chunk > 0) && ((d.data.len - d.meta.wi) > dst_chunk)) {
        d.data.len = d.meta.wi + dst_chunk;
      }
      wuffs_base__io_buffer s = *src;
      s.meta.wi = end;
      s.meta.closed = (op == BROTLI_ENC_OP_FINISH);
      status = wuffs_brotli__encoder__transform_io(enc, &d, &s, workbuf);
      dst->meta.wi = d.meta.wi;
      src->meta.ri = s.meta.ri;

      if (status.repr == NULL) {
        if (op != BROTLI_ENC_OP_FINISH) {
          ret = "wuffs_brotli_encode_plan: unexpected ok status";
          goto cleanup;
        }
        break;
      } else if (status.repr == wuffs_base__suspension__short_write) {
        if (dst->meta.wi == dst->data.len) {
          ret = "wuffs_brotli_encode_plan: dst is full";
          goto cleanup;
        }
        continue;
      } else if ((status.repr == wuffs_base__suspension__short_read) &&
                 (op != BROTLI_ENC_OP_FINISH) && (src->meta.ri == end)) {
        break;
      }
      ret = status.repr;
      goto cleanup;
    }
  }

cleanup:
  free(enc);
  return ret;
}

// wuffs_brotli_encode encodes src to dst in one go, at the given quality and
// lgwin, with a dst limit of wlimit bytes per transform_io call.
const char*  //
wuffs_brotli_encode(wuffs_base__io_buffer* dst,
                    wuffs_base__io_buffer* src,
                    uint32_t quality,
                    uint32_t lgwin,
                    size_t dst_chunk) {
  uint32_t keys[2] = {BROTLI_ENC_PARAM_QUALITY, BROTLI_ENC_PARAM_LGWIN};
  uint32_t values[2] = {quality, lgwin};
  uint32_t op = BROTLI_ENC_OP_FINISH;
  size_t len = wuffs_base__io_buffer__reader_length(src);
  return wuffs_brotli_encode_plan(dst, src, keys, values, 2, &op, &len, 1,
                                  dst_chunk);
}

// do_test_brotli_encode_round_trip encodes data[:data_len] (with the plan's
// config c) and then decodes it (with Wuffs' std/brotli decoder), checking
// that the result is the original data.
const char*  //
do_test_brotli_encode_round_trip(const char* name,
                                 const brotli_enc_config* c,
                                 const uint8_t* data,
                                 size_t data_len) {
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(make_brotli_enc_plan(&src, c, data, data_len));
  wuffs_base__io_buffer have = ((wuffs_base__io_buffer){
      .data = g_have_slice_u8,
  });
  const char* status = wuffs_brotli_encode_plan(
      &have, &src, c->param_keys, c->param_values, c->num_params,
      g_brotli_enc_step_ops, g_brotli_enc_step_lens, g_brotli_enc_num_steps,
      c->dst_chunk);
  if (status) {
    RETURN_FAIL("%s: encode: %s", name, status);
  }
  have.meta.closed = true;

  wuffs_base__io_buffer decoded = ((wuffs_base__io_buffer){
      .data = g_want_slice_u8,
  });
  status = wuffs_brotli_decode(&decoded, &have, WUFFS_INITIALIZE__DEFAULT_OPTIONS,
                               UINT64_MAX, UINT64_MAX);
  if (status) {
    RETURN_FAIL("%s: decode: %s", name, status);
  } else if (have.meta.ri != have.meta.wi) {
    RETURN_FAIL("%s: decode: trailing data", name);
  } else if (decoded.meta.wi != data_len) {
    RETURN_FAIL("%s: decoded length: have %zu, want %zu", name,
                decoded.meta.wi, data_len);
  } else if (memcmp(decoded.data.ptr, data, data_len)) {
    RETURN_FAIL("%s: decoded bytes differ", name);
  }
  return NULL;
}

// brotli_enc_corpus_filenames lists the round trip (and parity) test inputs:
// every file in test/data/brotli plus some others.
const char* brotli_enc_corpus_filenames[] = {
    "test/data/256.bytes",
    "test/data/enwik5",
    "test/data/romeo.txt",
    "test/data/brotli/README.md",
};

// for_each_brotli_enc_corpus_file calls f on every corpus file, loading each
// one into a malloc'ed buffer.
const char*  //
for_each_brotli_enc_corpus_file(
    const char* (*f)(const char* name, const uint8_t* data, size_t len)) {
  const char* names[256];
  size_t num_names = 0;
  for (size_t i = 0; i < WUFFS_TESTLIB_ARRAY_SIZE(brotli_enc_corpus_filenames);
       i++) {
    names[num_names++] = brotli_enc_corpus_filenames[i];
  }
  for (size_t i = 0; i < WUFFS_TESTLIB_ARRAY_SIZE(g_brotli_vectors); i++) {
    names[num_names++] = g_brotli_vectors[i].src_filename;
    names[num_names++] = g_brotli_vectors[i].want_filename;
  }
  for (size_t i = 0; i < WUFFS_TESTLIB_ARRAY_SIZE(g_brotli_malformed); i++) {
    if (g_brotli_malformed[i].src_filename) {
      names[num_names++] = g_brotli_malformed[i].src_filename;
    }
  }

  uint8_t* buf = malloc(1 << 20);
  if (!buf) {
    return "malloc failed";
  }
  const char* ret = NULL;
  for (size_t i = 0; i < num_names; i++) {
    bool dup = false;
    for (size_t j = 0; j < i; j++) {
      if (!strcmp(names[i], names[j])) {
        dup = true;
        break;
      }
    }
    if (dup) {
      continue;
    }
    wuffs_base__io_buffer b = ((wuffs_base__io_buffer){
        .data = wuffs_base__make_slice_u8(buf, 1 << 20),
    });
    ret = read_file(&b, names[i]);
    if (!ret) {
      ret = f(names[i], b.data.ptr, b.meta.wi);
    }
    if (ret) {
      break;
    }
  }
  free(buf);
  return ret;
}

// brotli_enc_synthetic makes one of the synthetic test inputs: 0 is 1 MiB of
// zeroes, 1 is 256 KiB of pseudo-random bytes and 2 is a 100-byte pattern
// repeated to 64 KiB. It returns a malloc'ed buffer.
uint8_t*  //
brotli_enc_synthetic(int which, size_t* len) {
  size_t n = (which == 0) ? (1 << 20) : (which == 1) ? (256 << 10) : (64 << 10);
  uint8_t* p = malloc(n);
  if (!p) {
    return NULL;
  }
  if (which == 0) {
    memset(p, 0, n);
  } else if (which == 1) {
    brotli_enc_fill_random(p, n, 0x1234);
  } else {
    uint8_t pattern[100];
    brotli_enc_fill_random(pattern, 100, 0x5678);
    for (size_t i = 0; i < n; i++) {
      p[i] = pattern[i % 100];
    }
  }
  *len = n;
  return p;
}

brotli_enc_config  //
make_brotli_enc_config(uint32_t quality, uint32_t lgwin) {
  brotli_enc_config c;
  memset(&c, 0, sizeof c);
  brotli_enc_config__add_param(&c, BROTLI_ENC_PARAM_QUALITY, quality);
  brotli_enc_config__add_param(&c, BROTLI_ENC_PARAM_LGWIN, lgwin);
  return c;
}

const char*  //
do_test_brotli_encode_round_trip_file(const char* name,
                                      const uint8_t* data,
                                      size_t len) {
  for (uint32_t q = 0; q <= 1; q++) {
    for (int k = 0; k < 3; k++) {
      brotli_enc_config c = make_brotli_enc_config(q, (k == 0) ? 10 : 22);
      if (k == 2) {
        c.chunking = BROTLI_ENC_CHUNK_RANDOM;
        c.flush = BROTLI_ENC_FLUSH_MIDDLE;
        c.dst_chunk = 97;
        c.seed = len;
      }
      char s[256];
      snprintf(s, sizeof s, "%s (q=%u, k=%d)", name, q, k);
      CHECK_STRING(do_test_brotli_encode_round_trip(s, &c, data, len));
    }
  }
  return NULL;
}

const char*  //
test_wuffs_brotli_encode_round_trip_corpus() {
  CHECK_FOCUS(__func__);
  return for_each_brotli_enc_corpus_file(do_test_brotli_encode_round_trip_file);
}

const char*  //
test_wuffs_brotli_encode_round_trip_lengths() {
  CHECK_FOCUS(__func__);
  uint8_t data[40];
  brotli_enc_fill_random(data, 20, 0x9999);
  memcpy(data + 20, data, 20);
  for (size_t n = 0; n <= 40; n++) {
    char s[64];
    snprintf(s, sizeof s, "length %zu", n);
    CHECK_STRING(do_test_brotli_encode_round_trip_file(s, data, n));
  }
  return NULL;
}

const char*  //
test_wuffs_brotli_encode_round_trip_synthetic() {
  CHECK_FOCUS(__func__);
  for (int which = 0; which < 3; which++) {
    size_t len = 0;
    uint8_t* data = brotli_enc_synthetic(which, &len);
    if (!data) {
      RETURN_FAIL("malloc failed");
    }
    char s[64];
    snprintf(s, sizeof s, "synthetic #%d", which);
    const char* status = do_test_brotli_encode_round_trip_file(s, data, len);
    free(data);
    if (status) {
      return status;
    }
  }
  return NULL;
}

const char*  //
test_wuffs_brotli_encode_metadata_round_trip() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer data = ((wuffs_base__io_buffer){
      .data = g_mimiclib_scratch_slice_u8,
  });
  CHECK_STRING(read_file(&data, "test/data/romeo.txt"));
  static const size_t lens[] = {0, 1, 100, 70000};
  for (uint32_t q = 0; q <= 1; q++) {
    for (size_t i = 0; i < WUFFS_TESTLIB_ARRAY_SIZE(lens); i++) {
      for (uint32_t pos = 1; pos <= 3; pos++) {
        brotli_enc_config c = make_brotli_enc_config(q, 18);
        c.chunking = BROTLI_ENC_CHUNK_7;
        c.metadata_position = pos;
        c.metadata_len = lens[i];
        c.dst_chunk = (pos == 2) ? 3 : 0;
        char s[64];
        snprintf(s, sizeof s, "q=%u, len=%zu, pos=%u", q, lens[i], pos);
        CHECK_STRING(do_test_brotli_encode_round_trip(s, &c, data.data.ptr,
                                                      data.meta.wi));
      }
    }
  }
  return NULL;
}

const char*  //
test_wuffs_brotli_encode_parameters() {
  CHECK_FOCUS(__func__);
  wuffs_brotli__encoder* enc = wuffs_brotli__encoder__alloc();
  if (!enc) {
    RETURN_FAIL("alloc failed");
  }
  const char* ret = NULL;
  wuffs_base__status status;
  uint8_t buf[64];
  wuffs_base__io_buffer dst =
      wuffs_base__ptr_u8__writer(buf, sizeof buf);
  wuffs_base__io_buffer src = make_io_buffer_from_string("", 0);

#define BROTLI_ENC_CHECK(cond, msg) \
  if (!(cond)) {                    \
    ret = msg;                      \
    goto cleanup;                   \
  }

  // Unknown keys and out-of-range values.
  status = wuffs_brotli__encoder__set_quirk(
      enc, WUFFS_BROTLI__QUIRK_STREAM_OFFSET + 1, 0);
  BROTLI_ENC_CHECK(status.repr == wuffs_base__error__unsupported_option,
                   "unknown key");
  status = wuffs_brotli__encoder__set_quirk(enc, 12345, 0);
  BROTLI_ENC_CHECK(status.repr == wuffs_base__error__unsupported_option,
                   "unknown key (2)");
  status = wuffs_brotli__encoder__set_quirk(
      enc, WUFFS_BROTLI__QUIRK_ENCODER_QUALITY, 0x100000000ull);
  BROTLI_ENC_CHECK(status.repr == wuffs_base__error__bad_argument,
                   "value > 0xFFFFFFFF");
  status = wuffs_brotli__encoder__set_quirk(
      enc, WUFFS_BROTLI__QUIRK_DISABLE_LITERAL_CONTEXT_MODELING, 2);
  BROTLI_ENC_CHECK(status.repr == wuffs_base__error__bad_argument,
                   "disable_lcm > 1");
  status = wuffs_brotli__encoder__set_quirk(
      enc, WUFFS_BROTLI__QUIRK_STREAM_OFFSET, (1 << 30) + 1);
  BROTLI_ENC_CHECK(status.repr == wuffs_base__error__bad_argument,
                   "stream_offset > 1<<30");
  status = wuffs_brotli__encoder__request_emit_metadata(enc, (1 << 24) + 1);
  BROTLI_ENC_CHECK(status.repr == wuffs_base__error__bad_argument,
                   "metadata length > 1<<24");

  // Raw values are stored as is, and sanitized later.
  status = wuffs_brotli__encoder__set_quirk(
      enc, WUFFS_BROTLI__QUIRK_LGWIN, 99);
  BROTLI_ENC_CHECK(!status.repr, "set lgwin");
  BROTLI_ENC_CHECK(wuffs_brotli__encoder__get_quirk(
                       enc, WUFFS_BROTLI__QUIRK_LGWIN) == 99,
                   "get lgwin");
  // The default quality is 11, which is unsupported.
  BROTLI_ENC_CHECK(wuffs_brotli__encoder__workbuf_len(enc).max_incl == 0,
                   "workbuf_len at q11");
  status = wuffs_brotli__encoder__set_quirk(
      enc, WUFFS_BROTLI__QUIRK_ENCODER_QUALITY, 0xFFFFFFFF);  // (int)-1.
  BROTLI_ENC_CHECK(!status.repr, "set quality");
  // Quality -1 is sanitized to 0 and lgwin 99 to 24.
  BROTLI_ENC_CHECK(wuffs_brotli__encoder__workbuf_len(enc).max_incl ==
                       ((2u << 24) + 503 + 16),
                   "workbuf_len at q0w24");
  status = wuffs_brotli__encoder__set_quirk(
      enc, WUFFS_BROTLI__QUIRK_LGWIN, 0xFFFFFFFF);  // (int)-1.
  BROTLI_ENC_CHECK(!status.repr, "set lgwin (2)");
  BROTLI_ENC_CHECK(wuffs_brotli__encoder__workbuf_len(enc).max_incl ==
                       ((2u << 10) + 503 + 16),
                   "workbuf_len at q0w10");

  // max_compressed_size mirrors BrotliEncoderMaxCompressedSize.
  BROTLI_ENC_CHECK(wuffs_brotli__encoder__max_compressed_size(enc, 0) == 2,
                   "max_compressed_size(0)");
  BROTLI_ENC_CHECK(
      wuffs_brotli__encoder__max_compressed_size(enc, 1 << 20) ==
          ((1 << 20) + 6 + (4 * 64)),
      "max_compressed_size(1<<20)");
  BROTLI_ENC_CHECK(
      wuffs_brotli__encoder__max_compressed_size(enc, UINT64_MAX) == 0,
      "max_compressed_size(overflow)");

  // A too-short workbuf.
  src.meta.closed = false;
  status = wuffs_brotli__encoder__transform_io(
      enc, &dst, &src, wuffs_base__make_slice_u8(g_work_slice_u8.ptr, 10));
  BROTLI_ENC_CHECK(status.repr == wuffs_base__suspension__short_read,
                   "empty input");

  // Parameters can't be changed after the first transform_io call.
  status = wuffs_brotli__encoder__set_quirk(
      enc, WUFFS_BROTLI__QUIRK_LGWIN, 16);
  BROTLI_ENC_CHECK(status.repr == wuffs_base__error__bad_call_sequence,
                   "set_quirk after transform_io");

  src.data.ptr = (uint8_t*)"abc";
  src.data.len = 3;
  src.meta.wi = 3;
  status = wuffs_brotli__encoder__transform_io(
      enc, &dst, &src, wuffs_base__make_slice_u8(g_work_slice_u8.ptr, 10));
  BROTLI_ENC_CHECK(status.repr == wuffs_base__error__bad_workbuf_length,
                   "short workbuf");

  // Qualities 2 to 11 are unsupported.
  for (uint32_t q = 2; q <= 12; q++) {
    free(enc);
    enc = wuffs_brotli__encoder__alloc();
    if (!enc) {
      RETURN_FAIL("alloc failed");
    }
    wuffs_brotli__encoder__set_quirk(enc, WUFFS_BROTLI__QUIRK_ENCODER_QUALITY,
                                     q);
    status = wuffs_brotli__encoder__transform_io(enc, &dst, &src,
                                                 g_work_slice_u8);
    BROTLI_ENC_CHECK(
        status.repr == wuffs_brotli__error__unsupported_encoder_quality,
        "quality 2..11");
  }

  // Requests.
  free(enc);
  enc = wuffs_brotli__encoder__alloc();
  if (!enc) {
    RETURN_FAIL("alloc failed");
  }
  status = wuffs_brotli__encoder__request_emit_metadata(enc, 1);
  BROTLI_ENC_CHECK(!status.repr, "request_emit_metadata");
  status = wuffs_brotli__encoder__request_flush(enc);
  BROTLI_ENC_CHECK(status.repr == wuffs_base__error__bad_call_sequence,
                   "request_flush while metadata is pending");
  status = wuffs_brotli__encoder__request_emit_metadata(enc, 1);
  BROTLI_ENC_CHECK(status.repr == wuffs_base__error__bad_call_sequence,
                   "request_emit_metadata twice");

  // A closed src with too few metadata bytes.
  wuffs_brotli__encoder__set_quirk(enc, WUFFS_BROTLI__QUIRK_ENCODER_QUALITY, 1);
  src = make_io_buffer_from_string("", 0);
  status = wuffs_brotli__encoder__transform_io(enc, &dst, &src,
                                               g_work_slice_u8);
  BROTLI_ENC_CHECK(status.repr == wuffs_brotli__error__truncated_input,
                   "truncated metadata");

#undef BROTLI_ENC_CHECK

cleanup:
  free(enc);
  if (ret) {
    RETURN_FAIL("%s: status \"%s\"", ret, status.repr);
  }
  return NULL;
}

const char*  //
test_wuffs_brotli_encode_interface() {
  CHECK_FOCUS(__func__);
  wuffs_brotli__encoder* enc = wuffs_brotli__encoder__alloc();
  if (!enc) {
    RETURN_FAIL("alloc failed");
  }
  wuffs_brotli__encoder__set_quirk(enc, WUFFS_BROTLI__QUIRK_ENCODER_QUALITY, 1);
  // Encoding romeo.txt at q1w22 gives 575 bytes, the last of which is 0xC3.
  const char* status = do_test__wuffs_base__io_transformer(
      wuffs_brotli__encoder__upcast_as__wuffs_base__io_transformer(enc),
      "test/data/romeo.txt", 0, SIZE_MAX, 575, 0xC3);
  free(enc);
  return status;
}

// ---------------- Encoder Mimic Tests

#ifdef WUFFS_MIMIC

// do_test_mimic_brotli_encode checks that Wuffs and libbrotlienc produce
// exactly the same bytes for encoding data[:data_len] with the config c.
const char*  //
do_test_mimic_brotli_encode(const char* name,
                            const brotli_enc_config* c,
                            const uint8_t* data,
                            size_t data_len) {
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(make_brotli_enc_plan(&src, c, data, data_len));
  wuffs_base__io_buffer have = ((wuffs_base__io_buffer){
      .data = g_have_slice_u8,
  });
  wuffs_base__io_buffer want = ((wuffs_base__io_buffer){
      .data = g_want_slice_u8,
  });

  wuffs_base__io_buffer src0 = src;
  const char* have_status = wuffs_brotli_encode_plan(
      &have, &src0, c->param_keys, c->param_values, c->num_params,
      g_brotli_enc_step_ops, g_brotli_enc_step_lens, g_brotli_enc_num_steps,
      c->dst_chunk);
  if (have_status) {
    RETURN_FAIL("%s: wuffs: %s", name, have_status);
  }
  wuffs_base__io_buffer src1 = src;
  const char* want_status = mimic_brotli_encode(
      &want, &src1, c->param_keys, c->param_values, c->num_params,
      g_brotli_enc_step_ops, g_brotli_enc_step_lens, g_brotli_enc_num_steps,
      c->dst_chunk);
  if (want_status) {
    RETURN_FAIL("%s: mimic: %s", name, want_status);
  }
  const char* status = check_io_buffers_equal("", &have, &want);
  if (status) {
    RETURN_FAIL("%s: %s", name, status);
  }
  return NULL;
}

const char* brotli_enc_parity_filenames[] = {
    "test/data/256.bytes",
    "test/data/romeo.txt",
    "test/data/brotli/alice29.txt",
    "test/data/brotli/random_org_10k.bin",
};

const char*  //
test_mimic_brotli_encode_parity_grid() {
  CHECK_FOCUS(__func__);
  static const uint32_t lgwins[] = {10, 16, 18, 22, 24};
  wuffs_base__io_buffer data = ((wuffs_base__io_buffer){
      .data = g_mimiclib_scratch_slice_u8,
  });
  for (size_t f = 0; f < WUFFS_TESTLIB_ARRAY_SIZE(brotli_enc_parity_filenames);
       f++) {
    data.meta.ri = 0;
    data.meta.wi = 0;
    data.meta.closed = false;
    CHECK_STRING(read_file(&data, brotli_enc_parity_filenames[f]));
    for (uint32_t q = 0; q <= 1; q++) {
      for (size_t w = 0; w < WUFFS_TESTLIB_ARRAY_SIZE(lgwins); w++) {
        for (uint32_t mode = 0; mode <= 2; mode++) {
          for (uint32_t ch = 0; ch < BROTLI_ENC_NUM_CHUNKINGS; ch++) {
            if ((ch == BROTLI_ENC_CHUNK_1) && (data.meta.wi > 4096)) {
              continue;
            }
            for (uint32_t fl = 0; fl < BROTLI_ENC_NUM_FLUSHES; fl++) {
              brotli_enc_config c = make_brotli_enc_config(q, lgwins[w]);
              brotli_enc_config__add_param(&c, BROTLI_ENC_PARAM_MODE, mode);
              c.chunking = ch;
              c.flush = fl;
              c.seed = (f << 16) | (w << 8) | (mode << 4) | fl;
              // To keep this test fast, the 7-byte chunking only uses the
              // first 4 KiB of the input. The
              // script/manual-test-brotli-encoder-parity.c program runs
              // the whole input.
              size_t n = data.meta.wi;
              if ((ch == BROTLI_ENC_CHUNK_7) && (n > 4096)) {
                n = 4096;
              }
              char s[256];
              snprintf(s, sizeof s,
                       "%s: q=%u lgwin=%u mode=%u chunking=%u flush=%u",
                       brotli_enc_parity_filenames[f], q, lgwins[w], mode, ch,
                       fl);
              CHECK_STRING(
                  do_test_mimic_brotli_encode(s, &c, data.data.ptr, n));
            }
          }
        }
      }
    }
  }
  return NULL;
}

const char*  //
test_mimic_brotli_encode_parity_metadata() {
  CHECK_FOCUS(__func__);
  static const size_t lens[] = {0, 1, 100, 70000};
  wuffs_base__io_buffer data = ((wuffs_base__io_buffer){
      .data = g_mimiclib_scratch_slice_u8,
  });
  CHECK_STRING(read_file(&data, "test/data/romeo.txt"));
  for (uint32_t q = 0; q <= 1; q++) {
    for (size_t i = 0; i < WUFFS_TESTLIB_ARRAY_SIZE(lens); i++) {
      for (uint32_t pos = 1; pos <= 3; pos++) {
        for (uint32_t fl = 0; fl < BROTLI_ENC_NUM_FLUSHES; fl++) {
          for (size_t dc = 0; dc < 2; dc++) {
            brotli_enc_config c = make_brotli_enc_config(q, 16);
            c.chunking = BROTLI_ENC_CHUNK_RANDOM;
            c.flush = fl;
            c.metadata_position = pos;
            c.metadata_len = lens[i];
            c.dst_chunk = dc ? 5 : 0;
            c.seed = (i << 8) | pos;
            char s[256];
            snprintf(s, sizeof s,
                     "q=%u metadata_len=%zu pos=%u flush=%u dst_chunk=%zu", q,
                     lens[i], pos, fl, c.dst_chunk);
            CHECK_STRING(do_test_mimic_brotli_encode(s, &c, data.data.ptr,
                                                     data.meta.wi));
          }
        }
      }
    }
  }
  return NULL;
}

const char*  //
test_mimic_brotli_encode_parity_params() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer data = ((wuffs_base__io_buffer){
      .data = g_mimiclib_scratch_slice_u8,
  });
  CHECK_STRING(read_file(&data, "test/data/brotli/alice29.txt"));
  uint32_t n = (uint32_t)data.meta.wi;
  static const struct {
    uint32_t key;
    uint32_t value;
  } extras[] = {
      {BROTLI_ENC_PARAM_SIZE_HINT, 0},
      {BROTLI_ENC_PARAM_SIZE_HINT, 0xFFFFFFFF},  // Replaced by the exact size.
      {BROTLI_ENC_PARAM_SIZE_HINT, 1u << 30},
      {BROTLI_ENC_PARAM_LGBLOCK, 16},
      {BROTLI_ENC_PARAM_LGBLOCK, 24},
      {BROTLI_ENC_PARAM_DISABLE_LITERAL_CONTEXT_MODELING, 1},
      {BROTLI_ENC_PARAM_LARGE_WINDOW, 1},
      {BROTLI_ENC_PARAM_NPOSTFIX, 2},
      {BROTLI_ENC_PARAM_NDIRECT, 12},
      {BROTLI_ENC_PARAM_STREAM_OFFSET, 1},
      {BROTLI_ENC_PARAM_STREAM_OFFSET, 1000},
      {BROTLI_ENC_PARAM_STREAM_OFFSET, 1u << 30},
      {BROTLI_ENC_PARAM_LGWIN, 0xFFFFFFFF},  // (int)-1.
      {BROTLI_ENC_PARAM_LGWIN, 30},
      {BROTLI_ENC_PARAM_QUALITY, 0xFFFFFFFF},  // (int)-1, so quality 0.
  };
  for (uint32_t q = 0; q <= 1; q++) {
    for (size_t i = 0; i < WUFFS_TESTLIB_ARRAY_SIZE(extras); i++) {
      for (uint32_t ch = 0; ch < BROTLI_ENC_NUM_CHUNKINGS; ch += 4) {
        brotli_enc_config c = make_brotli_enc_config(q, 22);
        uint32_t v = extras[i].value;
        if ((extras[i].key == BROTLI_ENC_PARAM_SIZE_HINT) &&
            (v == 0xFFFFFFFF)) {
          v = n;
        }
        brotli_enc_config__add_param(&c, extras[i].key, v);
        c.chunking = ch;
        c.flush = BROTLI_ENC_FLUSH_MIDDLE;
        c.seed = i;
        char s[256];
        snprintf(s, sizeof s, "q=%u param[%u]=%u chunking=%u", q,
                 extras[i].key, v, ch);
        CHECK_STRING(
            do_test_mimic_brotli_encode(s, &c, data.data.ptr, data.meta.wi));
      }
    }
  }
  return NULL;
}

const char*  //
do_test_mimic_brotli_encode_file(const char* name,
                                 const uint8_t* data,
                                 size_t len) {
  for (uint32_t q = 0; q <= 1; q++) {
    for (uint32_t w = 10; w <= 24; w += 14) {
      brotli_enc_config c = make_brotli_enc_config(q, w);
      c.chunking = (w == 10) ? BROTLI_ENC_CHUNK_SINGLE : BROTLI_ENC_CHUNK_RANDOM;
      c.flush = (w == 10) ? BROTLI_ENC_FLUSH_NONE : BROTLI_ENC_FLUSH_MIDDLE;
      c.seed = len;
      char s[256];
      snprintf(s, sizeof s, "%s: q=%u lgwin=%u", name, q, w);
      CHECK_STRING(do_test_mimic_brotli_encode(s, &c, data, len));
    }
  }
  return NULL;
}

const char*  //
test_mimic_brotli_encode_parity_corpus() {
  CHECK_FOCUS(__func__);
  CHECK_STRING(
      for_each_brotli_enc_corpus_file(do_test_mimic_brotli_encode_file));
  uint8_t data[40];
  brotli_enc_fill_random(data, 20, 0x9999);
  memcpy(data + 20, data, 20);
  for (size_t n = 0; n <= 40; n++) {
    char s[64];
    snprintf(s, sizeof s, "length %zu", n);
    CHECK_STRING(do_test_mimic_brotli_encode_file(s, data, n));
  }
  for (int which = 0; which < 3; which++) {
    size_t len = 0;
    uint8_t* p = brotli_enc_synthetic(which, &len);
    if (!p) {
      RETURN_FAIL("malloc failed");
    }
    char s[64];
    snprintf(s, sizeof s, "synthetic #%d", which);
    const char* status = do_test_mimic_brotli_encode_file(s, p, len);
    free(p);
    if (status) {
      return status;
    }
  }
  return NULL;
}

#endif  // WUFFS_MIMIC

// ---------------- Encoder Benches

const char*  //
do_bench_brotli_encode(const char* (*codec_func)(wuffs_base__io_buffer*,
                                                 wuffs_base__io_buffer*,
                                                 const uint32_t*,
                                                 const uint32_t*,
                                                 size_t,
                                                 const uint32_t*,
                                                 const size_t*,
                                                 size_t,
                                                 size_t),
                       uint32_t quality,
                       uint64_t iters_unscaled) {
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(read_file(&src, "test/data/brotli/alice29.txt"));
  uint32_t keys[2] = {BROTLI_ENC_PARAM_QUALITY, BROTLI_ENC_PARAM_LGWIN};
  uint32_t values[2] = {quality, 22};
  uint32_t op = BROTLI_ENC_OP_FINISH;
  size_t len = src.meta.wi;

  bench_start();
  uint64_t n_bytes = 0;
  uint64_t i;
  uint64_t iters = iters_unscaled * g_flags.iterscale;
  for (i = 0; i < iters; i++) {
    wuffs_base__io_buffer have = ((wuffs_base__io_buffer){
        .data = g_have_slice_u8,
    });
    src.meta.ri = 0;
    CHECK_STRING(codec_func(&have, &src, keys, values, 2, &op, &len, 1, 0));
    n_bytes += len;
  }
  bench_finish(iters, n_bytes);
  return NULL;
}

const char*  //
bench_wuffs_brotli_encode_alice29_q0() {
  CHECK_FOCUS(__func__);
  return do_bench_brotli_encode(wuffs_brotli_encode_plan, 0, 30);
}

const char*  //
bench_wuffs_brotli_encode_alice29_q1() {
  CHECK_FOCUS(__func__);
  return do_bench_brotli_encode(wuffs_brotli_encode_plan, 1, 30);
}

// ---------------- Encoder Mimic Benches

#ifdef WUFFS_MIMIC

const char*  //
bench_mimic_brotli_encode_alice29_q0() {
  CHECK_FOCUS(__func__);
  return do_bench_brotli_encode(mimic_brotli_encode, 0, 30);
}

const char*  //
bench_mimic_brotli_encode_alice29_q1() {
  CHECK_FOCUS(__func__);
  return do_bench_brotli_encode(mimic_brotli_encode, 1, 30);
}

#endif  // WUFFS_MIMIC

// ---------------- Brotli Benches

const char*  //
bench_wuffs_brotli_decode_alice29() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(
      wuffs_brotli_decode,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED, tcounter_dst,
      &g_brotli_alice29_gt, UINT64_MAX, UINT64_MAX, 5);
}

// ---------------- Mimic Benches

#ifdef WUFFS_MIMIC

const char*  //
bench_mimic_brotli_decode_alice29() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(
      mimic_brotli_decode,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED, tcounter_dst,
      &g_brotli_alice29_gt, UINT64_MAX, UINT64_MAX, 5);
}

#endif  // WUFFS_MIMIC

// ---------------- Manifest

proc g_tests[] = {

    test_wuffs_brotli_decode_alice29,
    test_wuffs_brotli_decode_alice29_q11w10,
    test_wuffs_brotli_decode_all,
    test_wuffs_brotli_decode_all_with_io_limits,
    test_wuffs_brotli_decode_interface,
    test_wuffs_brotli_decode_malformed,
    test_wuffs_brotli_decode_romeo,
    test_wuffs_brotli_decode_trailing_data,
    test_wuffs_brotli_decode_truncated,
    test_wuffs_brotli_decode_workbuf_length,

#ifdef WUFFS_MIMIC

    test_mimic_brotli_decode_alice29,
    test_mimic_brotli_decode_all,
    test_mimic_brotli_decode_differential,
    test_mimic_brotli_decode_malformed,
    test_mimic_brotli_decode_romeo,

#endif  // WUFFS_MIMIC

    // Encoder tests.

    test_wuffs_brotli_encode_interface,
    test_wuffs_brotli_encode_metadata_round_trip,
    test_wuffs_brotli_encode_parameters,
    test_wuffs_brotli_encode_round_trip_corpus,
    test_wuffs_brotli_encode_round_trip_lengths,
    test_wuffs_brotli_encode_round_trip_synthetic,

#ifdef WUFFS_MIMIC

    test_mimic_brotli_encode_parity_corpus,
    test_mimic_brotli_encode_parity_grid,
    test_mimic_brotli_encode_parity_metadata,
    test_mimic_brotli_encode_parity_params,

#endif  // WUFFS_MIMIC

    NULL,
};

proc g_benches[] = {

    bench_wuffs_brotli_decode_alice29,

#ifdef WUFFS_MIMIC

    bench_mimic_brotli_decode_alice29,

#endif  // WUFFS_MIMIC

    // Encoder benches.

    bench_wuffs_brotli_encode_alice29_q0,
    bench_wuffs_brotli_encode_alice29_q1,

#ifdef WUFFS_MIMIC

    bench_mimic_brotli_encode_alice29_q0,
    bench_mimic_brotli_encode_alice29_q1,

#endif  // WUFFS_MIMIC

    NULL,
};

int  //
main(int argc, char** argv) {
  g_proc_package_name = "std/brotli";
  return test_main(argc, argv, g_tests, g_benches);
}
