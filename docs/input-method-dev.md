# CLeonOS 输入法开发文档

本文档说明如何为 CLeonOS 编写第三方输入法。

当前输入法架构是：

- 内核负责输入法宿主逻辑：按键拦截、composing 文本、候选栏、翻页、候选提交、底部状态栏刷新。
- 用户态输入法 ELF 负责注册输入法和提供规则文件。
- 简单输入法不需要修改内核，只需要放置 ELF 和规则表。

## 1. 文件布局

输入法文件建议放在：

```text
/shell/inputm/<name>.elf
/system/inputm/<name>.db
```

例如罗马音日语输入法：

```text
/shell/inputm/romaji.elf
/system/inputm/romaji.db
```

系统启动时会扫描 `/shell/inputm/*.elf`。如果存在同名规则表 `/system/inputm/<name>.db`，内核会自动注册为规则表输入法。

输入法 ELF 也可以在运行时主动调用：

```c
cleonos_sys_inputm_register_rule(...);
```

主动注册适合需要自定义显示名称、规则路径或 flags 的输入法。

## 2. 规则表格式

规则表是 UTF-8 文本文件。

基础格式：

```text
key<TAB>候选1 候选2 候选3
```

示例：

```text
a	あ
ka	か
shi	し
```

说明：

- key 只能匹配当前内核输入处理支持的字母输入。
- key 与候选之间使用 TAB。
- 多个候选之间使用空格。
- 候选可以是 UTF-8 文本，例如中文、日文假名。
- 以 `#` 开头的行是注释。

规则文件头部可以声明元信息：

```text
# inputm.name=RomajiJP
# inputm.label=ROMAJI:
```

字段含义：

- `inputm.name`：输入法显示名称。
- `inputm.label`：底栏 composing 标签。

如果不写元信息，自动扫描时会使用 ELF 文件名作为输入法名称，label 默认为 `COMP:`。

## 3. 规则表匹配行为

内核统一处理以下操作：

- 输入字母：追加到 composing 文本。
- 数字 `1` 到 `5`：选择当前页候选。
- 空格：提交当前页第一个候选。
- `+` / `-`：候选翻页。
- Backspace：删除 composing 文本最后一个字符。
- Esc：清空 composing 文本。

如果启用 `CLEONOS_INPUTM_FLAG_RULE_SPLIT`，内核会尝试把连续输入拆成多个 key 并组合候选。

示例：

```text
ka	か
ki	き
ku	く
```

输入：

```text
kakiku
```

候选：

```text
かきく
```

## 4. 注册接口

推荐使用：

```c
u64 cleonos_sys_inputm_register_rule(const char *name,
                                     const char *path,
                                     const char *rule_path,
                                     const char *label,
                                     u64 flags);
```

示例：

```c
(void)cleonos_sys_inputm_register_rule(
    "RomajiJP",
    "/shell/inputm/romaji.elf",
    "/system/inputm/romaji.db",
    "ROMAJI:",
    CLEONOS_INPUTM_FLAG_JAPANESE_ROMAJI |
        CLEONOS_INPUTM_FLAG_RULE_LOWERCASE |
        CLEONOS_INPUTM_FLAG_RULE_SPLIT |
        CLEONOS_INPUTM_FLAG_RULE_COMMIT_RAW);
```

返回值：

- 成功：输入法 index。
- 失败：`(u64)-1`。

## 5. Flags

可用 flags：

- `CLEONOS_INPUTM_FLAG_RULE_TABLE`
- `CLEONOS_INPUTM_FLAG_RULE_LOWERCASE`
- `CLEONOS_INPUTM_FLAG_RULE_SPLIT`
- `CLEONOS_INPUTM_FLAG_RULE_COMMIT_RAW`
- `CLEONOS_INPUTM_FLAG_CHINESE_PINYIN`
- `CLEONOS_INPUTM_FLAG_JAPANESE_ROMAJI`

说明：

- `RULE_TABLE` 表示规则表输入法。调用 `cleonos_sys_inputm_register_rule()` 时内核会自动补上。
- `RULE_LOWERCASE` 会把输入字母转为小写再查表。
- `RULE_SPLIT` 会尝试组合多个规则 key。
- `RULE_COMMIT_RAW` 在没有候选时提交原始 composing 文本。
- `CHINESE_PINYIN` 和 `JAPANESE_ROMAJI` 是类型标记，主要用于工具显示和默认 label。

## 6. 最小输入法程序

```c
#include "cmd_runtime.h"

#include <stdio.h>

int cleonos_app_main(void) {
    u64 index = cleonos_sys_inputm_register_rule(
        "MyIME",
        "/shell/inputm/myime.elf",
        "/system/inputm/myime.db",
        "MYIME:",
        CLEONOS_INPUTM_FLAG_RULE_LOWERCASE |
            CLEONOS_INPUTM_FLAG_RULE_SPLIT |
            CLEONOS_INPUTM_FLAG_RULE_COMMIT_RAW);

    if (index == (u64)-1) {
        puts("myime: register failed");
        return 1;
    }

    printf("myime: registered at index %llu\n", index);
    return 0;
}
```

对应规则表：

```text
# inputm.name=MyIME
# inputm.label=MYIME:
a	A
b	B
ab	AB
```

## 7. 构建配置

输入法应用源码通常放在：

```text
cleonos/c/apps/<name>_main.c
```

然后在 `project.bdt` 的输入法输出组中加入 app 名称：

```ini
output_group.inputm.output = {root}/build/x86_64/user/apps/inputm
output_group.inputm.apps = pinyin, romaji, myime
```

规则表文件需要放进 ramdisk：

```text
ramdisk/system/inputm/myime.db
```

构建：

```bash
make userapps -j1
make iso
```

## 8. 测试方式

进入系统后：

```text
myime
pinyin list
```

或者对罗马音输入法：

```text
romaji list
romaji use <index>
```

切换输入法：

```text
Ctrl+Shift+Space
```

输入测试：

```text
kakiku
```

如果规则表包含：

```text
ka	か
ki	き
ku	く
```

并启用了 `RULE_SPLIT`，候选栏应显示：

```text
1.かきく
```

## 9. 当前限制

- 当前协议是规则表输入法协议，不是完整常驻输入法进程 IPC。
- 候选生成在内核中完成，规则文件适合拼音、罗马音、五笔这类 key 到候选的映射。
- 需要复杂语法模型、实时学习、上下文预测的输入法，后续应扩展为事件 IPC 协议。
- 当前按键输入以字母为主，非字母 composing 规则需要继续扩展内核输入处理。
