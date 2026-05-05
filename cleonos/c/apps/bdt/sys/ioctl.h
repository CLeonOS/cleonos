#ifndef CLEONOS_BDT_SYS_IOCTL_H
#define CLEONOS_BDT_SYS_IOCTL_H

#define TIOCGWINSZ 0

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

int ioctl(int fd, unsigned long request, ...);

#endif
