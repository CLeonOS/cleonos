# hbos HariboteOS 兼容宿主

`hbos` 是 CLeonOS 用户态 HariboteOS 兼容宿主，目标是在 CLeonOS 上运行《三十天自制操作系统》HariboteOS 风格环境。

## 运行

```sh
hbos
```

退出回 CLeonOS：

```text
exit2cleonos
```

## 当前实现

当前版本是用户态宿主，不修改 CLKS 内核，但 CLeonOS 镜像会直接携带 HariboteOS 内核镜像：

- 通过 `KBD_GET_CHAR` syscall 接收 CLeonOS 键盘输入。
- 启动时加载 `/system/hbos/HARIBOTE.SYS`，作为被宿主化的 HariboteOS 0.27f 内核镜像。
- `/system/hbos/HARIBOTE.IMG` 保存上游 Haribote 软盘镜像，便于后续完整 FAT/内核事件循环适配。
- `/system/hbos/*.HRB` 保存上游 Haribote 原生应用程序。
- 在用户态维护 Haribote 风格 640x400 画面缓冲。
- 通过 `FB_BLIT` syscall 把画面传回 CLeonOS TTY 像素绘制系统。
- 内置 Haribote 风格桌面、任务栏、终端窗口。
- 内置 FAT-like 8.3 文件表，可用 `dir` 和 `type` 查看。
- 内置 HRB app thunk 表，可运行 `a`、`hello3`、`winhello`、`stars`、`lines`、`noodle`。
- 支持读取真实 Haribote `.hrb` 文件头，格式检查与元信息输出。
- 支持 Haribote `OSASKCMP/TEK` 压缩 `.hrb` 自动解压。
- 支持最小 i386 用户态解释器，可运行只使用基础 Haribote API 的简单 `.hrb` 程序。
- 支持 `int 0x40` API：`putchar`、`putstr0`、`putstr1`、`end`、`getkey`、`beep`、`getlang`。

## 命令

```text
help
 dir
 type <file>
 app
 run <app>
 runhrb <path-or-name>
 hrbinfo <path-or-name>
 mem
 color
 lines
 about
 cls
 exit2cleonos
```

`run <app>` 和直接输入 app 名称等价，例如：

```text
run hello3
stars
noodle
```

真实 `.hrb` 可以放在：

```text
/system/hbos
/shell/hbos
/temp
```

默认构建会优先从 `build/tmp-haribote/harib27f` 构建并复制 Haribote 资源到 ramdisk：

```text
/system/hbos/HARIBOTE.SYS
/system/hbos/HARIBOTE.IMG
/system/hbos/*.HRB
```

如果 `/system/hbos/HARIBOTE.SYS` 不存在，`hbos` 会拒绝启动并提示重建 ramdisk。这样可以保证当前环境不是只靠占位表伪装 Haribote 内核。

也可以直接输入绝对路径：

```text
hrbinfo /system/hbos/hello5.hrb
runhrb /system/hbos/hello5.hrb
run hello5
```

## 兼容边界

当前实现已经直接携带并加载 Haribote 内核镜像，同时具备 HRB 文件解析、模拟数据段、入口 `0x1b`、最小 i386 指令解释和基础 `int 0x40` 分发。

它还不是完整 WoW64 级别转译层，复杂 Haribote 程序可能因为以下原因停止：

- 使用了尚未实现的 i386 指令。
- 使用了窗口、sheet、文件句柄、timer 等尚未完整映射的 Haribote API。
- 依赖完整 Haribote 内核任务调度、FAT 镜像运行时、sheet/window/timer 等尚未完全映射到 CLeonOS。

解释器遇到不支持的指令或 API 会在终端显示 `unsupported i386 op/api`，不会让 CLeonOS 内核崩溃。

建议优先测试这些基础程序：

```text
a.hrb
hello3.hrb
hello5.hrb
```

`stars.hrb`、`lines.hrb`、`winhelo.hrb` 这类图形/窗口程序会逐步随着 Haribote API 5-14 的实现而变得可用。

后续完整化建议顺序：

1. 将 `HARIBOTE.IMG` 的 FAT 目录作为 hbos 的真实文件系统来源。
2. 继续补 i386 指令：SIB 寻址、乘除法、字符串指令、更多条件跳转。
3. 完成 Haribote API 5-14 的窗口/sheet 映射。
4. 完成 Haribote API 16-26 的 timer、文件句柄、命令行参数。
5. 将 HariboteOS 原内核事件循环接到 hbos 的输入和 framebuffer 后端。
6. 逐步把 Haribote 内核全局状态迁移到 hbos 的用户态内存模型中。

## 上游代码

- HariboteOS 源码：`https://github.com/HariboteOS/harib27f`
- Linux 版构建工具：`https://github.com/HariboteOS/z_tools_linux`

上游 `harib27f` README 标注许可证为 KL-01。如果要把上游源码或构建工具作为仓库内容引入，需要单独确认许可证和第三方代码放置策略。
