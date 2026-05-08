# CLeonOS ANSI / TTY 转义序列文档

本文档记录 CLeonOS TTY 当前支持的 ANSI/VT 风格转义序列，主要面向 shell、TUI 程序、编辑器、包管理器和调试工具。

CLeonOS TTY 支持 UTF-8 文本输出，并支持一组常用 CSI 序列。未列出的序列会被忽略或只消费掉，不保证兼容完整 VT100/xterm。

## 1. 基础格式

ESC 字符为 `0x1B`，常见写法：

```c
"\x1b[31mred\x1b[0m"
```

CSI 格式：

```text
ESC [ 参数 最终字符
```

例如：

```text
ESC [ 2 J
ESC [ 31 ; 1 m
ESC [ 10 ; 20 H
```

## 2. 文本样式 SGR

格式：

```text
ESC [ <codes> m
```

支持的 SGR code：

| Code | 作用 |
| --- | --- |
| `0` | 重置前景色、背景色、粗体、下划线、反色、字体缩放 |
| `1` | 粗体 |
| `4` | 下划线 |
| `7` | 反色 |
| `21` / `22` | 关闭粗体 |
| `24` | 关闭下划线 |
| `27` | 关闭反色 |
| `39` | 前景色恢复默认 |
| `49` | 背景色恢复默认 |

示例：

```c
printf("\x1b[1mBold\x1b[0m\n");
printf("\x1b[4mUnderline\x1b[0m\n");
printf("\x1b[7mInverse\x1b[0m\n");
```

## 3. 16 色颜色

前景色：

| Code | 颜色 |
| --- | --- |
| `30` | black |
| `31` | red |
| `32` | green |
| `33` | yellow |
| `34` | blue |
| `35` | magenta |
| `36` | cyan |
| `37` | white |
| `90`-`97` | bright black 到 bright white |

背景色：

| Code | 颜色 |
| --- | --- |
| `40`-`47` | 普通背景色 |
| `100`-`107` | 高亮背景色 |

示例：

```c
printf("\x1b[31mred\x1b[0m\n");
printf("\x1b[97;44mbright white on blue\x1b[0m\n");
```

注意：`1` 粗体开启后，`30`-`37` 会映射到高亮色。

## 4. 256 色和 TrueColor

CLeonOS 支持 xterm 风格的 256 色和 24-bit RGB。

256 色前景：

```text
ESC [ 38 ; 5 ; <index> m
```

256 色背景：

```text
ESC [ 48 ; 5 ; <index> m
```

TrueColor 前景：

```text
ESC [ 38 ; 2 ; <r> ; <g> ; <b> m
```

TrueColor 背景：

```text
ESC [ 48 ; 2 ; <r> ; <g> ; <b> m
```

`r/g/b` 会被限制到 `0..255`。

示例：

```c
printf("\x1b[38;5;196m256 red\x1b[0m\n");
printf("\x1b[38;2;255;128;0mtruecolor orange\x1b[0m\n");
printf("\x1b[48;2;20;40;80mcustom bg\x1b[0m\n");
```

## 5. 光标移动

坐标参数是 1-based，左上角为 `1;1`。

| 序列 | 作用 |
| --- | --- |
| `ESC [ <row> ; <col> H` | 移动光标到指定位置 |
| `ESC [ <row> ; <col> f` | 同 `H` |
| `ESC [ <n> A` | 光标上移 n 行 |
| `ESC [ <n> B` | 光标下移 n 行 |
| `ESC [ <n> C` | 光标右移 n 列 |
| `ESC [ <n> D` | 光标左移 n 列 |
| `ESC [ <n> E` | 下移 n 行并回到列 1 |
| `ESC [ <n> F` | 上移 n 行并回到列 1 |
| `ESC [ <col> G` | 移动到当前行的指定列 |
| `ESC [ <row> d` | 移动到指定行，列不变 |

缺省参数按 `1` 处理。

示例：

```c
printf("\x1b[10;20Hhello");
printf("\x1b[2Aup two lines");
printf("\x1b[1Gline start");
```

## 6. 清屏和清行

清屏：

| 序列 | 作用 |
| --- | --- |
| `ESC [ 0 J` | 从光标位置清到屏幕结尾 |
| `ESC [ 1 J` | 从屏幕开头清到光标位置 |
| `ESC [ 2 J` | 清空可见屏幕 |
| `ESC [ 3 J` | 清空可见屏幕并清空 scrollback |

清行：

| 序列 | 作用 |
| --- | --- |
| `ESC [ 0 K` | 从光标位置清到行尾 |
| `ESC [ 1 K` | 从行首清到光标位置 |
| `ESC [ 2 K` | 清空整行 |

示例：

```c
printf("\x1b[2J\x1b[H");      // 清屏并回到左上角
printf("\x1b[2K\x1b[1G");     // 清当前行并回到行首
printf("\x1b[3J");             // 清屏并清 scrollback
```

## 7. 保存和恢复光标

支持两套常见写法：

| 序列 | 作用 |
| --- | --- |
| `ESC 7` | 保存光标 |
| `ESC 8` | 恢复光标 |
| `ESC [ s` | 保存光标 |
| `ESC [ u` | 恢复光标 |

示例：

```c
printf("\x1b[s");
printf("\x1b[1;1Hstatus");
printf("\x1b[u");
```

## 8. 光标显示控制

支持 DEC private mode 25：

| 序列 | 作用 |
| --- | --- |
| `ESC [ ? 25 h` | 显示/启用光标闪烁 |
| `ESC [ ? 25 l` | 隐藏/禁用光标闪烁 |

示例：

```c
printf("\x1b[?25l"); // hide cursor
printf("\x1b[?25h"); // show cursor
```

## 9. CLeonOS 字体缩放扩展

CLeonOS 额外支持自定义 CSI：

```text
ESC [ <scale> z
```

`scale` 范围：`1..3`。

| 序列 | 作用 |
| --- | --- |
| `ESC [ 1 z` | 普通字体 |
| `ESC [ 2 z` | 2 倍字体 |
| `ESC [ 3 z` | 3 倍字体 |

缩放会影响后续输出的字符占用单元格。换行后行高会按当前行内最大缩放处理。

示例：

```c
printf("\x1b[3zBig Title\x1b[1z\n");
printf("\x1b[2zSection\x1b[1z normal text\n");
```

注意：这是 CLeonOS 扩展，不是标准 ANSI/xterm 序列。

## 10. UTF-8 和宽字符

TTY 支持 UTF-8 解码。以下字符会按双宽处理：

- CJK 常用宽字符区间
- Hangul
- Fullwidth forms
- Supplementary Ideographic Plane
- Emoji 常用区间：`U+1F000..U+1FAFF`、`U+2600..U+27BF`

双宽字符会占用 2 个 TTY cell，后续 cell 会被标记为 continuation，不会重复绘制。

## 11. 当前不支持或不完整支持

当前实现不是完整 xterm。以下能力暂不保证：

- OSC 序列，例如窗口标题 `ESC ] 0 ; title BEL`
- alternate screen buffer，例如 `ESC [?1049h`
- scroll region，例如 `ESC [ top ; bottom r`
- tab stop 管理
- bracketed paste
- mouse reporting
- 光标形状切换
- SGR italic、dim、blink、strikethrough
- Unicode grapheme cluster 合成，例如 emoji ZWJ 序列、肤色修饰符组合

## 12. 推荐写法

TUI 程序建议：

```c
#define ESC "\x1b["

printf(ESC "?25l");       // hide cursor while redrawing
printf(ESC "2J" ESC "H"); // clear screen and home
printf(ESC "1;36mTitle" ESC "0m\n");
printf(ESC "?25h");       // restore cursor
```

如果程序会异常退出，尽量在退出前恢复状态：

```c
printf("\x1b[0m\x1b[1z\x1b[?25h");
```

## 13. 实现位置

主要实现文件：

- `clks/kernel/interface/tty/ansi.inc`
- `clks/kernel/interface/tty/screen.inc`
- `clks/kernel/interface/tty/draw_dirty.inc`
