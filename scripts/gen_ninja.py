#!/usr/bin/env python3
import os
import shlex
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NINJA = ROOT / "build.ninja"


def norm(path):
    return str(path).replace("\\", "/")


def r(path):
    return norm(ROOT / path)


def q(value):
    return shlex.quote(str(value))


def nesc(value):
    return str(value).replace("$", "$$").replace(":", "$:")


def vesc(value):
    return str(value).replace("$", "$$")


def uniq(items):
    out = []
    seen = set()
    for item in items:
        if not item or item in seen:
            continue
        seen.add(item)
        out.append(item)
    return out


def collect(directory, recursive=True, suffixes=(".c", ".cpp", ".cc", ".cxx", ".S"), exclude_names=()):
    base = ROOT / directory if not Path(directory).is_absolute() else Path(directory)
    if not base.exists():
        return []
    iterator = base.rglob("*") if recursive else base.glob("*")
    excluded = set(exclude_names)
    return sorted(norm(p) for p in iterator if p.is_file() and p.suffix in suffixes and p.name not in excluded)


def child_dirs(directory):
    base = ROOT / directory
    if not base.exists():
        return {}
    return {p.name: norm(p) for p in base.iterdir() if p.is_dir()}


def rel(path):
    p = norm(path)
    root = norm(ROOT)
    if p == root:
        return "."
    prefix = root + "/"
    if p.startswith(prefix):
        return p[len(prefix):]
    return p


def obj_for(obj_root, source, scope=None):
    source_rel = rel(source)
    stem = os.path.splitext(source_rel)[0] + ".o"
    if scope:
        stem = f"__apps/{scope}/{stem}"
    return norm(Path(obj_root) / stem)


def is_cxx(source):
    return Path(source).suffix in (".cpp", ".cc", ".cxx", ".C")


def is_asm(source):
    return Path(source).suffix == ".S"


TOOLS = {
    "CC": os.environ.get("CC", "gcc"),
    "KERNEL_CXX": os.environ.get("KERNEL_CXX", "g++"),
    "USER_CXX": os.environ.get("USER_CXX", "g++"),
    "LD": os.environ.get("LD", "ld"),
    "RUSTC": os.environ.get("RUSTC", "rustc"),
    "NM": os.environ.get("NM", "nm"),
    "TAR": os.environ.get("TAR", "tar"),
    "XORRISO": os.environ.get("XORRISO", "xorriso"),
    "QEMU_X86_64": os.environ.get("QEMU_X86_64", "qemu-system-x86_64"),
    "UEFI_CC": os.environ.get("UEFI_CC", "x86_64-w64-mingw32-gcc"),
    "PYTHON": os.environ.get("PYTHON", "python3"),
}
MENUCONFIG_PRESET = os.environ.get("MENUCONFIG_PRESET", "full")
MENUCONFIG_ARGS = os.environ.get("MENUCONFIG_ARGS", "")

FREESTANDING_ASSERT = r("build/bdt-freestanding/include/assert.h")
MENUCONFIG_HEADER = r("build/bdt-menuconfig/clks_config.h")
KCONFIG_FILE = r("configs/menuconfig/Kconfig")
KCONFIG_DOTCONFIG = r("configs/menuconfig/.config")
KCONFIG_BDT_CONFIG = r("configs/menuconfig/config.clks.bdt")
KCONFIG_SYNC = r("scripts/kconfig_sync.py")

KERNEL_CFLAGS = [
    "-std=c11", "-ffreestanding", "-fno-stack-protector", "-fno-builtin", "-U_FORTIFY_SOURCE",
    "-D_FORTIFY_SOURCE=0", "-fcf-protection=none", "-g", "-Wall", "-Wextra", "-Werror", "-Wno-error=unused-variable",
    "-Wno-error=unused-parameter", "-Wno-error=unused-function", f"-I{r('build/bdt-freestanding/include')}",
    f"-I{r('clks/include')}", "-include", MENUCONFIG_HEADER, "-m64", "-mno-red-zone", "-mcmodel=kernel",
    "-fno-pic", "-fno-pie", "-fno-PIE",
]
KERNEL_CXXFLAGS = [
    "-std=c++17", "-ffreestanding", "-fno-stack-protector", "-fno-builtin", "-U_FORTIFY_SOURCE",
    "-D_FORTIFY_SOURCE=0", "-fcf-protection=none", "-fno-exceptions", "-fno-rtti", "-fno-threadsafe-statics",
    "-fno-use-cxa-atexit", "-g", "-Wall", "-Wextra", "-Werror", "-Wno-error=unused-variable",
    "-Wno-error=unused-parameter", "-Wno-error=unused-function", f"-I{r('build/bdt-freestanding/include')}",
    f"-I{r('clks/include')}", "-include", MENUCONFIG_HEADER, "-m64", "-mno-red-zone", "-mcmodel=kernel",
    "-fno-pic", "-fno-pie", "-fno-PIE",
]
KERNEL_ASFLAGS = [
    "-ffreestanding", "-fcf-protection=none", f"-I{r('clks/include')}", "-include", MENUCONFIG_HEADER, "-m64", "-mno-red-zone",
    "-mcmodel=kernel", "-fno-pic", "-fno-pie", "-fno-PIE",
]
KERNEL_LDFLAGS = ["-nostdlib", "-z", "max-page-size=0x1000"]

USER_CFLAGS = [
    "-std=c11", "-ffreestanding", "-fno-stack-protector", "-fno-builtin", "-Wall", "-Wextra",
    f"-I{r('cleonos/c/include')}", f"-I{r('cleonos/c/apps')}",
]
USER_CXXFLAGS = [
    "-std=c++17", "-ffreestanding", "-fno-stack-protector", "-fno-builtin", "-fno-exceptions", "-fno-rtti",
    "-fno-threadsafe-statics", "-fno-use-cxa-atexit", "-Wall", "-Wextra", f"-I{r('cleonos/c/include')}",
    f"-I{r('cleonos/c/apps')}", f"-I{r('cleonos/third-party/StardustUI')}",
    f"-I{r('cleonos/third-party/StardustUI/includes')}", "-DSTARDUSTUI_CLEONOS",
]
USER_LDFLAGS = ["-nostdlib", "-z", "max-page-size=0x1000", "--gc-sections"]

TLS_CFLAGS = [f"-I{r('cleonos/third-party/mbedtls/include')}", "-DMBEDTLS_CONFIG_FILE='<tls/cleonos_mbedtls_config.h>'", "-DMBEDTLS_HAVE_INT32"]
GUMBO_CFLAGS = [f"-I{r('cleonos/third-party/litehtml/src/gumbo/include')}", f"-I{r('cleonos/third-party/litehtml/src/gumbo/include/gumbo')}", f"-I{r('cleonos/third-party/litehtml/src/gumbo')}", "-DNDEBUG"]
DOOM_CFLAGS = [f"-I{r('cleonos/third-party/doomgeneric/doomgeneric')}", f"-I{r('cleonos/c/apps/doom')}", "-include", r("cleonos/c/apps/doom/doom_shim.h"), "-DNORMALUNIX", "-D_DEFAULT_SOURCE"]
LUA_CFLAGS = [f"-I{r('cleonos/c/apps/lua')}", f"-I{r('cleonos/third-party/lua/src')}", "-include", r("cleonos/c/apps/lua/lua_cleonos_config.h"), "-include", r("cleonos/c/apps/lua/lua_cleonos_compat.h")]
ZLIB_CFLAGS = [f"-I{r('cleonos/third-party/zlib')}", "-DHAVE_MEMCPY", "-DNO_GZIP", "-DNO_GZCOMPRESS"]
MINIZIP_CFLAGS = ZLIB_CFLAGS + [f"-I{r('cleonos/third-party/zlib/contrib/minizip')}", "-DNO_ERRNO_H", "-DNOUNCRYPT"]
LIBPNG_CFLAGS = [f"-I{r('cleonos/third-party/libpng')}"] + ZLIB_CFLAGS
CJSON_CFLAGS = [f"-I{r('cleonos/third-party/cJSON')}", f"-I{r('cleonos/c/pkg')}", "-include", r("cleonos/c/pkg/pkg_cjson_config.h"), "-mstackrealign"]
STB_CFLAGS = [f"-I{r('cleonos/third-party/stb')}", f"-I{r('cleonos/c/apps/stb')}", "-mstackrealign", "-Wno-unused-parameter"]
TCC_CFLAGS = [f"-I{r('cleonos/c/apps/tcc')}", f"-I{r('cleonos/third-party/tinycc')}", f"-I{r('cleonos/third-party/tinycc/include')}", "-include", r("cleonos/c/apps/tcc/tcc_cleonos_compat.h"), "-DTCC_TARGET_X86_64", "-DCONFIG_TCC_STATIC", "-DONE_SOURCE=1", "-DNDEBUG", "-U_WIN32", "-U__x86_64__", "-U__amd64__", "-Wno-unused-parameter", "-Wno-sign-compare", "-Wno-missing-field-initializers", "-Wno-implicit-fallthrough", "-Wno-format", "-Wno-return-type"]
BDT_CFLAGS = [f"-I{r('cleonos/c/apps/bdt')}", f"-I{r('bdt/src')}", "-include", r("cleonos/c/apps/bdt/bdt_cleonos_compat.h"), "-DBDT_MAX_QUICK_MANIFESTS=64", "-Wno-unused-parameter", "-Wno-sign-compare"]
TERMBOX2_CFLAGS = [f"-I{r('cleonos/c/include')}"]
SQLITE_CFLAGS = [f"-I{r('cleonos/third-party/sqlite')}", "-DSQLITE_OS_OTHER=1", "-DSQLITE_THREADSAFE=0", "-DSQLITE_OMIT_LOAD_EXTENSION=1", "-DSQLITE_OMIT_WAL=1", "-DSQLITE_TEMP_STORE=3", "-DSQLITE_OMIT_SHARED_CACHE=1", "-DSQLITE_OMIT_LOCALTIME=1", "-DSQLITE_OMIT_UTF16=1", "-DSQLITE_DEFAULT_MEMSTATUS=0"]
FASTFETCH_CFLAGS = [f"-I{r('cleonos/c/apps/fastfetch')}", f"-I{r('cleonos/third-party/fastfetch/src')}", f"-I{r('cleonos/third-party/fastfetch/src/3rdparty/yyjson')}", "-include", r("cleonos/c/apps/fastfetch/fastfetch_cleonos_compat.h"), "-D_GNU_SOURCE", "-DFASTFETCH_VERSION='\"2.62.1\"'", "-Wno-unused-parameter", "-Wno-unused-function", "-Wno-sign-compare", "-Wno-missing-field-initializers", "-Wno-incompatible-pointer-types"]

TLS_SOURCES = [r(f"cleonos/third-party/mbedtls/library/{name}.c") for name in [
    "aes", "asn1parse", "asn1write", "bignum", "bignum_core", "bignum_mod", "bignum_mod_raw", "cipher",
    "cipher_wrap", "constant_time", "ctr_drbg", "ecdh", "ecdsa", "ecp", "ecp_curves", "entropy", "error",
    "gcm", "md", "oid", "pk", "pk_ecc", "pk_wrap", "pkparse", "platform", "platform_util", "rsa",
    "rsa_alt_helpers", "sha256", "sha512", "ssl_ciphersuites", "ssl_client", "ssl_msg", "ssl_tls",
    "ssl_tls12_client", "version_features", "x509", "x509_crt",
]] + [r("cleonos/c/apps/tls/cleonos_tls.c"), r("cleonos/c/apps/tls/cleonos_tls_entropy.c")]
LUA_SOURCES = [r("cleonos/c/apps/lua/lua_cleonos_compat.c"), r("cleonos/c/apps/lua/lua_cleonos_libs.c"), r("cleonos/c/apps/lua/lua_cleonos_setjmp.c")] + [r(f"cleonos/third-party/lua/src/{name}.c") for name in [
    "lapi", "lauxlib", "lbaselib", "lcode", "lcorolib", "lctype", "ldblib", "ldebug", "ldo", "ldump",
    "lfunc", "lgc", "llex", "lmem", "lobject", "lopcodes", "lparser", "lstate", "lstring", "lstrlib",
    "ltable", "ltablib", "ltm", "lundump", "lutf8lib", "lvm", "lzio",
]]
ZLIB_SOURCES = [r(f"cleonos/third-party/zlib/{name}.c") for name in ["adler32", "compress", "crc32", "deflate", "inffast", "inflate", "inftrees", "trees", "uncompr", "zutil"]]
MINIZIP_UNZIP_SOURCES = [r("cleonos/c/apps/minizip_cleonos_io.c"), r("cleonos/third-party/zlib/contrib/minizip/unzip.c")]
MINIZIP_ZIP_SOURCES = [r("cleonos/c/apps/minizip_cleonos_io.c"), r("cleonos/c/apps/minizip_cleonos_time.c"), r("cleonos/c/apps/minizip_cleonos_setjmp.c"), r("cleonos/c/apps/minizip_cleonos_assert.c"), r("cleonos/third-party/zlib/contrib/minizip/zip.c")]
LIBPNG_SOURCES = [r(f"cleonos/third-party/libpng/{name}.c") for name in ["png", "pngerror", "pngget", "pngmem", "pngread", "pngrio", "pngrtran", "pngrutil", "pngset", "pngtrans"]]
CJSON_SOURCES = [r("cleonos/third-party/cJSON/cJSON.c"), r("cleonos/c/pkg/pkg_cjson_compat.c")]
STB_SOURCES = [r("cleonos/c/apps/stb/stb_image_cleonos.c")]
SQLITE_SOURCES = [r("cleonos/c/apps/sqlite/sqlite_cleonos_vfs.c"), r("cleonos/third-party/sqlite/sqlite3.c")]
PKG_SOURCES = [r(f"cleonos/c/pkg/{name}.c") for name in ["pkg_state", "pkg_util", "pkg_json", "pkg_fs", "pkg_sha256", "pkg_manifest", "pkg_remote", "pkg_install", "pkg_check", "pkg_commands", "pkg_client", "pkg_sqlite"]]
BDT_SOURCES = [r("cleonos/c/apps/bdt/bdt_cleonos_compat.c")] + [r(f"bdt/src/{name}.c") for name in ["appbuilder", "bench", "cache", "config", "diagnostics", "executor", "graph", "hash", "lang", "log", "plugin", "project", "scanner", "shared_cache", "status", "toolchain", "trace", "util", "view"]]
STARDUSTUI_SOURCES = [r("cleonos/c/src/cxx_runtime.cpp")] + [r(f"cleonos/third-party/StardustUI/src/{name}.cpp") for name in ["file", "window", "components/base", "components/button", "components/canvas", "components/flex", "components/lable", "components/scrollbar", "components/textbox", "platforms/cleonos"]]
FASTFETCH_SOURCES = [r(f"cleonos/c/apps/fastfetch/{name}.c") for name in ["fastfetch_cleonos_setjmp", "fastfetch_cleonos_compat", "fastfetch_cli_entry", "fastfetch_cleonos_platform", "fastfetch_cleonos_detection", "fastfetch_cleonos_modules_registry", "fastfetch_cleonos_stubs"]] + [r("cleonos/third-party/fastfetch/src/3rdparty/yyjson/yyjson.c")] + [r(f"cleonos/third-party/fastfetch/src/{name}.c") for name in [
    "common/impl/commandoption", "common/impl/jsonconfig", "common/impl/duration", "common/impl/font", "common/impl/format",
    "common/impl/frequency", "common/impl/init", "common/impl/library", "common/impl/option", "common/impl/parsing",
    "common/impl/percent", "common/impl/printing", "common/impl/properties", "common/impl/settings", "common/impl/size",
    "common/impl/temps", "common/impl/time", "common/impl/edidHelper", "common/impl/base64", "common/impl/FFlist",
    "common/impl/FFstrbuf", "common/impl/path", "common/impl/FFPlatform", "common/impl/smbios", "common/impl/wcwidth",
    "detection/version/version", "logo/logo", "logo/builtin", "options/display", "options/general", "options/logo",
    "modules/title/title", "modules/separator/separator", "modules/os/os", "modules/host/host", "modules/kernel/kernel",
    "modules/uptime/uptime", "modules/shell/shell", "modules/terminal/terminal", "modules/terminalsize/terminalsize",
    "modules/memory/memory", "modules/disk/disk", "modules/locale/locale", "modules/colors/colors", "modules/version/version",
]]

OUTPUT_GROUPS = {
    "default": {"output": r("build/x86_64/user/apps"), "linker": r("cleonos/c/user.ld"), "apps": []},
    "driver": {"output": r("build/x86_64/user/apps/driver"), "linker": r("cleonos/c/user.ld"), "apps": ["ttydrv", "serialdrv", "fbdrv", "pcspeakerdrv", "diskdrv", "netdrv", "kbddrv", "mousedrv"]},
    "uwm": {"output": r("build/x86_64/user/apps/uwm"), "linker": r("cleonos/c/user.ld"), "apps": ["file_explorer", "terminal", "taskmgr", "pkg_gui", "uwm_uilib", "stardust_helloworld", "stardust_layout"]},
    "inputm": {"output": r("build/x86_64/user/apps/inputm"), "linker": r("cleonos/c/user.ld"), "apps": ["pinyin", "romaji", "emoji", "symbols"]},
    "system": {"output": r("build/x86_64/user/system"), "linker": r("cleonos/c/kelf.ld"), "apps": []},
}

APP_RULES = {
    "shell": {"include_runtime": False, "sources": [r("cleonos/c/apps/user/cleonos_user.c")]},
    "browser": {"cflags": TLS_CFLAGS + GUMBO_CFLAGS, "sources": TLS_SOURCES, "source_dirs": [r("cleonos/third-party/litehtml/src/gumbo")]},
    "httpget": {"cflags": TLS_CFLAGS, "sources": TLS_SOURCES},
    "wget": {"cflags": TLS_CFLAGS, "sources": TLS_SOURCES},
    "webconsole": {"cflags": SQLITE_CFLAGS + CJSON_CFLAGS, "sources": CJSON_SOURCES + SQLITE_SOURCES + PKG_SOURCES},
    "lua": {"cflags": LUA_CFLAGS, "sources": LUA_SOURCES},
    "zlibtest": {"cflags": ZLIB_CFLAGS, "sources": ZLIB_SOURCES},
    "unzip": {"cflags": MINIZIP_CFLAGS, "sources": MINIZIP_UNZIP_SOURCES + ZLIB_SOURCES},
    "zip": {"cflags": MINIZIP_CFLAGS, "sources": MINIZIP_ZIP_SOURCES + ZLIB_SOURCES},
    "pngtest": {"cflags": LIBPNG_CFLAGS, "sources": LIBPNG_SOURCES + ZLIB_SOURCES},
    "stbtest": {"cflags": STB_CFLAGS, "sources": STB_SOURCES},
    "imgview": {"cflags": LIBPNG_CFLAGS + STB_CFLAGS, "sources": LIBPNG_SOURCES + ZLIB_SOURCES + STB_SOURCES},
    "tcc": {"cflags": TCC_CFLAGS, "sources": [r("cleonos/c/apps/tcc/tcc_cleonos_compat.c"), r("cleonos/c/apps/minizip_cleonos_setjmp.c")]},
    "bdt": {"cflags": BDT_CFLAGS, "sources": BDT_SOURCES},
    "termbox2": {"cflags": TERMBOX2_CFLAGS},
    "termboxdemo": {"cflags": TERMBOX2_CFLAGS},
    "sqlitetest": {"cflags": SQLITE_CFLAGS, "sources": SQLITE_SOURCES},
    "pkg": {"cflags": CJSON_CFLAGS + SQLITE_CFLAGS, "sources": CJSON_SOURCES + SQLITE_SOURCES + PKG_SOURCES},
    "pkg_gui": {"cflags": CJSON_CFLAGS + SQLITE_CFLAGS, "sources": CJSON_SOURCES + SQLITE_SOURCES + PKG_SOURCES},
    "stardust_helloworld": {"cflags": ["-DSTARDUSTUI_CLEONOS"], "sources": STARDUSTUI_SOURCES + [r("cleonos/third-party/StardustUI/examples/helloworld/helloworld.cpp")]},
    "stardust_layout": {"cflags": ["-DSTARDUSTUI_CLEONOS"], "sources": STARDUSTUI_SOURCES + [r("cleonos/third-party/StardustUI/examples/layout/layout.cpp")]},
    "install2disk": {"sources": [r("cleonos/c/apps/user/cleonos_user.c")]},
    "leonfetch": {"sources": [r("cleonos/c/apps/user/cleonos_user.c")]},
    "fastfetch": {"cflags": FASTFETCH_CFLAGS, "sources": FASTFETCH_SOURCES},
    "passwd": {"sources": [r("cleonos/c/apps/user/cleonos_user.c")]},
    "useradd": {"sources": [r("cleonos/c/apps/user/cleonos_user.c")]},
    "userdel": {"sources": [r("cleonos/c/apps/user/cleonos_user.c")]},
    "usermod": {"sources": [r("cleonos/c/apps/user/cleonos_user.c")]},
    "doom": {"cflags": DOOM_CFLAGS, "source_dirs": [r("cleonos/third-party/doomgeneric/doomgeneric")], "exclude_sources": ["doomgeneric_allegro.c", "doomgeneric_emscripten.c", "doomgeneric_linuxvt.c", "doomgeneric_sdl.c", "doomgeneric_soso.c", "doomgeneric_sosox.c", "doomgeneric_win.c", "doomgeneric_xlib.c", "i_allegromusic.c", "i_allegrosound.c", "i_sdlmusic.c", "i_sdlsound.c", "icon.c"]},
    "ttydrv": {"include_runtime": False},
    "serialdrv": {"include_runtime": False},
    "fbdrv": {"include_runtime": False},
    "pcspeakerdrv": {"include_runtime": False},
    "diskdrv": {"include_runtime": False},
    "netdrv": {"include_runtime": False},
    "kbddrv": {"include_runtime": False},
    "mousedrv": {"include_runtime": False},
}

RUNTIME_SOURCES = [r("cleonos/c/apps/cmd_runtime.c"), r("cleonos/c/apps/uwm_uilib/uwm_uilib.c")]
RUNTIME_EXCLUDE_APPS = {"shell"}

KERNEL_COMMON_C_SOURCES = [
    "clks/kernel/core/interrupts.c",
    "clks/kernel/core/kmain.c",
    "clks/kernel/core/log.c",
    "clks/kernel/core/panic.c",
    "clks/kernel/core/scheduler.c",
    "clks/kernel/core/service.c",
    "clks/kernel/hal/audio/pcspeaker.c",
    "clks/kernel/hal/serial/serial.c",
    "clks/kernel/hal/video/font8x8.c",
    "clks/kernel/hal/video/framebuffer.c",
    "clks/kernel/hal/video/psf_font.c",
    "clks/kernel/input/inputm.c",
    "clks/kernel/input/keyboard.c",
    "clks/kernel/input/mouse.c",
    "clks/kernel/interface/bootsplash.c",
    "clks/kernel/interface/desktop.c",
    "clks/kernel/interface/display.c",
    "clks/kernel/interface/shell.c",
    "clks/kernel/interface/tty.c",
    "clks/kernel/interface/wm.c",
    "clks/kernel/memory/heap.c",
    "clks/kernel/memory/pmm.c",
    "clks/kernel/memory/vm.c",
    "clks/kernel/runtime/driver.c",
    "clks/kernel/runtime/elf64.c",
    "clks/kernel/runtime/elfrunner.c",
    "clks/kernel/runtime/exec.c",
    "clks/kernel/runtime/kelf.c",
    "clks/kernel/runtime/locale.c",
    "clks/kernel/runtime/net.c",
    "clks/kernel/runtime/pty.c",
    "clks/kernel/runtime/syscall.c",
    "clks/kernel/runtime/user.c",
    "clks/kernel/runtime/userland.c",
    "clks/kernel/storage/disk.c",
    "clks/kernel/storage/fs.c",
    "clks/kernel/storage/ramdisk.c",
    "clks/kernel/support/libc_compat.c",
    "clks/kernel/support/string.c",
    "clks/arch/x86_64/startup/boot.c",
    "clks/third_party/miniz/miniz_support.c",
    "clks/third_party/miniz/miniz_tdef.c",
    "clks/third_party/qrcodegen/qrcodegen.c",
]
KERNEL_CXX_SOURCES = ["clks/kernel/core/panic_qr.cpp"]
KERNEL_ASM_SOURCES = [
    "clks/arch/x86_64/interrupt/interrupt_stubs.S",
    "clks/arch/x86_64/startup/exec_stack_call.S",
]


class NinjaWriter:
    def __init__(self):
        self.lines = []
        self.objects = set()
        self.rules_out = set()

    def line(self, text=""):
        self.lines.append(text)

    def build(self, outputs, rule, inputs=None, implicit=None, variables=None):
        if isinstance(outputs, str):
            outputs = [outputs]
        inputs = inputs or []
        implicit = implicit or []
        variables = variables or {}
        primary = outputs[0] if outputs else ""
        if primary in self.rules_out:
            return
        if primary:
            self.rules_out.add(primary)
        lhs = " ".join(nesc(o) for o in outputs)
        rhs = " ".join(nesc(i) for i in inputs)
        if implicit:
            rhs += " | " + " ".join(nesc(i) for i in implicit)
        self.line(f"build {lhs}: {rule}" + (f" {rhs}" if rhs else ""))
        for key, value in variables.items():
            self.line(f"  {key} = {vesc(value)}")
        self.line()

    def compile_obj(self, source, obj, cflags=None, cxxflags=None, tool=None, cxx_tool=None, implicit=None, label=None):
        if obj in self.objects:
            return obj
        self.objects.add(obj)
        if is_cxx(source):
            rule = "cxx"
            compiler = cxx_tool or TOOLS["USER_CXX"]
            flags = cxxflags if cxxflags is not None else USER_CXXFLAGS
        else:
            rule = "cc"
            compiler = tool or TOOLS["CC"]
            flags = cflags if cflags is not None else USER_CFLAGS
            if is_asm(source):
                flags = cflags if cflags is not None else KERNEL_ASFLAGS
        self.build(obj, rule, [source], implicit=implicit or [], variables={
            "cc": compiler,
            "cflags": " ".join(flags),
            "label": label or rel(source),
        })
        return obj


def add_rules(nw):
    nw.line("ninja_required_version = 1.10")
    nw.line()
    nw.line("rule cc")
    nw.line("  command = mkdir -p \"$$(dirname \"$out\")\" && $cc $cflags -MMD -MF $out.d -c $in -o $out")
    nw.line("  depfile = $out.d")
    nw.line("  deps = gcc")
    nw.line("  description = CC $label")
    nw.line()
    nw.line("rule cxx")
    nw.line("  command = mkdir -p \"$$(dirname \"$out\")\" && $cc $cflags -MMD -MF $out.d -c $in -o $out")
    nw.line("  depfile = $out.d")
    nw.line("  deps = gcc")
    nw.line("  description = CXX $label")
    nw.line()
    nw.line("rule link")
    nw.line("  command = mkdir -p \"$$(dirname \"$out\")\" && $ld $ldflags -T $linker -o $out $in")
    nw.line("  description = LD $out")
    nw.line()
    nw.line("rule uefi")
    nw.line("  command = mkdir -p \"$$(dirname \"$out\")\" && $uefi_cc $uefi_cflags $in -o $out $uefi_ldflags")
    nw.line("  description = UEFI $out")
    nw.line()
    nw.line("rule rust_staticlib")
    nw.line("  command = mkdir -p \"$$(dirname \"$out\")\" && $rustc $flags $in -o $out")
    nw.line("  description = RUST $out")
    nw.line()
    nw.line("rule run")
    nw.line("  command = $cmd")
    nw.line("  description = RUN $desc")
    nw.line()
    nw.line("rule console")
    nw.line("  command = $cmd")
    nw.line("  description = RUN $desc")
    nw.line("  pool = console")
    nw.line()


def add_setup(nw):
    assert_lines = [
        "#ifndef BDT_FREESTANDING_ASSERT_H",
        "#define BDT_FREESTANDING_ASSERT_H",
        "#ifdef NDEBUG",
        "#define assert(x) ((void)0)",
        "#else",
        "#define assert(x) ((void)((x) ? 0 : 0))",
        "#endif",
        "#endif",
    ]
    assert_printf = " ".join(q(line) for line in assert_lines)
    assert_cmd = "mkdir -p " + q(Path(FREESTANDING_ASSERT).parent) + " && printf '%s\\n' " + assert_printf + " > " + q(FREESTANDING_ASSERT)
    nw.build(FREESTANDING_ASSERT, "run", variables={"cmd": assert_cmd, "desc": "freestanding-headers"})
    defconfig_cmd = f"{TOOLS['PYTHON']} {q(ROOT / KCONFIG_SYNC)} defconfig --preset {q(MENUCONFIG_PRESET)}"
    if MENUCONFIG_ARGS.strip():
        defconfig_cmd += " " + MENUCONFIG_ARGS.strip()
    nw.build(KCONFIG_DOTCONFIG, "console", [KCONFIG_SYNC, KCONFIG_FILE], variables={"cmd": defconfig_cmd, "desc": "defconfig"})
    nw.build("defconfig", "phony", [KCONFIG_DOTCONFIG])
    nw.build("olddefconfig", "console", [KCONFIG_DOTCONFIG, KCONFIG_SYNC, KCONFIG_FILE], variables={"cmd": f"{TOOLS['PYTHON']} {q(ROOT / KCONFIG_SYNC)} olddefconfig", "desc": "olddefconfig"})
    nw.build("menuconfig", "console", [KCONFIG_DOTCONFIG, KCONFIG_SYNC, KCONFIG_FILE], variables={"cmd": f"{TOOLS['PYTHON']} {q(ROOT / KCONFIG_SYNC)} menuconfig", "desc": "menuconfig"})
    nw.build(
        [MENUCONFIG_HEADER, KCONFIG_BDT_CONFIG],
        "run",
        [KCONFIG_DOTCONFIG, KCONFIG_SYNC, KCONFIG_FILE],
        variables={"cmd": f"{TOOLS['PYTHON']} {q(ROOT / KCONFIG_SYNC)} export", "desc": "menuconfig-headers"},
    )


def kernel_sources(boot_source):
    sources = [r(boot_source)]
    sources.extend(r(path) for path in KERNEL_COMMON_C_SOURCES)
    sources.extend(r(path) for path in KERNEL_CXX_SOURCES)
    sources.extend(r(path) for path in KERNEL_ASM_SOURCES)
    return uniq(sources)


def add_kernel(nw, phony_name, boot_source, obj_root, output):
    objs = []
    implicit = [FREESTANDING_ASSERT, MENUCONFIG_HEADER]
    for src in kernel_sources(boot_source):
        source_rel = rel(src)
        obj = obj_for(obj_root, src)
        if is_cxx(src):
            objs.append(nw.compile_obj(src, obj, cxxflags=KERNEL_CXXFLAGS, cxx_tool=TOOLS["KERNEL_CXX"], implicit=implicit, label=source_rel))
        elif is_asm(src):
            objs.append(nw.compile_obj(src, obj, cflags=KERNEL_ASFLAGS, tool=TOOLS["CC"], implicit=implicit, label=source_rel))
        else:
            objs.append(nw.compile_obj(src, obj, cflags=KERNEL_CFLAGS, tool=TOOLS["CC"], implicit=implicit, label=source_rel))

    rust_out = r("build/x86_64/libclks_kernel_rust.a")
    nw.build(rust_out, "rust_staticlib", [r("clks/rust/src/lib.rs")], variables={"rustc": TOOLS["RUSTC"], "flags": "--crate-type staticlib -C panic=abort -O"})
    objs.append(rust_out)
    nw.build(output, "link", objs, implicit=[r("clks/arch/x86_64/linker.ld")], variables={"ld": TOOLS["LD"], "ldflags": " ".join(KERNEL_LDFLAGS), "linker": r("clks/arch/x86_64/linker.ld")})
    nw.build(phony_name, "phony", [output])
    return output


def app_group(app):
    rule = APP_RULES.get(app, {})
    explicit = rule.get("output_group")
    if explicit:
        return OUTPUT_GROUPS.get(explicit, OUTPUT_GROUPS["default"])
    for group in OUTPUT_GROUPS.values():
        if app in group.get("apps", []):
            return group
    return OUTPUT_GROUPS["default"]


def add_app_source(nw, source, app, obj_root, objects, extra_flags):
    source_rel = rel(source)
    obj = obj_for(obj_root, source, scope=app)
    flags = (USER_CXXFLAGS if is_cxx(source) else USER_CFLAGS) + extra_flags
    objects.append(nw.compile_obj(source, obj, cflags=flags, cxxflags=flags, label=source_rel))


def add_userapps(nw):
    main_dir = r("cleonos/c/apps")
    obj_root = r("build/x86_64/user/obj")
    top_sources = collect(main_dir, recursive=False, suffixes=(".c", ".cpp", ".cc", ".cxx"))
    dirs = child_dirs("cleonos/c/apps")
    outputs = []

    shared_objs = []
    for src in collect("cleonos/c/src", recursive=True, suffixes=(".c",)):
        shared_objs.append(nw.compile_obj(src, obj_for(obj_root, src), cflags=USER_CFLAGS, label=rel(src)))

    runtime_objs = []
    for src in RUNTIME_SOURCES:
        runtime_objs.append(nw.compile_obj(src, obj_for(obj_root, src), cflags=USER_CFLAGS, label=rel(src)))

    runtime_rels = {rel(src) for src in RUNTIME_SOURCES}
    main_sources = [src for src in top_sources if src.endswith("_main.c")]
    for main in sorted(main_sources, key=rel):
        app = Path(main).name[:-len("_main.c")]
        rule = APP_RULES.get(app, {})
        objects = list(shared_objs)
        extra_flags = list(rule.get("cflags", []))
        add_app_source(nw, main, app, obj_root, objects, extra_flags)

        if rule.get("include_runtime", True) and app not in RUNTIME_EXCLUDE_APPS:
            objects.extend(runtime_objs)

        for src in rule.get("sources", []):
            add_app_source(nw, src, app, obj_root, objects, extra_flags)

        excludes = set(rule.get("exclude_sources", []))
        for directory in rule.get("source_dirs", []):
            for src in collect(directory, recursive=True, suffixes=(".c", ".cpp", ".cc", ".cxx"), exclude_names=excludes):
                if rel(src) in excludes:
                    continue
                add_app_source(nw, src, app, obj_root, objects, extra_flags)

        prefix = app + "_"
        for src in top_sources:
            name = Path(src).name
            srel = rel(src)
            if not name.startswith(prefix) or name.endswith("_main.c") or name.endswith("_kmain.c") or srel in runtime_rels:
                continue
            add_app_source(nw, src, app, obj_root, objects, extra_flags)

        subdir = dirs.get(app)
        if subdir:
            for src in collect(subdir, recursive=False, suffixes=(".c", ".cpp", ".cc", ".cxx")):
                name = Path(src).name
                srel = rel(src)
                if name.endswith("_main.c") or name.endswith("_kmain.c") or srel in runtime_rels:
                    continue
                add_app_source(nw, src, app, obj_root, objects, extra_flags)

        group = app_group(app)
        out = norm(Path(group["output"]) / f"{app}.elf")
        outputs.append(out)
        nw.build(out, "link", uniq(objects), implicit=[group["linker"]], variables={"ld": TOOLS["LD"], "ldflags": " ".join(USER_LDFLAGS), "linker": group["linker"]})

    system_group = OUTPUT_GROUPS["system"]
    for main in sorted([src for src in top_sources if src.endswith("_kmain.c")], key=rel):
        app = Path(main).name[:-len("_kmain.c")]
        obj = nw.compile_obj(main, obj_for(obj_root, main), cflags=USER_CFLAGS, label=rel(main))
        out = norm(Path(system_group["output"]) / f"{app}.elf")
        outputs.append(out)
        nw.build(out, "link", [obj], implicit=[system_group["linker"]], variables={"ld": TOOLS["LD"], "ldflags": " ".join(USER_LDFLAGS), "linker": system_group["linker"]})

    nw.build("userapps", "phony", outputs)
    return outputs


def add_misc(nw, normal_kernel, clboot_kernel, user_outputs):
    clboot_efi = r("build/x86_64/clboot/BOOTX64.EFI")
    clboot_font = r("boot/clboot/fonts/clboot.psf")
    clboot_module_deps = [r("boot/clboot/uefi/clboot_uefi.h"), r("boot/clboot/include/clboot_protocol.h")]
    clboot_module_deps += [r(rel(path)) for path in sorted((ROOT / "boot/clboot/uefi/modules").glob("*.inc"))]
    nw.build(clboot_efi, "uefi", [r("boot/clboot/uefi/main.c")], implicit=clboot_module_deps, variables={
        "uefi_cc": TOOLS["UEFI_CC"],
        "uefi_cflags": " ".join(["-ffreestanding", "-fno-stack-protector", "-fno-builtin", "-fcf-protection=none", "-fshort-wchar", "-mno-red-zone", "-Wall", "-Wextra", f"-I{r('boot/clboot/include')}", f"-I{r('boot/clboot/uefi')}"]),
        "uefi_ldflags": " ".join(["-nostdlib", "-Wl,--subsystem,10", "-Wl,--entry,efi_main"]),
    })
    nw.build("clboot", "phony", [clboot_efi])

    sym = r("build/x86_64/kernel.sym")
    nw.build(sym, "run", [normal_kernel], variables={"cmd": f"mkdir -p {q(Path(sym).parent)} && {TOOLS['NM']} -n {q(normal_kernel)} > {q(sym)}", "desc": "kernel-symbols"})
    nw.build("kernel-symbols", "phony", [sym])

    tcc_stamp = r("build/x86_64/tccroot/.stamp")
    nw.build(tcc_stamp, "run", [r("scripts/build_tcc_runtime.sh")], variables={"cmd": f"sh {q(ROOT / 'scripts/build_tcc_runtime.sh')} {q(ROOT)} {q(TOOLS['CC'])} ar && touch {q(tcc_stamp)}", "desc": "tcc-runtime"})
    nw.build("tcc-runtime", "phony", [tcc_stamp])

    ramdisk_root = r("build/x86_64/ramdisk_root")
    ramdisk_stamp = ramdisk_root + "/.stamp"
    ramdisk = r("build/x86_64/cleonos_ramdisk.tar")
    shell_outputs = " ".join(q(p) for p in user_outputs)
    ramdisk_cmd = (
        f"rm -rf {q(ramdisk_root)} && mkdir -p {q(ramdisk_root)} {q(ramdisk_root + '/system')} {q(ramdisk_root + '/system/install')} "
        f"{q(ramdisk_root + '/system/tcc')} {q(ramdisk_root + '/shell')} {q(ramdisk_root + '/shell/uwm')} {q(ramdisk_root + '/shell/inputm')} {q(ramdisk_root + '/driver')}"
        f" && cp -R {q(str(ROOT) + '/ramdisk/.')} {q(ramdisk_root + '/')}"
        f" && {TOOLS['PYTHON']} {q(ROOT / 'scripts/gen_os_version.py')} {q(ROOT)} {q(ramdisk_root + '/etc')}"
        f" && cp -R {q(str(ROOT / 'build/x86_64/tccroot') + '/.')} {q(ramdisk_root + '/system/tcc/')}"
        f" && cp {q(sym)} {q(ramdisk_root + '/system/kernel.sym')}"
        f" && cp {q(clboot_kernel)} {q(ramdisk_root + '/system/install/clks_kernel.elf')}"
        f" && cp {q(clboot_efi)} {q(ramdisk_root + '/system/install/BOOTX64.EFI')}"
        f" && cp {q(clboot_font)} {q(ramdisk_root + '/system/install/clboot.psf')}"
        f" && cp {q(ROOT / 'configs/clboot-harddisk.conf')} {q(ramdisk_root + '/system/install/clboot-harddisk.conf')}"
        f" && for f in {shell_outputs}; do case \"$f\" in */uwm/*.elf) cp \"$f\" {q(ramdisk_root + '/shell/uwm/')} ;; */inputm/*.elf) cp \"$f\" {q(ramdisk_root + '/shell/inputm/')} ;; */driver/*.elf) cp \"$f\" {q(ramdisk_root + '/driver/')} ;; */system/*.elf) cp \"$f\" {q(ramdisk_root + '/system/')} ;; *.elf) cp \"$f\" {q(ramdisk_root + '/shell/')} ;; esac; done"
        f" && touch {q(ramdisk_stamp)}"
    )
    nw.build(ramdisk_stamp, "run", [clboot_kernel, clboot_efi, clboot_font, r("configs/clboot-harddisk.conf"), sym, tcc_stamp] + user_outputs, variables={"cmd": ramdisk_cmd, "desc": "ramdisk-root"})
    nw.build("ramdisk-root", "phony", [ramdisk_stamp])
    nw.build(ramdisk, "run", [ramdisk_stamp], variables={"cmd": f"mkdir -p {q(Path(ramdisk).parent)} && {TOOLS['TAR']} -cf {q(ramdisk)} -C {q(ramdisk_root)} .", "desc": "ramdisk"})
    nw.build("ramdisk", "phony", [ramdisk])

    iso = r("build/CLeonOS-CLBoot-x86_64.iso")
    iso_root = r("build/x86_64/clboot_iso_root")
    efi_img = r("build/x86_64/clboot_efi.img")
    iso_efi_img = r("build/x86_64/clboot_iso_efi.img")
    harddisk_img = r("build/x86_64/clboot_harddisk.img")
    startup_nsh = r("build/x86_64/startup.nsh")
    efi_offset = 1048576

    startup_cmd = f"mkdir -p {q(Path(startup_nsh).parent)} && printf 'FS0:\\\\EFI\\\\BOOT\\\\BOOTX64.EFI\\r\\n' > {q(startup_nsh)}"
    nw.build(startup_nsh, "run", variables={"cmd": startup_cmd, "desc": "clboot-startup"})

    esp_cmd = (
        f"rm -f {q(efi_img)}"
        f" && mkdir -p {q(Path(efi_img).parent)}"
        f" && truncate -s 134217728 {q(efi_img)}"
        f" && printf 'label: dos\\nunit: sectors\\n\\nstart=2048, size=260096, type=ef, bootable\\n' | sfdisk {q(efi_img)}"
        f" && mformat -i {q(efi_img + '@@' + str(efi_offset))} -F ::"
        f" && mmd -i {q(efi_img + '@@' + str(efi_offset))} ::/EFI ::/EFI/BOOT ::/EFI/CLEONOS ::/boot"
        f" && mcopy -i {q(efi_img + '@@' + str(efi_offset))} {q(clboot_efi)} ::/EFI/BOOT/BOOTX64.EFI"
        f" && mcopy -i {q(efi_img + '@@' + str(efi_offset))} {q(ROOT / 'configs/clboot.conf')} ::/EFI/CLEONOS/CLBOOT.CONF"
        f" && mcopy -i {q(efi_img + '@@' + str(efi_offset))} {q(clboot_font)} ::/EFI/CLEONOS/CLBOOT.PSF"
        f" && mcopy -i {q(efi_img + '@@' + str(efi_offset))} {q(startup_nsh)} ::/STARTUP.NSH"
        f" && mcopy -i {q(efi_img + '@@' + str(efi_offset))} {q(clboot_kernel)} ::/boot/clks_kernel.elf"
        f" && mcopy -i {q(efi_img + '@@' + str(efi_offset))} {q(ramdisk)} ::/boot/cleonos_ramdisk.tar"
    )
    nw.build(efi_img, "run", [clboot_efi, clboot_kernel, clboot_font, ramdisk, r("configs/clboot.conf"), startup_nsh],
             variables={"cmd": esp_cmd, "desc": "clboot-esp"})
    nw.build("clboot-esp", "phony", [efi_img])

    iso_efi_cmd = (
        f"rm -f {q(iso_efi_img)}"
        f" && mkdir -p {q(Path(iso_efi_img).parent)}"
        f" && truncate -s 67108864 {q(iso_efi_img)}"
        f" && mformat -i {q(iso_efi_img)} -F ::"
        f" && mmd -i {q(iso_efi_img)} ::/EFI ::/EFI/BOOT ::/EFI/CLEONOS ::/boot"
        f" && mcopy -i {q(iso_efi_img)} {q(clboot_efi)} ::/EFI/BOOT/BOOTX64.EFI"
        f" && mcopy -i {q(iso_efi_img)} {q(ROOT / 'configs/clboot.conf')} ::/EFI/CLEONOS/CLBOOT.CONF"
        f" && mcopy -i {q(iso_efi_img)} {q(clboot_font)} ::/EFI/CLEONOS/CLBOOT.PSF"
        f" && mcopy -i {q(iso_efi_img)} {q(startup_nsh)} ::/STARTUP.NSH"
        f" && mcopy -i {q(iso_efi_img)} {q(clboot_kernel)} ::/boot/clks_kernel.elf"
        f" && mcopy -i {q(iso_efi_img)} {q(ramdisk)} ::/boot/cleonos_ramdisk.tar"
    )
    nw.build(iso_efi_img, "run", [clboot_efi, clboot_kernel, clboot_font, ramdisk, r("configs/clboot.conf"), startup_nsh],
             variables={"cmd": iso_efi_cmd, "desc": "clboot-iso-efi"})
    nw.build("clboot-iso-efi", "phony", [iso_efi_img])

    iso_cmd = (
        f"rm -rf {q(iso_root)} && mkdir -p {q(iso_root + '/boot')} {q(iso_root + '/EFI/BOOT')} {q(iso_root + '/EFI/CLEONOS')}"
        f" && cp {q(clboot_kernel)} {q(iso_root + '/boot/clks_kernel.elf')}"
        f" && cp {q(ramdisk)} {q(iso_root + '/boot/cleonos_ramdisk.tar')}"
        f" && cp {q(clboot_efi)} {q(iso_root + '/EFI/BOOT/BOOTX64.EFI')}"
        f" && cp {q(ROOT / 'configs/clboot.conf')} {q(iso_root + '/EFI/CLEONOS/clboot.conf')}"
        f" && cp {q(clboot_font)} {q(iso_root + '/EFI/CLEONOS/clboot.psf')}"
        f" && cp {q(startup_nsh)} {q(iso_root + '/startup.nsh')}"
        f" && cp {q(iso_efi_img)} {q(iso_root + '/efi.img')}"
        f" && {TOOLS['XORRISO']} -as mkisofs -R -J -e efi.img -no-emul-boot {q(iso_root)} -o {q(iso)}"
    )
    nw.build(iso, "run", [clboot_efi, clboot_kernel, clboot_font, ramdisk, r("configs/clboot.conf"), startup_nsh, iso_efi_img],
             variables={"cmd": iso_cmd, "desc": "clboot-iso"})
    nw.build("clboot-iso", "phony", [iso])

    harddisk_cmd = (
        f"rm -f {q(harddisk_img)}"
        f" && mkdir -p {q(Path(harddisk_img).parent)}"
        f" && truncate -s 268435456 {q(harddisk_img)}"
        f" && printf 'label: dos\\nunit: sectors\\n\\nstart=2048, size=522240, type=ef, bootable\\n' | sfdisk {q(harddisk_img)}"
        f" && mformat -i {q(harddisk_img + '@@' + str(efi_offset))} -F ::"
        f" && mmd -i {q(harddisk_img + '@@' + str(efi_offset))} ::/EFI ::/EFI/BOOT ::/EFI/CLEONOS ::/boot ::/boot/kernels"
        f" && mcopy -i {q(harddisk_img + '@@' + str(efi_offset))} {q(clboot_efi)} ::/EFI/BOOT/BOOTX64.EFI"
        f" && mcopy -i {q(harddisk_img + '@@' + str(efi_offset))} {q(ROOT / 'configs/clboot-harddisk.conf')} ::/EFI/CLEONOS/CLBOOT.CONF"
        f" && mcopy -i {q(harddisk_img + '@@' + str(efi_offset))} {q(clboot_font)} ::/EFI/CLEONOS/CLBOOT.PSF"
        f" && mcopy -i {q(harddisk_img + '@@' + str(efi_offset))} {q(startup_nsh)} ::/STARTUP.NSH"
        f" && mcopy -i {q(harddisk_img + '@@' + str(efi_offset))} {q(clboot_kernel)} ::/boot/clks_kernel.elf"
    )
    nw.build(harddisk_img, "run", [clboot_efi, clboot_kernel, clboot_font, r("configs/clboot-harddisk.conf"), startup_nsh],
             variables={"cmd": harddisk_cmd, "desc": "clboot-harddisk"})
    nw.build("clboot-harddisk", "phony", [harddisk_img])

    disk = r("build/x86_64/cleonos_disk.img")
    disk_cmd = f"mkdir -p {q(Path(disk).parent)} && truncate -s 128M {q(disk)}"
    nw.build(disk, "run", variables={"cmd": disk_cmd, "desc": "disk-image"})
    nw.build("disk-image", "phony", [disk])

    run_clboot_cmd = (
        "if [ -f /usr/share/OVMF/OVMF_CODE.fd ]; then OVMF=/usr/share/OVMF/OVMF_CODE.fd; "
        "elif [ -f /usr/share/ovmf/OVMF.fd ]; then OVMF=/usr/share/ovmf/OVMF.fd; "
        "else echo \"run-clboot: OVMF firmware not found\"; exit 1; fi; "
        "export GSETTINGS_BACKEND=memory; "
        f"{TOOLS['QEMU_X86_64']} -M q35 -m 1024M -bios \"$OVMF\" -boot order=d "
        f"-drive {q('file=' + iso + ',format=raw,if=none,id=clksiso,media=cdrom,readonly=on')} "
        "-device ide-cd,drive=clksiso,bus=ide.0,bootindex=1 "
        "-netdev user,id=clksnet0 -device e1000,netdev=clksnet0 -serial stdio"
    )
    nw.build("run-clboot", "console", [iso], variables={"cmd": run_clboot_cmd, "desc": "run-clboot"})
    nw.build("run", "phony", ["run-clboot"])

    run_clboot_harddisk_cmd = (
        "if [ -f /usr/share/OVMF/OVMF_CODE.fd ]; then OVMF=/usr/share/OVMF/OVMF_CODE.fd; "
        "elif [ -f /usr/share/ovmf/OVMF.fd ]; then OVMF=/usr/share/ovmf/OVMF.fd; "
        "else echo \"run-clboot-harddisk: OVMF firmware not found\"; exit 1; fi; "
        "export GSETTINGS_BACKEND=memory; "
        f"{TOOLS['QEMU_X86_64']} -M q35 -m 1024M -bios \"$OVMF\" -boot order=c "
        f"-drive {q('file=' + harddisk_img + ',format=raw,if=none,id=clbootdisk,media=disk')} "
        "-device ide-hd,drive=clbootdisk,bus=ide.0,bootindex=1 "
        "-netdev user,id=clksnet0 -device e1000,netdev=clksnet0 -serial stdio"
    )
    nw.build("run-clboot-harddisk", "console", [harddisk_img],
             variables={"cmd": run_clboot_harddisk_cmd, "desc": "run-clboot-harddisk"})

    nw.build("kernel", "phony", [normal_kernel])
    nw.build("clboot-kernel", "phony", [clboot_kernel])
    nw.build("all", "phony", ["clboot-iso"])
    nw.line("default all")


def main():
    nw = NinjaWriter()
    add_rules(nw)
    add_setup(nw)
    normal_kernel = add_kernel(nw, "kernel-limine", "clks/kernel/boot/limine/limine_requests.c", r("build/x86_64/ninja_obj"), r("build/x86_64/clks_kernel.elf"))
    clboot_kernel = add_kernel(nw, "clboot-kernel", "clks/kernel/boot/clboot/clboot.c", r("build/x86_64/ninja_clboot_obj"), r("build/x86_64/clks_kernel_clboot.elf"))
    user_outputs = add_userapps(nw)
    add_misc(nw, normal_kernel, clboot_kernel, user_outputs)
    NINJA.write_text("\n".join(nw.lines) + "\n", encoding="utf-8")
    print(f"generated {NINJA}")


if __name__ == "__main__":
    main()
