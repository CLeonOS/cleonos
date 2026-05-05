/* pnglibconf.h - CLeonOS libpng build configuration
 *
 * This repository vendors libpng as a submodule, but the submodule commit
 * used in CI only tracks scripts/pnglibconf.h.prebuilt and does not always
 * ship a root-level pnglibconf.h.  libpng's png.h includes "pnglibconf.h",
 * so provide a stable tracked configuration header from the main tree.
 *
 * The configuration intentionally keeps the feature set read-only and small
 * for the freestanding CLeonOS userland. Applications should use
 * png_set_read_fn() instead of stdio-backed helpers.
 */
#ifndef PNGLCONF_H
#define PNGLCONF_H

/* Core library options. */
#define PNG_16BIT_SUPPORTED
#define PNG_ALIGNED_MEMORY_SUPPORTED
#define PNG_CHECK_FOR_INVALID_INDEX_SUPPORTED
#define PNG_EASY_ACCESS_SUPPORTED
#define PNG_ERROR_TEXT_SUPPORTED
#define PNG_FIXED_POINT_SUPPORTED
#define PNG_FORMAT_AFIRST_SUPPORTED
#define PNG_FORMAT_BGR_SUPPORTED
#define PNG_GET_PALETTE_MAX_SUPPORTED
#define PNG_INFO_IMAGE_SUPPORTED
#define PNG_IO_STATE_SUPPORTED
#define PNG_POINTER_INDEXING_SUPPORTED
#define PNG_READ_INT_FUNCTIONS_SUPPORTED
#define PNG_READ_SUPPORTED
#define PNG_SEQUENTIAL_READ_SUPPORTED
#define PNG_SET_OPTION_SUPPORTED
#define PNG_SET_USER_LIMITS_SUPPORTED
#define PNG_USER_LIMITS_SUPPORTED
#define PNG_USER_MEM_SUPPORTED
#define PNG_WARNINGS_SUPPORTED

/* Read transforms needed by browsers and image viewers. */
#define PNG_READ_16BIT_SUPPORTED
#define PNG_READ_ANCILLARY_CHUNKS_SUPPORTED
#define PNG_READ_BGR_SUPPORTED
#define PNG_READ_CHECK_FOR_INVALID_INDEX_SUPPORTED
#define PNG_READ_EXPAND_SUPPORTED
#define PNG_READ_FILLER_SUPPORTED
#define PNG_READ_GRAY_TO_RGB_SUPPORTED
#define PNG_READ_INTERLACING_SUPPORTED
#define PNG_READ_PACK_SUPPORTED
#define PNG_READ_STRIP_16_TO_8_SUPPORTED
#define PNG_READ_TRANSFORMS_SUPPORTED
#define PNG_READ_tRNS_SUPPORTED

/* Chunks required for common PNG files without text, time or ICC support. */
#define PNG_tRNS_SUPPORTED

/* Settings. */
#define PNG_API_RULE 0
#define PNG_DEFAULT_READ_MACROS 1
#define PNG_IDAT_READ_SIZE PNG_ZBUF_SIZE
#define PNG_INFLATE_BUF_SIZE 1024
#define PNG_LINKAGE_API extern
#define PNG_LINKAGE_CALLBACK extern
#define PNG_LINKAGE_DATA extern
#define PNG_LINKAGE_FUNCTION extern
#define PNG_USER_CHUNK_CACHE_MAX 128
#define PNG_USER_CHUNK_MALLOC_MAX 1048576
#define PNG_USER_HEIGHT_MAX 16384
#define PNG_USER_WIDTH_MAX 16384
#define PNG_ZBUF_SIZE 8192
#define PNG_ZLIB_VERNUM 0

/* Keep libpng's noreturn expectations correct in the freestanding libc. */
#define PNG_ABORT()   \
    do {              \
        abort();      \
        for (;;) {    \
        }             \
    } while (0)

#endif /* PNGLCONF_H */
