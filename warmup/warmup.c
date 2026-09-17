/* warmup.c -- byte-level pointer practice.
 *
 * Fill in the four TODO functions. main() is already written and asserts
 * your work; when it prints "all tests passed", you're done.
 *
 * Build:  gcc -Wall -Wextra -Werror -std=c11 -g -o warmup warmup.c && ./warmup
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- 1 ----------------------------------------------------------------
 * Write `val` into buf[0..3] in LITTLE-ENDIAN order: least significant
 * byte first. Do it with shifts and masks, not memcpy -- the whole point
 * is that the layout is yours, not the CPU's.
 *
 *   0x11223344  ->  buf[0]=0x44  buf[1]=0x33  buf[2]=0x22  buf[3]=0x11
 */
void buf_write_u32(uint8_t *buf, uint32_t val) {
    buf[0] = val & 0xFF;
    buf[1] = (val >> 8) & 0xFF;
    buf[2] = (val >> 16) & 0xFF;
    buf[3] = (val >> 24) & 0xFF;
}

/* ---- 2 ----------------------------------------------------------------
 * Read back a little-endian uint32 from buf[0..3].
 * Watch out: shifting a uint8_t left by 24 promotes to int and can go
 * negative. Cast each byte to uint32_t before shifting.
 */
uint32_t buf_read_u32(const uint8_t *buf) {
    uint32_t val = buf[0];
    val |= (uint32_t)buf[1] << 8;
    val |= (uint32_t)buf[2] << 16;
    val |= (uint32_t)buf[3] << 24;
    return val;
}

/* ---- 3 ----------------------------------------------------------------
 * Pack a key and value into buf in this layout:
 *
 *   [key_len: 4B LE][key bytes][val_len: 4B LE][value bytes]
 *
 * Return the total number of bytes written.
 * Use your buf_write_u32 for the lengths and memcpy for the payloads.
 * Assume buf is large enough (it is, in main).
 *
 * Note there is no NUL terminator anywhere -- the length IS the terminator.
 * That is what makes this binary-safe: keys may contain zero bytes.
 */
size_t pack_pair(uint8_t *buf,
                 const char *key, uint32_t key_len,
                 const char *value, uint32_t val_len) {

    size_t offset = 0; 
    buf_write_u32(buf + offset, key_len); offset += 4;
    memcpy(buf + offset, key, key_len); offset += key_len; 

    buf_write_u32(buf + offset, val_len); offset += 4; 
    memcpy(buf + offset, value, val_len); offset += val_len;
    return offset;   
}

/* ---- 4 ----------------------------------------------------------------
 * Reverse of pack_pair. Read the two lengths and both payloads out of buf.
 *
 * Allocate fresh, NUL-terminated copies of the key and value with malloc,
 * and store them through *out_key / *out_value. Store the lengths through
 * *out_key_len / *out_val_len. Return total bytes consumed.
 *
 * "NUL-terminated" means: malloc len+1 bytes, memcpy len of them, set the
 * last to '\0'. The store keeps explicit lengths for binary safety, but a
 * trailing NUL means printf("%s") also works on them. Costs one byte.
 *
 * THE OWNERSHIP RULE: this function mallocs, the CALLER frees. Nothing in
 * C enforces that -- it is a comment and a convention, and every leak in
 * this project will come from one side forgetting. Note how main frees.
 */
size_t unpack_pair(const uint8_t *buf,
                   char **out_key, uint32_t *out_key_len,
                   char **out_value, uint32_t *out_val_len) {
    
    size_t offset = 0;
    *out_key_len = buf_read_u32(buf + offset);
    offset += 4;

    char *key = malloc(*out_key_len + 1);
    memcpy(key, buf+offset, *out_key_len);
    key[*out_key_len] = '\0';
    offset += *out_key_len; 
    
    *out_val_len = buf_read_u32(buf + offset);
    offset += 4;

    char *value = malloc(*out_val_len + 1);
    memcpy(value, buf+offset, *out_val_len);
    value[*out_val_len] = '\0';
    offset += *out_val_len;
    
    *out_key = key; 
    *out_value = value;

    return offset; 
}

/* ---- tests: already written, do not edit ----------------------------- */

int main(void) {
    /* 1 + 2: endianness round-trip */
    uint8_t four[4];
    buf_write_u32(four, 0x11223344u);
    assert(four[0] == 0x44 && four[1] == 0x33 &&
           four[2] == 0x22 && four[3] == 0x11);
    assert(buf_read_u32(four) == 0x11223344u);

    buf_write_u32(four, 0);
    assert(buf_read_u32(four) == 0);
    buf_write_u32(four, 0xFFFFFFFFu);
    assert(buf_read_u32(four) == 0xFFFFFFFFu);   /* the sign-extension trap */
    printf("u32 round-trip ok\n");

    /* 3 + 4: pack / unpack round-trip */
    uint8_t buf[256];
    const char *k = "name";
    const char *v = "Guy";
    size_t written = pack_pair(buf, k, 4, v, 3);
    assert(written == 4 + 4 + 4 + 3);        /* 4+key, 4+val */

    char *key = NULL, *value = NULL;
    uint32_t kl = 0, vl = 0;
    size_t consumed = unpack_pair(buf, &key, &kl, &value, &vl);
    assert(consumed == written);
    assert(kl == 4 && vl == 3);
    assert(strcmp(key, "name") == 0);
    assert(strcmp(value, "Guy") == 0);
    free(key);
    free(value);
    printf("pack round-trip ok\n");

    /* binary safety: a key containing a zero byte must survive */
    const char embedded[5] = { 'a', '\0', 'b', '\0', 'c' };
    written = pack_pair(buf, embedded, 5, "", 0);
    consumed = unpack_pair(buf, &key, &kl, &value, &vl);
    assert(consumed == written);
    assert(kl == 5 && memcmp(key, embedded, 5) == 0);
    assert(vl == 0);
    free(key);
    free(value);
    printf("binary-safe ok\n");

    printf("all tests passed\n");
    return 0;
}
