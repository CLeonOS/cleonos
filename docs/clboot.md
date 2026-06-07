# CLBoot UEFI 启动器

CLBoot 是 CLeonOS 自研启动器。目前主要目标是 x86_64 UEFI ISO 启动，不依赖 Limine 启动内核。

## 启动流程

```text
UEFI firmware
-> ISO El Torito EFI image
-> /EFI/BOOT/BOOTX64.EFI
-> CLBoot Boot Menu
-> 读取 /EFI/CLEONOS/clboot.conf
-> 读取 /boot/clks_kernel.elf
-> 读取 /boot/cleonos_ramdisk.tar
-> 获取 GOP framebuffer
-> 获取 UEFI memory map
-> 构造 clboot_info
-> ExitBootServices
-> 切换 CLBoot 页表
-> 跳转 kernel entry
-> CLKS 接收 clboot_info 并继续启动
```

## 构建和运行

```sh
make clboot
make clboot-iso
make ninja-clboot-harddisk
make run-clboot
make ninja-run-clboot-harddisk
```

Ninja 构建：

```sh
python3 scripts/gen_ninja.py
ninja -f build.ninja clboot-iso
ninja -f build.ninja clboot-harddisk
ninja -f build.ninja run-clboot
ninja -f build.ninja run-clboot-harddisk
```

`run-clboot` 启动的是生成的纯 UEFI ISO，不使用 QEMU `-kernel`，也不使用单独 ESP 硬盘镜像。
`run-clboot-harddisk` 启动的是生成的 UEFI 硬盘镜像，不挂载 ISO。

产物：

```text
build/x86_64/clboot/BOOTX64.EFI
build/x86_64/clks_kernel_clboot.elf
build/CLeonOS-CLBoot-x86_64.iso
build/x86_64/clboot_harddisk.img
```

`run-clboot` 需要 OVMF：

```text
/usr/share/OVMF/OVMF_CODE.fd
```

或：

```text
/usr/share/ovmf/OVMF.fd
```

## Boot Menu

CLBoot 启动后会显示 TUI 风格启动菜单，默认 5 秒倒计时自动启动第一项。

按键：

- `Enter`：启动当前选中项。
- `1` 到 `8`：选择当前页对应启动项。
- `Up/Down`：移动选择。
- `+/-`：翻页。
- `e`：编辑当前启动项的 entry-specific cmdline。
- `c`：预览最终 cmdline，即全局 cmdline + 当前启动项 cmdline。
- `i`：查看当前启动项详情，包括 title、kernel、ramdisk、cmdline、hint、来源。

内置菜单项：

```text
Try CLeonOS
Safe Mode
Verbose Boot
Quiet Boot
Install to Disk
Repair Disk System
Update Kernel
Verify Installation
```

这些菜单项会在配置文件 cmdline 后追加额外启动参数：

```text
Safe Mode: clks.rescue=1 clks.nosplash clks.loglevel=debug
Verbose Boot: clks.loglevel=debug
Quiet Boot: clks.loglevel=quiet
Install to Disk: clks.installer=install clks.nosplash
Repair Disk System: clks.installer=repair clks.nosplash
Update Kernel: clks.installer=update-kernel clks.nosplash
Verify Installation: clks.installer=verify clks.nosplash
```

如果 `/EFI/CLEONOS/clboot.conf` 定义了 `menuentry`，CLBoot 会使用配置文件里的菜单项替换内置菜单。启动器还会扫描 `/boot/kernels/*.elf`，把找到的内核追加到菜单尾部，方便测试不同内核版本和回滚。

## 启动 UI

CLBoot 会在加载阶段显示启动进度：

```text
connecting devices
reading kernel
loading kernel ELF
reading ramdisk
loading command line
probing framebuffer
allocating boot info
allocating memory map
allocating kernel stack
building page tables
collecting UEFI memory map
exiting boot services
```

每个阶段会同时写入 CLBoot bootlog，并通过 `clboot_info` 传给内核。

## 配置

配置文件：

```text
/EFI/CLEONOS/clboot.conf
```

ISO 构建使用 `configs/clboot.conf`，硬盘镜像构建使用 `configs/clboot-harddisk.conf`。两者菜单格式相同，主要区别是全局 cmdline：

```text
ISO:  clks.boot=iso clks.bootloader=clboot
Disk: clks.boot=disk clks.bootloader=clboot
```

配置格式：

```ini
timeout=5
default=0
cmdline=clks.boot=iso clks.bootloader=clboot clks.locale=zh_CN

menuentry=Try CLeonOS
kernel=\boot\clks_kernel.elf
ramdisk=\boot\cleonos_ramdisk.tar
cmdline=
hint=Normal ISO live boot.

menuentry=Safe Mode
kernel=\boot\clks_kernel.elf
ramdisk=\boot\cleonos_ramdisk.tar
cmdline=clks.rescue=1 clks.nosplash clks.loglevel=debug
hint=No splash, root rescue mode, verbose logs.
```

如果配置文件不存在，默认命令行是：

```text
clks.boot=iso clks.bootloader=clboot
```

字段说明：

- `timeout`：自动启动倒计时秒数。
- `default`：默认启动项，使用从 0 开始的索引。
- 全局 `cmdline`：所有启动项都会继承的基础启动参数。
- `menuentry`：创建一个启动项。
- `kernel`：当前启动项要加载的内核 ELF。
- `ramdisk`：当前启动项要加载的 ramdisk tar。
- 启动项内的 `cmdline`：追加到全局 cmdline 后面。
- `hint`：菜单底部显示的说明。

启动时按 `e` 可以临时编辑当前启动项的 `cmdline`。编辑只影响本次启动，不会写回 `clboot.conf`。

启动时按 `c` 可以查看最终传给内核的 cmdline。按 `i` 可以查看当前启动项来源：

- `builtin`：CLBoot 内置默认启动项。
- `clboot.conf`：来自 `/EFI/CLEONOS/clboot.conf`。
- `/boot/kernels scan`：来自 `/boot/kernels/*.elf` 扫描结果。

CLBoot 会在显示菜单前校验每个启动项的 `kernel` 和 `ramdisk` 是否能打开：

- 有效项显示 `[OK]`。
- 无效项显示 `[BAD]`。
- 当前默认项无效时会自动停止倒计时。
- 对无效项按 `Enter` 不会启动，会打开详情页显示缺失原因。

## CLBoot Protocol

协议头：

```text
boot/clboot/include/clboot_protocol.h
clks/include/clks/clboot.h
```

kernel 入口参数：

```c
void _start(u64 boot_magic, void *boot_info);
```

其中：

```c
boot_magic == CLBOOT_MAGIC
boot_info  == struct clboot_info *
```

CLKS 内核启动时会调用：

```c
clks_clboot_set_info(boot_magic, boot_info);
```

然后通过现有 `clks_boot_get_*` 接口读取 framebuffer、memmap、cmdline 和 ramdisk module。

协议版本 2 增加了 bootlog 字段：

```c
u64 bootlog;
u64 bootlog_size;
u64 bootlog_entry_count;
```

内核会通过 `clks_clboot_get_bootlog()` 读取这些字段，并在内核日志中输出：

```text
[INFO][CLBOOT] BOOT LOG BEGIN
...
[INFO][CLBOOT] BOOT LOG END
```

## 用户态 Shell 崩溃修复

此前 CLBoot 启动后，在内核切到用户态 shell 过程中可能重启。实际原因是执行用户进程时切换到了进程 CR3，但当前内核低地址栈/identity mapping 没有映射到进程页表里，导致页错误后进入 double fault/triple fault。

当前修复：

- 创建进程地址空间时继承内核页表的 `PML4[0]`，保留低地址 identity/current stack 映射。
- 销毁进程地址空间时跳过共享的 `PML4[0]`，避免释放内核共享页表。
- 用户 shell 自动启动改为异步 `clks_exec_spawn_path()`，避免在 userland tick 里同步运行交互式程序。

## 当前限制

- 仅 x86_64 UEFI。
- 支持 ISO 启动和 UEFI 硬盘镜像启动。
- 配置文件支持多个 `menuentry`，每个启动项可指定不同 kernel/ramdisk/cmdline。
- 支持扫描 `/boot/kernels/*.elf` 并追加到菜单。
- 暂无 Secure Boot。
- 硬盘启动当前是构建期生成的 CLBoot UEFI 镜像，仍会加载 ramdisk；不是 install2disk 的无 ramdisk 完整安装模式。
- 页表仍是 CLBoot 最小可用实现，后续可以继续做更完整的物理内存和 runtime services 处理。
