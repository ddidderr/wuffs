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
//
// The encoder's reference implementation is libbrotlienc v1.2.1.

#include <limits.h>
#include <stdint.h>

#include "brotli/decode.h"
#include "brotli/encode.h"

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

// ---------------- Encoder

// mimic_brotli_encode runs libbrotlienc's BrotliEncoderCompressStream over a
// "plan" of num_steps steps. Step i is the BrotliEncoderOperation step_ops[i]
// (0 = PROCESS, 1 = FLUSH, 2 = FINISH, 3 = EMIT_METADATA) applied to the next
// step_lens[i] bytes of src. Each step repeats that operation until all of its
// input is consumed and there is no more pending output (or, for FINISH,
// until the encoder is finished).
//
// Before the first step, each of the num_params (param_keys[j],
// param_values[j]) pairs is passed to BrotliEncoderSetParameter. The keys are
// BrotliEncoderParameter values.
//
// Each BrotliEncoderCompressStream call gets at most dst_chunk bytes of dst
// (or all of dst's writable bytes, if dst_chunk is zero).
const char*  //
mimic_brotli_encode(wuffs_base__io_buffer* dst,
                    wuffs_base__io_buffer* src,
                    const uint32_t* param_keys,
                    const uint32_t* param_values,
                    size_t num_params,
                    const uint32_t* step_ops,
                    const size_t* step_lens,
                    size_t num_steps,
                    size_t dst_chunk) {
  if (BrotliEncoderVersion() != 0x1002001) {
    return "libbrotlienc: unexpected version (want v1.2.1)";
  }
  BrotliEncoderState* state = BrotliEncoderCreateInstance(NULL, NULL, NULL);
  if (!state) {
    return "libbrotlienc: BrotliEncoderCreateInstance failed";
  }
  const char* ret = NULL;
  for (size_t j = 0; j < num_params; j++) {
    if (!BrotliEncoderSetParameter(state,
                                   (BrotliEncoderParameter)(param_keys[j]),
                                   param_values[j])) {
      ret = "libbrotlienc: BrotliEncoderSetParameter failed";
      goto cleanup;
    }
  }

  for (size_t i = 0; i < num_steps; i++) {
    size_t avail_in = step_lens[i];
    if (avail_in > wuffs_base__io_buffer__reader_length(src)) {
      ret = "mimic_brotli_encode: step length exceeds the input";
      goto cleanup;
    }
    const uint8_t* next_in = wuffs_base__io_buffer__reader_pointer(src);
    BrotliEncoderOperation op = (BrotliEncoderOperation)(step_ops[i]);
    while (true) {
      size_t avail_out = wuffs_base__io_buffer__writer_length(dst);
      if ((dst_chunk > 0) && (avail_out > dst_chunk)) {
        avail_out = dst_chunk;
      }
      size_t avail_out0 = avail_out;
      uint8_t* next_out = wuffs_base__io_buffer__writer_pointer(dst);
      if (!BrotliEncoderCompressStream(state, op, &avail_in, &next_in,
                                       &avail_out, &next_out, NULL)) {
        ret = "libbrotlienc: BrotliEncoderCompressStream failed";
        goto cleanup;
      }
      dst->meta.wi += avail_out0 - avail_out;
      if (op == BROTLI_OPERATION_FINISH) {
        if (BrotliEncoderIsFinished(state)) {
          break;
        }
      } else if ((avail_in == 0) && !BrotliEncoderHasMoreOutput(state)) {
        break;
      }
      if (wuffs_base__io_buffer__writer_length(dst) == 0) {
        ret = "mimic_brotli_encode: dst is full";
        goto cleanup;
      }
    }
    src->meta.ri += step_lens[i];
  }

cleanup:
  BrotliEncoderDestroyInstance(state);
  return ret;
}
