#include <stdlib.h>
#include <string.h>
#include "khp_dictionary.h"
#include "../third_party/puff/puff.h"

// RFC1950 Adler-32, computed independently of puff/zlib -- this is a
// small, standard, unpatented algorithm, not something worth pulling in
// a library for.
static uint32_t
adler32(const uint8_t *data, size_t len)
{
    uint32_t a = 1, b = 0;
    const uint32_t MOD = 65521;
    for (size_t i = 0; i < len; i++) {
        a = (a + data[i]) % MOD;
        b = (b + a) % MOD;
    }
    return (b << 16) | a;
}

enum khp_dictionary_status
khp_dictionary_inflate(struct khp_dictionary *out, const uint8_t *zlib_data
                       , size_t zlib_len)
{
    out->json = NULL;
    out->json_len = 0;

    if (zlib_len < 2 + 4) // header + at minimum a trailer's worth of bytes
        return KHP_DICT_BAD_ZLIB_HEADER;

    uint8_t cmf = zlib_data[0], flg = zlib_data[1];
    if ((cmf & 0x0f) != 8                        // compression method != deflate
        || ((uint32_t)(cmf << 8 | flg) % 31) != 0 // header check bits
        || (flg & 0x20))                          // FDICT: preset dictionary, unsupported
        return KHP_DICT_BAD_ZLIB_HEADER;

    const uint8_t *source = zlib_data + 2;
    size_t source_avail = zlib_len - 2;

    // Pass 1: measure the decompressed size (dest == NULL is puff()'s
    // documented "just count the output" mode).
    unsigned long destlen = 0;
    unsigned long sourcelen = source_avail;
    int err = puff(NULL, &destlen, source, &sourcelen);
    if (err != 0)
        return KHP_DICT_INFLATE_ERROR;

    uint8_t *buf = malloc(destlen + 1); // +1 for a NUL terminator
    if (!buf)
        return KHP_DICT_OOM;

    // Pass 2: the real decode, now that we know how much space it needs.
    unsigned long destlen2 = destlen;
    unsigned long sourcelen2 = source_avail;
    err = puff(buf, &destlen2, source, &sourcelen2);
    if (err != 0 || destlen2 != destlen) {
        free(buf);
        return KHP_DICT_INFLATE_ERROR;
    }
    buf[destlen] = '\0';

    // sourcelen2 was updated in place to the number of input bytes the
    // DEFLATE stream actually consumed -- the Adler-32 trailer starts
    // right after that.
    if (source_avail - sourcelen2 < 4) {
        free(buf);
        return KHP_DICT_BAD_ZLIB_HEADER;
    }
    const uint8_t *trailer = source + sourcelen2;
    uint32_t expected = ((uint32_t)trailer[0] << 24) | ((uint32_t)trailer[1] << 16)
                       | ((uint32_t)trailer[2] << 8) | trailer[3];
    if (adler32(buf, destlen) != expected) {
        free(buf);
        return KHP_DICT_ADLER_MISMATCH;
    }

    out->json = buf;
    out->json_len = destlen;
    return KHP_DICT_OK;
}

void
khp_dictionary_free(struct khp_dictionary *d)
{
    free(d->json);
    d->json = NULL;
    d->json_len = 0;
}
