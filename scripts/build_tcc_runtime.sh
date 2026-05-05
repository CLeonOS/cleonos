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

"$ar_tool" rcs "$lib/libc.a" \
    "$obj/posix.o" \
    "$obj/syscall.o" \
    "$obj/stdio.o" \
    "$obj/libc_ctype.o" \
    "$obj/libc_string.o" \
    "$obj/libc_strings.o" \
    "$obj/libc_stdlib.o"

"$ar_tool" rcs "$lib/libtcc1.a" "$lib/crti.o"
