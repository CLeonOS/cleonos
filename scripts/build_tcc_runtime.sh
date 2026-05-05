#!/usr/bin/env sh
set -eu

root="$1"
cc="${2:-cc}"
ar_tool="${3:-ar}"

out="$root/build/x86_64/tccroot"
obj="$out/obj"
lib="$out/lib"
inc="$out/include"

rm -rf "$out"
mkdir -p "$obj" "$lib" "$inc"

cp "$root/cleonos/c/user.ld" "$lib/user.ld"
cp "$root/cleonos/third-party/tinycc/include/"*.h "$inc/"
cp "$root/cleonos/c/include/"*.h "$inc/"
cp -R "$root/cleonos/c/include/sys" "$inc/"
cp "$root/cleonos/c/tccroot/include/"*.h "$inc/"

cflags="-std=c11 -ffreestanding -fno-stack-protector -fno-builtin -Wall -Wextra -I$root/cleonos/c/tccroot/include -I$root/cleonos/c/include"
tccflags="-std=c11 -ffreestanding -fno-stack-protector -fno-builtin -Wall -Wextra -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter -Wno-builtin-declaration-mismatch -I$root/cleonos/third-party/tinycc -I$root/cleonos/third-party/tinycc/include -I$root/cleonos/c/tccroot/include -I$root/cleonos/c/include"

"$cc" $cflags -c "$root/cleonos/c/tccroot/crt1.c" -o "$lib/crt1.o"
"$cc" -x c -c -o "$lib/crti.o" /dev/null
"$cc" -x c -c -o "$lib/crtn.o" /dev/null

"$cc" $cflags -c "$root/cleonos/c/tccroot/posix.c" -o "$obj/posix.o"
"$cc" $cflags -c "$root/cleonos/c/src/syscall.c" -o "$obj/syscall.o"
"$cc" $cflags -c "$root/cleonos/c/src/stdio.c" -o "$obj/stdio.o"
"$cc" $cflags -c "$root/cleonos/c/src/libc_ctype.c" -o "$obj/libc_ctype.o"
"$cc" $cflags -c "$root/cleonos/c/src/libc_string.c" -o "$obj/libc_string.o"
"$cc" $cflags -c "$root/cleonos/c/src/libc_strings.c" -o "$obj/libc_strings.o"
"$cc" $cflags -c "$root/cleonos/c/src/libc_stdlib.c" -o "$obj/libc_stdlib.o"
"$cc" $tccflags -c "$root/cleonos/third-party/tinycc/lib/libtcc1.c" -o "$obj/libtcc1.o"
"$cc" $tccflags -c "$root/cleonos/third-party/tinycc/lib/stdatomic.c" -o "$obj/stdatomic.o"
"$cc" $tccflags -c "$root/cleonos/third-party/tinycc/lib/builtin.c" -o "$obj/builtin.o"
"$cc" $tccflags -c "$root/cleonos/third-party/tinycc/lib/va_list.c" -o "$obj/va_list.o"
"$cc" $tccflags -c "$root/cleonos/third-party/tinycc/lib/dsohandle.c" -o "$obj/dsohandle.o"
"$cc" $tccflags -c "$root/cleonos/third-party/tinycc/lib/atomic.S" -o "$obj/atomic.o"
"$cc" $tccflags -c "$root/cleonos/third-party/tinycc/lib/alloca.S" -o "$obj/alloca.o"
"$cc" $tccflags -c "$root/cleonos/third-party/tinycc/lib/alloca-bt.S" -o "$obj/alloca-bt.o"

"$ar_tool" rcs "$lib/libc.a" \
    "$obj/posix.o" \
    "$obj/syscall.o" \
    "$obj/stdio.o" \
    "$obj/libc_ctype.o" \
    "$obj/libc_string.o" \
    "$obj/libc_strings.o" \
    "$obj/libc_stdlib.o"

"$ar_tool" rcs "$lib/libtcc1.a" \
    "$obj/libtcc1.o" \
    "$obj/stdatomic.o" \
    "$obj/atomic.o" \
    "$obj/builtin.o" \
    "$obj/alloca.o" \
    "$obj/alloca-bt.o" \
    "$obj/va_list.o" \
    "$obj/dsohandle.o"
