# CLKS 内核

[English](README.md) | [简体中文](README.zh-CN.md)

CLKS 是 CLeonOS 项目的内核部分。  
它包含架构启动代码、中断处理、内存管理、调度器、syscall 层、存储、TTY/控制台与核心运行时服务。

## 当前状态

CLKS 目前可以在单仓库中以仅内核模式构建，但还不是完全独立仓库形态的构建系统。

- 已支持仅内核构建模式。
- 用户态/ISO 目标是可选项，可以关闭。
- 仍有部分构建脚本与根目录共享（`cmake/`、`configs/`、`scripts/`）。

## 目录结构

```text
clks/
|- arch/          # 架构相关启动与底层代码
|- include/       # 对外内核头文件
|- kernel/        # 内核核心子系统
|- rust/          # 内核使用的 Rust staticlib
|- third_party/   # 内核使用的第三方源码
|- CMakeLists.txt # 内核构建规则
|- Makefile       # 面向内核的包装入口（委托到根构建）
```

## 构建（仅内核）

在仓库根目录执行：

```bash
make kernel CLEONOS_ENABLE=OFF
```

或通过 CLKS 包装入口：

```bash
make -C clks kernel
```

## Menuconfig（CLKS 作用域）

```bash
make menuconfig-clks
```

或：

```bash
make -C clks menuconfig
```

以上命令会更新 `configs/menuconfig/` 下的 CLKS 相关配置输出（包含 `config.clks.cmake`）。

## 后续独立拆分建议

若要将 CLKS 拆成独立仓库，下一步关键工作是把共享构建资产迁移到 `clks/`（或引入等价副本），重点包括：

- `cmake/` 工具脚本（`log.cmake`、符号化生成、工具检查）
- 当前位于 `configs/` 下的启动配置与镜像打包相关内容
- 当前位于 `scripts/` 与 `configs/menuconfig/` 下的 menuconfig 启动器与特性元数据

## 许可证

Apache-2.0（与项目根目录一致）。
