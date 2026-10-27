# Compression Encoders

Compression encoders read from one input stream (an `io_reader` called `src`)
and write to an output stream (an `io_writer` called `dst`). Like
[compression decoders](/doc/std/compression-decoders.md), they implement the
`base.io_transformer` interface, whose key method is `transform_io`. It
incrementally compresses the source data.

This method is a [coroutine](/doc/note/coroutines.md), and does not require
either all of the input or all of the output to fit in a single contiguous
buffer. It suspends with the `$short_read` status when it has consumed all of
`src` (and `src` is not closed) and with `$short_write` when `dst` is full. It
returns ok once `src` is closed and all of the compressed data has been
written. Encoder parameters (e.g. the compression quality) are set with
`set_quirk` before the first `transform_io` call.


## API Listing

In Wuffs syntax, the `base.io_transformer` methods are:

- `dst_history_retain_length() u64`
- `get_quirk(key: u32) u64`
- `set_quirk!(key: u32, value: u64) status`
- `transform_io?(dst: io_writer, src: io_reader, workbuf: slice u8)`
- `workbuf_len() range_ii_u64`

Encoders may have further, format-specific methods. For example, the brotli
encoder has `request_flush`, `request_emit_metadata`, `max_compressed_size`
and, for raw (LZ77 prefix) dictionaries, `add_dictionary` and
`add_dictionary_workbuf_len`. Like quirks, dictionaries are added before the
first `transform_io` call.


## Implementations

- [std/brotli](/std/brotli/README.md#encoder) (qualities 0 to 9 only,
  including large-window brotli, with windows of up to 1 GiB, at qualities 3
  to 9). Its `workbuf_len` is the worst case for the parameters (more than
  2 GiB for large-window brotli at lgwin 30), but a shorter workbuf works for
  short inputs: see the README's "Encoder Workbuf Layout" section. Up to 15
  raw dictionaries can be attached (like libbrotlienc's
  `BrotliEncoderAttachPreparedDictionary`), see the README's "Encoder
  Dictionaries" section. They live at the start of the workbuf, with their
  prepared hash tables, and `workbuf_len` grows accordingly.
