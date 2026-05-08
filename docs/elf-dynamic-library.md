# CLeonOS ELF 动态链接库使用文档

本文档说明如何在 CLeonOS 用户态使用 ELF 动态链接库。

当前实现提供 POSIX 风格接口：

```c
void *dlopen(const char *path, int flags);
void *dlsym(void *handle, const char *symbol);
int dlclose(void *handle);
```

头文件：

```c
#include <dlfcn.h>
```

底层 syscall：

- `CLEONOS_SYSCALL_DL_OPEN` = `77`
- `CLEONOS_SYSCALL_DL_CLOSE` = `78`
- `CLEONOS_SYSCALL_DL_SYM` = `79`

## 1. 当前模型

CLeonOS 当前的动态库是“可被 `dlopen` 加载的用户态 ELF 镜像”。

它和 Linux `.so` 不完全一样：

- 文件扩展名通常仍是 `.elf`。
- 通过 `dlopen("/shell/libdemo.elf", 0)` 加载。
- 通过 `dlsym(handle, "symbol_name")` 查找导出符号。
- 通过函数指针调用导出的函数。
- 不要求写 `cleonos_app_main` 才能被 `dlsym` 使用，但当前示例保留了它用于直接运行时打印状态。

当前限制：

- 不支持完整 Linux 动态链接器语义。
- 不支持自动解析依赖库链。
- 不保证完整 `DT_NEEDED`、RPATH、SONAME 等 ELF shared object 行为。
- 建议库和调用者使用明确的 C ABI，不要跨库传递复杂 C++ 对象。
- 建议导出函数使用基础整数、指针、C 字符串等简单参数。

## 2. 最小动态库

示例文件：

```text
cleonos/c/apps/libdemo_main.c
```

示例代码：

```c
#include <stdio.h>
#include "cmd_runtime.h"

typedef unsigned long long u64;

u64 cleonos_libdemo_add(u64 left, u64 right) {
    return left + right;
}

u64 cleonos_libdemo_mul(u64 left, u64 right) {
    return left * right;
}

u64 cleonos_libdemo_hello(void) {
    puts("[libdemo] hello from libdemo.elf");
    return 0ULL;
}

int cleonos_app_main(int argc, char **argv, char **envp) {
    (void)argc;
    (void)argv;
    (void)envp;
    puts("[libdemo] dynamic library image ready");
    return 0;
}
```

导出符号：

- `cleonos_libdemo_add`
- `cleonos_libdemo_mul`
- `cleonos_libdemo_hello`

符号名来自 ELF symbol table。调用方需要用完全一致的字符串传给 `dlsym`。

## 3. 调用动态库

示例文件：

```text
cleonos/c/apps/dltest_main.c
```

核心用法：

```c
#include <dlfcn.h>
#include <stdio.h>

typedef unsigned long long u64;
typedef u64 (*dl_math2_fn)(u64, u64);
typedef u64 (*dl_void_fn)(void);

int cleonos_app_main(int argc, char **argv, char **envp) {
    const char *lib_path = "/shell/libdemo.elf";
    void *handle;
    dl_math2_fn add_fn;
    dl_math2_fn mul_fn;
    dl_void_fn hello_fn;

    (void)argc;
    (void)argv;
    (void)envp;

    handle = dlopen(lib_path, 0);
    if (handle == (void *)0) {
        puts("dlopen failed");
        return 1;
    }

    add_fn = (dl_math2_fn)dlsym(handle, "cleonos_libdemo_add");
    mul_fn = (dl_math2_fn)dlsym(handle, "cleonos_libdemo_mul");
    hello_fn = (dl_void_fn)dlsym(handle, "cleonos_libdemo_hello");

    if (add_fn == (dl_math2_fn)0 || mul_fn == (dl_math2_fn)0 || hello_fn == (dl_void_fn)0) {
        puts("dlsym failed");
        (void)dlclose(handle);
        return 2;
    }

    (void)hello_fn();
    printf("add(7, 35) = %llu\n", add_fn(7ULL, 35ULL));
    printf("mul(6, 9) = %llu\n", mul_fn(6ULL, 9ULL));

    if (dlclose(handle) != 0) {
        puts("dlclose failed");
        return 3;
    }

    return 0;
}
```

## 4. 返回值规则

`dlopen`：

- 成功：返回非空 handle。
- 失败：返回 `NULL`。

`dlsym`：

- 成功：返回符号地址。
- 失败：返回 `NULL`。

`dlclose`：

- 成功：返回 `0`。
- 失败：返回 `-1`。

底层 syscall 返回值：

- `cleonos_sys_dl_open(path)`：成功返回 handle，失败返回 `-1`。
- `cleonos_sys_dl_sym(handle, symbol)`：成功返回地址，失败返回 `-1`。
- `cleonos_sys_dl_close(handle)`：成功返回 `0`，失败返回 `-1`。

## 5. 构建和放置

动态库和普通用户态 app 目前都通过 `project.bdt` 的用户态 app 构建流程生成。

源码命名约定：

```text
cleonos/c/apps/<name>_main.c
```

生成产物：

```text
build/x86_64/user/apps/<name>.elf
```

打包进 ramdisk 后位于：

```text
/shell/<name>.elf
```

示例：

```text
cleonos/c/apps/libdemo_main.c
build/x86_64/user/apps/libdemo.elf
/shell/libdemo.elf
```

构建：

```bash
make userapps -j1
make iso
```

运行测试：

```text
dltest
```

指定库路径：

```text
dltest /shell/libdemo.elf
```

## 6. 符号导出建议

建议：

- 使用唯一前缀，例如 `cleonos_libdemo_`。
- 使用 C ABI 风格函数。
- 参数和返回值优先使用 `u64`、`int`、`char *`、简单结构体指针。
- 调用方和库方必须保持函数指针类型一致。

不建议：

- 导出同名通用符号，例如 `init`、`open`、`read`。
- 跨库传递 C++ 对象。
- 假设构造函数、析构函数、全局初始化顺序和 Linux `.so` 一致。
- 在库里依赖另一份需要自动加载的动态库。

## 7. 常见问题

### `dlopen` 返回 `NULL`

可能原因：

- 路径错误。
- 文件没有打包进 `/shell`。
- ELF 加载失败。
- 当前内存不足。

检查：

```text
ls /shell
dltest /shell/libdemo.elf
```

### `dlsym` 返回 `NULL`

可能原因：

- 符号名拼错。
- 函数被声明成 `static`。
- 构建时符号没有进入 ELF symbol table。
- 调用方查找的名字和实际导出名不一致。

建议：

- 不要给需要导出的函数加 `static`。
- 使用长前缀命名，避免冲突。
- 先用现有 `libdemo.elf` / `dltest.elf` 验证动态加载框架正常。

### 调用后崩溃

常见原因：

- 函数指针类型和真实函数签名不一致。
- 传入了无效指针。
- 库函数保存了调用方栈上对象地址并在之后继续使用。
- 库内访问了未初始化的全局状态。

处理方式：

- 先把接口收敛成 `u64 fn(u64, u64)` 这类简单签名验证。
- 再逐步增加字符串、结构体指针等复杂参数。

## 8. 现有示例

库：

```text
cleonos/c/apps/libdemo_main.c
```

调用方：

```text
cleonos/c/apps/dltest_main.c
```

运行：

```text
dltest
```

预期输出包含：

```text
[libdemo] hello from libdemo.elf
[dltest] add(7, 35) = 42
[dltest] mul(6, 9) = 54
[dltest] PASS
```
