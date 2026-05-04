#include "cmd_runtime.h"

static void ush_chinese_rule(void) {
    ush_writeln("┌────────────────────────────────────────────┐");
}

static int ush_cmd_chinese(void) {
    int i;

    ush_writeln("\x1B[1;96mCLeonOS 中文/UTF-8 TTY 测试\x1B[0m");
    ush_chinese_rule();
    ush_writeln("基本中文: 你好，世界！这是中文显示测试。");
    ush_writeln("系统词汇: 内核 外壳 桌面 窗口 终端 浏览器 包管理器");
    ush_writeln("全角标点: ，。！？；：“”‘’（）【】《》——……￥");
    ush_writeln("中英混排: CLeonOS 支持 UTF-8 / Unicode / 中文 TTY 输出。");
    ush_writeln("数字混排: 版本 2026.05.04，进度 100%，状态 OK。");
    ush_chinese_rule();

    ush_writeln("双宽对齐检查:");
    ush_writeln("ASCII : |ABCD|EFGH|IJKL|");
    ush_writeln("中文  : |你好|世界|测试|");
    ush_writeln("混合  : |A你B|C好D|OS中文|");
    ush_writeln("全角  : |ＡＢ|１２|！？|");
    ush_chinese_rule();

    ush_writeln("ANSI 颜色中文:");
    ush_writeln("  \x1B[31m红色警告\x1B[0m  \x1B[32m绿色成功\x1B[0m  \x1B[33m黄色提示\x1B[0m  \x1B[36m青色信息\x1B[0m");
    ush_writeln("  \x1B[1m粗体中文\x1B[0m  \x1B[4m下划线中文\x1B[0m  \x1B[7m反色中文\x1B[0m");
    ush_chinese_rule();

    ush_writeln("盒线/符号:");
    ush_writeln("  ┌──────┬──────┐");
    ush_writeln("  │ 名称 │ 状态 │");
    ush_writeln("  ├──────┼──────┤");
    ush_writeln("  │ 中文 │ ✓ OK │");
    ush_writeln("  └──────┴──────┘");
    ush_writeln("  方块: █▓▒░  箭头: ← ↑ → ↓  图形: ★ ☆ ● ○ ■ □");
    ush_chinese_rule();

    ush_writeln("长行换行检查:");
    ush_writeln("  这是一段比较长的中文文本，用来检查 TTY 在中文双宽字符、英文字符、数字 1234567890 和标点混排时是否可以正确换行，不应出现覆盖、错位或乱码。");
    ush_chinese_rule();

    ush_writeln("字体缩放检查:");
    ush_writeln("\x1B[2z二倍中文标题\x1B[1z 普通大小恢复");
    ush_writeln("\x1B[3z三倍中文\x1B[1z 恢复");
    ush_chinese_rule();

    ush_writeln("滚动检查，下面输出 16 行中文:");
    for (i = 1; i <= 16; i++) {
        char num[4];
        num[0] = (char)('0' + (i / 10));
        num[1] = (char)('0' + (i % 10));
        num[2] = '\0';
        if (i < 10) {
            num[0] = (char)('0' + i);
            num[1] = '\0';
        }

        ush_write("  第 ");
        ush_write(num);
        ush_writeln(" 行：中文滚动输出测试，PgUp/PgDn 回看时应保持对齐。");
    }

    ush_chinese_rule();
    ush_writeln("\x1B[1;92m中文测试完成。如果这行显示正常，UTF-8 + 中文字体 + CJK 双宽基本闭环通过。\x1B[0m");
    return 1;
}

int cleonos_app_main(void) {
    ush_cmd_ctx ctx;
    ush_cmd_ret ret;
    ush_state sh;
    char initial_cwd[USH_PATH_MAX];
    int has_context = 0;
    int success;

    ush_zero(&ctx, (u64)sizeof(ctx));
    ush_zero(&ret, (u64)sizeof(ret));
    ush_init_state(&sh);
    ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);

    if (ush_command_ctx_read(&ctx) != 0) {
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "chinese") != 0) {
            has_context = 1;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_chinese();

    if (has_context != 0) {
        if (ush_streq(sh.cwd, initial_cwd) == 0) {
            ret.flags |= USH_CMD_RET_FLAG_CWD;
            ush_copy(ret.cwd, (u64)sizeof(ret.cwd), sh.cwd);
        }

        if (sh.exit_requested != 0) {
            ret.flags |= USH_CMD_RET_FLAG_EXIT;
            ret.exit_code = sh.exit_code;
        }

        (void)ush_command_ret_write(&ret);
    }

    return (success != 0) ? 0 : 1;
}
