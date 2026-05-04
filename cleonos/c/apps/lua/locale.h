#ifndef CLEONOS_LUA_LOCALE_H
#define CLEONOS_LUA_LOCALE_H

#define LC_ALL 0
#define LC_COLLATE 1
#define LC_CTYPE 2
#define LC_MONETARY 3
#define LC_NUMERIC 4
#define LC_TIME 5

struct lconv {
    char *decimal_point;
};

struct lconv *localeconv(void);
char *setlocale(int category, const char *locale);
int strcoll(const char *left, const char *right);

#endif
