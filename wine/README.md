# CLeonOS-Wine (Native)

CLeonOS-Wine 现在改为自研运行器：基于 Python + Unicorn，直接运行 CLeonOS x86_64 用户 ELF。

不再依赖 Qiling。

## 文件结构

- `wine/cleonos_wine.py`：兼容入口脚本
- `wine/cleonos_wine_lib/cli.py`：命令行参数与启动流程
- `wine/cleonos_wine_lib/runner.py`：ELF 装载、执行、syscall 分发
- `wine/cleonos_wine_lib/state.py`：内核态统计与共享状态
- `wine/cleonos_wine_lib/input_pump.py`：主机键盘输入线程
- `wine/cleonos_wine_lib/constants.py`：常量与 syscall ID
- `wine/cleonos_wine_lib/platform.py`：Unicorn 导入与平台适配
- `wine/requirements.txt`：Python 依赖（Unicorn + pygame）

## 安装

```bash
pip install -r wine/requirements.txt
```

## 运行

```bash
python wine/cleonos_wine.py /hello.elf --rootfs build/x86_64/ramdisk_root
python wine/cleonos_wine.py /shell/shell.elf --rootfs build/x86_64/ramdisk_root
python wine/cleonos_wine.py /shell/qrcode.elf --rootfs build/x86_64/ramdisk_root --fb-window -- --ecc H "hello wine"
```

也支持直接传宿主路径：

```bash
python wine/cleonos_wine.py build/x86_64/ramdisk_root/shell/shell.elf --rootfs build/x86_64/ramdisk_root
```

## 支持

- ELF64 (x86_64) PT_LOAD 段装载
- CLeonOS `int 0x80` syscall 0..83（含 `FD_*`、`DL_*`、`FB_*`、`PROC_*`、`STATS_*`、`EXEC_PATHV_IO`）
- TTY 输出与键盘输入队列
- rootfs 文件/目录访问（`FS_*`）
- `/temp` 写入限制（`FS_MKDIR/WRITE/APPEND/REMOVE`）
- `EXEC_PATH/EXEC_PATHV` 执行 ELF（带深度限制）
- `EXEC_PATHV_IO`（支持 stdio fd 继承/重定向）
- `SPAWN_PATH/SPAWN_PATHV/WAITPID/EXIT/SLEEP_TICKS/YIELD`
- 进程 `argv/env` 查询（`PROC_ARGC/PROC_ARGV/PROC_ENVC/PROC_ENV`）
- 进程枚举与快照（`PROC_COUNT/PROC_PID_AT/PROC_SNAPSHOT/PROC_KILL`）
- syscall 统计（`STATS_TOTAL/STATS_ID_COUNT/STATS_RECENT_*`）
- 文件描述符（`FD_OPEN/FD_READ/FD_WRITE/FD_CLOSE/FD_DUP`）
- 动态库兼容加载（`DL_OPEN/DL_CLOSE/DL_SYM`，基于 ELF 符号解析）
- framebuffer 兼容（`FB_INFO/FB_BLIT/FB_CLEAR`，支持内存缓冲与窗口显示）
- 异常退出状态编码与故障元信息（`PROC_LAST_SIGNAL/PROC_FAULT_*`）

## 参数

- `--no-kbd`：关闭输入线程
- `--fb-window`：启用 framebuffer 窗口显示（pygame）
- `--fb-scale N`：窗口缩放倍数（默认 `2`）
- `--fb-max-fps N`：窗口刷新上限（默认 `60`）
- `--fb-hold-ms N`：程序退出后窗口保留毫秒数（默认 `2500`，静态图更容易看清）
- `--argv-line "..."`：直接指定 guest 参数行（等价于 shell 参数字符串）
- `--cwd PATH`：写入命令上下文中的工作目录（默认 `/`）
- `--` 之后内容：作为 guest argv 透传（推荐）
- `--max-exec-depth N`：设置 exec 嵌套深度上限
- `--verbose`：打印更多日志

## `execv/spawnv` 参数格式

- `argv_line`：按空白字符分词（与内核当前实现一致，不支持引号转义）。
- `env_line`：按 `;` 或换行切分环境变量项，会去掉每项末尾空白。
- 子进程 `argv[0]` 固定为目标程序路径（如 `/shell/ls.elf`）。

## 退出状态说明

- 正常退出：返回普通退出码。
- 异常退出：最高位为 `1`，并编码：
- bits `7:0` = signal
- bits `15:8` = CPU exception vector
- bits `31:16` = error code 低 16 位
