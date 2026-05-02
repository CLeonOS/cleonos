#include "browser_internal.h"

#include "gumbo.h"
#include <stddef.h>

#define USH_BROWSER_TABLE_MAX_ROWS 48U
#define USH_BROWSER_TABLE_MAX_COLS 8U
#define USH_BROWSER_TABLE_CELL_TEXT_MAX 96U
#define USH_BROWSER_TABLE_CAPTION_MAX 96U
#define USH_BROWSER_TABLE_CELL_MIN_WIDTH 4U
#define USH_BROWSER_TABLE_CELL_MAX_WIDTH 24U

typedef struct ush_browser_table_cell {
    char text[USH_BROWSER_TABLE_CELL_TEXT_MAX];
    int header;
} ush_browser_table_cell;

typedef struct ush_browser_table_row {
    ush_browser_table_cell cells[USH_BROWSER_TABLE_MAX_COLS];
    u64 cell_count;
    int has_header;
} ush_browser_table_row;

typedef struct ush_browser_table_model {
    ush_browser_table_row rows[USH_BROWSER_TABLE_MAX_ROWS];
    u64 row_count;
    u64 col_count;
    char caption[USH_BROWSER_TABLE_CAPTION_MAX];
} ush_browser_table_model;

static ush_browser_table_model ush_browser_table_scratch;

static void ush_browser_collect_anchor_link(GumboNode *node);
static void ush_browser_walk_dom_styled(GumboNode *node, const ush_browser_style *parent_style, u64 current_form);
static void ush_browser_collect_plain_text(GumboNode *node, char *out, u64 out_cap, u64 *io_len);
static void ush_browser_trim_ascii_text(char *text);

static void ush_browser_append_u64(u64 value) {
    char buf[32];

    if (snprintf(buf, sizeof(buf), "%llu", (unsigned long long)value) <= 0) {
        return;
    }
    ush_browser_text_append(buf);
}

static int ush_browser_is_skip_tag(GumboTag tag) {
    switch (tag) {
    case GUMBO_TAG_HEAD:
    case GUMBO_TAG_TITLE:
    case GUMBO_TAG_BASE:
    case GUMBO_TAG_LINK:
    case GUMBO_TAG_META:
    case GUMBO_TAG_SCRIPT:
    case GUMBO_TAG_STYLE:
    case GUMBO_TAG_NOSCRIPT:
    case GUMBO_TAG_TEMPLATE:
    case GUMBO_TAG_SOURCE:
    case GUMBO_TAG_TRACK:
    case GUMBO_TAG_PARAM:
    case GUMBO_TAG_COL:
    case GUMBO_TAG_COLGROUP:
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

static int ush_browser_ascii_is_alpha(char ch) {
    return ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) ? 1 : 0;
}

static int ush_browser_ascii_is_word(char ch) {
    return (ush_browser_ascii_is_alpha(ch) != 0 || (ch >= '0' && ch <= '9')) ? 1 : 0;
}

static char ush_browser_ascii_toupper(char ch) {
    if (ch >= 'a' && ch <= 'z') {
        return (char)(ch - 'a' + 'A');
    }
    return ch;
}

static int ush_browser_streq_icase(const char *left, const char *right) {
    u64 i = 0ULL;

    if (left == (const char *)0 || right == (const char *)0) {
        return 0;
    }

    while (left[i] != '\0' && right[i] != '\0') {
        if (ush_browser_ascii_tolower(left[i]) != ush_browser_ascii_tolower(right[i])) {
            return 0;
        }
        i++;
    }

    return (left[i] == '\0' && right[i] == '\0') ? 1 : 0;
}

static const char *ush_browser_attr_value(GumboNode *node, const char *name) {
    GumboAttribute *attr;

    if (node == (GumboNode *)0 || name == (const char *)0 || node->type != GUMBO_NODE_ELEMENT) {
        return (const char *)0;
    }

    attr = gumbo_get_attribute(&node->v.element.attributes, name);
    if (attr == (GumboAttribute *)0 || attr->value == (const char *)0) {
        return (const char *)0;
    }

    return attr->value;
}

static void ush_browser_copy_lower_ascii(char *out, u64 out_size, const char *value, const char *fallback) {
    u64 i = 0ULL;
    const char *src = (value != (const char *)0 && value[0] != '\0') ? value : fallback;

    if (out == (char *)0 || out_size == 0ULL) {
        return;
    }

    out[0] = '\0';
    if (src == (const char *)0) {
        return;
    }

    while (src[i] != '\0' && i + 1ULL < out_size) {
        out[i] = ush_browser_ascii_tolower(src[i]);
        i++;
    }
    out[i] = '\0';
}

static int ush_browser_attr_present(GumboNode *node, const char *name) {
    if (node == (GumboNode *)0 || name == (const char *)0 || node->type != GUMBO_NODE_ELEMENT) {
        return 0;
    }

    return (gumbo_get_attribute(&node->v.element.attributes, name) != (GumboAttribute *)0) ? 1 : 0;
}

static int ush_browser_attr_value_is(GumboNode *node, const char *name, const char *expected) {
    const char *value = ush_browser_attr_value(node, name);

    if (value == (const char *)0 || expected == (const char *)0) {
        return 0;
    }

    return ush_browser_streq_icase(value, expected);
}

static void ush_browser_text_append_limited(const char *text, u64 max_len) {
    u64 i = 0ULL;

    if (text == (const char *)0 || max_len == 0ULL) {
        return;
    }

    while (text[i] != '\0' && i < max_len) {
        ush_browser_text_append_char(text[i]);
        i++;
    }
}

static void ush_browser_append_attr_value(const char *value, const char *fallback) {
    if (value != (const char *)0 && value[0] != '\0') {
        ush_browser_text_append_limited(value, 96ULL);
    } else if (fallback != (const char *)0) {
        ush_browser_text_append(fallback);
    }
}

static u64 ush_browser_begin_form(GumboNode *node) {
    ush_browser_form *form;
    const char *method;
    const char *action;
    const char *label;
    u64 form_index;

    if (node == (GumboNode *)0 || node->type != GUMBO_NODE_ELEMENT ||
        ush_browser_form_count >= (u64)USH_BROWSER_FORM_MAX) {
        return (u64)-1;
    }

    form_index = ush_browser_form_count++;
    form = &ush_browser_forms[form_index];
    ush_zero(form, (u64)sizeof(*form));

    method = ush_browser_attr_value(node, "method");
    action = ush_browser_attr_value(node, "action");
    label = ush_browser_attr_value(node, "name");
    if (label == (const char *)0 || label[0] == '\0') {
        label = ush_browser_attr_value(node, "id");
    }

    if (method != (const char *)0 && ush_browser_streq_icase(method, "post") != 0) {
        ush_copy(form->method, (u64)sizeof(form->method), "post");
    } else {
        ush_copy(form->method, (u64)sizeof(form->method), "get");
    }

    if (action != (const char *)0 && action[0] != '\0') {
        ush_copy(form->action, (u64)sizeof(form->action), action);
    }
    if (label != (const char *)0 && label[0] != '\0') {
        ush_copy(form->label, (u64)sizeof(form->label), label);
    } else {
        ush_copy(form->label, (u64)sizeof(form->label), "form");
    }
    form->first_field = ush_browser_form_field_count;
    return form_index;
}

static u64 ush_browser_add_form_field(u64 form_index, const char *name, const char *value, const char *type,
                                      int successful) {
    ush_browser_form_field *field;
    ush_browser_form *form;
    u64 field_index;

    if (form_index >= ush_browser_form_count || name == (const char *)0 || name[0] == '\0' ||
        ush_browser_form_field_count >= (u64)USH_BROWSER_FORM_FIELD_MAX) {
        return 0ULL;
    }

    field_index = ush_browser_form_field_count++;
    field = &ush_browser_form_fields[field_index];
    form = &ush_browser_forms[form_index];
    ush_zero(field, (u64)sizeof(*field));

    field->form_index = form_index;
    ush_copy(field->name, (u64)sizeof(field->name), name);
    ush_copy(field->value, (u64)sizeof(field->value), (value != (const char *)0) ? value : "");
    ush_browser_copy_lower_ascii(field->type, (u64)sizeof(field->type), type, "text");
    field->successful = successful;
    form->field_count++;
    return field_index + 1ULL;
}

static void ush_browser_append_text_styled(const char *text, const ush_browser_style *style) {
    u64 i = 0ULL;
    int capitalize_next = 1;

    if (text == (const char *)0 || style == (const ush_browser_style *)0) {
        return;
    }

    while (text[i] != '\0') {
        char ch = text[i];

        if (style->text_transform == USH_BROWSER_TEXT_TRANSFORM_UPPERCASE) {
            ch = ush_browser_ascii_toupper(ch);
        } else if (style->text_transform == USH_BROWSER_TEXT_TRANSFORM_LOWERCASE) {
            ch = ush_browser_ascii_tolower(ch);
        } else if (style->text_transform == USH_BROWSER_TEXT_TRANSFORM_CAPITALIZE) {
            if (ush_browser_ascii_is_alpha(ch) != 0) {
                ch = (capitalize_next != 0) ? ush_browser_ascii_toupper(ch) : ush_browser_ascii_tolower(ch);
                capitalize_next = 0;
            } else if (ush_browser_ascii_is_word(ch) == 0) {
                capitalize_next = 1;
            }
        }

        if (style->white_space_pre != 0) {
            if (ch == '\r') {
                if (text[i + 1ULL] != '\n') {
                    ush_browser_text_append_raw_char('\n');
                }
            } else if (ch == '\t') {
                ush_browser_text_append_raw("    ");
            } else {
                ush_browser_text_append_raw_char(ch);
            }
        } else {
            ush_browser_text_append_char(ch);
        }

        i++;
    }
}

static void ush_browser_apply_tag_style(GumboTag tag, ush_browser_style *io_style) {
    if (io_style == (ush_browser_style *)0) {
        return;
    }

    switch (tag) {
    case GUMBO_TAG_A:
        ush_browser_style_apply_anchor_default(io_style);
        break;
    case GUMBO_TAG_H1:
        io_style->font_scale = 3;
        io_style->bold = 1;
        break;
    case GUMBO_TAG_H2:
    case GUMBO_TAG_H3:
        io_style->font_scale = 2;
        io_style->bold = 1;
        break;
    case GUMBO_TAG_B:
    case GUMBO_TAG_STRONG:
    case GUMBO_TAG_TH:
    case GUMBO_TAG_H4:
    case GUMBO_TAG_H5:
    case GUMBO_TAG_H6:
    case GUMBO_TAG_DT:
    case GUMBO_TAG_LEGEND:
    case GUMBO_TAG_SUMMARY:
    case GUMBO_TAG_BIG:
        io_style->bold = 1;
        break;
    case GUMBO_TAG_EM:
    case GUMBO_TAG_I:
    case GUMBO_TAG_CITE:
    case GUMBO_TAG_DFN:
    case GUMBO_TAG_VAR:
    case GUMBO_TAG_ADDRESS:
        io_style->italic = 1;
        break;
    case GUMBO_TAG_U:
    case GUMBO_TAG_INS:
        io_style->underline = 1;
        break;
    case GUMBO_TAG_S:
    case GUMBO_TAG_DEL:
    case GUMBO_TAG_STRIKE:
        io_style->strike = 1;
        break;
    case GUMBO_TAG_SMALL:
    case GUMBO_TAG_RP:
        io_style->dim = 1;
        break;
    case GUMBO_TAG_BLINK:
        io_style->bold = 1;
        io_style->fg_set = 1;
        io_style->fg_rgb = 0xCC0000U;
        break;
    case GUMBO_TAG_MARK:
        io_style->fg_set = 1;
        io_style->fg_rgb = 0x000000U;
        io_style->bg_set = 1;
        io_style->bg_rgb = 0xFFFF00U;
        break;
    case GUMBO_TAG_CODE:
    case GUMBO_TAG_KBD:
    case GUMBO_TAG_SAMP:
    case GUMBO_TAG_TT:
        io_style->fg_set = 1;
        io_style->fg_rgb = 0x111111U;
        io_style->bg_set = 1;
        io_style->bg_rgb = 0xE6E6E6U;
        break;
    case GUMBO_TAG_PRE:
    case GUMBO_TAG_LISTING:
    case GUMBO_TAG_XMP:
    case GUMBO_TAG_PLAINTEXT:
        io_style->white_space_pre = 1;
        io_style->fg_set = 1;
        io_style->fg_rgb = 0x111111U;
        io_style->bg_set = 1;
        io_style->bg_rgb = 0xF2F2F2U;
        break;
    default:
        break;
    }
}

static void ush_browser_apply_html_attr_style(GumboNode *node, ush_browser_style *io_style) {
    const char *value;
    u32 rgb;

    if (node == (GumboNode *)0 || io_style == (ush_browser_style *)0 || node->type != GUMBO_NODE_ELEMENT) {
        return;
    }

    if (ush_browser_attr_present(node, "hidden") != 0 || ush_browser_attr_value_is(node, "aria-hidden", "true") != 0) {
        io_style->display_none = 1;
        return;
    }

    value = ush_browser_attr_value(node, "color");
    if (value != (const char *)0 && ush_browser_css_parse_color_value(value, ush_strlen(value), &rgb) != 0) {
        io_style->fg_set = 1;
        io_style->fg_rgb = rgb;
    }

    value = ush_browser_attr_value(node, "bgcolor");
    if (value != (const char *)0 && ush_browser_css_parse_color_value(value, ush_strlen(value), &rgb) != 0) {
        io_style->bg_set = 1;
        io_style->bg_rgb = rgb;
    }

    value = ush_browser_attr_value(node, "size");
    if (value != (const char *)0 && (value[0] == '1' || value[0] == '2' || value[0] == '-')) {
        io_style->dim = 1;
    } else if (value != (const char *)0 && (value[0] == '5' || value[0] == '6' || value[0] == '7' || value[0] == '+')) {
        io_style->bold = 1;
    }
}

static void ush_browser_append_heading_prefix(GumboTag tag) {
    switch (tag) {
    case GUMBO_TAG_H1:
    case GUMBO_TAG_H2:
    case GUMBO_TAG_H3:
        break;
    case GUMBO_TAG_H4:
        ush_browser_text_append("#### ");
        break;
    case GUMBO_TAG_H5:
        ush_browser_text_append("##### ");
        break;
    case GUMBO_TAG_H6:
        ush_browser_text_append("###### ");
        break;
    default:
        break;
    }
}

static int ush_browser_render_input_tag(GumboNode *node, u64 current_form) {
    const char *type = ush_browser_attr_value(node, "type");
    const char *name = ush_browser_attr_value(node, "name");
    const char *value = ush_browser_attr_value(node, "value");
    const char *placeholder = ush_browser_attr_value(node, "placeholder");
    u64 field_number = 0ULL;

    if (type == (const char *)0 || type[0] == '\0') {
        type = "text";
    }

    if (ush_browser_streq_icase(type, "hidden") != 0) {
        if (current_form != (u64)-1 && name != (const char *)0 && name[0] != '\0') {
            (void)ush_browser_add_form_field(current_form, name, value, type, 1);
        }
        return 1;
    }

    if (ush_browser_streq_icase(type, "checkbox") != 0 || ush_browser_streq_icase(type, "radio") != 0) {
        int checked = ush_browser_attr_present(node, "checked");
        if (current_form != (u64)-1 && name != (const char *)0 && name[0] != '\0') {
            field_number = ush_browser_add_form_field(
                current_form, name, (value != (const char *)0 && value[0] != '\0') ? value : "on", type, checked);
        }
        ush_browser_text_append((ush_browser_attr_present(node, "checked") != 0) ? "[x]" : "[ ]");
        if (field_number != 0ULL) {
            ush_browser_text_append("#");
            ush_browser_append_u64(field_number);
        }
        if (value != (const char *)0 && value[0] != '\0') {
            ush_browser_text_append_char(' ');
            ush_browser_text_append_limited(value, 48ULL);
        } else if (name != (const char *)0 && name[0] != '\0') {
            ush_browser_text_append_char(' ');
            ush_browser_text_append_limited(name, 48ULL);
        }
        return 1;
    }

    if (ush_browser_streq_icase(type, "button") != 0 || ush_browser_streq_icase(type, "submit") != 0 ||
        ush_browser_streq_icase(type, "reset") != 0) {
        if (current_form != (u64)-1 && ush_browser_streq_icase(type, "submit") != 0) {
            ush_browser_forms[current_form].has_submit = 1;
        }
        ush_browser_text_append("[button: ");
        ush_browser_append_attr_value(value, type);
        if (current_form != (u64)-1 && ush_browser_streq_icase(type, "submit") != 0) {
            ush_browser_text_append(" -> submit ");
            ush_browser_append_u64(current_form + 1ULL);
        }
        ush_browser_text_append("]");
        return 1;
    }

    if (ush_browser_streq_icase(type, "password") != 0) {
        if (current_form != (u64)-1 && name != (const char *)0 && name[0] != '\0') {
            field_number = ush_browser_add_form_field(current_form, name, value, type, 1);
        }
        ush_browser_text_append("[password");
        if (field_number != 0ULL) {
            ush_browser_text_append("#");
            ush_browser_append_u64(field_number);
        }
        ush_browser_text_append("]");
        return 1;
    }

    ush_browser_text_append("[input");
    if (current_form != (u64)-1 && name != (const char *)0 && name[0] != '\0') {
        field_number = ush_browser_add_form_field(current_form, name, value, type, 1);
        if (field_number != 0ULL) {
            ush_browser_text_append("#");
            ush_browser_append_u64(field_number);
        }
    }
    if (name != (const char *)0 && name[0] != '\0') {
        ush_browser_text_append_char(' ');
        ush_browser_text_append_limited(name, 48ULL);
    }
    if (value != (const char *)0 && value[0] != '\0') {
        ush_browser_text_append("=\"");
        ush_browser_text_append_limited(value, 64ULL);
        ush_browser_text_append("\"");
    } else if (placeholder != (const char *)0 && placeholder[0] != '\0') {
        ush_browser_text_append(" placeholder=\"");
        ush_browser_text_append_limited(placeholder, 64ULL);
        ush_browser_text_append("\"");
    }
    ush_browser_text_append("]");
    return 1;
}

static int ush_browser_render_textarea_tag(GumboNode *node, u64 current_form) {
    const char *name = ush_browser_attr_value(node, "name");
    char text[USH_BROWSER_FORM_VALUE_MAX];
    u64 len = 0ULL;
    u64 field_number = 0ULL;

    ush_zero(text, (u64)sizeof(text));
    ush_browser_collect_plain_text(node, text, (u64)sizeof(text), &len);
    ush_browser_trim_ascii_text(text);

    if (current_form != (u64)-1 && name != (const char *)0 && name[0] != '\0') {
        field_number = ush_browser_add_form_field(current_form, name, text, "textarea", 1);
    }

    ush_browser_text_append("[textarea");
    if (field_number != 0ULL) {
        ush_browser_text_append("#");
        ush_browser_append_u64(field_number);
    }
    if (name != (const char *)0 && name[0] != '\0') {
        ush_browser_text_append_char(' ');
        ush_browser_text_append_limited(name, 48ULL);
    }
    if (text[0] != '\0') {
        ush_browser_text_append("=\"");
        ush_browser_text_append_limited(text, 64ULL);
        ush_browser_text_append("\"");
    }
    ush_browser_text_append("]");
    return 1;
}

static int ush_browser_render_simple_placeholder(GumboNode *node, const char *kind, const char *primary_attr,
                                                 const char *fallback_attr) {
    const char *primary = ush_browser_attr_value(node, primary_attr);
    const char *fallback = ush_browser_attr_value(node, fallback_attr);

    if (kind == (const char *)0) {
        return 0;
    }

    ush_browser_text_append("[");
    ush_browser_text_append(kind);
    if (primary != (const char *)0 && primary[0] != '\0') {
        ush_browser_text_append(": ");
        ush_browser_text_append_limited(primary, 96ULL);
    } else if (fallback != (const char *)0 && fallback[0] != '\0') {
        ush_browser_text_append(": ");
        ush_browser_text_append_limited(fallback, 96ULL);
    }
    ush_browser_text_append("]");
    return 1;
}

static int ush_browser_render_void_or_replaced_tag(GumboNode *node, GumboTag tag, u64 current_form) {
    const char *value;

    switch (tag) {
    case GUMBO_TAG_IMG:
    case GUMBO_TAG_IMAGE:
        return ush_browser_render_simple_placeholder(node, "image", "alt", "src");
    case GUMBO_TAG_IFRAME:
        return ush_browser_render_simple_placeholder(node, "iframe", "title", "src");
    case GUMBO_TAG_EMBED:
        return ush_browser_render_simple_placeholder(node, "embed", "title", "src");
    case GUMBO_TAG_OBJECT:
        return ush_browser_render_simple_placeholder(node, "object", "title", "data");
    case GUMBO_TAG_VIDEO:
        return ush_browser_render_simple_placeholder(node, "video", "title", "src");
    case GUMBO_TAG_AUDIO:
        return ush_browser_render_simple_placeholder(node, "audio", "title", "src");
    case GUMBO_TAG_CANVAS:
        return ush_browser_render_simple_placeholder(node, "canvas", "aria-label", "title");
    case GUMBO_TAG_SVG:
        return ush_browser_render_simple_placeholder(node, "svg", "aria-label", "title");
    case GUMBO_TAG_MATH:
        return ush_browser_render_simple_placeholder(node, "math", "aria-label", "title");
    case GUMBO_TAG_INPUT:
        return ush_browser_render_input_tag(node, current_form);
    case GUMBO_TAG_PROGRESS:
        ush_browser_text_append("[progress");
        value = ush_browser_attr_value(node, "value");
        if (value != (const char *)0 && value[0] != '\0') {
            ush_browser_text_append_char(' ');
            ush_browser_text_append_limited(value, 32ULL);
            value = ush_browser_attr_value(node, "max");
            if (value != (const char *)0 && value[0] != '\0') {
                ush_browser_text_append("/");
                ush_browser_text_append_limited(value, 32ULL);
            }
        }
        ush_browser_text_append("]");
        return 1;
    case GUMBO_TAG_METER:
        ush_browser_text_append("[meter");
        value = ush_browser_attr_value(node, "value");
        if (value != (const char *)0 && value[0] != '\0') {
            ush_browser_text_append_char(' ');
            ush_browser_text_append_limited(value, 32ULL);
        }
        ush_browser_text_append("]");
        return 1;
    case GUMBO_TAG_AREA:
        return ush_browser_render_simple_placeholder(node, "area", "alt", "href");
    default:
        return 0;
    }
}

static void ush_browser_emit_tag_prefix(GumboNode *node, GumboTag tag, u64 current_form) {
    const char *title;
    const char *value;

    switch (tag) {
    case GUMBO_TAG_LI:
        ush_browser_text_append("* ");
        break;
    case GUMBO_TAG_DT:
        ush_browser_text_append("- ");
        break;
    case GUMBO_TAG_DD:
        ush_browser_text_append_raw("  ");
        break;
    case GUMBO_TAG_BLOCKQUOTE:
        ush_browser_text_append("> ");
        break;
    case GUMBO_TAG_CAPTION:
        ush_browser_text_append("Table: ");
        break;
    case GUMBO_TAG_FIGCAPTION:
        ush_browser_text_append("Figure: ");
        break;
    case GUMBO_TAG_LEGEND:
        ush_browser_text_append("Legend: ");
        break;
    case GUMBO_TAG_SUMMARY:
        ush_browser_text_append("Summary: ");
        break;
    case GUMBO_TAG_Q:
        ush_browser_text_append("\"");
        break;
    case GUMBO_TAG_SUB:
        ush_browser_text_append("_");
        break;
    case GUMBO_TAG_SUP:
        ush_browser_text_append("^");
        break;
    case GUMBO_TAG_ABBR:
        title = ush_browser_attr_value(node, "title");
        if (title != (const char *)0 && title[0] != '\0') {
            ush_browser_text_append("[");
        }
        break;
    case GUMBO_TAG_TIME:
        value = ush_browser_attr_value(node, "datetime");
        if (value != (const char *)0 && value[0] != '\0') {
            ush_browser_text_append("[time ");
            ush_browser_text_append_limited(value, 48ULL);
            ush_browser_text_append(": ");
        }
        break;
    case GUMBO_TAG_DATA:
        value = ush_browser_attr_value(node, "value");
        if (value != (const char *)0 && value[0] != '\0') {
            ush_browser_text_append("[data ");
            ush_browser_text_append_limited(value, 48ULL);
            ush_browser_text_append(": ");
        }
        break;
    case GUMBO_TAG_BUTTON:
        ush_browser_text_append("[button: ");
        if (current_form != (u64)-1) {
            const char *type = ush_browser_attr_value(node, "type");
            if (type == (const char *)0 || type[0] == '\0' || ush_browser_streq_icase(type, "submit") != 0) {
                ush_browser_forms[current_form].has_submit = 1;
            }
        }
        break;
    case GUMBO_TAG_SELECT:
        ush_browser_text_append("[select: ");
        break;
    case GUMBO_TAG_OPTION:
        if (ush_browser_attr_present(node, "selected") != 0) {
            ush_browser_text_append("*");
        }
        ush_browser_text_append("{");
        break;
    case GUMBO_TAG_TEXTAREA:
        ush_browser_text_append("[textarea: ");
        break;
    case GUMBO_TAG_OUTPUT:
        ush_browser_text_append("[output: ");
        break;
    case GUMBO_TAG_TD:
    case GUMBO_TAG_TH:
        ush_browser_text_append("| ");
        break;
    default:
        ush_browser_append_heading_prefix(tag);
        break;
    }
}

static void ush_browser_emit_tag_suffix(GumboNode *node, GumboTag tag, u64 current_form) {
    const char *title;

    switch (tag) {
    case GUMBO_TAG_Q:
        ush_browser_text_append("\"");
        break;
    case GUMBO_TAG_ABBR:
        title = ush_browser_attr_value(node, "title");
        if (title != (const char *)0 && title[0] != '\0') {
            ush_browser_text_append("] (");
            ush_browser_text_append_limited(title, 72ULL);
            ush_browser_text_append(")");
        }
        break;
    case GUMBO_TAG_TIME:
        if (ush_browser_attr_value(node, "datetime") != (const char *)0) {
            ush_browser_text_append("]");
        }
        break;
    case GUMBO_TAG_DATA:
        if (ush_browser_attr_value(node, "value") != (const char *)0) {
            ush_browser_text_append("]");
        }
        break;
    case GUMBO_TAG_BUTTON:
        if (current_form != (u64)-1) {
            const char *type = ush_browser_attr_value(node, "type");
            if (type == (const char *)0 || type[0] == '\0' || ush_browser_streq_icase(type, "submit") != 0) {
                ush_browser_text_append(" -> submit ");
                ush_browser_append_u64(current_form + 1ULL);
            }
        }
        ush_browser_text_append("]");
        break;
    case GUMBO_TAG_SELECT:
    case GUMBO_TAG_TEXTAREA:
    case GUMBO_TAG_OUTPUT:
        ush_browser_text_append("]");
        break;
    case GUMBO_TAG_OPTION:
        ush_browser_text_append("} ");
        break;
    case GUMBO_TAG_TD:
    case GUMBO_TAG_TH:
        ush_browser_text_append_char(' ');
        break;
    default:
        break;
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

static void ush_browser_trim_ascii_text(char *text) {
    u64 start = 0ULL;
    u64 end;
    u64 i;

    if (text == (char *)0) {
        return;
    }

    while (text[start] == ' ' || text[start] == '\t' || text[start] == '\r' || text[start] == '\n') {
        start++;
    }

    end = ush_strlen(text);
    while (end > start && (text[end - 1ULL] == ' ' || text[end - 1ULL] == '\t' || text[end - 1ULL] == '\r' ||
                           text[end - 1ULL] == '\n')) {
        end--;
    }

    if (start > 0ULL) {
        for (i = 0ULL; start + i < end; i++) {
            text[i] = text[start + i];
        }
        text[i] = '\0';
    } else {
        text[end] = '\0';
    }
}

static void ush_browser_table_model_reset(ush_browser_table_model *model) {
    if (model == (ush_browser_table_model *)0) {
        return;
    }

    ush_zero(model, (u64)sizeof(*model));
}

static void ush_browser_table_collect_cell_text(GumboNode *node, char *out, u64 out_cap) {
    u64 len = 0ULL;

    if (out == (char *)0 || out_cap == 0ULL) {
        return;
    }

    out[0] = '\0';
    ush_browser_collect_plain_text(node, out, out_cap, &len);
    (void)len;
    ush_browser_trim_ascii_text(out);
}

static void ush_browser_table_add_cell(ush_browser_table_model *model, u64 row_index, GumboNode *cell_node,
                                       int header) {
    ush_browser_table_row *row;
    ush_browser_table_cell *cell;

    if (model == (ush_browser_table_model *)0 || cell_node == (GumboNode *)0 ||
        row_index >= (u64)USH_BROWSER_TABLE_MAX_ROWS) {
        return;
    }

    row = &model->rows[row_index];
    if (row->cell_count >= (u64)USH_BROWSER_TABLE_MAX_COLS) {
        return;
    }

    cell = &row->cells[row->cell_count];
    ush_browser_table_collect_cell_text(cell_node, cell->text, (u64)sizeof(cell->text));
    ush_browser_collect_anchor_link(cell_node);
    cell->header = header;
    if (header != 0) {
        row->has_header = 1;
    }
    row->cell_count++;
    if (row->cell_count > model->col_count) {
        model->col_count = row->cell_count;
    }
}

static void ush_browser_table_collect_row(ush_browser_table_model *model, GumboNode *row_node) {
    GumboVector *children;
    u64 row_index;
    u64 i;

    if (model == (ush_browser_table_model *)0 || row_node == (GumboNode *)0 || row_node->type != GUMBO_NODE_ELEMENT ||
        row_node->v.element.tag != GUMBO_TAG_TR || model->row_count >= (u64)USH_BROWSER_TABLE_MAX_ROWS) {
        return;
    }

    row_index = model->row_count;
    model->row_count++;
    children = &row_node->v.element.children;
    for (i = 0ULL; i < (u64)children->length; i++) {
        GumboNode *child = (GumboNode *)children->data[i];
        GumboTag tag;

        if (child == (GumboNode *)0 || child->type != GUMBO_NODE_ELEMENT) {
            continue;
        }

        tag = child->v.element.tag;
        if (tag == GUMBO_TAG_TD || tag == GUMBO_TAG_TH) {
            ush_browser_table_add_cell(model, row_index, child, (tag == GUMBO_TAG_TH) ? 1 : 0);
        }
    }

    if (model->rows[row_index].cell_count == 0ULL) {
        model->row_count--;
    }
}

static void ush_browser_table_collect_walk(ush_browser_table_model *model, GumboNode *node) {
    GumboVector *children;
    u64 i;

    if (model == (ush_browser_table_model *)0 || node == (GumboNode *)0 ||
        model->row_count >= (u64)USH_BROWSER_TABLE_MAX_ROWS) {
        return;
    }

    if (node->type != GUMBO_NODE_ELEMENT && node->type != GUMBO_NODE_TEMPLATE) {
        return;
    }

    if (node->v.element.tag == GUMBO_TAG_CAPTION && model->caption[0] == '\0') {
        ush_browser_table_collect_cell_text(node, model->caption, (u64)sizeof(model->caption));
        return;
    }

    if (node->v.element.tag == GUMBO_TAG_TR) {
        ush_browser_table_collect_row(model, node);
        return;
    }

    children = &node->v.element.children;
    for (i = 0ULL; i < (u64)children->length; i++) {
        ush_browser_table_collect_walk(model, (GumboNode *)children->data[i]);
    }
}

static u64 ush_browser_visible_text_len(const char *text) {
    u64 i = 0ULL;
    u64 out = 0ULL;

    if (text == (const char *)0) {
        return 0ULL;
    }

    while (text[i] != '\0') {
        if ((unsigned char)text[i] >= 0x20U) {
            out++;
        }
        i++;
    }
    return out;
}

static void ush_browser_table_append_repeated(char ch, u64 count) {
    while (count > 0ULL) {
        ush_browser_text_append_raw_char(ch);
        count--;
    }
}

static void ush_browser_table_append_cell_text(const char *text, u64 width) {
    u64 i = 0ULL;
    u64 used = 0ULL;

    if (text == (const char *)0) {
        text = "";
    }

    while (text[i] != '\0' && used < width) {
        unsigned char ch = (unsigned char)text[i];
        if (ch >= 0x20U) {
            ush_browser_text_append_raw_char((char)ch);
            used++;
        }
        i++;
    }

    while (used < width) {
        ush_browser_text_append_raw_char(' ');
        used++;
    }
}

static void ush_browser_table_append_border(const u64 *widths, u64 cols) {
    u64 col;

    ush_browser_text_append_raw_char('+');
    for (col = 0ULL; col < cols; col++) {
        ush_browser_table_append_repeated('-', widths[col] + 2ULL);
        ush_browser_text_append_raw_char('+');
    }
    ush_browser_text_newline();
}

static void ush_browser_table_append_row(const ush_browser_table_row *row, const u64 *widths, u64 cols) {
    u64 col;

    ush_browser_text_append_raw_char('|');
    for (col = 0ULL; col < cols; col++) {
        const char *text = "";

        if (row != (const ush_browser_table_row *)0 && col < row->cell_count) {
            text = row->cells[col].text;
        }

        ush_browser_text_append_raw_char(' ');
        ush_browser_table_append_cell_text(text, widths[col]);
        ush_browser_text_append_raw_char(' ');
        ush_browser_text_append_raw_char('|');
    }
    ush_browser_text_newline();
}

static void ush_browser_table_compute_widths(const ush_browser_table_model *model, u64 *widths, u64 cols) {
    u64 col;
    u64 row;
    u64 total = 1ULL;

    for (col = 0ULL; col < cols; col++) {
        widths[col] = (u64)USH_BROWSER_TABLE_CELL_MIN_WIDTH;
    }

    for (row = 0ULL; row < model->row_count; row++) {
        for (col = 0ULL; col < cols && col < model->rows[row].cell_count; col++) {
            u64 len = ush_browser_visible_text_len(model->rows[row].cells[col].text);
            if (len > widths[col]) {
                widths[col] = len;
            }
        }
    }

    for (col = 0ULL; col < cols; col++) {
        if (widths[col] > (u64)USH_BROWSER_TABLE_CELL_MAX_WIDTH) {
            widths[col] = (u64)USH_BROWSER_TABLE_CELL_MAX_WIDTH;
        }
        total += widths[col] + 3ULL;
    }

    while (total > (u64)USH_BROWSER_OUTPUT_COLS && cols > 0ULL) {
        int shrunk = 0;
        for (col = 0ULL; col < cols && total > (u64)USH_BROWSER_OUTPUT_COLS; col++) {
            if (widths[col] > (u64)USH_BROWSER_TABLE_CELL_MIN_WIDTH) {
                widths[col]--;
                total--;
                shrunk = 1;
            }
        }
        if (shrunk == 0) {
            break;
        }
    }
}

static int ush_browser_render_table_tag(GumboNode *node) {
    ush_browser_table_model *model = &ush_browser_table_scratch;
    u64 widths[USH_BROWSER_TABLE_MAX_COLS];
    u64 cols;
    u64 row;

    if (node == (GumboNode *)0 || node->type != GUMBO_NODE_ELEMENT || node->v.element.tag != GUMBO_TAG_TABLE) {
        return 0;
    }

    ush_browser_table_model_reset(model);
    ush_browser_table_collect_walk(model, node);
    if (model->row_count == 0ULL || model->col_count == 0ULL) {
        return 0;
    }

    cols = model->col_count;
    if (cols > (u64)USH_BROWSER_TABLE_MAX_COLS) {
        cols = (u64)USH_BROWSER_TABLE_MAX_COLS;
    }
    ush_browser_table_compute_widths(model, widths, cols);

    ush_browser_text_newline();
    if (model->caption[0] != '\0') {
        ush_browser_text_append("Table: ");
        ush_browser_text_append(model->caption);
        ush_browser_text_newline();
    }

    ush_browser_table_append_border(widths, cols);
    for (row = 0ULL; row < model->row_count; row++) {
        ush_browser_table_append_row(&model->rows[row], widths, cols);
        if (model->rows[row].has_header != 0) {
            ush_browser_table_append_border(widths, cols);
        }
    }
    ush_browser_table_append_border(widths, cols);

    if (model->row_count >= (u64)USH_BROWSER_TABLE_MAX_ROWS) {
        ush_browser_text_append("[browser] table truncated");
        ush_browser_text_newline();
    }

    return 1;
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

static void ush_browser_walk_dom_styled(GumboNode *node, const ush_browser_style *parent_style, u64 current_form) {
    if (node == (GumboNode *)0 || parent_style == (const ush_browser_style *)0) {
        return;
    }

    switch (node->type) {
    case GUMBO_NODE_TEXT:
    case GUMBO_NODE_WHITESPACE:
    case GUMBO_NODE_CDATA:
        if (parent_style->display_none == 0) {
            ush_browser_append_text_styled(node->v.text.text, parent_style);
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
        u64 child_form = current_form;
        u64 i;

        if (ush_browser_is_skip_tag(tag) != 0) {
            return;
        }

        if (tag == GUMBO_TAG_A) {
            ush_browser_collect_anchor_link(node);
        }

        ush_browser_apply_tag_style(tag, &style);
        ush_browser_apply_html_attr_style(node, &style);
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

        if (tag == GUMBO_TAG_FORM) {
            child_form = ush_browser_begin_form(node);
            if (child_form != (u64)-1) {
                ush_browser_text_append("[form ");
                ush_browser_append_u64(child_form + 1ULL);
                ush_browser_text_append(" ");
                ush_browser_text_append(ush_browser_forms[child_form].method);
                if (ush_browser_forms[child_form].action[0] != '\0') {
                    ush_browser_text_append(" ");
                    ush_browser_text_append_limited(ush_browser_forms[child_form].action, 72ULL);
                }
                ush_browser_text_append("]");
                ush_browser_text_newline();
            }
        }

        if (ush_browser_render_void_or_replaced_tag(node, tag, child_form) != 0) {
            return;
        }

        if (tag == GUMBO_TAG_TEXTAREA) {
            (void)ush_browser_render_textarea_tag(node, child_form);
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

        if (tag == GUMBO_TAG_TABLE && ush_browser_render_table_tag(node) != 0) {
            return;
        }

        style_changed = (ush_browser_style_equal(&style, parent_style) == 0) ? 1 : 0;

        if (is_block != 0) {
            ush_browser_text_newline();
        }

        if (style_changed != 0) {
            ush_browser_text_emit_style(&style);
        }

        ush_browser_emit_tag_prefix(node, tag, child_form);

        for (i = 0ULL; i < (u64)children->length; i++) {
            ush_browser_walk_dom_styled((GumboNode *)children->data[i], &style, child_form);
        }

        ush_browser_emit_tag_suffix(node, tag, child_form);

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
            ush_browser_walk_dom_styled((GumboNode *)children->data[i], parent_style, current_form);
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
    ush_browser_forms_reset();
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
    ush_browser_forms_reset();
    ush_browser_css_rule_count = 0ULL;
    ush_browser_text_reset();
    ush_zero(ush_browser_title, (u64)sizeof(ush_browser_title));
    ush_browser_style_reset(&root_style);

    (void)ush_browser_find_title_node(output->root, ush_browser_title, (u64)sizeof(ush_browser_title));
    ush_browser_css_scan_style_nodes(output->root);
    ush_browser_walk_dom_styled(output->root, &root_style, (u64)-1);
    ush_browser_text_trim_trailing_spaces();

    gumbo_destroy_output(&options, output);
    return 1;
}
