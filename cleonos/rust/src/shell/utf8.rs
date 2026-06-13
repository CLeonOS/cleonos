use super::*;

pub(crate) fn utf8_is_cont(ch: u8) -> bool {
    ch >= 0x80 && ch <= 0xBF
}

pub(crate) fn utf8_len_from_lead(lead: u8) -> usize {
    if lead < 0x80 {
        1
    } else if lead >= 0xC2 && lead <= 0xDF {
        2
    } else if lead >= 0xE0 && lead <= 0xEF {
        3
    } else if lead >= 0xF0 && lead <= 0xF4 {
        4
    } else {
        1
    }
}

pub(crate) fn utf8_char_len_at(text: &[u8], len: usize, pos: usize) -> usize {
    if pos >= len {
        return 0;
    }
    let need = utf8_len_from_lead(text[pos]);
    if need == 1 || pos + need > len {
        return 1;
    }
    let mut i = 1usize;
    while i < need {
        if !utf8_is_cont(text[pos + i]) {
            return 1;
        }
        i += 1;
    }
    need
}

pub(crate) fn utf8_prev_boundary(text: &[u8], len: usize, mut pos: usize) -> usize {
    if pos == 0 {
        return 0;
    }
    if pos > len {
        pos = len;
    }
    pos -= 1;
    while pos > 0 && utf8_is_cont(text[pos]) {
        pos -= 1;
    }
    pos
}

pub(crate) fn utf8_next_boundary(text: &[u8], len: usize, pos: usize) -> usize {
    if pos >= len {
        return len;
    }
    let adv = utf8_char_len_at(text, len, pos);
    if adv == 0 || pos + adv > len {
        len
    } else {
        pos + adv
    }
}

pub(crate) fn utf8_decode_at(text: &[u8], len: usize, pos: usize, out_adv: &mut usize) -> u32 {
    *out_adv = 0;
    if pos >= len {
        return 0;
    }
    let b0 = text[pos];
    let adv = utf8_char_len_at(text, len, pos);
    *out_adv = adv;
    if adv == 1 {
        return b0 as u32;
    }
    if adv == 2 {
        return (((b0 & 0x1F) as u32) << 6) | ((text[pos + 1] & 0x3F) as u32);
    }
    if adv == 3 {
        return (((b0 & 0x0F) as u32) << 12)
            | (((text[pos + 1] & 0x3F) as u32) << 6)
            | ((text[pos + 2] & 0x3F) as u32);
    }
    (((b0 & 0x07) as u32) << 18)
        | (((text[pos + 1] & 0x3F) as u32) << 12)
        | (((text[pos + 2] & 0x3F) as u32) << 6)
        | ((text[pos + 3] & 0x3F) as u32)
}

pub(crate) fn codepoint_width(cp: u32) -> usize {
    if cp == 0 {
        0
    } else if (cp >= 0x0300 && cp <= 0x036F) || (cp >= 0xFE00 && cp <= 0xFE0F) {
        0
    } else if (cp >= 0x1100 && cp <= 0x115F)
        || (cp >= 0x2E80 && cp <= 0xA4CF)
        || (cp >= 0xAC00 && cp <= 0xD7A3)
        || (cp >= 0xF900 && cp <= 0xFAFF)
        || (cp >= 0xFE10 && cp <= 0xFE19)
        || (cp >= 0xFE30 && cp <= 0xFE6F)
        || (cp >= 0xFF00 && cp <= 0xFF60)
        || (cp >= 0xFFE0 && cp <= 0xFFE6)
        || (cp >= 0x20000 && cp <= 0x3FFFD)
    {
        2
    } else {
        1
    }
}

pub(crate) fn utf8_visual_width(text: &[u8], len: usize) -> usize {
    let mut pos = 0usize;
    let mut cols = 0usize;
    while pos < len {
        let mut adv = 1usize;
        let cp = utf8_decode_at(text, len, pos, &mut adv);
        if adv == 0 {
            break;
        }
        cols += codepoint_width(cp);
        pos += adv;
    }
    cols
}

pub(crate) fn collect_utf8(first: u8, out: &mut [u8; 4]) -> usize {
    out[0] = first;
    let need = utf8_len_from_lead(first);
    let mut len = 1usize;
    while len < need && len < out.len() {
        let next = read_char_blocking();
        out[len] = next;
        len += 1;
        if !utf8_is_cont(next) {
            break;
        }
    }
    len
}

