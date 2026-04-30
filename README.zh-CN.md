# CLeonOS

[English](README.md) | [简体中文](README.zh-CN.md)

一个实验性的 x86_64 操作系统项目，包含 C 内核、Rust 辅助运行时代码、用户态 ELF 应用，以及按 Stage 演进的开发历史。

## 项目特性

- 基于 Limine 启动的 x86_64 内核
- RAM-disk VFS 目录布局（`/system`、`/shell`、`/temp`、`/driver`）
- 虚拟磁盘后端，支持 FAT32 格式化与挂载（默认挂载点：`/temp/disk`）
- 虚拟 TTY 子系统（多 TTY、ANSI、光标、PSF 字体）
- 键盘/鼠标输入栈，TTY2 提供桌面模式
- 用户态 ELF 应用模型，syscall ABI 使用 `int 0x80`
- 用户 Shell + 外部命令 ELF（`ls`、`cat`、`grep`、`mkdir`、`cp`、`mv`、`rm` 等）
- Shell 管道与重定向（`|`、`>`、`>>`）
- 可选主机侧 CLeonOS-Wine 运行器（Python + Unicorn），位于 [`wine/`](wine)

## 仓库结构

```text
.
|- clks/                 # CLKS 内核子模块（独立内核仓库）
|- cleonos/              # 用户态运行时、基础库、用户应用、Rust 用户库
|- kit/                  # 独立用户态应用 SDK（无需内核源码树即可构建 ELF）
|- ramdisk/              # 运行时 ramdisk 的静态文件
|- configs/              # 启动配置（Limine）
|- bdt/                  # Build Tool 源码
|- docs/                 # Stage 文档与 syscall 文档
|- wine/                 # 主机侧用户 ELF 运行器（无需完整虚拟机）
|- project.bdt           # bdt 主项目定义
|- Makefile              # 面向开发者的 bdt 包装入口
```

## 构建依赖

最低需要：

- `make`
- 用于自举 `bdt` 的主机 C 编译器
- `git`
- `tar`
- `xorriso`
- `sh`（POSIX shell）
- `rustc`
- 内核/用户工具链（会自动回退查找）：
  - 内核：`gcc`/`g++` + `ld`
  - 用户态：`gcc` + `ld`

如需从源码构建 Limine，请安装额外依赖：`autoconf`、`automake`、`libtool`、`pkg-config`、`mtools`、`nasm`。

运行时还需要：

- `qemu-system-x86_64`（用于 `make run` / `make debug`）

## 快速开始

```bash
git clone <your-repo-url>
cd cleonos
git submodule update --init --recursive
make run
```

`make run` 现在会自动准备 `build/x86_64/cleonos_disk.img`（不存在时自动创建），并以 QEMU 模拟物理硬盘方式挂载（`-drive ... if=ide`）。
该磁盘**不会**打包进 ISO，也**不会**通过 Limine `module_path` 注入。
内核当前通过内存缓存窗口处理磁盘元数据/文件（默认最多约 8MB）。
你也可以覆盖磁盘大小（MB），例如：`make run DISK_IMAGE_MB=128`。
进入系统后可先执行 `diskinfo` 确认磁盘已识别。

## 常用目标

- `make setup` - 检查工具并准备 Limine
- `make kernel` - 构建内核 ELF
- `make userapps` - 构建用户态 ELF 应用
- `make ramdisk` - 打包运行时 ramdisk
- `make disk-image` - 创建/调整运行时磁盘镜像（`build/x86_64/cleonos_disk.img`）
- `make iso` - 生成可启动 ISO
- `make run` - 启动 QEMU
- `make debug` - 以 `-s -S` 启动 QEMU 供 GDB 附加
- `make clean` - 清理 `build/x86_64`
- `make clean-all` - 清理全部构建产物

## 调试（GDB）

先启动调试 VM：

```bash
make debug
```

再在另一个终端连接：

```bash
gdb build/x86_64/clks_kernel.elf
(gdb) target remote :1234
```

## User Shell 示例

```sh
help
ls /shell
cat /shell/init.cmd
grep -n exec /shell/init.cmd
cat /shell/init.cmd | grep -n exec
ls /shell > /temp/shell_list.txt
diskinfo
partctl list
partctl init-mbr
partctl create 1 2048 32768 0x0C 1
partctl delete 1
mkfsfat32 CLEONOS
mount /temp/disk
write /temp/disk/hello.txt hello-disk
```

## 文档

- Stage 索引：[`docs/README.md`](docs/README.md)
- Syscall ABI 文档：[`docs/syscall.md`](docs/syscall.md)

## CI

GitHub Actions 工作流 [`build-os.yml`](.github/workflows/build-os.yml) 会在 push/PR 时自动构建 ISO，并上传为 artifact。

## 贡献指南

1. Fork 仓库并创建功能分支。
2. 尽量按 Stage 组织改动，并同步更新文档。
3. 提交 PR 前至少执行一次 `make iso`。
4. 涉及可见行为变化时，附上启动日志片段或截图。

## 许可证

Apache-2.0。详见 [`License`](License)。
