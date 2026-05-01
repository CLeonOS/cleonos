#ifndef CLEONOS_TUI_H
#define CLEONOS_TUI_H

typedef unsigned long long tui_u64;

#define CLEONOS_TUI_COLOR_BLACK 0
#define CLEONOS_TUI_COLOR_RED 1
#define CLEONOS_TUI_COLOR_GREEN 2
#define CLEONOS_TUI_COLOR_YELLOW 3
#define CLEONOS_TUI_COLOR_BLUE 4
#define CLEONOS_TUI_COLOR_MAGENTA 5
#define CLEONOS_TUI_COLOR_CYAN 6
#define CLEONOS_TUI_COLOR_WHITE 7
#define CLEONOS_TUI_COLOR_DEFAULT 9

#define CLEONOS_TUI_ATTR_BOLD 0x01U
#define CLEONOS_TUI_ATTR_UNDERLINE 0x02U
#define CLEONOS_TUI_ATTR_REVERSE 0x04U

typedef struct cleonos_tui_rect {
    int x;
    int y;
    int width;
    int height;
} cleonos_tui_rect;

typedef struct cleonos_tui_style {
    int fg;
    int bg;
    unsigned int attrs;
} cleonos_tui_style;

void cleonos_tui_init(void);
void cleonos_tui_shutdown(void);
void cleonos_tui_clear(void);
void cleonos_tui_refresh(void);
void cleonos_tui_move(int y, int x);
void cleonos_tui_set_style(cleonos_tui_style style);
void cleonos_tui_reset_style(void);
void cleonos_tui_puts_at(int y, int x, const char *text);
void cleonos_tui_fill_rect(cleonos_tui_rect rect, char ch, cleonos_tui_style style);
void cleonos_tui_box(cleonos_tui_rect rect, const char *title, cleonos_tui_style style);
void cleonos_tui_hline(int y, int x, int width, char ch, cleonos_tui_style style);
void cleonos_tui_vline(int y, int x, int height, char ch, cleonos_tui_style style);
void cleonos_tui_button(cleonos_tui_rect rect, const char *label, int focused);
void cleonos_tui_status_bar(int y, const char *left, const char *right);
int cleonos_tui_read_key(void);
const char *cleonos_tui_key_name(int key, char *out, tui_u64 out_size);
cleonos_tui_style cleonos_tui_make_style(int fg, int bg, unsigned int attrs);

#endif
