#include "cmd_runtime.h"
static int ush_cmd_help(void) {
    ush_writeln_i18n("commands:", "命令:");
    ush_writeln("  help");
    ush_writeln("  args [a b c]      (print argc/argv/envp)");
    ush_writeln("  ls [-l] [-R] [path]");
    ush_writeln("  cat [file]        (reads pipeline input when file omitted)");
    ush_writeln("  grep [-n] <pattern> [file]");
    ush_writeln("  head [-n N] [file] / tail [-n N] [file]");
    ush_writeln("  wc [file] / cut -d <char> -f <N> [file] / uniq [file] / sort [file]");
    ush_writeln("  pwd");
    ush_writeln("  cd [dir]");
    ush_writeln("  exec|run <path|name> [args...]");
    ush_writeln("  clear");
    ush_writeln("  ansi / ansitest / color");
    ush_writeln("  bmpview <file.bmp> [cols]");
    ush_writeln("  imgview <file.png> [--ascii]");
    ush_writeln("  qrcode [--ecc <L|M|Q|H>] <text>");
    ush_writeln("  vim [file]         (vim-like editor: normal/insert/:w/:q/:wq)");
    ush_writeln("  uwm                (user-space window manager; Start includes Task Manager)");
    ush_writeln("  wavplay <file.wav> [steps] [ticks] / wavplay --stop");
    ush_writeln("  uname [-a|--sysinfo]");
    ush_writeln("  locale [get|set <locale>]  (kernel system language)");
    ush_writeln("  fastfetch [--plain]");
    ush_writeln("  whoami / passwd [user] / logout");
    ush_writeln("  users / useradd [-a|--admin] <name> / userdel <name> / usermod <admin|user> <name>");
    ush_writeln("  doom [wad_path]    (framebuffer bootstrap renderer)");
    ush_writeln("  memstat / fsstat / taskstat / userstat / shstat / stats");
    ush_writeln("  tty [index]");
    ush_writeln("  dmesg [n]");
    ush_writeln("  kbdstat");
    ush_writeln("  mkdir <dir>");
    ush_writeln("  touch <file>");
    ush_writeln("  write <file> <text>   (or from pipeline)");
    ush_writeln("  append <file> <text>  (or from pipeline)");
    ush_writeln("  cp <src> <dst>");
    ush_writeln("  mv <src> <dst>");
    ush_writeln("  rm <path>");
    ush_writeln("  diskinfo");
    ush_writeln("  mkfsfat32 [label]");
    ush_writeln("  mount [path]     (default suggested: /temp/disk)");
    ush_writeln("  partctl <subcommand>   (mbr partition control: list/init-mbr/create/delete/set-boot)");
    ush_writeln("  ping <a.b.c.d> [count]");
    ush_writeln("  ifconfig");
    ush_writeln("  nslookup <domain>");
    ush_writeln("  httpget <http://...|https://...>");
    ush_writeln("  wget <http://...|https://...> [output] / wget -O <output> <url>");
    ush_writeln("  pid");
    ush_writeln("  spawn <path|name> [args...] / bg <path|name> [args...]");
    ush_writeln("  wait <pid> / fg [pid]");
    ush_writeln("  kill <pid> [signal]");
    ush_writeln("  jobs [-a] / ps [-a] [-u] / procstat [pid|self] [-a]");
    ush_writeln("  top [--once] [-n loops] [-d ticks] / sysstat [-a] [-n N]");
    ush_writeln("  kdbg sym <addr> / kdbg bt <rbp> <rip> / kdbg regs");
    ush_writeln("  sleep <ticks>");
    ush_writeln("  spin               (busy loop test for Alt+Ctrl+C)");
    ush_writeln("  yield");
    ush_writeln("  shutdown / restart");
    ush_writeln("  exit [code]");
    ush_writeln("  rusttest / panic / elfloader (kernel shell only)");
    ush_writeln_i18n("pipeline/redirection: cmd1 | cmd2 | cmd3 > out.txt",
                     "管道/重定向: cmd1 | cmd2 | cmd3 > out.txt");
    ush_writeln_i18n("redirection append:   cmd >> out.txt", "追加重定向:   cmd >> out.txt");
    ush_writeln_i18n("edit keys: Left/Right, Home/End, Up/Down history",
                     "编辑按键: Left/Right, Home/End, Up/Down 历史");
    return 1;
}

int cleonos_app_main(void) {
    ush_cmd_ctx ctx;
    ush_cmd_ret ret;
    ush_state sh;
    char initial_cwd[USH_PATH_MAX];
    int has_context = 0;
    int success = 0;

    ush_zero(&ctx, (u64)sizeof(ctx));
    ush_zero(&ret, (u64)sizeof(ret));
    ush_init_state(&sh);
    ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);

    if (ush_command_ctx_read(&ctx) != 0) {
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "help") != 0) {
            has_context = 1;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_help();

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
