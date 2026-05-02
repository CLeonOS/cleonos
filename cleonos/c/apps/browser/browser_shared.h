#ifndef CLEONOS_BROWSER_SHARED_H
#define CLEONOS_BROWSER_SHARED_H

#include "../cmd_runtime.h"

#define USH_BROWSER_SOURCE_MAX 256U
#define USH_BROWSER_HOST_MAX 128U
#define USH_BROWSER_PATH_MAX 256U
#define USH_BROWSER_HTML_MAX (256U * 1024U)
#define USH_BROWSER_TEXT_MAX (96U * 1024U)
#define USH_BROWSER_GUMBO_PARSE_MAX (96U * 1024U)
#define USH_BROWSER_GUMBO_MAX_ERRORS 0
#define USH_BROWSER_HTML_BUF_CAP (USH_BROWSER_HTML_MAX + 1U)
#define USH_BROWSER_TEXT_BUF_CAP (USH_BROWSER_TEXT_MAX + 1U)
#define USH_BROWSER_TITLE_MAX 128U
#define USH_BROWSER_DNS_PACKET_MAX 512U
#define USH_BROWSER_HTTP_RECV_CHUNK 2048U
#define USH_BROWSER_TCP_POLL_BUDGET 200000000ULL
#define USH_BROWSER_TCP_RECV_IDLE_LOOPS 160ULL
#define USH_BROWSER_FETCH_ERROR_MAX 160U

#define USH_BROWSER_LINK_MAX 96U
#define USH_BROWSER_LINK_TEXT_MAX 96U
#define USH_BROWSER_LINK_HREF_MAX 192U
#define USH_BROWSER_FORM_MAX 16U
#define USH_BROWSER_FORM_FIELD_MAX 48U
#define USH_BROWSER_FORM_NAME_MAX 48U
#define USH_BROWSER_FORM_VALUE_MAX 128U
#define USH_BROWSER_FORM_ACTION_MAX 192U
#define USH_BROWSER_FORM_BODY_MAX 8192U
#define USH_BROWSER_COOKIE_MAX 24U
#define USH_BROWSER_COOKIE_NAME_MAX 48U
#define USH_BROWSER_COOKIE_VALUE_MAX 128U
#define USH_BROWSER_COOKIE_DOMAIN_MAX 128U
#define USH_BROWSER_COOKIE_PATH_MAX 96U
#define USH_BROWSER_COOKIE_HEADER_MAX 512U
#define USH_BROWSER_TOKEN_MAX 192U
#define USH_BROWSER_HISTORY_MAX 16U
#define USH_BROWSER_INPUT_MAX 256U
#define USH_BROWSER_SEG_MAX 32U
#define USH_BROWSER_SEG_LEN_MAX 63U
#define USH_BROWSER_CSS_TEXT_MAX 4096U
#define USH_BROWSER_CSS_RULE_MAX 96U
#define USH_BROWSER_CSS_IDENT_MAX 48U
#define USH_BROWSER_OUTPUT_COLS 78U
#define USH_BROWSER_ANSI_RESET "\x1B[0m"
#define USH_BROWSER_ANSI_BLUE "\x1B[34m"
#define USH_BROWSER_ANSI_UNDERLINE "\x1B[4m"
#define USH_BROWSER_ANSI_BLUE_UNDERLINE "\x1B[34;4m"

#define USH_BROWSER_TEXT_TRANSFORM_NONE 0
#define USH_BROWSER_TEXT_TRANSFORM_UPPERCASE 1
#define USH_BROWSER_TEXT_TRANSFORM_LOWERCASE 2
#define USH_BROWSER_TEXT_TRANSFORM_CAPITALIZE 3

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

typedef struct ush_browser_url {
    char host[USH_BROWSER_HOST_MAX];
    char path[USH_BROWSER_PATH_MAX];
    u16 port;
    int tls;
} ush_browser_url;

typedef struct ush_browser_link {
    char text[USH_BROWSER_LINK_TEXT_MAX];
    char href[USH_BROWSER_LINK_HREF_MAX];
} ush_browser_link;

typedef struct ush_browser_form_field {
    u64 form_index;
    char name[USH_BROWSER_FORM_NAME_MAX];
    char value[USH_BROWSER_FORM_VALUE_MAX];
    char type[16];
    int successful;
} ush_browser_form_field;

typedef struct ush_browser_form {
    char method[8];
    char action[USH_BROWSER_FORM_ACTION_MAX];
    char label[USH_BROWSER_LINK_TEXT_MAX];
    u64 first_field;
    u64 field_count;
    int has_submit;
} ush_browser_form;

typedef struct ush_browser_cookie {
    char name[USH_BROWSER_COOKIE_NAME_MAX];
    char value[USH_BROWSER_COOKIE_VALUE_MAX];
    char domain[USH_BROWSER_COOKIE_DOMAIN_MAX];
    char path[USH_BROWSER_COOKIE_PATH_MAX];
    int secure;
} ush_browser_cookie;

typedef struct ush_browser_style {
    int fg_set;
    u32 fg_rgb;
    int bg_set;
    u32 bg_rgb;
    int bold;
    int italic;
    int dim;
    int underline;
    int strike;
    int display_none;
    int white_space_pre;
    int text_transform;
    int font_scale;
} ush_browser_style;

typedef struct ush_browser_style_delta {
    int set_fg;
    u32 fg_rgb;
    int set_bg;
    u32 bg_rgb;
    int set_bold;
    int bold;
    int set_italic;
    int italic;
    int set_dim;
    int dim;
    int set_underline;
    int underline;
    int set_strike;
    int strike;
    int set_display_none;
    int display_none;
    int set_white_space_pre;
    int white_space_pre;
    int set_text_transform;
    int text_transform;
    int set_font_scale;
    int font_scale;
} ush_browser_style_delta;

typedef struct ush_browser_css_rule {
    char tag[USH_BROWSER_CSS_IDENT_MAX];
    char class_name[USH_BROWSER_CSS_IDENT_MAX];
    char id_name[USH_BROWSER_CSS_IDENT_MAX];
    ush_browser_style_delta delta;
} ush_browser_css_rule;

extern char *ush_browser_http_raw_buf;

int ush_browser_ensure_buffers(void);
int ush_browser_is_http_url(const char *text);
int ush_browser_is_https_url(const char *text);
int ush_browser_parse_url(const char *url, ush_browser_url *out_url);
int ush_browser_fetch_http(const char *url_text, char *out_html, u64 out_html_cap, u64 *out_size);
int ush_browser_fetch_http_request(const char *url_text, const char *method, const char *body, u64 body_len,
                                   char *out_html, u64 out_html_cap, u64 *out_size);
const char *ush_browser_fetch_last_error(void);

#endif
