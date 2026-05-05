#include "cmd_runtime.h"

int cleonos_app_main(void) {
    ush_writeln_i18n("[USER][HELLO] Hello world from /hello.elf",
                     "[USER][HELLO] 来自 /hello.elf 的你好世界");
    return 0;
}
