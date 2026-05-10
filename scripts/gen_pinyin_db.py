#!/usr/bin/env python3
"""Generate CLeonOS pinyin input-method dictionary.

The runtime dictionary format is intentionally simple:
    key<TAB>candidate candidate candidate

Source dictionaries come from wolfgitpr/csharp-pinyin.  They mainly provide
single characters and some phrases, so we also merge a small CLeonOS-oriented
common phrase patch to cover everyday input such as "caozuo -> 操作".
"""
from __future__ import annotations

import argparse
import collections
import re
import sys
import unicodedata
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SRC = ROOT / "build" / "deps" / "csharp-pinyin" / "Pinyin" / "dict" / "mandarin"
DEFAULT_OUT = ROOT / "ramdisk" / "system" / "inputm" / "pinyin.db"
DEFAULT_BUILD_OUT = ROOT / "build" / "x86_64" / "ramdisk_root" / "system" / "inputm" / "pinyin.db"

# Keep this list small and high-signal. The upstream csharp-pinyin phrase
# dictionary is not a general IME dictionary, so these patch common OS terms and
# words that users reasonably expect from the CLeonOS input method.
COMMON_PHRASES: tuple[tuple[str, str], ...] = (
    ("aizhonghua", "爱中华"),
    ("anzhuang", "安装"),
    ("baocun", "保存"),
    ("biaoti", "标题"),
    ("bianji", "编辑"),
    ("bianyima", "编码"),
    ("biaoqian", "标签"),
    ("biaodan", "表单"),
    ("biaoge", "表格"),
    ("biaoji", "标记"),
    ("bofang", "播放"),
    ("canshu", "参数"),
    ("caozuo", "操作"),
    ("ceshi", "测试"),
    ("chakan", "查看"),
    ("changshi", "尝试"),
    ("chenggong", "成功"),
    ("chengxu", "程序"),
    ("chongqi", "重启"),
    ("chongshi", "重试"),
    ("chuangkou", "窗口"),
    ("chuangjian", "创建"),
    ("cuowu", "错误"),
    ("dakai", "打开"),
    ("denglu", "登录"),
    ("denglu", "登陆"),
    ("diaoshi", "调试"),
    ("dianji", "点击"),
    ("diannao", "电脑"),
    ("diaoyong", "调用"),
    ("dinglan", "顶栏"),
    ("duankou", "端口"),
    ("duankai", "断开"),
    ("duilie", "队列"),
    ("duqu", "读取"),
    ("fanhui", "返回"),
    ("fasong", "发送"),
    ("fenbianlv", "分辨率"),
    ("fenbianlu", "分辨率"),
    ("fuwuqi", "服务器"),
    ("fuzhi", "复制"),
    ("guanbi", "关闭"),
    ("guanji", "关机"),
    ("guanli", "管理"),
    ("guanliyuan", "管理员"),
    ("guangbiao", "光标"),
    ("huancun", "缓存"),
    ("huifu", "恢复"),
    ("huoqu", "获取"),
    ("jiaoben", "脚本"),
    ("jiami", "加密"),
    ("jiancha", "检查"),
    ("jianpan", "键盘"),
    ("jieshou", "接收"),
    ("jieshu", "结束"),
    ("jisuan", "计算"),
    ("jisuanji", "计算机"),
    ("jindu", "进度"),
    ("jinzhi", "禁止"),
    ("kaifa", "开发"),
    ("kongjian", "空间"),
    ("kongzhi", "控制"),
    ("lanjie", "拦截"),
    ("lianjie", "连接"),
    ("liulanqi", "浏览器"),
    ("mingling", "命令"),
    ("mima", "密码"),
    ("mulu", "目录"),
    ("neicun", "内存"),
    ("nihao", "你好"),
    ("peizhi", "配置"),
    ("qidong", "启动"),
    ("qingchu", "清除"),
    ("quanxian", "权限"),
    ("renwu", "任务"),
    ("rizhi", "日志"),
    ("shanchu", "删除"),
    ("shezhi", "设置"),
    ("shibai", "失败"),
    ("shijian", "时间"),
    ("shijie", "世界"),
    ("shili", "示例"),
    ("shiming", "实名"),
    ("shuchu", "输出"),
    ("shuju", "数据"),
    ("shujuku", "数据库"),
    ("shuru", "输入"),
    ("shubiao", "鼠标"),
    ("shuaxin", "刷新"),
    ("sousuo", "搜索"),
    ("tianjia", "添加"),
    ("tishi", "提示"),
    ("tuichu", "退出"),
    ("wangluo", "网络"),
    ("wangye", "网页"),
    ("weixiu", "维修"),
    ("wenjian", "文件"),
    ("wenjianjia", "文件夹"),
    ("wenti", "问题"),
    ("xiaoxi", "消息"),
    ("xianshi", "显示"),
    ("xiazai", "下载"),
    ("xieyi", "协议"),
    ("xieru", "写入"),
    ("xinjian", "新建"),
    ("xitong", "系统"),
    ("xiufu", "修复"),
    ("xuanzhong", "选中"),
    ("xuanze", "选择"),
    ("yanzheng", "验证"),
    ("yingyong", "应用"),
    ("yingyongchengxu", "应用程序"),
    ("yonghu", "用户"),
    ("youjian", "邮件"),
    ("yunxing", "运行"),
    ("zhanghao", "账号"),
    ("zhengchang", "正常"),
    ("zhongduan", "终端"),
    ("zhongwen", "中文"),
    ("zhuce", "注册"),
    ("zhuangtai", "状态"),
    ("zhuomian", "桌面"),
    ("zaijian", "再见"),
    ("zuowen", "作文"),
    ("ziti", "字体"),
)

TONE_TRANSLATION = str.maketrans(
    {
        "ā": "a", "á": "a", "ǎ": "a", "à": "a",
        "ē": "e", "é": "e", "ě": "e", "è": "e",
        "ī": "i", "í": "i", "ǐ": "i", "ì": "i",
        "ō": "o", "ó": "o", "ǒ": "o", "ò": "o",
        "ū": "u", "ú": "u", "ǔ": "u", "ù": "u",
        "ǖ": "u", "ǘ": "u", "ǚ": "u", "ǜ": "u", "ü": "u", "Ü": "u",
        "ń": "n", "ň": "n", "ǹ": "n", "ḿ": "m",
    }
)

CANDIDATE_SEP_RE = re.compile(r"[\s,;/|]+")
VALID_KEY_RE = re.compile(r"^[a-z]+$")


def normalize_pinyin(raw: str) -> str:
    raw = raw.strip().lower().translate(TONE_TRANSLATION)
    # Handle numbered pinyin and decomposed combining marks defensively.
    raw = unicodedata.normalize("NFKD", raw)
    raw = "".join(ch for ch in raw if not unicodedata.combining(ch))
    # Existing CLeonOS dictionaries used plain "u" for ü, e.g. 女 -> nu.
    # Keep that ABI so old muscle-memory keys still work.
    raw = raw.replace("u:", "u").replace("ü", "u")
    raw = re.sub(r"[1-5]", "", raw)
    raw = re.sub(r"[^a-z]+", "", raw)
    return raw


def looks_cjk(text: str) -> bool:
    if not text:
        return False
    for ch in text:
        cp = ord(ch)
        if not (
            0x3400 <= cp <= 0x4DBF
            or 0x4E00 <= cp <= 0x9FFF
            or 0xF900 <= cp <= 0xFAFF
            or 0x20000 <= cp <= 0x2A6DF
            or 0x2A700 <= cp <= 0x2B73F
            or 0x2B740 <= cp <= 0x2B81F
            or 0x2B820 <= cp <= 0x2CEAF
            or 0x2CEB0 <= cp <= 0x2EBEF
            or 0x30000 <= cp <= 0x3134F
            or 0x31350 <= cp <= 0x323AF
        ):
            return False
    return True


def add_candidate(
    db: dict[str, list[str]], seen: dict[str, set[str]], key: str, candidate: str, *, prefer: bool = False
) -> None:
    key = normalize_pinyin(key)
    candidate = candidate.strip()
    if not key or not VALID_KEY_RE.match(key) or not candidate or not looks_cjk(candidate):
        return
    if len(candidate.encode("utf-8")) >= 64:
        return
    bucket = db[key]
    if candidate in seen[key]:
        return
    seen[key].add(candidate)
    if prefer:
        bucket.insert(0, candidate)
    else:
        bucket.append(candidate)


def parse_colon_dict(path: Path, db: dict[str, list[str]], seen: dict[str, set[str]]) -> int:
    count = 0
    if not path.exists():
        return count
    with path.open("r", encoding="utf-8-sig", errors="ignore") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#") or ":" not in line:
                continue
            word, pinyin = line.split(":", 1)
            word = word.strip()
            syllables = [normalize_pinyin(part) for part in CANDIDATE_SEP_RE.split(pinyin) if part.strip()]
            if not word or not syllables:
                continue
            key = "".join(syllables)
            before = len(db[key])
            add_candidate(db, seen, key, word)
            if len(db[key]) != before:
                count += 1
            # For single characters, also include every alternate reading.
            if len(word) == 1:
                for syllable in syllables:
                    before = len(db[syllable])
                    add_candidate(db, seen, syllable, word)
                    if len(db[syllable]) != before:
                        count += 1
    return count


def parse_user_dict(path: Path, db: dict[str, list[str]], seen: dict[str, set[str]]) -> int:
    count = 0
    if not path.exists():
        return count
    with path.open("r", encoding="utf-8-sig", errors="ignore") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#") or ":" not in line:
                continue
            word, pinyin = line.split(":", 1)
            key = "".join(normalize_pinyin(part) for part in CANDIDATE_SEP_RE.split(pinyin) if part.strip())
            before = len(db[key])
            add_candidate(db, seen, key, word)
            if len(db[key]) != before:
                count += 1
    return count


def apply_common_phrases(db: dict[str, list[str]], seen: dict[str, set[str]]) -> int:
    count = 0
    for key, word in COMMON_PHRASES:
        before = len(db[normalize_pinyin(key)])
        add_candidate(db, seen, key, word, prefer=True)
        if len(db[normalize_pinyin(key)]) != before:
            count += 1
    return count


def limit_candidates(db: dict[str, list[str]], max_candidates: int) -> dict[str, list[str]]:
    limited: dict[str, list[str]] = {}
    for key, values in db.items():
        if not values:
            continue
        # Keep source order. The upstream word list already puts common readings
        # early, while COMMON_PHRASES are inserted at the front as priority
        # patches for everyday IME usage.
        limited[key] = values[:max_candidates]
    return limited


def write_db(path: Path, db: dict[str, list[str]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as f:
        f.write("# inputm.name=PinyinCN\n")
        f.write("# inputm.label=PINYIN:\n")
        f.write("# CLeonOS pinyin dictionary generated from wolfgitpr/csharp-pinyin mandarin dictionaries\n")
        f.write("# Source main license: Apache-2.0; mandarin dictionary: CC-CEDICT / CC BY-SA 4.0. See pinyin_LICENSE.txt.\n")
        f.write("# Regenerate with: python scripts/gen_pinyin_db.py\n")
        for key in sorted(db):
            f.write(f"{key}\t{' '.join(db[key])}\n")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Generate CLeonOS pinyin.db")
    parser.add_argument("--src", type=Path, default=DEFAULT_SRC, help="csharp-pinyin mandarin dictionary directory")
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT, help="output pinyin.db path")
    parser.add_argument("--max-candidates", type=int, default=48, help="max candidates stored per key")
    parser.add_argument("--sync-build", action="store_true", help="also update build ramdisk copy when it exists")
    args = parser.parse_args(argv)

    src = args.src
    if not src.exists():
        print(f"error: dictionary directory not found: {src}", file=sys.stderr)
        return 2

    db: dict[str, list[str]] = collections.defaultdict(list)
    seen: dict[str, set[str]] = collections.defaultdict(set)

    phrase_count = parse_colon_dict(src / "phrases_dict.txt", db, seen)
    word_count = parse_colon_dict(src / "word.txt", db, seen)
    # trans_word.txt maps traditional/variant characters to simplified ones.
    # It is for source text normalization, not an IME candidate dictionary.
    trans_word_count = 0
    user_count = parse_user_dict(src / "user_dict.txt", db, seen)
    patch_count = apply_common_phrases(db, seen)

    db = limit_candidates(db, args.max_candidates)
    write_db(args.out, db)

    if args.sync_build and DEFAULT_BUILD_OUT.parent.exists():
        write_db(DEFAULT_BUILD_OUT, db)

    total_candidates = sum(len(values) for values in db.values())
    print(f"generated {args.out}")
    print(f"keys={len(db)} candidates={total_candidates}")
    print(
        "sources: "
        f"phrases={phrase_count} words={word_count} trans_words={trans_word_count} "
        f"user={user_count} patch={patch_count}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
