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

#endif  // WUFFS_MIMIC

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

#ifdef WUFFS_MIMIC

    test_mimic_brotli_decode_alice29,
    test_mimic_brotli_decode_all,
    test_mimic_brotli_decode_differential,
    test_mimic_brotli_decode_malformed,
    test_mimic_brotli_decode_romeo,

    // Large window and dictionaries.
    test_mimic_brotli_decode_ext_differential,
    test_mimic_brotli_decode_ext_malformed,
    test_mimic_brotli_decode_large_window_far,

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
