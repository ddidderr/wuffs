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

// ¿ wuffs mimic cflags: -DWUFFS_MIMIC -lbrotlidec -lbrotlicommon

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

    NULL,
};

proc g_benches[] = {

    bench_wuffs_brotli_decode_alice29,

#ifdef WUFFS_MIMIC

    bench_mimic_brotli_decode_alice29,

#endif  // WUFFS_MIMIC

    NULL,
};

int  //
main(int argc, char** argv) {
  g_proc_package_name = "std/brotli";
  return test_main(argc, argv, g_tests, g_benches);
}
