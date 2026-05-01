#include "browser_internal.h"

#include <stdio.h>

#include "../tls/cleonos_tls.h"

static char ush_browser_fetch_error[USH_BROWSER_FETCH_ERROR_MAX];

static void ush_browser_serial_log(const char *message) {
    if (message == (const char *)0 || message[0] == '\0') {
        return;
    }

    (void)cleonos_sys_log_write(message, ush_strlen(message));
}

void ush_browser_fetch_error_set(const char *message) {
    if (message == (const char *)0 || message[0] == '\0') {
        ush_browser_fetch_error[0] = '\0';
        return;
    }

    ush_copy(ush_browser_fetch_error, (u64)sizeof(ush_browser_fetch_error), message);
}

void ush_browser_fetch_error_set_tls(const char *prefix, const cleonos_tls_conn *conn) {
    char tls_error[96];
    char line[USH_BROWSER_FETCH_ERROR_MAX];
    char serial_line[USH_BROWSER_FETCH_ERROR_MAX + 48U];
    int tls_code;

    if (prefix == (const char *)0) {
        prefix = "tls failed";
    }

    tls_code = cleonos_tls_last_error(conn);
    cleonos_tls_error_text(tls_code, tls_error, (u64)sizeof(tls_error));
    if (snprintf(line, sizeof(line), "%s: %s (code=%d)", prefix, tls_error, tls_code) <= 0) {
        ush_browser_serial_log("[BROWSER][TLS] error formatting failed");
        ush_browser_fetch_error_set(prefix);
        return;
    }

    if (snprintf(serial_line, sizeof(serial_line), "[BROWSER][TLS] %s", line) > 0) {
        ush_browser_serial_log(serial_line);
    } else {
        ush_browser_serial_log("[BROWSER][TLS] error formatting failed");
    }
    ush_browser_fetch_error_set(line);
}

const char *ush_browser_fetch_last_error(void) {
    if (ush_browser_fetch_error[0] == '\0') {
        return "unknown fetch error";
    }
    return ush_browser_fetch_error;
}
