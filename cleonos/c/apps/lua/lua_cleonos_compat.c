#include <cleonos_syscall.h>
#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lua_cleonos_compat.h"

#define CLEONOS_LUA_FILE_MAGIC 0x4C554146U
#define CLEONOS_LUA_MAX_FILES 12U

int errno = 0;

struct cleonos_lua_file {
    unsigned int magic;
    int fd;
    int stdio;
    int eof;
    int err;
    int ungot;
    unsigned char ungot_ch;
};

static struct cleonos_lua_file lua_stdin_file = {CLEONOS_LUA_FILE_MAGIC, 0, 1, 0, 0, 0, 0};
static struct cleonos_lua_file lua_stdout_file = {CLEONOS_LUA_FILE_MAGIC, 1, 1, 0, 0, 0, 0};
static struct cleonos_lua_file lua_stderr_file = {CLEONOS_LUA_FILE_MAGIC, 2, 1, 0, 0, 0, 0};
static struct cleonos_lua_file lua_files[CLEONOS_LUA_MAX_FILES];

CLEONOS_LUA_FILE *cleonos_lua_stdin = (CLEONOS_LUA_FILE *)&lua_stdin_file;
CLEONOS_LUA_FILE *cleonos_lua_stdout = (CLEONOS_LUA_FILE *)&lua_stdout_file;
CLEONOS_LUA_FILE *cleonos_lua_stderr = (CLEONOS_LUA_FILE *)&lua_stderr_file;

int *__errno_location(void) {
    return &errno;
}

char *strerror(int errnum) {
    switch (errnum) {
    case 0:
        return "no error";
    case ENOENT:
        return "not found";
    case EACCES:
        return "permission denied";
    case EBADF:
        return "bad file descriptor";
    case EINVAL:
        return "invalid argument";
    case ENOMEM:
        return "out of memory";
    case EIO:
        return "i/o error";
    case ENOSYS:
        return "not implemented";
    default:
        return "error";
    }
}

static struct cleonos_lua_file *lua_file_from_stream(FILE *stream) {
    struct cleonos_lua_file *file = (struct cleonos_lua_file *)stream;

    if (file == (struct cleonos_lua_file *)0 || file->magic != CLEONOS_LUA_FILE_MAGIC) {
        errno = EBADF;
        return (struct cleonos_lua_file *)0;
    }

    return file;
}

static struct cleonos_lua_file *lua_alloc_file(void) {
    unsigned int i;

    for (i = 0U; i < CLEONOS_LUA_MAX_FILES; i++) {
        if (lua_files[i].magic == 0U) {
            memset(&lua_files[i], 0, sizeof(lua_files[i]));
            lua_files[i].magic = CLEONOS_LUA_FILE_MAGIC;
            lua_files[i].fd = -1;
            return &lua_files[i];
        }
    }

    errno = EMFILE;
    return (struct cleonos_lua_file *)0;
}

static u64 lua_open_flags_from_mode(const char *mode) {
    int write = 0;
    int append = 0;
    int plus = 0;
    const char *p = mode;
    u64 flags;

    if (mode == (const char *)0 || mode[0] == '\0') {
        errno = EINVAL;
        return (u64)-1;
    }

    if (mode[0] == 'w') {
        write = 1;
    } else if (mode[0] == 'a') {
        write = 1;
        append = 1;
    } else if (mode[0] != 'r') {
        errno = EINVAL;
        return (u64)-1;
    }

    while (*p != '\0') {
        if (*p == '+') {
            plus = 1;
        }
        p++;
    }

    flags = plus ? CLEONOS_O_RDWR : (write ? CLEONOS_O_WRONLY : CLEONOS_O_RDONLY);
    if (write != 0) {
        flags |= CLEONOS_O_CREAT;
    }
    if (mode[0] == 'w') {
        flags |= CLEONOS_O_TRUNC;
    }
    if (append != 0) {
        flags |= CLEONOS_O_APPEND;
    }

    return flags;
}

CLEONOS_LUA_FILE *cleonos_lua_fopen(const char *path, const char *mode) {
    struct cleonos_lua_file *file;
    u64 flags;
    u64 fd;

    if (path == (const char *)0 || path[0] == '\0') {
        errno = EINVAL;
        return (CLEONOS_LUA_FILE *)0;
    }

    flags = lua_open_flags_from_mode(mode);
    if (flags == (u64)-1) {
        return (CLEONOS_LUA_FILE *)0;
    }

    fd = cleonos_sys_fd_open(path, flags, 0ULL);
    if (fd == (u64)-1) {
        errno = ENOENT;
        return (CLEONOS_LUA_FILE *)0;
    }

    file = lua_alloc_file();
    if (file == (struct cleonos_lua_file *)0) {
        (void)cleonos_sys_fd_close(fd);
        return (FILE *)0;
    }

    file->fd = (int)fd;
    return (CLEONOS_LUA_FILE *)file;
}

CLEONOS_LUA_FILE *cleonos_lua_freopen(const char *path, const char *mode, CLEONOS_LUA_FILE *stream) {
    CLEONOS_LUA_FILE *next;
    struct cleonos_lua_file *file = lua_file_from_stream((FILE *)stream);

    if (file == (struct cleonos_lua_file *)0) {
        return (CLEONOS_LUA_FILE *)0;
    }

    next = cleonos_lua_fopen(path, mode);
    if (next == (CLEONOS_LUA_FILE *)0) {
        return (CLEONOS_LUA_FILE *)0;
    }

    if (file->stdio == 0 && file->fd >= 0) {
        (void)cleonos_sys_fd_close((u64)file->fd);
    }

    *file = *(struct cleonos_lua_file *)next;
    memset(next, 0, sizeof(struct cleonos_lua_file));
    return stream;
}

int cleonos_lua_fclose(CLEONOS_LUA_FILE *stream) {
    struct cleonos_lua_file *file = lua_file_from_stream((FILE *)stream);

    if (file == (struct cleonos_lua_file *)0) {
        return EOF;
    }

    if (file->stdio == 0 && file->fd >= 0 && cleonos_sys_fd_close((u64)file->fd) == (u64)-1) {
        file->err = 1;
        errno = EIO;
        return EOF;
    }

    if (file->stdio == 0) {
        memset(file, 0, sizeof(*file));
    }

    return 0;
}

size_t cleonos_lua_fread(void *out, size_t size, size_t nmemb, CLEONOS_LUA_FILE *stream) {
    struct cleonos_lua_file *file = lua_file_from_stream((FILE *)stream);
    unsigned char *dst = (unsigned char *)out;
    size_t total;
    size_t done = 0U;

    if (file == (struct cleonos_lua_file *)0 || out == (void *)0) {
        return 0U;
    }

    if (size == 0U || nmemb == 0U) {
        return 0U;
    }

    total = size * nmemb;
    if (file->ungot != 0 && total > 0U) {
        dst[done++] = file->ungot_ch;
        file->ungot = 0;
    }

    while (done < total) {
        u64 got = cleonos_sys_fd_read((u64)file->fd, dst + done, (u64)(total - done));

        if (got == (u64)-1) {
            file->err = 1;
            errno = EIO;
            break;
        }

        if (got == 0ULL) {
            file->eof = 1;
            break;
        }

        done += (size_t)got;
    }

    return done / size;
}

size_t cleonos_lua_fwrite(const void *data, size_t size, size_t nmemb, CLEONOS_LUA_FILE *stream) {
    struct cleonos_lua_file *file = lua_file_from_stream((FILE *)stream);
    const unsigned char *src = (const unsigned char *)data;
    size_t total;
    size_t done = 0U;

    if (file == (struct cleonos_lua_file *)0 || data == (const void *)0) {
        return 0U;
    }

    if (size == 0U || nmemb == 0U) {
        return 0U;
    }

    total = size * nmemb;
    while (done < total) {
        u64 wrote = cleonos_sys_fd_write((u64)file->fd, src + done, (u64)(total - done));

        if (wrote == (u64)-1 || wrote == 0ULL) {
            file->err = 1;
            errno = EIO;
            break;
        }

        done += (size_t)wrote;
    }

    return done / size;
}

int cleonos_lua_fflush(CLEONOS_LUA_FILE *stream) {
    (void)stream;
    return 0;
}

int cleonos_lua_ferror(CLEONOS_LUA_FILE *stream) {
    struct cleonos_lua_file *file = lua_file_from_stream((FILE *)stream);
    return (file != (struct cleonos_lua_file *)0) ? file->err : 1;
}

int cleonos_lua_feof(CLEONOS_LUA_FILE *stream) {
    struct cleonos_lua_file *file = lua_file_from_stream((FILE *)stream);
    return (file != (struct cleonos_lua_file *)0) ? file->eof : 1;
}

int cleonos_lua_getc(CLEONOS_LUA_FILE *stream) {
    return cleonos_lua_fgetc(stream);
}

int cleonos_lua_fgetc(CLEONOS_LUA_FILE *stream) {
    unsigned char ch = 0U;
    return (cleonos_lua_fread(&ch, 1U, 1U, stream) == 1U) ? (int)ch : EOF;
}

int cleonos_lua_ungetc(int ch, CLEONOS_LUA_FILE *stream) {
    struct cleonos_lua_file *file = lua_file_from_stream((FILE *)stream);

    if (file == (struct cleonos_lua_file *)0 || ch == EOF || file->ungot != 0) {
        return EOF;
    }

    file->ungot = 1;
    file->ungot_ch = (unsigned char)ch;
    file->eof = 0;
    return ch & 0xFF;
}

int cleonos_lua_vfprintf(CLEONOS_LUA_FILE *stream, const char *fmt, va_list args) {
    char buf[512];
    int len;

    if (stream == stdout) {
        return vprintf(fmt, args);
    }

    if (stream == stderr) {
        return vdprintf(2, fmt, args);
    }

    len = vsnprintf(buf, (unsigned long)sizeof(buf), fmt, args);
    if (len < 0) {
        return len;
    }

    if (cleonos_lua_fwrite(buf, 1U, (size_t)len, stream) != (size_t)len) {
        return EOF;
    }

    return len;
}

int cleonos_lua_fprintf(CLEONOS_LUA_FILE *stream, const char *fmt, ...) {
    va_list args;
    int rc;

    va_start(args, fmt);
    rc = cleonos_lua_vfprintf(stream, fmt, args);
    va_end(args);
    return rc;
}

int cleonos_lua_fputs(const char *text, CLEONOS_LUA_FILE *stream) {
    size_t len;

    if (text == (const char *)0) {
        errno = EINVAL;
        return EOF;
    }

    len = strlen(text);
    return (cleonos_lua_fwrite(text, 1U, len, stream) == len) ? (int)len : EOF;
}

int cleonos_lua_fputc(int ch, CLEONOS_LUA_FILE *stream) {
    unsigned char out = (unsigned char)ch;
    return (cleonos_lua_fwrite(&out, 1U, 1U, stream) == 1U) ? (int)out : EOF;
}

char *cleonos_lua_fgets(char *out, int size, CLEONOS_LUA_FILE *stream) {
    int pos = 0;

    if (out == (char *)0 || size <= 0 || stream == (CLEONOS_LUA_FILE *)0) {
        errno = EINVAL;
        return (char *)0;
    }

    while (pos + 1 < size) {
        int ch = cleonos_lua_fgetc(stream);

        if (ch == EOF) {
            break;
        }

        out[pos++] = (char)ch;
        if (ch == '\n') {
            break;
        }
    }

    if (pos == 0) {
        return (char *)0;
    }

    out[pos] = '\0';
    return out;
}

static unsigned long lua_fmt_strlen(const char *text) {
    unsigned long len = 0UL;

    if (text == (const char *)0) {
        return 0UL;
    }

    while (text[len] != '\0') {
        len++;
    }

    return len;
}

static char *lua_fmt_append_repeat(char *out, char ch, int count) {
    while (count > 0) {
        *out++ = ch;
        count--;
    }

    return out;
}

static char *lua_fmt_append_mem(char *out, const char *text, unsigned long len) {
    unsigned long i;

    for (i = 0UL; i < len; i++) {
        out[i] = text[i];
    }

    return out + len;
}

static unsigned long lua_fmt_u64(char *out, unsigned long long value, unsigned int base, int upper) {
    char tmp[64];
    unsigned long pos = 0UL;
    unsigned long len = 0UL;
    const char *digits = (upper != 0) ? "0123456789ABCDEF" : "0123456789abcdef";

    if (base < 2U || base > 16U) {
        out[0] = '\0';
        return 0UL;
    }

    if (value == 0ULL) {
        out[0] = '0';
        out[1] = '\0';
        return 1UL;
    }

    while (value != 0ULL && pos < (unsigned long)sizeof(tmp)) {
        tmp[pos++] = digits[value % (unsigned long long)base];
        value /= (unsigned long long)base;
    }

    while (pos > 0UL) {
        out[len++] = tmp[--pos];
    }
    out[len] = '\0';
    return len;
}

struct lua_fmt_state {
    int left;
    int plus;
    int space;
    int alt;
    int zero;
    int width;
    int precision;
    int precision_set;
    int upper;
};

static char *lua_fmt_apply_width(char *out, const char *text, unsigned long len, struct lua_fmt_state *st) {
    int pad = (st->width > (int)len) ? st->width - (int)len : 0;

    if (st->left == 0) {
        out = lua_fmt_append_repeat(out, ' ', pad);
    }
    out = lua_fmt_append_mem(out, text, len);
    if (st->left != 0) {
        out = lua_fmt_append_repeat(out, ' ', pad);
    }

    return out;
}

static char *lua_fmt_signed(char *out, long long value, struct lua_fmt_state *st) {
    char digits[64];
    char tmp[128];
    unsigned long long mag;
    unsigned long digits_len;
    unsigned long pos = 0UL;
    int zeros;
    int pad;
    char sign = '\0';
    int total_len;

    if (value < 0LL) {
        sign = '-';
        mag = (unsigned long long)(-(value + 1LL)) + 1ULL;
    } else {
        mag = (unsigned long long)value;
        if (st->plus != 0) {
            sign = '+';
        } else if (st->space != 0) {
            sign = ' ';
        }
    }

    digits_len = lua_fmt_u64(digits, mag, 10U, 0);
    if (st->precision_set != 0 && st->precision == 0 && mag == 0ULL) {
        digits_len = 0UL;
    }

    zeros = (st->precision_set != 0 && st->precision > (int)digits_len) ? st->precision - (int)digits_len : 0;
    total_len = (int)digits_len + zeros + ((sign != '\0') ? 1 : 0);
    pad = (st->width > total_len) ? st->width - total_len : 0;

    if (st->left == 0 && st->zero != 0 && st->precision_set == 0) {
        if (sign != '\0') {
            tmp[pos++] = sign;
            sign = '\0';
        }
        while (pad-- > 0 && pos < sizeof(tmp) - 1UL) {
            tmp[pos++] = '0';
        }
    } else if (st->left == 0) {
        while (pad-- > 0 && pos < sizeof(tmp) - 1UL) {
            tmp[pos++] = ' ';
        }
    }

    if (sign != '\0' && pos < sizeof(tmp) - 1UL) {
        tmp[pos++] = sign;
    }
    while (zeros-- > 0 && pos < sizeof(tmp) - 1UL) {
        tmp[pos++] = '0';
    }
    pos = (unsigned long)(lua_fmt_append_mem(tmp + pos, digits, digits_len) - tmp);
    if (st->left != 0) {
        while (pad-- > 0 && pos < sizeof(tmp) - 1UL) {
            tmp[pos++] = ' ';
        }
    }
    tmp[pos] = '\0';

    return lua_fmt_append_mem(out, tmp, pos);
}

static char *lua_fmt_unsigned(char *out, unsigned long long value, unsigned int base, struct lua_fmt_state *st) {
    char digits[64];
    char tmp[160];
    const char *prefix = "";
    unsigned long prefix_len = 0UL;
    unsigned long digits_len;
    unsigned long pos = 0UL;
    int zeros;
    int pad;
    int total_len;

    digits_len = lua_fmt_u64(digits, value, base, st->upper);
    if (st->precision_set != 0 && st->precision == 0 && value == 0ULL) {
        digits_len = 0UL;
    }

    if (st->alt != 0 && value != 0ULL) {
        if (base == 16U) {
            prefix = (st->upper != 0) ? "0X" : "0x";
            prefix_len = 2UL;
        } else if (base == 8U) {
            prefix = "0";
            prefix_len = 1UL;
        }
    }

    zeros = (st->precision_set != 0 && st->precision > (int)digits_len) ? st->precision - (int)digits_len : 0;
    total_len = (int)(prefix_len + digits_len) + zeros;
    pad = (st->width > total_len) ? st->width - total_len : 0;

    if (st->left == 0 && st->zero != 0 && st->precision_set == 0) {
        pos = (unsigned long)(lua_fmt_append_mem(tmp + pos, prefix, prefix_len) - tmp);
        prefix_len = 0UL;
        while (pad-- > 0 && pos < sizeof(tmp) - 1UL) {
            tmp[pos++] = '0';
        }
    } else if (st->left == 0) {
        while (pad-- > 0 && pos < sizeof(tmp) - 1UL) {
            tmp[pos++] = ' ';
        }
    }

    pos = (unsigned long)(lua_fmt_append_mem(tmp + pos, prefix, prefix_len) - tmp);
    while (zeros-- > 0 && pos < sizeof(tmp) - 1UL) {
        tmp[pos++] = '0';
    }
    pos = (unsigned long)(lua_fmt_append_mem(tmp + pos, digits, digits_len) - tmp);
    if (st->left != 0) {
        while (pad-- > 0 && pos < sizeof(tmp) - 1UL) {
            tmp[pos++] = ' ';
        }
    }
    tmp[pos] = '\0';

    return lua_fmt_append_mem(out, tmp, pos);
}

static double lua_fmt_pow10(int exp) {
    double out = 1.0;

    while (exp > 0) {
        out *= 10.0;
        exp--;
    }
    while (exp < 0) {
        out *= 0.1;
        exp++;
    }

    return out;
}

static int lua_fmt_decimal_exp(double value) {
    int exp = 0;

    if (value <= 0.0) {
        return 0;
    }

    while (value >= 10.0 && exp < 308) {
        value *= 0.1;
        exp++;
    }
    while (value < 1.0 && exp > -308) {
        value *= 10.0;
        exp--;
    }

    return exp;
}

static unsigned long lua_fmt_fixed_raw(char *out, double value, int precision, int trim) {
    char intbuf[64];
    unsigned long long whole;
    unsigned long long frac;
    unsigned long int_len;
    unsigned long pos = 0UL;
    double scale;
    double rounded;
    int i;

    if (precision < 0) {
        precision = 6;
    }
    if (precision > 12) {
        precision = 12;
    }

    scale = lua_fmt_pow10(precision);
    rounded = value * scale + 0.5;
    whole = (unsigned long long)(rounded / scale);
    frac = (unsigned long long)(rounded - ((double)whole * scale));
    int_len = lua_fmt_u64(intbuf, whole, 10U, 0);
    pos = (unsigned long)(lua_fmt_append_mem(out + pos, intbuf, int_len) - out);

    if (precision > 0) {
        char fracbuf[16];
        int end = precision;

        for (i = precision - 1; i >= 0; i--) {
            fracbuf[i] = (char)('0' + (frac % 10ULL));
            frac /= 10ULL;
        }

        if (trim != 0) {
            while (end > 0 && fracbuf[end - 1] == '0') {
                end--;
            }
        }

        if (end > 0) {
            out[pos++] = '.';
            for (i = 0; i < end; i++) {
                out[pos++] = fracbuf[i];
            }
        }
    }

    out[pos] = '\0';
    return pos;
}

static unsigned long lua_fmt_exp_raw(char *out, double value, int precision, int upper, int trim) {
    int exp = lua_fmt_decimal_exp(value);
    double mant = value / lua_fmt_pow10(exp);
    unsigned long pos;
    char echar = (upper != 0) ? 'E' : 'e';
    int exp_abs;

    if (mant >= 10.0) {
        mant *= 0.1;
        exp++;
    } else if (mant < 1.0 && value != 0.0) {
        mant *= 10.0;
        exp--;
    }

    pos = lua_fmt_fixed_raw(out, mant, precision, trim);
    out[pos++] = echar;
    out[pos++] = (exp < 0) ? '-' : '+';
    exp_abs = (exp < 0) ? -exp : exp;
    if (exp_abs < 10) {
        out[pos++] = '0';
        out[pos++] = (char)('0' + exp_abs);
    } else {
        char ebuf[16];
        unsigned long elen = lua_fmt_u64(ebuf, (unsigned long long)exp_abs, 10U, 0);
        pos = (unsigned long)(lua_fmt_append_mem(out + pos, ebuf, elen) - out);
    }
    out[pos] = '\0';
    return pos;
}

static unsigned long lua_fmt_hex_raw(char *out, double value, int precision, int upper, int trim) {
    int exp = 0;
    double mant = frexp(value, &exp) * 2.0;
    double frac;
    const char *digits = (upper != 0) ? "0123456789ABCDEF" : "0123456789abcdef";
    char pch = (upper != 0) ? 'P' : 'p';
    unsigned long pos = 0UL;
    int end;
    int i;

    if (precision < 0) {
        precision = 13;
    }
    if (precision > 13) {
        precision = 13;
    }

    exp--;
    out[pos++] = '0';
    out[pos++] = (upper != 0) ? 'X' : 'x';
    out[pos++] = '1';
    frac = mant - 1.0;

    if (precision > 0) {
        char fracbuf[16];
        for (i = 0; i < precision; i++) {
            int d;
            frac *= 16.0;
            d = (int)frac;
            if (d < 0) {
                d = 0;
            } else if (d > 15) {
                d = 15;
            }
            fracbuf[i] = digits[d];
            frac -= (double)d;
        }

        end = precision;
        if (trim != 0) {
            while (end > 0 && fracbuf[end - 1] == '0') {
                end--;
            }
        }
        if (end > 0) {
            out[pos++] = '.';
            for (i = 0; i < end; i++) {
                out[pos++] = fracbuf[i];
            }
        }
    }

    out[pos++] = pch;
    out[pos++] = (exp < 0) ? '-' : '+';
    {
        char ebuf[16];
        unsigned long elen = lua_fmt_u64(ebuf, (unsigned long long)((exp < 0) ? -exp : exp), 10U, 0);
        pos = (unsigned long)(lua_fmt_append_mem(out + pos, ebuf, elen) - out);
    }
    out[pos] = '\0';
    return pos;
}

static char *lua_fmt_float(char *out, double value, char spec, struct lua_fmt_state *st) {
    char raw[160];
    char tmp[192];
    unsigned long raw_len;
    unsigned long pos = 0UL;
    int precision = st->precision_set ? st->precision : 6;
    int negative = (value < 0.0) ? 1 : 0;
    char sign = '\0';
    int pad;

    if (negative != 0) {
        value = -value;
        sign = '-';
    } else if (st->plus != 0) {
        sign = '+';
    } else if (st->space != 0) {
        sign = ' ';
    }

    if (value != value) {
        raw[0] = st->upper ? 'N' : 'n';
        raw[1] = st->upper ? 'A' : 'a';
        raw[2] = st->upper ? 'N' : 'n';
        raw[3] = '\0';
        raw_len = 3UL;
    } else if (value == HUGE_VAL || value > 1.0e308) {
        raw[0] = st->upper ? 'I' : 'i';
        raw[1] = st->upper ? 'N' : 'n';
        raw[2] = st->upper ? 'F' : 'f';
        raw[3] = '\0';
        raw_len = 3UL;
    } else if (spec == 'e' || spec == 'E') {
        raw_len = lua_fmt_exp_raw(raw, value, precision, st->upper, 0);
    } else if (spec == 'g' || spec == 'G') {
        int exp = lua_fmt_decimal_exp(value);
        int gprec = precision;
        if (gprec == 0) {
            gprec = 1;
        }
        if (exp < -4 || exp >= gprec) {
            raw_len = lua_fmt_exp_raw(raw, value, gprec - 1, st->upper, st->alt == 0);
        } else {
            raw_len = lua_fmt_fixed_raw(raw, value, gprec - (exp + 1), st->alt == 0);
        }
    } else if (spec == 'a' || spec == 'A') {
        raw_len = lua_fmt_hex_raw(raw, value, st->precision_set ? st->precision : -1, st->upper, st->alt == 0);
    } else {
        raw_len = lua_fmt_fixed_raw(raw, value, precision, 0);
    }

    if (sign != '\0') {
        tmp[pos++] = sign;
    }
    pos = (unsigned long)(lua_fmt_append_mem(tmp + pos, raw, raw_len) - tmp);
    tmp[pos] = '\0';

    pad = (st->width > (int)pos) ? st->width - (int)pos : 0;
    if (st->left == 0) {
        out = lua_fmt_append_repeat(out, (st->zero != 0) ? '0' : ' ', pad);
    }
    out = lua_fmt_append_mem(out, tmp, pos);
    if (st->left != 0) {
        out = lua_fmt_append_repeat(out, ' ', pad);
    }

    return out;
}

int cleonos_lua_sprintf(char *out, const char *fmt, ...) {
    va_list args;
    const char *p = fmt;
    char *dst = out;

    if (out == (char *)0 || fmt == (const char *)0) {
        errno = EINVAL;
        return EOF;
    }

    va_start(args, fmt);
    while (*p != '\0') {
        struct lua_fmt_state st;
        int length = 0;
        char spec;

        if (*p != '%') {
            *dst++ = *p++;
            continue;
        }

        p++;
        if (*p == '%') {
            *dst++ = *p++;
            continue;
        }

        memset(&st, 0, sizeof(st));
        st.precision = 0;
        while (*p == '-' || *p == '+' || *p == ' ' || *p == '#' || *p == '0') {
            if (*p == '-') {
                st.left = 1;
            } else if (*p == '+') {
                st.plus = 1;
            } else if (*p == ' ') {
                st.space = 1;
            } else if (*p == '#') {
                st.alt = 1;
            } else if (*p == '0') {
                st.zero = 1;
            }
            p++;
        }

        while (*p >= '0' && *p <= '9') {
            st.width = (st.width * 10) + (*p - '0');
            p++;
        }

        if (*p == '.') {
            st.precision_set = 1;
            st.precision = 0;
            p++;
            while (*p >= '0' && *p <= '9') {
                st.precision = (st.precision * 10) + (*p - '0');
                p++;
            }
        }

        if (*p == 'l') {
            length = 1;
            p++;
            if (*p == 'l') {
                length = 2;
                p++;
            }
        } else if (*p == 'z') {
            length = 3;
            p++;
        } else if (*p == 'L') {
            length = 4;
            p++;
        }

        spec = *p;
        if (spec == '\0') {
            break;
        }
        p++;

        st.upper = (spec >= 'A' && spec <= 'Z') ? 1 : 0;
        if (spec == 'd' || spec == 'i') {
            long long value;
            if (length == 2 || length == 3) {
                value = va_arg(args, long long);
            } else if (length == 1) {
                value = (long long)va_arg(args, long);
            } else {
                value = (long long)va_arg(args, int);
            }
            dst = lua_fmt_signed(dst, value, &st);
        } else if (spec == 'u' || spec == 'x' || spec == 'X' || spec == 'o') {
            unsigned long long value;
            unsigned int base = (spec == 'o') ? 8U : ((spec == 'u') ? 10U : 16U);
            if (length == 2 || length == 3) {
                value = va_arg(args, unsigned long long);
            } else if (length == 1) {
                value = (unsigned long long)va_arg(args, unsigned long);
            } else {
                value = (unsigned long long)va_arg(args, unsigned int);
            }
            dst = lua_fmt_unsigned(dst, value, base, &st);
        } else if (spec == 'p') {
            st.alt = 1;
            dst = lua_fmt_unsigned(dst, (unsigned long long)(unsigned long)va_arg(args, void *), 16U, &st);
        } else if (spec == 's') {
            const char *text = va_arg(args, const char *);
            unsigned long len;
            if (text == (const char *)0) {
                text = "(null)";
            }
            len = lua_fmt_strlen(text);
            if (st.precision_set != 0 && st.precision < (int)len) {
                len = (unsigned long)st.precision;
            }
            dst = lua_fmt_apply_width(dst, text, len, &st);
        } else if (spec == 'c') {
            char ch = (char)va_arg(args, int);
            dst = lua_fmt_apply_width(dst, &ch, 1UL, &st);
        } else if (spec == 'f' || spec == 'F' || spec == 'e' || spec == 'E' || spec == 'g' || spec == 'G' ||
                   spec == 'a' || spec == 'A') {
            double value = (length == 4) ? (double)va_arg(args, long double) : va_arg(args, double);
            dst = lua_fmt_float(dst, value, spec, &st);
        } else {
            *dst++ = '%';
            *dst++ = spec;
        }
    }
    va_end(args);

    *dst = '\0';
    return (int)(dst - out);
}

int cleonos_lua_write_fd(int fd, const char *text, size_t len) {
    size_t done = 0U;

    if (text == (const char *)0) {
        return 0;
    }

    while (done < len) {
        u64 wrote = cleonos_sys_fd_write((u64)fd, text + done, (u64)(len - done));

        if (wrote == (u64)-1 || wrote == 0ULL) {
            return 0;
        }

        done += (size_t)wrote;
    }

    return 1;
}

int cleonos_lua_write_error(const char *fmt, const char *arg) {
    char buf[384];
    int len = snprintf(buf, (unsigned long)sizeof(buf), fmt, (arg != (const char *)0) ? arg : "");

    if (len < 0) {
        return 0;
    }

    if ((size_t)len >= sizeof(buf)) {
        len = (int)sizeof(buf) - 1;
    }

    return cleonos_lua_write_fd(2, buf, (size_t)len);
}

void (*signal(int sig, void (*handler)(int)))(int) {
    (void)sig;
    (void)handler;
    return SIG_DFL;
}

time_t time(time_t *out_time) {
    time_t value = (time_t)(cleonos_sys_time_ms() / 1000ULL);

    if (out_time != (time_t *)0) {
        *out_time = value;
    }

    return value;
}

clock_t clock(void) {
    return (clock_t)cleonos_sys_time_ms();
}

struct lconv *localeconv(void) {
    static struct lconv conv = {"."};
    return &conv;
}

char *setlocale(int category, const char *locale) {
    (void)category;
    (void)locale;
    return "C";
}

int strcoll(const char *left, const char *right) {
    return strcmp(left, right);
}

double floor(double value) {
    long long whole;

    if (value != value || value > 9000000000000000000.0 || value < -9000000000000000000.0) {
        return value;
    }

    whole = (long long)value;
    if ((double)whole > value) {
        whole--;
    }

    return (double)whole;
}

double ceil(double value) {
    long long whole;

    if (value != value || value > 9000000000000000000.0 || value < -9000000000000000000.0) {
        return value;
    }

    whole = (long long)value;
    if ((double)whole < value) {
        whole++;
    }

    return (double)whole;
}

double fmod(double left, double right) {
    double div;

    if (right == 0.0 || left != left || right != right) {
        return 0.0 / 0.0;
    }

    div = (double)((long long)(left / right));
    return left - (div * right);
}

double ldexp(double value, int exponent) {
    if (value == 0.0 || value != value) {
        return value;
    }

    while (exponent > 0) {
        value *= 2.0;
        exponent--;
    }
    while (exponent < 0) {
        value *= 0.5;
        exponent++;
    }

    return value;
}

double frexp(double value, int *out_exponent) {
    int exponent = 0;
    int negative = 0;

    if (out_exponent != (int *)0) {
        *out_exponent = 0;
    }

    if (value == 0.0 || value != value) {
        return value;
    }

    if (value < 0.0) {
        negative = 1;
        value = -value;
    }

    while (value >= 1.0) {
        value *= 0.5;
        exponent++;
    }
    while (value < 0.5) {
        value *= 2.0;
        exponent--;
    }

    if (out_exponent != (int *)0) {
        *out_exponent = exponent;
    }

    return (negative != 0) ? -value : value;
}

double sqrt(double value) {
    double guess;
    int i;

    if (value < 0.0) {
        return 0.0 / 0.0;
    }

    if (value == 0.0) {
        return 0.0;
    }

    guess = (value > 1.0) ? value : 1.0;
    for (i = 0; i < 32; i++) {
        guess = 0.5 * (guess + (value / guess));
    }

    return guess;
}

static double lua_exp_approx(double value) {
    double result = 1.0;
    double term = 1.0;
    int invert = 0;
    int halves = 0;
    int i;

    if (value < 0.0) {
        invert = 1;
        value = -value;
    }

    while (value > 1.0 && halves < 16) {
        value *= 0.5;
        halves++;
    }

    for (i = 1; i <= 32; i++) {
        term *= value / (double)i;
        result += term;
    }

    while (halves > 0) {
        result *= result;
        halves--;
    }

    return (invert != 0) ? 1.0 / result : result;
}

static double lua_ln_approx(double value) {
    double y;
    double y2;
    double term;
    double sum;
    int k = 0;
    int n;

    if (value <= 0.0) {
        return 0.0 / 0.0;
    }

    while (value > 2.0) {
        value *= 0.5;
        k++;
    }

    while (value < 0.5) {
        value *= 2.0;
        k--;
    }

    y = (value - 1.0) / (value + 1.0);
    y2 = y * y;
    term = y;
    sum = 0.0;

    for (n = 0; n < 36; n++) {
        sum += term / (double)(2 * n + 1);
        term *= y2;
    }

    return (2.0 * sum) + ((double)k * 0.69314718055994530942);
}

double pow(double base, double exponent) {
    long long whole = (long long)exponent;
    double result = 1.0;
    double factor = base;
    long long n;

    if (exponent == (double)whole && whole >= -63LL && whole <= 63LL) {
        n = whole;
        if (n < 0) {
            n = -n;
        }

        while (n > 0) {
            if ((n & 1LL) != 0LL) {
                result *= factor;
            }
            factor *= factor;
            n >>= 1LL;
        }

        return (whole < 0) ? 1.0 / result : result;
    }

    if (base <= 0.0) {
        return 0.0 / 0.0;
    }

    return lua_exp_approx(exponent * lua_ln_approx(base));
}
