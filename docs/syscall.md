# CLeonOS Syscall 文档

本文档描述 CLeonOS 用户态通过 `int 0x80` 进入内核的 syscall ABI 与当前实现行为。

## 1. 调用约定（x86_64）

用户态统一通过：

```c
u64 cleonos_syscall(u64 id, u64 arg0, u64 arg1, u64 arg2);
```

寄存器约定：

- `rax` = syscall id
- `rbx` = arg0
- `rcx` = arg1
- `rdx` = arg2
- 返回值在 `rax`

中断向量：

- `int 0x80`

头文件位置：

- `cleonos/c/include/cleonos_syscall.h`

内核分发位置：

- `clks/kernel/runtime/syscall.c`
- 实际实现已经按分类拆分到 `clks/kernel/runtime/syscall/**/*.inc`
- `clks/kernel/runtime/syscall.c` 现在是聚合入口，包含分类实现和 dispatch

## 2. 全局返回规则

- 成功时通常返回非负值（如长度、计数、状态）。
- 失败时多数接口返回 `0xFFFFFFFFFFFFFFFF`（即 `u64` 的 `-1`）。
- 部分接口失败返回 `0`（例如 `FS_READ` / `FS_WRITE` / `FS_APPEND` / `FS_REMOVE` / `LOG_JOURNAL_READ`）。

进程退出状态补充（`EXEC_PATH*` / `WAITPID`）：

- 普通退出：返回值即用户程序退出码。
- 异常终止：返回值最高位为 `1`（`1<<63`），并编码：
- bits `7:0` = signal
- bits `15:8` = CPU exception vector
- bits `31:16` = exception error code 低 16 位

进程状态值（`proc_snapshot.state`）：

- `0` = `UNUSED`
- `1` = `PENDING`
- `2` = `RUNNING`
- `3` = `EXITED`
- `4` = `STOPPED`

常用信号值（`PROC_KILL`）：

- `SIGKILL` = `9`
- `SIGTERM` = `15`
- `SIGCONT` = `18`
- `SIGSTOP` = `19`

## 3. 当前实现中的长度/路径限制

以下限制由内核 `clks/kernel/runtime/syscall/**/*.inc` 当前实现决定：

- 日志写入 `LOG_WRITE`：最大拷贝 `191` 字节。
- TTY 文本写入 `TTY_WRITE`：最大拷贝 `2048` 字节。
- 文件读取 `FS_READ`：最多读取 `min(file_size, buffer_size)` 字节。
- 文件写入 `FS_WRITE` / `FS_APPEND`：内核按 `65536` 字节分块搬运；这是实现分块大小，不是文件大小上限。
- log journal 行读取缓冲：`256` 字节。
- 路径缓冲上限：`192` 字节（包含 `\0`）。
- 文件名输出上限：`96` 字节（与 `CLEONOS_FS_NAME_MAX` 对齐）。

文件系统写入类 syscall 的权限限制：

- `FS_MKDIR` / `FS_WRITE` / `FS_APPEND` / `FS_REMOVE` 不再限制到 `/temp`。
- 仍禁止直接修改根路径 `/` 与动态设备文件（例如 `/dev/fb0`、`/dev/net0`、`/dev/disk0`、`/dev/tty0`、`/dev/input/kbd`、`/dev/input/mouse`）。
- `/proc` 仍为只读虚拟目录，写入类 syscall 不支持修改。
- 已挂载磁盘路径仍走磁盘后端；默认挂载点通常为 `/temp/disk`，但写入权限不再依赖 `/temp` 前缀。

UserSafeController（USC）危险 syscall 确认：

- USC 拦截到危险 syscall 时仍支持“仅本次 / 本会话 / 永久 / 拒绝”四种结果。
- 如果内核窗口管理器已初始化且桌面 TTY 正在前台，确认请求会以置顶桌面弹窗显示；可点击按钮，也可按 `1/O`、`2/S`、`3/P`、`N/Esc/Enter` 选择。
- 如果当前不在桌面环境、键盘被禁用，或弹窗创建失败，则回退到原 TTY/串口确认流程。

`/proc` 虚拟目录（由 syscall 层动态导出）：

- `/proc`：目录（children = `self`、`list`、以及全部 PID 名称）
- `/proc/self`：当前进程快照文本
- `/proc/list`：所有进程列表文本
- `/proc/<pid>`：指定 PID 快照文本
- `/proc` 为只读；写入类 syscall 不支持。

## 4. Syscall 列表（0~129）

### 0 `CLEONOS_SYSCALL_LOG_WRITE`

- 参数：
- `arg0`: `const char *message`
- `arg1`: `u64 length`
- 返回：实际写入长度
- 说明：写入内核日志通道（tag 为 `SYSCALL`），长度会被截断到 191。

### 1 `CLEONOS_SYSCALL_TIMER_TICKS`

- 参数：无
- 返回：系统 timer tick 计数

### 2 `CLEONOS_SYSCALL_TASK_COUNT`

- 参数：无
- 返回：任务总数

### 3 `CLEONOS_SYSCALL_CUR_TASK`

- 参数：无
- 返回：当前任务 ID

### 4 `CLEONOS_SYSCALL_SERVICE_COUNT`

- 参数：无
- 返回：服务总数

### 5 `CLEONOS_SYSCALL_SERVICE_READY_COUNT`

- 参数：无
- 返回：ready 服务数

### 6 `CLEONOS_SYSCALL_CONTEXT_SWITCHES`

- 参数：无
- 返回：上下文切换计数

### 7 `CLEONOS_SYSCALL_KELF_COUNT`

- 参数：无
- 返回：内核态 ELF 应用计数

### 8 `CLEONOS_SYSCALL_KELF_RUNS`

- 参数：无
- 返回：内核态 ELF 累计运行次数

### 9 `CLEONOS_SYSCALL_FS_NODE_COUNT`

- 参数：无
- 返回：VFS 节点总数

### 10 `CLEONOS_SYSCALL_FS_CHILD_COUNT`

- 参数：
- `arg0`: `const char *dir_path`
- 返回：子节点数量
- 说明：当 `dir_path=/proc` 时，返回 `2 + proc_count`（`self`、`list`、PID 子项）。

### 11 `CLEONOS_SYSCALL_FS_GET_CHILD_NAME`

- 参数：
- `arg0`: `const char *dir_path`
- `arg1`: `u64 index`
- `arg2`: `char *out_name`
- 返回：成功 `1`，失败 `0`
- 说明：最多写入 96 字节（含终止符）。
- 说明：当 `dir_path=/proc` 时，`index=0/1` 分别为 `self/list`，后续索引为 PID 文本。

### 12 `CLEONOS_SYSCALL_FS_READ`

- 参数：
- `arg0`: `const char *path`
- `arg1`: `char *out_buffer`
- `arg2`: `u64 buffer_size`
- 返回：实际读取字节数，失败/文件空返回 `0`
- 说明：不会自动追加 `\0`，调用方应自行处理文本终止。
- 说明：支持读取 `/proc/self`、`/proc/list`、`/proc/<pid>`。

### 13 `CLEONOS_SYSCALL_EXEC_PATH`

- 参数：
- `arg0`: `const char *path`
- 返回：
- `-1`：请求失败
- 其他：由内核执行器返回状态（通常 `0` 表示 accepted）

### 14 `CLEONOS_SYSCALL_EXEC_REQUESTS`

- 参数：无
- 返回：执行请求累计数

### 15 `CLEONOS_SYSCALL_EXEC_SUCCESS`

- 参数：无
- 返回：执行成功累计数

### 16 `CLEONOS_SYSCALL_USER_SHELL_READY`

- 参数：无
- 返回：用户 shell ready（1/0）

### 17 `CLEONOS_SYSCALL_USER_EXEC_REQUESTED`

- 参数：无
- 返回：是否发起过用户侧 exec 请求（1/0）

### 18 `CLEONOS_SYSCALL_USER_LAUNCH_TRIES`

- 参数：无
- 返回：用户态启动尝试次数

### 19 `CLEONOS_SYSCALL_USER_LAUNCH_OK`

- 参数：无
- 返回：用户态启动成功次数

### 20 `CLEONOS_SYSCALL_USER_LAUNCH_FAIL`

- 参数：无
- 返回：用户态启动失败次数

### 21 `CLEONOS_SYSCALL_TTY_COUNT`

- 参数：无
- 返回：TTY 总数

### 22 `CLEONOS_SYSCALL_TTY_ACTIVE`

- 参数：无
- 返回：当前 active TTY 索引

### 23 `CLEONOS_SYSCALL_TTY_SWITCH`

- 参数：
- `arg0`: `u64 tty_index`
- 返回：切换后的 active TTY 索引

### 24 `CLEONOS_SYSCALL_TTY_WRITE`

- 参数：
- `arg0`: `const char *text`
- `arg1`: `u64 length`
- 返回：实际写入长度
- 说明：长度会被截断到 2048。

### 25 `CLEONOS_SYSCALL_TTY_WRITE_CHAR`

- 参数：
- `arg0`: `u64 ch`（低 8 位有效）
- 返回：当前实现固定返回 `1`

### 26 `CLEONOS_SYSCALL_KBD_GET_CHAR`

- 参数：无
- 返回：
- 无输入时 `-1`
- 有输入时返回字符值（低 8 位；按当前进程/TTY 上下文读取）

### 27 `CLEONOS_SYSCALL_FS_STAT_TYPE`

- 参数：
- `arg0`: `const char *path`
- 返回：`1=FILE`，`2=DIR`，失败 `-1`
- 说明：`/proc` 返回目录，`/proc/self`、`/proc/list`、`/proc/<pid>` 返回文件。

### 28 `CLEONOS_SYSCALL_FS_STAT_SIZE`

- 参数：
- `arg0`: `const char *path`
- 返回：文件大小；目录通常为 `0`；失败 `-1`
- 说明：`/proc/*` 文件大小按生成文本长度返回。

### 29 `CLEONOS_SYSCALL_FS_MKDIR`

- 参数：
- `arg0`: `const char *path`
- 返回：成功 `1`，失败 `0`
- 说明：创建目录；不允许目标为根路径或动态设备文件。

### 30 `CLEONOS_SYSCALL_FS_WRITE`

- 参数：
- `arg0`: `const char *path`
- `arg1`: `const char *data`
- `arg2`: `u64 size`
- 返回：成功 `1`，失败 `0`
- 说明：覆盖写；不允许目标为根路径、目录或动态设备文件。

### 31 `CLEONOS_SYSCALL_FS_APPEND`

- 参数：
- `arg0`: `const char *path`
- `arg1`: `const char *data`
- `arg2`: `u64 size`
- 返回：成功 `1`，失败 `0`
- 说明：追加写；不允许目标为根路径、目录或动态设备文件。

### 32 `CLEONOS_SYSCALL_FS_REMOVE`

- 参数：
- `arg0`: `const char *path`
- 返回：成功 `1`，失败 `0`
- 说明：删除文件或空目录；不允许删除根路径或动态设备文件，目录需为空。

### 33 `CLEONOS_SYSCALL_LOG_JOURNAL_COUNT`

- 参数：无
- 返回：日志 journal 条目数量

### 34 `CLEONOS_SYSCALL_LOG_JOURNAL_READ`

- 参数：
- `arg0`: `u64 index_from_oldest`
- `arg1`: `char *out_line`
- `arg2`: `u64 out_size`
- 返回：成功 `1`，失败 `0`
- 说明：内核会保证输出字符串有 `\0` 终止。

### 35 `CLEONOS_SYSCALL_KBD_BUFFERED`

- 参数：无
- 返回：当前键盘队列中的字符数量

### 36 `CLEONOS_SYSCALL_KBD_PUSHED`

- 参数：无
- 返回：键盘累计入队计数

### 37 `CLEONOS_SYSCALL_KBD_POPPED`

- 参数：无
- 返回：键盘累计出队计数

### 38 `CLEONOS_SYSCALL_KBD_DROPPED`

- 参数：无
- 返回：键盘队列溢出丢弃计数

### 39 `CLEONOS_SYSCALL_KBD_HOTKEY_SWITCHES`

- 参数：无
- 返回：ALT+F1..F4 热键切换计数

### 40 `CLEONOS_SYSCALL_GETPID`

- 参数：无
- 返回：当前进程 PID（无活动进程时为 `0`）

### 41 `CLEONOS_SYSCALL_SPAWN_PATH`

- 参数：
- `arg0`: `const char *path`
- 返回：
- 成功：子进程 PID
- 失败：`-1`
- 说明：当前实现为异步 spawn（进入 pending 队列，后续由调度 tick 派发执行）。

### 42 `CLEONOS_SYSCALL_WAITPID`

- 参数：
- `arg0`: `u64 pid`
- `arg1`: `u64 *out_status`（可为 `0`）
- 返回：
- `-1`：PID 不存在
- `0`：目标进程仍未退出（`PENDING` / `RUNNING` / `STOPPED`）
- `1`：目标进程已退出
- 说明：当返回 `1` 且 `arg1!=0` 时，会写入退出码。

### 43 `CLEONOS_SYSCALL_EXIT`

- 参数：
- `arg0`: `u64 status`
- 返回：
- `1`：已记录退出请求
- `0`：当前上下文不支持退出请求

### 44 `CLEONOS_SYSCALL_SLEEP_TICKS`

- 参数：
- `arg0`: `u64 ticks`
- 返回：实际休眠 tick 数

### 45 `CLEONOS_SYSCALL_YIELD`

- 参数：无
- 返回：当前 tick

### 46 `CLEONOS_SYSCALL_SHUTDOWN`

- 参数：无
- 返回：理论上不返回；成功路径会触发关机流程（当前 x86_64 走 QEMU/ACPI 关机端口）
- 说明：若关机流程未生效，内核会进入 halt 循环。

### 47 `CLEONOS_SYSCALL_RESTART`

- 参数：无
- 返回：理论上不返回；成功路径会触发重启流程（当前 x86_64 走 8042 reset）
- 说明：若重启流程未生效，内核会进入 halt 循环。

### 48 `CLEONOS_SYSCALL_AUDIO_AVAILABLE`

- 参数：无
- 返回：
- `1`：音频输出可用
- `0`：当前平台无音频输出

### 49 `CLEONOS_SYSCALL_AUDIO_PLAY_TONE`

- 参数：
- `arg0`: `u64 hz`（频率，`0` 表示静音等待）
- `arg1`: `u64 ticks`（持续 tick）
- 返回：成功 `1`，失败 `0`
- 说明：当前实现基于 PC Speaker（x86_64），用于最小音频链路。

### 50 `CLEONOS_SYSCALL_AUDIO_STOP`

- 参数：无
- 返回：当前实现固定返回 `1`
- 说明：立即停止当前音频输出。

### 51 `CLEONOS_SYSCALL_EXEC_PATHV`

- 参数：
- `arg0`: `const char *path`
- `arg1`: `const char *argv_line`（可为 `0`）
- `arg2`: `const char *env_line`（可为 `0`）
- 返回：
- `-1`：请求失败
- 其他：目标程序退出状态
- 说明：`argv_line` 以空白分词，`env_line` 以 `;` 或换行分隔条目。

### 52 `CLEONOS_SYSCALL_SPAWN_PATHV`

- 参数：
- `arg0`: `const char *path`
- `arg1`: `const char *argv_line`（可为 `0`）
- `arg2`: `const char *env_line`（可为 `0`）
- 返回：成功为子进程 PID，失败 `-1`

### 53 `CLEONOS_SYSCALL_PROC_ARGC`

- 参数：无
- 返回：当前进程 `argc`

### 54 `CLEONOS_SYSCALL_PROC_ARGV`

- 参数：
- `arg0`: `u64 index`
- `arg1`: `char *out_value`
- `arg2`: `u64 out_size`
- 返回：成功 `1`，失败 `0`
- 说明：单条参数字符串最大写入 `128` 字节（含终止符）。

### 55 `CLEONOS_SYSCALL_PROC_ENVC`

- 参数：无
- 返回：当前进程 `envc`

### 56 `CLEONOS_SYSCALL_PROC_ENV`

- 参数：
- `arg0`: `u64 index`
- `arg1`: `char *out_value`
- `arg2`: `u64 out_size`
- 返回：成功 `1`，失败 `0`
- 说明：单条环境变量字符串最大写入 `128` 字节（含终止符）。

### 57 `CLEONOS_SYSCALL_PROC_LAST_SIGNAL`

- 参数：无
- 返回：当前进程最近一次异常映射的信号号（无则 `0`）

### 58 `CLEONOS_SYSCALL_PROC_FAULT_VECTOR`

- 参数：无
- 返回：当前进程最近一次 CPU 异常向量号（无则 `0`）

### 59 `CLEONOS_SYSCALL_PROC_FAULT_ERROR`

- 参数：无
- 返回：当前进程最近一次 CPU 异常错误码（无则 `0`）

### 60 `CLEONOS_SYSCALL_PROC_FAULT_RIP`

- 参数：无
- 返回：当前进程最近一次 CPU 异常 RIP（无则 `0`）

### 61 `CLEONOS_SYSCALL_PROC_COUNT`

- 参数：无
- 返回：当前进程表中已使用槽位数量

### 62 `CLEONOS_SYSCALL_PROC_PID_AT`

- 参数：
- `arg0`: `u64 index`
- `arg1`: `u64 *out_pid`
- 返回：成功 `1`，失败 `0`
- 说明：用于枚举进程；`index` 超出范围返回 `0`。

### 63 `CLEONOS_SYSCALL_PROC_SNAPSHOT`

- 参数：
- `arg0`: `u64 pid`
- `arg1`: `struct cleonos_proc_snapshot *out_snapshot`
- `arg2`: `u64 out_size`（需 `>= sizeof(cleonos_proc_snapshot)`）
- 返回：成功 `1`，失败 `0`
- 说明：返回 PID/PPID/状态（含 `STOPPED`）/运行 tick/内存估算/TTY/路径等快照信息。

### 64 `CLEONOS_SYSCALL_PROC_KILL`

- 参数：
- `arg0`: `u64 pid`
- `arg1`: `u64 signal`（`0` 时按 `SIGTERM(15)` 处理）
- 返回：
- `1`：请求成功
- `0`：当前不可终止（例如非当前上下文中的 running 进程）
- `-1`：PID 不存在
- 语义：
- `SIGTERM`/`SIGKILL`（以及其它非 STOP/CONT 信号）：终止目标进程。
- `SIGSTOP`：将 `PENDING` 进程置为 `STOPPED`；对已 `STOPPED` 目标返回成功。
- `SIGCONT`：将 `STOPPED` 进程恢复为 `PENDING`。

### 65 `CLEONOS_SYSCALL_KDBG_SYM`

- 参数：
- `arg0`: `u64 addr`
- `arg1`: `char *out_line`
- `arg2`: `u64 out_size`
- 返回：写入字节数（含截断）
- 说明：将地址符号化为文本（含偏移与可选源码位置）。

### 66 `CLEONOS_SYSCALL_KDBG_BT`

- 参数：
- `arg0`: `struct { u64 rbp; u64 rip; u64 out_ptr; u64 out_size; } *req`
- 返回：写入字节数
- 说明：输出回溯文本；x86_64 下会尝试沿帧指针遍历。

### 67 `CLEONOS_SYSCALL_KDBG_REGS`

- 参数：
- `arg0`: `char *out_text`
- `arg1`: `u64 out_size`
- 返回：写入字节数
- 说明：输出最近一次 syscall 进入内核时的寄存器快照。

### 68 `CLEONOS_SYSCALL_STATS_TOTAL`

- 参数：无
- 返回：自启动以来的 syscall 总调用次数

### 69 `CLEONOS_SYSCALL_STATS_ID_COUNT`

- 参数：
- `arg0`: `u64 id`
- 返回：指定 syscall ID 的累计调用次数（ID 越界返回 `0`）

### 70 `CLEONOS_SYSCALL_STATS_RECENT_WINDOW`

- 参数：无
- 返回：最近窗口内样本数量
- 说明：当前内核实现窗口大小为 `256` 次 syscall。

### 71 `CLEONOS_SYSCALL_STATS_RECENT_ID`

- 参数：
- `arg0`: `u64 id`
- 返回：指定 syscall ID 在“最近窗口”中的出现次数（ID 越界返回 `0`）

### 72 `CLEONOS_SYSCALL_FD_OPEN`

- 参数：
- `arg0`: `const char *path`
- `arg1`: `u64 flags`
- `arg2`: `u64 mode`（当前保留）
- 返回：成功返回 `fd`，失败返回 `-1`
- 说明：
- 当前支持普通文件与设备文件：`/dev/tty`、`/dev/tty0`、`/dev/null`、`/dev/zero`、`/dev/random`、`/dev/urandom`、`/dev/fb0`、`/dev/input/kbd`、`/dev/input/mouse`、`/dev/net0`、`/dev/disk0`。
- 默认进程会预置 `fd 0/1/2`（stdin/stdout/stderr）。
- 标志位兼容子集：`O_RDONLY/O_WRONLY/O_RDWR/O_CREAT/O_TRUNC/O_APPEND`。

### 73 `CLEONOS_SYSCALL_FD_READ`

- 参数：
- `arg0`: `u64 fd`
- `arg1`: `void *out_buffer`
- `arg2`: `u64 size`
- 返回：读取字节数；错误返回 `-1`
- 说明：
- 对 `tty fd`（如 stdin）为非阻塞读取：无输入时返回 `0`。
- 对文件 fd 为顺序读取，内部维护偏移。

### 74 `CLEONOS_SYSCALL_FD_WRITE`

- 参数：
- `arg0`: `u64 fd`
- `arg1`: `const void *buffer`
- `arg2`: `u64 size`
- 返回：写入字节数；错误返回 `-1`
- 说明：
- `tty fd` 输出到终端。
- 文件 fd 支持顺序写；`O_APPEND` 下始终追加。

### 75 `CLEONOS_SYSCALL_FD_CLOSE`

- 参数：
- `arg0`: `u64 fd`
- 返回：成功 `0`，失败 `-1`

### 76 `CLEONOS_SYSCALL_FD_DUP`

- 参数：
- `arg0`: `u64 oldfd`
- 返回：新 fd；失败 `-1`
- 说明：当前为“按值复制”语义（复制 flags/offset/目标对象）。

### 77 `CLEONOS_SYSCALL_DL_OPEN`

- 参数：
- `arg0`: `const char *path`
- 返回：动态库句柄（`handle`）；失败 `-1`
- 说明：将用户态动态库（ELF）加载到当前进程地址空间，供 `DL_SYM` 查询符号。

### 78 `CLEONOS_SYSCALL_DL_CLOSE`

- 参数：
- `arg0`: `u64 handle`
- 返回：成功 `0`，失败 `-1`
- 说明：关闭/释放由 `DL_OPEN` 返回的动态库句柄。

### 79 `CLEONOS_SYSCALL_DL_SYM`

- 参数：
- `arg0`: `u64 handle`
- `arg1`: `const char *symbol`
- 返回：符号地址（`u64`）；失败返回 `0`
- 说明：用于查询库导出符号入口地址。

### 80 `CLEONOS_SYSCALL_EXEC_PATHV_IO`

- 参数：
- `arg0`: `const char *path`
- `arg1`: `const char *argv_line`（可为 `0`）
- `arg2`: `struct { u64 env_line_ptr; u64 stdin_fd; u64 stdout_fd; u64 stderr_fd; } *req`
- 返回：与 `EXEC_PATHV` 一致（成功返回子进程退出状态，失败 `-1`）
- 说明：在 `EXEC_PATHV` 基础上增加 I/O 句柄重定向（用于管道与重定向）。

### 81 `CLEONOS_SYSCALL_FB_INFO`

- 参数：
- `arg0`: `cleonos_fb_info *out_info`
- 返回：成功 `1`，失败 `0`
- 说明：获取 framebuffer 信息（`width/height/pitch/bpp`）。`out_info` 为空或 framebuffer 未就绪时失败。

### 82 `CLEONOS_SYSCALL_FB_BLIT`

- 参数：
- `arg0`: `const cleonos_fb_blit_req *req`
- 返回：成功 `1`，失败 `0`
- 说明：
- 从用户态 `pixels_ptr` 指向的 32bpp 源缓冲拷贝到 framebuffer。
- `src_pitch_bytes=0` 时按 `src_width*4` 推导。
- 当前实现限制：`src_width<=4096`、`src_height<=4096`、`scale<=8`，且目标坐标需落在屏幕内。

### 83 `CLEONOS_SYSCALL_FB_CLEAR`

- 参数：
- `arg0`: `u64 rgb`（低 32 位有效）
- 返回：成功 `1`，失败 `0`
- 说明：用纯色清屏。

### 84 `CLEONOS_SYSCALL_KERNEL_VERSION`

- 参数：
- `arg0`: `char *out_version`
- `arg1`: `u64 out_size`
- 返回：实际写入字节数（不含终止符），失败返回 `0`
- 说明：返回 CLKS 内核版本字符串（当前默认 `1.0.0-alpha`），内核会保证输出以 `\0` 结尾。

### 85 `CLEONOS_SYSCALL_DISK_PRESENT`

- 参数：无
- 返回：`1` 表示存在可用磁盘后端，`0` 表示不存在。
- 说明：在 QEMU 运行时，磁盘后端来自模拟物理硬盘（`-drive`），不是 ISO/Limine module。

### 86 `CLEONOS_SYSCALL_DISK_SIZE_BYTES`

- 参数：无
- 返回：磁盘容量（字节）；无磁盘时返回 `0`。

### 87 `CLEONOS_SYSCALL_DISK_SECTOR_COUNT`

- 参数：无
- 返回：扇区总数（按 512 字节扇区）；无磁盘时返回 `0`。

### 88 `CLEONOS_SYSCALL_DISK_FORMATTED`

- 参数：无
- 返回：`1` 表示当前磁盘已识别为 FAT32，`0` 表示未格式化/不识别。

### 89 `CLEONOS_SYSCALL_DISK_FORMAT_FAT32`

- 参数：
- `arg0`: `const char *label`（可为 `0` 或空字符串）
- 返回：成功 `1`，失败 `0`
- 说明：将当前磁盘格式化为 FAT32；此操作会清空已有磁盘数据。该 syscall 在 USC 策略中默认视为高风险操作。

### 90 `CLEONOS_SYSCALL_DISK_MOUNT`

- 参数：
- `arg0`: `const char *mount_path`（绝对路径）
- 返回：成功 `1`，失败 `0`
- 说明：将 FAT32 磁盘挂载到指定路径（不能是 `/`）。挂载成功后，`FS_*` 接口可通过该路径访问磁盘内容。

### 91 `CLEONOS_SYSCALL_DISK_MOUNTED`

- 参数：无
- 返回：`1` 表示已挂载，`0` 表示未挂载。

### 92 `CLEONOS_SYSCALL_DISK_MOUNT_PATH`

- 参数：
- `arg0`: `char *out_path`
- `arg1`: `u64 out_size`
- 返回：实际写入字节数（不含终止符），失败返回 `0`
- 说明：查询当前挂载点路径；返回值为写入长度，且内核保证 `\0` 终止。

### 93 `CLEONOS_SYSCALL_DISK_READ_SECTOR`

- 参数：
- `arg0`: `u64 lba`
- `arg1`: `void *out_sector`（512 字节缓冲区）
- 返回：成功 `1`，失败 `0`
- 说明：按 LBA 读取单个扇区（512B）到用户缓冲区，常用于分区表/低级磁盘工具。

### 94 `CLEONOS_SYSCALL_DISK_WRITE_SECTOR`

- 参数：
- `arg0`: `u64 lba`
- `arg1`: `const void *sector_data`（512 字节缓冲区）
- 返回：成功 `1`，失败 `0`
- 说明：按 LBA 写入单个扇区（512B）。该 syscall 在 USC 策略中默认视为高风险操作。

### 95 `CLEONOS_SYSCALL_NET_AVAILABLE`

- 参数：无
- 返回：`1` 表示网络栈可用（e1000 初始化成功），`0` 表示不可用。

### 96 `CLEONOS_SYSCALL_NET_IPV4_ADDR`

- 参数：无
- 返回：当前 IPv4 地址（`u32`，以网络字节序放在返回值低 32 位）。
- 说明：优先使用 DHCP 自动获取地址；若 DHCP 失败回退到 `10.0.2.15`（QEMU usernet 常见默认值）。

### 97 `CLEONOS_SYSCALL_NET_PING`

- 参数：
- `arg0`: `u64 dst_ipv4_be`（目标 IPv4，网络字节序）
- `arg1`: `u64 poll_budget`（轮询预算；`0` 使用内核默认）
- 返回：成功收到 echo reply 返回 `1`，否则返回 `0`。

### 98 `CLEONOS_SYSCALL_NET_UDP_SEND`

- 参数：
- `arg0`: `const struct { u64 dst_ipv4_be; u64 dst_port; u64 src_port; u64 payload_ptr; u64 payload_len; } *req`
- 返回：实际发送 payload 字节数；失败返回 `0`。
- 说明：当前实现为最小 UDP 能力，`payload_len` 建议不超过 MTU 可承载范围。

### 99 `CLEONOS_SYSCALL_NET_UDP_RECV`

- 参数：
- `arg0`: `struct { u64 out_payload_ptr; u64 payload_capacity; u64 out_src_ipv4_ptr; u64 out_src_port_ptr; u64 out_dst_port_ptr; } *req`
- 返回：实际拷贝到 `out_payload_ptr` 的字节数；无数据/失败返回 `0`。
- 说明：`out_src_*`/`out_dst_*` 指针可为 `0`（表示不关心该字段）。

### 100 `CLEONOS_SYSCALL_NET_NETMASK`

- 参数：无
- 返回：当前 IPv4 子网掩码（`u32`，网络字节序，位于返回值低 32 位）。
- 说明：优先使用 DHCP 下发值；若 DHCP 未提供则按回退策略填充。

### 101 `CLEONOS_SYSCALL_NET_GATEWAY`

- 参数：无
- 返回：当前 IPv4 默认网关（`u32`，网络字节序，位于返回值低 32 位）。
- 说明：优先使用 DHCP 下发值；若 DHCP 未提供则按回退策略填充。

### 102 `CLEONOS_SYSCALL_NET_DNS_SERVER`

- 参数：无
- 返回：当前 DNS 服务器 IPv4（`u32`，网络字节序，位于返回值低 32 位）。
- 说明：优先使用 DHCP 下发值；若 DHCP 未提供则按回退策略填充。

### 103 `CLEONOS_SYSCALL_NET_TCP_CONNECT`

- 参数：
- `arg0`: `const struct { u64 dst_ipv4_be; u64 dst_port; u64 src_port; u64 poll_budget; } *req`
- 返回：连接成功返回 `1`，失败返回 `0`。
- 说明：最小 TCP 客户端连接能力（SYN/SYN-ACK/ACK）；`src_port=0` 时内核自动分配临时端口。

### 104 `CLEONOS_SYSCALL_NET_TCP_SEND`

- 参数：
- `arg0`: `const struct { u64 payload_ptr; u64 payload_len; u64 poll_budget; } *req`
- 返回：实际发送并确认的字节数；失败返回 `0`。
- 说明：当前实现为单连接发送路径，内部按分片发送并等待 ACK。

### 105 `CLEONOS_SYSCALL_NET_TCP_RECV`

- 参数：
- `arg0`: `struct { u64 out_payload_ptr; u64 payload_capacity; u64 poll_budget; } *req`
- 返回：实际拷贝到用户缓冲区的字节数；无数据/超时/失败返回 `0`。
- 说明：接收缓冲区上限受内核实现限制（当前 64 KiB）。

### 106 `CLEONOS_SYSCALL_NET_TCP_CLOSE`

- 参数：
- `arg0`: `u64 poll_budget`
- 返回：关闭成功返回 `1`，失败返回 `0`。
- 说明：最小关闭流程支持 FIN/ACK 轮询等待，超时会强制回收连接状态。

### 107 `CLEONOS_SYSCALL_MOUSE_STATE`

- 参数：
- `arg0`: `struct { u64 x; u64 y; u64 buttons; u64 packet_count; u64 ready; } *out_state`
- 返回：成功返回 `1`，失败返回 `0`。
- 说明：
- 读取当前鼠标状态快照，坐标为 framebuffer 像素坐标系（左上角为原点）。
- `buttons` 位掩码：`bit0=left`、`bit1=right`、`bit2=middle`。
- `ready=1` 表示鼠标设备已上线；`ready=0` 时坐标/按键可能为默认值。

### 108 `CLEONOS_SYSCALL_WM_CREATE`

- 参数：
- `arg0`: `const struct { u64 x; u64 y; u64 width; u64 height; u64 flags; } *req`
- 返回：成功返回非零 `window_id`，失败返回 `0`
- 说明：
- 创建一个由内核合成的窗口（当前在 TTY2 桌面合成器上显示）。
- `x/y` 为有符号坐标（按 `i64` 解释），`width/height` 为像素尺寸。

### 109 `CLEONOS_SYSCALL_WM_DESTROY`

- 参数：
- `arg0`: `u64 window_id`
- 返回：成功 `1`，失败 `0`
- 说明：销毁窗口并回收其事件队列与内核缓冲。

### 110 `CLEONOS_SYSCALL_WM_PRESENT`

- 参数：
- `arg0`: `const struct { u64 window_id; u64 pixels_ptr; u64 src_width; u64 src_height; u64 src_pitch_bytes; } *req`
- 返回：成功 `1`，失败 `0`
- 说明：
- 将用户态 ARGB/RGB32 像素缓冲提交到指定窗口内容区。
- 当前要求 `src_width/src_height` 与创建窗口时一致。

### 111 `CLEONOS_SYSCALL_WM_POLL_EVENT`

- 参数：
- `arg0`: `u64 window_id`
- `arg1`: `struct cleonos_wm_event *out_event`
- 返回：有事件时返回 `1` 并写入事件；无事件或失败返回 `0`
- 事件类型：
- `1` = `FOCUS_GAINED`
- `2` = `FOCUS_LOST`
- `3` = `KEY`（`arg0` 为按键值）
- `4` = `MOUSE_MOVE`（`arg0/arg1` 为全局坐标，`arg2/arg3` 为窗口局部坐标）
- `5` = `MOUSE_BUTTON`（`arg0` 为按钮状态位掩码，`arg1` 为变化掩码）

### 112 `CLEONOS_SYSCALL_WM_MOVE`

- 参数：
- `arg0`: `const struct { u64 window_id; u64 x; u64 y; } *req`
- 返回：成功 `1`，失败 `0`
- 说明：移动窗口到目标坐标（坐标按 `i64` 解释，内核会进行边界裁剪）。

### 113 `CLEONOS_SYSCALL_WM_SET_FOCUS`

- 参数：
- `arg0`: `u64 window_id`
- 返回：成功 `1`，失败 `0`
- 说明：将目标窗口置为焦点并提升到顶层 z-order。该操作允许 UWM/任务栏聚焦其他进程创建的窗口；窗口内容提交、移动、销毁仍由窗口拥有者限制。

### 114 `CLEONOS_SYSCALL_WM_SET_FLAGS`

- 参数：
- `arg0`: `u64 window_id`
- `arg1`: `u64 flags`
- 返回：成功 `1`，失败 `0`
- 说明：
- 设置窗口协议标志；当前支持 `CLEONOS_WM_FLAG_TOPMOST`（`bit0`），置位后窗口保持在普通窗口之上。

### 115 `CLEONOS_SYSCALL_WM_RESIZE`

- 参数：
- `arg0`: `const struct { u64 window_id; u64 width; u64 height; } *req`
- 返回：成功 `1`，失败 `0`
- 说明：
- 调整窗口尺寸并保持 `window_id` 不变；调整后用户态应重新 `WM_PRESENT` 一次提交新尺寸内容。

### 116 `CLEONOS_SYSCALL_PTY_OPEN`

- 参数：无
- 返回：成功返回一个可读写 FD，失败返回 `(u64)-1`
- 说明：
- 创建桌面伪 tty 输出端。用户态可把该 FD 传给 `EXEC_PATHV_IO` 的 stdout/stderr，然后通过 `FD_READ` 从同一 FD 读取子进程输出。
- 当前 PTY 是“命令输出捕获型”最小实现，不提供完整主从终端会话语义；阻塞式交互 shell 仍应继续使用普通 TTY。

### 117 `CLEONOS_SYSCALL_WM_COUNT`

- 参数：无
- 返回：当前内核 WM 窗口数量。
- 说明：窗口列表按当前 z-order 枚举，主要给 UWM、Task Manager 这类桌面组件做窗口/进程关联。

### 118 `CLEONOS_SYSCALL_WM_ID_AT`

- 参数：
- `arg0`: `u64 index`
- `arg1`: `u64 *out_window_id`
- 返回：成功返回 `1`，失败返回 `0`。
- 说明：按 z-order 索引读取窗口 ID；`index` 范围为 `[0, WM_COUNT)`。

### 119 `CLEONOS_SYSCALL_WM_SNAPSHOT`

- 参数：
- `arg0`: `u64 window_id`
- `arg1`: `struct cleonos_wm_snapshot *out_snapshot`
- `arg2`: `u64 out_size`
- 返回：成功返回 `1`，失败返回 `0`。
- 快照结构：

```c
typedef struct cleonos_wm_snapshot {
    u64 window_id;
    u64 owner_pid;
    u64 flags;
    u64 x;
    u64 y;
    u64 width;
    u64 height;
    u64 focused;
    u64 presented;
    u64 event_count;
} cleonos_wm_snapshot;
```

- 说明：`owner_pid` 是创建窗口的用户进程 PID；内核仍会限制窗口修改/销毁等操作只能由窗口拥有者执行。

### 120 `CLEONOS_SYSCALL_USER_HEAP_ALLOC`

- 参数：
- `arg0`: `u64 size`
- 返回：成功返回当前进程可读写的 heap 块指针，失败返回 `0`。
- 说明：
- 这是当前 CLeonOS 用户态执行模型下的动态 heap 后端。用户态 `malloc/calloc/realloc/free` 会按需调用它申请大块内存，然后在用户态块内做 free-list 管理。
- 内核按进程追踪这些块，进程退出、异常终止或被 kill 时统一释放。
- 单次申请上限当前为 `4 MiB`；需要更大内存时用户态 allocator 会多次申请块。
- 因为当前用户态 ELF 仍是内核直接加载/调用模型，这不是完整 POSIX `brk/mmap`，但可以避免把几 MiB heap 静态放进 ELF `.bss` 导致 `ELF LOAD ALLOC FAILED`。

### 121 `CLEONOS_SYSCALL_DRIVER_COUNT`

- 参数：无
- 返回：当前 driver model 登记的驱动数量。
- 说明：包括启动时自动扫描到的标准 `/driver/*.elf` 驱动，以及之后手动加载的第三方 `/driver/*.elf` 驱动。当前标准驱动也不再登记为 builtin 表项。

### 122 `CLEONOS_SYSCALL_DRIVER_INFO`

- 参数：
- `arg0`: `u64 index`
- `arg1`: `struct cleonos_driver_info *out_info`
- `arg2`: `u64 out_size`
- 返回：成功返回 `1`，失败返回 `0`。
- 说明：按索引读取驱动快照。结构包含 `name/path/kind/state/driver_class/from_elf/image_size/elf_entry/load_id/owner_pid`。

### 123 `CLEONOS_SYSCALL_DRIVER_LOAD`

- 参数：
- `arg0`: `const char *path`
- 返回：成功返回 `load_id`，失败返回 `0`。
- 说明：校验 `/driver/*.elf`，登记到 driver model，并以普通用户态进程方式启动该驱动 ELF。当前不会把第三方 ELF 直接跳入内核态执行。

### 124 `CLEONOS_SYSCALL_DRIVER_UNLOAD`

- 参数：
- `arg0`: `const char *name_or_path`
- 返回：成功返回 `1`，失败返回 `0`。
- 说明：仅支持卸载 ELF 驱动。卸载会终止该驱动对应的 `owner_pid`，并将 driver model 条目标记为 `unloaded`。

### 125 `CLEONOS_SYSCALL_DRIVER_RELOAD`

- 参数：无
- 返回：本次从 `/driver` 新加载的驱动数量。
- 说明：重新扫描 `/driver/*.elf`，已登记的驱动会被跳过。

### 126 `CLEONOS_SYSCALL_TIMER_HZ`

- 参数：无
- 返回：系统 timer 每秒 tick 数；失败或未初始化时返回 `0`。
- 说明：用户态封装为 `cleonos_sys_timer_hz()`。配合 `TIMER_TICKS` 可把 tick 转成秒/毫秒。

### 127 `CLEONOS_SYSCALL_TIME_MS`

- 参数：无
- 返回：自系统启动以来的毫秒数。
- 说明：内核按 `timer_ticks * 1000 / timer_hz` 计算；若 `timer_hz=0` 则返回 `0`。

### 128 `CLEONOS_SYSCALL_SLEEP_MS`

- 参数：
- `arg0`: `u64 ms`
- 返回：实际换算并休眠的 tick 数。
- 说明：用户态封装为 `cleonos_sys_sleep_ms()`；底层仍走调度器 sleep/yield 机制，不是忙等。

### 129 `CLEONOS_SYSCALL_NET_TCP_LAST_ERROR`

- 参数：无
- 返回：最近一次 TCP connect 失败原因码。
- 说明：用户态封装为 `cleonos_sys_net_tcp_last_error()`，主要用于 `wget/pkg/browser` 这类网络工具打印更具体的错误。
- 当前错误码约定：
- `0` = 无错误
- `1` = 网络不可用
- `2` = 地址或端口非法
- `3` = ARP/网关解析失败
- `4` = SYN 发送失败
- `5` = 连接被 reset/refused
- `6` = SYN-ACK 超时
- `7` = 收到旧连接的 stale ACK

## 4.1 `/dev` 设备文件

- `/dev/fb0`：`FD_READ` 返回 framebuffer 信息；`FD_WRITE` 支持 `clear RRGGBB` 或写入整屏 RGBA buffer。
- `/dev/input/kbd`：`FD_READ` 从当前 TTY 键盘队列读取字节，非阻塞。
- `/dev/input/mouse`：`FD_READ` 返回鼠标 ready/x/y/buttons/packets 文本快照。
- `/dev/net0`：`FD_READ` 返回 available/ipv4/netmask/gateway/dns 文本快照；`FD_WRITE` 触发一次网络 poll 并返回写入长度。
- `/dev/disk0`：`FD_READ` 返回 present/bytes/sectors/fat32/mounted 文本快照。
- `/dev/tty0`：固定访问 TTY0；`/dev/tty` 仍表示当前进程所在 TTY。

## 5. 用户态封装函数

用户态封装位于：

- `cleonos/c/src/syscall.c`

常用封装示例：

- `cleonos_sys_fs_read()`
- `cleonos_sys_fs_write()` / `cleonos_sys_fs_append()` / `cleonos_sys_fs_remove()`
- `cleonos_sys_log_journal_count()` / `cleonos_sys_log_journal_read()`
- `cleonos_sys_timer_ticks()` / `cleonos_sys_timer_hz()` / `cleonos_sys_time_ms()`
- `cleonos_sys_exec_path()`
- `cleonos_sys_exec_pathv()`
- `cleonos_sys_tty_write()`
- `cleonos_sys_kbd_get_char()` / `cleonos_sys_kbd_buffered()`
- `cleonos_sys_getpid()` / `cleonos_sys_spawn_path()` / `cleonos_sys_wait_pid()`
- `cleonos_sys_wm_count()` / `cleonos_sys_wm_id_at()` / `cleonos_sys_wm_snapshot()`
- `cleonos_sys_user_heap_alloc()`
- `cleonos_sys_driver_count()` / `cleonos_sys_driver_info()` / `cleonos_sys_driver_load()` / `cleonos_sys_driver_unload()` / `cleonos_sys_driver_reload()`
- `cleonos_sys_spawn_pathv()`
- `cleonos_sys_exit()` / `cleonos_sys_sleep_ticks()` / `cleonos_sys_sleep_ms()` / `cleonos_sys_yield()` / `cleonos_sys_shutdown()` / `cleonos_sys_restart()`
- `cleonos_sys_audio_available()` / `cleonos_sys_audio_play_tone()` / `cleonos_sys_audio_stop()`
- `cleonos_sys_proc_argc()` / `cleonos_sys_proc_argv()` / `cleonos_sys_proc_envc()` / `cleonos_sys_proc_env()`
- `cleonos_sys_proc_last_signal()` / `cleonos_sys_proc_fault_vector()` / `cleonos_sys_proc_fault_error()` / `cleonos_sys_proc_fault_rip()`
- `cleonos_sys_proc_count()` / `cleonos_sys_proc_pid_at()` / `cleonos_sys_proc_snapshot()` / `cleonos_sys_proc_kill()`
- `cleonos_sys_kdbg_sym()` / `cleonos_sys_kdbg_bt()` / `cleonos_sys_kdbg_regs()`
- `cleonos_sys_stats_total()` / `cleonos_sys_stats_id_count()` / `cleonos_sys_stats_recent_window()` / `cleonos_sys_stats_recent_id()`
- `cleonos_sys_fd_open()` / `cleonos_sys_fd_read()` / `cleonos_sys_fd_write()` / `cleonos_sys_fd_close()` / `cleonos_sys_fd_dup()`
- `cleonos_sys_dl_open()` / `cleonos_sys_dl_close()` / `cleonos_sys_dl_sym()`
- `cleonos_sys_exec_pathv_io()`
- `cleonos_sys_fb_info()` / `cleonos_sys_fb_blit()` / `cleonos_sys_fb_clear()`
- `cleonos_sys_kernel_version()`
- `cleonos_sys_disk_present()` / `cleonos_sys_disk_size_bytes()` / `cleonos_sys_disk_sector_count()`
- `cleonos_sys_disk_formatted()` / `cleonos_sys_disk_format_fat32()` / `cleonos_sys_disk_mount()` / `cleonos_sys_disk_mounted()` / `cleonos_sys_disk_mount_path()`
- `cleonos_sys_disk_read_sector()` / `cleonos_sys_disk_write_sector()`
- `cleonos_sys_net_available()` / `cleonos_sys_net_ipv4_addr()` / `cleonos_sys_net_netmask()` / `cleonos_sys_net_gateway()` / `cleonos_sys_net_dns_server()` / `cleonos_sys_net_ping()`
- `cleonos_sys_net_udp_send()` / `cleonos_sys_net_udp_recv()`
- `cleonos_sys_net_tcp_connect()` / `cleonos_sys_net_tcp_send()` / `cleonos_sys_net_tcp_recv()` / `cleonos_sys_net_tcp_close()` / `cleonos_sys_net_tcp_last_error()`
- `cleonos_sys_mouse_state()`
- `cleonos_sys_wm_create()` / `cleonos_sys_wm_destroy()` / `cleonos_sys_wm_present()`
- `cleonos_sys_wm_poll_event()` / `cleonos_sys_wm_move()` / `cleonos_sys_wm_set_focus()` / `cleonos_sys_wm_set_flags()` / `cleonos_sys_wm_resize()`
- `cleonos_sys_pty_open()`

## 6. 开发注意事项

- 传入的字符串/缓冲指针目前按“同地址空间可直接访问”模型处理，后续若引入严格用户态地址隔离，需要补充用户内存校验。
- `FS_READ` 不保证文本终止符；读取文本请预留 1 字节并手动 `buf[n] = '\0'`。
- `FS_WRITE`/`FS_APPEND` 不再限制到 `/temp`；大数据写入由内核自动分块处理。
- `/proc` 由 syscall 层虚拟导出，不占用 RAMDISK 节点，也不能通过写入类 syscall 修改。

## 7. Wine 兼容说明

- `wine/cleonos_wine_lib/runner.py` 当前已覆盖到 `0..125`（含 `DL_*`、`FB_*`、`KERNEL_VERSION`、`DISK_*`、`NET_*`、`MOUSE_STATE`、`WM_*`、`PTY_OPEN`、`USER_HEAP_ALLOC`、`DRIVER_*`）。
- `126..129`（`TIMER_HZ`、`TIME_MS`、`SLEEP_MS`、`NET_TCP_LAST_ERROR`）目前是 CLKS 内核/用户态头文件已定义的 syscall；Wine 常量和 runner 尚未同步覆盖这些 ID。
- `DL_*`（`77..79`）在 Wine 中为“可运行兼容”实现：
- `DL_OPEN`：加载 guest ELF 到当前 Unicorn 地址空间，返回稳定 `handle`，并做引用计数。
- `DL_SYM`：解析 ELF `SYMTAB/DYNSYM` 并返回 guest 可调用地址。
- `DL_CLOSE`：引用计数归零后释放句柄。
- `DL_*` 兼容限制：未实现完整动态链接器语义（例如完整重定位/依赖库链），但对 CLeonOS 现有用户态库调用场景可工作。
- framebuffer syscall（`81..83`）在 Wine 中已实现兼容：
- `FB_INFO` 返回 framebuffer 参数（默认 `1280x800x32`，可用环境变量 `CLEONOS_WINE_FB_WIDTH/HEIGHT` 调整）。
- `FB_BLIT` 实现内核同类参数校验并支持 `scale>=1` 绘制。
- 配合 Wine 参数 `--fb-window` 可将 framebuffer 实时显示到主机窗口（pygame 后端）；未启用时保持内存缓冲模式。
- `FB_CLEAR` 支持清屏颜色写入。
- `KERNEL_VERSION`（`84`）在 Wine 中返回内核版本字符串（当前默认 `1.0.0-alpha`）。
- `DISK_*`（`85..94`）在 Wine 中已实现：
- 提供虚拟磁盘容量信息与 FAT32 格式化状态查询。
- `DISK_FORMAT_FAT32` 会初始化/重置 Wine rootfs 下的虚拟磁盘目录。
- `DISK_MOUNT`/`DISK_MOUNT_PATH` 支持挂载点管理；挂载路径内的 `FS_MKDIR/WRITE/APPEND/REMOVE` 会走磁盘后端。
- `DISK_READ_SECTOR`/`DISK_WRITE_SECTOR`（`93..94`）在 Wine 中已实现为 512B 原始扇区读写（host 文件后端）。
- 网络 syscall（`95..106`）在 Wine 当前为兼容占位实现（统一返回 `0`）；即 Wine 运行模式下不会提供真实网络收发。
- `MOUSE_STATE`（`107`）在 Wine 中为基础兼容实现：可返回指针数据结构；未启用窗口鼠标事件时 `ready` 可能为 `0`。
- `WM_*`（`108..115`）在 Wine 当前为兼容占位实现（统一返回 `0`）；不会创建真实窗口服务。
- `PTY_OPEN`（`116`）在 Wine 中创建内存缓冲 FD；写入端通过 `FD_WRITE` 追加，读取端通过 `FD_READ` 消费，用于桌面 Terminal 捕获子进程输出。
- `USER_HEAP_ALLOC`（`120`）在 Wine 中分配 guest 用户态 heap 区域，用于模拟内核动态 heap 分配。
- `DRIVER_*`（`121..125`）在 Wine 中为兼容实现，用于枚举/加载/卸载/重扫驱动条目；不执行真实内核态驱动。
- Wine 在运行时崩溃场景下会生成与内核一致格式的“信号编码退出状态”，可通过 `WAITPID` 读取。
- Wine 当前音频 syscall 为占位实现：`AUDIO_AVAILABLE=0`，`AUDIO_PLAY_TONE=0`，`AUDIO_STOP=1`。
- Wine 版本号策略固定为 `85.0.0-wine`（历史兼容号；不会随 syscall 扩展继续增长）。
