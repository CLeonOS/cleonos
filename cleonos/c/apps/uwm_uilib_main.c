#include <stdio.h>
#include <uwm_uilib.h>
#include "cmd_runtime.h"

int cleonos_app_main(int argc, char **argv, char **envp) {
    (void)argc;
    (void)argv;
    (void)envp;
    ush_writeln_i18n("uwm_uilib: UI component library image ready",
                     "uwm_uilib: UI 组件库镜像就绪");
    return 0;
}
