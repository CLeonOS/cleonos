#include "browser_internal.h"

#include "gumbo.h"
#include <stddef.h>

static int ush_browser_is_skip_tag(GumboTag tag) {
    switch (tag) {
    case GUMBO_TAG_SCRIPT:
    case GUMBO_TAG_STYLE:
    case GUMBO_TAG_NOSCRIPT:
    case GUMBO_TAG_TEMPLATE:
    case GUMBO_TAG_IFRAME:
    case GUMBO_TAG_OBJECT:
        return 1;
    default:
        return 0;
    }
}

static int ush_browser_is_block_tag(GumboTag tag) {
    switch (tag) {
    case GUMBO_TAG_HTML:
    case GUMBO_TAG_HEAD:
    case GUMBO_TAG_BODY:
    case GUMBO_TAG_DIV:
    case GUMBO_TAG_SECTION:
    case GUMBO_TAG_ARTICLE:
    case GUMBO_TAG_ASIDE:
    case GUMBO_TAG_NAV:
    case GUMBO_TAG_ADDRESS:
    case GUMBO_TAG_P:
    case GUMBO_TAG_UL:
    case GUMBO_TAG_OL:
    case GUMBO_TAG_LI:
    case GUMBO_TAG_DL:
    case GUMBO_TAG_DT:
    case GUMBO_TAG_DD:
    case GUMBO_TAG_TABLE:
    case GUMBO_TAG_CAPTION:
    case GUMBO_TAG_TBODY:
    case GUMBO_TAG_THEAD:
    case GUMBO_TAG_TFOOT:
    case GUMBO_TAG_TR:
    case GUMBO_TAG_TD:
    case GUMBO_TAG_TH:
    case GUMBO_TAG_PRE:
    case GUMBO_TAG_BLOCKQUOTE:
    case GUMBO_TAG_FIGURE:
    case GUMBO_TAG_FIGCAPTION:
    case GUMBO_TAG_H1:
    case GUMBO_TAG_H2:
    case GUMBO_TAG_H3:
    case GUMBO_TAG_H4:
    case GUMBO_TAG_H5:
    case GUMBO_TAG_H6:
    case GUMBO_TAG_FOOTER:
    case GUMBO_TAG_HEADER:
    case GUMBO_TAG_MAIN:
    case GUMBO_TAG_HGROUP:
    case GUMBO_TAG_FORM:
    case GUMBO_TAG_FIELDSET:
    case GUMBO_TAG_DETAILS:
    case GUMBO_TAG_SUMMARY:
    case GUMBO_TAG_MENU:
    case GUMBO_TAG_DIR:
    case GUMBO_TAG_LISTING:
    case GUMBO_TAG_XMP:
    case GUMBO_TAG_PLAINTEXT:
    case GUMBO_TAG_CENTER:
    case GUMBO_TAG_DIALOG:
    case GUMBO_TAG_SEARCH:
        return 1;
    default:
        return 0;
    }
}

static void ush_browser_collect_plain_text(GumboNode *node, char *out, u64 out_cap, u64 *io_len) {
    if (node == (GumboNode *)0 || out == (char *)0 || io_len == (u64 *)0 || out_cap == 0ULL) {
        return;
    }

    if (node->type == GUMBO_NODE_TEXT || node->type == GUMBO_NODE_WHITESPACE || node->type == GUMBO_NODE_CDATA) {
        const char *txt = node->v.text.text;
        u64 i = 0ULL;

        while (txt != (const char *)0 && txt[i] != '\0') {
            char ch = txt[i];

            if (ch == '\r' || ch == '\n' || ch == '\t' || ch == '\f' || ch == '\v') {
                ch = ' ';
            }

            if ((unsigned char)ch >= 0x20U) {
                if (*io_len + 1ULL >= out_cap) {
                    break;
                }

                if (ch == ' ') {
                    if (*io_len > 0ULL && out[*io_len - 1ULL] != ' ') {
                        out[(*io_len)++] = ' ';
                    }
                } else {
                    out[(*io_len)++] = ch;
                }
            }

            i++;
        }

        out[*io_len] = '\0';
        return;
    }

    if (node->type == GUMBO_NODE_ELEMENT || node->type == GUMBO_NODE_TEMPLATE) {
        GumboVector *children = &node->v.element.children;
        u64 i;

        for (i = 0ULL; i < (u64)children->length; i++) {
            ush_browser_collect_plain_text((GumboNode *)children->data[i], out, out_cap, io_len);
        }
    }
}

static void ush_browser_collect_anchor_link(GumboNode *node) {
    GumboAttribute *href;
    u64 text_len = 0ULL;
    char text[USH_BROWSER_LINK_TEXT_MAX];

    if (node == (GumboNode *)0 || node->type != GUMBO_NODE_ELEMENT || node->v.element.tag != GUMBO_TAG_A) {
        return;
    }

    href = gumbo_get_attribute(&node->v.element.attributes, "href");
    if (href == (GumboAttribute *)0 || href->value == (const char *)0 || href->value[0] == '\0') {
        return;
    }

    if (ush_browser_link_count >= (u64)USH_BROWSER_LINK_MAX) {
        return;
    }

    ush_zero(text, (u64)sizeof(text));
    ush_browser_collect_plain_text(node, text, (u64)sizeof(text), &text_len);
    if (text[0] == '\0') {
        ush_copy(text, (u64)sizeof(text), "(link)");
    }

    ush_copy(ush_browser_links[ush_browser_link_count].text,
             (u64)sizeof(ush_browser_links[ush_browser_link_count].text), text);
    ush_copy(ush_browser_links[ush_browser_link_count].href,
             (u64)sizeof(ush_browser_links[ush_browser_link_count].href), href->value);
    ush_browser_link_count++;
}

static int ush_browser_find_title_node(GumboNode *node, char *out, u64 out_cap) {
    if (node == (GumboNode *)0 || out == (char *)0 || out_cap == 0ULL) {
        return 0;
    }

    if (node->type == GUMBO_NODE_ELEMENT && node->v.element.tag == GUMBO_TAG_TITLE) {
        u64 len = 0ULL;
        out[0] = '\0';
        ush_browser_collect_plain_text(node, out, out_cap, &len);
        return (out[0] != '\0') ? 1 : 0;
    }

    if (node->type == GUMBO_NODE_ELEMENT || node->type == GUMBO_NODE_TEMPLATE || node->type == GUMBO_NODE_DOCUMENT) {
        GumboVector *children =
            (node->type == GUMBO_NODE_DOCUMENT) ? &node->v.document.children : &node->v.element.children;
        u64 i;

        for (i = 0ULL; i < (u64)children->length; i++) {
            if (ush_browser_find_title_node((GumboNode *)children->data[i], out, out_cap) != 0) {
                return 1;
            }
        }
    }

    return 0;
}

static void ush_browser_walk_dom_styled(GumboNode *node, const ush_browser_style *parent_style) {
    if (node == (GumboNode *)0 || parent_style == (const ush_browser_style *)0) {
        return;
    }

    switch (node->type) {
    case GUMBO_NODE_TEXT:
    case GUMBO_NODE_WHITESPACE:
    case GUMBO_NODE_CDATA:
        if (parent_style->display_none == 0) {
            ush_browser_text_append(node->v.text.text);
        }
        return;

    case GUMBO_NODE_ELEMENT:
    case GUMBO_NODE_TEMPLATE: {
        GumboTag tag = node->v.element.tag;
        GumboVector *children = &node->v.element.children;
        GumboAttribute *style_attr;
        ush_browser_style style = *parent_style;
        ush_browser_style_delta inline_delta;
        int is_block = ush_browser_is_block_tag(tag);
        int style_changed;
        u64 i;

        if (ush_browser_is_skip_tag(tag) != 0) {
            return;
        }

        if (tag == GUMBO_TAG_A) {
            ush_browser_collect_anchor_link(node);
            ush_browser_style_apply_anchor_default(&style);
        }

        ush_browser_css_apply_rules_for_node(node, &style);

        style_attr = gumbo_get_attribute(&node->v.element.attributes, "style");
        if (style_attr != (GumboAttribute *)0 && style_attr->value != (const char *)0) {
            ush_browser_style_delta_reset(&inline_delta);
            ush_browser_css_apply_declarations(style_attr->value, ush_strlen(style_attr->value), &inline_delta);
            ush_browser_style_apply_delta(&style, &inline_delta);
        }

        if (style.display_none != 0) {
            return;
        }

        if (tag == GUMBO_TAG_BR) {
            ush_browser_text_newline();
            return;
        }

        if (tag == GUMBO_TAG_HR) {
            ush_browser_text_horizontal_rule();
            return;
        }

        style_changed = (ush_browser_style_equal(&style, parent_style) == 0) ? 1 : 0;

        if (is_block != 0) {
            ush_browser_text_newline();
            if (tag == GUMBO_TAG_LI) {
                ush_browser_text_append("* ");
            }
        }

        if (style_changed != 0) {
            ush_browser_text_emit_style(&style);
        }

        for (i = 0ULL; i < (u64)children->length; i++) {
            ush_browser_walk_dom_styled((GumboNode *)children->data[i], &style);
        }

        if (style_changed != 0) {
            ush_browser_text_emit_style(parent_style);
        }

        if (is_block != 0) {
            ush_browser_text_newline();
        }
        return;
    }

    case GUMBO_NODE_DOCUMENT: {
        GumboVector *children = &node->v.document.children;
        u64 i;

        if (parent_style->display_none != 0) {
            return;
        }

        for (i = 0ULL; i < (u64)children->length; i++) {
            ush_browser_walk_dom_styled((GumboNode *)children->data[i], parent_style);
        }
        return;
    }

    default:
        return;
    }
}

static int ush_browser_html_tag_name_matches(const char *html, u64 html_size, u64 pos, const char *tag_name) {
    u64 tag_len;
    u64 i;
    char end_ch;

    if (html == (const char *)0 || tag_name == (const char *)0 || pos >= html_size || html[pos] != '<') {
        return 0;
    }

    tag_len = ush_strlen(tag_name);
    if (tag_len == 0ULL) {
        return 0;
    }

    pos++;
    while (pos < html_size &&
           (html[pos] == ' ' || html[pos] == '\t' || html[pos] == '\r' || html[pos] == '\n' || html[pos] == '\f')) {
        pos++;
    }

    if (pos >= html_size || html[pos] == '/') {
        return 0;
    }

    for (i = 0ULL; i < tag_len; i++) {
        if (pos + i >= html_size ||
            ush_browser_ascii_tolower(html[pos + i]) != ush_browser_ascii_tolower(tag_name[i])) {
            return 0;
        }
    }

    pos += tag_len;
    if (pos >= html_size) {
        return 1;
    }

    end_ch = html[pos];
    return (end_ch == '>' || end_ch == '/' || end_ch == ' ' || end_ch == '\t' || end_ch == '\r' || end_ch == '\n' ||
            end_ch == '\f')
               ? 1
               : 0;
}

static int ush_browser_render_html_fallback(const char *html, u64 html_size) {
    u64 i = 0ULL;
    int in_tag = 0;
    int pending_space = 0;

    if (html == (const char *)0 || html_size == 0ULL) {
        return 0;
    }

    ush_browser_link_count = 0ULL;
    ush_browser_css_rule_count = 0ULL;
    ush_browser_text_reset();
    ush_zero(ush_browser_title, (u64)sizeof(ush_browser_title));
    ush_copy(ush_browser_title, (u64)sizeof(ush_browser_title), "HTML fallback");

    while (i < html_size && html[i] != '\0') {
        char ch = html[i];

        if (ch == '<') {
            in_tag = 1;
            if (ush_browser_html_tag_name_matches(html, html_size, i, "hr") != 0) {
                ush_browser_text_horizontal_rule();
                pending_space = 1;
            } else if (pending_space == 0) {
                ush_browser_text_append_char(' ');
                pending_space = 1;
            }
            i++;
            continue;
        }

        if (in_tag != 0) {
            if (ch == '>') {
                in_tag = 0;
            }
            i++;
            continue;
        }

        if (ch == '&') {
            ush_browser_text_append_char(' ');
            pending_space = 1;
            i++;
            continue;
        }

        ush_browser_text_append_char(ch);
        pending_space = (ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t') ? 1 : 0;
        i++;
    }

    ush_browser_text_trim_trailing_spaces();
    if (ush_browser_text_len == 0ULL) {
        ush_browser_text_append("[browser] no renderable text");
    }

    return 1;
}

int ush_browser_render_html(const char *html, u64 html_size) {
    GumboOutput *output;
    GumboOptions options;
    ush_browser_style root_style;
    u64 parse_size;

    if (html == (const char *)0 || html_size == 0ULL) {
        return 0;
    }

    parse_size = html_size;
    if (parse_size > (u64)USH_BROWSER_GUMBO_PARSE_MAX) {
        parse_size = (u64)USH_BROWSER_GUMBO_PARSE_MAX;
        return ush_browser_render_html_fallback(html, parse_size);
    }

    options = kGumboDefaultOptions;
    options.max_errors = USH_BROWSER_GUMBO_MAX_ERRORS;
    options.stop_on_first_error = false;

    output = gumbo_parse_with_options(&options, html, (size_t)parse_size);
    if (output == (GumboOutput *)0 || output->root == (GumboNode *)0) {
        if (output != (GumboOutput *)0) {
            gumbo_destroy_output(&options, output);
        }
        return ush_browser_render_html_fallback(html, parse_size);
    }

    ush_browser_link_count = 0ULL;
    ush_browser_css_rule_count = 0ULL;
    ush_browser_text_reset();
    ush_zero(ush_browser_title, (u64)sizeof(ush_browser_title));
    ush_browser_style_reset(&root_style);

    (void)ush_browser_find_title_node(output->root, ush_browser_title, (u64)sizeof(ush_browser_title));
    ush_browser_css_scan_style_nodes(output->root);
    ush_browser_walk_dom_styled(output->root, &root_style);
    ush_browser_text_trim_trailing_spaces();

    gumbo_destroy_output(&options, output);
    return 1;
}
