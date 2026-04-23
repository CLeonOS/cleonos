#ifndef NDEBUG
#define NDEBUG 1
#endif

#ifndef MINIZ_NO_STDIO
#define MINIZ_NO_STDIO 1
#endif

#ifndef MINIZ_NO_INFLATE_APIS
#define MINIZ_NO_INFLATE_APIS 1
#endif

#ifndef MINIZ_NO_TIME
#define MINIZ_NO_TIME 1
#endif

#ifndef MINIZ_NO_MALLOC
#define MINIZ_NO_MALLOC 1
#endif

#ifndef MINIZ_NO_ZLIB_APIS
#define MINIZ_NO_ZLIB_APIS 1
#endif

#ifndef MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES 1
#endif

#include "miniz.h"

mz_ulong mz_adler32(mz_ulong adler, const unsigned char *ptr, size_t buf_len) {
    mz_uint32 s1 = (mz_uint32)(adler & 0xFFFFU);
    mz_uint32 s2 = (mz_uint32)((adler >> 16) & 0xFFFFU);
    size_t i;

    if (ptr == 0) {
        return MZ_ADLER32_INIT;
    }

    for (i = 0; i < buf_len; i++) {
        s1 += (mz_uint32)ptr[i];
        if (s1 >= 65521U) {
            s1 -= 65521U;
        }

        s2 += s1;
        if (s2 >= 65521U) {
            s2 -= 65521U;
        }
    }

    return ((mz_ulong)s2 << 16) | (mz_ulong)s1;
}

mz_ulong mz_crc32(mz_ulong crc, const unsigned char *ptr, size_t buf_len) {
    mz_uint32 cur = (mz_uint32)crc ^ 0xFFFFFFFFU;
    size_t i;
    unsigned int bit;

    if (ptr == 0) {
        return MZ_CRC32_INIT;
    }

    for (i = 0; i < buf_len; i++) {
        cur ^= (mz_uint32)ptr[i];
        for (bit = 0U; bit < 8U; bit++) {
            if ((cur & 1U) != 0U) {
                cur = (cur >> 1) ^ 0xEDB88320U;
            } else {
                cur >>= 1;
            }
        }
    }

    return (mz_ulong)(cur ^ 0xFFFFFFFFU);
}
