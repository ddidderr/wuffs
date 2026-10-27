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

Without the large-window quirk and without attached dictionaries (see the
"Large Window" and "Dictionaries" sections below), `workbuf_len` is `(1 << 24)
+ 542` bytes, both minimum and maximum: the largest (non-large-window) brotli
window size plus some slack. The slack
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
| `BROTLI_DECODER_ERROR_COMPOUND_DICTIONARY`            | `"#bad compound dictionary"`                          |
| `BROTLI_DECODER_NEEDS_MORE_INPUT` (with closed `src`) | `"#truncated input"`                                  |

Notes:

- `"#unsupported large window"` is for the large-window escape `WBITS` value
  (`0010001`) when the `QUIRK_LARGE_WINDOW` quirk is not set. libbrotlidec
  (with `BROTLI_DECODER_PARAM_LARGE_WINDOW` not set, the default) rejects it
  with `BROTLI_DECODER_ERROR_FORMAT_WINDOW_BITS`. For standard
  (non-large-window) brotli, no other `WBITS` value is invalid. With the quirk
  set, `"#bad window bits"` is for a large-window header with a non-zero
  reserved bit or with window bits outside of `[10 ..= 30]`.
- Like the C code, which error an overrun metablock produces (`"#bad block
  length"` or `"#bad metablock length"`) can depend on how the input is split
  across `transform_io` calls. When the C code runs out of input, it first
  tries to flush its output (`WriteRingBuffer`) and that checks for an overrun
  metablock. A Wuffs `$short read` suspension corresponds to the C code's
  running out of input.
- `"#bad block switch"` needs more than `(1 << 24)` commands in one metablock.
  It is implemented but not tested.
- The remaining libbrotlidec error codes do not apply: `DICTIONARY_NOT_SET`
  (it needs a word list without data, which libbrotlidec only has when built
  with `BROTLI_EXTERNAL_DICTIONARY_DATA` and not given that data),
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

- Metadata callbacks (metadata blocks are currently skipped). See the
  "Metadata Blocks" section below.
- Fast paths.
- Encoder quality levels 2 to 11, encoder dictionaries and large-window
  encoding (see the "Encoder" section below).


## Encoder

This package also has an `encoder`, which implements the `base.io_transformer`
interface. It aims for byte-exact parity with the reference implementation's
encoder (libbrotlienc), version v1.2.1: for the same parameters and the same
sequence of operations, it produces exactly the same bytes. Like the decoder,
the Wuffs code mirrors the C code function by function, with comments like
"Mirrors BrotliCompressFragmentFast in compress_fragment.c". Its floating
point arithmetic (the C code's `double`s) uses `std/softfloat`, so that the
results are bit-exact regardless of the compiler and CPU.

Only qualities 0 and 1 (the C code's `FAST_ONE_PASS_COMPRESSION_QUALITY` and
`FAST_TWO_PASS_COMPRESSION_QUALITY`, implemented by `compress_fragment.c` and
`compress_fragment_two_pass.c`) are implemented. For qualities 2 to 11
(including the default quality, 11), `transform_io` returns
`"#unsupported encoder quality"`.

The source files are:

- `encode_brotli.wuffs`: the `encoder` struct, its public methods and the
  `encode.c` streaming logic (`BrotliEncoderCompressStream`,
  `BrotliEncoderCompressStreamFast`, `ProcessMetadata`, `EnsureInitialized`,
  `InjectBytePaddingBlock`, etc).
- `encode_quirks.wuffs`: the encoder's quirk keys.
- `encode_bit_writer.wuffs`: `write_bits.h`, `fast_log.h`,
  `find_match_length.h` and small `brotli_bit_stream.c` helpers.
- `encode_entropy.wuffs`: `entropy_encode.c` (Huffman tree construction) and
  the `brotli_bit_stream.c` functions that store Huffman codes.
- `encode_fragment_fast.wuffs`: `compress_fragment.c` (quality 0).
- `encode_fragment_two_pass.wuffs`: `compress_fragment_two_pass.c` (quality
  1).
- `encode_bit_cost.wuffs`: `bit_cost.c`, `bit_cost_inc.h` and `fast_log.h`
  (`BrotliBitsEntropy`, `BrotliPopulationCost{Literal,Command,Distance}`), as
  methods of the `bit_cost` struct.
- `encode_histogram.wuffs`: `histogram.h`, `histogram_inc.h` and
  `histogram.c` (histogram helpers and `BrotliBuildHistogramsWithContext`),
  also as `bit_cost` methods.
- `encode_tester.wuffs`: TEST ONLY. The `metablock_tester` struct exposes
  the internal functions above to C test programs. It is not part of the
  encoder API.
- `common_encoder_tables.wuffs`: tables generated by `go run
  script/print-brotli-tables.go -encoder` from the C code.


### Encoder API

In addition to the `base.io_transformer` methods, the encoder has:

- `request_flush!() status`: the next `transform_io` call acts like the C
  code's `BROTLI_OPERATION_FLUSH` (instead of `PROCESS` or `FINISH`) until
  the flush is complete. It returns `"#bad call sequence"` while a metadata
  request is pending.
- `request_emit_metadata!(length: u64) status`: the next `transform_io` call
  acts like `BROTLI_OPERATION_EMIT_METADATA`, with the next `length` bytes of
  `src` as the metadata block's contents. `length` can be at most `1 << 24`
  (otherwise `"#bad argument"`). It returns `"#bad call sequence"` while
  another request is pending.
- `max_compressed_size(input_size: u64) u64`: mirrors
  `BrotliEncoderMaxCompressedSize` (0 means overflow).

`dst_history_retain_length` is 0: the encoder never re-reads `dst`.


### Encoder Quirks

Each quirk mirrors one `BrotliEncoderParameter`, with the value semantics of
`BrotliEncoderSetParameter`. Values are stored as is (and must fit in a u32,
otherwise `set_quirk` returns `"#bad argument"`). Like the C code, they are
sanitized (`SanitizeParams`, `ComputeLgBlock`, `ChooseDistanceParams`) at the
first `transform_io` call, after which `set_quirk` returns
`"#bad call sequence"`.

| Quirk                                    | C parameter                                    |
| ---------------------------------------- | ---------------------------------------------- |
| `QUIRK_ENCODER_QUALITY`                  | `BROTLI_PARAM_QUALITY` (default 11)            |
| `QUIRK_LGWIN`                            | `BROTLI_PARAM_LGWIN` (default 22)              |
| `QUIRK_LGBLOCK`                          | `BROTLI_PARAM_LGBLOCK`                         |
| `QUIRK_MODE`                             | `BROTLI_PARAM_MODE`                            |
| `QUIRK_DISABLE_LITERAL_CONTEXT_MODELING` | `BROTLI_PARAM_DISABLE_LITERAL_CONTEXT_MODELING` (0 or 1) |
| `QUIRK_SIZE_HINT`                        | `BROTLI_PARAM_SIZE_HINT`                       |
| `QUIRK_LARGE_WINDOW` (shared with the decoder) | `BROTLI_PARAM_LARGE_WINDOW`              |
| `QUIRK_NPOSTFIX`                         | `BROTLI_PARAM_NPOSTFIX`                        |
| `QUIRK_NDIRECT`                          | `BROTLI_PARAM_NDIRECT`                         |
| `QUIRK_STREAM_OFFSET`                    | `BROTLI_PARAM_STREAM_OFFSET` (at most `1 << 30`) |

The quality quirk is not named `QUIRK_QUALITY` because that name would clash
with `base.QUIRK_QUALITY`. At qualities 0 and 1, only the quality, lgwin and
stream offset affect the output (lgblock, mode, size hint, large window,
npostfix, ndirect and literal context modeling are stored, sanitized and then
ignored, as in the C code).


### Encoder Workbuf Layout

At qualities 0 and 1, the workbuf is the C code's metablock storage (its
`GetBrotliStorage` buffer): `workbuf_len` is `(2 << lgwin) + 503 + 16` bytes
(the storage for a `1 << lgwin` byte block plus 16 bytes of slack for 8-byte
bit writes), where lgwin is the sanitized value. A shorter workbuf still works
if every input block is shorter: a block of `n` bytes needs `(2 * n) + 519`
bytes, otherwise `transform_io` returns `"#bad workbuf length"`. At other
qualities, `workbuf_len` is 0.

The hash table (up to `1 << 17` entries), the quality 1 command and literal
buffers (`1 << 17` entries each) and the Huffman scratch space are arrays in
the `encoder` struct (which is therefore more than 1 MiB). They do not need
zero-initializing: `WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED` is
fine. Unlike the C code, the input block is not copied: the encoder compresses
directly from `src`.


### Encoder Operation Mapping

Each `transform_io` call (initial or resumed) corresponds to one or more
`BrotliEncoderCompressStream` calls, with `src`'s readable bytes as the C
code's `available_in` and `dst`'s writable bytes as `available_out`. The
operation is:

- `EMIT_METADATA` if `request_emit_metadata` is pending (until the metadata
  block is complete),
- otherwise `FLUSH` if `request_flush` is pending (until the flush is
  complete),
- otherwise `FINISH` if `src` is closed,
- otherwise `PROCESS`.

`transform_io` suspends with `"$short write"` when there is pending output
(the C code's `BrotliEncoderHasMoreOutput`) but `dst` is full, suspends with
`"$short read"` when all of `src` is consumed (and, for a flush, the flush is
complete) and returns ok once the stream is finished (the C code's
`BrotliEncoderIsFinished`). So these two loops produce the same bytes:

- C: for each chunk, call `BrotliEncoderCompressStream(op)` until all of the
  chunk is consumed and there is no more output (or, for `FINISH`, until
  finished).
- Wuffs: for each chunk, call `request_flush` (for `FLUSH`) or
  `request_emit_metadata(len)` (for `EMIT_METADATA`), make `src` hold exactly
  that chunk (closed for `FINISH`) and call `transform_io` until it returns
  `"$short read"` (or ok for `FINISH`), giving it more `dst` room after each
  `"$short write"`.

At qualities 0 and 1 the C code compresses each call's available input
immediately (in blocks of at most `1 << lgwin` bytes), so the output depends
on how the input is split into chunks. The test programs' encode drivers
(`wuffs_brotli_encode_plan` in `test/c/std/brotli.c` and
`mimic_brotli_encode` in `test/c/mimiclib/brotli.c`) run the same "plan" of
(operation, length) steps with each library.


### Encoder Testing

`wuffs test -mimic std/brotli` checks byte-exact parity with libbrotlienc
over a grid of qualities, lgwins, modes, input chunkings, flushes, metadata
blocks, size hints and other parameters, as well as round trips through the
decoder. `script/manual-test-brotli-encoder-parity.c` runs a larger grid
(about 86 thousand configurations over every file in `test/data/brotli` and
some synthetic inputs).


### Encoder Differences from the C Code

- The input block is not copied (see the workbuf layout above), and output is
  always staged in the workbuf (or a small internal buffer) and then copied to
  `dst`. The C code sometimes writes directly to `next_out`. The bytes are the
  same.
- Wuffs' C code generator does not support `slice base.u16` or
  `slice base.u32` arguments (and mishandles `copy_from_slice` on `base.u16`
  arrays), so some functions (e.g. `create_huffman_tree`,
  `convert_bit_depths_to_symbols`) take a selector naming one of the
  encoder's arrays instead of a slice.
- Like the C code, there is no fallback to an uncompressed stream (that is
  only in the C code's one-shot `BrotliEncoderCompress`).


### Encoder TODO

- Qualities 2 to 11 (the C code's `EncodeData`, hashers, block splitting,
  context modeling, Zopfli, etc).
- Large-window encoding (only used at qualities 3 and above).
- Custom and shared dictionaries (`BrotliEncoderAttachPreparedDictionary`).
- Changing parameters mid-stream.


## Large Window

Large-window brotli (an incompatible extension of RFC 7932, with window sizes
up to `(1 << 30)` bytes) is enabled by the `QUIRK_LARGE_WINDOW` quirk. It
mirrors libbrotlidec's `BROTLI_DECODER_PARAM_LARGE_WINDOW` parameter: it must
be set before decoding starts (afterwards, `set_quirk` returns `"#bad call
sequence"`, like `BrotliDecoderSetParameter` failing once the decoder has
left its `BROTLI_STATE_UNINITED` state). With the quirk set:

- a stream that starts with the large-window `WBITS` escape is decoded as a
  large-window stream: the escape is followed by a reserved bit (that must be
  zero) and 6 window bits (that must be in `[10 ..= 30]`), otherwise the
  decoder fails with `"#bad window bits"`.
- a stream that does not start with that escape is decoded as standard brotli,
  exactly as without the quirk.
- `workbuf_len` is `(1 << 30) + 542` bytes (plus any dictionaries), the
  largest large-window window size plus slack. As before, the workbuf only
  needs to be as long as the stream's actual window size plus 542 bytes (plus
  any dictionaries). Without the quirk, it is `(1 << 24) + 542` bytes (plus
  any dictionaries), as before.

For a large-window stream, like the C code, the distance alphabet's
`alphabet_size_max` uses `BROTLI_LARGE_MAX_DISTANCE_BITS` (62) and its
`alphabet_size_limit` comes from `BrotliCalculateDistanceCodeLimit` (at most
544 symbols, so that no distance exceeds `BROTLI_MAX_ALLOWED_DISTANCE`). Up to
29 extra distance bits are read (the C code's `BrotliSafeReadBits32`) and the
distance is calculated with 64-bit arithmetic, like the C code's
`brotli_reg_t`. The Huffman table stride for distance trees is sized for 544
symbols in both modes.

The `DECODER_WORKBUF_LEN_MAX_INCL_WORST_CASE` constant is now the overall
worst case: `(1 << 30) + 542` plus the maximum total dictionary length
(`0x7FFF_FFFF`). The new `DECODER_WORKBUF_LEN_STANDARD` and
`DECODER_WORKBUF_LEN_LARGE_WINDOW` constants give the window-only values.


## Dictionaries

`decoder.add_dictionary!(dict_type: base.u32, dict: roslice base.u8, workbuf:
slice base.u8) base.status` mirrors libbrotlidec's
`BrotliDecoderAttachDictionary`. `dict_type` is `DICTIONARY_TYPE_RAW` (0, the
C code's `BROTLI_SHARED_DICTIONARY_RAW`) or `DICTIONARY_TYPE_SERIALIZED` (1,
`BROTLI_SHARED_DICTIONARY_SERIALIZED`). It must be called before decoding
starts. It can be called multiple times.

Wuffs structs cannot hold pointers or slices, so `add_dictionary` copies the
dictionary bytes into the workbuf. Every `add_dictionary` and `transform_io`
call must be passed the same workbuf (or at least one whose first bytes hold
the same dictionary bytes). The `add_dictionary` workbuf only needs to be long
enough to hold all of the dictionaries added so far. Call `workbuf_len` after
the last `add_dictionary` call: it grows by each dictionary's length.

The workbuf layout is:

    [0 .. D)            the attached dictionaries, concatenated, where D is
                        the total length of all successfully added
                        dictionaries (raw or serialized, in full).
    [D .. D + W + 542)  the history ring buffer (W = 1 << WBITS) and slack.

The dictionaries come first because their position should not depend on the
stream's window size, which is only known once decoding has started.

Raw dictionaries are LZ77 prefix dictionaries. Like the C code (its
`BrotliDecoderCompoundDictionary` and `AttachCompoundDictionary`,
`InitializeCompoundDictionaryCopy` and `CopyFromCompoundDictionary`
functions), at most 15 such "compound dictionary" chunks (raw dictionaries
plus serialized dictionaries' LZ77 prefixes) can be attached. A backward
reference distance `d` beyond the current maximum LZ77 distance `m` (but
within `BROTLI_MAX_ALLOWED_DISTANCE`) refers to the compound dictionary when
`d - m - 1` is less than the compound dictionary's total size `T`, at address
`T - (d - m)`. A copy that would extend past the compound dictionary's end
fails with `"#bad compound dictionary"`. Otherwise, the static dictionary's
address is `d - m - 1 - T`.

Serialized dictionaries are parsed and validated like
`BrotliSharedDictionaryAttach` (`DecodeSharedDictionary`,
`DryParseDictionary`, `ParseDictionary`, `ParseWordList`,
`ParseTransformsList` and `ParsePrefixSuffixTable` in
`c/common/shared_dictionary.c`): the `0x91 0x00` magic, an optional LZ77
prefix (at most 1073741823 bytes), up to 64 custom word lists (word lengths 4
to 31, at most 15 size bits per length), up to 64 custom transform lists (up
to 256 prefix/suffix stringlets, up to 255 transforms, transform types up to
`BROTLI_TRANSFORM_SHIFT_ALL`, with 16-bit parameters for the
`BROTLI_TRANSFORM_SHIFT_FIRST` and `BROTLI_TRANSFORM_SHIFT_ALL` types), up to
64 (word list, transform list) combinations and an optional 64-entry context
map. The parsed metadata lives in fixed-size decoder fields (`wl_etc`, `tl_etc`
and `sd_etc`), with offsets into the copied bytes in the workbuf. Like the C
code:

- only one attached serialized dictionary can have custom word lists or
  transform lists (otherwise `"#unsupported multiple custom static
  dictionaries"`), and
- attaching a serialized dictionary without custom word lists or transform
  lists (only an LZ77 prefix) resets the static dictionary to the built-in
  (RFC 7932) one, even if an earlier serialized dictionary had custom lists.

Static dictionary references then mirror `ProcessCommandsInternal`: the word
list and transform list are selected by the literal context (`BROTLI_CONTEXT(p1,
p2, context_lookup)`) through the context map (if enabled), with the C code's
fall back to the other (word list, transform list) combinations when the
address is out of range, and words are transformed like
`BrotliTransformDictionaryWord` (including `Shift` for the shift transforms).
Transformed words can be up to 541 bytes long.

`add_dictionary` statuses (the C code just returns `BROTLI_FALSE`):

| Condition                                                      | Wuffs status                                          |
| -------------------------------------------------------------- | ----------------------------------------------------- |
| decoding has started                                           | `base."#bad call sequence"`                           |
| unknown `dict_type`                                            | `base."#bad argument"`                                |
| more than 15 compound dictionary chunks                        | `"#too many compound dictionaries"`                   |
| invalid serialized dictionary                                  | `"#bad shared dictionary"`                            |
| a second serialized dictionary with custom lists               | `"#unsupported multiple custom static dictionaries"`  |
| more than `0x7FFF_FFFF` bytes of dictionaries in total         | `"#too much dictionary data"`                         |
| `workbuf` shorter than the dictionaries so far plus this one   | `base."#bad workbuf length"`                          |

Differences from the C code:

- On failure, `add_dictionary` leaves the decoder unchanged. libbrotlidec's
  `BrotliDecoderAttachDictionary` can fail after partially updating its shared
  dictionary (e.g. after counting a serialized dictionary's LZ77 prefix or
  reading its number of word lists), which can affect later attach calls.
- libbrotlicommon only parses serialized dictionaries when built with
  `BROTLI_EXPERIMENTAL`, which is not the default. A default build rejects
  every serialized dictionary. This package always accepts them, matching an
  experimental build. The mimic tests skip serialized dictionary cases when
  the linked libbrotlidec rejects them; they pass, with no skips, when linked
  against a `BROTLI_EXPERIMENTAL` build.
- The C code holds the compound dictionary's total size in an `int`. This
  package rejects more than `0x7FFF_FFFF` bytes of dictionaries in total
  (counting serialized dictionaries in full), where the C code would overflow.
- `BROTLI_DECODER_ERROR_DICTIONARY_NOT_SET` cannot happen (see above).


## Metadata Blocks

libbrotlidec can report metadata blocks' contents through callbacks
(`BrotliDecoderSetMetadataCallbacks`). Wuffs code cannot call back into its
caller, so there is no equivalent: metadata blocks are skipped (validated and
consumed but not reported), exactly as libbrotlidec does without callbacks,
so the decoded output is the same. A future quirk could expose metadata
blocks, e.g. by suspending with a status when one starts.
