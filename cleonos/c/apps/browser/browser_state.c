#include "browser_internal.h"

#include <stdlib.h>

char *ush_browser_html_buf;
char *ush_browser_http_raw_buf;
char *ush_browser_text_buf;
char ush_browser_title[USH_BROWSER_TITLE_MAX];
ush_browser_link ush_browser_links[USH_BROWSER_LINK_MAX];
u64 ush_browser_text_len = 0ULL;
int ush_browser_last_space = 1;
u64 ush_browser_link_count = 0ULL;
ush_browser_css_rule ush_browser_css_rules[USH_BROWSER_CSS_RULE_MAX];
u64 ush_browser_css_rule_count = 0ULL;

int ush_browser_ensure_buffers(void) {
    if (ush_browser_html_buf != (char *)0 && ush_browser_http_raw_buf != (char *)0 &&
        ush_browser_text_buf != (char *)0) {
        return 1;
    }

    ush_browser_html_buf = (char *)malloc((size_t)USH_BROWSER_HTML_BUF_CAP);
    ush_browser_http_raw_buf = (char *)malloc((size_t)USH_BROWSER_HTML_BUF_CAP);
    ush_browser_text_buf = (char *)malloc((size_t)USH_BROWSER_TEXT_BUF_CAP);
    if (ush_browser_html_buf == (char *)0 || ush_browser_http_raw_buf == (char *)0 ||
        ush_browser_text_buf == (char *)0) {
        return 0;
    }

    ush_browser_html_buf[0] = '\0';
    ush_browser_http_raw_buf[0] = '\0';
    ush_browser_text_buf[0] = '\0';
    return 1;
}
