#include "doomgeneric.h"
#include "doomkeys.h"

#include "../../include/cleonos_syscall.h"

#define CLEONOS_KEY_LEFT 0x01U
#define CLEONOS_KEY_RIGHT 0x02U
#define CLEONOS_KEY_UP 0x03U
#define CLEONOS_KEY_DOWN 0x04U

#define DG_KEY_QUEUE_CAP 256U

struct dg_key_event {
    int pressed;
    unsigned char key;
};

static struct dg_key_event g_key_queue[DG_KEY_QUEUE_CAP];
static u64 g_key_r = 0ULL;
static u64 g_key_w = 0ULL;

static cleonos_fb_info g_fb_info;
static cleonos_fb_blit_req g_blit_req;
static u64 g_scale = 1ULL;
static u64 g_dst_x = 0ULL;
static u64 g_dst_y = 0ULL;

static void dg_push_key(int pressed, unsigned char key) {
    u64 next = (g_key_w + 1ULL) % DG_KEY_QUEUE_CAP;

    if (next == g_key_r) {
        return;
    }

    g_key_queue[g_key_w].pressed = pressed;
    g_key_queue[g_key_w].key = key;
    g_key_w = next;
}

static int dg_pop_key(int *pressed, unsigned char *key) {
    if (g_key_r == g_key_w || pressed == (int *)0 || key == (unsigned char *)0) {
        return 0;
    }

    *pressed = g_key_queue[g_key_r].pressed;
    *key = g_key_queue[g_key_r].key;
    g_key_r = (g_key_r + 1ULL) % DG_KEY_QUEUE_CAP;
    return 1;
}

static unsigned char dg_translate_key(u64 code) {
    if (code == (u64)CLEONOS_KEY_LEFT) {
        return KEY_LEFTARROW;
    }

    if (code == (u64)CLEONOS_KEY_RIGHT) {
        return KEY_RIGHTARROW;
    }

    if (code == (u64)CLEONOS_KEY_UP) {
        return KEY_UPARROW;
    }

    if (code == (u64)CLEONOS_KEY_DOWN) {
        return KEY_DOWNARROW;
    }

    if (code == (u64)'\r' || code == (u64)'\n') {
        return KEY_ENTER;
    }

    if (code == (u64)8U || code == (u64)127U) {
        return KEY_BACKSPACE;
    }

    if (code == (u64)27U) {
        return KEY_ESCAPE;
    }

    if (code == (u64)'\t') {
        return KEY_TAB;
    }

    if (code >= 32ULL && code <= 126ULL) {
        return (unsigned char)(code & 0xFFULL);
    }

    return 0U;
}

static void dg_poll_keyboard(void) {
    for (;;) {
        u64 code = cleonos_sys_kbd_get_char();
        unsigned char key;

        if (code == (u64)-1) {
            break;
        }

        key = dg_translate_key(code);

        if (key == 0U) {
            continue;
        }

        dg_push_key(1, key);
        dg_push_key(0, key);
    }
}

void DG_Init(void) {
    u64 sx;
    u64 sy;

    g_key_r = 0ULL;
    g_key_w = 0ULL;

    g_fb_info.width = 0ULL;
    g_fb_info.height = 0ULL;
    g_fb_info.pitch = 0ULL;
    g_fb_info.bpp = 0ULL;

    if (cleonos_sys_fb_info(&g_fb_info) == 0ULL || g_fb_info.width == 0ULL || g_fb_info.height == 0ULL ||
        g_fb_info.bpp != 32ULL) {
        return;
    }

    sx = g_fb_info.width / (u64)DOOMGENERIC_RESX;
    sy = g_fb_info.height / (u64)DOOMGENERIC_RESY;
    g_scale = (sx < sy) ? sx : sy;

    if (g_scale == 0ULL) {
        g_scale = 1ULL;
    }

    if (g_scale > 4ULL) {
        g_scale = 4ULL;
    }

    g_dst_x = (g_fb_info.width > ((u64)DOOMGENERIC_RESX * g_scale))
                  ? ((g_fb_info.width - ((u64)DOOMGENERIC_RESX * g_scale)) / 2ULL)
                  : 0ULL;
    g_dst_y = (g_fb_info.height > ((u64)DOOMGENERIC_RESY * g_scale))
                  ? ((g_fb_info.height - ((u64)DOOMGENERIC_RESY * g_scale)) / 2ULL)
                  : 0ULL;

    g_blit_req.pixels_ptr = 0ULL;
    g_blit_req.src_width = (u64)DOOMGENERIC_RESX;
    g_blit_req.src_height = (u64)DOOMGENERIC_RESY;
    g_blit_req.src_pitch_bytes = (u64)DOOMGENERIC_RESX * 4ULL;
    g_blit_req.dst_x = g_dst_x;
    g_blit_req.dst_y = g_dst_y;
    g_blit_req.scale = g_scale;

    (void)cleonos_sys_fb_clear(0x00000000ULL);
}

void DG_DrawFrame(void) {
    dg_poll_keyboard();

    if (g_fb_info.width == 0ULL || g_fb_info.height == 0ULL || DG_ScreenBuffer == (pixel_t *)0) {
        return;
    }

    g_blit_req.pixels_ptr = (u64)(void *)DG_ScreenBuffer;
    (void)cleonos_sys_fb_blit(&g_blit_req);
}

void DG_SleepMs(uint32_t ms) {
    u64 ticks = ((u64)ms + 9ULL) / 10ULL;

    if (ticks == 0ULL && ms != 0U) {
        ticks = 1ULL;
    }

    if (ticks > 0ULL) {
        (void)cleonos_sys_sleep_ticks(ticks);
    }
}

uint32_t DG_GetTicksMs(void) {
    return (uint32_t)(cleonos_sys_timer_ticks() * 10ULL);
}

int DG_GetKey(int *pressed, unsigned char *key) {
    dg_poll_keyboard();
    return dg_pop_key(pressed, key);
}

void DG_SetWindowTitle(const char *title) {
    (void)title;
}

int cl_doom_run_main(int argc, char **argv) {
    doomgeneric_Create(argc, argv);

    for (;;) {
        doomgeneric_Tick();
    }
}
