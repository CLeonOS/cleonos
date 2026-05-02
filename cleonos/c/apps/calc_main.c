#include "cmd_runtime.h"

#define CALC_EXPR_MAX 256U
#define CALC_ERR_MAX 96U
#define CALC_IDENT_MAX 32U
#define CALC_PLOT_W 78U
#define CALC_PLOT_H 24U
#define CALC_PIXEL_PLOT_MAX_W 800U
#define CALC_PIXEL_PLOT_MAX_H 480U
#define CALC_PIXEL_PLOT_MIN_W 240U
#define CALC_PIXEL_PLOT_MIN_H 160U
#define CALC_PROMPT "calc> "

typedef unsigned int calc_color;

typedef struct calc_parser {
    const char *text;
    unsigned long pos;
    double x_value;
    int failed;
    char error[CALC_ERR_MAX];
} calc_parser;

static double calc_absd(double value) {
    return (value < 0.0) ? -value : value;
}

static int calc_is_nan(double value) {
    return value != value;
}

static int calc_is_bad(double value) {
    double limit = 1.0e100;
    return calc_is_nan(value) || value > limit || value < -limit;
}

static double calc_nan(void) {
    volatile double zero = 0.0;
    return zero / zero;
}

static double calc_floor(double value) {
    long long whole;

    if (calc_is_nan(value) || value > 9000000000000000000.0 || value < -9000000000000000000.0) {
        return value;
    }

    whole = (long long)value;
    if ((double)whole > value) {
        whole--;
    }

    return (double)whole;
}

static double calc_ceil(double value) {
    long long whole;

    if (calc_is_nan(value) || value > 9000000000000000000.0 || value < -9000000000000000000.0) {
        return value;
    }

    whole = (long long)value;
    if ((double)whole < value) {
        whole++;
    }

    return (double)whole;
}

static double calc_round(double value) {
    if (value >= 0.0) {
        return calc_floor(value + 0.5);
    }

    return calc_ceil(value - 0.5);
}

static double calc_mod(double left, double right) {
    double div;
    double ratio;

    if (calc_is_nan(left) || calc_is_nan(right) || right == 0.0) {
        return calc_nan();
    }

    ratio = left / right;
    if (ratio > 9000000000000000000.0 || ratio < -9000000000000000000.0) {
        return calc_nan();
    }

    div = (double)((long long)ratio);
    return left - (div * right);
}

static double calc_sqrt(double value) {
    double guess;
    int i;

    if (value < 0.0) {
        return calc_nan();
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

static double calc_wrap_pi(double value) {
    const double two_pi = 6.28318530717958647692;
    long long turns;

    if (calc_is_nan(value) || value > 1000000000000.0 || value < -1000000000000.0) {
        return calc_nan();
    }

    turns = (long long)(value / two_pi);
    value -= (double)turns * two_pi;

    while (value > 3.14159265358979323846) {
        value -= two_pi;
    }

    while (value < -3.14159265358979323846) {
        value += two_pi;
    }

    return value;
}

static double calc_sin(double value) {
    double x = calc_wrap_pi(value);
    double x2 = x * x;
    double term = x;
    double sum = x;
    int n;

    for (n = 1; n <= 9; n++) {
        double denom = (double)((2 * n) * (2 * n + 1));
        term = -(term * x2) / denom;
        sum += term;
    }

    return sum;
}

static double calc_cos(double value) {
    double x = calc_wrap_pi(value);
    double x2 = x * x;
    double term = 1.0;
    double sum = 1.0;
    int n;

    for (n = 1; n <= 9; n++) {
        double denom = (double)((2 * n - 1) * (2 * n));
        term = -(term * x2) / denom;
        sum += term;
    }

    return sum;
}

static double calc_tan(double value) {
    double c = calc_cos(value);

    if (calc_absd(c) < 1.0e-12) {
        return calc_nan();
    }

    return calc_sin(value) / c;
}

static double calc_atan(double value) {
    double x = value;
    double sign = 1.0;
    double term;
    double sum;
    double x2;
    int n;

    if (x < 0.0) {
        sign = -1.0;
        x = -x;
    }

    if (x > 1.0) {
        return sign * (1.57079632679489661923 - calc_atan(1.0 / x));
    }

    term = x;
    sum = x;
    x2 = x * x;
    for (n = 1; n < 28; n++) {
        term *= -x2;
        sum += term / (double)(2 * n + 1);
    }

    return sign * sum;
}

static double calc_asin(double value) {
    if (value < -1.0 || value > 1.0) {
        return calc_nan();
    }

    if (value == 1.0) {
        return 1.57079632679489661923;
    }

    if (value == -1.0) {
        return -1.57079632679489661923;
    }

    return calc_atan(value / calc_sqrt(1.0 - (value * value)));
}

static double calc_acos(double value) {
    if (value < -1.0 || value > 1.0) {
        return calc_nan();
    }

    return 1.57079632679489661923 - calc_asin(value);
}

static double calc_exp(double value) {
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

    if (invert != 0) {
        if (result == 0.0) {
            return calc_nan();
        }
        return 1.0 / result;
    }

    return result;
}

static double calc_ln(double value) {
    double y;
    double y2;
    double term;
    double sum;
    int k = 0;
    int n;

    if (value <= 0.0) {
        return calc_nan();
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

static double calc_pow(double base, double exponent) {
    long long whole = 0LL;
    double result = 1.0;
    double factor = base;
    long long n;

    if (calc_is_nan(base) || calc_is_nan(exponent)) {
        return calc_nan();
    }

    if (exponent >= -63.0 && exponent <= 63.0) {
        whole = (long long)exponent;
    }

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

        if (whole < 0) {
            if (result == 0.0) {
                return calc_nan();
            }
            return 1.0 / result;
        }

        return result;
    }

    if (base <= 0.0) {
        return calc_nan();
    }

    return calc_exp(exponent * calc_ln(base));
}

static int calc_streq(const char *left, const char *right) {
    if (left == (const char *)0 || right == (const char *)0) {
        return 0;
    }

    while (*left != '\0' && *right != '\0') {
        if (*left != *right) {
            return 0;
        }
        left++;
        right++;
    }

    return *left == '\0' && *right == '\0';
}

static void calc_set_error(calc_parser *parser, const char *message) {
    unsigned long i = 0;

    if (parser == (calc_parser *)0 || parser->failed != 0) {
        return;
    }

    parser->failed = 1;
    if (message == (const char *)0) {
        message = "parse error";
    }

    while (message[i] != '\0' && i + 1U < CALC_ERR_MAX) {
        parser->error[i] = message[i];
        i++;
    }
    parser->error[i] = '\0';
}

static void calc_skip_ws(calc_parser *parser) {
    while (parser->text[parser->pos] == ' ' || parser->text[parser->pos] == '\t' ||
           parser->text[parser->pos] == '\r' || parser->text[parser->pos] == '\n') {
        parser->pos++;
    }
}

static int calc_match(calc_parser *parser, char ch) {
    calc_skip_ws(parser);
    if (parser->text[parser->pos] == ch) {
        parser->pos++;
        return 1;
    }

    return 0;
}

static double calc_parse_expr(calc_parser *parser);

static double calc_call_func(calc_parser *parser, const char *name, double *args, int argc) {
    if (calc_streq(name, "sin") != 0 && argc == 1) {
        return calc_sin(args[0]);
    }
    if (calc_streq(name, "cos") != 0 && argc == 1) {
        return calc_cos(args[0]);
    }
    if (calc_streq(name, "tan") != 0 && argc == 1) {
        return calc_tan(args[0]);
    }
    if (calc_streq(name, "asin") != 0 && argc == 1) {
        return calc_asin(args[0]);
    }
    if (calc_streq(name, "acos") != 0 && argc == 1) {
        return calc_acos(args[0]);
    }
    if (calc_streq(name, "atan") != 0 && argc == 1) {
        return calc_atan(args[0]);
    }
    if ((calc_streq(name, "sqrt") != 0 || calc_streq(name, "sqr") != 0) && argc == 1) {
        return calc_sqrt(args[0]);
    }
    if (calc_streq(name, "abs") != 0 && argc == 1) {
        return calc_absd(args[0]);
    }
    if (calc_streq(name, "exp") != 0 && argc == 1) {
        return calc_exp(args[0]);
    }
    if ((calc_streq(name, "ln") != 0 || calc_streq(name, "log") != 0) && argc == 1) {
        return calc_ln(args[0]);
    }
    if (calc_streq(name, "log10") != 0 && argc == 1) {
        return calc_ln(args[0]) / 2.30258509299404568402;
    }
    if (calc_streq(name, "floor") != 0 && argc == 1) {
        return calc_floor(args[0]);
    }
    if (calc_streq(name, "ceil") != 0 && argc == 1) {
        return calc_ceil(args[0]);
    }
    if (calc_streq(name, "round") != 0 && argc == 1) {
        return calc_round(args[0]);
    }
    if (calc_streq(name, "sign") != 0 && argc == 1) {
        return (args[0] > 0.0) ? 1.0 : ((args[0] < 0.0) ? -1.0 : 0.0);
    }
    if (calc_streq(name, "min") != 0 && argc == 2) {
        return (args[0] < args[1]) ? args[0] : args[1];
    }
    if (calc_streq(name, "max") != 0 && argc == 2) {
        return (args[0] > args[1]) ? args[0] : args[1];
    }
    if (calc_streq(name, "pow") != 0 && argc == 2) {
        return calc_pow(args[0], args[1]);
    }
    if (calc_streq(name, "clamp") != 0 && argc == 3) {
        if (args[0] < args[1]) {
            return args[1];
        }
        if (args[0] > args[2]) {
            return args[2];
        }
        return args[0];
    }

    calc_set_error(parser, "unknown function or wrong argument count");
    return calc_nan();
}

static double calc_parse_primary(calc_parser *parser) {
    char ch;
    calc_skip_ws(parser);
    ch = parser->text[parser->pos];

    if (ch == '(') {
        double value;
        parser->pos++;
        value = calc_parse_expr(parser);
        if (calc_match(parser, ')') == 0) {
            calc_set_error(parser, "missing ')'");
        }
        return value;
    }

    if ((ch >= '0' && ch <= '9') || ch == '.') {
        char *end = (char *)0;
        double value = strtod(&parser->text[parser->pos], &end);

        if (end == &parser->text[parser->pos]) {
            calc_set_error(parser, "bad number");
            return calc_nan();
        }

        parser->pos = (unsigned long)(end - parser->text);
        return value;
    }

    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_') {
        char name[CALC_IDENT_MAX];
        unsigned long len = 0;

        while (((parser->text[parser->pos] >= 'a' && parser->text[parser->pos] <= 'z') ||
                (parser->text[parser->pos] >= 'A' && parser->text[parser->pos] <= 'Z') ||
                (parser->text[parser->pos] >= '0' && parser->text[parser->pos] <= '9') ||
                parser->text[parser->pos] == '_') &&
               len + 1U < CALC_IDENT_MAX) {
            char c = parser->text[parser->pos++];
            if (c >= 'A' && c <= 'Z') {
                c = (char)(c - 'A' + 'a');
            }
            name[len++] = c;
        }
        name[len] = '\0';

        if (calc_streq(name, "pi") != 0) {
            return 3.14159265358979323846;
        }
        if (calc_streq(name, "tau") != 0) {
            return 6.28318530717958647692;
        }
        if (calc_streq(name, "e") != 0) {
            return 2.71828182845904523536;
        }
        if (calc_streq(name, "x") != 0) {
            return parser->x_value;
        }

        if (calc_match(parser, '(') != 0) {
            double args[3];
            int argc = 0;

            calc_skip_ws(parser);
            if (parser->text[parser->pos] != ')') {
                while (argc < 3) {
                    args[argc++] = calc_parse_expr(parser);
                    if (parser->failed != 0) {
                        return calc_nan();
                    }
                    if (calc_match(parser, ',') == 0) {
                        break;
                    }
                }
            }

            if (calc_match(parser, ')') == 0) {
                calc_set_error(parser, "missing ')' after function");
                return calc_nan();
            }

            return calc_call_func(parser, name, args, argc);
        }

        calc_set_error(parser, "unknown identifier");
        return calc_nan();
    }

    calc_set_error(parser, "expected number, name, or '('");
    return calc_nan();
}

static double calc_parse_unary(calc_parser *parser) {
    if (calc_match(parser, '+') != 0) {
        return calc_parse_unary(parser);
    }

    if (calc_match(parser, '-') != 0) {
        return -calc_parse_unary(parser);
    }

    return calc_parse_primary(parser);
}

static double calc_parse_power(calc_parser *parser) {
    double left = calc_parse_unary(parser);

    if (calc_match(parser, '^') != 0) {
        double right = calc_parse_power(parser);
        left = calc_pow(left, right);
    }

    return left;
}

static double calc_parse_mul(calc_parser *parser) {
    double value = calc_parse_power(parser);

    while (parser->failed == 0) {
        if (calc_match(parser, '*') != 0) {
            value *= calc_parse_power(parser);
        } else if (calc_match(parser, '/') != 0) {
            double right = calc_parse_power(parser);
            if (right == 0.0) {
                calc_set_error(parser, "division by zero");
                return calc_nan();
            }
            value /= right;
        } else if (calc_match(parser, '%') != 0) {
            double right = calc_parse_power(parser);
            value = calc_mod(value, right);
        } else {
            break;
        }
    }

    return value;
}

static double calc_parse_expr(calc_parser *parser) {
    double value = calc_parse_mul(parser);

    while (parser->failed == 0) {
        if (calc_match(parser, '+') != 0) {
            value += calc_parse_mul(parser);
        } else if (calc_match(parser, '-') != 0) {
            value -= calc_parse_mul(parser);
        } else {
            break;
        }
    }

    return value;
}

static int calc_eval(const char *expr, double x_value, double *out_value, char *err, unsigned long err_size) {
    calc_parser parser;
    double value;
    unsigned long i;

    parser.text = expr;
    parser.pos = 0;
    parser.x_value = x_value;
    parser.failed = 0;
    parser.error[0] = '\0';

    value = calc_parse_expr(&parser);
    calc_skip_ws(&parser);

    if (parser.failed == 0 && parser.text[parser.pos] != '\0') {
        calc_set_error(&parser, "trailing characters");
    }

    if (parser.failed != 0 || calc_is_bad(value)) {
        if (err != (char *)0 && err_size > 0U) {
            const char *message = (parser.error[0] != '\0') ? parser.error : "invalid result";
            i = 0;
            while (message[i] != '\0' && i + 1U < err_size) {
                err[i] = message[i];
                i++;
            }
            err[i] = '\0';
        }
        return 0;
    }

    if (out_value != (double *)0) {
        *out_value = value;
    }

    if (err != (char *)0 && err_size > 0U) {
        err[0] = '\0';
    }

    return 1;
}

static int calc_parse_full_double(const char *text, double *out_value) {
    char *end = (char *)0;
    double value;

    if (text == (const char *)0 || text[0] == '\0') {
        return 0;
    }

    value = strtod(text, &end);
    if (end == text) {
        return 0;
    }

    while (*end == ' ' || *end == '\t') {
        end++;
    }

    if (*end != '\0') {
        return 0;
    }

    if (out_value != (double *)0) {
        *out_value = value;
    }

    return 1;
}

static int calc_try_take_plot_ranges(char *text, double *xmin, double *xmax, double *ymin, double *ymax) {
    char work[CALC_EXPR_MAX];
    char *tokens[4];
    char *end;
    unsigned long n = 0;
    int i;

    if (text == (char *)0) {
        return 0;
    }

    while (text[n] != '\0' && n + 1U < sizeof(work)) {
        work[n] = text[n];
        n++;
    }
    work[n] = '\0';

    end = work + strlen(work);
    while (end > work && (end[-1] == ' ' || end[-1] == '\t')) {
        end--;
    }
    *end = '\0';

    for (i = 3; i >= 0; i--) {
        char *token_end = end;

        while (end > work && end[-1] != ' ' && end[-1] != '\t') {
            end--;
        }

        if (end == token_end) {
            return 0;
        }

        tokens[i] = end;
        *token_end = '\0';

        while (end > work && (end[-1] == ' ' || end[-1] == '\t')) {
            end--;
        }

        if (i > 0 && end == work) {
            return 0;
        }
    }

    if (calc_parse_full_double(tokens[0], xmin) == 0 ||
        calc_parse_full_double(tokens[1], xmax) == 0 ||
        calc_parse_full_double(tokens[2], ymin) == 0 ||
        calc_parse_full_double(tokens[3], ymax) == 0) {
        return 0;
    }

    while (end > work && (end[-1] == ' ' || end[-1] == '\t')) {
        end--;
    }
    *end = '\0';
    if (work[0] == '\0') {
        return 0;
    }

    n = 0;
    while (work[n] != '\0') {
        text[n] = work[n];
        n++;
    }
    text[n] = '\0';
    return 1;
}

static void calc_print_u64(unsigned long long value) {
    char tmp[32];
    unsigned long len = 0;

    if (value == 0ULL) {
        (void)putchar('0');
        return;
    }

    while (value > 0ULL && len < sizeof(tmp)) {
        tmp[len++] = (char)('0' + (value % 10ULL));
        value /= 10ULL;
    }

    while (len > 0U) {
        len--;
        (void)putchar(tmp[len]);
    }
}

static void calc_print_double(double value) {
    unsigned long long whole;
    unsigned long long frac;
    int digits = 6;
    int i;

    if (calc_is_bad(value)) {
        (void)fputs("nan", 1);
        return;
    }

    if (value < 0.0) {
        (void)putchar('-');
        value = -value;
    }

    if (value > 999999999999999.0) {
        (void)fputs("too-large", 1);
        return;
    }

    whole = (unsigned long long)value;
    frac = (unsigned long long)((value - (double)whole) * 1000000.0 + 0.5);
    if (frac >= 1000000ULL) {
        whole++;
        frac -= 1000000ULL;
    }

    calc_print_u64(whole);
    (void)putchar('.');

    for (i = 100000; i >= 1; i /= 10) {
        (void)putchar((int)('0' + (frac / (unsigned long long)i) % 10ULL));
        digits--;
        if (digits <= 0) {
            break;
        }
    }
}

static int calc_join_args(int argc, char **argv, int start, char *out, unsigned long out_size) {
    unsigned long pos = 0;
    int i;

    if (out == (char *)0 || out_size == 0U) {
        return 0;
    }

    out[0] = '\0';
    for (i = start; i < argc; i++) {
        const char *arg = argv[i];
        unsigned long j = 0;

        if (arg == (const char *)0) {
            continue;
        }

        if (i > start) {
            if (pos + 1U >= out_size) {
                return 0;
            }
            out[pos++] = ' ';
        }

        while (arg[j] != '\0') {
            if (pos + 1U >= out_size) {
                return 0;
            }
            out[pos++] = arg[j++];
        }
        out[pos] = '\0';
    }

    return pos > 0U;
}

static void calc_help(void) {
    (void)puts("usage:");
    (void)puts("  calc \"1+2*3\"");
    (void)puts("  calc \"sin(pi/2)+sqrt(9)\"");
    (void)puts("  calc plot \"sin(x)\" [-10 10 -2 2]");
    (void)puts("");
    (void)puts("operators: + - * / % ^  parentheses supported");
    (void)puts("constants: pi e tau  variable: x");
    (void)puts("functions:");
    (void)puts("  sin cos tan asin acos atan sqrt abs exp ln log log10");
    (void)puts("  floor ceil round sign min max pow clamp");
    (void)puts("plot: framebuffer pixel plot first, TTY ASCII fallback otherwise");
    (void)puts("");
    (void)puts("interactive commands:");
    (void)puts("  plot <expr> [xmin xmax ymin ymax]");
    (void)puts("  help");
    (void)puts("  quit");
}

static int calc_plot_pixel_to_x(double x_value, double xmin, double xmax, unsigned int width) {
    return (int)(((x_value - xmin) / (xmax - xmin)) * (double)(width - 1U) + 0.5);
}

static int calc_plot_pixel_to_y(double y_value, double ymin, double ymax, unsigned int height) {
    return (int)(((ymax - y_value) / (ymax - ymin)) * (double)(height - 1U) + 0.5);
}

static void calc_plot_put_pixel(calc_color *pixels, unsigned int width, unsigned int height, int x, int y, calc_color color) {
    if (pixels == (calc_color *)0 || x < 0 || y < 0 || x >= (int)width || y >= (int)height) {
        return;
    }

    pixels[(unsigned long)y * (unsigned long)width + (unsigned long)x] = color;
}

static void calc_plot_draw_line(calc_color *pixels,
                                unsigned int width,
                                unsigned int height,
                                int x0,
                                int y0,
                                int x1,
                                int y1,
                                calc_color color) {
    int dx;
    int dy;
    int sx;
    int sy;
    int err;

    if (pixels == (calc_color *)0) {
        return;
    }

    dx = (x0 < x1) ? (x1 - x0) : (x0 - x1);
    dy = (y0 < y1) ? (y1 - y0) : (y0 - y1);
    sx = (x0 < x1) ? 1 : -1;
    sy = (y0 < y1) ? 1 : -1;
    err = dx - dy;

    for (;;) {
        int e2;

        calc_plot_put_pixel(pixels, width, height, x0, y0, color);
        calc_plot_put_pixel(pixels, width, height, x0 + 1, y0, color);
        calc_plot_put_pixel(pixels, width, height, x0, y0 + 1, color);

        if (x0 == x1 && y0 == y1) {
            break;
        }

        e2 = err * 2;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void calc_plot_fill(calc_color *pixels, unsigned int width, unsigned int height, calc_color color) {
    unsigned long count = (unsigned long)width * (unsigned long)height;
    unsigned long i;

    if (pixels == (calc_color *)0) {
        return;
    }

    for (i = 0; i < count; i++) {
        pixels[i] = color;
    }
}

static void calc_plot_draw_grid(calc_color *pixels,
                                unsigned int width,
                                unsigned int height,
                                double xmin,
                                double xmax,
                                double ymin,
                                double ymax) {
    unsigned int i;
    calc_color grid_color = 0x00262D36U;
    calc_color axis_color = 0x00AAB4C0U;

    for (i = 1; i < 10U; i++) {
        unsigned int x = (width * i) / 10U;
        unsigned int y = (height * i) / 10U;
        unsigned int p;

        for (p = 0; p < height; p++) {
            calc_plot_put_pixel(pixels, width, height, (int)x, (int)p, grid_color);
        }
        for (p = 0; p < width; p++) {
            calc_plot_put_pixel(pixels, width, height, (int)p, (int)y, grid_color);
        }
    }

    if (xmin <= 0.0 && xmax >= 0.0) {
        int axis_x = calc_plot_pixel_to_x(0.0, xmin, xmax, width);
        unsigned int y;

        for (y = 0; y < height; y++) {
            calc_plot_put_pixel(pixels, width, height, axis_x, (int)y, axis_color);
            calc_plot_put_pixel(pixels, width, height, axis_x + 1, (int)y, axis_color);
        }
    }

    if (ymin <= 0.0 && ymax >= 0.0) {
        int axis_y = calc_plot_pixel_to_y(0.0, ymin, ymax, height);
        unsigned int x;

        for (x = 0; x < width; x++) {
            calc_plot_put_pixel(pixels, width, height, (int)x, axis_y, axis_color);
            calc_plot_put_pixel(pixels, width, height, (int)x, axis_y + 1, axis_color);
        }
    }
}

static int calc_plot_framebuffer(const char *expr, double xmin, double xmax, double ymin, double ymax) {
    cleonos_fb_info fb;
    cleonos_fb_blit_req req;
    calc_color *pixels;
    unsigned int width;
    unsigned int height;
    unsigned int x;
    int have_prev = 0;
    int prev_x = 0;
    int prev_y = 0;
    char err[CALC_ERR_MAX];
    unsigned long count;

    if (expr == (const char *)0 || xmax <= xmin || ymax <= ymin) {
        return 0;
    }

    if (cleonos_sys_fb_info(&fb) == 0ULL || fb.width == 0ULL || fb.height == 0ULL || fb.bpp != 32ULL) {
        return 0;
    }

    width = (fb.width > (u64)CALC_PIXEL_PLOT_MAX_W) ? CALC_PIXEL_PLOT_MAX_W : (unsigned int)fb.width;
    height = (fb.height > (u64)CALC_PIXEL_PLOT_MAX_H) ? CALC_PIXEL_PLOT_MAX_H : (unsigned int)fb.height;

    if (fb.width > 64ULL && width > (unsigned int)(fb.width - 32ULL)) {
        width = (unsigned int)(fb.width - 32ULL);
    }
    if (fb.height > 128ULL && height > (unsigned int)(fb.height - 96ULL)) {
        height = (unsigned int)(fb.height - 96ULL);
    }

    if (width < CALC_PIXEL_PLOT_MIN_W || height < CALC_PIXEL_PLOT_MIN_H) {
        return 0;
    }

    count = (unsigned long)width * (unsigned long)height;
    if (count == 0UL || count > ((unsigned long)CALC_PIXEL_PLOT_MAX_W * (unsigned long)CALC_PIXEL_PLOT_MAX_H)) {
        return 0;
    }

    pixels = (calc_color *)malloc((size_t)(count * sizeof(calc_color)));
    if (pixels == (calc_color *)0) {
        return 0;
    }

    calc_plot_fill(pixels, width, height, 0x000B1018U);
    calc_plot_draw_grid(pixels, width, height, xmin, xmax, ymin, ymax);

    for (x = 0; x < width; x++) {
        double xv = xmin + ((xmax - xmin) * (double)x / (double)(width - 1U));
        double yv = 0.0;

        if (calc_eval(expr, xv, &yv, err, (unsigned long)sizeof(err)) != 0 && yv >= ymin && yv <= ymax) {
            int py = calc_plot_pixel_to_y(yv, ymin, ymax, height);

            if (have_prev != 0 && py >= -32 && py <= (int)height + 32 && prev_y >= -32 && prev_y <= (int)height + 32 &&
                ((py > prev_y) ? (py - prev_y) : (prev_y - py)) <= (int)(height / 2U)) {
                calc_plot_draw_line(pixels, width, height, prev_x, prev_y, (int)x, py, 0x0039FF88U);
            } else {
                calc_plot_put_pixel(pixels, width, height, (int)x, py, 0x0039FF88U);
                calc_plot_put_pixel(pixels, width, height, (int)x + 1, py, 0x0039FF88U);
                calc_plot_put_pixel(pixels, width, height, (int)x, py + 1, 0x0039FF88U);
            }

            prev_x = (int)x;
            prev_y = py;
            have_prev = 1;
        } else {
            have_prev = 0;
        }
    }

    req.pixels_ptr = (u64)(usize)pixels;
    req.src_width = (u64)width;
    req.src_height = (u64)height;
    req.src_pitch_bytes = (u64)width * 4ULL;
    req.dst_x = (fb.width > (u64)width) ? ((fb.width - (u64)width) / 2ULL) : 0ULL;
    req.dst_y = (fb.height > (u64)height) ? ((fb.height - (u64)height) / 2ULL) : 0ULL;
    req.scale = 1ULL;

    if (cleonos_sys_fb_blit(&req) == 0ULL) {
        free(pixels);
        return 0;
    }

    (void)fputs("pixel plot: ", 1);
    calc_print_u64((unsigned long long)width);
    (void)putchar('x');
    calc_print_u64((unsigned long long)height);
    (void)fputs(" framebuffer image drawn\n", 1);

    free(pixels);
    return 1;
}

static void calc_plot_ascii(const char *expr, double xmin, double xmax, double ymin, double ymax) {
    char grid[CALC_PLOT_H][CALC_PLOT_W + 1U];
    char err[CALC_ERR_MAX];
    unsigned int x;
    unsigned int y;
    int axis_x = -1;
    int axis_y = -1;

    if (xmax <= xmin || ymax <= ymin) {
        (void)puts("calc: invalid plot range");
        return;
    }

    for (y = 0; y < CALC_PLOT_H; y++) {
        for (x = 0; x < CALC_PLOT_W; x++) {
            grid[y][x] = ' ';
        }
        grid[y][CALC_PLOT_W] = '\0';
    }

    if (xmin <= 0.0 && xmax >= 0.0) {
        axis_x = (int)(((0.0 - xmin) / (xmax - xmin)) * (double)(CALC_PLOT_W - 1U) + 0.5);
    }

    if (ymin <= 0.0 && ymax >= 0.0) {
        axis_y = (int)(((ymax - 0.0) / (ymax - ymin)) * (double)(CALC_PLOT_H - 1U) + 0.5);
    }

    if (axis_y >= 0 && axis_y < (int)CALC_PLOT_H) {
        for (x = 0; x < CALC_PLOT_W; x++) {
            grid[axis_y][x] = '-';
        }
    }

    if (axis_x >= 0 && axis_x < (int)CALC_PLOT_W) {
        for (y = 0; y < CALC_PLOT_H; y++) {
            grid[y][axis_x] = '|';
        }
    }

    if (axis_x >= 0 && axis_x < (int)CALC_PLOT_W && axis_y >= 0 && axis_y < (int)CALC_PLOT_H) {
        grid[axis_y][axis_x] = '+';
    }

    for (x = 0; x < CALC_PLOT_W; x++) {
        double xv = xmin + ((xmax - xmin) * (double)x / (double)(CALC_PLOT_W - 1U));
        double yv = 0.0;

        if (calc_eval(expr, xv, &yv, err, (unsigned long)sizeof(err)) != 0 && yv >= ymin && yv <= ymax) {
            int row = (int)(((ymax - yv) / (ymax - ymin)) * (double)(CALC_PLOT_H - 1U) + 0.5);
            if (row >= 0 && row < (int)CALC_PLOT_H) {
                grid[row][x] = '*';
            }
        }
    }

    (void)fputs("plot: y=", 1);
    calc_print_double(ymax);
    (void)fputs(" .. ", 1);
    calc_print_double(ymin);
    (void)fputs("  x=", 1);
    calc_print_double(xmin);
    (void)fputs(" .. ", 1);
    calc_print_double(xmax);
    (void)putchar('\n');

    for (y = 0; y < CALC_PLOT_H; y++) {
        (void)puts(grid[y]);
    }
}

static void calc_plot(const char *expr, double xmin, double xmax, double ymin, double ymax) {
    if (xmax <= xmin || ymax <= ymin) {
        (void)puts("calc: invalid plot range");
        return;
    }

    if (calc_plot_framebuffer(expr, xmin, xmax, ymin, ymax) != 0) {
        return;
    }

    calc_plot_ascii(expr, xmin, xmax, ymin, ymax);
}

static int calc_parse_plot_args(int argc, char **argv) {
    char expr[CALC_EXPR_MAX];
    double xmin = -10.0;
    double xmax = 10.0;
    double ymin = -5.0;
    double ymax = 5.0;

    if (argc < 3 || argv == (char **)0 || argv[2] == (char *)0) {
        (void)puts("calc: usage calc plot \"expr\" [xmin xmax ymin ymax]");
        return 1;
    }

    if (calc_join_args(argc, argv, 2, expr, (unsigned long)sizeof(expr)) == 0) {
        (void)puts("calc: expression too long");
        return 1;
    }

    (void)calc_try_take_plot_ranges(expr, &xmin, &xmax, &ymin, &ymax);
    calc_plot(expr, xmin, xmax, ymin, ymax);
    return 0;
}

static void calc_render_input_line(const char *prompt, const char *line, unsigned long len, unsigned long *io_rendered_len) {
    unsigned long rendered_len = 0;
    unsigned long i;

    if (prompt == (const char *)0) {
        prompt = "";
    }

    if (line == (const char *)0) {
        line = "";
    }

    if (io_rendered_len != (unsigned long *)0) {
        rendered_len = *io_rendered_len;
    }

    (void)putchar('\r');
    (void)fputs(prompt, 1);

    for (i = 0; i < len; i++) {
        (void)putchar(line[i]);
    }

    (void)putchar('_');

    if (rendered_len > len) {
        for (i = len; i < rendered_len; i++) {
            (void)putchar(' ');
        }
    }

    (void)putchar('\r');
    (void)fputs(prompt, 1);

    for (i = 0; i < len; i++) {
        (void)putchar(line[i]);
    }

    if (io_rendered_len != (unsigned long *)0) {
        *io_rendered_len = len;
    }
}

static void calc_finish_input_line(const char *prompt, const char *line, unsigned long len, unsigned long rendered_len) {
    unsigned long i;

    if (prompt == (const char *)0) {
        prompt = "";
    }

    if (line == (const char *)0) {
        line = "";
    }

    (void)putchar('\r');
    (void)fputs(prompt, 1);
    for (i = 0; i < len; i++) {
        (void)putchar(line[i]);
    }

    if (rendered_len >= len) {
        for (i = len; i <= rendered_len; i++) {
            (void)putchar(' ');
        }
        (void)putchar('\r');
        (void)fputs(prompt, 1);
        for (i = 0; i < len; i++) {
            (void)putchar(line[i]);
        }
    }

    (void)putchar('\n');
}

static int calc_read_line(const char *prompt, char *out, unsigned long out_size) {
    unsigned long pos = 0;
    unsigned long rendered_len = 0;

    if (out == (char *)0 || out_size == 0U) {
        return 0;
    }

    out[0] = '\0';
    calc_render_input_line(prompt, out, pos, &rendered_len);

    while (pos + 1U < out_size) {
        int ch = getchar();

        if (ch == '\r') {
            continue;
        }

        if (ch == '\n') {
            calc_finish_input_line(prompt, out, pos, rendered_len);
            break;
        }

        if (ch == 8 || ch == 127) {
            if (pos > 0U) {
                pos--;
                out[pos] = '\0';
                calc_render_input_line(prompt, out, pos, &rendered_len);
            }
            continue;
        }

        if (ch < 0) {
            break;
        }

        if (ch < 32 || ch > 126) {
            continue;
        }

        out[pos++] = (char)ch;
        out[pos] = '\0';
        calc_render_input_line(prompt, out, pos, &rendered_len);
    }

    out[pos] = '\0';
    return pos > 0U;
}

static int calc_line_starts_with(const char *line, const char *prefix) {
    while (*prefix != '\0') {
        if (*line != *prefix) {
            return 0;
        }
        line++;
        prefix++;
    }

    return 1;
}

static int calc_interactive_plot(const char *line) {
    char expr[CALC_EXPR_MAX];
    const char *p = line + 4;
    char *end;
    unsigned long len = 0;
    double xmin = -10.0;
    double xmax = 10.0;
    double ymin = -5.0;
    double ymax = 5.0;

    while (*p == ' ' || *p == '\t') {
        p++;
    }

    while (*p != '\0' && len + 1U < sizeof(expr)) {
        if (*p == ';') {
            break;
        }
        expr[len++] = *p++;
    }
    expr[len] = '\0';

    if (*p == ';') {
        p++;
        xmin = strtod(p, &end);
        p = end;
        xmax = strtod(p, &end);
        p = end;
        ymin = strtod(p, &end);
        p = end;
        ymax = strtod(p, (char **)0);
    } else {
        (void)calc_try_take_plot_ranges(expr, &xmin, &xmax, &ymin, &ymax);
    }

    if (expr[0] == '\0') {
        (void)puts("calc: plot usage: plot expr ; xmin xmax ymin ymax");
        return 1;
    }

    calc_plot(expr, xmin, xmax, ymin, ymax);
    return 0;
}

static int calc_interactive(void) {
    char line[CALC_EXPR_MAX];

    (void)puts("CLeonOS universal calculator");
    (void)puts("type 'help' for usage, 'quit' to exit");

    for (;;) {
        double value = 0.0;
        char err[CALC_ERR_MAX];

        if (calc_read_line(CALC_PROMPT, line, (unsigned long)sizeof(line)) == 0) {
            continue;
        }

        if (calc_streq(line, "quit") != 0 || calc_streq(line, "exit") != 0) {
            break;
        }

        if (calc_streq(line, "help") != 0 || calc_streq(line, "?") != 0) {
            calc_help();
            continue;
        }

        if (calc_line_starts_with(line, "plot") != 0) {
            (void)calc_interactive_plot(line);
            continue;
        }

        if (calc_eval(line, 0.0, &value, err, (unsigned long)sizeof(err)) == 0) {
            (void)fputs("error: ", 1);
            (void)puts(err);
            continue;
        }

        calc_print_double(value);
        (void)putchar('\n');
    }

    return 0;
}

int cleonos_app_main(int argc, char **argv, char **envp) {
    char expr[CALC_EXPR_MAX];
    double value = 0.0;
    char err[CALC_ERR_MAX];

    (void)envp;

    if (argc <= 1 || argv == (char **)0) {
        return calc_interactive();
    }

    if (argv[1] != (char *)0 &&
        (calc_streq(argv[1], "--help") != 0 || calc_streq(argv[1], "-h") != 0 ||
         calc_streq(argv[1], "help") != 0)) {
        calc_help();
        return 0;
    }

    if (argv[1] != (char *)0 && calc_streq(argv[1], "plot") != 0) {
        return calc_parse_plot_args(argc, argv);
    }

    if (calc_join_args(argc, argv, 1, expr, (unsigned long)sizeof(expr)) == 0) {
        (void)puts("calc: expression too long");
        return 1;
    }

    if (calc_eval(expr, 0.0, &value, err, (unsigned long)sizeof(err)) == 0) {
        (void)fputs("calc: ", 1);
        (void)puts(err);
        return 1;
    }

    calc_print_double(value);
    (void)putchar('\n');
    return 0;
}
