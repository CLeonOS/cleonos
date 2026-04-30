#ifndef CLEONOS_UWM_UILIB_H
#define CLEONOS_UWM_UILIB_H

#include <cleonos_syscall.h>

typedef unsigned int uwm_ui_color;

typedef struct uwm_ui_surface {
    uwm_ui_color *pixels;
    int width;
    int height;
    int pitch_pixels;
} uwm_ui_surface;

#define UWM_UI_COLOR_WHITE 0x00FFFFFFU
#define UWM_UI_COLOR_TEXT 0x00232323U
#define UWM_UI_COLOR_MUTED 0x00666666U
#define UWM_UI_COLOR_BORDER 0x00D0D0D0U
#define UWM_UI_COLOR_BUTTON 0x00E7E7E7U
#define UWM_UI_COLOR_BUTTON_HOT 0x00D8EBFAU
#define UWM_UI_COLOR_CLOSE 0x00E81123U
#define UWM_UI_COLOR_CONTROL_ACTIVE 0x001A5EA0U
#define UWM_UI_COLOR_CONTROL_INACTIVE 0x00E5E5E5U

#define UWM_UI_CONTROL_MINIMIZE 0
#define UWM_UI_CONTROL_MAXIMIZE 1
#define UWM_UI_CONTROL_CLOSE 2
#define UWM_UI_CONTROL_RESTORE 3

uwm_ui_surface uwm_uilib_surface(uwm_ui_color *pixels, int width, int height, int pitch_pixels);
int uwm_uilib_clampi(int value, int min_value, int max_value);
char uwm_uilib_upper_char(char ch);
u64 uwm_uilib_glyph_mask(char ch);
void uwm_uilib_fill_rect(const uwm_ui_surface *surface, int x, int y, int w, int h, uwm_ui_color color);
void uwm_uilib_stroke_rect(const uwm_ui_surface *surface, int x, int y, int w, int h, uwm_ui_color color);
void uwm_uilib_draw_char(const uwm_ui_surface *surface, int x, int y, char ch, int scale, uwm_ui_color color);
void uwm_uilib_draw_text_limit(const uwm_ui_surface *surface, int x, int y, const char *text, int scale,
                               uwm_ui_color color, int max_x);
void uwm_uilib_draw_text(const uwm_ui_surface *surface, int x, int y, const char *text, int scale,
                         uwm_ui_color color);
void uwm_uilib_draw_button(const uwm_ui_surface *surface, int x, int y, int w, int h, const char *label,
                           uwm_ui_color bg, uwm_ui_color hot_bg, uwm_ui_color text, uwm_ui_color border, int hot);
void uwm_uilib_draw_control_button(const uwm_ui_surface *surface, int x, int y, int w, int h, int active, int kind,
                                   uwm_ui_color text_color);
u64 uwm_uilib_present(u64 window_id, const uwm_ui_surface *surface);

#endif
