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

// The reference implementation for std/brotli is libbrotlidec v1.2.1 (from
// github.com/google/brotli). Older versions (such as v1.1.0) differ slightly
// in which error code some invalid inputs produce.

#include <limits.h>
#include <stdint.h>

#include "brotli/decode.h"

// mimic_brotli_decode_error_code is like mimic_brotli_decode but also stores
// the libbrotlidec error code (a BrotliDecoderErrorCode value) in
// *error_code_ptr. A BROTLI_DECODER_NEEDS_MORE_INPUT (2) error code means that
// the input was truncated.
const char*  //
mimic_brotli_decode_error_code(wuffs_base__io_buffer* dst,
                               wuffs_base__io_buffer* src,
                               int* error_code_ptr) {
  *error_code_ptr = 0;
  BrotliDecoderState* state = BrotliDecoderCreateInstance(NULL, NULL, NULL);
  if (!state) {
    return "libbrotlidec: BrotliDecoderCreateInstance failed";
  }
  size_t avail_in = wuffs_base__io_buffer__reader_length(src);
  const uint8_t* next_in = wuffs_base__io_buffer__reader_pointer(src);
  size_t avail_out = wuffs_base__io_buffer__writer_length(dst);
  uint8_t* next_out = wuffs_base__io_buffer__writer_pointer(dst);
  size_t total_out = 0;

  BrotliDecoderResult result = BrotliDecoderDecompressStream(
      state, &avail_in, &next_in, &avail_out, &next_out, &total_out);
  *error_code_ptr = (int)BrotliDecoderGetErrorCode(state);
  BrotliDecoderDestroyInstance(state);

  src->meta.ri += wuffs_base__io_buffer__reader_length(src) - avail_in;
  dst->meta.wi += wuffs_base__io_buffer__writer_length(dst) - avail_out;

  switch (result) {
    case BROTLI_DECODER_RESULT_SUCCESS:
      return NULL;
    case BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT:
      return "libbrotlidec: needs more input";
    case BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT:
      return "libbrotlidec: needs more output";
    default:
      break;
  }
  return "libbrotlidec: BrotliDecoderDecompressStream failed";
}

const char*  //
mimic_brotli_decode(wuffs_base__io_buffer* dst,
                    wuffs_base__io_buffer* src,
                    uint32_t wuffs_initialize_flags,
                    uint64_t wlimit,
                    uint64_t rlimit) {
  if ((wlimit < UINT64_MAX) || (rlimit < UINT64_MAX)) {
    // It's simpler if we only assume one-shot decompression.
    return "unsupported I/O limit";
  }
  int error_code = 0;
  return mimic_brotli_decode_error_code(dst, src, &error_code);
}
