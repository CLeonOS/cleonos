#ifndef CLEONOS_TCC_CONFIG_H
#define CLEONOS_TCC_CONFIG_H

#define TCC_VERSION "0.9.28-cleonos"
#define TCC_GITHASH "cleonos-port"
#define CONFIG_TCC_STATIC 1
#define CONFIG_TCC_SEMLOCK 0
#define CONFIG_TCC_BACKTRACE 0
#define CONFIG_TCC_BCHECK 0
#define CONFIG_TCCDIR "/system/tcc"
#define CONFIG_TCC_SYSINCLUDEPATHS "{B}/include:/include"
#define CONFIG_TCC_LIBPATHS "{B}/lib:{B}:/system/lib:/lib"
#define CONFIG_TCC_CRTPREFIX "/system/tcc/lib"
#define CONFIG_TCC_ELFINTERP "-"
#define CONFIG_TCC_CROSSPREFIX ""
#define CONFIG_LDDIR "lib"
#define CONFIG_TCC_MUSL 1
#define CONFIG_USE_LIBGCC 0
#define TCC_TARGET_X86_64 1

#endif
