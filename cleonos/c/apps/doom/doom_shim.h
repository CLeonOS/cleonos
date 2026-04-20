#ifndef CLEONOS_DOOM_SHIM_H
#define CLEONOS_DOOM_SHIM_H

/* Redirect stdio-family APIs that clash with cleonos/c/src/stdio.c signatures. */
#define fopen dg_fopen
#define fclose dg_fclose
#define fread dg_fread
#define fwrite dg_fwrite
#define fseek dg_fseek
#define ftell dg_ftell
#define fflush dg_fflush
#define fgets dg_fgets
#define fprintf dg_fprintf
#define vfprintf dg_vfprintf
#define feof dg_feof
#define fileno dg_fileno
#define perror dg_perror
#define sscanf dg_sscanf

#endif

