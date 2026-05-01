#ifndef CLEONOS_BROWSER_INTERNAL_H
#define CLEONOS_BROWSER_INTERNAL_H

#include "browser_shared.h"

typedef struct GumboInternalNode GumboNode;
typedef struct cleonos_tls_conn cleonos_tls_conn;

extern char *ush_browser_html_buf;
extern char *ush_browser_text_buf;
extern char ush_browser_title[USH_BROWSER_TITLE_MAX];
extern ush_browser_link ush_browser_links[USH_BROWSER_LINK_MAX];
extern u64 ush_browser_text_len;
extern int ush_browser_last_space;
extern u64 ush_browser_link_count;
extern ush_browser_css_rule ush_browser_css_rules[USH_BROWSER_CSS_RULE_MAX];
extern u64 ush_browser_css_rule_count;

int ush_browser_render_html(const char *html, u64 html_size);
void ush_browser_print_rendered(const char *source_desc);
void ush_browser_print_source(const char *source_desc, const char *html, u64 html_size);
int ush_browser_run_session(const ush_state *sh, const char *arg);
int ush_browser_load_source(const ush_state *sh, const char *source, char *out_html, u64 out_html_cap, u64 *out_size);
int ush_browser_read_line(char *out_text, u64 out_size);
int ush_browser_resolve_href(const char *base_source, const char *href, char *out_source, u64 out_size);

char ush_browser_ascii_tolower(char ch);
int ush_browser_line_has_token_icase(const char *line, u64 line_len, const char *token);
void ush_browser_text_reset(void);
void ush_browser_text_trim_trailing_spaces(void);
void ush_browser_text_newline(void);
void ush_browser_text_append_char(char ch);
void ush_browser_text_append_raw_char(char ch);
void ush_browser_text_append(const char *text);
void ush_browser_text_append_raw(const char *text);
void ush_browser_text_horizontal_rule(void);
void ush_browser_style_reset(ush_browser_style *out_style);
void ush_browser_style_delta_reset(ush_browser_style_delta *out_delta);
int ush_browser_style_equal(const ush_browser_style *a, const ush_browser_style *b);
void ush_browser_style_apply_delta(ush_browser_style *io_style, const ush_browser_style_delta *delta);
void ush_browser_style_apply_anchor_default(ush_browser_style *io_style);
void ush_browser_text_emit_style(const ush_browser_style *style);
int ush_browser_css_parse_color_value(const char *value, u64 value_len, u32 *out_rgb);
void ush_browser_css_apply_declarations(const char *decl, u64 decl_len, ush_browser_style_delta *io_delta);
void ush_browser_css_scan_style_nodes(GumboNode *node);
void ush_browser_css_apply_rules_for_node(GumboNode *node, ush_browser_style *io_style);

void ush_browser_fetch_error_set(const char *message);
void ush_browser_fetch_error_set_tls(const char *prefix, const cleonos_tls_conn *conn);
int ush_browser_parse_ipv4_be(const char *text, u64 *out_ipv4_be);
u16 ush_browser_read_be16(const u8 *ptr);
void ush_browser_write_be16(u8 *ptr, u16 value);
int ush_browser_dns_resolve_ipv4(const char *host, u64 *out_ipv4_be);
int ush_browser_http_status_code(const char *raw, u64 raw_len);
int ush_browser_copy_http_header_value(const char *raw, u64 raw_len, const char *name, char *out, u64 out_cap);
int ush_browser_parse_http_headers(const char *raw, u64 raw_len, u64 *out_body_off, int *out_chunked,
                                   u64 *out_content_len, int *out_has_content_len, int *out_compressed);
int ush_browser_find_http_header_end(const char *raw, u64 raw_len, u64 *out_body_off);
int ush_browser_is_chunked_body_complete(const char *body, u64 body_len);
int ush_browser_decode_chunked_body(const char *body, u64 body_len, char *out, u64 out_cap, u64 *out_len);

#endif
