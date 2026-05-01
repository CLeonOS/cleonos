CLeonOS ramdisk root layout

/system  : kernel-mode ELF apps and core system components
/shell   : user shell and command ELF apps
/temp    : runtime temp/cache files
/driver  : standard and third-party ELF driver processes
/dev     : device interface nodes (/dev/tty, /dev/tty0, /dev/fb0, /dev/input/*, /dev/net0, /dev/disk0)

Root ELF demos:
/hello.elf     : Hello world user ELF
