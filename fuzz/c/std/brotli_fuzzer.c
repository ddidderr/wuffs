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

// Silence the nested slash-star warning for the next comment's command line.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcomment"

/*
This fuzzer (the fuzz function) is typically run indirectly, by a framework
such as https://github.com/google/oss-fuzz calling LLVMFuzzerTestOneInput.

When working on the fuzz implementation, or as a coherence check, defining
WUFFS_CONFIG__FUZZLIB_MAIN will let you manually run fuzz over a set of files:

gcc -DWUFFS_CONFIG__FUZZLIB_MAIN brotli_fuzzer.c
./a.out ../../../test/data/brotli/*
rm -f ./a.out

It should print "PASS", amongst other information, and exit(0).
*/

#pragma clang diagnostic pop

// Wuffs ships as a "single file C library" or "header file library" as per
// https://github.com/nothings/stb/blob/master/docs/stb_howto.txt
//
// To use that single file as a "foo.c"-like implementation, instead of a
// "foo.h"-like header, #define WUFFS_IMPLEMENTATION before #include'ing or
// compiling it.
#define WUFFS_IMPLEMENTATION

#if defined(WUFFS_CONFIG__FUZZLIB_MAIN)
// Defining the WUFFS_CONFIG__STATIC_FUNCTIONS macro is optional, but when
// combined with WUFFS_IMPLEMENTATION, it demonstrates making all of Wuffs'
// functions have static storage.
//
// This can help the compiler ignore or discard unused code, which can produce
// faster compiles and smaller binaries. Other motivations are discussed in the
// "ALLOW STATIC IMPLEMENTATION" section of
// https://raw.githubusercontent.com/nothings/stb/master/docs/stb_howto.txt
#define WUFFS_CONFIG__STATIC_FUNCTIONS
#endif  // defined(WUFFS_CONFIG__FUZZLIB_MAIN)

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
#include "../fuzzlib/fuzzlib.c"

// 64 KiB.
#define DST_BUFFER_ARRAY_SIZE 65536

// Wuffs allows either statically or dynamically allocated work buffers. This
// program exercises static allocation.
//
// We hard-code 64 MiB, as WUFFS_BROTLI__DECODER_WORKBUF_LEN_MAX_INCL_WORST_CASE
// is over 3 GiB and even the large-window workbuf_len is over 1 GiB. Using
// those can cause "relocation truncated to fit: R_X86_64_PC32 against symbol
// etc" compiler errors. Large-window streams with a window of 64 MiB or more
// are rejected with "#base: bad workbuf length", but smaller ones are fine.
#define WORK_BUFFER_ARRAY_SIZE 67108864
#if WORK_BUFFER_ARRAY_SIZE > 0
uint8_t g_work_buffer_array[WORK_BUFFER_ARRAY_SIZE];
#else
// Not all C/C++ compilers support 0-length arrays.
uint8_t g_work_buffer_array[1];
#endif

// g_raw_dictionary is a raw (LZ77 prefix) dictionary.
static const uint8_t g_raw_dictionary[] =
    "The quick brown fox jumps over the lazy dog. "
    "<html><head><title>Wuffs</title></head><body></body></html>";

// g_serialized_dictionary is a small serialized shared dictionary. It has an
// LZ77 prefix, a custom word list and a custom transform list (with
// BROTLI_TRANSFORM_SHIFT_FIRST and BROTLI_TRANSFORM_SHIFT_ALL transforms) and
// two context-selected word and transform list combinations.
static const uint8_t g_serialized_dictionary[] = {
    0x91, 0x00, 0x11, 0x77, 0x75, 0x66, 0x66, 0x73, 0x20, 0x62, 0x72, 0x6F,
    0x74, 0x6C, 0x69, 0x20, 0x66, 0x75, 0x7A, 0x7A, 0x01, 0x01, 0x01, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x6F, 0x76, 0x65, 0x72, 0x6C, 0x61, 0x7A, 0x79, 0x71, 0x75, 0x69,
    0x63, 0x6B, 0x62, 0x72, 0x6F, 0x77, 0x6E, 0x77, 0x69, 0x7A, 0x61, 0x72,
    0x64, 0x73, 0x71, 0x75, 0x69, 0x65, 0x74, 0x6C, 0x79, 0x01, 0x06, 0x00,
    0x01, 0x20, 0x02, 0xC3, 0xA9, 0x00, 0x06, 0x02, 0x00, 0x02, 0x02, 0x15,
    0x00, 0x01, 0x16, 0x02, 0x02, 0x0B, 0x01, 0x00, 0x0E, 0x00, 0x02, 0x15,
    0x02, 0x00, 0x00, 0x41, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x80, 0x02, 0x00, 0x00, 0x01, 0x01, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
    0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
    0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
    0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
    0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
    0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01,
};

const char*  //
fuzz(wuffs_base__io_buffer* src, uint64_t hash) {
  wuffs_brotli__decoder dec;
  wuffs_base__status status = wuffs_brotli__decoder__initialize(
      &dec, sizeof dec, WUFFS_VERSION,
      (hash & 1) ? WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED : 0);
  if (!wuffs_base__status__is_ok(&status)) {
    return wuffs_base__status__message(&status);
  }

  // Enable large-window brotli for half of the inputs.
  wuffs_brotli__decoder__set_quirk(&dec, WUFFS_BROTLI__QUIRK_LARGE_WINDOW,
                                   (hash >> 1) & 1);

  wuffs_base__slice_u8 workbuf =
      wuffs_base__make_slice_u8(g_work_buffer_array, WORK_BUFFER_ARRAY_SIZE);

  // Attach dictionaries for some of the inputs.
  switch ((hash >> 2) & 7) {
    case 1:
      status = wuffs_brotli__decoder__add_dictionary(
          &dec, WUFFS_BROTLI__DICTIONARY_TYPE_RAW,
          wuffs_base__make_slice_u8((uint8_t*)g_raw_dictionary,
                                    sizeof g_raw_dictionary - 1),
          workbuf);
      break;
    case 2:
      status = wuffs_brotli__decoder__add_dictionary(
          &dec, WUFFS_BROTLI__DICTIONARY_TYPE_SERIALIZED,
          wuffs_base__make_slice_u8((uint8_t*)g_serialized_dictionary,
                                    sizeof g_serialized_dictionary),
          workbuf);
      break;
    case 3:
      status = wuffs_brotli__decoder__add_dictionary(
          &dec, WUFFS_BROTLI__DICTIONARY_TYPE_SERIALIZED,
          wuffs_base__make_slice_u8((uint8_t*)g_serialized_dictionary,
                                    sizeof g_serialized_dictionary),
          workbuf);
      if (wuffs_base__status__is_ok(&status)) {
        status = wuffs_brotli__decoder__add_dictionary(
            &dec, WUFFS_BROTLI__DICTIONARY_TYPE_RAW,
            wuffs_base__make_slice_u8((uint8_t*)g_raw_dictionary,
                                      sizeof g_raw_dictionary - 1),
            workbuf);
      }
      break;
  }
  if (!wuffs_base__status__is_ok(&status)) {
    return wuffs_base__status__message(&status);
  }
  wuffs_base__range_ii_u64 workbuf_len =
      wuffs_brotli__decoder__workbuf_len(&dec);
  if (workbuf.len > workbuf_len.max_incl) {
    workbuf.len = (size_t)workbuf_len.max_incl;
  }

  static uint8_t dst_buffer[DST_BUFFER_ARRAY_SIZE];
  wuffs_base__io_buffer dst =
      wuffs_base__ptr_u8__writer(&dst_buffer[0], DST_BUFFER_ARRAY_SIZE);

  // Use random-ish I/O limits for some of the inputs.
  bool limited = ((hash >> 5) & 3) == 0;
  size_t wlimit = 1 + ((hash >> 8) & 0xFFFF);
  size_t rlimit = 1 + ((hash >> 24) & 0xFFF);

  while (true) {
    dst.meta.wi = 0;
    wuffs_base__io_buffer limited_dst = dst;
    wuffs_base__io_buffer limited_src = *src;
    if (limited) {
      if (limited_dst.data.len > wlimit) {
        limited_dst.data.len = wlimit;
      }
      if ((limited_src.meta.wi - limited_src.meta.ri) > rlimit) {
        limited_src.meta.wi = limited_src.meta.ri + rlimit;
        limited_src.meta.closed = false;
      }
    }
    status = wuffs_brotli__decoder__transform_io(&dec, &limited_dst,
                                                 &limited_src, workbuf);
    src->meta.ri = limited_src.meta.ri;
    if (status.repr == wuffs_base__suspension__short_read) {
      if (limited && (src->meta.ri < src->meta.wi)) {
        continue;
      }
      break;
    } else if (status.repr != wuffs_base__suspension__short_write) {
      break;
    }
    if (limited_dst.meta.wi == 0) {
      fprintf(stderr, "wuffs_brotli__decoder__transform_io made no progress\n");
      intentional_segfault();
    }
  }
  return wuffs_base__status__message(&status);
}
