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
- Compound dictionary matches at encoder qualities 10 and 11 (see the
  "Encoder" section below).


## Encoder

This package also has an `encoder`, which implements the `base.io_transformer`
interface. It aims for byte-exact parity with the reference implementation's
encoder (libbrotlienc), version v1.2.1: for the same parameters and the same
sequence of operations, it produces exactly the same bytes. Like the decoder,
the Wuffs code mirrors the C code function by function, with comments like
"Mirrors BrotliCompressFragmentFast in compress_fragment.c". Its floating
point arithmetic (the C code's `double`s) uses `std/softfloat`, so that the
results are bit-exact regardless of the compiler and CPU.

All qualities (0 to 11) are implemented: qualities 0 and 1 (the C code's
`FAST_ONE_PASS_COMPRESSION_QUALITY` and `FAST_TWO_PASS_COMPRESSION_QUALITY`,
implemented by `compress_fragment.c` and `compress_fragment_two_pass.c`) and
qualities 2 to 9 (the C code's `EncodeData` path: a ring buffer, hashers,
`BrotliCreateBackwardReferences` with static dictionary matching, and
`WriteMetaBlockInternal`, which picks `BrotliStoreMetaBlockFast` at quality
2, `BrotliStoreMetaBlockTrivial` at quality 3 and, at qualities 4 to 9, the
literal context modeling heuristics, `BrotliBuildMetaBlockGreedy`,
`BrotliOptimizeHistograms` and `BrotliStoreMetaBlock`) and qualities 10 and
11 (the default is 11: the same `EncodeData` path with the H10 binary tree
hasher, `BrotliCreateZopfliBackwardReferences` at quality 10 and
`BrotliCreateHqZopfliBackwardReferences` at quality 11, and then
`ChooseContextMode`, `BrotliBuildMetaBlock`, `BrotliOptimizeHistograms` and
`BrotliStoreMetaBlock`). Large-window brotli (see "Encoder Large Window"
below) works at qualities 3 to 11 with lgwin 10 to 30. Like the C code,
qualities 0 to 2 ignore the large-window quirk.
Raw (LZ77 prefix) dictionaries (see "Encoder Dictionaries" below) work at
qualities 0 to 9, and, like the C code, qualities 0 and 1 ignore them. At
qualities 10 and 11, `transform_io` returns `"#unsupported encoder quality"`
if a dictionary is attached (the Zopfli path's dictionary lookups are not
implemented yet).

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
- `encode_data.wuffs`: the rest of `encode.c` that qualities 2 to 11 use
  (`EncodeData`, `WriteMetaBlockInternal`, `ShouldCompress`,
  `DecideOverLiteralContextModeling`, `ChooseContextMap`, `UpdateSizeHint`,
  `ExtendLastCommand`, etc), `ringbuffer.h`, the `quality.h` helpers and the
  workbuf layout.
- `encode_hasher.wuffs`: `hash.h` and the `hash_*_inc.h` hashers (H2, H3, H4,
  H54, H5, H6, H58, H68, H40, H41 and H42) and `SearchInStaticDictionary`
  (with `static_dict.c`'s matching), as one set of functions selected by the
  hasher type at run time.
- `encode_hasher_composite.wuffs`: the large-window hashers:
  `hash_composite_inc.h` (H35, H55 and H65, which combine H3, H54 or H6 with
  a rolling hasher) and `hash_rolling_inc.h` (`HROLLING_FAST` and
  `HROLLING`).
- `encode_dictionary.wuffs`: `compound_dictionary.c`
  (`CreatePreparedDictionary`, `AttachPreparedDictionary`), the raw
  dictionary cases of `BrotliEncoderPrepareDictionary` and
  `BrotliEncoderAttachPreparedDictionary` (as `add_dictionary`), the
  compound dictionary search of `hash.h` (`FindCompoundDictionaryMatch`,
  `LookupCompoundDictionaryMatch`) and the compound dictionary branch of
  `ExtendLastCommand`.
- `encode_backward_references.wuffs`: `backward_references.c` and
  `backward_references_inc.h` (`BrotliCreateBackwardReferences`),
  `command.h` and `ComputeDistanceCode`.
- `encode_bit_stream.wuffs`: `brotli_bit_stream.c` (`StoreMetaBlockHeader`,
  `BrotliStoreUncompressedMetaBlock`, `BrotliStoreMetaBlockTrivial`,
  `BrotliStoreMetaBlockFast`, `BrotliStoreMetaBlock`, block split codes,
  `EncodeContextMap` with move-to-front and run-length coding,
  `BuildAndStoreEntropyCodes`, etc).
- `encode_hash_to_binary_tree.wuffs`: `hash_to_binary_tree_inc.h` (the H10
  hasher of qualities 10 and 11, including `StoreRange`,
  `StitchToPreviousBlock` and `FindAllMatches`).
- `encode_static_dict.wuffs`: `static_dict.c`'s
  `BrotliFindAllStaticDictionaryMatches` (every transform), with the
  `static_dict_lut.h` tables.
- `encode_literal_cost.wuffs`: `literal_cost.c`
  (`BrotliEstimateBitCostsForLiterals`, with `utf8_util.c`'s
  `BrotliIsMostlyUTF8`), in f32 arithmetic via `std/softfloat`.
- `encode_zopfli.wuffs`: `backward_references_hq.c` (the `ZopfliNode` array,
  `ZopfliCostModel`, `StartPosQueue`, `UpdateNodes`,
  `ZopfliComputeShortestPath`, `ZopfliCreateCommands`,
  `BrotliCreateZopfliBackwardReferences` and
  `BrotliCreateHqZopfliBackwardReferences`), with the C code's f32 costs as
  binary32 bit patterns.
- `encode_store_hq.wuffs`: the quality 10 and 11 parts of
  `WriteMetaBlockInternal` (`ChooseContextMode`, the `metablock_builder` call
  and `BrotliStoreMetaBlock` with any literal context mode and context maps)
  and the quality 10 and 11 workbuf sizing.
- `encode_metablock_greedy.wuffs`: `BrotliBuildMetaBlockGreedy` (and its
  greedy block splitters, `metablock_inc.h`) and `BrotliOptimizeHistograms`
  for its output.
- `encode_util.wuffs`: little-endian workbuf accessors.
- `encode_block_splitter.wuffs`: `block_splitter.c` and
  `block_splitter_inc.h` (`BrotliSplitBlock`), as methods of the
  `metablock_builder` struct.
- `encode_cluster.wuffs`: `cluster.c` and `cluster_inc.h`
  (`BrotliClusterHistograms` and its helpers), also as `metablock_builder`
  methods.
- `encode_metablock.wuffs`: the `metablock_builder` struct and `metablock.c`
  (`BrotliInitDistanceParams`, `BrotliBuildMetaBlock`,
  `BrotliOptimizeHistograms`). See "Encoder Metablock Builder" below.
  Only qualities 10 and 11 use `BrotliBuildMetaBlock` (lower qualities use
  some of the smaller functions: `BrotliInitDistanceParams`,
  `PrefixEncodeCopyDistance`, `CommandRestoreDistanceCode` and
  `BrotliOptimizeHuffmanCountsForRle`).
- `encode_tester.wuffs`: TEST ONLY. The `metablock_tester` struct exposes
  the internal functions above to C test programs. It is not part of the
  encoder API.
- `common_encoder_tables.wuffs`: tables generated by `go run
  script/print-brotli-tables.go -encoder` from the C code (including the
  static dictionary hash tables of `dictionary_hash.c` and the
  `static_dict_lut.h` buckets and words).


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
- `add_dictionary!(dict_type: u32, dict: roslice u8, workbuf: slice u8)
  status` and `add_dictionary_workbuf_len(dict_len: u64) u64`: attach a raw
  dictionary, like `BrotliEncoderPrepareDictionary` plus
  `BrotliEncoderAttachPreparedDictionary`. See "Encoder Dictionaries" below.

`dst_history_retain_length` is 0: the encoder never re-reads `dst`.


### Encoder Quirks

Each quirk mirrors one `BrotliEncoderParameter`, with the value semantics of
`BrotliEncoderSetParameter`. Values are stored as is (and must fit in a u32,
otherwise `set_quirk` returns `"#bad argument"`). Like the C code, they are
sanitized (`SanitizeParams`, `ComputeLgBlock`, `ChooseDistanceParams`) at the
first `transform_io` call, after which `set_quirk` returns
`"#bad call sequence"`. Quirks and dictionaries (`add_dictionary`) can be set
in any order before that first call.

| Quirk                                    | C parameter                                    |
| ---------------------------------------- | ---------------------------------------------- |
| `QUIRK_ENCODER_QUALITY`                  | `BROTLI_PARAM_QUALITY` (default 11)            |
| `QUIRK_LGWIN`                            | `BROTLI_PARAM_LGWIN` (default 22)              |
| `QUIRK_LGBLOCK`                          | `BROTLI_PARAM_LGBLOCK`                         |
| `QUIRK_MODE`                             | `BROTLI_PARAM_MODE`                            |
| `QUIRK_DISABLE_LITERAL_CONTEXT_MODELING` | `BROTLI_PARAM_DISABLE_LITERAL_CONTEXT_MODELING` (0 or 1) |
| `QUIRK_SIZE_HINT`                        | `BROTLI_PARAM_SIZE_HINT`                       |
| `QUIRK_LARGE_WINDOW` (shared with the decoder) | `BROTLI_PARAM_LARGE_WINDOW` (0 or 1)     |
| `QUIRK_NPOSTFIX`                         | `BROTLI_PARAM_NPOSTFIX`                        |
| `QUIRK_NDIRECT`                          | `BROTLI_PARAM_NDIRECT`                         |
| `QUIRK_STREAM_OFFSET`                    | `BROTLI_PARAM_STREAM_OFFSET` (at most `1 << 30`) |

The quality quirk is not named `QUIRK_QUALITY` because that name would clash
with `base.QUIRK_QUALITY`. At qualities 0 and 1, only the quality, lgwin and
stream offset affect the output (lgblock, mode, size hint, large window,
npostfix, ndirect and literal context modeling are stored, sanitized and then
ignored, as in the C code). At qualities 2 to 9, they have the same effect as
in the C code: large window (see "Encoder Large Window" below), lgblock (at
qualities 4 and above; the C code's
`ComputeLgBlock` fixes it to 14 below that), npostfix, ndirect and the mode
(only `BROTLI_MODE_FONT` matters, through `ChooseDistanceParams`, at
qualities 4 and above), disabling literal context modeling (at qualities 5
and above) and the size hint (the hasher choice at qualities 4 and above and
the complex static context map at qualities 5 and above). An unset (zero)
size hint is replaced by the C code's `UpdateSizeHint` estimate.


### Encoder Workbuf Layout

Any attached dictionaries come first (see "Encoder Dictionaries" below): the
workbuf's first `D` bytes hold them (with their prepared hash tables) and
everything described in this section starts at offset `D` (so all of the
offsets and lengths below are relative to `workbuf[D ..]`, and `workbuf_len`
is `D` more than the lengths given here). `D` is 0 without dictionaries.

At qualities 0 and 1, the workbuf is the C code's metablock storage (its
`GetBrotliStorage` buffer): `workbuf_len` is `(2 << lgwin) + 503 + 16` bytes
(the storage for a `1 << lgwin` byte block plus 16 bytes of slack for 8-byte
bit writes), where lgwin is the sanitized value. A shorter workbuf still works
if every input block is shorter: a block of `n` bytes needs `(2 * n) + 519`
bytes, otherwise `transform_io` returns `"#bad workbuf length"`.

At qualities 2 to 9, the workbuf holds everything whose size depends on the
input or on the parameters, in this order (see `compute_layout_offset` in
`encode_data.wuffs`). Here `rb_bits` is `1 + max(lgwin, lgblock)` (the C
code's `ComputeRbBits`), `max_len` is `1 << min(rb_bits, 24)` (the C code's
`MaxMetablockSize`) and `cmds_cap` is the commands capacity (see below):

1. The metablock storage: `(2 * max_len) + 503 + 16` bytes, like at
   qualities 0 and 1 but for the largest metablock.
2. The hasher's memory (the C code's `common->extra[0 .. 3]`): the sum of
   `FN(HashMemAllocInBytes)` for the hasher that `ChooseHasher` picks. When
   the size hint quirk is unset (zero), that choice depends on the input's
   length (through `UpdateSizeHint`), so this is the larger of the two
   possible choices (for a size hint below or at least 1 MiB). With lgwin
   above 24 (large window), the composite hashers H35, H55 and H65 add their
   rolling hasher's table (`(1 << 24)` u32le values, 64 MiB) after H3's,
   H54's or H6's memory.
3. The commands: `16 * cmds_cap` bytes, in the C struct order: `insert_len_`
   (u32le), `copy_len_` (u32le, the copy length in the low 25 bits and
   `copy_code - copy_len` in the high 7 bits), `dist_extra_` (u32le),
   `cmd_prefix_` (u16le) and `dist_prefix_` (u16le). `cmds_cap` is
   `min(prior + ((1 << lgblock) / 2) + 1, (max_len / 2) + 1)`, where `prior`
   is `(max_len / 8) - 1` (and at most `MAX_NUM_DELAYED_SYMBOLS - 1`, which
   is `0x2FFE`, below quality 4): `EncodeData` keeps (merges with the next
   input block) fewer than that many commands and, like the C code's
   `num_commands_ + bytes / 2 + 1` allocation, each input block of `n` bytes
   adds at most `(n / 2) + 1` commands.
4. At qualities 4 and above, `BrotliBuildMetaBlockGreedy`'s `MetaBlockSplit`
   and `BrotliStoreMetaBlock`'s scratch space:
   - the block types (u8) and then the block lengths (u32le) of the literal,
     command and distance block splits, with room for `(max_len / 512) + 1`,
     `(cmds_cap / 1024) + 1` and `(cmds_cap / 512) + 1` blocks (the C
     code's `max_num_blocks`, given that a metablock has at most `max_len`
     literals and `cmds_cap` commands),
   - the histograms (u32le counts, without the C code's `total_count_` and
     `bit_cost_` fields, which are not needed here): up to 260 literal
     histograms (`(256 / 13) + 1` block types of 13 contexts), 257 command
     histograms and 257 distance histograms of 256, 704 and 544 counts,
   - the block splitters' `combined_histo` (2 * 13 * 256 u32le counts),
   - the literal context map (256 * 64 bytes),
   - `EncodeContextMap`'s `rle_symbols` (256 * 64 u32le values) and
   - the block encoders' depths (u8) and then bits (u16le) for up to 256
     histograms per category.
5. The ring buffer (the C code's `RingBuffer.buffer_`): `2 + (1 << rb_bits) +
   (1 << lgblock) + 7` bytes. The first 2 bytes are the copy of the last two
   bytes that precede `data_`, the tail (`1 << lgblock` bytes) mirrors the
   start of the buffer and the 7 trailing bytes are the C code's slack for
   8-byte loads.

Unlike the C code, which allocates its storage, ring buffer and command
buffers as it goes (for the actual metablock and block sizes, and the actual
input length), `workbuf_len` is the worst case for the sanitized parameters,
so that one workbuf works for any input. A shorter workbuf still works for
short inputs, because the ring buffer is the last region and the encoder
only touches its first `2 + n + 7` bytes for an input of `n` bytes, as long
as the ring buffer never wraps around. Precisely, at qualities 2 to 11
(where, at qualities 10 and 11, `workbuf_len` means its `min_incl`):

- `transform_io` returns `"#bad workbuf length"` if the workbuf is shorter
  than `workbuf_len - R + 18`, where `R` is the ring buffer region's length
  (`2 + (1 << rb_bits) + (1 << lgblock) + 7`), i.e. if it does not reach 16
  bytes into the ring buffer's data.
- If any `transform_io` call gets a workbuf shorter than `workbuf_len`, then
  from then on (for the rest of the stream), before copying input into the
  ring buffer, `transform_io` returns `"#bad workbuf length"` (without
  consuming that input) if the total input so far (not counting metadata)
  would be `n` bytes with `n + 16 > (1 << rb_bits)` (the ring buffer would
  wrap, or its tail would be needed) or with the workbuf shorter than
  `workbuf_len - R + 2 + n + 16`.

So, for a total input of `n` bytes with `n + 16 <= (1 << rb_bits)`, a
workbuf of `workbuf_len - R + n + 18` bytes produces the same output as one
of `workbuf_len` bytes. The parity test checks this with exactly that length
(and that one byte less fails). This matters most for large-window brotli,
whose ring buffer is up to 2 GiB. Some examples of `workbuf_len` (in MiB,
with the default lgblock and an unset size hint):

| quality | lgwin 10 | lgwin 16 | lgwin 18 | lgwin 22 | lgwin 24 |
| ------- | -------- | -------- | -------- | -------- | -------- |
| 0, 1    | 0.0      | 0.1      | 0.5      | 8.0      | 32.0     |
| 2, 3    | 0.5      | 1.0      | 2.1      | 24.6     | 64.6     |
| 4       | 7.9      | 7.9      | 9.8      | 47.3     | 103.4    |
| 5       | 4.4      | 4.4      | 7.0      | 45.9     | 102.0    |
| 7       | 4.4      | 4.4      | 13.8     | 51.4     | 107.5    |
| 9       | 5.1      | 5.1      | 39.5     | 77.1     | 133.2    |

With the large-window quirk (at qualities 3 and above; lgwin up to 24 gives
the same numbers as without it), larger lgwin values double the ring buffer
for each extra bit, and lgwin above 24 adds the composite hashers' 64 MiB
rolling hash table (at quality 4 and at qualities 5 to 9 with lgwin of at
least 19, only for a size hint of at least 1 MiB, or an unset one, as
`ChooseHasher` otherwise keeps H3, H5 or H58):

| quality | lgwin 25 | lgwin 26 | lgwin 28 | lgwin 30 |
| ------- | -------- | -------- | -------- | -------- |
| 3       | 160.6    | 224.6    | 608.6    | 2144.6   |
| 4       | 199.4    | 263.4    | 647.4    | 2183.4   |
| 5       | 197.5    | 261.5    | 645.5    | 2181.5   |
| 7       | 203.5    | 267.5    | 651.5    | 2187.5   |
| 9       | 229.2    | 293.2    | 677.2    | 2213.2   |

At qualities 10 and 11, `workbuf_len` is a range instead (see the next
section).

Fixed-size state (the hash table and command and literal buffers of
qualities 0 and 1, the Huffman scratch space, the greedy block splitters'
state, the block encoders' block type and length codes, etc) lives in the `encoder` struct (which is therefore
about 2.5 MiB). It does not need zero-initializing:
`WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED` is fine. At
qualities 0 and 1, the input block is not copied: the encoder compresses
directly from `src`. At qualities 2 to 9, like the C code, the input is
copied into the ring buffer.


### Qualities 10 and 11 Workbuf Length

At qualities 10 and 11, the workbuf has the same regions as at quality 9
(with `cmds_cap` computed the same way), except that:

- region 2 is the H10 hasher: `4 << 17` bytes of buckets (u32le) and then
  the binary tree forest (`8 << lgwin` bytes, u32le),
- region 4 keeps only the literal context map, `rle_symbols` and the block
  encoders' depths and bits, followed by a variable region, and
- the ring buffer (region 5) is still the last region, after the variable
  region.

The variable region's length is set at the first `transform_io` call: it is
its minimum length (see below) plus however much of the workbuf beyond
`workbuf_len().min_incl` there is (up to the worst case), and the ring
buffer moves along with it. Later `transform_io` calls should pass a
workbuf of the same length. A workbuf shorter than `min_incl` follows the
short workbuf rules above (with the minimum variable region).

The variable region holds, at different times, the Zopfli scratch space
(while `EncodeData` computes the backward references: `(n + 1)` 16-byte
`ZopfliNode`s, `(n + 2)` f32 literal costs and, at quality 10, room for one
position's matches or, at quality 11, `n` u32 match counts followed by the
8-byte `BackwardMatch`es of the whole input block, for an input block of `n`
bytes) and then the `metablock_builder`'s workbuf (see "Encoder Metablock
Builder" below), whose results the metablock store then reads.

The C code allocates these as it goes, for the actual input, and their worst
cases are huge (see `workbuf_len_worst_case` below), so `workbuf_len` returns
a range whose `max_incl` is the C code's worst case for the sanitized
parameters (saturating at `0xFFFF_FFFF_FFFF_FFFF`) and whose `min_incl` is a
practical bound. Both have the same fixed regions, with a variable region of
`max(Z, M)` bytes, where `L` is the most literals that a metablock
can hold (`min(max_len, (max_len / 8) + (1 << lgblock))`, since `EncodeData`
merges another input block only while there are fewer than `max_len / 8`
literals) and:

- `Z` is the Zopfli scratch space for `n = 1 << lgblock` bytes, with up to
  100 `BackwardMatch`es per byte (`max_incl`: up to 2 from the short
  distance scan, 64 from the binary tree and 34 from the static dictionary)
  or 16 per byte (`min_incl`), and
- `M` is the `metablock_builder`'s `workbuf_len_worst_case(L, cmds_cap)`
  (`max_incl`) or `workbuf_len_practical(L, cmds_cap)` (`min_incl`).

`workbuf_len_practical` is `workbuf_len_worst_case` with these caps:
`FindBlocks` makes at most `(k / 16) + 1` blocks for `k` literals, commands
or distances (instead of one block per symbol), `ClusterBlocks`' first phase
makes at most `(k / 64) + 64` clusters and there are at most that many (and
at most 256) block types. Over 142 files from `test/data` (including
`harvesters.bmp`, 3 MiB, and other images, text, JSON and binaries) and some
synthetic inputs, at qualities 10 and 11 and lgwins 16 and 24, the C
algorithm's actual maxima were one block per 83 literals (per 85 commands,
per 154 distances), one first-phase cluster per 286 literals (per 915
commands) and about 7.4 `BackwardMatch`es per byte (for random text over a
2-letter alphabet; at most 2.3 for the files), so the caps have a margin of
at least 4x. The largest `metablock_builder` workbuf used was 43.7 MB, for
2.1 MiB of literals.

This is one documented deviation from the C code: with a workbuf of at least
`min_incl` but less than `max_incl` bytes, an input that needs more than the
variable region holds makes `transform_io` return `"#bad workbuf length"`
(where the C code would allocate more memory). A workbuf of `max_incl` bytes
never fails. In MiB (with the default lgblock, which is
`min(18, lgwin)` for lgwin above 16 and 16 otherwise), `min_incl` and
`max_incl` are:

| quality | lgwin 10      | lgwin 16      | lgwin 18      | lgwin 22       | lgwin 24       |
| ------- | ------------- | ------------- | ------------- | -------------- | -------------- |
| 10      | 91.7 .. 288.0 | 92.2 .. 288.5 | 128.7 .. 914.2 | 403.0 .. 5116.1 | 775.5 .. 9678.1 |
| 11      | 91.7 .. 288.0 | 92.2 .. 288.5 | 128.7 .. 914.2 | 403.0 .. 5116.1 | 775.5 .. 9678.1 |

(Quality 11's extra `BackwardMatch` storage fits in the space that the
`metablock_builder` needs anyway, so the two rows are equal. With lgblock
24, `min_incl` is about 2.1 GiB at quality 10 and 2.6 GiB at quality 11.)

At qualities 10 and 11, every command's distance code is less than `1 <<
25` (distances are at most `(1 << lgwin) - 16` or, for static dictionary
words, at most the C code's `max_distance` check in `FindAllMatchesH10` and
less than `(1 << 24) + (1 << 17)`, plus 15 for the distance code), which
satisfies the `metablock_builder`'s precondition (distance codes at most the
distance params' `max_distance`, which is at least `(1 << 26) - 4` without
large-window brotli).


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

At qualities 2 to 9 the C code copies the input into its ring buffer and
only compresses it (`EncodeData`) when an input block (`1 << lgblock` bytes)
is full, or on a flush or finish, so the output depends on the chunking only
through the flushes (and the `UpdateSizeHint` estimate, which depends on how
much input is available when the first block is compressed). The Wuffs
encoder makes the same `EncodeData` calls, at the same points. One
`transform_io` call can correspond to several C calls: with a stream offset
(the C code's "flint"), a C call can return early, with input left, after
emitting the flint, and its callers then call it again.


### Encoder Large Window

The `QUIRK_LARGE_WINDOW` quirk mirrors `BROTLI_PARAM_LARGE_WINDOW`
(large-window brotli is an incompatible extension of RFC 7932, decodable by
this package's decoder with the same quirk, see "Large Window" above, and by
libbrotlidec with `BROTLI_DECODER_PARAM_LARGE_WINDOW`). Like the C code's
`SanitizeParams`, `EnsureInitialized` and `ChooseHasher`:

- At qualities 0 to 2 (the C code's `MAX_QUALITY_FOR_STATIC_ENTROPY_CODES`
  and below), the quirk is ignored: lgwin is clamped to 24 and the output is
  standard brotli.
- At qualities 3 and above, lgwin is clamped to `[10 ..= 30]` (instead of
  `[10 ..= 24]`) and the stream starts with the 14-bit large-window header
  (`EncodeWindowBits`), even for lgwin 24 and below. The distance parameters
  (`BrotliInitDistanceParams` with `large_window`) allow distances up to
  `BROTLI_MAX_ALLOWED_DISTANCE` with an alphabet of up to
  `BROTLI_LARGE_MAX_DISTANCE_BITS` (62) distance bits, so the distance
  Huffman codes are stored with a larger alphabet size than without the
  quirk, even when no distance is large. Backward distances are at most
  `(1 << lgwin) - 16`, so their extra bits (at most 30) fit in a command's
  32-bit `dist_extra_`, as in the C code.
- lgwin above 24 changes the hasher (only at qualities 3 to 9; the C code's
  qualities 10 and 11 use H10 regardless): H3 becomes H35 (H3 plus
  `HROLLING_FAST`), H54 becomes H55 (H54 plus `HROLLING_FAST`) and H6 and
  H68 become H65 (H6, not H68, plus `HROLLING`). H5, H58 and H40 to H42 stay
  as they are, so qualities 5 to 9 only use H65 when the size hint (set or
  estimated by `UpdateSizeHint`) is at least 1 MiB (and lgwin is at least 19,
  which it is). The rolling hashers find matches of at least 32 bytes at any
  distance in the window (`HROLLING_FAST` only at positions and distances
  that are multiples of 4) and do not search the static dictionary.
- lgblock is unaffected (it is at most 24), but `rb_bits` (and so the ring
  buffer) grows with lgwin. Metablocks are still at most `1 << 24` bytes.
- At qualities 10 and 11, the H10 hasher's binary tree forest grows with
  lgwin (`8 << lgwin` bytes, so 8 GiB at lgwin 30, as in the C code, which
  allocates the same), and the Zopfli and static dictionary distance limits
  use the large-window `max_distance`.

Positions (the C code's `size_t` and the `WrapPosition`'ed `uint32_t`
values) and distances are `base.u64` throughout the encoder, so nothing is
truncated for windows up to `1 << 30`. The rolling hashers' table holds
32-bit positions, like the C code's.


### Encoder Dictionaries

`encoder.add_dictionary!(dict_type: base.u32, dict: roslice base.u8, workbuf:
slice base.u8) base.status` mirrors the C API's
`BrotliEncoderPrepareDictionary(BROTLI_SHARED_DICTIONARY_RAW, ...)`
followed by `BrotliEncoderAttachPreparedDictionary`: it attaches a raw (LZ77
prefix) dictionary, as the next chunk of the C code's compound dictionary.
The decoder must be given the same dictionaries, in the same order (with
this package's `decoder.add_dictionary` or libbrotlidec's
`BrotliDecoderAttachDictionary`, as `DICTIONARY_TYPE_RAW` /
`BROTLI_SHARED_DICTIONARY_RAW`).

Ordering: like quirks, dictionaries must be added before the first
`transform_io` call (the C API's "before the first
`BrotliEncoderCompressStream`"). Quirks and dictionaries can be set in any
order (preparing a raw dictionary does not depend on any parameter: the
`quality` argument of `BrotliEncoderPrepareDictionary` is unused for raw
dictionaries). Call `workbuf_len` after the last `add_dictionary` and
`set_quirk` call.

Like the decoder, the encoder copies each dictionary into the workbuf, so
every `add_dictionary` and `transform_io` call must be passed the same
workbuf (or at least one whose first bytes hold the same contents). Each
dictionary is followed by its prepared hash tables (the C code's
`PreparedDictionary`), which `add_dictionary` builds exactly like
`CreatePreparedDictionary` and `CreatePreparedDictionaryWithParams` in
`compound_dictionary.c`, so that the compound dictionary search finds the
same candidates in the same order:

- The hash table size depends only on the dictionary length `n`: 17 bucket
  bits and 7 slot bits for `n` up to 2 MiB, one more of each for each
  doubling of `n` (18 and 8 up to 4 MiB, etc), up to 22 and 12 (for more than
  32 MiB). Each position `i` with `i + 7 < n` is hashed (the low 40 bits of
  its 8 little-endian bytes, times `kPreparedDictionaryHashMul64Long`), with
  at most 32 positions per bucket (the most recent ones), and the bucket
  limits are lowered per slot so that each slot has fewer than 65535 items.
- Its layout, after the `n` dictionary bytes, is `slot_offsets` (`1 <<
  slot_bits` u32le values), `heads` (`1 << bucket_bits` u16le values, 0xFFFF
  for an empty bucket) and `items` (`num_items` u32le values, each a
  dictionary position, with the high bit marking the end of a bucket's
  chain), like the C struct's dynamic members.

So the workbuf layout is:

    [0 .. D)    the dictionaries, each one being its n bytes, then
                (4 << slot_bits) + (2 << bucket_bits) + (4 * num_items)
                bytes of hash tables. num_items is at most max(0, n - 7).
    [D .. )     the encoder's other regions (see "Encoder Workbuf Layout").

Preparing a dictionary needs some scratch space (the C code's temporary
"bloated" hasher: `(6 << bucket_bits) + (4 * n)` bytes), which `add_dictionary`
puts after the largest possible hash tables. So `add_dictionary`'s workbuf
must be at least `add_dictionary_workbuf_len(n)` bytes long: `D` (for the
dictionaries added so far) plus `n + (4 << slot_bits) + (2 << bucket_bits) +
(4 * max(0, n - 7))` plus that scratch space. For example, for a 1 MiB
dictionary that is `D` plus about 9.8 MiB, and for a 40 MiB dictionary `D`
plus about 384 MiB. After the call, `D` grows by the dictionary's bytes and
actual hash tables (e.g. by at most 5.3 MiB for 1 MiB of dictionary).

The compound dictionary then affects compression like in the C code:

- Qualities 0 and 1 (`BrotliEncoderCompressStreamFast`) ignore it.
- At qualities 2 to 9, its total size (the C code's `gap`) is added to the
  static dictionary's base distance (`dictionary_start + gap` in
  `CreateBackwardReferences`), and distances up to `dictionary_start + gap`
  (which include compound dictionary references) update the distance cache.
  `ExtendLastCommand` can extend a copy from the compound dictionary.
- Only the hashers that `BrotliCreateBackwardReferences` instantiates with
  `ENABLE_COMPOUND_DICTIONARY` search the dictionaries (with
  `LookupCompoundDictionaryMatch`, after the hasher's own search, for each
  chunk in order): H5, H6, H58, H68, H40, H41, H42, H55 and H65. So
  qualities 5 to 9 search them, quality 4 only with the large-window H55
  (lgwin above 24 and a big size hint), and qualities 2 and 3 (H2, H3, H35)
  and quality 4 with H4 or H54 never do (the C code falls back to its
  non-dictionary variant for those hashers). The search only finds matches
  that are within `params.dist.max_distance` (`BROTLI_MAX_DISTANCE`, or
  `BROTLI_MAX_ALLOWED_DISTANCE` for large-window brotli) and that do not
  cross a chunk's end.
- With `QUIRK_STREAM_OFFSET`, `dictionary_start` is `min(position +
  stream_offset, max_backward_limit)`, as in the C code (whose output then
  continues another stream and can only be decoded after it).

`add_dictionary` statuses (the C code's functions return `NULL` or
`BROTLI_FALSE`):

| Condition                                                    | Wuffs status                                   | C code |
| ------------------------------------------------------------ | ---------------------------------------------- | ------ |
| `transform_io` has been called                               | `base."#bad call sequence"`                    | accepted (see below) |
| `DICTIONARY_TYPE_SERIALIZED`                                 | `"#unsupported serialized encoder dictionary"` | `BrotliEncoderPrepareDictionary` returns `NULL` |
| unknown `dict_type`                                          | `base."#bad argument"`                         | `BrotliEncoderPrepareDictionary` returns `NULL` |
| a 16th dictionary                                            | `"#too many compound dictionaries"`            | `BrotliEncoderAttachPreparedDictionary` fails |
| more than `0x7FFF_FFFF` bytes of dictionaries in total       | `"#too much dictionary data"`                  | accepted |
| `workbuf` shorter than `add_dictionary_workbuf_len(n)`       | `base."#bad workbuf length"`                   | n/a |

An empty dictionary is accepted (and counts towards the 15), like in the C
code. On failure, the encoder's state is unchanged. A `transform_io` call
with a workbuf shorter than `D` returns `base."#bad workbuf length"`.

Differences from the C code:

- Serialized (shared) dictionaries are rejected, exactly like the default
  (non-`BROTLI_EXPERIMENTAL`) libbrotlienc build, whose
  `BrotliEncoderPrepareDictionary` returns `NULL` for them. (The decoder, in
  contrast, accepts them, like an experimental libbrotlidec build.) With an
  experimental build, the C encoder would also use their custom static
  dictionaries; that is not implemented.
- The C code's `BrotliEncoderAttachPreparedDictionary` accepts a raw
  dictionary even after compression has started (it only checks
  `is_initialized_` for serialized dictionaries). The compound dictionary's
  size then changes mid-stream, so the static dictionary references before
  and after that point use different distance bases, and the output is
  generally not decodable. Here, `add_dictionary` returns `"#bad call
  sequence"` once `transform_io` has been called.
- The C code holds each dictionary's length in a `uint32_t` (and item
  offsets in 31 bits). This package rejects more than `0x7FFF_FFFF` bytes
  of dictionaries in total (like its decoder).
- When `ExtendLastCommand` extends a copy past the end of a compound
  dictionary chunk into an empty chunk, the C code reads (one or more
  bytes) past the end of that empty chunk's (zero-length) data. Here, the
  extension stops there. This needs an empty dictionary attached after a
  non-empty one and a copy extension that reaches the non-empty one's end
  (with empty dictionaries only before the non-empty ones, the behaviors are
  the same).
- Like the C code's `BrotliEncoderPrepareDictionary` with the default
  allocator, there is no separate "prepared dictionary" object that could be
  shared between encoders: each encoder prepares its own copy (the C code's
  "lean" prepared dictionaries reference the caller's bytes instead).


### Encoder Testing

`wuffs test -mimic std/brotli` (including its encoder dictionary tests)
checks byte-exact parity with libbrotlienc
over a grid of qualities (0 to 11), lgwins, modes, input chunkings, flushes,
metadata blocks, size hints and other parameters (lgblock, npostfix, ndirect,
disabling literal context modeling, stream offset, large window), as well as
round trips through the decoder. Its large-window tests cover qualities 3 to
9 with lgwin 18 to 30 (and the H35, H55 and H65 hashers) on small files and
on a 46 MiB input with repeats at distances of about 20 and 40 MiB, decoded
by both this package's decoder and libbrotlidec.
`script/manual-test-brotli-encoder-parity.c` runs a larger grid over every
file in `test/data/brotli`, some larger files in `test/data` (up to about
400 KiB) and some synthetic inputs (up to 1 MiB). Its `-q=LO-HI` and
`-shard=K/N` options select the qualities (0 to 11) and split the inputs
over several processes, `-lw` runs only its large-window grid (lgwin 10 to 30 at
qualities 2 to 9, with various chunkings, flushes, size hints and other
parameters, each also with the shortest workbuf that should work) and `-big`
replaces the inputs with synthetic ones of 3 to 70 MiB with repeats at
distances of up to 40 MiB. `-dict` replaces the usual grid with a
dictionary grid (see "Encoder Dictionaries" above): for each input, seven
sets of raw dictionaries (a prefix of the input, the whole input,
`test/data/enwik5`, noise plus the input's middle third, 15 pieces of the
input, tiny and empty dictionaries, and the input's swapped halves plus some
text), with qualities 0 to 9, lgwins, chunkings, flushes, metadata blocks,
size hints, stream offsets, large window and other parameters, plus
dictionaries of 2 MiB to 40 MiB (every hash table size) with a 1 MiB input.
Each dictionary configuration's output is also decoded by libbrotlidec and
by this package's decoder (with the same dictionaries), except with a
stream offset, and the whole-input and big dictionaries check that the
output is smaller than without them (at qualities 5 and above, unless it is
already tiny). With `-dict` (and `-q=0-9`) it runs 419,802 configurations,
all byte-exact against libbrotlienc v1.2.1 on x86-64, with 356,546 round
trips through both decoders. With
`-q=0-9` (the default) and without `-dict` it runs 814,706
configurations (96,688 of them in the large-window grid), all byte-exact
against libbrotlienc v1.2.1 on x86-64. With `-lw -big` it runs 720 more
(on 3, 6 and 12 MiB inputs with the large-window grid and on 24, 40 and
70 MiB inputs with one-shot and streaming large-window configurations at
lgwin 24 to 30), also all byte-exact.

At qualities 10 and 11 (which use the smaller grid for inputs longer than
64 KiB, and the `workbuf_len` minimum, so that the practical bound is
exercised), `-q=10-11` ran 144,036 configurations over 179 inputs before
the large-window grid was added to the default grid, and, afterwards,
`-q=10-11 -lw` ran 24,172 configurations (22,990 also with the shortest
workbuf) and `-q=10-11` on a third of the inputs (shards 0, 3, 6, etc of
60) another 55,450 (7,740 also with the shortest workbuf), all byte-exact,
except for 474 configurations skipped because their workbuf would be more
than 12 GiB (large-window brotli with lgwin 30 and lgblock 24). `-oneshot=W`
checks one configuration per input and quality (lgwin W, the whole input
in one `FINISH` step), e.g. for the 3 MiB `test/data/harvesters.bmp` at
qualities 10 and 11 with lgwin 22 and 24, also byte-exact. At qualities 10
and 11 the Wuffs encoder is roughly 7 times slower than libbrotlienc,
so the full `-q=10-11` grid takes a few CPU hours.


### Encoder Differences from the C Code

- At qualities 0 and 1, the input block is not copied (see the workbuf
  layout above). Output is always staged in the workbuf (or a small internal
  buffer) and then copied to `dst`. The C code sometimes writes directly to
  `next_out`. The bytes are the same.
- The C code instantiates each hasher (and the block splitters) from
  templates. Here, each template is one set of functions, parameterized at
  run time (e.g. by the hasher type). The H58 and H68 hashers' SIMD tag
  matching (`GetMatchingTagMask` in `matching_tag_mask.h`) is computed one
  tag at a time, which finds the same candidates in the same order. Like the
  x86-64 GCC reference build (where `BROTLI_MAX_SIMD_QUALITY` is 6, as in
  any build with a 64-bit `__builtin_ctzll`), qualities 5 and 6 with lgwin
  above 16 use H58 or H68 instead of H5 or H6. libbrotlienc builds where
  `BROTLI_MAX_SIMD_QUALITY` is undefined (no 64-bit count-trailing-zeros
  builtin) or 7 (MSVC on x86-64) choose differently and so produce
  different (but valid) output at those qualities.
- The composite hashers (H35, H55 and H65) are not separate hasher types
  here: `hasher_type` is their first hasher (H3, H54 or H6) and a non-zero
  `hr_jump` (4 for `HROLLING_FAST`, 1 for `HROLLING`) adds the rolling
  hasher, whose functions run after the first hasher's, like the C code's
  composite `FN(Prepare)`, `FN(StitchToPreviousBlock)` and
  `FN(FindLongestMatch)`. The C code's composite hasher defers its inner
  hashers' `FN(Initialize)` to its first `FN(Prepare)`. Here, the first
  hasher's `FN(Initialize)` runs in `hasher_setup` (it does not touch the
  hasher's memory, so this is equivalent) and the rolling hasher's at the
  first `hasher_prepare`. The C code's composite gives its first hasher a
  copy of its `HasherCommon` (with zeroed static dictionary statistics);
  here, the first hasher uses the encoder's statistics, which the composite
  itself never uses, so the result is the same.
- The workbuf's regions are in a different order than the C code's
  allocations and the ring buffer is fully reserved from the start (see the
  workbuf layout above). This does not change the output.
- `BrotliBuildMetaBlockGreedy`'s two kinds of block splitter (one for a
  single histogram per block type, `metablock_inc.h`, and one for several
  literal contexts, `ContextBlockSplitter` in `metablock.c`) are one
  implementation here, as they make the same decisions with one context.
- Wuffs' C code generator does not support `slice base.u16` or
  `slice base.u32` arguments (and mishandles `copy_from_slice` on `base.u16`
  arrays), so some functions (e.g. `create_huffman_tree`,
  `convert_bit_depths_to_symbols`) take a selector naming one of the
  encoder's arrays instead of a slice.
- Like the C code, there is no fallback to an uncompressed stream (that is
  only in the C code's one-shot `BrotliEncoderCompress`).
- At qualities 10 and 11, a workbuf between `workbuf_len`'s `min_incl` and
  `max_incl` can be too short for an unusual input, which then fails with
  `"#bad workbuf length"` (see "Qualities 10 and 11 Workbuf Length").
- At qualities 10 and 11, the C code's f32 costs (`ZopfliNode` costs, the
  cost model and literal costs) are computed with `std/softfloat`'s binary32
  operations, rounding each operation to f32 like an SSE2 build (whose
  `float` arithmetic has no excess precision).


### Encoder Metablock Builder

The `metablock_builder` struct (a `pri struct`, embeddable in the encoder
with one field, e.g. `mb : metablock_builder` in the private data section)
implements the quality 10 and 11 metablock construction (`BrotliBuildMetaBlock`)
and `BrotliOptimizeHistograms`. The encoder has one (its `mb` field) and
uses it at qualities 10 and 11 (and its smaller helpers at lower
qualities). Its
fixed-size state (about 0.8 MiB: FindBlocks' cost arrays and a 64Ki-entry
FastLog2 cache in its `bit_cost`) lives in the struct; everything that the C
code allocates on the heap lives in a caller-supplied workbuf. The FastLog2
cache (like the encoder's own `bit_cost` one) is filled lazily, one entry at
a time, when `fast_log2_memo` first needs that entry (a bitmap, zeroed when the
struct is initialized, records which entries are valid): typically only a few
thousand of the 65536 entries are ever used, and each one costs a (slow, exact)
software `log2`.

Data formats (all little-endian, matching the C structs' memory layout on a
64-bit little-endian machine, so that results can be compared with `memcmp`):

- A command is 16 bytes: `insert_len_` (u32), `copy_len_` (u32), `dist_extra_`
  (u32), `cmd_prefix_` (u16), `dist_prefix_` (u16), as built by `command.h`'s
  `InitCommand` / `InitInsertCommand`.
- A histogram is `(4 * data_size) + 16` bytes: `data_size` u32 counts, the
  u64 `total_count_` and the binary64 bits of `bit_cost_`. `data_size` is 256
  (literals, 1040 bytes), 704 (commands, 2832 bytes) or 544 (distances, 2192
  bytes).
- A BlockSplit's types are u8s and its lengths u32s. Context maps are u32s.
- Doubles are binary64 bit patterns (u64), computed with `std/softfloat` in
  the C code's operation order.

Usage, mirroring the C code's `WriteMetaBlockInternal` for quality >= 10:

```
// Once per metablock. The params are the C code's block_params, a copy of
// the encoder's params: init_distance_params must be called each time,
// because build_meta_block overwrites the dist_etc fields with the chosen
// distance params.
this.mb.set_params!(quality: q, large_window: lw,
        disable_literal_context_modeling: dlcm)
this.mb.init_distance_params!(npostfix: np, ndirect: nd, large_window: lw)
status = this.mb.build_meta_block!(wb: workbuf, ringbuffer: rb, pos: pos,
        mask: mask, prev_byte: pb, prev_byte2: pb2, cmds: cmds,
        num_commands: n, literal_context_mode: mode)
// Optional (quality >= 4): ndc is min(this.mb.dist_alphabet_size_limit, 544).
this.mb.optimize_histograms!(wb: workbuf, num_distance_codes: ndc)
```

The exact signatures are:

- `set_params!(quality: u32, large_window: bool,
  disable_literal_context_modeling: bool)`.
- `init_distance_params!(npostfix: u32, ndirect: u32, large_window: bool)`
  (`BrotliInitDistanceParams`; sets `dist_postfix_bits`,
  `dist_num_direct_codes`, `dist_alphabet_size_max`,
  `dist_alphabet_size_limit` and `dist_max_distance`).
- `build_meta_block!(wb: slice u8, ringbuffer: roslice u8, pos: u64, mask:
  u64, prev_byte: u8, prev_byte2: u8, cmds: slice u8, num_commands: u64,
  literal_context_mode: u32) status`. `ringbuffer` must be at least `mask + 1`
  bytes. `literal_context_mode` is a `ContextType` (0 = LSB6, 1 = MSB6, 2 =
  UTF8, 3 = SIGNED). Like the C code, it rewrites the commands'
  `dist_prefix_` and `dist_extra_` fields for the chosen distance params and
  leaves those params in the `dist_etc` fields. Like the encoder's commands,
  every command's distance code must be at most the given params'
  `max_distance` (with larger, invalid distance codes, the C code's
  `ComputeDistanceCost` writes past the end of its distance histogram, which
  the Wuffs code cannot mirror: it ignores out-of-range histogram symbols
  instead). It returns
  `"#bad workbuf length"` if `wb` is too short (the results are then invalid
  but `cmds` may already be rewritten, so retrying needs the original
  commands).
- `optimize_histograms!(wb: slice u8, num_distance_codes: u32)` works on the
  MetaBlockSplit described by the fields below, whether `build_meta_block`
  or the caller (e.g. a greedy builder) filled them in.
- `workbuf_len_worst_case(num_literals: u64, num_commands: u64) u64` is an
  upper bound on the workbuf length that `build_meta_block` needs, where
  `num_literals` is the sum of the commands' insert lengths. It assumes the
  worst case for every run-time-sized allocation (one `FindBlocks` block per
  symbol, 256 block types with 64 literal contexts each, full histogram pair
  queues), so it is far larger than what real inputs need: about 76 MiB for
  a 1 KiB metablock, 2.6 GiB for 1 MiB of literals and 128 Ki commands, and
  41 GiB for 16 MiB of literals and 4 Mi commands. These are the C code's
  own worst cases (`ClusterBlocks` and `BrotliClusterHistograms` size their
  histogram pair queues by the number of blocks and clusters), not artifacts
  of the port. `wb_high` records what a call actually used: over the parity
  harness's inputs, at most about 42 MB (for 1.3 MiB of literals split into
  256 literal block types).
- `workbuf_len_practical(num_literals: u64, num_commands: u64) u64` is the
  same bound with caps on the numbers of blocks and clusters (see
  "Qualities 10 and 11 Workbuf Length"), which the encoder uses for its
  `workbuf_len().min_incl`.

The MetaBlockSplit result is described by these fields, indexed by category
(0 = literal, 1 = command, 2 = distance), with offsets into `wb`:

| Field                  | C code                                          |
| ---------------------- | ----------------------------------------------- |
| `split_num_types[c]`   | `{literal,command,distance}_split.num_types`    |
| `split_num_blocks[c]`  | `..._split.num_blocks`                          |
| `split_types_off[c]`   | `..._split.types` (u8 array)                    |
| `split_lengths_off[c]` | `..._split.lengths` (u32 array)                 |
| `context_map_off[c]`   | `{literal,distance}_context_map` (u32 array; none for commands) |
| `context_map_size[c]`  | `{literal,distance}_context_map_size`           |
| `histograms_off[c]`    | `{literal,command,distance}_histograms`         |
| `histograms_size[c]`   | `..._histograms_size`                           |

Workbuf layout during `build_meta_block` (a bump allocator, `wb_top`, carves
regions out of `wb` in this order; "scratch" regions are released when the
C code frees them, so later regions reuse their bytes):

1. Scratch: one distance histogram for the distance parameter search.
2. The three BlockSplits' types and lengths arrays, sized for one block per
   symbol (`max(1, n)` entries, where n is the number of literals, commands
   or distance codes).
3. Scratch: `BrotliSplitBlock`'s literal / command code / distance code
   arrays and `SplitByteVector`'s histograms, block ids, bitmaps and
   `ClusterBlocks` state.
4. The command histograms, literal context map, literal histograms (room for
   `64 * num_literal_types`), distance context map and distance histograms
   (room for `4 * num_distance_types`).
5. Scratch: the per-context literal and distance histograms that are then
   clustered, and `BrotliClusterHistograms`' state (cluster sizes, histogram
   pairs, etc).

After `build_meta_block` returns, the results (regions 2 and 4) are below
`wb_top`, and `wb[wb_top ..]` is free for the caller (e.g. for storing the
metablock) for as long as it needs the results.


### Encoder TODO

- `workbuf_len` is a worst case (or, at qualities 10 and 11, a practical
  bound). A future option could take the input length (like the C code's
  size hint) to give a tighter bound.
- Speed: at qualities 10 and 11 the Wuffs encoder is several times slower
  than libbrotlienc, mostly because of the soft float arithmetic in the
  Zopfli cost model, the block splitter and histogram clustering.
- Compound dictionary matches at qualities 10 and 11 (the Zopfli path's
  `FindCompoundDictionaryMatch` lookups); attaching a dictionary at those
  qualities currently returns `"#unsupported encoder quality"`.
- Serialized (shared) dictionaries, which only an experimental
  (`BROTLI_EXPERIMENTAL`) libbrotlienc build supports.
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
