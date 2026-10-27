# M7b handoff: brotli differential fuzzing

Branch m7b-fuzz (worktree /home/user/wuffs-m7b). Stopped 2026-10-27 16:47 UTC at the
coordinator's request. Scratch area (logs, binaries, repro files):
SC = the agent session's scratchpad directory, subdirectory m7b (under /tmp; ask the coordinator for the path)
(session scratch, may not survive; the regression tests below cover divergences 1-3).

## Harnesses and how to run them

1. script/manual-test-brotli-fuzz.c: differential fuzzer against libbrotli v1.2.1.
   Modes: -dec (mutated streams, random I/O chunking, output limits, workbuf lengths, LW quirk,
   0-15 raw + serialized dictionaries), -enc (random inputs, params, plans, dst limits, dicts,
   q10-11 workbufs; byte-exact output + round trip through both decoders), -api (edge cases and
   random step sequences), -replay=PATH (re-run a saved decoder failure).
   Options: -seed=S -n=N -secs=T -q=LO-HI -nohq -out=DIR -data=DIR.
   Build (from the repo root; libbrotlidec must be built with -DBROTLI_EXPERIMENTAL, see the
   file's header comment; SC/exp/libbrotlidecexp.a is that build):
     gcc -O2 -g -std=c99 -I/home/user/brotli-ref/c/include -I/home/user/brotli-ref/c \
         script/manual-test-brotli-fuzz.c /home/user/brotli-ref-build/libbrotlienc.a \
         SC/exp/libbrotlidecexp.a -lm -o SC/bfuzz
   Shards: SC/shard.sh MODE SEED SECS [args] (log SC/logs/MODE-SEED.log, summary line appended
   to SC/results-MODE.txt); SC/chain.sh "MODE|ARGS|SEED"... runs shards one after another and
   skips seeds already in the results file. Use at most 2 shards at once. A shard's counts are
   only printed when it ends (a killed shard counts nothing).
   Known/accepted classes (not failures): decoder "known BLOCK_LENGTH" (divergence 1); encoder
   both_rejected (SetParameter vs "#bad argument"), both_flint_failed (stream offset flush
   failure, README "Encoder Operation Mapping"), c_state_corrupted (divergence 2), uninit_tail
   (divergence 4).
2. fuzz/c/std/brotli_fuzzer.c: libFuzzer/FUZZLIB_MAIN target; now also exercises LW, dicts and an
   encoder round trip; 128 MiB workbuf. Seeds: fuzz/c/std/seed_corpora.txt (brotli line).
3. Replay helpers (scratch only): SC/rep/encrep SRC TXT (re-runs a saved encoder failure:
   SRC = saved .br input, TXT = its description), SC/rep/apirep.c, SC/declog (libbrotlidec built
   with BROTLI_ENABLE_LOG + d/d2 drivers), SC/dbg2 and SC/dbg3 (instrumented C encoder and Wuffs
   snapshot copies that print commands, bit positions, tree sizes and workbuf layout).

## Seeding bug (affects all shards before a856d13)

Seeding SplitMix64 with seed * gamma made seed N+1 replay seed N's sequence shifted by one
number, so shards with consecutive seeds repeated each other's cases (encoder shards 222 and 223
hit the same config). Fixed in a856d13 (seed hashed). All counts below are from shards run after
the fix; the earlier shards (decoder 101-109, encoder 201-208/221-223/301-302, API 401-403,
history in SC/HANDOFF-old.md and SC/logs-oldseed/) are NOT counted as distinct inputs. They
found divergences 2 and 3, which stand.

## Campaign counts (seeds hashed; 20-minute shards; "pre-merge" = before 73de2fe, the merge
## of 24807c6 speed work; "post-merge" = 73de2fe + later)

Campaign 1, decoder (each input decoded 4 ways: Wuffs and C, one-shot and chunked):
  pre-merge  1101 685,184 / 1102 724,096 / 1103 735,936 / 1104 722,560 = 2,867,776 inputs,
             0 failures, 6 known BLOCK_LENGTH.
  post-merge 1105 767,488 / 1106 750,528 / 1107 781,632 / 1108 806,976 = 3,106,624 inputs,
             0 failures, 9 known BLOCK_LENGTH.
  total 5,974,400 mutated inputs (target 5M: met; post-merge target 2M: met).
Campaign 2, encoder:
  pre-merge  1201 (q0-9) 87,344, 1221 (q0-4) 96,192, 1202 (q0-9) 99,392, 1222 (q0-4) 95,936
             = 378,864 configs (q0-4 303,081; q5-9 75,783; q10-11 0), 0 failures,
             348,531 round trips, 94,910 with dicts, 75,212 LW, 30,331 stream offset,
             75 flint both-fail, 2 c_state_corrupted. Shard 1201 ran before the dbd5a00 fix.
  post-merge 1301 (q10-11) 2,960 (308 with dicts, 3 skipped >3 GiB), 1203 (q0-9) 93,888,
             1223 (q0-4) 97,248, 1204 (q0-9) 101,072 = 295,168 configs (q0-4 212,493;
             q5-9 79,715; q10-11 2,960), 1 failure (divergence 4, now classified),
             271,641 round trips, 73,212 with dicts, 58,702 LW, 23,522 stream offset,
             49 flint both-fail, 1 c_state_corrupted.
  total 674,032 configs (target 1M: NOT met; ~4 more q0-9/q0-4 shards and more q10-11).
Campaign 3, API: no post-seed-fix shard completed (1401 was stopped). Old-seed shards 401-403:
  682,258 checks; 401's 65 failures were a harness artifact (fixed 0e0674d), 402/403 clean.
  Remaining: run -api shards with hashed seeds (e.g. 1401, 1402).
Campaign 4, fuzz/c/std/brotli_fuzzer.c (pre-merge code): gcc ASan/UBSan FUZZLIB_MAIN over 109
  seed files PASS; clang libFuzzer + ASan/UBSan 1 hour: 35,889 runs, 1,125 new units, peak RSS
  558 MB, 0 crashes (slow-unit artifacts only, slowest 21 s), plus an aborted ~5,170-run start.
  Not rerun post-merge.

## Divergences

1. Decoder, chunked input only: C BLOCK_LENGTH_1 (-9) where Wuffs reports "#bad metablock
   length" (-10). Root cause: libbrotlidec's internal input buffer after a rolled-back
   command/distance/block switch read makes extra NEEDS_MORE_INPUT returns and length checks.
   Not a Wuffs bug; documented (README "Status Mapping"), test
   test_mimic_brotli_decode_split_block_length (2241af9). Saved cases SC/out/fail-known-*.
2. Encoder q0/q1, metadata with no dst room then a flush: libbrotlienc bug (stale next_out_
   = tiny_buf_+16 overwrites remaining_metadata_bytes_; later calls fail). Wuffs not changed;
   documented, test test_mimic_brotli_encode_metadata_stale_pointer, harness class
   c_state_corrupted (8a8df66). Repros: SC/rep/stale.c, SC/out/fail-enc-980061935075926891-1.br.
3. Encoder q2 (BrotliStoreMetaBlockFast, >128 commands): REAL WUFFS BUG, invalid stream.
   build_and_store_huffman_tree_fast_generic indexed the 704-entry ZERO_REPS/NON_ZERO_REPS
   tables with reps & 511. Fixed dbd5a00 (reps.min(no_more_than: 703)); tests
   test_wuffs_brotli_encode_long_zero_run and test_mimic_brotli_encode_long_zero_run (both fail
   before the fix). Repro: SC/out/fail-enc-17396939997611138402-1.br, minimized SC/rep/min.bin.
4. Encoder q3 lgwin 13, multi-step plan (post-merge shard 1204): outputs differ, both decode.
   Both encoders read the uninitialized ring buffer tail after a short first write (C: malloc'd
   memory in RingBufferWrite's lazy allocation; Wuffs: workbuf bytes). With equal memory
   contents (zeroed, or MALLOC_PERTURB_=84 vs a 0xAB-filled workbuf) the outputs are equal; the
   same happens with pre-merge code. Not a Wuffs mirroring bug; documented in README, harness
   class uninit_tail (8dcecc6, reruns with zeroed memory and checks Wuffs' output decodes).
   Repro: SC/out/fail-enc-13322404997902801073-1.br (+.txt); FILLWB=255 SC/rep/encrep SRC TXT.
5. Harness-only: API mode zero-length dst calls with a stream offset (fixed 0e0674d); stream
   offset flush failure accepted when both fail identically (0c23849, README note 6fb1f22).

## Remaining

- Encoder campaign to >= 1M configs (674,032 now): more q0-9/q0-4 shards with hashed seeds
  (e.g. 1205, 1224, 1206, 1225), plus q10-11 shards (1302+), on the merged code.
- API campaign with hashed seeds (1401, 1402).
- Optionally rerun brotli_fuzzer (libFuzzer) on the merged code.
- `wuffs test -mimic std/brotli` on gcc and clang (full, ~30 min each) was NOT run after the
  merges and fixes; only the new focused tests were run (gcc, PASS). `wuffs gen` and
  `wuffsfmt -l std/brotli` are clean at HEAD.
