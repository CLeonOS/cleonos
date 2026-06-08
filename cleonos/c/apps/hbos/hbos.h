#ifndef CLEONOS_HBOS_H
#define CLEONOS_HBOS_H

#include <cleonos_syscall.h>
#include <stddef.h>

#define HBOS_SCREEN_W 640U
#define HBOS_SCREEN_H 400U
#define HBOS_BYTES_PER_PIXEL 4U
#define HBOS_CONSOLE_COLS 74U
#define HBOS_CONSOLE_ROWS 18U
#define HBOS_CONSOLE_X 24U
#define HBOS_CONSOLE_Y 154U
#define HBOS_CONSOLE_CELL_W 8U
#define HBOS_CONSOLE_CELL_H 10U
#define HBOS_CMD_MAX 128U
#define HBOS_HISTORY_MAX 26U
#define HBOS_MAX_FILES 24U
#define HBOS_MAX_APPS 16U
#define HBOS_KERNEL_PATH "/system/hbos/HARIBOTE.SYS"
#define HBOS_KERNEL_MAX_BYTES (4U * 1024U * 1024U)
#define HBOS_HRB_MAX_FILE_BYTES (2U * 1024U * 1024U)
#define HBOS_HRB_MAX_SEG_BYTES (8U * 1024U * 1024U)
#define HBOS_HRB_STEP_LIMIT 200000U

#define HBOS_COLOR_BG 0x00008484U
#define HBOS_COLOR_PANEL 0x00C6C6C6U
#define HBOS_COLOR_PANEL_DARK 0x00848484U
#define HBOS_COLOR_PANEL_LIGHT 0x00FFFFFFU
#define HBOS_COLOR_TITLE 0x00000084U
#define HBOS_COLOR_TITLE_TEXT 0x00FFFFFFU
#define HBOS_COLOR_TEXT 0x00000000U
#define HBOS_COLOR_CONSOLE_BG 0x00000000U
#define HBOS_COLOR_CONSOLE_TEXT 0x00FFFFFFU
#define HBOS_COLOR_CONSOLE_DIM 0x00A8A8A8U
#define HBOS_COLOR_WARN 0x00FFFF00U
#define HBOS_COLOR_OK 0x0000FF88U

typedef unsigned char hbos_u8;
typedef unsigned int hbos_u32;
typedef unsigned long long hbos_u64;

typedef struct hbos_file {
    const char *name;
    const char *ext;
    const char *type;
    hbos_u32 size;
    const char *content;
} hbos_file;

typedef struct hbos_app hbos_app;
typedef struct hbos_state hbos_state;
typedef struct hbos_hrb_info hbos_hrb_info;

typedef int (*hbos_app_entry)(hbos_state *state, const char *args);

struct hbos_app {
    const char *name;
    const char *display_name;
    const char *description;
    hbos_app_entry entry;
};

struct hbos_hrb_info {
    hbos_u32 file_size;
    hbos_u32 segment_size;
    hbos_u32 entry;
    hbos_u32 esp;
    hbos_u32 data_size;
    hbos_u32 data_offset;
};

struct hbos_state {
    hbos_u8 *haribote_kernel;
    hbos_u32 haribote_kernel_size;
    int haribote_kernel_loaded;
    hbos_u32 *pixels;
    hbos_u32 *present_pixels;
    hbos_u32 present_w;
    hbos_u32 present_h;
    int present_cleared;
    hbos_u32 terminal_flushed;
    int prompt_pending;
    int terminal_only;
    char history[HBOS_HISTORY_MAX][HBOS_CONSOLE_COLS + 1U];
    hbos_u32 history_color[HBOS_HISTORY_MAX];
    hbos_u32 history_count;
    char input[HBOS_CMD_MAX];
    hbos_u32 input_len;
    int running;
    int dirty;
    int show_about;
    hbos_u64 frame_no;
};

int hbos_init(hbos_state *state);
void hbos_shutdown(hbos_state *state);
void hbos_present(hbos_state *state);
void hbos_redraw(hbos_state *state);
void hbos_sleep(hbos_u64 ms);
int hbos_poll_char(void);
void hbos_terminal_write(const char *text);
void hbos_put_history(hbos_state *state, const char *text, hbos_u32 color);
void hbos_put_history_fmt_u32(hbos_state *state, const char *prefix, hbos_u32 value, const char *suffix,
                              hbos_u32 color);
void hbos_execute_line(hbos_state *state, const char *line);
void hbos_console_prompt(hbos_state *state);
void hbos_console_backspace(hbos_state *state);
void hbos_console_input_char(hbos_state *state, char ch);
void hbos_console_submit(hbos_state *state);
void hbos_video_clear(hbos_state *state, hbos_u32 color);
void hbos_video_rect(hbos_state *state, int x, int y, int w, int h, hbos_u32 color);
void hbos_video_frame(hbos_state *state, int x, int y, int w, int h, hbos_u32 light, hbos_u32 dark);
void hbos_video_text(hbos_state *state, int x, int y, const char *text, hbos_u32 fg, hbos_u32 bg, int scale);
void hbos_video_text_limit(hbos_state *state, int x, int y, const char *text, hbos_u32 fg, hbos_u32 bg, int scale,
                           int max_chars);
const hbos_file *hbos_file_at(hbos_u32 index);
hbos_u32 hbos_file_count(void);
const hbos_file *hbos_find_file(const char *name);
hbos_u32 hbos_file_size_for_state(const hbos_state *state, const hbos_file *file);
const char *hbos_file_content_for_state(const hbos_state *state, const hbos_file *file);
void hbos_format_83_name(const hbos_file *file, char *out, hbos_u32 out_size);
const hbos_app *hbos_app_at(hbos_u32 index);
hbos_u32 hbos_app_count(void);
const hbos_app *hbos_find_app(const char *name);
int hbos_run_builtin_app(hbos_state *state, const hbos_app *app, const char *cmdline);
void hbos_ascii_upper(char *text);
int hbos_streq_ci(const char *a, const char *b);
int hbos_starts_with_ci(const char *text, const char *prefix);
const char *hbos_skip_spaces(const char *text);
void hbos_split_first(const char *line, char *cmd, hbos_u32 cmd_size, const char **rest);
int hbos_read_file_alloc(const char *path, hbos_u8 **out_data, hbos_u32 *out_size, hbos_u32 max_size);
void hbos_free_file_alloc(hbos_u8 *data, hbos_u32 size);
int hbos_tek_getsize(unsigned char *data);
int hbos_tek_decomp(unsigned char *data, char *out, int size);
int hbos_hrb_probe(const hbos_u8 *data, hbos_u32 size, hbos_hrb_info *out_info);
int hbos_hrb_info_path(const char *path, hbos_hrb_info *out_info);
int hbos_hrb_run_path(hbos_state *state, const char *path, const char *args);

#endif
