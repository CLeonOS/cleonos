#include <stdio.h>
#include <string.h>

#include <zlib.h>

static int ztest_failed = 0;

static void ztest_fail(const char *name, int rc) {
    ztest_failed++;
    printf("zlibtest: FAIL %s rc=%d\n", name, rc);
}

static int ztest_roundtrip(void) {
    static const unsigned char input[] =
        "CLeonOS zlib test: compress, inflate, crc32, adler32. "
        "This string is intentionally repeated. "
        "CLeonOS zlib test: compress, inflate, crc32, adler32.";
    unsigned char compressed[512];
    unsigned char output[256];
    uLongf compressed_len = (uLongf)sizeof(compressed);
    uLongf output_len = (uLongf)sizeof(output);
    int rc;

    rc = compress2(compressed, &compressed_len, input, (uLong)strlen((const char *)input), Z_BEST_COMPRESSION);
    if (rc != Z_OK) {
        ztest_fail("compress2", rc);
        return 0;
    }

    rc = uncompress(output, &output_len, compressed, compressed_len);
    if (rc != Z_OK) {
        ztest_fail("uncompress", rc);
        return 0;
    }

    output[output_len] = '\0';
    if (output_len != strlen((const char *)input) || memcmp(output, input, output_len) != 0) {
        ztest_fail("roundtrip-compare", -1);
        return 0;
    }

    printf("zlibtest: compressed %llu -> %llu bytes\n", (unsigned long long)strlen((const char *)input),
           (unsigned long long)compressed_len);
    return 1;
}

static int ztest_checksums(void) {
    static const unsigned char text[] = "123456789";
    uLong adler = adler32(0L, Z_NULL, 0);
    uLong crc = crc32(0L, Z_NULL, 0);

    adler = adler32(adler, text, 9U);
    crc = crc32(crc, text, 9U);

    if (adler != 0x091E01DEUL) {
        ztest_fail("adler32", (int)adler);
        return 0;
    }

    if (crc != 0xCBF43926UL) {
        ztest_fail("crc32", (int)crc);
        return 0;
    }

    printf("zlibtest: adler32=0x%lX crc32=0x%lX\n", adler, crc);
    return 1;
}

int cleonos_app_main(void) {
    printf("zlibtest: zlib %s\n", zlibVersion());

    (void)ztest_roundtrip();
    (void)ztest_checksums();

    if (ztest_failed == 0) {
        puts("zlibtest: PASS");
        return 0;
    }

    printf("zlibtest: FAIL failed=%d\n", ztest_failed);
    return 1;
}
