# Brotli

Brotli is a general purpose compression format, combining LZ77 style
length-distance back-references, Huffman coding, second order context modeling
and a built-in static dictionary. It is specified by [RFC
7932](https://www.rfc-editor.org/rfc/rfc7932). The reference implementation is
at [github.com/google/brotli](https://github.com/google/brotli).

This package (`std/brotli`) is a decoder. It aims for exact parity with the
reference implementation's decoder (libbrotlidec), version v1.2.1, and in
particular its `c/dec/decode.c` file: the same output for valid input and the
same verdict (and the same error code, mapped as below) for invalid input. The
Wuffs code mirrors the C code's logic and validation checks, function by
function, and comments like "Mirrors ReadHuffmanCode in decode.c" name the
corresponding C function. The C code's explicit state machine (its
`BROTLI_STATE_ETC` enums) is replaced by Wuffs coroutines.


## Format Overview

A brotli stream starts with a 1, 4 or 7 bit `WBITS` header, giving the window
size (the maximum back-reference distance, plus 16). It is followed by a
sequence of metablocks. Each metablock header gives its uncompressed length
(`MLEN`) and whether it is the last metablock. A metablock is one of:

- a metadata block, whose payload is skipped (it does not produce output),
- an uncompressed block, whose payload is byte-aligned and copied verbatim, or
- a compressed block.

A compressed metablock header defines, for each of three block categories
(literals, insert-and-copy commands and distances), the number of block types
and the prefix codes for block type switches and block lengths. It then gives
the distance parameters (`NPOSTFIX` and `NDIRECT`), the literal context modes,
the literal and distance context maps (run-length and move-to-front encoded)
and finally the groups of prefix (Huffman) codes. Each prefix code is either a
"simple" code (up to 4 symbols) or a "complex" code (code lengths that are
themselves prefix coded).

The metablock body is a sequence of commands. Each command inserts zero or more
literal bytes (decoded with a prefix code chosen by the context map, from the
previous two bytes and the current literal block type) and then copies bytes,
either from earlier in the output (an LZ77 back-reference, possibly re-using
one of the last four distances) or, for distances beyond the maximum possible
back-reference, a transformed word from the static dictionary.

The final metablock is followed by zero padding up to a byte boundary. The
decoder does not read any further than that.


## Workbuf Layout

`workbuf_len` is `(1 << 24) + 542` bytes, both minimum and maximum: the
largest (non-large-window) brotli window size plus some slack. The slack
mirrors the C code's `kRingBufferWriteAheadSlack`, which its fast paths use to
write (and discard) a few bytes past the ring buffer's end. This
implementation does not yet have such fast paths, but it still reserves the
slack, so that adding them later will not change the `workbuf_len` API. A
workbuf shorter than the stream's window size plus 542 bytes is rejected with
`base."#bad workbuf length"`.

The first `(1 << WBITS)` bytes of the workbuf are the history ring buffer.
Decoded bytes are written to the ring buffer and then copied to the `dst`
`io_writer`. Before the ring buffer wraps around, all of its bytes must have
been copied to `dst`, so `transform_io` can suspend with `$short write`.

The C code's ring buffer (`s->ringbuffer_size`) can start smaller than the
window size (its "canny ring buffer allocation") and grows over successive
metablocks. The Wuffs ring buffer is always `(1 << WBITS)` bytes long, but the
decoder still tracks the C code's (virtual) ring buffer size, as it decides
when the C code calls `WriteRingBuffer`. That in turn decides whether some
invalid input (an overrun metablock) is reported as `BLOCK_LENGTH_1` or
`BLOCK_LENGTH_2`, and when the C code's `max_distance` becomes fixed.

Everything else (the Huffman tables, context maps, etc.) lives in the decoder
struct, not the workbuf. The Huffman tables use the same 8-bit root table and
second level table layout as the C code, sized per tree as the C code
allocates them.


## Status Mapping

Every `BROTLI_DECODER_ERROR_FORMAT_*` error code (and running out of input) has
a corresponding Wuffs status:

| libbrotlidec error code                               | Wuffs status                                          |
| ----------------------------------------------------- | ----------------------------------------------------- |
| `BROTLI_DECODER_ERROR_FORMAT_EXUBERANT_NIBBLE`        | `"#bad exuberant nibble"`                             |
| `BROTLI_DECODER_ERROR_FORMAT_RESERVED`                | `"#bad reserved bit"`                                 |
| `BROTLI_DECODER_ERROR_FORMAT_EXUBERANT_META_NIBBLE`   | `"#bad exuberant metadata nibble"`                    |
| `BROTLI_DECODER_ERROR_FORMAT_SIMPLE_HUFFMAN_ALPHABET` | `"#bad simple Huffman alphabet"`                      |
| `BROTLI_DECODER_ERROR_FORMAT_SIMPLE_HUFFMAN_SAME`     | `"#bad simple Huffman symbol"`                        |
| `BROTLI_DECODER_ERROR_FORMAT_CL_SPACE`                | `"#bad code length code lengths"`                     |
| `BROTLI_DECODER_ERROR_FORMAT_HUFFMAN_SPACE`           | `"#bad Huffman space"`                                |
| `BROTLI_DECODER_ERROR_FORMAT_CONTEXT_MAP_REPEAT`      | `"#bad context map repeat"`                           |
| `BROTLI_DECODER_ERROR_FORMAT_BLOCK_LENGTH_1`          | `"#bad block length"`                                 |
| `BROTLI_DECODER_ERROR_FORMAT_BLOCK_LENGTH_2`          | `"#bad metablock length"`                             |
| `BROTLI_DECODER_ERROR_FORMAT_TRANSFORM`               | `"#bad transform"`                                    |
| `BROTLI_DECODER_ERROR_FORMAT_DICTIONARY`              | `"#bad dictionary"`                                   |
| `BROTLI_DECODER_ERROR_FORMAT_WINDOW_BITS`             | `"#bad window bits"` or `"#unsupported large window"` |
| `BROTLI_DECODER_ERROR_FORMAT_PADDING_1`               | `"#bad padding"`                                      |
| `BROTLI_DECODER_ERROR_FORMAT_PADDING_2`               | `"#bad final padding"`                                |
| `BROTLI_DECODER_ERROR_FORMAT_DISTANCE`                | `"#bad distance"`                                     |
| `BROTLI_DECODER_ERROR_FORMAT_BLOCK_SWITCH`            | `"#bad block switch"`                                 |
| `BROTLI_DECODER_NEEDS_MORE_INPUT` (with closed `src`) | `"#truncated input"`                                  |

Notes:

- `"#unsupported large window"` is for the large-window escape `WBITS` value
  (`0010001`). libbrotlidec (with `BROTLI_DECODER_PARAM_LARGE_WINDOW` not set,
  the default) rejects it with `BROTLI_DECODER_ERROR_FORMAT_WINDOW_BITS`. For
  standard (non-large-window) brotli, no other `WBITS` value is invalid, so
  `"#bad window bits"` is not currently produced.
- Like the C code, which error an overrun metablock produces (`"#bad block
  length"` or `"#bad metablock length"`) can depend on how the input is split
  across `transform_io` calls. When the C code runs out of input, it first
  tries to flush its output (`WriteRingBuffer`) and that checks for an overrun
  metablock. A Wuffs `$short read` suspension corresponds to the C code's
  running out of input.
- `"#bad block switch"` needs more than `(1 << 24)` commands in one metablock.
  It is implemented but not tested.
- The remaining libbrotlidec error codes do not apply: `COMPOUND_DICTIONARY`
  and `DICTIONARY_NOT_SET` (custom dictionaries are not supported),
  `INVALID_ARGUMENTS`, the `ALLOC_*` codes (this decoder does not allocate)
  and `UNREACHABLE`.

The `"#internal error: etc"` statuses should never happen. They are
consistency checks, generally where the C code relies on (unchecked)
invariants for memory safety but Wuffs requires explicit bounds checks.


## Differences from the C Code

- The bit reader reads input bytes only when it needs them (like the C code's
  "safe" `BrotliSafeReadBits` and `SafeReadSymbol` functions), so that
  `transform_io` can suspend at any point. There is no "fast path" (like the C
  code's `ProcessCommands`, as opposed to `SafeProcessCommands`). Both paths
  produce the same output and errors.
- LZ77 copies and dictionary words are written to the ring buffer one byte at
  a time, with ring buffer events (the C code's `WriteRingBuffer` and
  `WrapRingBuffer` calls) at the same positions as the C code.
- On error, the Wuffs decoder may have already written more output to `dst`
  than libbrotlidec would have (libbrotlidec does not flush on error), but
  what both have written agrees.


## TODO

- Large-window brotli (`BROTLI_DECODER_PARAM_LARGE_WINDOW`, window bits up to
  30). The window-related fields are sized for 24 bits.
- Custom, shared and compound dictionaries.
- Metadata callbacks (metadata blocks are currently skipped).
- Fast paths.
- Fuzzers.
- An encoder.
