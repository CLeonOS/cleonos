#include "user/cleonos_user.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef unsigned char cleonos_user_u8;

typedef struct cleonos_user_sha256_ctx {
    cleonos_user_u8 data[64];
    unsigned int datalen;
    unsigned long long bitlen;
    unsigned int state[8];
} cleonos_user_sha256_ctx;

static int cu_streq(const char *left, const char *right) {
    return (left != (const char *)0 && right != (const char *)0 && strcmp(left, right) == 0) ? 1 : 0;
}

static void cu_copy(char *dst, u64 dst_size, const char *src) {
    if (dst == (char *)0 || dst_size == 0ULL) {
        return;
    }

    if (src == (const char *)0) {
        dst[0] = '\0';
        return;
    }

    (void)strncpy(dst, src, (size_t)(dst_size - 1ULL));
    dst[dst_size - 1ULL] = '\0';
}

static void cu_append(char *dst, u64 dst_size, const char *src) {
    u64 len;

    if (dst == (char *)0 || dst_size == 0ULL || src == (const char *)0) {
        return;
    }

    len = (u64)strlen(dst);
    if (len >= dst_size) {
        dst[dst_size - 1ULL] = '\0';
        return;
    }

    cu_copy(dst + len, dst_size - len, src);
}

static void cu_trim(char *text) {
    u64 start = 0ULL;
    u64 len;

    if (text == (char *)0) {
        return;
    }

    while (text[start] != '\0' && isspace((unsigned char)text[start]) != 0) {
        start++;
    }

    if (start > 0ULL) {
        (void)memmove(text, text + start, strlen(text + start) + 1U);
    }

    len = (u64)strlen(text);
    while (len > 0ULL && isspace((unsigned char)text[len - 1ULL]) != 0) {
        text[len - 1ULL] = '\0';
        len--;
    }
}

static int cu_read_all(const char *path, char *out, u64 out_size, u64 *out_len) {
    u64 size;
    u64 got;

    if (path == (const char *)0 || out == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    out[0] = '\0';
    if (out_len != (u64 *)0) {
        *out_len = 0ULL;
    }

    size = cleonos_sys_fs_stat_size(path);
    if (size == (u64)-1 || size + 1ULL > out_size) {
        return 0;
    }

    if (size == 0ULL) {
        return 1;
    }

    got = cleonos_sys_fs_read(path, out, size);
    if (got != size) {
        return 0;
    }

    out[got] = '\0';
    if (out_len != (u64 *)0) {
        *out_len = got;
    }
    return 1;
}

static unsigned int cu_rotr(unsigned int value, unsigned int count) {
    return (value >> count) | (value << (32U - count));
}

static unsigned int cu_load_be32(const cleonos_user_u8 *data) {
    return (((unsigned int)data[0]) << 24U) | (((unsigned int)data[1]) << 16U) | (((unsigned int)data[2]) << 8U) |
           ((unsigned int)data[3]);
}

static void cu_store_be32(unsigned int value, cleonos_user_u8 *out) {
    out[0] = (cleonos_user_u8)((value >> 24U) & 0xFFU);
    out[1] = (cleonos_user_u8)((value >> 16U) & 0xFFU);
    out[2] = (cleonos_user_u8)((value >> 8U) & 0xFFU);
    out[3] = (cleonos_user_u8)(value & 0xFFU);
}

static void cu_sha256_transform(cleonos_user_sha256_ctx *ctx, const cleonos_user_u8 data[64]) {
    static const unsigned int k[64] = {
        0x428A2F98U, 0x71374491U, 0xB5C0FBCFU, 0xE9B5DBA5U, 0x3956C25BU, 0x59F111F1U, 0x923F82A4U,
        0xAB1C5ED5U, 0xD807AA98U, 0x12835B01U, 0x243185BEU, 0x550C7DC3U, 0x72BE5D74U, 0x80DEB1FEU,
        0x9BDC06A7U, 0xC19BF174U, 0xE49B69C1U, 0xEFBE4786U, 0x0FC19DC6U, 0x240CA1CCU, 0x2DE92C6FU,
        0x4A7484AAU, 0x5CB0A9DCU, 0x76F988DAU, 0x983E5152U, 0xA831C66DU, 0xB00327C8U, 0xBF597FC7U,
        0xC6E00BF3U, 0xD5A79147U, 0x06CA6351U, 0x14292967U, 0x27B70A85U, 0x2E1B2138U, 0x4D2C6DFCU,
        0x53380D13U, 0x650A7354U, 0x766A0ABBU, 0x81C2C92EU, 0x92722C85U, 0xA2BFE8A1U, 0xA81A664BU,
        0xC24B8B70U, 0xC76C51A3U, 0xD192E819U, 0xD6990624U, 0xF40E3585U, 0x106AA070U, 0x19A4C116U,
        0x1E376C08U, 0x2748774CU, 0x34B0BCB5U, 0x391C0CB3U, 0x4ED8AA4AU, 0x5B9CCA4FU, 0x682E6FF3U,
        0x748F82EEU, 0x78A5636FU, 0x84C87814U, 0x8CC70208U, 0x90BEFFFAU, 0xA4506CEBU, 0xBEF9A3F7U,
        0xC67178F2U};
    unsigned int m[64];
    unsigned int a;
    unsigned int b;
    unsigned int c;
    unsigned int d;
    unsigned int e;
    unsigned int f;
    unsigned int g;
    unsigned int h;
    unsigned int i;

    for (i = 0U; i < 16U; i++) {
        m[i] = cu_load_be32(data + (i * 4U));
    }

    for (i = 16U; i < 64U; i++) {
        unsigned int s0 = cu_rotr(m[i - 15U], 7U) ^ cu_rotr(m[i - 15U], 18U) ^ (m[i - 15U] >> 3U);
        unsigned int s1 = cu_rotr(m[i - 2U], 17U) ^ cu_rotr(m[i - 2U], 19U) ^ (m[i - 2U] >> 10U);
        m[i] = m[i - 16U] + s0 + m[i - 7U] + s1;
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for (i = 0U; i < 64U; i++) {
        unsigned int s1 = cu_rotr(e, 6U) ^ cu_rotr(e, 11U) ^ cu_rotr(e, 25U);
        unsigned int ch = (e & f) ^ ((~e) & g);
        unsigned int temp1 = h + s1 + ch + k[i] + m[i];
        unsigned int s0 = cu_rotr(a, 2U) ^ cu_rotr(a, 13U) ^ cu_rotr(a, 22U);
        unsigned int maj = (a & b) ^ (a & c) ^ (b & c);
        unsigned int temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void cu_sha256_init(cleonos_user_sha256_ctx *ctx) {
    ctx->datalen = 0U;
    ctx->bitlen = 0ULL;
    ctx->state[0] = 0x6A09E667U;
    ctx->state[1] = 0xBB67AE85U;
    ctx->state[2] = 0x3C6EF372U;
    ctx->state[3] = 0xA54FF53AU;
    ctx->state[4] = 0x510E527FU;
    ctx->state[5] = 0x9B05688CU;
    ctx->state[6] = 0x1F83D9ABU;
    ctx->state[7] = 0x5BE0CD19U;
}

static void cu_sha256_update(cleonos_user_sha256_ctx *ctx, const cleonos_user_u8 *data, u64 len) {
    u64 i;

    for (i = 0ULL; i < len; i++) {
        ctx->data[ctx->datalen] = data[i];
        ctx->datalen++;
        if (ctx->datalen == 64U) {
            cu_sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512ULL;
            ctx->datalen = 0U;
        }
    }
}

static void cu_sha256_final(cleonos_user_sha256_ctx *ctx, cleonos_user_u8 hash[32]) {
    unsigned int i = ctx->datalen;

    if (ctx->datalen < 56U) {
        ctx->data[i++] = 0x80U;
        while (i < 56U) {
            ctx->data[i++] = 0U;
        }
    } else {
        ctx->data[i++] = 0x80U;
        while (i < 64U) {
            ctx->data[i++] = 0U;
        }
        cu_sha256_transform(ctx, ctx->data);
        (void)memset(ctx->data, 0, 56U);
    }

    ctx->bitlen += (unsigned long long)ctx->datalen * 8ULL;
    ctx->data[63] = (cleonos_user_u8)(ctx->bitlen);
    ctx->data[62] = (cleonos_user_u8)(ctx->bitlen >> 8U);
    ctx->data[61] = (cleonos_user_u8)(ctx->bitlen >> 16U);
    ctx->data[60] = (cleonos_user_u8)(ctx->bitlen >> 24U);
    ctx->data[59] = (cleonos_user_u8)(ctx->bitlen >> 32U);
    ctx->data[58] = (cleonos_user_u8)(ctx->bitlen >> 40U);
    ctx->data[57] = (cleonos_user_u8)(ctx->bitlen >> 48U);
    ctx->data[56] = (cleonos_user_u8)(ctx->bitlen >> 56U);
    cu_sha256_transform(ctx, ctx->data);

    for (i = 0U; i < 8U; i++) {
        cu_store_be32(ctx->state[i], hash + (i * 4U));
    }
}

static char cu_hex_digit(u64 value) {
    value &= 0xFULL;
    return (value < 10ULL) ? (char)('0' + value) : (char)('a' + (value - 10ULL));
}

void cleonos_user_hash_password(const char *password, char out_hex[CLEONOS_USER_HASH_HEX_LEN + 1U]) {
    cleonos_user_sha256_ctx ctx;
    cleonos_user_u8 hash[32];
    u64 i;

    cu_sha256_init(&ctx);
    if (password != (const char *)0) {
        cu_sha256_update(&ctx, (const cleonos_user_u8 *)password, (u64)strlen(password));
    }
    cu_sha256_final(&ctx, hash);

    for (i = 0ULL; i < 32ULL; i++) {
        out_hex[i * 2ULL] = cu_hex_digit(((u64)hash[i]) >> 4U);
        out_hex[(i * 2ULL) + 1ULL] = cu_hex_digit((u64)hash[i]);
    }
    out_hex[CLEONOS_USER_HASH_HEX_LEN] = '\0';
}

int cleonos_user_is_disk_boot(void) {
    char mount_path[64];

    mount_path[0] = '\0';
    if (cleonos_sys_disk_mounted() == 0ULL) {
        return 0;
    }

    if (cleonos_sys_disk_mount_path(mount_path, (u64)sizeof(mount_path)) == 0ULL) {
        return 0;
    }

    return cu_streq(mount_path, "/");
}

int cleonos_user_name_valid(const char *name) {
    u64 i;
    u64 len;

    if (name == (const char *)0 || name[0] == '\0') {
        return 0;
    }

    len = (u64)strlen(name);
    if (len >= (u64)CLEONOS_USER_NAME_MAX) {
        return 0;
    }

    for (i = 0ULL; name[i] != '\0'; i++) {
        char ch = name[i];
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' ||
            ch == '-') {
            continue;
        }
        return 0;
    }

    return 1;
}

void cleonos_user_home_for(char *out_home, u64 out_size, const char *name) {
    if (out_home == (char *)0 || out_size == 0ULL) {
        return;
    }

    out_home[0] = '\0';
    cu_append(out_home, out_size, "/home/");
    cu_append(out_home, out_size, name);
}

int cleonos_user_db_exists(void) {
    return (cleonos_sys_fs_stat_type(CLEONOS_USER_DB_PATH) == 1ULL) ? 1 : 0;
}

static int cu_parse_record(const char *line, cleonos_user_record *out) {
    char tmp[CLEONOS_USER_RECORD_MAX];
    char *save = (char *)0;
    char *name;
    char *role_text;
    char *hash;
    char *home;

    if (line == (const char *)0 || out == (cleonos_user_record *)0) {
        return 0;
    }

    cu_copy(tmp, (u64)sizeof(tmp), line);
    cu_trim(tmp);

    if (tmp[0] == '\0' || tmp[0] == '#') {
        return 0;
    }

    name = strtok_r(tmp, ":", &save);
    role_text = strtok_r((char *)0, ":", &save);
    hash = strtok_r((char *)0, ":", &save);
    home = strtok_r((char *)0, ":", &save);

    if (name == (char *)0 || role_text == (char *)0 || hash == (char *)0 || home == (char *)0) {
        return 0;
    }

    if (cleonos_user_name_valid(name) == 0 || strlen(hash) != CLEONOS_USER_HASH_HEX_LEN || home[0] != '/') {
        return 0;
    }

    (void)memset(out, 0, sizeof(*out));
    cu_copy(out->name, (u64)sizeof(out->name), name);
    cu_copy(out->hash, (u64)sizeof(out->hash), hash);
    cu_copy(out->home, (u64)sizeof(out->home), home);
    out->role = (cu_streq(role_text, "admin") != 0 || cu_streq(role_text, "root") != 0) ? CLEONOS_USER_ROLE_ADMIN
                                                                                        : CLEONOS_USER_ROLE_USER;
    return 1;
}

static void cu_record_line(const cleonos_user_record *record, char *out, u64 out_size) {
    if (out == (char *)0 || out_size == 0ULL) {
        return;
    }

    out[0] = '\0';
    if (record == (const cleonos_user_record *)0) {
        return;
    }

    cu_append(out, out_size, record->name);
    cu_append(out, out_size, ":");
    cu_append(out, out_size, (record->role == CLEONOS_USER_ROLE_ADMIN) ? "admin" : "user");
    cu_append(out, out_size, ":");
    cu_append(out, out_size, record->hash);
    cu_append(out, out_size, ":");
    cu_append(out, out_size, record->home);
    cu_append(out, out_size, "\n");
}

static int cu_db_load(char *out, u64 out_size, u64 *out_len) {
    return cu_read_all(CLEONOS_USER_DB_PATH, out, out_size, out_len);
}

int cleonos_user_find(const char *name, cleonos_user_record *out_record) {
    static char db[4096];
    u64 len = 0ULL;
    u64 start = 0ULL;
    u64 i;

    if (cleonos_user_name_valid(name) == 0 || cu_db_load(db, (u64)sizeof(db), &len) == 0) {
        return 0;
    }

    for (i = 0ULL; i <= len; i++) {
        if (i == len || db[i] == '\n') {
            char line[CLEONOS_USER_RECORD_MAX];
            u64 line_len = i - start;
            cleonos_user_record record;

            if (line_len >= (u64)sizeof(line)) {
                line_len = (u64)sizeof(line) - 1ULL;
            }
            (void)memcpy(line, db + start, (size_t)line_len);
            line[line_len] = '\0';

            if (cu_parse_record(line, &record) != 0 && cu_streq(record.name, name) != 0) {
                if (out_record != (cleonos_user_record *)0) {
                    *out_record = record;
                }
                return 1;
            }

            start = i + 1ULL;
        }
    }

    return 0;
}

int cleonos_user_verify_password(const char *name, const char *password, cleonos_user_record *out_record) {
    cleonos_user_record record;
    char hash[CLEONOS_USER_HASH_HEX_LEN + 1U];

    if (cleonos_user_find(name, &record) == 0) {
        return 0;
    }

    cleonos_user_hash_password(password, hash);
    if (cu_streq(record.hash, hash) == 0) {
        return 0;
    }

    if (out_record != (cleonos_user_record *)0) {
        *out_record = record;
    }
    return 1;
}

static int cu_db_rewrite_with(const cleonos_user_record *replace_record, const char *remove_name, int *out_found) {
    static char db[4096];
    static char next_db[4096];
    u64 len = 0ULL;
    u64 next_len = 0ULL;
    u64 start = 0ULL;
    u64 i;
    int found = 0;

    if (out_found != (int *)0) {
        *out_found = 0;
    }

    if (cu_db_load(db, (u64)sizeof(db), &len) == 0) {
        return 0;
    }

    next_db[0] = '\0';
    for (i = 0ULL; i <= len; i++) {
        if (i == len || db[i] == '\n') {
            char line[CLEONOS_USER_RECORD_MAX];
            char record_line[CLEONOS_USER_RECORD_MAX];
            u64 line_len = i - start;
            cleonos_user_record record;
            int emit_original = 1;

            if (line_len >= (u64)sizeof(line)) {
                line_len = (u64)sizeof(line) - 1ULL;
            }
            (void)memcpy(line, db + start, (size_t)line_len);
            line[line_len] = '\0';

            if (cu_parse_record(line, &record) != 0) {
                if (remove_name != (const char *)0 && cu_streq(record.name, remove_name) != 0) {
                    found = 1;
                    emit_original = 0;
                } else if (replace_record != (const cleonos_user_record *)0 && cu_streq(record.name, replace_record->name) != 0) {
                    found = 1;
                    cu_record_line(replace_record, record_line, (u64)sizeof(record_line));
                    if (next_len + (u64)strlen(record_line) + 1ULL >= (u64)sizeof(next_db)) {
                        return 0;
                    }
                    cu_append(next_db, (u64)sizeof(next_db), record_line);
                    next_len += (u64)strlen(record_line);
                    emit_original = 0;
                }
            }

            if (emit_original != 0 && line[0] != '\0') {
                if (next_len + line_len + 2ULL >= (u64)sizeof(next_db)) {
                    return 0;
                }
                cu_append(next_db, (u64)sizeof(next_db), line);
                cu_append(next_db, (u64)sizeof(next_db), "\n");
                next_len += line_len + 1ULL;
            }

            start = i + 1ULL;
        }
    }

    if (out_found != (int *)0) {
        *out_found = found;
    }

    if (found == 0) {
        return 0;
    }

    return (cleonos_sys_fs_write(CLEONOS_USER_DB_PATH, next_db, (u64)strlen(next_db)) != 0ULL) ? 1 : 0;
}

int cleonos_user_create(const char *name, const char *password, u64 role, int allow_existing) {
    cleonos_user_record record;
    char line[CLEONOS_USER_RECORD_MAX];

    if (cleonos_user_name_valid(name) == 0 || password == (const char *)0 || password[0] == '\0') {
        return 0;
    }

    if (cleonos_sys_user_add(name, password, role) != 0ULL) {
        return 1;
    }

    if (cleonos_sys_fs_stat_type(CLEONOS_USER_DB_PATH) != 1ULL &&
        cleonos_sys_fs_write(CLEONOS_USER_DB_PATH, "", 0ULL) == 0ULL) {
        return 0;
    }

    if (cleonos_user_find(name, &record) != 0) {
        return (allow_existing != 0) ? 1 : 0;
    }

    (void)memset(&record, 0, sizeof(record));
    cu_copy(record.name, (u64)sizeof(record.name), name);
    record.role = (role == CLEONOS_USER_ROLE_ADMIN) ? CLEONOS_USER_ROLE_ADMIN : CLEONOS_USER_ROLE_USER;
    cleonos_user_hash_password(password, record.hash);
    cleonos_user_home_for(record.home, (u64)sizeof(record.home), name);

    if (cleonos_sys_fs_stat_type("/home") != 2ULL && cleonos_sys_fs_mkdir("/home") == 0ULL) {
        return 0;
    }
    if (cleonos_sys_fs_stat_type(record.home) != 2ULL && cleonos_sys_fs_mkdir(record.home) == 0ULL) {
        return 0;
    }

    cu_record_line(&record, line, (u64)sizeof(line));
    return (cleonos_sys_fs_append(CLEONOS_USER_DB_PATH, line, (u64)strlen(line)) != 0ULL) ? 1 : 0;
}

int cleonos_user_change_password(const char *name, const char *new_password) {
    cleonos_user_record record;
    int found = 0;

    if (cleonos_user_find(name, &record) == 0 || new_password == (const char *)0 || new_password[0] == '\0') {
        return 0;
    }

    cleonos_user_hash_password(new_password, record.hash);
    return cu_db_rewrite_with(&record, (const char *)0, &found);
}

int cleonos_user_set_role(const char *name, u64 role) {
    cleonos_user_record record;
    int found = 0;

    if (cleonos_user_find(name, &record) == 0) {
        return 0;
    }

    if (cleonos_sys_user_set_role(name, role) != 0ULL) {
        return 1;
    }

    record.role = (role == CLEONOS_USER_ROLE_ADMIN) ? CLEONOS_USER_ROLE_ADMIN : CLEONOS_USER_ROLE_USER;
    return cu_db_rewrite_with(&record, (const char *)0, &found);
}

int cleonos_user_remove(const char *name) {
    int found = 0;

    if (cleonos_user_name_valid(name) == 0 || cu_streq(name, "root") != 0) {
        return 0;
    }

    if (cleonos_sys_user_remove(name) != 0ULL) {
        return 1;
    }

    return cu_db_rewrite_with((const cleonos_user_record *)0, name, &found);
}

int cleonos_user_list(void (*emit)(const cleonos_user_record *record, void *ctx), void *ctx) {
    u64 kernel_count;
    u64 i;
    static char db[4096];
    u64 len = 0ULL;
    u64 start = 0ULL;
    int count = 0;

    kernel_count = cleonos_sys_user_count();
    if (emit != (void (*)(const cleonos_user_record *, void *))0 && kernel_count != (u64)-1 && kernel_count > 0ULL) {
        for (i = 0ULL; i < kernel_count; i++) {
            cleonos_user_info info;
            cleonos_user_record record;

            if (cleonos_sys_user_at(i, &info) == 0ULL) {
                continue;
            }

            (void)memset(&record, 0, sizeof(record));
            cu_copy(record.name, (u64)sizeof(record.name), info.name);
            record.role = info.role;
            cu_copy(record.home, (u64)sizeof(record.home), info.home);
            emit(&record, ctx);
            count++;
        }
        return count;
    }

    if (emit == (void (*)(const cleonos_user_record *, void *))0 || cu_db_load(db, (u64)sizeof(db), &len) == 0) {
        return 0;
    }

    for (i = 0ULL; i <= len; i++) {
        if (i == len || db[i] == '\n') {
            char line[CLEONOS_USER_RECORD_MAX];
            u64 line_len = i - start;
            cleonos_user_record record;

            if (line_len >= (u64)sizeof(line)) {
                line_len = (u64)sizeof(line) - 1ULL;
            }
            (void)memcpy(line, db + start, (size_t)line_len);
            line[line_len] = '\0';

            if (cu_parse_record(line, &record) != 0) {
                emit(&record, ctx);
                count++;
            }

            start = i + 1ULL;
        }
    }

    return count;
}

int cleonos_user_session_write(const cleonos_user_record *record) {
    if (record == (const cleonos_user_record *)0) {
        return 0;
    }

    return 1;
}

int cleonos_user_session_read(cleonos_user_record *out_record) {
    cleonos_user_info info;

    if (out_record != (cleonos_user_record *)0 && cleonos_sys_user_current(&info) != 0ULL && info.logged_in != 0ULL) {
        (void)memset(out_record, 0, sizeof(*out_record));
        cu_copy(out_record->name, (u64)sizeof(out_record->name), info.name);
        out_record->role = info.role;
        cu_copy(out_record->home, (u64)sizeof(out_record->home), info.home);
        return 1;
    }

    return 0;
}

int cleonos_user_session_clear(void) {
    (void)cleonos_sys_user_logout();
    (void)cleonos_sys_fs_remove(CLEONOS_USER_SESSION_PATH);
    return 1;
}

int cleonos_user_current_is_admin(void) {
    return (cleonos_sys_user_is_admin() != 0ULL) ? 1 : 0;
}
