# CLKS 启动逻辑与新版根目录

本文档描述 CLKS 删除内核内置 Shell 后的启动流程，以及 CLeonOS 当前的根目录布局。

## 启动流程

新版流程：

1. CLBoot 加载 CLKS kernel 和 ramdisk，并通过 cmdline 传递启动模式。
2. CLKS 初始化必要内核功能：串口、framebuffer/TTY、内存、VFS、locale、exec、输入、网络、驱动、syscall。
3. CLKS 根据启动模式选择 `/system/configs/user_space_enter*.conf`。
4. CLKS 校验配置中的入口 ELF 是否存在且 ELF inspect 成功。
5. `usrd` tick 异步启动入口应用。
6. 如果没有任何用户程序处于 pending/running/stopped 状态，CLKS 会按 retry interval 自动重新启动入口应用。

CLKS 不再包含 `clks>` 内核交互 Shell，也不再读取 `/shell/init.cmd`。

## user_space_enter 配置

普通启动读取：

```ini
path=/shell/apps/shell.elf
args=
env=LAUNCHER=/shell/apps/shell.elf
```

启动模式对应配置：

- 普通启动：`/system/configs/user_space_enter.conf`
- 安装：`/system/configs/user_space_enter.install.conf`
- 修复：`/system/configs/user_space_enter.repair.conf`
- 更新内核：`/system/configs/user_space_enter.update-kernel.conf`
- 验证安装：`/system/configs/user_space_enter.verify.conf`

配置字段：

- `path`：入口 ELF 的绝对路径，不能为空。
- `args`：传给入口程序的参数行，可为空。
- `env`：传给入口程序的环境变量行，可为空。

配置缺失、`path` 为空、路径不是绝对路径、入口 ELF 不存在、入口 ELF inspect 失败时，内核直接 panic，并输出配置路径、目标 path 和失败原因。

## 新版目录布局

- `/system/configs`：系统配置，例如 locale、theme、font、net、user_space_enter。
- `/system/cache`：系统缓存。
- `/system/databases`：系统数据库，例如 users、pkg、install manifest、update state。
- `/system/others`：杂项资源，例如字体、kernel.sym、install 资源、tcc runtime。
- `/system/drivers`：用户态驱动 ELF。
- `/shell/apps`：普通用户应用 ELF。
- `/shell/apps/uwm`：UWM 应用 ELF。
- `/shell/apps/inputm`：输入法 ELF。
- `/shell/data`：应用数据。
- `/inputm`：输入法词库和 license。
- `/tests`：测试资源。
- `/temp`：临时文件。
- `/etc`：系统版本和发行信息。

## 适配要求

其它发行版如果复用新版 CLKS，需要至少提供：

- `/system/configs/user_space_enter.conf`
- 配置里指定的入口 ELF，例如 `/shell/apps/shell.elf`
- `/system/configs`、`/system/cache`、`/system/databases`、`/system/others`、`/system/drivers`、`/shell/apps`、`/shell/data`、`/inputm`、`/temp`、`/tests`

如果发行版不提供这些目录或入口配置，CLKS 会在启动时失败或 panic。
