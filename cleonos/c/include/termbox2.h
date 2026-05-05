#ifndef CLEONOS_TERMBOX2_H
#define CLEONOS_TERMBOX2_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TB_VERSION_STR "2.7.0-cleonos"

typedef uint64_t uintattr_t;

#define TB_KEY_CTRL_TILDE       0x00
#define TB_KEY_CTRL_2           0x00
#define TB_KEY_CTRL_A           0x01
#define TB_KEY_CTRL_B           0x02
#define TB_KEY_CTRL_C           0x03
#define TB_KEY_CTRL_D           0x04
#define TB_KEY_CTRL_E           0x05
#define TB_KEY_CTRL_F           0x06
#define TB_KEY_CTRL_G           0x07
#define TB_KEY_BACKSPACE        0x08
#define TB_KEY_CTRL_H           0x08
#define TB_KEY_TAB              0x09
#define TB_KEY_CTRL_I           0x09
#define TB_KEY_CTRL_J           0x0a
#define TB_KEY_CTRL_K           0x0b
#define TB_KEY_CTRL_L           0x0c
#define TB_KEY_ENTER            0x0d
#define TB_KEY_CTRL_M           0x0d
#define TB_KEY_CTRL_N           0x0e
#define TB_KEY_CTRL_O           0x0f
#define TB_KEY_CTRL_P           0x10
#define TB_KEY_CTRL_Q           0x11
#define TB_KEY_CTRL_R           0x12
#define TB_KEY_CTRL_S           0x13
#define TB_KEY_CTRL_T           0x14
#define TB_KEY_CTRL_U           0x15
#define TB_KEY_CTRL_V           0x16
#define TB_KEY_CTRL_W           0x17
#define TB_KEY_CTRL_X           0x18
#define TB_KEY_CTRL_Y           0x19
#define TB_KEY_CTRL_Z           0x1a
#define TB_KEY_ESC              0x1b
#define TB_KEY_SPACE            0x20
#define TB_KEY_BACKSPACE2       0x7f

#define tb_key_i(i)             (0xffffU - (i))
#define TB_KEY_F1               (0xffffU - 0U)
#define TB_KEY_F2               (0xffffU - 1U)
#define TB_KEY_F3               (0xffffU - 2U)
#define TB_KEY_F4               (0xffffU - 3U)
#define TB_KEY_F5               (0xffffU - 4U)
#define TB_KEY_F6               (0xffffU - 5U)
#define TB_KEY_F7               (0xffffU - 6U)
#define TB_KEY_F8               (0xffffU - 7U)
#define TB_KEY_F9               (0xffffU - 8U)
#define TB_KEY_F10              (0xffffU - 9U)
#define TB_KEY_F11              (0xffffU - 10U)
#define TB_KEY_F12              (0xffffU - 11U)
#define TB_KEY_INSERT           (0xffffU - 12U)
#define TB_KEY_DELETE           (0xffffU - 13U)
#define TB_KEY_HOME             (0xffffU - 14U)
#define TB_KEY_END              (0xffffU - 15U)
#define TB_KEY_PGUP             (0xffffU - 16U)
#define TB_KEY_PGDN             (0xffffU - 17U)
#define TB_KEY_ARROW_UP         (0xffffU - 18U)
#define TB_KEY_ARROW_DOWN       (0xffffU - 19U)
#define TB_KEY_ARROW_LEFT       (0xffffU - 20U)
#define TB_KEY_ARROW_RIGHT      (0xffffU - 21U)
#define TB_KEY_BACK_TAB         (0xffffU - 22U)
#define TB_KEY_MOUSE_LEFT       (0xffffU - 23U)
#define TB_KEY_MOUSE_RIGHT      (0xffffU - 24U)
#define TB_KEY_MOUSE_MIDDLE     (0xffffU - 25U)
#define TB_KEY_MOUSE_RELEASE    (0xffffU - 26U)
#define TB_KEY_MOUSE_WHEEL_UP   (0xffffU - 27U)
#define TB_KEY_MOUSE_WHEEL_DOWN (0xffffU - 28U)

#define TB_DEFAULT              0x0000U
#define TB_BLACK                0x0001U
#define TB_RED                  0x0002U
#define TB_GREEN                0x0003U
#define TB_YELLOW               0x0004U
#define TB_BLUE                 0x0005U
#define TB_MAGENTA              0x0006U
#define TB_CYAN                 0x0007U
#define TB_WHITE                0x0008U
#define TB_BOLD                 0x0100U
#define TB_UNDERLINE            0x0200U
#define TB_REVERSE              0x0400U

#define TB_EVENT_KEY            1
#define TB_EVENT_RESIZE         2
#define TB_EVENT_MOUSE          3

#define TB_MOD_ALT              1
#define TB_MOD_CTRL             2
#define TB_MOD_SHIFT            4
#define TB_MOD_MOTION           8

#define TB_INPUT_CURRENT        0
#define TB_INPUT_ESC            1
#define TB_INPUT_ALT            2
#define TB_INPUT_MOUSE          4

#define TB_OUTPUT_CURRENT       0
#define TB_OUTPUT_NORMAL        1
#define TB_OUTPUT_256           2
#define TB_OUTPUT_216           3
#define TB_OUTPUT_GRAYSCALE     4
#define TB_OUTPUT_TRUECOLOR     5

#define TB_OK                   0
#define TB_ERR                 -1
#define TB_ERR_INIT_ALREADY    -2
#define TB_ERR_MEM             -3
#define TB_ERR_NO_EVENT        -4
#define TB_ERR_NOT_INIT        -5
#define TB_ERR_OUT_OF_BOUNDS   -6

struct tb_cell {
    uint32_t ch;
    uintattr_t fg;
    uintattr_t bg;
};

struct tb_event {
    uint8_t type;
    uint8_t mod;
    uint16_t key;
    uint32_t ch;
    int32_t w;
    int32_t h;
    int32_t x;
    int32_t y;
};

int tb_init(void);
int tb_init_file(const char *path);
int tb_init_fd(int ttyfd);
int tb_init_rwfd(int rfd, int wfd);
int tb_shutdown(void);

int tb_width(void);
int tb_height(void);
int tb_clear(void);
int tb_set_clear_attrs(uintattr_t fg, uintattr_t bg);
int tb_present(void);
int tb_invalidate(void);
int tb_set_cursor(int cx, int cy);
int tb_hide_cursor(void);
int tb_set_cell(int x, int y, uint32_t ch, uintattr_t fg, uintattr_t bg);
int tb_peek_event(struct tb_event *event, int timeout_ms);
int tb_poll_event(struct tb_event *event);
int tb_set_input_mode(int mode);
int tb_set_output_mode(int mode);
int tb_print(int x, int y, uintattr_t fg, uintattr_t bg, const char *str);
int tb_printf(int x, int y, uintattr_t fg, uintattr_t bg, const char *fmt, ...);
const char *tb_strerror(int err);
const char *tb_version(void);
int tb_last_errno(void);
struct tb_cell *tb_cell_buffer(void);
int tb_utf8_char_to_unicode(uint32_t *out, const char *c);
int tb_utf8_unicode_to_char(char *out, uint32_t c);

#ifdef __cplusplus
}
#endif

#endif
