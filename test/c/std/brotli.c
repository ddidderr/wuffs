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
      {wuffs_brotli__error__bad_compound_dictionary, -18},
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

// g_brotli_split_block_length is an invalid stream (found by differential
// fuzzing) whose last metablock is overrun. In one piece, libbrotlidec
// reports BLOCK_LENGTH_2 (and Wuffs "#bad metablock length"). Split after
// bytes 24, 25 and 30, libbrotlidec reports BLOCK_LENGTH_1, because of its
// internal input buffer, while Wuffs still reports "#bad metablock length".
// This is a documented difference (see "Status Mapping" in
// std/brotli/README.md).
static const uint8_t g_brotli_split_block_length[36] = {
    0x09, 0x83, 0x04, 0x00, 0x00, 0x36, 0xE0, 0x28, 0x0A, 0x61, 0xD0, 0x09,
    0x07, 0x93, 0xE3, 0xB6, 0x38, 0xE4, 0x02, 0x00, 0xB0, 0xDC, 0x1A, 0xA5,
    0x6F, 0x3F, 0x00, 0x20, 0x00, 0xE2, 0xB0, 0x00, 0x6A, 0xDD, 0x07, 0x00,
};

const char*  //
test_mimic_brotli_decode_split_block_length() {
  CHECK_FOCUS(__func__);
  static const size_t ends[2][4] = {{36, 0, 0, 0}, {24, 25, 30, 36}};
  static const int want_c[2] = {-10, -9};
  for (int split = 0; split < 2; split++) {
    // libbrotlidec.
    BrotliDecoderState* state = BrotliDecoderCreateInstance(NULL, NULL, NULL);
    if (!state) {
      RETURN_FAIL("BrotliDecoderCreateInstance failed");
    }
    size_t pos = 0;
    size_t total_out = 0;
    for (int k = 0; (k < 4) && ends[split][k]; k++) {
      size_t avail_in = ends[split][k] - pos;
      const uint8_t* next_in = g_brotli_split_block_length + pos;
      size_t avail_out = g_have_slice_u8.len - total_out;
      uint8_t* next_out = g_have_slice_u8.ptr + total_out;
      BrotliDecoderResult r = BrotliDecoderDecompressStream(
          state, &avail_in, &next_in, &avail_out, &next_out, NULL);
      pos = ends[split][k] - avail_in;
      total_out = g_have_slice_u8.len - avail_out;
      if (r != BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT) {
        break;
      }
    }
    int code = (int)BrotliDecoderGetErrorCode(state);
    BrotliDecoderDestroyInstance(state);
    if (code != want_c[split]) {
      RETURN_FAIL("split=%d: libbrotlidec error code: have %d, want %d", split,
                  code, want_c[split]);
    }

    // Wuffs.
    wuffs_brotli__decoder dec;
    CHECK_STATUS("initialize",
                 wuffs_brotli__decoder__initialize(
                     &dec, sizeof dec, WUFFS_VERSION,
                     WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
    wuffs_base__io_buffer dst = ((wuffs_base__io_buffer){
        .data = g_have_slice_u8,
    });
    wuffs_base__status status = wuffs_base__make_status(NULL);
    pos = 0;
    for (int k = 0; (k < 4) && ends[split][k]; k++) {
      wuffs_base__io_buffer src = wuffs_base__ptr_u8__reader(
          (uint8_t*)g_brotli_split_block_length, ends[split][k],
          ends[split][k] == 36);
      src.meta.ri = pos;
      status = wuffs_brotli__decoder__transform_io(&dec, &dst, &src,
                                                   g_work_slice_u8);
      pos = src.meta.ri;
      if (status.repr != wuffs_base__suspension__short_read) {
        break;
      }
    }
    if (status.repr != wuffs_brotli__error__bad_metablock_length) {
      RETURN_FAIL("split=%d: Wuffs status: have \"%s\", want \"%s\"", split,
                  status.repr, wuffs_brotli__error__bad_metablock_length);
    }
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

const char*  //
wuffs_brotli_encode_plan_with_dictionaries(wuffs_base__io_buffer* dst,
                                           wuffs_base__io_buffer* src,
                                           const uint32_t* param_keys,
                                           const uint32_t* param_values,
                                           size_t num_params,
                                           size_t num_dicts,
                                           const uint32_t* dict_types,
                                           const wuffs_base__slice_u8* dicts,
                                           const uint32_t* step_ops,
                                           const size_t* step_lens,
                                           size_t num_steps,
                                           size_t dst_chunk);

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
  return wuffs_brotli_encode_plan_with_dictionaries(
      dst, src, param_keys, param_values, num_params, 0, NULL, NULL, step_ops,
      step_lens, num_steps, dst_chunk);
}

// wuffs_brotli_encode_plan_with_dictionaries is the Wuffs equivalent of
// mimic_brotli_encode_with_dictionaries: it adds (with add_dictionary) the
// num_dicts dictionaries after setting the quirks and before the first step.
// Each dict_types[i] is WUFFS_BROTLI__DICTIONARY_TYPE_RAW or
// WUFFS_BROTLI__DICTIONARY_TYPE_SERIALIZED.
const char*  //
wuffs_brotli_encode_plan_with_dictionaries(wuffs_base__io_buffer* dst,
                                           wuffs_base__io_buffer* src,
                                           const uint32_t* param_keys,
                                           const uint32_t* param_values,
                                           size_t num_params,
                                           size_t num_dicts,
                                           const uint32_t* dict_types,
                                           const wuffs_base__slice_u8* dicts,
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
  // At qualities 2 and above, the workbuf can be longer than
  // g_work_slice_u8 (e.g. about 80 MiB at quality 9 and lgwin 22).
  uint8_t* big_workbuf = NULL;
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
  wuffs_base__slice_u8 workbuf = g_work_slice_u8;
  workbuf.len = 0;
  size_t big_workbuf_len = 0;
  for (size_t j = 0; j <= num_dicts; j++) {
    // Each add_dictionary call needs add_dictionary_workbuf_len bytes and
    // transform_io needs workbuf_len bytes, with the same dictionary bytes
    // at the start.
    uint64_t n = (j < num_dicts)
                     ? wuffs_brotli__encoder__add_dictionary_workbuf_len(
                           enc, dicts[j].len)
                     : wuffs_brotli__encoder__workbuf_len(enc).min_incl;
    // workbuf_len's minimum is used so that the qualities 10 and 11
    // practical bound (see the README) is exercised. At other qualities,
    // min_incl == max_incl. Large-window brotli at lgwin 30 needs more
    // than 2 GiB.
    if ((n > (3ull << 30)) || (n > SIZE_MAX)) {
      ret = "workbuf_len is too large";
      goto cleanup;
    } else if ((n > g_work_slice_u8.len) && (n > big_workbuf_len)) {
      uint8_t* p = realloc(big_workbuf, (size_t)n);
      if (!p) {
        ret = "malloc failed";
        goto cleanup;
      }
      if (!big_workbuf && workbuf.len) {
        // Keep the dictionaries added so far.
        memcpy(p, g_work_slice_u8.ptr, workbuf.len);
      }
      big_workbuf = p;
      big_workbuf_len = (size_t)n;
      workbuf.ptr = big_workbuf;
    }
    workbuf.len = (size_t)n;
    if (j < num_dicts) {
      wuffs_base__status status = wuffs_brotli__encoder__add_dictionary(
          enc, dict_types[j], dicts[j], workbuf);
      if (status.repr) {
        ret = status.repr;
        goto cleanup;
      }
    }
  }

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
  free(big_workbuf);
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
    "test/data/archive.iso",
    "test/data/enwik5",
    "test/data/nobel-prizes.json",
    "test/data/pi.txt",
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
  for (uint32_t q = 0; q <= 11; q++) {
    for (int k = (q <= 1) ? 0 : 1; k < 3; k++) {
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
  for (uint32_t q = 0; q <= 11; q++) {
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
  // The default quality is 11 (and lgwin 99 is sanitized to 24).
  BROTLI_ENC_CHECK(wuffs_brotli__encoder__workbuf_len(enc).min_incl > 0,
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

  // At qualities 2 to 9, workbuf_len depends on the parameters, and a
  // too-short workbuf is rejected.
  for (uint32_t q = 2; q <= 9; q++) {
    free(enc);
    enc = wuffs_brotli__encoder__alloc();
    if (!enc) {
      RETURN_FAIL("alloc failed");
    }
    wuffs_brotli__encoder__set_quirk(enc, WUFFS_BROTLI__QUIRK_ENCODER_QUALITY,
                                     q);
    wuffs_brotli__encoder__set_quirk(enc, WUFFS_BROTLI__QUIRK_LGWIN, 10);
    uint64_t wlen = wuffs_brotli__encoder__workbuf_len(enc).max_incl;
    BROTLI_ENC_CHECK((wlen > 0) && (wlen <= g_work_slice_u8.len),
                     "workbuf_len at quality 2..9");
    wuffs_brotli__encoder__set_quirk(enc, WUFFS_BROTLI__QUIRK_LGWIN, 24);
    BROTLI_ENC_CHECK(wuffs_brotli__encoder__workbuf_len(enc).max_incl > wlen,
                     "workbuf_len grows with lgwin");
    wuffs_brotli__encoder__set_quirk(enc, WUFFS_BROTLI__QUIRK_LGWIN, 10);
    src = make_io_buffer_from_string("abc", 3);
    dst.meta.wi = 0;
    status = wuffs_brotli__encoder__transform_io(
        enc, &dst, &src,
        wuffs_base__make_slice_u8(g_work_slice_u8.ptr, (size_t)(wlen / 2)));
    BROTLI_ENC_CHECK(status.repr == wuffs_base__error__bad_workbuf_length,
                     "short workbuf at quality 2..9");
  }
  src = make_io_buffer_from_string("", 0);
  dst.meta.wi = 0;

  // With the large-window quirk, lgwin can be up to 30 (instead of 24) at
  // qualities 3 and above. Like the C code, quality 2 and below ignore the
  // quirk. Above lgwin 24, the ring buffer is (2 << lgwin) bytes and, at
  // quality 3, the H35 hasher has a 64 MiB rolling hash table on top of
  // H3's 256 KiB.
  for (uint32_t q = 2; q <= 3; q++) {
    uint64_t wlens[3] = {0};
    for (int k = 0; k < 3; k++) {
      free(enc);
      enc = wuffs_brotli__encoder__alloc();
      if (!enc) {
        RETURN_FAIL("alloc failed");
      }
      wuffs_brotli__encoder__set_quirk(
          enc, WUFFS_BROTLI__QUIRK_ENCODER_QUALITY, q);
      wuffs_brotli__encoder__set_quirk(enc, WUFFS_BROTLI__QUIRK_LGWIN,
                                       (k == 0) ? 24 : 30);
      if (k > 0) {
        wuffs_brotli__encoder__set_quirk(
            enc, WUFFS_BROTLI__QUIRK_LARGE_WINDOW, (k == 2) ? 1 : 0);
      }
      wlens[k] = wuffs_brotli__encoder__workbuf_len(enc).max_incl;
    }
    BROTLI_ENC_CHECK(wlens[0] == wlens[1],
                     "lgwin 30 without large window is lgwin 24");
    if (q == 2) {
      BROTLI_ENC_CHECK(wlens[0] == wlens[2],
                       "large window is ignored at quality 2");
    } else {
      BROTLI_ENC_CHECK(
          wlens[2] == (wlens[0] + (2ull << 30) - (2ull << 24) + (64 << 20)),
          "workbuf_len at q3w30 with large window");
    }
  }

  // At qualities 2 to 9, the ring buffer is the workbuf's last region (of
  // (2 + (1 << rb_bits) + (1 << lgblock) + 7) bytes) and a shorter workbuf
  // works if it holds (2 + input_length + 16) bytes of it (and the input is
  // short enough for the ring buffer never to wrap).
  {
    free(enc);
    enc = wuffs_brotli__encoder__alloc();
    if (!enc) {
      RETURN_FAIL("alloc failed");
    }
    wuffs_brotli__encoder__set_quirk(enc, WUFFS_BROTLI__QUIRK_ENCODER_QUALITY,
                                     3);
    wuffs_brotli__encoder__set_quirk(enc, WUFFS_BROTLI__QUIRK_LGWIN, 30);
    wuffs_brotli__encoder__set_quirk(enc, WUFFS_BROTLI__QUIRK_LARGE_WINDOW, 1);
    uint64_t wlen = wuffs_brotli__encoder__workbuf_len(enc).max_incl;
    uint64_t rb_region = 2 + (2ull << 30) + (1 << 14) + 7;
    BROTLI_ENC_CHECK(wlen > rb_region, "workbuf_len at q3w30");
    size_t short_len = (size_t)(wlen - rb_region + 2 + 3 + 16);
    uint8_t* short_workbuf = malloc(short_len);
    if (!short_workbuf) {
      RETURN_FAIL("malloc failed");
    }
    for (int k = 0; k < 2; k++) {
      wuffs_brotli__encoder* e = wuffs_brotli__encoder__alloc();
      if (!e) {
        free(short_workbuf);
        RETURN_FAIL("alloc failed");
      }
      wuffs_brotli__encoder__set_quirk(e, WUFFS_BROTLI__QUIRK_ENCODER_QUALITY,
                                       3);
      wuffs_brotli__encoder__set_quirk(e, WUFFS_BROTLI__QUIRK_LGWIN, 30);
      wuffs_brotli__encoder__set_quirk(e, WUFFS_BROTLI__QUIRK_LARGE_WINDOW,
                                       1);
      wuffs_base__io_buffer s = make_io_buffer_from_string("abc", 3);
      s.meta.closed = true;
      dst.meta.wi = 0;
      status = wuffs_brotli__encoder__transform_io(
          e, &dst, &s,
          wuffs_base__make_slice_u8(short_workbuf, short_len - (k ? 0 : 1)));
      free(e);
      if (k == 0) {
        if (status.repr != wuffs_base__error__bad_workbuf_length) {
          free(short_workbuf);
          RETURN_FAIL("too short workbuf at q3w30: %s", status.repr);
        }
      } else if (status.repr) {
        free(short_workbuf);
        RETURN_FAIL("short workbuf at q3w30: %s", status.repr);
      }
    }
    free(short_workbuf);
  }

  // At qualities 10 and 11, workbuf_len is a range (see the README's
  // "Qualities 10 and 11 Workbuf Length" section).
  for (uint32_t q = 10; q <= 12; q++) {
    static const struct {
      uint32_t lgwin;
      uint64_t min_incl;
      uint64_t max_incl;
    } want[] = {
        {16, 96686024ull, 302507168ull},
        {22, 422539208ull, 5364573344ull},
        {24, 813133768ull, 10148177056ull},
    };
    for (size_t i = 0; i < WUFFS_TESTLIB_ARRAY_SIZE(want); i++) {
      free(enc);
      enc = wuffs_brotli__encoder__alloc();
      if (!enc) {
        RETURN_FAIL("alloc failed");
      }
      wuffs_brotli__encoder__set_quirk(
          enc, WUFFS_BROTLI__QUIRK_ENCODER_QUALITY, q);
      wuffs_brotli__encoder__set_quirk(enc, WUFFS_BROTLI__QUIRK_LGWIN,
                                       want[i].lgwin);
      wuffs_base__range_ii_u64 r = wuffs_brotli__encoder__workbuf_len(enc);
      if ((r.min_incl != want[i].min_incl) ||
          (r.max_incl != want[i].max_incl)) {
        RETURN_FAIL("workbuf_len at q=%u lgwin=%u: have %" PRIu64
                    "..%" PRIu64 ", want %" PRIu64 "..%" PRIu64,
                    q, want[i].lgwin, r.min_incl, r.max_incl,
                    want[i].min_incl, want[i].max_incl);
      }
    }
    // A workbuf that does not even reach the ring buffer is rejected.
    src = make_io_buffer_from_string("abc", 3);
    dst.meta.wi = 0;
    status = wuffs_brotli__encoder__transform_io(
        enc, &dst, &src, wuffs_base__make_slice_u8(g_work_slice_u8.ptr, 1000));
    BROTLI_ENC_CHECK(status.repr == wuffs_base__error__bad_workbuf_length,
                     "short workbuf at quality 10..11");
  }
  // Compound dictionaries at qualities 10 and 11. The want bytes are
  // libbrotlienc's output (with the dictionary prepared and attached, lgwin
  // 16 and BROTLI_OPERATION_FINISH), which is the same at both qualities.
  for (uint32_t q = 10; q <= 11; q++) {
    free(enc);
    enc = wuffs_brotli__encoder__alloc();
    if (!enc) {
      RETURN_FAIL("alloc failed");
    }
    wuffs_brotli__encoder__set_quirk(enc, WUFFS_BROTLI__QUIRK_ENCODER_QUALITY,
                                     q);
    wuffs_brotli__encoder__set_quirk(enc, WUFFS_BROTLI__QUIRK_LGWIN, 16);
    static const char dict_bytes[] = "a raw dictionary, for quality 11";
    static const char input[] = "for quality 10 or 11, a raw dictionary";
    static const uint8_t want[] = {
        0xA2, 0x04, 0x00, 0xBF, 0x34, 0x08, 0x0B, 0x4C, 0x8C, 0x8C,
        0x20, 0x18, 0x71, 0xEA, 0x81, 0x22, 0x4A, 0xEC, 0x4B,
    };
    wuffs_base__slice_u8 d = wuffs_base__make_slice_u8(
        (uint8_t*)dict_bytes, sizeof dict_bytes - 1);
    // The workbuf (whose prefix holds the prepared dictionary) is longer
    // than g_work_slice_u8, so it is malloc'ed and then realloc'ed.
    uint64_t n = wuffs_brotli__encoder__add_dictionary_workbuf_len(enc, d.len);
    uint8_t* p = malloc((size_t)n);
    BROTLI_ENC_CHECK(p, "malloc failed");
    status = wuffs_brotli__encoder__add_dictionary(
        enc, WUFFS_BROTLI__DICTIONARY_TYPE_RAW, d,
        wuffs_base__make_slice_u8(p, (size_t)n));
    if (status.repr) {
      free(p);
    }
    BROTLI_ENC_CHECK(!status.repr, "add_dictionary at quality 10..11");
    n = wuffs_brotli__encoder__workbuf_len(enc).min_incl;
    uint8_t* p2 = realloc(p, (size_t)n);
    if (!p2) {
      free(p);
    }
    BROTLI_ENC_CHECK(p2, "realloc failed");
    src = make_io_buffer_from_string(input, sizeof input - 1);
    src.meta.closed = true;
    dst.meta.wi = 0;
    status = wuffs_brotli__encoder__transform_io(
        enc, &dst, &src, wuffs_base__make_slice_u8(p2, (size_t)n));
    free(p2);
    BROTLI_ENC_CHECK(!status.repr, "dictionary at quality 10..11");
    BROTLI_ENC_CHECK((dst.meta.wi == sizeof want) &&
                         !memcmp(dst.data.ptr, want, sizeof want),
                     "dictionary at quality 10..11: output bytes");
  }
  src = make_io_buffer_from_string("", 0);
  dst.meta.wi = 0;
  src = make_io_buffer_from_string("", 0);
  dst.meta.wi = 0;

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

// test_mimic_brotli_encode_parity_q2_grid is like
// test_mimic_brotli_encode_parity_grid but for qualities 2 to 9, with fewer
// lgwins and chunkings (to keep it fast).
const char*  //
test_mimic_brotli_encode_parity_q2_grid() {
  CHECK_FOCUS(__func__);
  static const uint32_t lgwins[] = {10, 18, 24};
  static const uint32_t chunkings[] = {
      BROTLI_ENC_CHUNK_SINGLE,
      BROTLI_ENC_CHUNK_7,
      BROTLI_ENC_CHUNK_RANDOM,
  };
  wuffs_base__io_buffer data = ((wuffs_base__io_buffer){
      .data = g_mimiclib_scratch_slice_u8,
  });
  for (size_t f = 0; f < WUFFS_TESTLIB_ARRAY_SIZE(brotli_enc_parity_filenames);
       f++) {
    data.meta.ri = 0;
    data.meta.wi = 0;
    data.meta.closed = false;
    CHECK_STRING(read_file(&data, brotli_enc_parity_filenames[f]));
    for (uint32_t q = 2; q <= 9; q++) {
      for (size_t w = 0; w < WUFFS_TESTLIB_ARRAY_SIZE(lgwins); w++) {
        for (uint32_t mode = 0; mode <= 2; mode++) {
          for (size_t k = 0; k < WUFFS_TESTLIB_ARRAY_SIZE(chunkings); k++) {
            uint32_t ch = chunkings[k];
            for (uint32_t fl = 0; fl < BROTLI_ENC_NUM_FLUSHES; fl++) {
              brotli_enc_config c = make_brotli_enc_config(q, lgwins[w]);
              brotli_enc_config__add_param(&c, BROTLI_ENC_PARAM_MODE, mode);
              c.chunking = ch;
              c.flush = fl;
              c.seed = (f << 16) | (w << 8) | (mode << 4) | fl | (q << 20);
              // The chunked plans only use the first 4 KiB (7-byte chunks)
              // or 64 KiB (random chunks) of the input.
              size_t n = data.meta.wi;
              if ((ch == BROTLI_ENC_CHUNK_7) && (n > 4096)) {
                n = 4096;
              } else if ((ch == BROTLI_ENC_CHUNK_RANDOM) && (n > 65536)) {
                n = 65536;
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
test_mimic_brotli_encode_parity_q2_metadata() {
  CHECK_FOCUS(__func__);
  static const size_t lens[] = {0, 1, 100, 70000};
  wuffs_base__io_buffer data = ((wuffs_base__io_buffer){
      .data = g_mimiclib_scratch_slice_u8,
  });
  CHECK_STRING(read_file(&data, "test/data/romeo.txt"));
  for (uint32_t q = 2; q <= 9; q++) {
    for (size_t i = 0; i < WUFFS_TESTLIB_ARRAY_SIZE(lens); i++) {
      for (uint32_t pos = 1; pos <= 3; pos++) {
        for (uint32_t fl = 0; fl < BROTLI_ENC_NUM_FLUSHES; fl++) {
          for (size_t dc = 0; dc < 2; dc++) {
            brotli_enc_config c = make_brotli_enc_config(q, 16);
            c.chunking = dc ? BROTLI_ENC_CHUNK_RANDOM : BROTLI_ENC_CHUNK_7;
            c.flush = fl;
            c.metadata_position = pos;
            c.metadata_len = lens[i];
            c.dst_chunk = dc ? 5 : 0;
            c.seed = (i << 8) | pos | (q << 12);
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
test_mimic_brotli_encode_parity_q2_params() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer data = ((wuffs_base__io_buffer){
      .data = g_mimiclib_scratch_slice_u8,
  });
  CHECK_STRING(read_file(&data, "test/data/brotli/alice29.txt"));
  uint32_t n = (uint32_t)data.meta.wi;
  // Each row is up to three (BrotliEncoderParameter, value) pairs. A zero key
  // (other than in the first pair) ends the row.
  static const uint32_t extras[][6] = {
      {BROTLI_ENC_PARAM_SIZE_HINT, 0xFFFFFFFF},  // Replaced by the exact size.
      {BROTLI_ENC_PARAM_SIZE_HINT, 1},
      {BROTLI_ENC_PARAM_SIZE_HINT, 1u << 20},
      {BROTLI_ENC_PARAM_SIZE_HINT, 1u << 30},
      {BROTLI_ENC_PARAM_LGBLOCK, 16},
      {BROTLI_ENC_PARAM_LGBLOCK, 17},
      {BROTLI_ENC_PARAM_LGBLOCK, 20},
      {BROTLI_ENC_PARAM_LGBLOCK, 24},
      {BROTLI_ENC_PARAM_DISABLE_LITERAL_CONTEXT_MODELING, 1},
      {BROTLI_ENC_PARAM_DISABLE_LITERAL_CONTEXT_MODELING, 1,  //
       BROTLI_ENC_PARAM_MODE, 1},
      {BROTLI_ENC_PARAM_MODE, 1},
      {BROTLI_ENC_PARAM_MODE, 2},
      {BROTLI_ENC_PARAM_NPOSTFIX, 1},
      {BROTLI_ENC_PARAM_NPOSTFIX, 2, BROTLI_ENC_PARAM_NDIRECT, 12},
      {BROTLI_ENC_PARAM_NPOSTFIX, 3, BROTLI_ENC_PARAM_NDIRECT, 120},
      {BROTLI_ENC_PARAM_NPOSTFIX, 0, BROTLI_ENC_PARAM_NDIRECT, 15},
      {BROTLI_ENC_PARAM_MODE, 2, BROTLI_ENC_PARAM_NPOSTFIX, 3,
       BROTLI_ENC_PARAM_NDIRECT, 30},
      {BROTLI_ENC_PARAM_STREAM_OFFSET, 1},
      {BROTLI_ENC_PARAM_STREAM_OFFSET, 1000},
      {BROTLI_ENC_PARAM_LGWIN, 0xFFFFFFFF},  // (int)-1.
      {BROTLI_ENC_PARAM_SIZE_HINT, 1u << 21,  //
       BROTLI_ENC_PARAM_LGBLOCK, 18},
  };
  for (uint32_t q = 2; q <= 9; q++) {
    for (size_t i = 0; i < WUFFS_TESTLIB_ARRAY_SIZE(extras); i++) {
      for (uint32_t ch = 0; ch < BROTLI_ENC_NUM_CHUNKINGS; ch += 4) {
        brotli_enc_config c = make_brotli_enc_config(q, 22);
        char s[256];
        int k = snprintf(s, sizeof s, "q=%u chunking=%u", q, ch);
        for (size_t j = 0; (j < 6) && ((j == 0) || extras[i][j]); j += 2) {
          uint32_t v = extras[i][j + 1];
          if ((extras[i][j] == BROTLI_ENC_PARAM_SIZE_HINT) &&
              (v == 0xFFFFFFFF)) {
            v = n;
          }
          brotli_enc_config__add_param(&c, extras[i][j], v);
          if ((k > 0) && ((size_t)k < sizeof s)) {
            k += snprintf(s + k, sizeof s - (size_t)k, " param[%u]=%u",
                          extras[i][j], v);
          }
        }
        c.chunking = ch;
        c.flush = BROTLI_ENC_FLUSH_MIDDLE;
        c.seed = i | (q << 8);
        CHECK_STRING(
            do_test_mimic_brotli_encode(s, &c, data.data.ptr, data.meta.wi));
      }
    }
  }
  // Large-window brotli is supported (by the Wuffs encoder) at quality 2.
  for (uint32_t w = 22; w <= 30; w += 4) {
    brotli_enc_config c = make_brotli_enc_config(2, w);
    brotli_enc_config__add_param(&c, BROTLI_ENC_PARAM_LARGE_WINDOW, 1);
    c.chunking = BROTLI_ENC_CHUNK_RANDOM;
    c.flush = BROTLI_ENC_FLUSH_MIDDLE;
    c.seed = w;
    char s[256];
    snprintf(s, sizeof s, "q=2 large_window lgwin=%u", w);
    CHECK_STRING(
        do_test_mimic_brotli_encode(s, &c, data.data.ptr, data.meta.wi));
  }
  return NULL;
}

// test_mimic_brotli_encode_parity_q10_grid is like
// test_mimic_brotli_encode_parity_q2_grid but for qualities 10 and 11, with
// shorter inputs (to keep it fast).
const char*  //
test_mimic_brotli_encode_parity_q10_grid() {
  CHECK_FOCUS(__func__);
  static const uint32_t lgwins[] = {10, 24};
  static const uint32_t chunkings[] = {
      BROTLI_ENC_CHUNK_SINGLE,
      BROTLI_ENC_CHUNK_7,
      BROTLI_ENC_CHUNK_RANDOM,
  };
  wuffs_base__io_buffer data = ((wuffs_base__io_buffer){
      .data = g_mimiclib_scratch_slice_u8,
  });
  for (size_t f = 0; f < WUFFS_TESTLIB_ARRAY_SIZE(brotli_enc_parity_filenames);
       f++) {
    data.meta.ri = 0;
    data.meta.wi = 0;
    data.meta.closed = false;
    CHECK_STRING(read_file(&data, brotli_enc_parity_filenames[f]));
    for (uint32_t q = 10; q <= 11; q++) {
      for (size_t w = 0; w < WUFFS_TESTLIB_ARRAY_SIZE(lgwins); w++) {
        for (uint32_t mode = 0; mode <= 2; mode++) {
          for (size_t k = 0; k < WUFFS_TESTLIB_ARRAY_SIZE(chunkings); k++) {
            uint32_t ch = chunkings[k];
            for (uint32_t fl = 0; fl < BROTLI_ENC_NUM_FLUSHES; fl++) {
              brotli_enc_config c = make_brotli_enc_config(q, lgwins[w]);
              brotli_enc_config__add_param(&c, BROTLI_ENC_PARAM_MODE, mode);
              c.chunking = ch;
              c.flush = fl;
              c.seed = (f << 16) | (w << 8) | (mode << 4) | fl | (q << 20);
              // Use the first 4 KiB (7-byte chunks) or 24 KiB (otherwise)
              // of the input.
              size_t n = data.meta.wi;
              if ((ch == BROTLI_ENC_CHUNK_7) && (n > 4096)) {
                n = 4096;
              } else if (n > 24576) {
                n = 24576;
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
test_mimic_brotli_encode_parity_q10_metadata() {
  CHECK_FOCUS(__func__);
  static const size_t lens[] = {0, 1, 100, 70000};
  wuffs_base__io_buffer data = ((wuffs_base__io_buffer){
      .data = g_mimiclib_scratch_slice_u8,
  });
  CHECK_STRING(read_file(&data, "test/data/romeo.txt"));
  for (uint32_t q = 10; q <= 11; q++) {
    for (size_t i = 0; i < WUFFS_TESTLIB_ARRAY_SIZE(lens); i++) {
      for (uint32_t pos = 1; pos <= 3; pos++) {
        for (uint32_t fl = 0; fl < BROTLI_ENC_NUM_FLUSHES; fl++) {
          for (size_t dc = 0; dc < 2; dc++) {
            brotli_enc_config c = make_brotli_enc_config(q, 16);
            c.chunking = dc ? BROTLI_ENC_CHUNK_RANDOM : BROTLI_ENC_CHUNK_7;
            c.flush = fl;
            c.metadata_position = pos;
            c.metadata_len = lens[i];
            c.dst_chunk = dc ? 5 : 0;
            c.seed = (i << 8) | pos | (q << 12);
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
test_mimic_brotli_encode_parity_q10_params() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer data = ((wuffs_base__io_buffer){
      .data = g_mimiclib_scratch_slice_u8,
  });
  CHECK_STRING(read_file(&data, "test/data/brotli/alice29.txt"));
  // Use the first 40 KiB, to keep it fast.
  uint32_t n = (uint32_t)data.meta.wi;
  if (n > 40960) {
    n = 40960;
  }
  // Each row is up to three (BrotliEncoderParameter, value) pairs. A zero key
  // (other than in the first pair) ends the row.
  static const uint32_t extras[][6] = {
      {BROTLI_ENC_PARAM_SIZE_HINT, 0xFFFFFFFF},  // Replaced by the exact size.
      {BROTLI_ENC_PARAM_SIZE_HINT, 1},
      {BROTLI_ENC_PARAM_SIZE_HINT, 1u << 30},
      {BROTLI_ENC_PARAM_LGBLOCK, 16},
      {BROTLI_ENC_PARAM_LGBLOCK, 17},
      {BROTLI_ENC_PARAM_LGBLOCK, 24},
      {BROTLI_ENC_PARAM_DISABLE_LITERAL_CONTEXT_MODELING, 1},
      {BROTLI_ENC_PARAM_DISABLE_LITERAL_CONTEXT_MODELING, 1,  //
       BROTLI_ENC_PARAM_MODE, 1},
      {BROTLI_ENC_PARAM_MODE, 1},
      {BROTLI_ENC_PARAM_MODE, 2},
      {BROTLI_ENC_PARAM_NPOSTFIX, 1},
      {BROTLI_ENC_PARAM_NPOSTFIX, 2, BROTLI_ENC_PARAM_NDIRECT, 12},
      {BROTLI_ENC_PARAM_NPOSTFIX, 3, BROTLI_ENC_PARAM_NDIRECT, 120},
      {BROTLI_ENC_PARAM_NPOSTFIX, 0, BROTLI_ENC_PARAM_NDIRECT, 15},
      {BROTLI_ENC_PARAM_MODE, 2, BROTLI_ENC_PARAM_NPOSTFIX, 3,
       BROTLI_ENC_PARAM_NDIRECT, 30},
      {BROTLI_ENC_PARAM_STREAM_OFFSET, 1},
      {BROTLI_ENC_PARAM_STREAM_OFFSET, 1000},
      {BROTLI_ENC_PARAM_STREAM_OFFSET, 1u << 30},
      {BROTLI_ENC_PARAM_LGWIN, 0xFFFFFFFF},  // (int)-1.
      {BROTLI_ENC_PARAM_SIZE_HINT, 1u << 21,  //
       BROTLI_ENC_PARAM_LGBLOCK, 18},
  };
  for (uint32_t q = 10; q <= 11; q++) {
    for (size_t i = 0; i < WUFFS_TESTLIB_ARRAY_SIZE(extras); i++) {
      for (uint32_t ch = 0; ch < BROTLI_ENC_NUM_CHUNKINGS; ch += 4) {
        brotli_enc_config c = make_brotli_enc_config(q, 22);
        char s[256];
        int k = snprintf(s, sizeof s, "q=%u chunking=%u", q, ch);
        for (size_t j = 0; (j < 6) && ((j == 0) || extras[i][j]); j += 2) {
          uint32_t v = extras[i][j + 1];
          if ((extras[i][j] == BROTLI_ENC_PARAM_SIZE_HINT) &&
              (v == 0xFFFFFFFF)) {
            v = n;
          }
          brotli_enc_config__add_param(&c, extras[i][j], v);
          if ((k > 0) && ((size_t)k < sizeof s)) {
            k += snprintf(s + k, sizeof s - (size_t)k, " param[%u]=%u",
                          extras[i][j], v);
          }
        }
        c.chunking = ch;
        c.flush = BROTLI_ENC_FLUSH_MIDDLE;
        c.seed = i | (q << 8);
        CHECK_STRING(do_test_mimic_brotli_encode(s, &c, data.data.ptr, n));
      }
    }
  }
  return NULL;
}

const char*  //
do_test_mimic_brotli_encode_file(const char* name,
                                 const uint8_t* data,
                                 size_t len) {
  for (uint32_t q = 0; q <= 11; q++) {
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

const char*  //
bench_wuffs_brotli_encode_alice29_q5() {
  CHECK_FOCUS(__func__);
  return do_bench_brotli_encode(wuffs_brotli_encode_plan, 5, 10);
}

const char*  //
bench_wuffs_brotli_encode_alice29_q9() {
  CHECK_FOCUS(__func__);
  return do_bench_brotli_encode(wuffs_brotli_encode_plan, 9, 3);
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

const char*  //
bench_mimic_brotli_encode_alice29_q5() {
  CHECK_FOCUS(__func__);
  return do_bench_brotli_encode(mimic_brotli_encode, 5, 10);
}

const char*  //
bench_mimic_brotli_encode_alice29_q9() {
  CHECK_FOCUS(__func__);
  return do_bench_brotli_encode(mimic_brotli_encode, 9, 3);
}

#endif  // WUFFS_MIMIC

// ---------------- Metablock Builder Tests

// These tests check the encoder's metablock builder (the ports of
// libbrotlienc's bit_cost.c, histogram.c, block_splitter.c, cluster.c and
// metablock.c, exposed through the test-only metablock_tester struct)
// against golden values computed with libbrotlienc v1.2.1. The much larger
// script/manual-test-brotli-metablock-parity.c compares them with the C
// functions directly.

static uint32_t  //
mbt_log2_floor(uint64_t x) {
  uint32_t n = 0;
  while (x >>= 1) {
    n++;
  }
  return n;
}

static uint64_t  //
mbt_fnv(uint64_t h, const uint8_t* p, size_t n) {
  size_t i;
  for (i = 0; i < n; i++) {
    h = (h ^ p[i]) * 0x100000001B3ull;
  }
  return h;
}

static uint64_t  //
mbt_fnv_u64(uint64_t h, uint64_t v) {
  uint8_t b[8];
  int i;
  for (i = 0; i < 8; i++) {
    b[i] = (uint8_t)(v >> (8 * i));
  }
  return mbt_fnv(h, b, 8);
}

static void  //
mbt_put_u32le(uint8_t* p, uint32_t v) {
  p[0] = (uint8_t)(v >> 0);
  p[1] = (uint8_t)(v >> 8);
  p[2] = (uint8_t)(v >> 16);
  p[3] = (uint8_t)(v >> 24);
}

// mbt_make_cmds writes the 16-byte commands (as command.h's InitCommand and
// InitInsertCommand would, for the given npostfix and ndirect) of a
// pseudo-random parse of total bytes to dst and returns how many there are.
// dst must have room for total + 1 commands.
static size_t  //
mbt_make_cmds(uint8_t* dst, size_t total, uint32_t npostfix, uint32_t ndirect,
              uint32_t seed) {
  size_t n = 0;
  size_t pos = 0;
  uint32_t r = seed;
  while (pos < total) {
    size_t ins, copy, dcode, rem;
    uint32_t inscode, copycode, dist_extra = 0;
    uint16_t dist_prefix, cmd_prefix;
    int use_last;
    int insert_only = 0;
    r = (r * 1103515245u) + 12345u;
    ins = (r >> 16) % ((r & 0x300) ? 12 : 300);
    r = (r * 1103515245u) + 12345u;
    copy = 2 + ((r >> 16) % ((r & 0x700) ? 14 : 200));
    rem = total - pos;
    if (ins > rem) {
      ins = rem;
    }
    if ((rem - ins) < copy) {
      ins = rem;
      copy = 0;
    }
    if (copy == 0) {
      // InitInsertCommand.
      insert_only = 1;
      dist_prefix = 16;
      copy = 4;
      use_last = 0;
    } else {
      r = (r * 1103515245u) + 12345u;
      if ((r & 0x30000) == 0) {
        dcode = (r >> 18) & 15;
      } else {
        size_t max = pos + ins;
        dcode = 16 + (((r >> 8) % (max ? max : 1)) % 3000);
      }
      // PrefixEncodeCopyDistance.
      if (dcode < 16 + ndirect) {
        dist_prefix = (uint16_t)dcode;
      } else {
        size_t d = ((size_t)1 << (npostfix + 2)) + (dcode - 16 - ndirect);
        size_t bucket = mbt_log2_floor(d) - 1;
        size_t postfix = d & ((1u << npostfix) - 1);
        size_t prefix = (d >> bucket) & 1;
        size_t offset = (2 + prefix) << bucket;
        size_t nbits = bucket - npostfix;
        dist_prefix = (uint16_t)((nbits << 10) |
                                 (16 + ndirect +
                                  ((2 * (nbits - 1) + prefix) << npostfix) +
                                  postfix));
        dist_extra = (uint32_t)((d - offset) >> npostfix);
      }
      use_last = (dist_prefix & 0x3FF) == 0;
    }
    // GetInsertLengthCode.
    if (ins < 6) {
      inscode = (uint32_t)ins;
    } else if (ins < 130) {
      uint32_t nb = mbt_log2_floor(ins - 2) - 1;
      inscode = (nb << 1) + (uint32_t)((ins - 2) >> nb) + 2;
    } else if (ins < 2114) {
      inscode = mbt_log2_floor(ins - 66) + 10;
    } else if (ins < 6210) {
      inscode = 21;
    } else if (ins < 22594) {
      inscode = 22;
    } else {
      inscode = 23;
    }
    // GetCopyLengthCode.
    if (copy < 10) {
      copycode = (uint32_t)(copy - 2);
    } else if (copy < 134) {
      uint32_t nb = mbt_log2_floor(copy - 6) - 1;
      copycode = (nb << 1) + (uint32_t)((copy - 6) >> nb) + 4;
    } else if (copy < 2118) {
      copycode = mbt_log2_floor(copy - 70) + 12;
    } else {
      copycode = 23;
    }
    // CombineLengthCodes.
    {
      uint32_t bits64 = (copycode & 7) | ((inscode & 7) << 3);
      if (use_last && (inscode < 8) && (copycode < 16)) {
        cmd_prefix = (uint16_t)((copycode < 8) ? bits64 : (bits64 | 64));
      } else {
        uint32_t off = 2 * ((copycode >> 3) + 3 * (inscode >> 3));
        off = (off << 5) + 0x40 + ((0x520D40u >> off) & 0xC0);
        cmd_prefix = (uint16_t)(off | bits64);
      }
    }
    mbt_put_u32le(dst + 16 * n + 0, (uint32_t)ins);
    mbt_put_u32le(dst + 16 * n + 4, insert_only ? (4u << 25) : (uint32_t)copy);
    mbt_put_u32le(dst + 16 * n + 8, dist_extra);
    dst[16 * n + 12] = (uint8_t)cmd_prefix;
    dst[16 * n + 13] = (uint8_t)(cmd_prefix >> 8);
    dst[16 * n + 14] = (uint8_t)dist_prefix;
    dst[16 * n + 15] = (uint8_t)(dist_prefix >> 8);
    n++;
    pos += ins + (insert_only ? 0 : copy);
  }
  return n;
}

typedef struct {
  const char* filename;  // NULL means the synthetic input.
  uint32_t quality;
  uint32_t mode;
  uint32_t nolcm;
  uint32_t npostfix;
  uint32_t ndirect;
  uint32_t large_window;
  uint32_t seed;
  uint64_t want_hash;
} mbt_case;

static const mbt_case g_mbt_cases[] = {
    {"test/data/romeo.txt", 11, 2, 0, 0, 0, 0, 0x12345678,
     0x91BCDB702326DA68},
    {"test/data/midsummer.txt", 10, 3, 1, 1, 4, 0, 0x9ABCDEF0,
     0xC1BC25672AFFE52C},
    {NULL, 11, 0, 0, 2, 8, 1, 0x0BADF00D, 0x106C079EF30D8335},
    {NULL, 10, 1, 0, 3, 120, 0, 0x00C0FFEE, 0x1CE5AB9FFAB4CEDB},
    {"test/data/pi.txt", 11, 2, 0, 1, 2, 0, 0x31415926, 0x4434059D684D4F0E},
    {"", 11, 2, 0, 0, 0, 0, 0x00000001, 0xAE70E50382B8388F},
};

const char*  //
test_wuffs_brotli_metablock_build_meta_block() {
  CHECK_FOCUS(__func__);
  wuffs_brotli__metablock_tester* t = wuffs_brotli__metablock_tester__alloc();
  if (!t) {
    RETURN_FAIL("alloc failed");
  }
  const char* ret = NULL;
  uint8_t* rb = NULL;
  uint8_t* cmds = NULL;
  uint8_t* wb = NULL;
  size_t wb_len = 1 << 20;
  size_t i;
  for (i = 0; i < WUFFS_TESTLIB_ARRAY_SIZE(g_mbt_cases); i++) {
    const mbt_case* tc = &g_mbt_cases[i];
    size_t total = 0;
    size_t rb_size = 1024;
    size_t n, k;
    int c;
    uint64_t h = 0xCBF29CE484222325ull;
    wuffs_base__status status;
    wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
        .data = g_have_slice_u8,
    });
    if (!tc->filename) {
      // A byte distribution that shifts every 700 bytes, 48 KiB.
      uint32_t r = 1;
      total = 49152;
      for (k = 0; k < total; k++) {
        r = (r * 1103515245u) + 12345u;
        src.data.ptr[k] = (uint8_t)(((k / 700) * 37) +
                                    ((r >> 16) % (1 + (k / 700) % 23)));
      }
    } else if (*tc->filename) {
      ret = read_file(&src, tc->filename);
      if (ret) {
        goto done;
      }
      total = src.meta.wi;
    }
    while (rb_size < total) {
      rb_size *= 2;
    }
    free(rb);
    free(cmds);
    rb = calloc(rb_size, 1);
    cmds = malloc(16 * (total + 1));
    if (!rb || !cmds) {
      ret = "out of memory";
      goto done;
    }
    if (total) {
      memcpy(rb, src.data.ptr, total);
    }
    n = mbt_make_cmds(cmds, total, tc->npostfix, tc->ndirect, tc->seed);
    while (true) {
      if (!wb) {
        wb = malloc(wb_len);
        if (!wb) {
          ret = "out of memory";
          goto done;
        }
      }
      status = wuffs_brotli__metablock_tester__build_meta_block(
          t, wuffs_base__make_slice_u8(wb, wb_len),
          wuffs_base__make_slice_u8(rb, rb_size), 0, rb_size - 1, 0, 0,
          wuffs_base__make_slice_u8(cmds, 16 * n), n, tc->mode, tc->quality,
          tc->large_window, tc->nolcm, tc->npostfix, tc->ndirect);
      if (status.repr != wuffs_base__error__bad_workbuf_length) {
        break;
      }
      // The commands were rewritten, so regenerate them.
      n = mbt_make_cmds(cmds, total, tc->npostfix, tc->ndirect, tc->seed);
      free(wb);
      wb = NULL;
      wb_len *= 2;
    }
    if (status.repr) {
      ret = status.repr;
      goto done;
    }
    {
      uint64_t ndc = wuffs_brotli__metablock_tester__get(t, 27);
      wuffs_brotli__metablock_tester__optimize_histograms(
          t, wuffs_base__make_slice_u8(wb, wb_len),
          (uint32_t)((ndc < 544) ? ndc : 544));
    }
    for (c = 0; c < 3; c++) {
      uint64_t num_blocks = wuffs_brotli__metablock_tester__get(t, 4 * c + 1);
      uint64_t types = wuffs_brotli__metablock_tester__get(t, 4 * c + 2);
      uint64_t lengths = wuffs_brotli__metablock_tester__get(t, 4 * c + 3);
      h = mbt_fnv_u64(h, wuffs_brotli__metablock_tester__get(t, 4 * c + 0));
      h = mbt_fnv_u64(h, num_blocks);
      h = mbt_fnv(h, wb + types, num_blocks);
      h = mbt_fnv(h, wb + lengths, 4 * num_blocks);
    }
    for (c = 0; c < 3; c += 2) {
      uint64_t size = wuffs_brotli__metablock_tester__get(t, 15 + c);
      h = mbt_fnv_u64(h, size);
      h = mbt_fnv(h, wb + wuffs_brotli__metablock_tester__get(t, 12 + c),
                  4 * size);
    }
    for (c = 0; c < 3; c++) {
      static const size_t hsizes[3] = {1040, 2832, 2192};
      uint64_t size = wuffs_brotli__metablock_tester__get(t, 21 + c);
      h = mbt_fnv_u64(h, size);
      h = mbt_fnv(h, wb + wuffs_brotli__metablock_tester__get(t, 18 + c),
                  hsizes[c] * size);
    }
    for (c = 24; c < 29; c++) {
      h = mbt_fnv_u64(h, wuffs_brotli__metablock_tester__get(t, c));
    }
    h = mbt_fnv(h, cmds, 16 * n);
    if (h != tc->want_hash) {
      RETURN_FAIL("case %zu: hash: have 0x%016" PRIX64 ", want 0x%016" PRIX64,
                  i, h, tc->want_hash);
    }
    if (wuffs_brotli__metablock_tester__get(t, 29) >
        wuffs_brotli__metablock_tester__get(t, 30)) {
      RETURN_FAIL("case %zu: workbuf used exceeds the worst case", i);
    }
  }
done:
  free(rb);
  free(cmds);
  free(wb);
  free(t);
  return ret;
}

const char*  //
test_wuffs_brotli_metablock_bit_cost() {
  CHECK_FOCUS(__func__);
  wuffs_brotli__metablock_tester* t = wuffs_brotli__metablock_tester__alloc();
  if (!t) {
    RETURN_FAIL("alloc failed");
  }
  // The C code's BrotliBitsEntropy and BrotliPopulationCost{Literal,Command,
  // Distance} results, as binary64 bit patterns.
  static const uint64_t want_pop[6][3] = {
      {0x4028000000000000, 0x4028000000000000, 0x4028000000000000},
      {0x4028000000000000, 0x4028000000000000, 0x4028000000000000},
      {0x4055800000000000, 0x4055800000000000, 0x4055800000000000},
      {0x4055800000000000, 0x406AC00000000000, 0x406AC00000000000},
      {0x4055800000000000, 0x4076300000000000, 0x4076300000000000},
      {0x40F842EE8C1F42B3, 0x4113DB811CA8812F, 0x410D76A1EA720A2C},
  };
  static const uint32_t data_sizes[3] = {256, 704, 544};
  // Each histogram is data_size u32le counts, a u64le total_count_ and a
  // binary64 bit_cost_.
  uint8_t pop[4 * 704];
  uint8_t histo[(4 * 704) + 16];
  uint32_t counts[704];
  size_t i;
  int k, c;
  for (i = 0; i < 704; i++) {
    counts[i] = (uint32_t)((i * i * 7 + 3) % 101);
    mbt_put_u32le(pop + 4 * i, counts[i]);
  }
  {
    uint64_t have = wuffs_brotli__metablock_tester__bits_entropy(
        t, wuffs_base__make_slice_u8(pop, sizeof(pop)), 256);
    if (have != 0x40F79A0C92B03A26ull) {
      free(t);
      RETURN_FAIL("bits_entropy: have 0x%016" PRIX64, have);
    }
  }
  for (k = 0; k < 6; k++) {
    static const int nz[6] = {0, 1, 2, 3, 4, 300};
    for (c = 0; c < 3; c++) {
      uint64_t total = 0;
      uint64_t have;
      memset(histo, 0, sizeof(histo));
      for (i = 0; i < data_sizes[c]; i++) {
        int on = (nz[k] >= 300) ? (counts[i] != 0)
                                : (((i % 151) == 7) && ((int)(i / 151) < nz[k]));
        if (on) {
          mbt_put_u32le(histo + 4 * i, counts[i] + 1);
          total += counts[i] + 1;
        }
      }
      for (i = 0; i < 8; i++) {
        histo[(4 * data_sizes[c]) + i] = (uint8_t)(total >> (8 * i));
      }
      have = wuffs_brotli__metablock_tester__population_cost(
          t, wuffs_base__make_slice_u8(histo, (4 * data_sizes[c]) + 16),
          data_sizes[c]);
      if (have != want_pop[k][c]) {
        free(t);
        RETURN_FAIL("population_cost(k=%d, c=%d): have 0x%016" PRIX64
                    ", want 0x%016" PRIX64,
                    k, c, have, want_pop[k][c]);
      }
    }
  }
  free(t);
  return NULL;
}

// ---------------- Large Window and Dictionary Tests

// See test/data/brotli/README.md for where the large-window (lw), raw
// dictionary (raw) and serialized shared dictionary (sd) test files came from.

typedef struct {
  uint32_t type;  // WUFFS_BROTLI__DICTIONARY_TYPE_ETC.
  const char* filename;
  // lo and hi select the [lo .. hi) bytes of the file. A zero hi means the
  // whole file.
  size_t lo;
  size_t hi;
} brotli_dict_spec;

#define BROTLI_DICT_SPECS_MAX 4

// g_brotli_dict_sets lists the sets of dictionaries (attached in order) that
// the test files use. A NULL filename terminates each set.
struct {
  brotli_dict_spec specs[BROTLI_DICT_SPECS_MAX];
} g_brotli_dict_sets[] = {
#define BROTLI_DICT_SET_NONE 0
    {{{0, NULL, 0, 0}}},
#define BROTLI_DICT_SET_RAW_ROMEO 1
    {{{WUFFS_BROTLI__DICTIONARY_TYPE_RAW, "test/data/romeo.txt", 0, 0}}},
#define BROTLI_DICT_SET_RAW_ALICE_CHUNKS 2
    {{
        {WUFFS_BROTLI__DICTIONARY_TYPE_RAW, "test/data/brotli/alice29.txt", 0,
         20000},
        {WUFFS_BROTLI__DICTIONARY_TYPE_RAW, "test/data/brotli/alice29.txt",
         20000, 20001},
        {WUFFS_BROTLI__DICTIONARY_TYPE_RAW, "test/data/brotli/alice29.txt",
         20001, 20001},
        {WUFFS_BROTLI__DICTIONARY_TYPE_RAW, "test/data/brotli/alice29.txt",
         20001, 60000},
    }},
#define BROTLI_DICT_SET_RAW_ROMEO_CHUNKS 3
    {{
        {WUFFS_BROTLI__DICTIONARY_TYPE_RAW, "test/data/romeo.txt", 0, 300},
        {WUFFS_BROTLI__DICTIONARY_TYPE_RAW, "test/data/romeo.txt", 300, 500},
        {WUFFS_BROTLI__DICTIONARY_TYPE_RAW, "test/data/romeo.txt", 500, 942},
    }},
#define BROTLI_DICT_SET_SD_FULL 4
    {{{WUFFS_BROTLI__DICTIONARY_TYPE_SERIALIZED,
       "test/data/brotli/shared-dictionary.full.bin", 0, 0}}},
#define BROTLI_DICT_SET_SD_WORDS 5
    {{{WUFFS_BROTLI__DICTIONARY_TYPE_SERIALIZED,
       "test/data/brotli/shared-dictionary.words.bin", 0, 0}}},
#define BROTLI_DICT_SET_SD_PREFIX_AND_RAW 6
    {{
        {WUFFS_BROTLI__DICTIONARY_TYPE_SERIALIZED,
         "test/data/brotli/shared-dictionary.prefix.bin", 0, 0},
        {WUFFS_BROTLI__DICTIONARY_TYPE_RAW, "test/data/romeo.txt", 0, 300},
    }},
#define BROTLI_DICT_SET_SD_UTF8 7
    {{{WUFFS_BROTLI__DICTIONARY_TYPE_SERIALIZED,
       "test/data/brotli/shared-dictionary.utf8.bin", 0, 0}}},
};

// g_brotli_ext_vectors lists (compressed, decompressed) pairs of test files
// that need the large-window quirk or attached dictionaries.
struct {
  const char* src_filename;
  const char* want_filename;
  bool large_window;
  size_t dict_set;
} g_brotli_ext_vectors[] = {
    {"test/data/brotli/alice29.txt.q11lw26.br", "test/data/brotli/alice29.txt",
     true, BROTLI_DICT_SET_NONE},
    {"test/data/brotli/alice29.txt.q5lw10.br", "test/data/brotli/alice29.txt",
     true, BROTLI_DICT_SET_NONE},
    {"test/data/brotli/romeo.txt.q11lw30.br", "test/data/romeo.txt", true,
     BROTLI_DICT_SET_NONE},
    {"test/data/brotli/romeo.txt.q9lw28.br", "test/data/romeo.txt", true,
     BROTLI_DICT_SET_NONE},
    {"test/data/brotli/romeo.txt.q11w22.raw-romeo.br", "test/data/romeo.txt",
     false, BROTLI_DICT_SET_RAW_ROMEO},
    {"test/data/brotli/romeo.txt.q11lw30.raw-romeo.br", "test/data/romeo.txt",
     true, BROTLI_DICT_SET_RAW_ROMEO},
    {"test/data/brotli/alice29.txt.q5w18.raw-chunks.br",
     "test/data/brotli/alice29.txt", false, BROTLI_DICT_SET_RAW_ALICE_CHUNKS},
    {"test/data/brotli/alice29.txt.q11w18.raw-chunks.br",
     "test/data/brotli/alice29.txt", false, BROTLI_DICT_SET_RAW_ALICE_CHUNKS},
    {"test/data/brotli/romeo.txt.q11w16.raw-chunks.br", "test/data/romeo.txt",
     false, BROTLI_DICT_SET_RAW_ROMEO_CHUNKS},
    {"test/data/brotli/shared-dictionary.text.q5w18.sd-full.br",
     "test/data/brotli/shared-dictionary.text", false, BROTLI_DICT_SET_SD_FULL},
    {"test/data/brotli/shared-dictionary.text.q11w18.sd-full.br",
     "test/data/brotli/shared-dictionary.text", false, BROTLI_DICT_SET_SD_FULL},
    {"test/data/brotli/shared-dictionary.text.q11lw24.sd-full.br",
     "test/data/brotli/shared-dictionary.text", true, BROTLI_DICT_SET_SD_FULL},
    {"test/data/brotli/alice29.txt.q11w22.sd-words.br",
     "test/data/brotli/alice29.txt", false, BROTLI_DICT_SET_SD_WORDS},
    {"test/data/brotli/asyoulik.txt.q11w18.sd-prefix-raw.br",
     "test/data/brotli/asyoulik.txt", false, BROTLI_DICT_SET_SD_PREFIX_AND_RAW},
    {"test/data/brotli/romeo-mutated.sd-utf8.br",
     "test/data/brotli/romeo-mutated.sd-utf8.txt", false,
     BROTLI_DICT_SET_SD_UTF8},
};

// g_brotli_ext_malformed is like g_brotli_malformed but for inputs that need
// the large-window quirk or attached dictionaries. The invalid.lw-*,
// invalid.raw-* and invalid.sd-* files were found by mutating the
// g_brotli_ext_vectors files (see test/data/brotli/README.md).
struct {
  const char* src_filename;
  const char* data;
  size_t len;
  bool large_window;
  size_t dict_set;
  const char* want_status;
} g_brotli_ext_malformed[] = {
    // BROTLI_DECODER_ERROR_FORMAT_WINDOW_BITS: the large-window escape
    // followed by a non-zero reserved bit.
    {NULL, "\x91", 1, true, BROTLI_DICT_SET_NONE,
     wuffs_brotli__error__bad_window_bits},
    // BROTLI_DECODER_ERROR_FORMAT_WINDOW_BITS: large-window WBITS of 9 (too
    // small) and 62 (too large).
    {NULL, "\x11\x09", 2, true, BROTLI_DICT_SET_NONE,
     wuffs_brotli__error__bad_window_bits},
    {NULL, "\x11\x3E", 2, true, BROTLI_DICT_SET_NONE,
     wuffs_brotli__error__bad_window_bits},
    // BROTLI_DECODER_NEEDS_MORE_INPUT: a truncated large-window header.
    {NULL, "\x11", 1, true, BROTLI_DICT_SET_NONE,
     wuffs_brotli__error__truncated_input},
    // BROTLI_DECODER_ERROR_FORMAT_WINDOW_BITS: a large-window stream without
    // the large-window quirk.
    {"test/data/brotli/romeo.txt.q11lw30.br", NULL, 0, false,
     BROTLI_DICT_SET_NONE, wuffs_brotli__error__unsupported_large_window},
    // BROTLI_DECODER_ERROR_FORMAT_DISTANCE.
    {"test/data/brotli/invalid.lw-distance.br", NULL, 0, true,
     BROTLI_DICT_SET_NONE, wuffs_brotli__error__bad_distance},
    // BROTLI_DECODER_ERROR_FORMAT_DICTIONARY.
    {"test/data/brotli/invalid.lw-dictionary.br", NULL, 0, true,
     BROTLI_DICT_SET_NONE, wuffs_brotli__error__bad_dictionary},
    // BROTLI_DECODER_ERROR_COMPOUND_DICTIONARY.
    {"test/data/brotli/invalid.raw-compound-dictionary.br", NULL, 0, false,
     BROTLI_DICT_SET_RAW_ROMEO_CHUNKS,
     wuffs_brotli__error__bad_compound_dictionary},
    // BROTLI_DECODER_ERROR_FORMAT_DICTIONARY.
    {"test/data/brotli/invalid.raw-dictionary.br", NULL, 0, false,
     BROTLI_DICT_SET_RAW_ROMEO_CHUNKS, wuffs_brotli__error__bad_dictionary},
    // BROTLI_DECODER_ERROR_FORMAT_TRANSFORM.
    {"test/data/brotli/invalid.raw-transform.br", NULL, 0, false,
     BROTLI_DICT_SET_RAW_ROMEO_CHUNKS, wuffs_brotli__error__bad_transform},
    // BROTLI_DECODER_ERROR_COMPOUND_DICTIONARY: a stream compressed without
    // a dictionary refers to the static dictionary, but with a compound
    // dictionary attached, the same distance refers to the compound
    // dictionary.
    {"test/data/brotli/romeo.txt.q11w22.br", NULL, 0, false,
     BROTLI_DICT_SET_SD_FULL, wuffs_brotli__error__bad_compound_dictionary},
    // BROTLI_DECODER_ERROR_COMPOUND_DICTIONARY.
    {"test/data/brotli/invalid.sd-compound-dictionary.br", NULL, 0, false,
     BROTLI_DICT_SET_SD_FULL, wuffs_brotli__error__bad_compound_dictionary},
    // BROTLI_DECODER_ERROR_FORMAT_DICTIONARY.
    {"test/data/brotli/invalid.sd-dictionary.br", NULL, 0, false,
     BROTLI_DICT_SET_SD_FULL, wuffs_brotli__error__bad_dictionary},
    // BROTLI_DECODER_ERROR_FORMAT_TRANSFORM.
    {"test/data/brotli/invalid.sd-transform.br", NULL, 0, false,
     BROTLI_DICT_SET_SD_FULL, wuffs_brotli__error__bad_transform},
    {"test/data/brotli/invalid.lw-sd-transform.br", NULL, 0, true,
     BROTLI_DICT_SET_SD_FULL, wuffs_brotli__error__bad_transform},
};

// g_brotli_dict_arrays holds the dictionaries' bytes.
uint8_t g_brotli_dict_arrays[BROTLI_DICT_SPECS_MAX][256 * 1024];

// load_brotli_dict_set loads the dict_set'th g_brotli_dict_sets entry.
const char*  //
load_brotli_dict_set(size_t dict_set,
                     size_t* num_dicts,
                     uint32_t* dict_types,
                     wuffs_base__slice_u8* dicts) {
  *num_dicts = 0;
  if (dict_set >= WUFFS_TESTLIB_ARRAY_SIZE(g_brotli_dict_sets)) {
    return "load_brotli_dict_set: dict_set out of range";
  }
  for (size_t i = 0; i < BROTLI_DICT_SPECS_MAX; i++) {
    const brotli_dict_spec* spec = &g_brotli_dict_sets[dict_set].specs[i];
    if (!spec->filename) {
      break;
    }
    wuffs_base__io_buffer buf = ((wuffs_base__io_buffer){
        .data = wuffs_base__make_slice_u8(g_brotli_dict_arrays[i],
                                          sizeof g_brotli_dict_arrays[i]),
    });
    CHECK_STRING(read_file(&buf, spec->filename));
    size_t hi = spec->hi ? spec->hi : buf.meta.wi;
    if ((spec->lo > hi) || (hi > buf.meta.wi)) {
      return "load_brotli_dict_set: bad lo/hi";
    }
    dict_types[i] = spec->type;
    dicts[i] = wuffs_base__make_slice_u8(buf.data.ptr + spec->lo,
                                         hi - spec->lo);
    *num_dicts = i + 1;
  }
  return NULL;
}

// get_brotli_big_workbuf sets *workbuf to a malloc'ed buffer that is large
// enough for any large-window stream and 1 MiB of dictionaries. It is never
// freed. Only the parts of it that the decoder uses are touched, so the
// operating system should only need to commit as much memory as the stream's
// window size.
const char*  //
get_brotli_big_workbuf(wuffs_base__slice_u8* workbuf) {
  static uint8_t* ptr = NULL;
  const size_t n = WUFFS_BROTLI__DECODER_WORKBUF_LEN_LARGE_WINDOW + (1 << 20);
  if (!ptr) {
    ptr = (uint8_t*)malloc(n);
    if (!ptr) {
      return "get_brotli_big_workbuf: out of memory";
    }
  }
  *workbuf = wuffs_base__make_slice_u8(ptr, n);
  return NULL;
}

// wuffs_brotli_decode_with_options is like wuffs_brotli_decode but it can
// also set the large-window quirk and attach dictionaries.
const char*  //
wuffs_brotli_decode_with_options(wuffs_base__io_buffer* dst,
                                 wuffs_base__io_buffer* src,
                                 uint64_t wlimit,
                                 uint64_t rlimit,
                                 bool large_window,
                                 size_t dict_set) {
  size_t num_dicts = 0;
  uint32_t dict_types[BROTLI_DICT_SPECS_MAX] = {0};
  wuffs_base__slice_u8 dicts[BROTLI_DICT_SPECS_MAX] = {0};
  CHECK_STRING(load_brotli_dict_set(dict_set, &num_dicts, dict_types, dicts));

  wuffs_brotli__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_brotli__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
  // The large-window workbuf_len (more than 1 GiB) is larger than
  // g_work_slice_u8.
  wuffs_base__slice_u8 workbuf = g_work_slice_u8;
  if (large_window) {
    CHECK_STATUS("set_quirk",
                 wuffs_brotli__decoder__set_quirk(
                     &dec, WUFFS_BROTLI__QUIRK_LARGE_WINDOW, 1));
    CHECK_STRING(get_brotli_big_workbuf(&workbuf));
  }
  for (size_t i = 0; i < num_dicts; i++) {
    CHECK_STATUS("add_dictionary",
                 wuffs_brotli__decoder__add_dictionary(
                     &dec, dict_types[i], dicts[i], workbuf));
  }
  wuffs_base__range_ii_u64 workbuf_len =
      wuffs_brotli__decoder__workbuf_len(&dec);
  if (workbuf_len.max_incl > workbuf.len) {
    return "workbuf_len is too large";
  }
  workbuf.len = workbuf_len.max_incl;

  while (true) {
    wuffs_base__io_buffer limited_dst = make_limited_writer(*dst, wlimit);
    wuffs_base__io_buffer limited_src = make_limited_reader(*src, rlimit);

    wuffs_base__status status = wuffs_brotli__decoder__transform_io(
        &dec, &limited_dst, &limited_src, workbuf);

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
do_test_brotli_decode_ext_all(uint64_t wlimit, uint64_t rlimit) {
  size_t n = WUFFS_TESTLIB_ARRAY_SIZE(g_brotli_ext_vectors);
  for (size_t i = 0; i < n; i++) {
    wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
        .data = g_src_slice_u8,
    });
    wuffs_base__io_buffer have = ((wuffs_base__io_buffer){
        .data = g_have_slice_u8,
    });
    wuffs_base__io_buffer want = ((wuffs_base__io_buffer){
        .data = g_want_slice_u8,
    });
    CHECK_STRING(read_file(&src, g_brotli_ext_vectors[i].src_filename));
    CHECK_STRING(read_file(&want, g_brotli_ext_vectors[i].want_filename));
    const char* status = wuffs_brotli_decode_with_options(
        &have, &src, wlimit, rlimit, g_brotli_ext_vectors[i].large_window,
        g_brotli_ext_vectors[i].dict_set);
    if (status) {
      RETURN_FAIL("%s: %s", g_brotli_ext_vectors[i].src_filename, status);
    }
    status = check_io_buffers_equal("", &have, &want);
    if (status) {
      RETURN_FAIL("%s: %s", g_brotli_ext_vectors[i].src_filename, status);
    }
    if (src.meta.ri != src.meta.wi) {
      RETURN_FAIL("%s: ri: have %zu, want %zu",
                  g_brotli_ext_vectors[i].src_filename, src.meta.ri,
                  src.meta.wi);
    }
  }
  return NULL;
}

const char*  //
test_wuffs_brotli_decode_ext_all() {
  CHECK_FOCUS(__func__);
  return do_test_brotli_decode_ext_all(UINT64_MAX, UINT64_MAX);
}

const char*  //
test_wuffs_brotli_decode_ext_all_with_io_limits() {
  CHECK_FOCUS(__func__);
  return do_test_brotli_decode_ext_all(1, 1);
}

// make_brotli_far_data writes the decompressed form of
// test/data/brotli/far17m.q9lw25.br: 64 KiB of xorshift32 output, 17 MiB of
// zeroes and the same 64 KiB again. Its second 64 KiB copy is more than
// (1 << 24) bytes after the first.
const char*  //
make_brotli_far_data(wuffs_base__io_buffer* dst) {
  const size_t a = 65536;
  const size_t z = 17 << 20;
  if ((dst->data.len - dst->meta.wi) < (a + z + a)) {
    return "make_brotli_far_data: dst is too short";
  }
  uint8_t* p = dst->data.ptr + dst->meta.wi;
  uint32_t x = 0x12345678;
  for (size_t i = 0; i < a; i++) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    p[i] = (uint8_t)(x >> 24);
  }
  memset(p + a, 0, z);
  memcpy(p + a + z, p, a);
  dst->meta.wi += a + z + a;
  return NULL;
}

const char*  //
do_test_brotli_decode_large_window_far(
    const char* (*codec_func)(wuffs_base__io_buffer*,
                              wuffs_base__io_buffer*,
                              uint64_t,
                              uint64_t,
                              bool,
                              size_t),
    uint64_t wlimit,
    uint64_t rlimit) {
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  wuffs_base__io_buffer have = ((wuffs_base__io_buffer){
      .data = g_have_slice_u8,
  });
  wuffs_base__io_buffer want = ((wuffs_base__io_buffer){
      .data = g_want_slice_u8,
  });
  CHECK_STRING(read_file(&src, "test/data/brotli/far17m.q9lw25.br"));
  CHECK_STRING(make_brotli_far_data(&want));
  CHECK_STRING(
      (*codec_func)(&have, &src, wlimit, rlimit, true, BROTLI_DICT_SET_NONE));
  return check_io_buffers_equal("", &have, &want);
}

const char*  //
test_wuffs_brotli_decode_large_window_far() {
  CHECK_FOCUS(__func__);
  CHECK_STRING(do_test_brotli_decode_large_window_far(
      wuffs_brotli_decode_with_options, UINT64_MAX, UINT64_MAX));
  return do_test_brotli_decode_large_window_far(
      wuffs_brotli_decode_with_options, 4099, 113);
}

const char*  //
test_wuffs_brotli_decode_large_window_quirk() {
  CHECK_FOCUS(__func__);

  // With the quirk set, standard (not large-window) streams still decode.
  size_t n = WUFFS_TESTLIB_ARRAY_SIZE(g_brotli_vectors);
  for (size_t i = 0; i < n; i++) {
    wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
        .data = g_src_slice_u8,
    });
    wuffs_base__io_buffer have = ((wuffs_base__io_buffer){
        .data = g_have_slice_u8,
    });
    wuffs_base__io_buffer want = ((wuffs_base__io_buffer){
        .data = g_want_slice_u8,
    });
    CHECK_STRING(read_file(&src, g_brotli_vectors[i].src_filename));
    CHECK_STRING(read_file(&want, g_brotli_vectors[i].want_filename));
    const char* status = wuffs_brotli_decode_with_options(
        &have, &src, UINT64_MAX, UINT64_MAX, true, BROTLI_DICT_SET_NONE);
    if (status) {
      RETURN_FAIL("%s: %s", g_brotli_vectors[i].src_filename, status);
    }
    CHECK_STRING(check_io_buffers_equal("", &have, &want));
  }

  // The quirk's value and the workbuf_len.
  wuffs_brotli__decoder dec;
  CHECK_STATUS("initialize", wuffs_brotli__decoder__initialize(
                                 &dec, sizeof dec, WUFFS_VERSION,
                                 WUFFS_INITIALIZE__DEFAULT_OPTIONS));
  if (wuffs_brotli__decoder__get_quirk(&dec,
                                       WUFFS_BROTLI__QUIRK_LARGE_WINDOW) != 0) {
    RETURN_FAIL("get_quirk #0: have non-zero, want zero");
  }
  wuffs_base__range_ii_u64 r = wuffs_brotli__decoder__workbuf_len(&dec);
  if ((r.min_incl != WUFFS_BROTLI__DECODER_WORKBUF_LEN_STANDARD) ||
      (r.max_incl != WUFFS_BROTLI__DECODER_WORKBUF_LEN_STANDARD) ||
      (r.max_incl != ((1 << 24) + 542))) {
    RETURN_FAIL("workbuf_len #0: have %" PRIu64, r.max_incl);
  }
  CHECK_STATUS("set_quirk", wuffs_brotli__decoder__set_quirk(
                                &dec, WUFFS_BROTLI__QUIRK_LARGE_WINDOW, 1));
  if (wuffs_brotli__decoder__get_quirk(&dec,
                                       WUFFS_BROTLI__QUIRK_LARGE_WINDOW) != 1) {
    RETURN_FAIL("get_quirk #1: have zero, want non-zero");
  }
  r = wuffs_brotli__decoder__workbuf_len(&dec);
  if ((r.min_incl != r.max_incl) ||
      (r.max_incl != WUFFS_BROTLI__DECODER_WORKBUF_LEN_LARGE_WINDOW) ||
      (r.max_incl != ((1 << 30) + 542))) {
    RETURN_FAIL("workbuf_len #1: have %" PRIu64, r.max_incl);
  }

  // Quirks cannot be set after decoding has started.
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  wuffs_base__io_buffer have = ((wuffs_base__io_buffer){
      .data = g_have_slice_u8,
  });
  CHECK_STRING(read_file(&src, "test/data/brotli/alice29.txt.q5lw10.br"));
  src.meta.wi = 3;
  src.meta.closed = false;
  wuffs_base__slice_u8 workbuf = g_work_slice_u8;
  wuffs_base__status status =
      wuffs_brotli__decoder__transform_io(&dec, &have, &src, workbuf);
  if (status.repr != wuffs_base__suspension__short_read) {
    RETURN_FAIL("transform_io: have \"%s\", want \"%s\"", status.repr,
                wuffs_base__suspension__short_read);
  }
  status = wuffs_brotli__decoder__set_quirk(
      &dec, WUFFS_BROTLI__QUIRK_LARGE_WINDOW, 0);
  if (status.repr != wuffs_base__error__bad_call_sequence) {
    RETURN_FAIL("set_quirk: have \"%s\", want \"%s\"", status.repr,
                wuffs_base__error__bad_call_sequence);
  }
  return NULL;
}

const char*  //
test_wuffs_brotli_decode_ext_malformed() {
  CHECK_FOCUS(__func__);
  size_t n = WUFFS_TESTLIB_ARRAY_SIZE(g_brotli_ext_malformed);
  for (size_t i = 0; i < n; i++) {
    for (int j = 0; j < 2; j++) {
      wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
          .data = g_src_slice_u8,
      });
      wuffs_base__io_buffer have = ((wuffs_base__io_buffer){
          .data = g_have_slice_u8,
      });
      if (g_brotli_ext_malformed[i].src_filename) {
        CHECK_STRING(read_file(&src, g_brotli_ext_malformed[i].src_filename));
      } else {
        memcpy(src.data.ptr, g_brotli_ext_malformed[i].data,
               g_brotli_ext_malformed[i].len);
        src.meta.wi = g_brotli_ext_malformed[i].len;
        src.meta.closed = true;
      }
      uint64_t limit = (j == 0) ? UINT64_MAX : 1;
      const char* have_status = wuffs_brotli_decode_with_options(
          &have, &src, limit, limit, g_brotli_ext_malformed[i].large_window,
          g_brotli_ext_malformed[i].dict_set);
      const char* want_status = g_brotli_ext_malformed[i].want_status;
      if (have_status != want_status) {
        RETURN_FAIL("i=%zu, j=%d: have \"%s\", want \"%s\"", i, j, have_status,
                    want_status);
      }
    }
  }
  return NULL;
}

const char*  //
test_wuffs_brotli_add_dictionary() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer full = ((wuffs_base__io_buffer){
      .data = wuffs_base__make_slice_u8(g_brotli_dict_arrays[0],
                                        sizeof g_brotli_dict_arrays[0]),
  });
  wuffs_base__io_buffer words = ((wuffs_base__io_buffer){
      .data = wuffs_base__make_slice_u8(g_brotli_dict_arrays[1],
                                        sizeof g_brotli_dict_arrays[1]),
  });
  wuffs_base__io_buffer prefix = ((wuffs_base__io_buffer){
      .data = wuffs_base__make_slice_u8(g_brotli_dict_arrays[2],
                                        sizeof g_brotli_dict_arrays[2]),
  });
  CHECK_STRING(
      read_file(&full, "test/data/brotli/shared-dictionary.full.bin"));
  CHECK_STRING(
      read_file(&words, "test/data/brotli/shared-dictionary.words.bin"));
  CHECK_STRING(
      read_file(&prefix, "test/data/brotli/shared-dictionary.prefix.bin"));
  wuffs_base__slice_u8 full_s =
      wuffs_base__make_slice_u8(full.data.ptr, full.meta.wi);
  wuffs_base__slice_u8 words_s =
      wuffs_base__make_slice_u8(words.data.ptr, words.meta.wi);
  wuffs_base__slice_u8 prefix_s =
      wuffs_base__make_slice_u8(prefix.data.ptr, prefix.meta.wi);
  wuffs_base__slice_u8 raw_s =
      wuffs_base__make_slice_u8(prefix.data.ptr, 100);
  wuffs_base__slice_u8 workbuf = g_work_slice_u8;
  wuffs_brotli__decoder dec;
  wuffs_base__status status;

  // An unknown dictionary type.
  CHECK_STATUS("initialize", wuffs_brotli__decoder__initialize(
                                 &dec, sizeof dec, WUFFS_VERSION,
                                 WUFFS_INITIALIZE__DEFAULT_OPTIONS));
  status = wuffs_brotli__decoder__add_dictionary(&dec, 2, raw_s, workbuf);
  if (status.repr != wuffs_base__error__bad_argument) {
    RETURN_FAIL("type 2: have \"%s\", want \"%s\"", status.repr,
                wuffs_base__error__bad_argument);
  }

  // At most 15 compound (raw or prefix) dictionaries. The workbuf_len grows
  // with each one.
  for (int i = 0; i < 15; i++) {
    CHECK_STATUS("add_dictionary #0",
                 wuffs_brotli__decoder__add_dictionary(
                     &dec, WUFFS_BROTLI__DICTIONARY_TYPE_RAW, raw_s, workbuf));
    wuffs_base__range_ii_u64 r = wuffs_brotli__decoder__workbuf_len(&dec);
    uint64_t want = WUFFS_BROTLI__DECODER_WORKBUF_LEN_STANDARD +
                    (100 * ((uint64_t)i + 1));
    if ((r.min_incl != want) || (r.max_incl != want)) {
      RETURN_FAIL("workbuf_len: have %" PRIu64 ", want %" PRIu64, r.max_incl,
                  want);
    }
  }
  status = wuffs_brotli__decoder__add_dictionary(
      &dec, WUFFS_BROTLI__DICTIONARY_TYPE_RAW, raw_s, workbuf);
  if (status.repr != wuffs_brotli__error__too_many_compound_dictionaries) {
    RETURN_FAIL("16th raw: have \"%s\", want \"%s\"", status.repr,
                wuffs_brotli__error__too_many_compound_dictionaries);
  }
  status = wuffs_brotli__decoder__add_dictionary(
      &dec, WUFFS_BROTLI__DICTIONARY_TYPE_SERIALIZED, prefix_s, workbuf);
  if (status.repr != wuffs_brotli__error__too_many_compound_dictionaries) {
    RETURN_FAIL("16th prefix: have \"%s\", want \"%s\"", status.repr,
                wuffs_brotli__error__too_many_compound_dictionaries);
  }
  // A serialized dictionary without a prefix is still OK.
  CHECK_STATUS("add_dictionary #1",
               wuffs_brotli__decoder__add_dictionary(
                   &dec, WUFFS_BROTLI__DICTIONARY_TYPE_SERIALIZED, words_s,
                   workbuf));

  // Only one serialized dictionary can have custom word or transform lists,
  // but a serialized dictionary without them (only a prefix) is OK.
  CHECK_STATUS("initialize", wuffs_brotli__decoder__initialize(
                                 &dec, sizeof dec, WUFFS_VERSION,
                                 WUFFS_INITIALIZE__DEFAULT_OPTIONS));
  CHECK_STATUS("add_dictionary #2",
               wuffs_brotli__decoder__add_dictionary(
                   &dec, WUFFS_BROTLI__DICTIONARY_TYPE_SERIALIZED, full_s,
                   workbuf));
  status = wuffs_brotli__decoder__add_dictionary(
      &dec, WUFFS_BROTLI__DICTIONARY_TYPE_SERIALIZED, words_s, workbuf);
  if (status.repr !=
      wuffs_brotli__error__unsupported_multiple_custom_static_dictionaries) {
    RETURN_FAIL(
        "words after full: have \"%s\", want \"%s\"", status.repr,
        wuffs_brotli__error__unsupported_multiple_custom_static_dictionaries);
  }
  CHECK_STATUS("add_dictionary #3",
               wuffs_brotli__decoder__add_dictionary(
                   &dec, WUFFS_BROTLI__DICTIONARY_TYPE_SERIALIZED, prefix_s,
                   workbuf));
  // Attaching a prefix-only serialized dictionary resets the static
  // dictionary to the built-in one, so custom lists can be attached again.
  CHECK_STATUS("add_dictionary #4",
               wuffs_brotli__decoder__add_dictionary(
                   &dec, WUFFS_BROTLI__DICTIONARY_TYPE_SERIALIZED, words_s,
                   workbuf));

  // Invalid serialized dictionaries: every proper prefix of a valid one, and
  // a bad magic number. A failed add_dictionary does not change the
  // workbuf_len.
  CHECK_STATUS("initialize", wuffs_brotli__decoder__initialize(
                                 &dec, sizeof dec, WUFFS_VERSION,
                                 WUFFS_INITIALIZE__DEFAULT_OPTIONS));
  for (size_t i = 0; i < full_s.len; i++) {
    status = wuffs_brotli__decoder__add_dictionary(
        &dec, WUFFS_BROTLI__DICTIONARY_TYPE_SERIALIZED,
        wuffs_base__make_slice_u8(full_s.ptr, i), workbuf);
    if (status.repr != wuffs_brotli__error__bad_shared_dictionary) {
      RETURN_FAIL("prefix length %zu: have \"%s\", want \"%s\"", i,
                  status.repr, wuffs_brotli__error__bad_shared_dictionary);
    }
  }
  full_s.ptr[0] ^= 1;
  status = wuffs_brotli__decoder__add_dictionary(
      &dec, WUFFS_BROTLI__DICTIONARY_TYPE_SERIALIZED, full_s, workbuf);
  full_s.ptr[0] ^= 1;
  if (status.repr != wuffs_brotli__error__bad_shared_dictionary) {
    RETURN_FAIL("bad magic: have \"%s\", want \"%s\"", status.repr,
                wuffs_brotli__error__bad_shared_dictionary);
  }
  if (wuffs_brotli__decoder__workbuf_len(&dec).max_incl !=
      WUFFS_BROTLI__DECODER_WORKBUF_LEN_STANDARD) {
    RETURN_FAIL("workbuf_len changed");
  }

  // A too-short workbuf.
  status = wuffs_brotli__decoder__add_dictionary(
      &dec, WUFFS_BROTLI__DICTIONARY_TYPE_RAW, raw_s,
      wuffs_base__make_slice_u8(workbuf.ptr, 99));
  if (status.repr != wuffs_base__error__bad_workbuf_length) {
    RETURN_FAIL("short workbuf: have \"%s\", want \"%s\"", status.repr,
                wuffs_base__error__bad_workbuf_length);
  }
  CHECK_STATUS("add_dictionary #5",
               wuffs_brotli__decoder__add_dictionary(
                   &dec, WUFFS_BROTLI__DICTIONARY_TYPE_RAW, raw_s,
                   wuffs_base__make_slice_u8(workbuf.ptr, 100)));

  // A transform_io workbuf that is shorter than the dictionaries.
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  wuffs_base__io_buffer have = ((wuffs_base__io_buffer){
      .data = g_have_slice_u8,
  });
  CHECK_STRING(read_file(&src, "test/data/brotli/romeo.txt.q11w22.br"));
  status = wuffs_brotli__decoder__transform_io(
      &dec, &have, &src, wuffs_base__make_slice_u8(workbuf.ptr, 99));
  if (status.repr != wuffs_base__error__bad_workbuf_length) {
    RETURN_FAIL("transform_io: have \"%s\", want \"%s\"", status.repr,
                wuffs_base__error__bad_workbuf_length);
  }

  // Dictionaries cannot be added after decoding has started.
  CHECK_STATUS("initialize", wuffs_brotli__decoder__initialize(
                                 &dec, sizeof dec, WUFFS_VERSION,
                                 WUFFS_INITIALIZE__DEFAULT_OPTIONS));
  src.meta.ri = 0;
  src.meta.wi = 0;
  src.meta.closed = false;
  // With no input, decoding has not started (the C code is still in its
  // BROTLI_STATE_UNINITED state).
  status = wuffs_brotli__decoder__transform_io(&dec, &have, &src, workbuf);
  if (status.repr != wuffs_base__suspension__short_read) {
    RETURN_FAIL("transform_io #0: have \"%s\", want \"%s\"", status.repr,
                wuffs_base__suspension__short_read);
  }
  CHECK_STATUS("add_dictionary #6",
               wuffs_brotli__decoder__add_dictionary(
                   &dec, WUFFS_BROTLI__DICTIONARY_TYPE_RAW, raw_s, workbuf));
  CHECK_STRING(read_file(&src, "test/data/brotli/romeo.txt.q11w22.br"));
  src.meta.wi = 1;
  src.meta.closed = false;
  status = wuffs_brotli__decoder__transform_io(&dec, &have, &src, workbuf);
  if (status.repr != wuffs_base__suspension__short_read) {
    RETURN_FAIL("transform_io #1: have \"%s\", want \"%s\"", status.repr,
                wuffs_base__suspension__short_read);
  }
  status = wuffs_brotli__decoder__add_dictionary(
      &dec, WUFFS_BROTLI__DICTIONARY_TYPE_RAW, raw_s, workbuf);
  if (status.repr != wuffs_base__error__bad_call_sequence) {
    RETURN_FAIL("after start: have \"%s\", want \"%s\"", status.repr,
                wuffs_base__error__bad_call_sequence);
  }
  return NULL;
}

// ---------------- Large Window and Dictionary Mimic Tests

#ifdef WUFFS_MIMIC

// do_test_mimic_brotli_differential_with_options is like
// do_test_mimic_brotli_differential but with the large-window quirk and
// attached dictionaries. It sets *skipped (and returns NULL) if libbrotlidec
// cannot attach the dictionaries: by default, it does not support serialized
// dictionaries.
const char*  //
do_test_mimic_brotli_differential_with_options(const char* name,
                                               wuffs_base__io_buffer* src,
                                               const char* want_filename,
                                               bool want_error,
                                               bool large_window,
                                               size_t dict_set,
                                               bool* skipped) {
  *skipped = false;
  wuffs_base__io_buffer have = ((wuffs_base__io_buffer){
      .data = g_have_slice_u8,
  });
  wuffs_base__io_buffer want = ((wuffs_base__io_buffer){
      .data = g_want_slice_u8,
  });
  wuffs_base__io_buffer src0 = *src;
  const char* have_status = wuffs_brotli_decode_with_options(
      &have, &src0, UINT64_MAX, UINT64_MAX, large_window, dict_set);

  size_t num_dicts = 0;
  uint32_t dict_types[BROTLI_DICT_SPECS_MAX] = {0};
  wuffs_base__slice_u8 dicts[BROTLI_DICT_SPECS_MAX] = {0};
  CHECK_STRING(load_brotli_dict_set(dict_set, &num_dicts, dict_types, dicts));
  wuffs_base__io_buffer src1 = *src;
  int error_code = 0;
  const char* want_status = mimic_brotli_decode_with_options(
      &want, &src1, large_window, num_dicts, dict_types, dicts, &error_code);
  if (want_status == mimic_brotli_attach_dictionary_failed_message) {
    *skipped = true;
    return NULL;
  }

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

  // Also check the golden file.
  if (want_filename) {
    wuffs_base__io_buffer golden = ((wuffs_base__io_buffer){
        .data = g_want_slice_u8,
    });
    CHECK_STRING(read_file(&golden, want_filename));
    status = check_io_buffers_equal("", &have, &golden);
    if (status) {
      RETURN_FAIL("%s: golden: %s", name, status);
    }
  }
  return NULL;
}

const char*  //
test_mimic_brotli_decode_ext_differential() {
  CHECK_FOCUS(__func__);
  size_t n = WUFFS_TESTLIB_ARRAY_SIZE(g_brotli_ext_vectors);
  size_t num_skipped = 0;
  for (size_t i = 0; i < n; i++) {
    wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
        .data = g_src_slice_u8,
    });
    CHECK_STRING(read_file(&src, g_brotli_ext_vectors[i].src_filename));
    bool skipped = false;
    CHECK_STRING(do_test_mimic_brotli_differential_with_options(
        g_brotli_ext_vectors[i].src_filename, &src,
        g_brotli_ext_vectors[i].want_filename, false,
        g_brotli_ext_vectors[i].large_window,
        g_brotli_ext_vectors[i].dict_set, &skipped));
    num_skipped += skipped ? 1 : 0;
  }
  // Only the serialized dictionary files (6 of them) can be skipped.
  if (num_skipped > 6) {
    RETURN_FAIL("too many skipped: %zu", num_skipped);
  }
  return NULL;
}

const char*  //
test_mimic_brotli_decode_ext_malformed() {
  CHECK_FOCUS(__func__);
  size_t n = WUFFS_TESTLIB_ARRAY_SIZE(g_brotli_ext_malformed);
  for (size_t i = 0; i < n; i++) {
    wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
        .data = g_src_slice_u8,
    });
    if (g_brotli_ext_malformed[i].src_filename) {
      CHECK_STRING(read_file(&src, g_brotli_ext_malformed[i].src_filename));
    } else {
      memcpy(src.data.ptr, g_brotli_ext_malformed[i].data,
             g_brotli_ext_malformed[i].len);
      src.meta.wi = g_brotli_ext_malformed[i].len;
      src.meta.closed = true;
    }
    char name[64];
    snprintf(name, sizeof name, "g_brotli_ext_malformed[%zu]", i);
    bool skipped = false;
    CHECK_STRING(do_test_mimic_brotli_differential_with_options(
        name, &src, NULL, true, g_brotli_ext_malformed[i].large_window,
        g_brotli_ext_malformed[i].dict_set, &skipped));
  }
  return NULL;
}

// mimic_brotli_decode_large_window adapts mimic_brotli_decode_with_options
// to do_test_brotli_decode_large_window_far's codec_func type.
const char*  //
mimic_brotli_decode_large_window(wuffs_base__io_buffer* dst,
                                 wuffs_base__io_buffer* src,
                                 uint64_t wlimit,
                                 uint64_t rlimit,
                                 bool large_window,
                                 size_t dict_set) {
  if ((wlimit < UINT64_MAX) || (rlimit < UINT64_MAX) ||
      (dict_set != BROTLI_DICT_SET_NONE)) {
    return "unsupported option";
  }
  int error_code = 0;
  return mimic_brotli_decode_with_options(dst, src, large_window, 0, NULL,
                                          NULL, &error_code);
}

const char*  //
test_mimic_brotli_decode_large_window_far() {
  CHECK_FOCUS(__func__);
  return do_test_brotli_decode_large_window_far(
      mimic_brotli_decode_large_window, UINT64_MAX, UINT64_MAX);
}

#endif  // WUFFS_MIMIC

// ---------------- Brotli Large Window Encoder Tests

// do_test_brotli_encode_large_window encodes data[:data_len] with the plan's
// config c (which should set the large-window quirk) and checks that
// decoding the result (with Wuffs' std/brotli decoder with the large-window
// quirk set, and, if mimic is true, with libbrotlidec) gives back the
// original data. If mimic is true, it also checks that libbrotlienc's output
// is the same. If max_compressed_len is non-zero, it checks that the
// compressed length is at most that. The decoded data goes in g_want.
const char*  //
do_test_brotli_encode_large_window(const char* name,
                                   const brotli_enc_config* c,
                                   const uint8_t* data,
                                   size_t data_len,
                                   bool mimic,
                                   size_t max_compressed_len) {
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(make_brotli_enc_plan(&src, c, data, data_len));
  wuffs_base__io_buffer have = ((wuffs_base__io_buffer){
      .data = g_have_slice_u8,
  });
  wuffs_base__io_buffer src0 = src;
  const char* status = wuffs_brotli_encode_plan(
      &have, &src0, c->param_keys, c->param_values, c->num_params,
      g_brotli_enc_step_ops, g_brotli_enc_step_lens, g_brotli_enc_num_steps,
      c->dst_chunk);
  if (status) {
    RETURN_FAIL("%s: wuffs encode: %s", name, status);
  } else if (max_compressed_len && (have.meta.wi > max_compressed_len)) {
    RETURN_FAIL("%s: compressed length: have %zu, want at most %zu", name,
                have.meta.wi, max_compressed_len);
  }
  have.meta.closed = true;

  for (int decoder = 0; decoder < (mimic ? 3 : 1); decoder++) {
    wuffs_base__io_buffer want = ((wuffs_base__io_buffer){
        .data = g_want_slice_u8,
    });
    wuffs_base__io_buffer have1 = have;
    if (decoder == 0) {
      status = wuffs_brotli_decode_with_options(
          &want, &have1, UINT64_MAX, UINT64_MAX, true, BROTLI_DICT_SET_NONE);
#ifdef WUFFS_MIMIC
    } else if (decoder == 1) {
      status = mimic_brotli_decode_large_window(
          &want, &have1, UINT64_MAX, UINT64_MAX, true, BROTLI_DICT_SET_NONE);
    } else {
      wuffs_base__io_buffer src1 = src;
      status = mimic_brotli_encode(
          &want, &src1, c->param_keys, c->param_values, c->num_params,
          g_brotli_enc_step_ops, g_brotli_enc_step_lens,
          g_brotli_enc_num_steps, c->dst_chunk);
      if (status) {
        RETURN_FAIL("%s: mimic encode: %s", name, status);
      }
      status = check_io_buffers_equal("", &have, &want);
      if (status) {
        RETURN_FAIL("%s: %s", name, status);
      }
      continue;
#endif  // WUFFS_MIMIC
    }
    const char* which = (decoder == 0) ? "wuffs" : "mimic";
    if (status) {
      RETURN_FAIL("%s: %s decode: %s", name, which, status);
    } else if (have1.meta.ri != have1.meta.wi) {
      RETURN_FAIL("%s: %s decode: trailing data", name, which);
    } else if (want.meta.wi != data_len) {
      RETURN_FAIL("%s: %s decoded length: have %zu, want %zu", name, which,
                  want.meta.wi, data_len);
    } else if (memcmp(want.data.ptr, data, data_len)) {
      RETURN_FAIL("%s: %s decoded bytes differ", name, which);
    }
  }
  return NULL;
}

// do_test_brotli_encode_large_window_files runs a grid of large-window
// configurations (qualities 3 to 9, lgwin 18 to 30) over a few small files.
const char*  //
do_test_brotli_encode_large_window_files(bool mimic) {
  static const char* filenames[] = {
      "test/data/romeo.txt",
      "test/data/brotli/alice29.txt",
      "test/data/brotli/random_org_10k.bin",
      "test/data/brotli/quickfox_repeated",
  };
  static const uint32_t lgwins[] = {18, 25, 26, 28, 30};
  wuffs_base__io_buffer data = ((wuffs_base__io_buffer){
      .data = g_work_slice_u8,
  });
  for (size_t f = 0; f < WUFFS_TESTLIB_ARRAY_SIZE(filenames); f++) {
    // g_work is free to use for the input: each config's workbuf (of more
    // than 64 MiB) is malloc'ed.
    data.meta.wi = 0;
    data.meta.ri = 0;
    data.meta.closed = false;
    CHECK_STRING(read_file(&data, filenames[f]));
    uint8_t* copy = malloc(data.meta.wi ? data.meta.wi : 1);
    if (!copy) {
      return "malloc failed";
    }
    memcpy(copy, data.data.ptr, data.meta.wi);
    const char* ret = NULL;
    for (uint32_t q = 3; (q <= 9) && !ret; q++) {
      for (size_t w = 0; (w < 5) && !ret; w++) {
        for (int k = 0; (k < 2) && !ret; k++) {
          if ((k == 1) && (lgwins[w] != 26)) {
            continue;
          }
          brotli_enc_config c = make_brotli_enc_config(q, lgwins[w]);
          brotli_enc_config__add_param(&c, BROTLI_ENC_PARAM_LARGE_WINDOW, 1);
          if (k == 1) {
            // A big size hint selects H55 (at quality 4) and H65 (at
            // qualities 5 to 9).
            brotli_enc_config__add_param(&c, BROTLI_ENC_PARAM_SIZE_HINT,
                                         1 << 20);
          }
          c.chunking = (w & 1) ? BROTLI_ENC_CHUNK_SINGLE
                               : BROTLI_ENC_CHUNK_RANDOM;
          c.flush = (w & 1) ? BROTLI_ENC_FLUSH_NONE : BROTLI_ENC_FLUSH_MIDDLE;
          c.seed = (q << 8) | w;
          char s[256];
          snprintf(s, sizeof s, "%s: q=%u lgwin=%u size_hint=%s",
                   filenames[f], q, lgwins[w], k ? "1<<20" : "0");
          ret = do_test_brotli_encode_large_window(s, &c, copy, data.meta.wi,
                                                   mimic, 0);
        }
      }
    }
    free(copy);
    if (ret) {
      return ret;
    }
  }
  return NULL;
}

// make_brotli_enc_far_data returns a malloc'ed buffer of pseudo-random bytes
// with two 2 MiB repeats at distances of (20 MiB + 64) and (40 MiB - 4096).
// The repeats' sources are not repeated elsewhere, so only an encoder with a
// large enough window (lgwin 25 or more for the first, 26 or more for the
// second) can compress it.
uint8_t*  //
make_brotli_enc_far_data(size_t* len) {
  const size_t mib = 1 << 20;
  const size_t n = (46 * mib) + 12345;
  uint8_t* p = malloc(n);
  if (!p) {
    return NULL;
  }
  brotli_enc_fill_random(p, n, 0xFA12);
  memcpy(p + (21 * mib) + 64, p + (1 * mib), 2 * mib);
  memcpy(p + (44 * mib) - 4096, p + (4 * mib), 2 * mib);
  *len = n;
  return p;
}

const char*  //
do_test_brotli_encode_large_window_far(bool mimic) {
  size_t len = 0;
  uint8_t* data = make_brotli_enc_far_data(&len);
  if (!data) {
    return "malloc failed";
  }
  static const struct {
    uint32_t quality;
    uint32_t lgwin;
    uint32_t size_hint;
    uint32_t chunking;
    size_t num_repeats;
  } configs[] = {
      // H35.
      {3, 25, 0, BROTLI_ENC_CHUNK_SINGLE, 1},
      {3, 30, 0, BROTLI_ENC_CHUNK_RANDOM, 2},
      // H55.
      {4, 26, 1 << 30, BROTLI_ENC_CHUNK_SINGLE, 2},
      // H65.
      {5, 30, 1 << 30, BROTLI_ENC_CHUNK_RANDOM, 2},
      {7, 28, 0, BROTLI_ENC_CHUNK_SINGLE, 2},
      {9, 26, 0, BROTLI_ENC_CHUNK_SINGLE, 2},
  };
  const char* ret = NULL;
  for (size_t i = 0; (i < WUFFS_TESTLIB_ARRAY_SIZE(configs)) && !ret; i++) {
    brotli_enc_config c =
        make_brotli_enc_config(configs[i].quality, configs[i].lgwin);
    brotli_enc_config__add_param(&c, BROTLI_ENC_PARAM_LARGE_WINDOW, 1);
    if (configs[i].size_hint) {
      brotli_enc_config__add_param(&c, BROTLI_ENC_PARAM_SIZE_HINT,
                                   configs[i].size_hint);
    }
    c.chunking = configs[i].chunking;
    c.flush = (configs[i].chunking == BROTLI_ENC_CHUNK_RANDOM)
                  ? BROTLI_ENC_FLUSH_MIDDLE
                  : BROTLI_ENC_FLUSH_NONE;
    c.seed = i;
    char s[256];
    snprintf(s, sizeof s, "far data: q=%u lgwin=%u", configs[i].quality,
             configs[i].lgwin);
    // Pseudo-random data is incompressible, except for the repeats (of
    // 2 MiB each), which must be found (mostly) to meet this limit.
    size_t max_compressed_len = len - (configs[i].num_repeats * (3 << 19));
    ret = do_test_brotli_encode_large_window(s, &c, data, len, mimic,
                                             max_compressed_len);
  }
  free(data);
  return ret;
}

const char*  //
test_wuffs_brotli_encode_large_window_files() {
  CHECK_FOCUS(__func__);
  return do_test_brotli_encode_large_window_files(false);
}

const char*  //
test_wuffs_brotli_encode_large_window_far() {
  CHECK_FOCUS(__func__);
  return do_test_brotli_encode_large_window_far(false);
}

#ifdef WUFFS_MIMIC

const char*  //
test_mimic_brotli_encode_large_window_files() {
  CHECK_FOCUS(__func__);
  return do_test_brotli_encode_large_window_files(true);
}

const char*  //
test_mimic_brotli_encode_large_window_far() {
  CHECK_FOCUS(__func__);
  return do_test_brotli_encode_large_window_far(true);
}

#endif  // WUFFS_MIMIC

// ---------------- Brotli Encoder Dictionary Tests

// brotli_enc_load_file returns a malloc'ed copy of the named file (of up to
// 1 MiB) and sets *len to its length.
uint8_t*  //
brotli_enc_load_file(const char* filename, size_t* len) {
  uint8_t* buf = malloc(1 << 20);
  if (!buf) {
    return NULL;
  }
  wuffs_base__io_buffer b = ((wuffs_base__io_buffer){
      .data = wuffs_base__make_slice_u8(buf, 1 << 20),
  });
  if (read_file(&b, filename)) {
    free(buf);
    return NULL;
  }
  *len = b.meta.wi;
  return buf;
}

// brotli_enc_config_has_param returns whether c sets the key param to a
// non-zero value.
bool  //
brotli_enc_config_has_param(const brotli_enc_config* c, uint32_t key) {
  bool ret = false;
  for (size_t j = 0; j < c->num_params; j++) {
    if (c->param_keys[j] == key) {
      ret = c->param_values[j] != 0;
    }
  }
  return ret;
}

// wuffs_brotli_decode_with_raw_dictionaries decodes src to dst with Wuffs'
// std/brotli decoder, with the given raw dictionaries attached.
const char*  //
wuffs_brotli_decode_with_raw_dictionaries(wuffs_base__io_buffer* dst,
                                          wuffs_base__io_buffer* src,
                                          bool large_window,
                                          size_t num_dicts,
                                          const wuffs_base__slice_u8* dicts) {
  wuffs_brotli__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_brotli__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
  CHECK_STATUS("set_quirk",
               wuffs_brotli__decoder__set_quirk(
                   &dec, WUFFS_BROTLI__QUIRK_LARGE_WINDOW, large_window));
  size_t total = 0;
  for (size_t j = 0; j < num_dicts; j++) {
    total += dicts[j].len;
  }
  size_t n = total + (large_window ? (1 << 30) : (1 << 24)) + 542;
  uint8_t* p = malloc(n);
  if (!p) {
    return "malloc failed";
  }
  wuffs_base__slice_u8 workbuf = wuffs_base__make_slice_u8(p, n);
  const char* ret = NULL;
  for (size_t j = 0; j < num_dicts; j++) {
    ret = wuffs_brotli__decoder__add_dictionary(
              &dec, WUFFS_BROTLI__DICTIONARY_TYPE_RAW, dicts[j], workbuf)
              .repr;
    if (ret) {
      goto cleanup;
    }
  }
  if (wuffs_brotli__decoder__workbuf_len(&dec).max_incl > n) {
    ret = "decoder workbuf_len is too large";
    goto cleanup;
  }
  ret = wuffs_brotli__decoder__transform_io(&dec, dst, src, workbuf).repr;
cleanup:
  free(p);
  return ret;
}

// do_test_brotli_encode_dictionary encodes data[:data_len] with the plan's
// config c and the given raw dictionaries. Unless the config sets a stream
// offset (whose output is the continuation of another stream), it checks
// that Wuffs' decoder (and, if mimic is true, libbrotlidec), with the same
// dictionaries, decodes the result back to data. If mimic is true, it also
// checks that libbrotlienc's output is the same. It sets *compressed_len.
const char*  //
do_test_brotli_encode_dictionary(const char* name,
                                 const brotli_enc_config* c,
                                 const uint8_t* data,
                                 size_t data_len,
                                 size_t num_dicts,
                                 const wuffs_base__slice_u8* dicts,
                                 bool mimic,
                                 size_t* compressed_len) {
  uint32_t dict_types[16] = {0};
  for (size_t j = 0; j < 16; j++) {
    dict_types[j] = WUFFS_BROTLI__DICTIONARY_TYPE_RAW;
  }
  if (num_dicts > 16) {
    return "too many dictionaries";
  }
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(make_brotli_enc_plan(&src, c, data, data_len));
  wuffs_base__io_buffer have = ((wuffs_base__io_buffer){
      .data = g_have_slice_u8,
  });
  wuffs_base__io_buffer src0 = src;
  const char* status = wuffs_brotli_encode_plan_with_dictionaries(
      &have, &src0, c->param_keys, c->param_values, c->num_params, num_dicts,
      dict_types, dicts, g_brotli_enc_step_ops, g_brotli_enc_step_lens,
      g_brotli_enc_num_steps, c->dst_chunk);
  if (status) {
    RETURN_FAIL("%s: wuffs encode: %s", name, status);
  }
  have.meta.closed = true;
  *compressed_len = have.meta.wi;
  bool lw = brotli_enc_config_has_param(c, BROTLI_ENC_PARAM_LARGE_WINDOW);
  bool so = brotli_enc_config_has_param(c, BROTLI_ENC_PARAM_STREAM_OFFSET);

  for (int k = 0; k < (mimic ? 3 : 1); k++) {
    wuffs_base__io_buffer want = ((wuffs_base__io_buffer){
        .data = g_want_slice_u8,
    });
    wuffs_base__io_buffer have1 = have;
    if (k == 0) {
      if (so) {
        continue;
      }
      status = wuffs_brotli_decode_with_raw_dictionaries(&want, &have1, lw,
                                                         num_dicts, dicts);
#ifdef WUFFS_MIMIC
    } else if (k == 1) {
      if (so) {
        continue;
      }
      int error_code = 0;
      status = mimic_brotli_decode_with_options(
          &want, &have1, lw, num_dicts, dict_types, dicts, &error_code);
    } else {
      wuffs_base__io_buffer src1 = src;
      status = mimic_brotli_encode_with_dictionaries(
          &want, &src1, c->param_keys, c->param_values, c->num_params,
          num_dicts, dict_types, dicts, g_brotli_enc_step_ops,
          g_brotli_enc_step_lens, g_brotli_enc_num_steps, c->dst_chunk);
      if (status) {
        RETURN_FAIL("%s: mimic encode: %s", name, status);
      }
      status = check_io_buffers_equal("", &have, &want);
      if (status) {
        RETURN_FAIL("%s: %s", name, status);
      }
      continue;
#endif  // WUFFS_MIMIC
    }
    const char* which = (k == 0) ? "wuffs" : "mimic";
    if (status) {
      RETURN_FAIL("%s: %s decode: %s", name, which, status);
    } else if (have1.meta.ri != have1.meta.wi) {
      RETURN_FAIL("%s: %s decode: trailing data", name, which);
    } else if (want.meta.wi != data_len) {
      RETURN_FAIL("%s: %s decoded length: have %zu, want %zu", name, which,
                  want.meta.wi, data_len);
    } else if (memcmp(want.data.ptr, data, data_len)) {
      RETURN_FAIL("%s: %s decoded bytes differ", name, which);
    }
  }
  return NULL;
}

const char*  //
test_wuffs_brotli_encode_add_dictionary() {
  CHECK_FOCUS(__func__);
  static const uint8_t text[] = "the quick brown fox jumps over the lazy dog";
  wuffs_base__slice_u8 dict =
      wuffs_base__make_slice_u8((uint8_t*)text, sizeof text - 1);
  wuffs_base__slice_u8 workbuf = g_work_slice_u8;

  wuffs_brotli__encoder enc;
  CHECK_STATUS("initialize",
               wuffs_brotli__encoder__initialize(
                   &enc, sizeof enc, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
  CHECK_STATUS("set_quirk", wuffs_brotli__encoder__set_quirk(
                                &enc, WUFFS_BROTLI__QUIRK_ENCODER_QUALITY, 5));
  uint64_t wlen0 = wuffs_brotli__encoder__workbuf_len(&enc).max_incl;

  // Serialized dictionaries are rejected (like libbrotlienc's default,
  // non-experimental build) and so are unknown dictionary types.
  const char* have = wuffs_brotli__encoder__add_dictionary(
                         &enc, WUFFS_BROTLI__DICTIONARY_TYPE_SERIALIZED, dict,
                         workbuf)
                         .repr;
  if (have != wuffs_brotli__error__unsupported_serialized_encoder_dictionary) {
    RETURN_FAIL("serialized: have \"%s\"", have);
  }
  have = wuffs_brotli__encoder__add_dictionary(&enc, 2, dict, workbuf).repr;
  if (have != wuffs_base__error__bad_argument) {
    RETURN_FAIL("unknown type: have \"%s\"", have);
  }

  // The add_dictionary_workbuf_len formula: for a dictionary of n bytes (and
  // 17 bucket bits and 7 slot bits, for n up to 2 MiB), n bytes, (4 << 7)
  // slot offsets, (2 << 17) heads and up to 4 * (n - 7) items, then (6 <<
  // 17) + (4 * n) bytes of scratch space.
  uint64_t want_wlen =
      43 + (4 << 7) + (2 << 17) + (4 * 36) + (6 << 17) + (4 * 43);
  uint64_t have_wlen =
      wuffs_brotli__encoder__add_dictionary_workbuf_len(&enc, dict.len);
  if (have_wlen != want_wlen) {
    RETURN_FAIL("add_dictionary_workbuf_len: have %" PRIu64 ", want %" PRIu64,
                have_wlen, want_wlen);
  }
  workbuf.len = (size_t)(have_wlen - 1);
  have = wuffs_brotli__encoder__add_dictionary(
             &enc, WUFFS_BROTLI__DICTIONARY_TYPE_RAW, dict, workbuf)
             .repr;
  if (have != wuffs_base__error__bad_workbuf_length) {
    RETURN_FAIL("short workbuf: have \"%s\"", have);
  }
  if (wuffs_brotli__encoder__workbuf_len(&enc).max_incl != wlen0) {
    RETURN_FAIL("workbuf_len changed after failed add_dictionary calls");
  }

  // 15 dictionaries (the C code's SHARED_BROTLI_MAX_COMPOUND_DICTS), one of
  // them empty, can be added, but not 16.
  uint64_t dict_end = 0;
  for (int i = 0; i < 16; i++) {
    wuffs_base__slice_u8 d = dict;
    if (i == 3) {
      d.len = 0;
    }
    workbuf.len = g_work_slice_u8.len;
    uint64_t n = wuffs_brotli__encoder__add_dictionary_workbuf_len(&enc, d.len);
    if (n != (dict_end + ((i == 3) ? ((4 << 7) + (8 << 17)) : want_wlen))) {
      RETURN_FAIL("i=%d: add_dictionary_workbuf_len: have %" PRIu64, i, n);
    }
    have = wuffs_brotli__encoder__add_dictionary(
               &enc, WUFFS_BROTLI__DICTIONARY_TYPE_RAW, d, workbuf)
               .repr;
    if (i == 15) {
      if (have != wuffs_brotli__error__too_many_compound_dictionaries) {
        RETURN_FAIL("i=%d: have \"%s\"", i, have);
      }
      break;
    } else if (have) {
      RETURN_FAIL("i=%d: have \"%s\"", i, have);
    }
    // The dictionary bytes, (4 << 7) slot offsets, (2 << 17) heads and 4
    // bytes per item. Each of the (43 - 7) positions of "the quick brown fox
    // etc" has a distinct hash, so there are 36 items.
    dict_end += (i == 3) ? ((4 << 7) + (2 << 17))
                         : (43 + (4 << 7) + (2 << 17) + (4 * 36));
    uint64_t wlen = wuffs_brotli__encoder__workbuf_len(&enc).max_incl;
    if (wlen != (wlen0 + dict_end)) {
      RETURN_FAIL("i=%d: workbuf_len: have %" PRIu64 ", want %" PRIu64, i,
                  wlen, wlen0 + dict_end);
    }
  }

  // Quirks can still be set after add_dictionary, until transform_io.
  CHECK_STATUS("set_quirk", wuffs_brotli__encoder__set_quirk(
                                &enc, WUFFS_BROTLI__QUIRK_LGWIN, 16));
  uint64_t wlen = wuffs_brotli__encoder__workbuf_len(&enc).max_incl;
  if (wlen <= dict_end) {
    RETURN_FAIL("workbuf_len: have %" PRIu64, wlen);
  }

  // A workbuf shorter than the dictionaries fails.
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = wuffs_base__make_slice_u8((uint8_t*)text, sizeof text - 1),
      .meta = wuffs_base__make_io_buffer_meta(sizeof text - 1, 0, 0, true),
  });
  wuffs_base__io_buffer dst = ((wuffs_base__io_buffer){
      .data = g_have_slice_u8,
  });
  // (An error disables the encoder, so try this on a copy.)
  wuffs_brotli__encoder* copy = malloc(sizeof enc);
  if (!copy) {
    return "malloc failed";
  }
  memcpy(copy, &enc, sizeof enc);
  workbuf.len = (size_t)(dict_end - 1);
  have = wuffs_brotli__encoder__transform_io(copy, &dst, &src, workbuf).repr;
  free(copy);
  if (have != wuffs_base__error__bad_workbuf_length) {
    RETURN_FAIL("transform_io (short workbuf): have \"%s\"", have);
  }
  workbuf.len = (size_t)wlen;
  CHECK_STATUS("transform_io",
               wuffs_brotli__encoder__transform_io(&enc, &dst, &src, workbuf));
  if (dst.meta.wi >= (sizeof text - 1)) {
    RETURN_FAIL("compressed length: have %zu", dst.meta.wi);
  }

  // After transform_io, add_dictionary (and set_quirk) fail. (The C code's
  // BrotliEncoderAttachPreparedDictionary accepts a raw dictionary at any
  // time, but attaching one after compression has started changes the
  // compound dictionary's size mid-stream, which is not supported here.)
  have = wuffs_brotli__encoder__add_dictionary(
             &enc, WUFFS_BROTLI__DICTIONARY_TYPE_RAW, dict, workbuf)
             .repr;
  if (have != wuffs_base__error__bad_call_sequence) {
    RETURN_FAIL("after transform_io: have \"%s\"", have);
  }
  return NULL;
}

// brotli_enc_dict_set fills dicts with one of the dictionary test sets (for
// the alice29.txt, asyoulik.txt and romeo.txt files a, b and r, each of
// length a_len, b_len and r_len) and returns how many dictionaries it has.
size_t  //
brotli_enc_dict_set(int which,
                    wuffs_base__slice_u8* dicts,
                    uint8_t* a,
                    size_t a_len,
                    uint8_t* b,
                    size_t b_len,
                    uint8_t* r,
                    size_t r_len) {
  switch (which) {
    case 0:  // One small dictionary.
      dicts[0] = wuffs_base__make_slice_u8(r, r_len);
      return 1;
    case 1:  // The first half of alice29.txt.
      dicts[0] = wuffs_base__make_slice_u8(a, a_len / 2);
      return 1;
    case 2:  // Several dictionaries, including tiny and empty ones.
      dicts[0] = wuffs_base__make_slice_u8(r, 0);
      dicts[1] = wuffs_base__make_slice_u8(b + 1000, 5);
      dicts[2] = wuffs_base__make_slice_u8(a + (a_len / 2), a_len / 4);
      dicts[3] = wuffs_base__make_slice_u8(b, 4000);
      dicts[4] = wuffs_base__make_slice_u8(r + 100, 300);
      return 5;
    case 3:  // 15 (the maximum number of) pieces of alice29.txt.
      for (size_t j = 0; j < 15; j++) {
        dicts[j] = wuffs_base__make_slice_u8(a + (j * 10000), 9000);
      }
      return 15;
  }
  dicts[0] = wuffs_base__make_slice_u8(b, b_len);
  return 1;
}

const char*  //
do_test_brotli_encode_dictionary_grid(bool mimic) {
  size_t a_len = 0;
  size_t b_len = 0;
  size_t r_len = 0;
  uint8_t* a = brotli_enc_load_file("test/data/brotli/alice29.txt", &a_len);
  uint8_t* b = brotli_enc_load_file("test/data/brotli/asyoulik.txt", &b_len);
  uint8_t* r = brotli_enc_load_file("test/data/romeo.txt", &r_len);
  const char* ret = NULL;
  if (!a || !b || !r || (a_len < 150000)) {
    ret = "could not load the test files";
    goto cleanup;
  }

  // Each config is (lgwin, chunking, flush, metadata, extra param, value).
  static const uint32_t configs[][6] = {
      {22, BROTLI_ENC_CHUNK_SINGLE, BROTLI_ENC_FLUSH_NONE, 0, 0, 0},
      {16, BROTLI_ENC_CHUNK_RANDOM, BROTLI_ENC_FLUSH_MIDDLE, 0, 0, 0},
      {10, BROTLI_ENC_CHUNK_RANDOM, BROTLI_ENC_FLUSH_EVERY_CHUNK, 0, 0, 0},
      {18, BROTLI_ENC_CHUNK_RANDOM, BROTLI_ENC_FLUSH_MIDDLE,
       BROTLI_ENC_METADATA_MIDDLE, 0, 0},
      {22, BROTLI_ENC_CHUNK_RANDOM, BROTLI_ENC_FLUSH_MIDDLE, 0,
       BROTLI_ENC_PARAM_STREAM_OFFSET, 1000},
      {24, BROTLI_ENC_CHUNK_SINGLE, BROTLI_ENC_FLUSH_NONE, 0,
       BROTLI_ENC_PARAM_STREAM_OFFSET, 1 << 30},
      {26, BROTLI_ENC_CHUNK_RANDOM, BROTLI_ENC_FLUSH_MIDDLE, 0,
       BROTLI_ENC_PARAM_LARGE_WINDOW, 1},
      {22, BROTLI_ENC_CHUNK_SINGLE, BROTLI_ENC_FLUSH_NONE, 0,
       BROTLI_ENC_PARAM_SIZE_HINT, 1 << 20},
  };
  // The input is the second half of alice29.txt, except for dictionary set
  // 4 (all of asyoulik.txt), whose input is romeo.txt.
  for (int set = 0; (set < 5) && !ret; set++) {
    wuffs_base__slice_u8 dicts[16];
    size_t num_dicts = brotli_enc_dict_set(set, dicts, a, a_len, b, b_len, r,
                                           r_len);
    const uint8_t* data = (set == 4) ? r : (a + (a_len / 2));
    size_t data_len = (set == 4) ? r_len : (a_len - (a_len / 2));
    for (uint32_t q = 0; (q <= 11) && !ret; q++) {
      for (size_t i = 0; (i < WUFFS_TESTLIB_ARRAY_SIZE(configs)) && !ret;
           i++) {
        if (!mimic && (i >= 2) && (q & 1)) {
          continue;
        } else if ((q >= 10) &&
                   ((i == 2) || (i == 7) || (!mimic && (i != 0)) ||
                    ((q == 11) && (i & 1)))) {
          // Qualities 10 and 11 are slow, so they skip some configs.
          continue;
        }
        brotli_enc_config c = make_brotli_enc_config(q, configs[i][0]);
        if (configs[i][4]) {
          brotli_enc_config__add_param(&c, configs[i][4], configs[i][5]);
        }
        c.chunking = configs[i][1];
        c.flush = configs[i][2];
        c.metadata_position = configs[i][3];
        c.metadata_len = configs[i][3] ? 100 : 0;
        c.seed = (set << 16) | (q << 8) | i;
        char s[256];
        snprintf(s, sizeof s, "set=%d q=%u config=%zu", set, q, i);
        size_t have_len = 0;
        ret = do_test_brotli_encode_dictionary(s, &c, data, data_len,
                                               num_dicts, dicts, mimic,
                                               &have_len);
        if (ret || (set != 1) || (q < 5) || (i != 0)) {
          continue;
        }
        // Encoding the second half of alice29.txt with the first half as a
        // dictionary should be at least 5% better than without it.
        size_t without_len = 0;
        ret = do_test_brotli_encode_dictionary(s, &c, data, data_len, 0, NULL,
                                               false, &without_len);
        if (!ret && ((have_len * 100) >= (without_len * 95))) {
          snprintf(g_fail_msg, sizeof g_fail_msg,
                   "%s: compressed length: have %zu, without dictionary %zu",
                   s, have_len, without_len);
          ret = g_fail_msg;
        }
      }
    }
  }

cleanup:
  free(a);
  free(b);
  free(r);
  return ret;
}

const char*  //
do_test_brotli_encode_dictionary_big(bool mimic) {
  // Dictionaries of exactly 2 MiB (the largest with 17 bucket bits), one
  // byte more (18 bucket bits) and 5 and 33 MiB (19 and 22 bucket bits).
  // Each holds, at various offsets, pieces of the 1 MiB input.
  static const size_t sizes[] = {2 << 20, (2 << 20) + 1, (5 << 20) + 3,
                                 (33 << 20) + 5};
  const size_t len = 1 << 20;
  uint8_t* data = malloc(len);
  uint8_t* dict = malloc(sizes[3]);
  const char* ret = NULL;
  if (!data || !dict) {
    ret = "malloc failed";
    goto cleanup;
  }
  brotli_enc_fill_random(data, len, 0xDA7A);
  for (size_t i = 0; i < len; i++) {
    // Make it compressible: mostly lowercase letters.
    data[i] = 'a' + (data[i] % 13);
  }
  for (size_t k = 0; (k < WUFFS_TESTLIB_ARRAY_SIZE(sizes)) && !ret; k++) {
    size_t dlen = sizes[k];
    brotli_enc_fill_random(dict, dlen, 0xD1C7 + k);
    for (size_t j = 0; j < 3; j++) {
      size_t at = (j == 0) ? 999 : (j == 1) ? (dlen / 2) : (dlen - (65 << 10));
      memcpy(dict + at, data + (j * (300 << 10)), 64 << 10);
    }
    wuffs_base__slice_u8 dicts[2] = {
        wuffs_base__make_slice_u8(dict, dlen),
        wuffs_base__make_slice_u8(data + (len - 1000), 1000),
    };
    static const uint32_t qs[] = {2, 4, 5, 9, 10, 11};
    for (size_t i = 0; (i < WUFFS_TESTLIB_ARRAY_SIZE(qs)) && !ret; i++) {
      // Without mimic, and for the largest dictionary, a smaller grid. The
      // slow qualities 10 and 11 only run with mimic, for one dictionary
      // each.
      if (qs[i] >= 10) {
        if (!mimic || (k != ((qs[i] == 10) ? 3 : 1))) {
          continue;
        }
      } else if ((!mimic && ((k & 1) == 0)) ||
                 ((!mimic || (k == 3)) && (i != 2))) {
        continue;
      }
      brotli_enc_config c = make_brotli_enc_config(qs[i], 24);
      if (i & 1) {
        brotli_enc_config__add_param(&c, BROTLI_ENC_PARAM_SIZE_HINT, 1 << 20);
        c.chunking = BROTLI_ENC_CHUNK_RANDOM;
        c.flush = BROTLI_ENC_FLUSH_MIDDLE;
      }
      c.seed = (k << 8) | i;
      char s[256];
      snprintf(s, sizeof s, "dictionary of %zu bytes: q=%u", dlen, qs[i]);
      size_t have_len = 0;
      // At qualities 10 and 11, only the first 128 KiB of the input (which
      // has the first piece) is encoded.
      ret = do_test_brotli_encode_dictionary(
          s, &c, data, (qs[i] >= 10) ? (128 << 10) : len, 2, dicts, mimic,
          &have_len);
      // The three 64 KiB pieces (of 3.7 bits of entropy per byte) should be
      // found at quality 5 and above, even when they are more than 16 MiB
      // (lgwin 24's window) into the dictionary. (At the slow qualities 10
      // and 11, this is left to the parity check.)
      size_t want_max = 0;
      if (!ret && (qs[i] >= 5) && (qs[i] <= 9)) {
        c = make_brotli_enc_config(qs[i], 24);
        ret = do_test_brotli_encode_dictionary(s, &c, data, len, 0, NULL,
                                               false, &want_max);
        want_max -= 3 * (24 << 10);
        if (!ret && (have_len > want_max)) {
          snprintf(g_fail_msg, sizeof g_fail_msg,
                   "%s: compressed length: have %zu, want at most %zu", s,
                   have_len, want_max);
          ret = g_fail_msg;
        }
      }
    }
  }
cleanup:
  free(data);
  free(dict);
  return ret;
}

const char*  //
test_wuffs_brotli_encode_dictionary_big() {
  CHECK_FOCUS(__func__);
  return do_test_brotli_encode_dictionary_big(false);
}

const char*  //
test_wuffs_brotli_encode_dictionary_grid() {
  CHECK_FOCUS(__func__);
  return do_test_brotli_encode_dictionary_grid(false);
}

#ifdef WUFFS_MIMIC

const char*  //
test_mimic_brotli_encode_dictionary_big() {
  CHECK_FOCUS(__func__);
  return do_test_brotli_encode_dictionary_big(true);
}

const char*  //
test_mimic_brotli_encode_dictionary_grid() {
  CHECK_FOCUS(__func__);
  return do_test_brotli_encode_dictionary_grid(true);
}

// test_mimic_brotli_encode_dictionary_rejections checks that the dictionaries
// that libbrotlienc (a default, non-experimental build) rejects are also
// rejected by Wuffs, and that an empty dictionary is accepted by both.
const char*  //
test_mimic_brotli_encode_dictionary_rejections() {
  CHECK_FOCUS(__func__);
  static const uint8_t text[] = "the quick brown fox jumps over the lazy dog";
  // A serialized dictionary (an LZ77 prefix only), which an experimental
  // libbrotlienc build would accept.
  static const uint8_t serialized[] = {0x91, 0x00, 0x03, 'f', 'o', 'x', 0x00,
                                       0x00};
  static const struct {
    size_t num_dicts;
    uint32_t type;  // The last dictionary's type.
    size_t len;     // The last dictionary's length.
    const char* want_wuffs;
    const char* want_mimic;
  } cases[] = {
      {1, WUFFS_BROTLI__DICTIONARY_TYPE_SERIALIZED, sizeof serialized,
       wuffs_brotli__error__unsupported_serialized_encoder_dictionary,
       mimic_brotli_prepare_dictionary_failed_message},
      {16, WUFFS_BROTLI__DICTIONARY_TYPE_RAW, 9,
       wuffs_brotli__error__too_many_compound_dictionaries,
       mimic_brotli_attach_prepared_dictionary_failed_message},
      {15, WUFFS_BROTLI__DICTIONARY_TYPE_RAW, 9, NULL, NULL},
      {1, WUFFS_BROTLI__DICTIONARY_TYPE_RAW, 0, NULL, NULL},
      {3, WUFFS_BROTLI__DICTIONARY_TYPE_RAW, 0, NULL, NULL},
  };
  // Quality 5 and the Zopfli qualities 10 and 11.
  static const uint32_t qs[] = {5, 10, 11};
  for (size_t iq = 0; iq < WUFFS_TESTLIB_ARRAY_SIZE(qs); iq++) {
    for (size_t i = 0; i < WUFFS_TESTLIB_ARRAY_SIZE(cases); i++) {
      uint32_t dict_types[16];
      wuffs_base__slice_u8 dicts[16];
      for (size_t j = 0; j < cases[i].num_dicts; j++) {
        dict_types[j] = WUFFS_BROTLI__DICTIONARY_TYPE_RAW;
        dicts[j] = wuffs_base__make_slice_u8((uint8_t*)text + j, 20);
      }
      size_t last = cases[i].num_dicts - 1;
      dict_types[last] = cases[i].type;
      dicts[last] = wuffs_base__make_slice_u8(
          (uint8_t*)((cases[i].type == WUFFS_BROTLI__DICTIONARY_TYPE_RAW)
                         ? text
                         : serialized),
          cases[i].len);

      brotli_enc_config c = make_brotli_enc_config(qs[iq], 22);
      wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
          .data = g_src_slice_u8,
      });
      CHECK_STRING(make_brotli_enc_plan(&src, &c, text, sizeof text - 1));
      wuffs_base__io_buffer dsts[2] = {
          ((wuffs_base__io_buffer){.data = g_have_slice_u8}),
          ((wuffs_base__io_buffer){.data = g_want_slice_u8}),
      };
      for (int k = 0; k < 2; k++) {
        wuffs_base__io_buffer src1 = src;
        const char* have =
            (k ? mimic_brotli_encode_with_dictionaries
               : wuffs_brotli_encode_plan_with_dictionaries)(
                &dsts[k], &src1, c.param_keys, c.param_values, c.num_params,
                cases[i].num_dicts, dict_types, dicts, g_brotli_enc_step_ops,
                g_brotli_enc_step_lens, g_brotli_enc_num_steps, c.dst_chunk);
        const char* want = k ? cases[i].want_mimic : cases[i].want_wuffs;
        if (have != want) {
          RETURN_FAIL("q=%u cases[%zu] (%s): have \"%s\", want \"%s\"",
                      qs[iq], i, k ? "mimic" : "wuffs", have ? have : "",
                      want ? want : "");
        }
      }
      if (!cases[i].want_wuffs) {
        CHECK_STRING(check_io_buffers_equal("", &dsts[0], &dsts[1]));
      }
    }
  }
  return NULL;
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

    // Large window and dictionaries.
    test_wuffs_brotli_add_dictionary,
    test_wuffs_brotli_decode_ext_all,
    test_wuffs_brotli_decode_ext_all_with_io_limits,
    test_wuffs_brotli_decode_ext_malformed,
    test_wuffs_brotli_decode_large_window_far,
    test_wuffs_brotli_decode_large_window_quirk,

    // Metablock builder tests.
    test_wuffs_brotli_metablock_bit_cost,
    test_wuffs_brotli_metablock_build_meta_block,

#ifdef WUFFS_MIMIC

    test_mimic_brotli_decode_alice29,
    test_mimic_brotli_decode_all,
    test_mimic_brotli_decode_differential,
    test_mimic_brotli_decode_malformed,
    test_mimic_brotli_decode_romeo,
    test_mimic_brotli_decode_split_block_length,

    // Large window and dictionaries.
    test_mimic_brotli_decode_ext_differential,
    test_mimic_brotli_decode_ext_malformed,
    test_mimic_brotli_decode_large_window_far,

#endif  // WUFFS_MIMIC

    // Encoder tests.

    test_wuffs_brotli_encode_add_dictionary,
    test_wuffs_brotli_encode_dictionary_big,
    test_wuffs_brotli_encode_dictionary_grid,
    test_wuffs_brotli_encode_interface,
    test_wuffs_brotli_encode_large_window_far,
    test_wuffs_brotli_encode_large_window_files,
    test_wuffs_brotli_encode_metadata_round_trip,
    test_wuffs_brotli_encode_parameters,
    test_wuffs_brotli_encode_round_trip_corpus,
    test_wuffs_brotli_encode_round_trip_lengths,
    test_wuffs_brotli_encode_round_trip_synthetic,

#ifdef WUFFS_MIMIC

    test_mimic_brotli_encode_dictionary_big,
    test_mimic_brotli_encode_dictionary_grid,
    test_mimic_brotli_encode_dictionary_rejections,
    test_mimic_brotli_encode_large_window_far,
    test_mimic_brotli_encode_large_window_files,
    test_mimic_brotli_encode_parity_corpus,
    test_mimic_brotli_encode_parity_grid,
    test_mimic_brotli_encode_parity_metadata,
    test_mimic_brotli_encode_parity_params,
    test_mimic_brotli_encode_parity_q2_grid,
    test_mimic_brotli_encode_parity_q2_metadata,
    test_mimic_brotli_encode_parity_q2_params,
    test_mimic_brotli_encode_parity_q10_grid,
    test_mimic_brotli_encode_parity_q10_metadata,
    test_mimic_brotli_encode_parity_q10_params,

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
    bench_wuffs_brotli_encode_alice29_q5,
    bench_wuffs_brotli_encode_alice29_q9,

#ifdef WUFFS_MIMIC

    bench_mimic_brotli_encode_alice29_q0,
    bench_mimic_brotli_encode_alice29_q1,
    bench_mimic_brotli_encode_alice29_q5,
    bench_mimic_brotli_encode_alice29_q9,

#endif  // WUFFS_MIMIC

    NULL,
};

int  //
main(int argc, char** argv) {
  g_proc_package_name = "std/brotli";
  return test_main(argc, argv, g_tests, g_benches);
}
