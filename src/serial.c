/*
 * serial.c — log entry serialization and CRC32.
 *
 * Pure functions only: no file I/O, no printf. Everything here operates on
 * byte buffers so it can be unit-tested without touching the disk.
 */

#include "serial.h"

/* ---- CRC32 ---------------------------------------------------------------
 *
 * Standard CRC-32 (the one used by zip, PNG, Ethernet). The message is treated
 * as one long binary polynomial and divided by a fixed 33-bit polynomial using
 * XOR instead of subtraction; the 32-bit remainder is the checksum. Unlike a
 * plain byte-sum, every bit's contribution depends on its position, so swapped
 * bytes or a +1/-1 pair change the result.
 *
 * Doing the division bit-by-bit costs 8 steps per byte. Instead we precompute
 * a 256-entry table — "what remainder does this single byte produce" — and
 * process one whole byte per step.
 */

/* Lookup table, filled once by crc_init(). `static` at file scope = private to
 * this .c file; no other translation unit can see or link against it. */
static uint32_t crc_table[256];

/* Has crc_table been filled yet? Checked on every crc32() call so the caller
 * never has to remember to initialize anything. */
static int table_ready = 0;

/* Build crc_table. For each byte value i, run the 8-step bitwise division and
 * store the resulting remainder. Called exactly once, lazily, from crc32(). */
static void crc_init(void) {

}

/* Compute the CRC-32 of buf[0..len).
 *
 * Start with all-ones, fold in one byte at a time via the table, then invert
 * the result. The initial/final inversions are part of the standard and make
 * leading zero bytes affect the checksum.
 *
 * Known-answer test: crc32("123456789", 9) == 0xCBF43926. */
uint32_t crc32(const uint8_t *buf, size_t len) {
    if (!table_ready) { crc_init(); table_ready = 1; }

}
