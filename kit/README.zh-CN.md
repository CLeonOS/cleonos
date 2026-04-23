# CLeonOS 应用开发套件

[English](README.md) | [简体中文](README.zh-CN.md)

`kit/` 是一个独立的 CLeonOS 用户态应用 SDK。  
它可以在**不依赖 CLKS 内核源码**的情况下构建 ring3 ELF 程序。

## 工具链要求

请使用 **ELF** 目标工具链（不要使用 MinGW 的 PE/COFF 目标）：

- 推荐：`x86_64-elf-gcc` + `x86_64-elf-ld`
- 备选：`clang` + ELF 模式 `ld.lld`

## 套件内容

- 来自 CLeonOS 用户态的运行时实现（`runtime/`）
- syscall / libc 风格头文件（`include/`）
- 用户态链接脚本（`linker/user.ld`）
- 独立的 CMake 工程和 Makefile 包装
- 示例程序：`apps/hello/main.c`

默认示例行为：
- 输出 `Hello world!`

## 快速开始

```bash
cd kit
make apps
```

生成的 ELF 位于：

```text
kit/build/apps/*.elf
```

## 仅构建指定应用

只构建一个：

```bash
make apps APPS=hello
```

构建多个：

```bash
make apps APPS=hello,foo,bar
```

每个应用应放在 `apps/<name>/` 目录，并包含一个或多个 `.c` 文件。

## 程序入口

默认启用 `runtime/main_adapter.c`，因此应用可以直接写标准入口：

```c
int main(int argc, char **argv)
```

如果你要使用原始入口 `cleonos_app_main(int,char**,char**)`，可关闭适配器：

```bash
cmake -S . -B build-cmake -DCLEONOS_KIT_USE_MAIN_ADAPTER=OFF
```

## 直接使用 CMake

```bash
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake --target apps
```

显式指定 ELF 工具链示例：

```bash
cmake -S . -B build-cmake \
  -DCLEONOS_KIT_CC=x86_64-elf-gcc \
  -DCLEONOS_KIT_LD=x86_64-elf-ld
cmake --build build-cmake --target apps
```

常用选项：

- `-DCLEONOS_KIT_APP_NAMES=hello,foo`
- `-DCLEONOS_KIT_ENABLE_WERROR=OFF`
- `-DCLEONOS_KIT_OUTPUT_DIR=<path>`

## 新增应用

1. 创建目录：`apps/myapp/`
2. 添加源码（例如 `apps/myapp/main.c`）
3. 执行 `make apps`

kit 会自动扫描并构建应用目录。
