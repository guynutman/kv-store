# kv-store — Claude Code Guide

Persistent key-value store in C with a write-ahead log. Mentoring project: the
student (Guy) writes every line; Claude gives concepts + specs and reviews.

## Who the student is

- Knows C++, Python, TypeScript, full-stack. **New to C.** Taking CS 111 (OS)
  next quarter and using this project to prepare.
- Has to be able to explain every line in a systems interview.

## How to work with them (established preferences)

- **Brief replies.** One concept at a time. No walls of text unless asked
  "explain from scratch" or "how does it work under the hood" — then go deep.
- **Spec, not implementation.** Give signatures, struct layouts, constraints,
  and the C idioms needed. Student writes the code, Claude reviews. Never
  write module code for them.
- **Student drives the compiler.** When a new file is created, walk them
  through the build (gcc flags, .o files, linking) rather than running it for
  them. They know: preprocess → compile → assemble → link, `-c`, `nm`, what
  `-Wall -Wextra -Werror -std=c11 -g` mean.
- To review, read the file from disk (`sed -n`), build with
  `gcc -Wall -Wextra -Werror -std=c11 -g`, run with `stdbuf -o0` (stdout is
  block-buffered to a pipe; abort() loses output otherwise).
- They ask good "why" questions — answer the actual question, then continue.
- Every design choice is judged by: *if the process dies right here, what's on
  disk and can I rebuild from it?* If the answer is "corruption", it's wrong.

## Progress

- [x] Concepts covered: WAL pattern, fsync/page cache, kernel vs user space,
  syscalls, checksums/CRC, compaction + atomic rename, disk boundedness,
  compilation pipeline, gcc flags, endianness, memcpy internals, malloc
  internals (chunks, bins, brk/mmap), pointers vs pointer-to-pointer,
  pass-by-value, ownership rule (callee mallocs → caller frees).
- [x] Warm-up `warmup/warmup.c` functions 1–3 (buf_write_u32, buf_read_u32,
  pack_pair) — passing.
- [ ] Warm-up function 4 `unpack_pair` — in progress. Last attempt memcpy'd
  bytes into the `char **` box instead of malloc'ing and storing the address;
  also missed final `offset += val_len`.
- [ ] Module 1 serial.c → 2 hashtable.c → 3 wal.c → 4 kvstore.c →
  5 test_crash.c → 6 benchmark.c → 7 main.c
- Valgrind is **not installed** yet (`sudo apt install valgrind`) — needed
  before module 2.
- `warmup/` is uncommitted scratch; delete or gitignore before module 1 commit.

## Build order & workflow rules

1. serial.c — serialize/deserialize + CRC32. Test round-trip + corruption.
2. hashtable.c — FNV-1a, chaining, resize at 0.75. Valgrind.
3. wal.c — append/fsync, replay, truncate partial tail, compact via rename.
4. kvstore.c — compose. Test put/get/delete, reopen recovery, compact.
5. test_crash.c — truncate log at many offsets, reopen, verify consistency.
6. benchmark.c — puts/sec, gets/sec, mixed, recovery time, compaction time,
   WAL vs in-memory-only.
7. main.c — REPL. Only file with printf/fgets.

- Valgrind after every module: `valgrind --leak-check=full ./test_runner`.
  Zero leaks, zero errors.
- Commit after each module.
- README: what/build/CLI/architecture/benchmarks/next steps (batched fsync,
  B-tree index, network layer).

## Stack

C11, POSIX only (`open write read fsync ftruncate rename`, `mmap` optional),
Make, Valgrind. No external libs. `CFLAGS = -Wall -Wextra -Werror -std=c11 -g -O2`.

## Layout

```
src/serial.{h,c}  hashtable.{h,c}  wal.{h,c}  kvstore.{h,c}  benchmark.c  main.c
tests/test_hashtable.c test_wal.c test_kvstore.c test_crash.c
Makefile  README.md  LICENSE
```

## Log entry format

```
| op 1B | key_len 4B LE | key | val_len 4B LE | value | crc32 4B |
  PUT=1 / DEL=2 (DEL: val_len=0, no value). CRC over everything before it.
```

Replay: verify CRC → apply; first failure = partial write → stop, ftruncate
there. Compaction: walk hash table → write `<path>.tmp` → fsync → rename().

## Module specs

### serial.h
```c
typedef enum { OP_PUT = 1, OP_DELETE = 2 } OpType;
typedef struct { OpType op; char *key; uint32_t key_len;
                 char *value; uint32_t val_len; } LogEntry;   // value NULL for DEL
size_t   entry_serialize(const LogEntry *e, uint8_t *buf);  // bytes written; buf ≥ 1+4+kl+4+vl+4
size_t   entry_deserialize(const uint8_t *buf, size_t buf_len, LogEntry *out); // 0 if short/CRC fail; mallocs key/value
void     entry_free(LogEntry *e);
uint32_t crc32(const uint8_t *buf, size_t len);              // table-based
```
Pure, no I/O, little-endian, CRC failure → return 0.

### hashtable.h
```c
typedef struct HTEntry { char *key; char *value; uint32_t key_len, val_len;
                         struct HTEntry *next; } HTEntry;
typedef struct { HTEntry **buckets; size_t num_buckets; size_t count; } HashTable;
HashTable  *ht_create(size_t initial_capacity);
void        ht_destroy(HashTable *ht);
int         ht_put(HashTable*, const char *key, uint32_t kl, const char *val, uint32_t vl); // copies; 0/-1
const char *ht_get(HashTable*, const char *key, uint32_t kl, uint32_t *vl);  // owned by table
int         ht_delete(HashTable*, const char *key, uint32_t kl);             // 0 / -1 not found
void        ht_foreach(HashTable*, void (*fn)(const char*,uint32_t,const char*,uint32_t,void*), void *ud);
```
Table owns copies. Overwrite frees old value. Resize when count/buckets > 0.75. FNV-1a.

### wal.h
```c
typedef struct { int fd; char *path; size_t file_size; } WAL;
WAL *wal_open(const char *path);          // O_WRONLY|O_CREAT|O_APPEND
void wal_close(WAL*);
int  wal_append_put(WAL*, const char *k, uint32_t kl, const char *v, uint32_t vl); // fsync
int  wal_append_delete(WAL*, const char *k, uint32_t kl);                          // fsync
int  wal_replay(WAL*, void (*cb)(const LogEntry*, void*), void *ud); // count or -1; truncates partial tail
int  wal_compact(WAL*, HashTable *ht);    // tmp + fsync + rename
```
fsync per append (note where group commit would go).

### kvstore.h
```c
typedef struct { HashTable *ht; WAL *wal; } KVStore;
KVStore    *kvstore_open(const char *log_path);   // replays log
void        kvstore_close(KVStore*);
int         kvstore_put(KVStore*, const char *key, const char *value);  // WAL first, then ht
const char *kvstore_get(KVStore*, const char *key);                     // owned by store
int         kvstore_delete(KVStore*, const char *key);                  // WAL first
int         kvstore_compact(KVStore*);
```
Only public API for main.c / benchmark.c. WAL failure ⇒ do not touch ht.

### benchmark.c
`./benchmark [N]`: seq puts/s, random gets/s, 80/20 mixed, recovery time,
compaction time, WAL vs in-memory-only. Uses `clock_gettime(CLOCK_MONOTONIC)`.

### main.c
`./kvstore data.log` REPL: `put k v`, `get k`, `delete k`, `compact`, `quit`.

### Makefile
```makefile
CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -g -O2
SRCS = src/serial.c src/hashtable.c src/wal.c src/kvstore.c
OBJS = $(SRCS:.c=.o)
kvstore: src/main.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^
benchmark: src/benchmark.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^
test: tests/test_hashtable.c tests/test_wal.c tests/test_kvstore.c tests/test_crash.c $(OBJS)
	$(CC) $(CFLAGS) -o test_runner $^ && ./test_runner
clean:
	rm -f src/*.o kvstore benchmark test_runner *.log
.PHONY: clean test
```
