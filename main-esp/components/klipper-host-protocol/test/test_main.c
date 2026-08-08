// Host-buildable unit tests for the klipper-host-protocol component.
// Build with any C compiler, no ESP-IDF required:
//   gcc -Wall -I../include -o khp_test test_main.c ../src/vlq.c ../src/msgblock.c
#include <stdio.h>
#include <string.h>
#include "khp_vlq.h"
#include "khp_msgblock.h"

static int g_failures = 0;

#define CHECK(desc, cond) do { \
    if (cond) { \
        printf("PASS: %s\n", desc); \
    } else { \
        printf("FAIL: %s\n", desc); \
        g_failures++; \
    } \
} while (0)

static void
test_vlq_roundtrip(int32_t v)
{
    uint8_t buf[KHP_VLQ_MAX_BYTES];
    size_t n = khp_vlq_encode_int32(buf, v);
    const uint8_t *p = buf;
    int32_t got = khp_vlq_decode_int32(&p);
    char desc[64];
    snprintf(desc, sizeof(desc), "vlq roundtrip v=%d (%zu bytes)", v, n);
    CHECK(desc, got == v && (size_t)(p - buf) == n);
}

static void
test_vlq_boundaries(void)
{
    // Range-table boundaries from https://www.klipper3d.org/Protocol.html
    // (1 byte: -32..95, 2 bytes: -4096..12287, 3 bytes: -524288..1572863,
    //  4 bytes: -67108864..201326591, 5 bytes: everything else in int32).
    int32_t values[] = {
        0, 1, -1, 95, -32, 96, -33,
        12287, -4096, 12288, -4097,
        1572863, -524288, 1572864, -524289,
        201326591, -67108864, 201326592, -67108865,
        2147483647, -2147483647, -2147483647 - 1, // INT32_MIN without UB
    };
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++)
        test_vlq_roundtrip(values[i]);
}

static void
test_vlq_encoded_lengths(void)
{
    uint8_t buf[KHP_VLQ_MAX_BYTES];
    CHECK("vlq 1-byte length (v=0)", khp_vlq_encode_int32(buf, 0) == 1);
    CHECK("vlq 1-byte length (v=95)", khp_vlq_encode_int32(buf, 95) == 1);
    CHECK("vlq 2-byte length (v=96)", khp_vlq_encode_int32(buf, 96) == 2);
    CHECK("vlq 2-byte length (v=-33)", khp_vlq_encode_int32(buf, -33) == 2);
    CHECK("vlq 5-byte length (v=INT32_MAX)"
         , khp_vlq_encode_int32(buf, 2147483647) == 5);
}

static void
test_crc_empty(void)
{
    // Trivial but real: the CRC of a zero-length buffer is just the
    // algorithm's init value, true regardless of the bit-shuffle logic.
    CHECK("crc16_ccitt of empty buffer == 0xffff"
         , khp_crc16_ccitt((const uint8_t *)"", 0) == 0xffff);
}

static void
test_msgblock_roundtrip(const uint8_t *content, size_t content_len, uint8_t seq)
{
    uint8_t buf[KHP_MSG_MAX];
    size_t block_len = khp_msgblock_encode(buf, content, content_len, seq);
    char desc[80];

    snprintf(desc, sizeof(desc), "msgblock encode content_len=%zu succeeds"
            , content_len);
    CHECK(desc, block_len == KHP_MSG_HEADER_SIZE + content_len + KHP_MSG_TRAILER_SIZE);

    struct khp_msgblock_scanner scanner = {0};
    int r = khp_msgblock_check(&scanner, buf, (int)block_len);
    snprintf(desc, sizeof(desc), "msgblock check content_len=%zu accepts it"
            , content_len);
    CHECK(desc, r == (int)block_len);

    if (r > 0) {
        struct khp_msgblock_view view;
        khp_msgblock_view_init(&view, buf, r);
        snprintf(desc, sizeof(desc), "msgblock view content_len=%zu matches"
                , content_len);
        CHECK(desc, view.content_len == content_len
                    && view.seq == (seq & KHP_MSG_SEQ_MASK)
                    && (content_len == 0
                        || memcmp(view.content, content, content_len) == 0));
    }
}

static void
test_msgblock_boundaries(void)
{
    uint8_t empty[1] = {0};
    test_msgblock_roundtrip(empty, 0, 0);              // KHP_MSG_MIN case

    uint8_t max_content[KHP_MSG_MAX_CONTENT];
    for (size_t i = 0; i < sizeof(max_content); i++)
        max_content[i] = (uint8_t)i;
    test_msgblock_roundtrip(max_content, sizeof(max_content), 7);

    uint8_t small[3] = {0x01, 0x42, 0xff};
    test_msgblock_roundtrip(small, sizeof(small), 15); // max seq (4 bits)
}

static void
test_msgblock_corrupt_crc(void)
{
    uint8_t content[3] = {0x01, 0x02, 0x03};
    uint8_t buf[KHP_MSG_MAX];
    size_t block_len = khp_msgblock_encode(buf, content, sizeof(content), 0);
    buf[KHP_MSG_HEADER_SIZE] ^= 0xff; // corrupt the content after CRC was computed

    struct khp_msgblock_scanner scanner = {0};
    int r = khp_msgblock_check(&scanner, buf, (int)block_len);
    CHECK("msgblock check rejects corrupted content (bad crc)", r < 0);
}

static void
test_msgblock_resync(void)
{
    // Garbage, then a sync byte, then a valid message right after it.
    uint8_t content[2] = {0xaa, 0xbb};
    uint8_t valid[KHP_MSG_MAX];
    size_t valid_len = khp_msgblock_encode(valid, content, sizeof(content), 3);

    uint8_t stream[16 + KHP_MSG_MAX];
    size_t garbage_len = 4;
    memset(stream, 0x00, garbage_len); // no sync byte in here
    stream[garbage_len] = KHP_MSG_SYNC; // a stray sync byte, not a real trailer
    memcpy(stream + garbage_len + 1, valid, valid_len);
    size_t stream_len = garbage_len + 1 + valid_len;

    struct khp_msgblock_scanner scanner = {0};
    int r = khp_msgblock_check(&scanner, stream, (int)stream_len);
    CHECK("msgblock resync: first check discards up to stray sync byte"
         , r < 0 && -r == (int)(garbage_len + 1));

    // After discarding, the remainder should decode cleanly.
    r = khp_msgblock_check(&scanner, stream + garbage_len + 1
                           , (int)(stream_len - garbage_len - 1));
    CHECK("msgblock resync: remainder after discard is the valid message"
         , r == (int)valid_len);
}

static void
test_msgblock_need_more_data(void)
{
    uint8_t content[2] = {0xaa, 0xbb};
    uint8_t buf[KHP_MSG_MAX];
    size_t block_len = khp_msgblock_encode(buf, content, sizeof(content), 0);

    struct khp_msgblock_scanner scanner = {0};
    int r = khp_msgblock_check(&scanner, buf, (int)block_len - 1);
    CHECK("msgblock check returns 0 (need more data) on a truncated block"
         , r == 0);
}

int
main(void)
{
    test_vlq_boundaries();
    test_vlq_encoded_lengths();
    test_crc_empty();
    test_msgblock_boundaries();
    test_msgblock_corrupt_crc();
    test_msgblock_resync();
    test_msgblock_need_more_data();

    printf("\n%s (%d failure%s)\n"
          , g_failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED"
          , g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
