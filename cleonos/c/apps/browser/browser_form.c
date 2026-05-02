#include "browser_internal.h"

#include <stdio.h>
#include <string.h>

static int ush_browser_form_is_unreserved(char ch) {
    if (ch >= 'a' && ch <= 'z') {
        return 1;
    }
    if (ch >= 'A' && ch <= 'Z') {
        return 1;
    }
    if (ch >= '0' && ch <= '9') {
        return 1;
    }
    return (ch == '-' || ch == '_' || ch == '.' || ch == '~') ? 1 : 0;
}

static char ush_browser_form_hex(u8 value) {
    value &= 0x0FU;
    return (value < 10U) ? (char)('0' + value) : (char)('A' + (value - 10U));
}

static int ush_browser_form_urlenc_append(char *out, u64 out_cap, u64 *io_len, const char *text) {
    u64 i = 0ULL;

    if (out == (char *)0 || io_len == (u64 *)0 || text == (const char *)0 || out_cap == 0ULL) {
        return 0;
    }

    while (text[i] != '\0') {
        unsigned char ch = (unsigned char)text[i];

        if (ush_browser_form_is_unreserved((char)ch) != 0) {
            if (*io_len + 1ULL >= out_cap) {
                return 0;
            }
            out[(*io_len)++] = (char)ch;
        } else if (ch == ' ') {
            if (*io_len + 1ULL >= out_cap) {
                return 0;
            }
            out[(*io_len)++] = '+';
        } else {
            if (*io_len + 3ULL >= out_cap) {
                return 0;
            }
            out[(*io_len)++] = '%';
            out[(*io_len)++] = ush_browser_form_hex((u8)(ch >> 4U));
            out[(*io_len)++] = ush_browser_form_hex((u8)ch);
        }
        out[*io_len] = '\0';
        i++;
    }

    return 1;
}

static int ush_browser_form_build_body(u64 form_index, char *out_body, u64 out_body_size, u64 *out_body_len) {
    u64 i;
    int first = 1;

    if (out_body == (char *)0 || out_body_size == 0ULL || out_body_len == (u64 *)0) {
        return 0;
    }

    out_body[0] = '\0';
    *out_body_len = 0ULL;

    for (i = 0ULL; i < ush_browser_form_field_count; i++) {
        ush_browser_form_field *field = &ush_browser_form_fields[i];

        if (field->form_index != form_index || field->successful == 0 || field->name[0] == '\0') {
            continue;
        }

        if (first == 0) {
            if (*out_body_len + 1ULL >= out_body_size) {
                return 0;
            }
            out_body[(*out_body_len)++] = '&';
            out_body[*out_body_len] = '\0';
        }

        if (ush_browser_form_urlenc_append(out_body, out_body_size, out_body_len, field->name) == 0) {
            return 0;
        }
        if (*out_body_len + 1ULL >= out_body_size) {
            return 0;
        }
        out_body[(*out_body_len)++] = '=';
        out_body[*out_body_len] = '\0';
        if (ush_browser_form_urlenc_append(out_body, out_body_size, out_body_len, field->value) == 0) {
            return 0;
        }
        first = 0;
    }

    return 1;
}

static int ush_browser_form_join_get_url(const char *base_url, const char *query, char *out_source, u64 out_size) {
    u64 base_len;
    u64 query_len;
    char sep = '?';

    if (base_url == (const char *)0 || query == (const char *)0 || out_source == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    base_len = ush_strlen(base_url);
    query_len = ush_strlen(query);
    if (base_len == 0ULL || base_len + query_len + 2ULL > out_size) {
        return 0;
    }

    if (strchr(base_url, '?') != (char *)0) {
        sep = '&';
    }
    if (query_len == 0ULL) {
        ush_copy(out_source, out_size, base_url);
        return 1;
    }

    if (snprintf(out_source, (unsigned long)out_size, "%s%c%s", base_url, sep, query) <= 0) {
        return 0;
    }
    return 1;
}

void ush_browser_forms_reset(void) {
    ush_zero(ush_browser_forms, (u64)sizeof(ush_browser_forms));
    ush_zero(ush_browser_form_fields, (u64)sizeof(ush_browser_form_fields));
    ush_browser_form_count = 0ULL;
    ush_browser_form_field_count = 0ULL;
}

int ush_browser_form_set_value(u64 field_number, const char *value) {
    ush_browser_form_field *field;

    if (field_number == 0ULL || field_number > ush_browser_form_field_count || value == (const char *)0) {
        return 0;
    }

    field = &ush_browser_form_fields[field_number - 1ULL];
    if (field->successful == 0 || field->name[0] == '\0') {
        return 0;
    }

    ush_copy(field->value, (u64)sizeof(field->value), value);
    return 1;
}

int ush_browser_form_submit(const char *base_source, u64 form_number, char *out_source, u64 out_source_size,
                            char *out_body, u64 out_body_size, u64 *out_body_len, int *out_post) {
    ush_browser_form *form;
    char action[USH_BROWSER_FORM_ACTION_MAX];

    if (base_source == (const char *)0 || form_number == 0ULL || form_number > ush_browser_form_count ||
        out_source == (char *)0 || out_source_size == 0ULL || out_body == (char *)0 || out_body_size == 0ULL ||
        out_body_len == (u64 *)0 || out_post == (int *)0) {
        return 0;
    }

    form = &ush_browser_forms[form_number - 1ULL];
    out_source[0] = '\0';
    out_body[0] = '\0';
    *out_body_len = 0ULL;
    *out_post = 0;

    if (form->action[0] != '\0') {
        if (ush_browser_resolve_href(base_source, form->action, action, (u64)sizeof(action)) == 0) {
            return 0;
        }
    } else {
        ush_copy(action, (u64)sizeof(action), base_source);
    }

    if (ush_browser_form_build_body(form_number - 1ULL, out_body, out_body_size, out_body_len) == 0) {
        return 0;
    }

    if (form->method[0] == 'p' || form->method[0] == 'P') {
        ush_copy(out_source, out_source_size, action);
        *out_post = 1;
        return 1;
    }

    return ush_browser_form_join_get_url(action, out_body, out_source, out_source_size);
}

void ush_browser_print_forms(void) {
    u64 i;

    if (ush_browser_form_count == 0ULL && ush_browser_form_field_count == 0ULL) {
        return;
    }

    ush_writeln("");
    ush_writeln("[forms]");
    for (i = 0ULL; i < ush_browser_form_count; i++) {
        ush_browser_form *form = &ush_browser_forms[i];
        const char *action = (form->action[0] != '\0') ? form->action : "(current page)";
        const char *method = (form->method[0] != '\0') ? form->method : "get";

        (void)printf("  form[%llu] method=%s action=%s fields=%llu\n", (unsigned long long)(i + 1ULL), method,
                     action, (unsigned long long)form->field_count);
    }

    for (i = 0ULL; i < ush_browser_form_field_count; i++) {
        ush_browser_form_field *field = &ush_browser_form_fields[i];

        if (field->successful == 0 || field->name[0] == '\0') {
            continue;
        }

        (void)printf("  field[%llu] form=%llu name=%s type=%s value=\"%s\"\n", (unsigned long long)(i + 1ULL),
                     (unsigned long long)(field->form_index + 1ULL), field->name,
                     (field->type[0] != '\0') ? field->type : "text", field->value);
    }
}
