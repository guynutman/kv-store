# kv-store

A persistent key-value store written in C11 with a write-ahead log. No
external dependencies — just POSIX file I/O and the standard library.

> **Status:** in progress. Built as a learning project to understand crash-safe
> storage from the ground up before taking an operating systems course.

## What it does

- `put`, `get`, `delete` on string keys and values
- Every write is appended to a log and `fsync`'d **before** the in-memory
  index is updated, so a crash at any point leaves the log as the source of
  truth
- On startup the log is replayed to rebuild the in-memory hash table; a
  partially written tail (from a crash mid-write) is detected via CRC32 and
  truncated
- Compaction rewrites the log with only live keys, using write-to-temp +
  `fsync` + atomic `rename()` so a crash mid-compaction never loses data

## Build

```sh
make            # builds ./kvstore
make benchmark  # builds ./benchmark
make test       # builds and runs the test suite
make clean
```

Requires `gcc` (or any C11 compiler) and `make`. Tests are checked with
Valgrind for zero leaks and zero errors.

## Usage

```sh
./kvstore data.log
> put name Guy
OK
> get name
Guy
> delete name
OK
> compact
OK
> quit
```

## Architecture

```
 main.c / benchmark.c        REPL and benchmarks — only files with stdio
        │
    kvstore.c                composes the two below; WAL first, then hash table
      ┌─┴──────────┐
   wal.c       hashtable.c   append/fsync/replay/compact   FNV-1a, chaining, resize @ 0.75
      │
   serial.c                  entry (de)serialization + CRC32, pure, no I/O
```

### Log entry format

```
| op 1B | key_len 4B LE | key | val_len 4B LE | value | crc32 4B |
```

`op` is `1` for PUT, `2` for DELETE (DELETE has `val_len = 0` and no value).
The CRC covers every byte before it. On replay, the first entry whose CRC
fails is treated as a partial write: replay stops and the file is truncated
at that offset.

### Why a write-ahead log?

The hash table is a cache; the log is the truth. Writing to the log first
means the only thing a crash can lose is the write that was in flight — and
the CRC makes that loss detectable rather than silent corruption.

## Benchmarks

_To be filled in once `benchmark.c` exists._ Planned measurements: sequential
puts/sec, random gets/sec, 80/20 mixed, recovery time for N entries,
compaction time, and WAL vs in-memory-only throughput (the cost of `fsync`).

## Next steps

- Group commit (batch several writes per `fsync`)
- On-disk index (B-tree) so the dataset need not fit in RAM
- Network layer

## License

See [LICENSE](LICENSE).
