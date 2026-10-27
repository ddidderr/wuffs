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

As well as decoding its input (with or without the large-window quirk,
metadata reporting and dictionaries), it also encodes the input (with encoder
parameters, chunking, flushes, metadata blocks and dst limits derived from the
input's hash), decodes the result and checks that the round trip reproduces
the input.
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
#define WUFFS_CONFIG__MODULE__SOFTFLOAT

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
// We hard-code 128 MiB, as WUFFS_BROTLI__DECODER_WORKBUF_LEN_MAX_INCL_WORST_CASE
// is over 3 GiB and even the large-window workbuf_len is over 1 GiB. Using
// those can cause "relocation truncated to fit: R_X86_64_PC32 against symbol
// etc" compiler errors. Large-window streams with a window of 128 MiB or more
// are rejected with "#base: bad workbuf length", but smaller ones are fine.
//
// The encoder (see fuzz_encoder) uses the same work buffer, after decoding.
// Its workbuf_len depends on the parameters: about 92 MiB at qualities 10 and
// 11 with lgwin 16 (and less at lower qualities), so encoder parameters whose
// workbuf_len is larger are scaled down.
#define WORK_BUFFER_ARRAY_SIZE 134217728
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

// ---------------- Encoder round trip

// The encoder's input is capped (a prefix of the fuzz input), as is the input
// at qualities 10 and 11, which are slow.
#define ENC_MAX_INPUT 65536
#define ENC_MAX_INPUT_HQ 4096
#define ENC_DST_ARRAY_SIZE (2 * ENC_MAX_INPUT + 65536)

static wuffs_brotli__encoder g_encoder;
static wuffs_brotli__decoder g_rt_decoder;
static uint8_t g_enc_dst[ENC_DST_ARRAY_SIZE];
static uint8_t g_enc_want[ENC_MAX_INPUT];  // The input minus any metadata.
static uint8_t g_enc_have[ENC_MAX_INPUT + 1];

static uint64_t  //
mix64(uint64_t x) {
  x ^= x >> 31;
  x *= 0x7FB5D329728EA185ull;
  x ^= x >> 27;
  x *= 0x81DADEF4BC2DD44Dull;
  x ^= x >> 33;
  return x;
}

static void  //
fail_encoder(const char* what, const char* status) {
  fprintf(stderr, "brotli encoder round trip: %s: %s\n", what,
          status ? status : "ok");
  intentional_segfault();
}

// fuzz_encoder encodes data[0 .. n] with parameters derived from hash and
// checks that decoding the result gives back the input (minus the bytes used
// as metadata).
static void  //
fuzz_encoder(const uint8_t* data, size_t n, uint64_t hash) {
  uint64_t h = mix64(hash ^ 0x5DEECE66Dull);
  uint64_t h2 = mix64(h);
  uint32_t quality = (uint32_t)((h >> 0) % 12);
  uint32_t lgwin = 10 + (uint32_t)((h >> 4) % 15);
  bool large_window = ((h >> 8) & 7) == 0;
  bool use_dict = ((h >> 11) & 3) == 0;
  if (large_window && (quality >= 3)) {
    lgwin = 10 + (uint32_t)((h >> 4) % 17);  // Up to 26.
  }
  if (quality >= 10) {
    if (n > ENC_MAX_INPUT_HQ) {
      n = ENC_MAX_INPUT_HQ;
    }
    // Compound dictionaries are not supported at qualities 10 and 11 yet.
    use_dict = false;
  } else if (n > ENC_MAX_INPUT) {
    n = ENC_MAX_INPUT;
  }

  wuffs_base__status status = wuffs_brotli__encoder__initialize(
      &g_encoder, sizeof g_encoder, WUFFS_VERSION,
      (hash & 1) ? WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED : 0);
  if (!wuffs_base__status__is_ok(&status)) {
    fail_encoder("initialize", status.repr);
  }
  wuffs_base__slice_u8 workbuf =
      wuffs_base__make_slice_u8(g_work_buffer_array, WORK_BUFFER_ARRAY_SIZE);
  if (use_dict) {
    status = wuffs_brotli__encoder__add_dictionary(
        &g_encoder, WUFFS_BROTLI__DICTIONARY_TYPE_RAW,
        wuffs_base__make_slice_u8((uint8_t*)g_raw_dictionary,
                                  sizeof g_raw_dictionary - 1),
        workbuf);
    if (!wuffs_base__status__is_ok(&status)) {
      fail_encoder("add_dictionary", status.repr);
    }
  }

  // Set the parameters. Each has a (mostly) default value for some inputs.
  struct {
    uint32_t key;
    uint64_t value;
    bool enabled;
  } quirks[] = {
      {WUFFS_BROTLI__QUIRK_ENCODER_QUALITY, quality, true},
      {WUFFS_BROTLI__QUIRK_LGWIN, lgwin, true},
      {WUFFS_BROTLI__QUIRK_LARGE_WINDOW, 1, large_window},
      {WUFFS_BROTLI__QUIRK_MODE, (h >> 13) % 3, ((h >> 15) & 1) != 0},
      {WUFFS_BROTLI__QUIRK_LGBLOCK, 16 + ((h >> 16) % 9),
       ((h >> 20) & 3) == 0},
      {WUFFS_BROTLI__QUIRK_DISABLE_LITERAL_CONTEXT_MODELING, 1,
       ((h >> 22) & 3) == 0},
      {WUFFS_BROTLI__QUIRK_SIZE_HINT, ((h >> 24) & 1) ? n : (h >> 32),
       ((h >> 25) & 1) != 0},
      {WUFFS_BROTLI__QUIRK_NPOSTFIX, (h >> 26) & 3, ((h >> 28) & 3) == 0},
      {WUFFS_BROTLI__QUIRK_NDIRECT, ((h >> 30) & 15) << ((h >> 26) & 3),
       ((h >> 28) & 3) == 0},
  };
  for (size_t i = 0; i < sizeof quirks / sizeof quirks[0]; i++) {
    if (quirks[i].enabled) {
      status = wuffs_brotli__encoder__set_quirk(&g_encoder, quirks[i].key,
                                                quirks[i].value);
      if (!wuffs_base__status__is_ok(&status)) {
        fail_encoder("set_quirk", status.repr);
      }
    }
  }
  // Scale the parameters down until the workbuf fits.
  bool lgblock_set = quirks[4].enabled;
  while (wuffs_brotli__encoder__workbuf_len(&g_encoder).min_incl >
         WORK_BUFFER_ARRAY_SIZE) {
    if (lgwin > 16) {
      lgwin = 16;
      wuffs_brotli__encoder__set_quirk(&g_encoder, WUFFS_BROTLI__QUIRK_LGWIN,
                                       lgwin);
    } else if (lgblock_set) {
      lgblock_set = false;
      wuffs_brotli__encoder__set_quirk(&g_encoder, WUFFS_BROTLI__QUIRK_LGBLOCK,
                                       0);
    } else if (quality > 9) {
      quality = 9;
      wuffs_brotli__encoder__set_quirk(
          &g_encoder, WUFFS_BROTLI__QUIRK_ENCODER_QUALITY, quality);
    } else {
      fail_encoder("workbuf_len is too large", NULL);
    }
  }
  wuffs_base__range_ii_u64 wlen = wuffs_brotli__encoder__workbuf_len(&g_encoder);
  uint64_t wl = wlen.min_incl;
  if ((h2 & 1) && (wlen.max_incl > wl)) {
    wl = (wlen.max_incl < WORK_BUFFER_ARRAY_SIZE) ? wlen.max_incl
                                                  : WORK_BUFFER_ARRAY_SIZE;
  }
  workbuf.len = (size_t)wl;

  // The plan: chunks of up to chunk_max bytes, a flush after some of them and
  // some metadata blocks (whose contents are input bytes).
  size_t chunk_max = ((h2 >> 1) & 1) ? n : (1 + ((h2 >> 2) % 4096));
  uint32_t flush_mask = (uint32_t)((h2 >> 14) & 7);   // Flush if (i & 7) < it.
  uint32_t meta_mask = (uint32_t)((h2 >> 17) & 15);  // Metadata if == 15.
  size_t dst_limit = ((h2 >> 21) & 3) ? SIZE_MAX : (1 + ((h2 >> 23) % 64));
  uint64_t rng = h2;

  wuffs_base__io_buffer dst =
      wuffs_base__ptr_u8__writer(g_enc_dst, ENC_DST_ARRAY_SIZE);
  wuffs_base__io_buffer src =
      wuffs_base__ptr_u8__reader((uint8_t*)data, n, false);
  src.meta.wi = 0;
  size_t want_len = 0;
  for (uint32_t i = 0; src.meta.wi < n; i++) {
    rng = mix64(rng);
    size_t k = 1 + (size_t)(rng % chunk_max);
    if (k > (n - src.meta.wi)) {
      k = n - src.meta.wi;
    }
    bool meta = (meta_mask == 15) && ((rng >> 40) & 1) && (k <= 600);
    bool last = (src.meta.wi + k) == n;
    status = wuffs_base__make_status(NULL);
    if (meta) {
      status = wuffs_brotli__encoder__request_emit_metadata(&g_encoder, k);
    } else if (!last && ((i & 7) < flush_mask)) {
      status = wuffs_brotli__encoder__request_flush(&g_encoder);
    }
    if (!wuffs_base__status__is_ok(&status)) {
      fail_encoder("request", status.repr);
    }
    if (!meta) {
      memcpy(g_enc_want + want_len, data + src.meta.wi, k);
      want_len += k;
    }
    src.meta.wi += k;
    src.meta.closed = last && !meta;
    while (true) {
      wuffs_base__io_buffer d = dst;
      if ((d.data.len - d.meta.wi) > dst_limit) {
        d.data.len = d.meta.wi + dst_limit;
      }
      status = wuffs_brotli__encoder__transform_io(&g_encoder, &d, &src,
                                                   workbuf);
      dst.meta.wi = d.meta.wi;
      if (status.repr == wuffs_base__suspension__short_write) {
        if (dst.meta.wi == dst.data.len) {
          fail_encoder("dst is full", NULL);
        }
        continue;
      } else if ((status.repr == wuffs_base__suspension__short_read) &&
                 !src.meta.closed && (src.meta.ri == src.meta.wi)) {
        break;
      } else if (wuffs_base__status__is_ok(&status) && src.meta.closed) {
        break;
      }
      fail_encoder("transform_io", status.repr);
    }
  }
  if (!src.meta.closed) {
    // The input ended with a metadata block (or is empty): finish.
    src.meta.closed = true;
    while (true) {
      wuffs_base__io_buffer d = dst;
      if ((d.data.len - d.meta.wi) > dst_limit) {
        d.data.len = d.meta.wi + dst_limit;
      }
      status = wuffs_brotli__encoder__transform_io(&g_encoder, &d, &src,
                                                   workbuf);
      dst.meta.wi = d.meta.wi;
      if (status.repr == wuffs_base__suspension__short_write) {
        continue;
      } else if (wuffs_base__status__is_ok(&status)) {
        break;
      }
      fail_encoder("transform_io (finish)", status.repr);
    }
  }

  // Decode it again.
  wuffs_brotli__decoder* dec = &g_rt_decoder;
  status = wuffs_brotli__decoder__initialize(dec, sizeof *dec, WUFFS_VERSION,
                                             0);
  if (!wuffs_base__status__is_ok(&status)) {
    fail_encoder("decoder initialize", status.repr);
  }
  wuffs_brotli__decoder__set_quirk(dec, WUFFS_BROTLI__QUIRK_LARGE_WINDOW,
                                   large_window);
  workbuf.len = WORK_BUFFER_ARRAY_SIZE;
  if (use_dict) {
    status = wuffs_brotli__decoder__add_dictionary(
        dec, WUFFS_BROTLI__DICTIONARY_TYPE_RAW,
        wuffs_base__make_slice_u8((uint8_t*)g_raw_dictionary,
                                  sizeof g_raw_dictionary - 1),
        workbuf);
    if (!wuffs_base__status__is_ok(&status)) {
      fail_encoder("decoder add_dictionary", status.repr);
    }
  }
  wuffs_base__io_buffer have =
      wuffs_base__ptr_u8__writer(g_enc_have, sizeof g_enc_have);
  wuffs_base__io_buffer enc_src =
      wuffs_base__ptr_u8__reader(g_enc_dst, dst.meta.wi, true);
  status = wuffs_brotli__decoder__transform_io(dec, &have, &enc_src, workbuf);
  if (!wuffs_base__status__is_ok(&status)) {
    fail_encoder("decoder transform_io", status.repr);
  } else if ((have.meta.wi != want_len) ||
             memcmp(g_enc_have, g_enc_want, want_len) ||
             (enc_src.meta.ri != enc_src.meta.wi)) {
    fail_encoder("round trip mismatch", NULL);
  }
}

// ---------------- Decoder

const char*  //
fuzz(wuffs_base__io_buffer* src, uint64_t hash) {
  fuzz_encoder(wuffs_base__io_buffer__reader_pointer(src),
               wuffs_base__io_buffer__reader_length(src), hash);

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
  // Report metadata blocks for half of the inputs.
  wuffs_brotli__decoder__set_quirk(&dec, WUFFS_BROTLI__QUIRK_REPORT_METADATA,
                                   (hash >> 40) & 1);

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
    if (status.repr == wuffs_base__note__metadata_reported) {
      if (wuffs_brotli__decoder__metadata_length(&dec) > 0x1000000) {
        fprintf(stderr, "wuffs_brotli__decoder__metadata_length too large\n");
        intentional_segfault();
      }
      continue;
    } else if (status.repr == wuffs_base__suspension__short_read) {
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
