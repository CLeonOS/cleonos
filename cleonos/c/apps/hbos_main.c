#include "hbos/hbos.h"

int cleonos_app_main(int argc, char **argv, char **envp) {
    static hbos_state state;

    (void)argc;
    (void)argv;
    (void)envp;

    if (hbos_init(&state) == 0) {
        static const char msg[] = "hbos: failed to initialize terminal host\n";
        (void)cleonos_sys_tty_write(msg, sizeof(msg) - 1ULL);
        return 1;
    }

    (void)cleonos_sys_tty_status_set("hbos: HariboteOS user-mode emulator | exit2cleonos to return");
    hbos_present(&state);

    while (state.running != 0) {
        int ch;
        while ((ch = hbos_poll_char()) >= 0) {
            if (ch == '\r' || ch == '\n') {
                hbos_console_submit(&state);
            } else if (ch == 8 || ch == 127) {
                hbos_console_backspace(&state);
            } else {
                hbos_console_input_char(&state, (char)ch);
            }
            if (state.running == 0) {
                break;
            }
        }

        /*
         * Drain a whole keyboard burst before rendering/sleeping.  Processing a
         * single char per tick lets stale bytes interleave with the next prompt.
         */
        hbos_present(&state);
        hbos_sleep(1ULL);
    }

    hbos_present(&state);
    (void)cleonos_sys_tty_status_set("");
    hbos_shutdown(&state);
    return 0;
}
