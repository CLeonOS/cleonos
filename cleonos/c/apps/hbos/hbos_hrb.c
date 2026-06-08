#include "hbos.h"
#include <stdlib.h>

typedef struct hbos_i386_regs {
    hbos_u32 eax;
    hbos_u32 ecx;
    hbos_u32 edx;
    hbos_u32 ebx;
    hbos_u32 esp;
    hbos_u32 ebp;
    hbos_u32 esi;
    hbos_u32 edi;
    hbos_u32 eip;
    hbos_u32 zf;
    hbos_u32 sf;
} hbos_i386_regs;

typedef struct hbos_hrb_runtime {
    hbos_state *state;
    hbos_u8 *code;
    hbos_u32 code_size;
    hbos_u8 *data;
    hbos_u32 data_size;
    hbos_u32 running;
    hbos_u32 unsupported_eip;
    hbos_u32 unsupported_opcode;
} hbos_hrb_runtime;

static hbos_u32 hbos_le32(const hbos_u8 *p) {
    return (hbos_u32)p[0] | ((hbos_u32)p[1] << 8U) | ((hbos_u32)p[2] << 16U) | ((hbos_u32)p[3] << 24U);
}

static void hbos_write_dec(char *out, hbos_u32 out_size, hbos_u32 value) {
    char rev[12];
    hbos_u32 n = 0U;
    hbos_u32 p = 0U;

    if (out == (char *)0 || out_size == 0U) {
        return;
    }
    if (value == 0U) {
        rev[n++] = '0';
    }
    while (value > 0U && n < (hbos_u32)sizeof(rev)) {
        rev[n++] = (char)('0' + (value % 10U));
        value /= 10U;
    }
    while (n > 0U && p + 1U < out_size) {
        out[p++] = rev[--n];
    }
    out[p] = '\0';
}

static void hbos_append(char *out, hbos_u32 out_size, hbos_u32 *pos, const char *text) {
    hbos_u32 i = 0U;
    if (out == (char *)0 || pos == (hbos_u32 *)0 || out_size == 0U || text == (const char *)0) {
        return;
    }
    while (text[i] != '\0' && *pos + 1U < out_size) {
        out[*pos] = text[i];
        *pos = *pos + 1U;
        i++;
    }
    out[*pos] = '\0';
}

static void hbos_append_u32(char *out, hbos_u32 out_size, hbos_u32 *pos, hbos_u32 value) {
    char num[16];
    hbos_write_dec(num, (hbos_u32)sizeof(num), value);
    hbos_append(out, out_size, pos, num);
}

static int hbos_hrb_decode_loaded(hbos_u8 **io_data, hbos_u32 *io_size);

int hbos_read_file_alloc(const char *path, hbos_u8 **out_data, hbos_u32 *out_size, hbos_u32 max_size) {
    hbos_u64 size64;
    hbos_u64 fd;
    hbos_u64 done = 0ULL;
    hbos_u8 *data;

    if (out_data != (hbos_u8 **)0) {
        *out_data = (hbos_u8 *)0;
    }
    if (out_size != (hbos_u32 *)0) {
        *out_size = 0U;
    }
    if (path == (const char *)0 || path[0] == '\0' || out_data == (hbos_u8 **)0 || out_size == (hbos_u32 *)0) {
        return 0;
    }

    size64 = cleonos_sys_fs_stat_size(path);
    if (size64 == (hbos_u64)-1 || size64 == 0ULL || size64 > (hbos_u64)max_size) {
        return 0;
    }

    data = (hbos_u8 *)malloc((size_t)size64);
    if (data == (hbos_u8 *)0) {
        return 0;
    }

    fd = cleonos_sys_fd_open(path, CLEONOS_O_RDONLY, 0ULL);
    if (fd == (hbos_u64)-1) {
        free(data);
        return 0;
    }

    while (done < size64) {
        hbos_u64 got = cleonos_sys_fd_read(fd, data + done, size64 - done);
        if (got == (hbos_u64)-1 || got == 0ULL) {
            (void)cleonos_sys_fd_close(fd);
            free(data);
            return 0;
        }
        done += got;
    }

    (void)cleonos_sys_fd_close(fd);
    *out_data = data;
    *out_size = (hbos_u32)size64;
    return 1;
}

void hbos_free_file_alloc(hbos_u8 *data, hbos_u32 size) {
    (void)size;
    if (data != (hbos_u8 *)0) {
        free(data);
    }
}

static int hbos_hrb_decode_loaded(hbos_u8 **io_data, hbos_u32 *io_size) {
    hbos_u8 *src;
    hbos_u8 *dst;
    int decoded_size;

    if (io_data == (hbos_u8 **)0 || io_size == (hbos_u32 *)0 || *io_data == (hbos_u8 *)0) {
        return 0;
    }
    if (*io_size < 17U) {
        return 1;
    }

    src = *io_data;
    decoded_size = hbos_tek_getsize(src);
    if (decoded_size <= 0) {
        return 1;
    }
    if ((hbos_u32)decoded_size > HBOS_HRB_MAX_FILE_BYTES) {
        return 0;
    }

    dst = (hbos_u8 *)malloc((size_t)decoded_size);
    if (dst == (hbos_u8 *)0) {
        return 0;
    }
    if (hbos_tek_decomp(src, (char *)dst, decoded_size) != 0) {
        free(dst);
        return 0;
    }

    free(src);
    *io_data = dst;
    *io_size = (hbos_u32)decoded_size;
    return 1;
}

int hbos_hrb_probe(const hbos_u8 *data, hbos_u32 size, hbos_hrb_info *out_info) {
    hbos_u32 segment_size;
    hbos_u32 esp;
    hbos_u32 data_size;
    hbos_u32 data_offset;

    if (out_info != (hbos_hrb_info *)0) {
        out_info->file_size = 0U;
        out_info->segment_size = 0U;
        out_info->entry = 0U;
        out_info->esp = 0U;
        out_info->data_size = 0U;
        out_info->data_offset = 0U;
    }
    if (data == (const hbos_u8 *)0 || size < 36U) {
        return 0;
    }
    if (data[0] != 0U || data[4] != 'H' || data[5] != 'a' || data[6] != 'r' || data[7] != 'i') {
        return 0;
    }

    segment_size = hbos_le32(data + 0x00U);
    esp = hbos_le32(data + 0x0cU);
    data_size = hbos_le32(data + 0x10U);
    data_offset = hbos_le32(data + 0x14U);

    if (segment_size == 0U || segment_size > HBOS_HRB_MAX_SEG_BYTES) {
        return 0;
    }
    if (esp > segment_size || data_size > segment_size || esp + data_size > segment_size) {
        return 0;
    }
    if (data_offset > size || data_size > size || data_offset + data_size > size) {
        return 0;
    }

    if (out_info != (hbos_hrb_info *)0) {
        out_info->file_size = size;
        out_info->segment_size = segment_size;
        out_info->entry = 0x1bU;
        out_info->esp = esp;
        out_info->data_size = data_size;
        out_info->data_offset = data_offset;
    }
    return 1;
}

int hbos_hrb_info_path(const char *path, hbos_hrb_info *out_info) {
    hbos_u8 *data;
    hbos_u32 size;
    int ok;

    if (hbos_read_file_alloc(path, &data, &size, HBOS_HRB_MAX_FILE_BYTES) == 0) {
        return 0;
    }
    if (hbos_hrb_decode_loaded(&data, &size) == 0) {
        hbos_free_file_alloc(data, size);
        return 0;
    }
    ok = hbos_hrb_probe(data, size, out_info);
    hbos_free_file_alloc(data, size);
    return ok;
}

static int hbos_code_read8(const hbos_hrb_runtime *rt, hbos_u32 addr, hbos_u8 *out) {
    if (rt == (const hbos_hrb_runtime *)0 || out == (hbos_u8 *)0 || addr >= rt->code_size) {
        return 0;
    }
    *out = rt->code[addr];
    return 1;
}

static int hbos_code_read32(const hbos_hrb_runtime *rt, hbos_u32 addr, hbos_u32 *out) {
    if (rt == (const hbos_hrb_runtime *)0 || out == (hbos_u32 *)0 || addr + 4U > rt->code_size) {
        return 0;
    }
    *out = hbos_le32(rt->code + addr);
    return 1;
}

static int hbos_data_read8(const hbos_hrb_runtime *rt, hbos_u32 addr, hbos_u8 *out) {
    if (rt == (const hbos_hrb_runtime *)0 || out == (hbos_u8 *)0 || addr >= rt->data_size) {
        return 0;
    }
    *out = rt->data[addr];
    return 1;
}

static int hbos_data_write8(hbos_hrb_runtime *rt, hbos_u32 addr, hbos_u8 value) {
    if (rt == (hbos_hrb_runtime *)0 || addr >= rt->data_size) {
        return 0;
    }
    rt->data[addr] = value;
    return 1;
}

static int hbos_data_read32(const hbos_hrb_runtime *rt, hbos_u32 addr, hbos_u32 *out) {
    if (rt == (const hbos_hrb_runtime *)0 || out == (hbos_u32 *)0 || addr + 4U > rt->data_size) {
        return 0;
    }
    *out = hbos_le32(rt->data + addr);
    return 1;
}

static int hbos_data_write32(hbos_hrb_runtime *rt, hbos_u32 addr, hbos_u32 value) {
    if (rt == (hbos_hrb_runtime *)0 || addr + 4U > rt->data_size) {
        return 0;
    }
    rt->data[addr + 0U] = (hbos_u8)(value & 0xffU);
    rt->data[addr + 1U] = (hbos_u8)((value >> 8U) & 0xffU);
    rt->data[addr + 2U] = (hbos_u8)((value >> 16U) & 0xffU);
    rt->data[addr + 3U] = (hbos_u8)((value >> 24U) & 0xffU);
    return 1;
}

static hbos_u32 *hbos_reg32(hbos_i386_regs *regs, hbos_u8 index) {
    switch (index & 7U) {
    case 0U: return &regs->eax;
    case 1U: return &regs->ecx;
    case 2U: return &regs->edx;
    case 3U: return &regs->ebx;
    case 4U: return &regs->esp;
    case 5U: return &regs->ebp;
    case 6U: return &regs->esi;
    default: return &regs->edi;
    }
}

static hbos_u8 hbos_reg8_get(const hbos_i386_regs *regs, hbos_u8 index) {
    switch (index & 7U) {
    case 0U: return (hbos_u8)(regs->eax & 0xffU);
    case 1U: return (hbos_u8)(regs->ecx & 0xffU);
    case 2U: return (hbos_u8)(regs->edx & 0xffU);
    case 3U: return (hbos_u8)(regs->ebx & 0xffU);
    case 4U: return (hbos_u8)((regs->eax >> 8U) & 0xffU);
    case 5U: return (hbos_u8)((regs->ecx >> 8U) & 0xffU);
    case 6U: return (hbos_u8)((regs->edx >> 8U) & 0xffU);
    default: return (hbos_u8)((regs->ebx >> 8U) & 0xffU);
    }
}

static void hbos_reg8_set(hbos_i386_regs *regs, hbos_u8 index, hbos_u8 value) {
    switch (index & 7U) {
    case 0U: regs->eax = (regs->eax & 0xffffff00U) | value; break;
    case 1U: regs->ecx = (regs->ecx & 0xffffff00U) | value; break;
    case 2U: regs->edx = (regs->edx & 0xffffff00U) | value; break;
    case 3U: regs->ebx = (regs->ebx & 0xffffff00U) | value; break;
    case 4U: regs->eax = (regs->eax & 0xffff00ffU) | ((hbos_u32)value << 8U); break;
    case 5U: regs->ecx = (regs->ecx & 0xffff00ffU) | ((hbos_u32)value << 8U); break;
    case 6U: regs->edx = (regs->edx & 0xffff00ffU) | ((hbos_u32)value << 8U); break;
    default: regs->ebx = (regs->ebx & 0xffff00ffU) | ((hbos_u32)value << 8U); break;
    }
}

static int hbos_stack_push(hbos_hrb_runtime *rt, hbos_i386_regs *regs, hbos_u32 value) {
    regs->esp -= 4U;
    return hbos_data_write32(rt, regs->esp, value);
}

static int hbos_stack_pop(hbos_hrb_runtime *rt, hbos_i386_regs *regs, hbos_u32 *out) {
    if (hbos_data_read32(rt, regs->esp, out) == 0) {
        return 0;
    }
    regs->esp += 4U;
    return 1;
}

static int hbos_put_console_char(hbos_state *state, char ch, char *line, hbos_u32 *line_len) {
    if (state == (hbos_state *)0 || line == (char *)0 || line_len == (hbos_u32 *)0) {
        return 0;
    }
    if (ch == '\r') {
        return 1;
    }
    if (ch == '\n') {
        line[*line_len] = '\0';
        hbos_put_history(state, line, HBOS_COLOR_CONSOLE_TEXT);
        *line_len = 0U;
        line[0] = '\0';
        return 1;
    }
    if ((unsigned char)ch < 32U) {
        return 1;
    }
    if (*line_len + 1U >= HBOS_CONSOLE_COLS) {
        line[*line_len] = '\0';
        hbos_put_history(state, line, HBOS_COLOR_CONSOLE_TEXT);
        *line_len = 0U;
    }
    line[*line_len] = ch;
    *line_len = *line_len + 1U;
    line[*line_len] = '\0';
    return 1;
}

static int hbos_put_console_str0(hbos_hrb_runtime *rt, hbos_u32 addr, char *line, hbos_u32 *line_len) {
    hbos_u32 guard = 0U;
    hbos_u8 ch;

    while (guard < rt->data_size) {
        if (hbos_data_read8(rt, addr + guard, &ch) == 0) {
            return 0;
        }
        if (ch == 0U) {
            return 1;
        }
        if (hbos_put_console_char(rt->state, (char)ch, line, line_len) == 0) {
            return 0;
        }
        guard++;
    }
    return 0;
}

static int hbos_put_console_strn(hbos_hrb_runtime *rt, hbos_u32 addr, hbos_u32 len, char *line, hbos_u32 *line_len) {
    hbos_u32 i;
    hbos_u8 ch;
    for (i = 0U; i < len; i++) {
        if (hbos_data_read8(rt, addr + i, &ch) == 0) {
            return 0;
        }
        if (hbos_put_console_char(rt->state, (char)ch, line, line_len) == 0) {
            return 0;
        }
    }
    return 1;
}

static int hbos_hrb_api(hbos_hrb_runtime *rt, hbos_i386_regs *regs, char *line, hbos_u32 *line_len) {
    hbos_u64 key;
    hbos_u64 hz_ticks;

    switch (regs->edx) {
    case 1U:
        return hbos_put_console_char(rt->state, (char)(regs->eax & 0xffU), line, line_len);
    case 2U:
        return hbos_put_console_str0(rt, regs->ebx, line, line_len);
    case 3U:
        return hbos_put_console_strn(rt, regs->ebx, regs->ecx, line, line_len);
    case 4U:
        rt->running = 0U;
        return 1;
    case 15U:
        key = cleonos_sys_kbd_get_char();
        if (key == (hbos_u64)-1) {
            regs->eax = 0xffffffffU;
        } else {
            regs->eax = (hbos_u32)(key & 0xffU);
        }
        return 1;
    case 20U:
        if (regs->eax == 0U) {
            (void)cleonos_sys_audio_stop();
        } else if (cleonos_sys_audio_available() != 0ULL) {
            hz_ticks = (hbos_u64)regs->eax;
            if (hz_ticks > 20ULL && hz_ticks < 20000ULL) {
                (void)cleonos_sys_audio_play_tone(hz_ticks, 8ULL);
            }
        }
        return 1;
    case 27U:
        regs->eax = 0U;
        return 1;
    default:
        rt->unsupported_eip = regs->eip;
        rt->unsupported_opcode = 0x40000000U | regs->edx;
        return 0;
    }
}

static int hbos_decode_modrm_disp(hbos_hrb_runtime *rt, hbos_i386_regs *regs, hbos_u32 at, hbos_u8 modrm,
                                  hbos_u32 *out_addr, hbos_u32 *out_size) {
    hbos_u8 mod = (hbos_u8)((modrm >> 6U) & 3U);
    hbos_u8 rm = (hbos_u8)(modrm & 7U);
    hbos_u32 addr;
    hbos_u32 base_value = 0U;
    hbos_u32 index_value = 0U;
    hbos_u8 b;
    hbos_u8 sib;
    hbos_u8 scale;
    hbos_u8 index;
    hbos_u8 base;
    hbos_u32 disp32;
    hbos_u32 used = 0U;

    if (out_addr == (hbos_u32 *)0 || out_size == (hbos_u32 *)0 || mod == 3U) {
        return 0;
    }
    *out_size = 0U;

    if (rm == 4U) {
        if (hbos_code_read8(rt, at, &sib) == 0) {
            return 0;
        }
        used++;
        scale = (hbos_u8)((sib >> 6U) & 3U);
        index = (hbos_u8)((sib >> 3U) & 7U);
        base = (hbos_u8)(sib & 7U);

        if (index != 4U) {
            index_value = *hbos_reg32(regs, index) << scale;
        }
        if (mod == 0U && base == 5U) {
            if (hbos_code_read32(rt, at + used, &base_value) == 0) {
                return 0;
            }
            used += 4U;
        } else {
            base_value = *hbos_reg32(regs, base);
        }
        addr = base_value + index_value;
        if (mod == 1U) {
            if (hbos_code_read8(rt, at + used, &b) == 0) {
                return 0;
            }
            addr = (hbos_u32)((int)addr + (signed char)b);
            used++;
        } else if (mod == 2U) {
            if (hbos_code_read32(rt, at + used, &disp32) == 0) {
                return 0;
            }
            addr += disp32;
            used += 4U;
        }
        *out_addr = addr;
        *out_size = used;
        return 1;
    }
    if (mod == 0U && rm == 5U) {
        if (hbos_code_read32(rt, at, &addr) == 0) {
            return 0;
        }
        *out_addr = addr;
        *out_size = 4U;
        return 1;
    }

    addr = *hbos_reg32(regs, rm);
    if (mod == 1U) {
        if (hbos_code_read8(rt, at, &b) == 0) {
            return 0;
        }
        addr = (hbos_u32)((int)addr + (signed char)b);
        *out_size = 1U;
    } else if (mod == 2U) {
        if (hbos_code_read32(rt, at, &disp32) == 0) {
            return 0;
        }
        addr += disp32;
        *out_size = 4U;
    }
    *out_addr = addr;
    return 1;
}

static void hbos_set_flags_sub(hbos_i386_regs *regs, hbos_u32 left, hbos_u32 right, hbos_u32 result) {
    (void)left;
    (void)right;
    regs->zf = (result == 0U) ? 1U : 0U;
    regs->sf = ((result & 0x80000000U) != 0U) ? 1U : 0U;
}

static int hbos_i386_step(hbos_hrb_runtime *rt, hbos_i386_regs *regs, char *line, hbos_u32 *line_len) {
    hbos_u32 ip = regs->eip;
    hbos_u8 op;
    hbos_u8 modrm;
    hbos_u32 imm;
    hbos_u32 value;
    hbos_u32 addr;
    hbos_u32 extra;
    hbos_u32 next;
    hbos_u8 reg;
    hbos_u8 rm;

    if (hbos_code_read8(rt, ip, &op) == 0) {
        return 0;
    }

    if (op >= 0xb8U && op <= 0xbfU) {
        if (hbos_code_read32(rt, ip + 1U, &imm) == 0) {
            return 0;
        }
        *hbos_reg32(regs, (hbos_u8)(op - 0xb8U)) = imm;
        regs->eip = ip + 5U;
        return 1;
    }
    if (op >= 0x50U && op <= 0x57U) {
        if (hbos_stack_push(rt, regs, *hbos_reg32(regs, (hbos_u8)(op - 0x50U))) == 0) {
            return 0;
        }
        regs->eip = ip + 1U;
        return 1;
    }
    if (op >= 0x58U && op <= 0x5fU) {
        if (hbos_stack_pop(rt, regs, hbos_reg32(regs, (hbos_u8)(op - 0x58U))) == 0) {
            return 0;
        }
        regs->eip = ip + 1U;
        return 1;
    }
    if (op == 0x90U) {
        regs->eip = ip + 1U;
        return 1;
    }
    if (op == 0xcdU) {
        hbos_u8 intno;
        if (hbos_code_read8(rt, ip + 1U, &intno) == 0) {
            return 0;
        }
        regs->eip = ip + 2U;
        if (intno == 0x40U) {
            return hbos_hrb_api(rt, regs, line, line_len);
        }
        rt->unsupported_eip = ip;
        rt->unsupported_opcode = 0xcd00U | intno;
        return 0;
    }
    if (op == 0xe8U || op == 0xe9U) {
        if (hbos_code_read32(rt, ip + 1U, &imm) == 0) {
            return 0;
        }
        next = ip + 5U;
        if (op == 0xe8U && hbos_stack_push(rt, regs, next) == 0) {
            return 0;
        }
        regs->eip = (hbos_u32)((int)next + (int)imm);
        return 1;
    }
    if (op == 0xebU) {
        hbos_u8 rel8;
        if (hbos_code_read8(rt, ip + 1U, &rel8) == 0) {
            return 0;
        }
        regs->eip = (hbos_u32)((int)(ip + 2U) + (signed char)rel8);
        return 1;
    }
    if (op == 0xc3U) {
        if (hbos_stack_pop(rt, regs, &regs->eip) == 0) {
            rt->running = 0U;
        }
        return 1;
    }
    if (op == 0xc2U) {
        hbos_u8 lo;
        hbos_u8 hi;
        if (hbos_code_read8(rt, ip + 1U, &lo) == 0 || hbos_code_read8(rt, ip + 2U, &hi) == 0) {
            return 0;
        }
        if (hbos_stack_pop(rt, regs, &regs->eip) == 0) {
            rt->running = 0U;
        }
        regs->esp += (hbos_u32)lo | ((hbos_u32)hi << 8U);
        return 1;
    }
    if (op == 0xc9U) {
        regs->esp = regs->ebp;
        if (hbos_stack_pop(rt, regs, &regs->ebp) == 0) {
            return 0;
        }
        regs->eip = ip + 1U;
        return 1;
    }
    if (op == 0x55U) {
        if (hbos_stack_push(rt, regs, regs->ebp) == 0) {
            return 0;
        }
        regs->eip = ip + 1U;
        return 1;
    }
    if (op == 0x68U) {
        if (hbos_code_read32(rt, ip + 1U, &imm) == 0 || hbos_stack_push(rt, regs, imm) == 0) {
            return 0;
        }
        regs->eip = ip + 5U;
        return 1;
    }
    if (op == 0x6aU) {
        hbos_u8 imm8;
        if (hbos_code_read8(rt, ip + 1U, &imm8) == 0) {
            return 0;
        }
        if (hbos_stack_push(rt, regs, (hbos_u32)(int)(signed char)imm8) == 0) {
            return 0;
        }
        regs->eip = ip + 2U;
        return 1;
    }
    if (op == 0x83U || op == 0x81U || op == 0x89U || op == 0x8bU || op == 0x8aU || op == 0x88U || op == 0xc7U ||
        op == 0x01U || op == 0x29U || op == 0x39U) {
        if (hbos_code_read8(rt, ip + 1U, &modrm) == 0) {
            return 0;
        }
        reg = (hbos_u8)((modrm >> 3U) & 7U);
        rm = (hbos_u8)(modrm & 7U);
        next = ip + 2U;

        if ((modrm & 0xc0U) == 0xc0U) {
            hbos_u32 *left = hbos_reg32(regs, rm);
            hbos_u32 *right = hbos_reg32(regs, reg);
            if (op == 0x89U) {
                *left = *right;
            } else if (op == 0x8bU) {
                *right = *left;
            } else if (op == 0x01U) {
                *left += *right;
            } else if (op == 0x29U) {
                *left -= *right;
            } else if (op == 0x39U) {
                value = *left - *right;
                hbos_set_flags_sub(regs, *left, *right, value);
            } else if (op == 0x83U) {
                hbos_u8 imm8;
                if (hbos_code_read8(rt, next, &imm8) == 0) {
                    return 0;
                }
                next++;
                if (reg == 0U) {
                    *left += (hbos_u32)(int)(signed char)imm8;
                } else if (reg == 5U) {
                    *left -= (hbos_u32)(int)(signed char)imm8;
                } else if (reg == 7U) {
                    value = *left - (hbos_u32)(int)(signed char)imm8;
                    hbos_set_flags_sub(regs, *left, (hbos_u32)(int)(signed char)imm8, value);
                } else {
                    return 0;
                }
            } else {
                return 0;
            }
            regs->eip = next;
            return 1;
        }

        if (hbos_decode_modrm_disp(rt, regs, next, modrm, &addr, &extra) == 0) {
            return 0;
        }
        next += extra;

        if (op == 0x89U) {
            if (hbos_data_write32(rt, addr, *hbos_reg32(regs, reg)) == 0) {
                return 0;
            }
        } else if (op == 0x8bU) {
            if (hbos_data_read32(rt, addr, hbos_reg32(regs, reg)) == 0) {
                return 0;
            }
        } else if (op == 0x8aU) {
            hbos_u8 v8;
            if (hbos_data_read8(rt, addr, &v8) == 0) {
                return 0;
            }
            hbos_reg8_set(regs, reg, v8);
        } else if (op == 0x88U) {
            if (hbos_data_write8(rt, addr, hbos_reg8_get(regs, reg)) == 0) {
                return 0;
            }
        } else if (op == 0xc7U) {
            if (reg != 0U || hbos_code_read32(rt, next, &imm) == 0 || hbos_data_write32(rt, addr, imm) == 0) {
                return 0;
            }
            next += 4U;
        } else if (op == 0x83U) {
            hbos_u8 imm8;
            if (hbos_data_read32(rt, addr, &value) == 0 || hbos_code_read8(rt, next, &imm8) == 0) {
                return 0;
            }
            next++;
            if (reg == 0U) {
                value += (hbos_u32)(int)(signed char)imm8;
                if (hbos_data_write32(rt, addr, value) == 0) {
                    return 0;
                }
            } else if (reg == 5U) {
                value -= (hbos_u32)(int)(signed char)imm8;
                if (hbos_data_write32(rt, addr, value) == 0) {
                    return 0;
                }
            } else if (reg == 7U) {
                hbos_set_flags_sub(regs, value, (hbos_u32)(int)(signed char)imm8,
                                   value - (hbos_u32)(int)(signed char)imm8);
            } else {
                return 0;
            }
        } else if (op == 0x81U) {
            if (hbos_data_read32(rt, addr, &value) == 0 || hbos_code_read32(rt, next, &imm) == 0) {
                return 0;
            }
            next += 4U;
            if (reg == 0U) {
                value += imm;
                if (hbos_data_write32(rt, addr, value) == 0) {
                    return 0;
                }
            } else if (reg == 5U) {
                value -= imm;
                if (hbos_data_write32(rt, addr, value) == 0) {
                    return 0;
                }
            } else if (reg == 7U) {
                hbos_set_flags_sub(regs, value, imm, value - imm);
            } else {
                return 0;
            }
        } else {
            return 0;
        }
        regs->eip = next;
        return 1;
    }
    if (op == 0x05U || op == 0x2dU) {
        if (hbos_code_read32(rt, ip + 1U, &imm) == 0) {
            return 0;
        }
        if (op == 0x05U) {
            regs->eax += imm;
        } else {
            regs->eax -= imm;
        }
        regs->eip = ip + 5U;
        return 1;
    }
    if (op == 0x31U || op == 0x33U) {
        if (hbos_code_read8(rt, ip + 1U, &modrm) == 0 || (modrm & 0xc0U) != 0xc0U) {
            return 0;
        }
        reg = (hbos_u8)((modrm >> 3U) & 7U);
        rm = (hbos_u8)(modrm & 7U);
        if (op == 0x31U) {
            *hbos_reg32(regs, rm) ^= *hbos_reg32(regs, reg);
        } else {
            *hbos_reg32(regs, reg) ^= *hbos_reg32(regs, rm);
        }
        regs->zf = (*hbos_reg32(regs, (op == 0x31U) ? rm : reg) == 0U) ? 1U : 0U;
        regs->eip = ip + 2U;
        return 1;
    }
    if (op == 0x3dU) {
        if (hbos_code_read32(rt, ip + 1U, &imm) == 0) {
            return 0;
        }
        hbos_set_flags_sub(regs, regs->eax, imm, regs->eax - imm);
        regs->eip = ip + 5U;
        return 1;
    }
    if (op == 0x74U || op == 0x75U || op == 0x7cU || op == 0x7dU) {
        hbos_u8 rel;
        int take = 0;
        if (hbos_code_read8(rt, ip + 1U, &rel) == 0) {
            return 0;
        }
        if (op == 0x74U) {
            take = (regs->zf != 0U);
        } else if (op == 0x75U) {
            take = (regs->zf == 0U);
        } else if (op == 0x7cU) {
            take = (regs->sf != 0U);
        } else {
            take = (regs->sf == 0U);
        }
        regs->eip = (take != 0) ? (hbos_u32)((int)(ip + 2U) + (signed char)rel) : (ip + 2U);
        return 1;
    }

    rt->unsupported_eip = ip;
    rt->unsupported_opcode = op;
    return 0;
}

static int hbos_hrb_execute(hbos_state *state, const hbos_u8 *file_data, hbos_u32 file_size,
                            const hbos_hrb_info *info, const char *args) {
    hbos_hrb_runtime rt;
    hbos_i386_regs regs;
    hbos_u32 i;
    hbos_u32 steps = 0U;
    char line[HBOS_CONSOLE_COLS + 1U];
    hbos_u32 line_len = 0U;

    (void)args;
    rt.state = state;
    rt.code = (hbos_u8 *)file_data;
    rt.code_size = file_size;
    rt.data_size = info->segment_size;
    rt.data = (hbos_u8 *)malloc((size_t)rt.data_size);
    rt.running = 1U;
    rt.unsupported_eip = 0U;
    rt.unsupported_opcode = 0U;
    if (rt.data == (hbos_u8 *)0) {
        hbos_put_history(state, "runhrb: cannot allocate Haribote data segment", HBOS_COLOR_WARN);
        return 0;
    }
    for (i = 0U; i < rt.data_size; i++) {
        rt.data[i] = 0U;
    }
    for (i = 0U; i < info->data_size; i++) {
        rt.data[info->esp + i] = file_data[info->data_offset + i];
    }

    regs.eax = 0U;
    regs.ecx = 0U;
    regs.edx = 0U;
    regs.ebx = 0U;
    regs.esp = info->esp;
    regs.ebp = info->esp;
    regs.esi = 0U;
    regs.edi = 0U;
    regs.eip = info->entry;
    regs.zf = 0U;
    regs.sf = 0U;
    line[0] = '\0';

    while (rt.running != 0U && steps < HBOS_HRB_STEP_LIMIT) {
        if (hbos_i386_step(&rt, &regs, line, &line_len) == 0) {
            char msg[HBOS_CONSOLE_COLS + 1U];
            hbos_u32 p = 0U;
            hbos_append(msg, (hbos_u32)sizeof(msg), &p, "runhrb: unsupported i386 op/api at eip=");
            hbos_append_u32(msg, (hbos_u32)sizeof(msg), &p, rt.unsupported_eip);
            hbos_append(msg, (hbos_u32)sizeof(msg), &p, " code=");
            hbos_append_u32(msg, (hbos_u32)sizeof(msg), &p, rt.unsupported_opcode);
            hbos_put_history(state, msg, HBOS_COLOR_WARN);
            free(rt.data);
            return 0;
        }
        steps++;
    }

    if (line_len > 0U) {
        line[line_len] = '\0';
        hbos_put_history(state, line, HBOS_COLOR_CONSOLE_TEXT);
    }
    if (steps >= HBOS_HRB_STEP_LIMIT) {
        hbos_put_history(state, "runhrb: stopped by emulator step limit", HBOS_COLOR_WARN);
        free(rt.data);
        return 0;
    }

    free(rt.data);
    return 1;
}

int hbos_hrb_run_path(hbos_state *state, const char *path, const char *args) {
    hbos_u8 *data;
    hbos_u32 size;
    hbos_hrb_info info;
    int ok;

    if (state == (hbos_state *)0 || path == (const char *)0 || path[0] == '\0') {
        hbos_put_history(state, "runhrb: path required", HBOS_COLOR_WARN);
        return 0;
    }
    if (hbos_read_file_alloc(path, &data, &size, HBOS_HRB_MAX_FILE_BYTES) == 0) {
        hbos_put_history(state, "runhrb: cannot read HRB file", HBOS_COLOR_WARN);
        return 0;
    }
    if (hbos_hrb_decode_loaded(&data, &size) == 0) {
        hbos_free_file_alloc(data, size);
        hbos_put_history(state, "runhrb: OSASKCMP/TEK decode failed", HBOS_COLOR_WARN);
        return 0;
    }
    if (hbos_hrb_probe(data, size, &info) == 0) {
        hbos_free_file_alloc(data, size);
        hbos_put_history(state, "runhrb: .hrb file format error", HBOS_COLOR_WARN);
        return 0;
    }

    hbos_put_history(state, "[hbos] running real Haribote HRB in user-mode i386 emulator", HBOS_COLOR_CONSOLE_DIM);
    ok = hbos_hrb_execute(state, data, size, &info, args);
    hbos_free_file_alloc(data, size);
    return ok;
}
