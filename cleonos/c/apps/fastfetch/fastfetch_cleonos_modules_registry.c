#include "fastfetch.h"

#include "modules/colors/colors.h"
#include "modules/disk/disk.h"
#include "modules/host/host.h"
#include "modules/kernel/kernel.h"
#include "modules/locale/locale.h"
#include "modules/memory/memory.h"
#include "modules/os/os.h"
#include "modules/separator/separator.h"
#include "modules/shell/shell.h"
#include "modules/terminal/terminal.h"
#include "modules/terminalsize/terminalsize.h"
#include "modules/title/title.h"
#include "modules/uptime/uptime.h"
#include "modules/version/version.h"

static FFModuleBaseInfo *A[] = {NULL};
static FFModuleBaseInfo *B[] = {NULL};
static FFModuleBaseInfo *C[] = {&ffColorsModuleInfo, NULL};
static FFModuleBaseInfo *D[] = {&ffDiskModuleInfo, NULL};
static FFModuleBaseInfo *E[] = {NULL};
static FFModuleBaseInfo *F[] = {NULL};
static FFModuleBaseInfo *G[] = {NULL};
static FFModuleBaseInfo *H[] = {&ffHostModuleInfo, NULL};
static FFModuleBaseInfo *I[] = {NULL};
static FFModuleBaseInfo *J[] = {NULL};
static FFModuleBaseInfo *K[] = {&ffKernelModuleInfo, NULL};
static FFModuleBaseInfo *L[] = {&ffLocaleModuleInfo, NULL};
static FFModuleBaseInfo *M[] = {&ffMemoryModuleInfo, NULL};
static FFModuleBaseInfo *N[] = {NULL};
static FFModuleBaseInfo *O[] = {&ffOSModuleInfo, NULL};
static FFModuleBaseInfo *P[] = {NULL};
static FFModuleBaseInfo *Q[] = {NULL};
static FFModuleBaseInfo *R[] = {NULL};
static FFModuleBaseInfo *S[] = {&ffSeparatorModuleInfo, &ffShellModuleInfo, NULL};
static FFModuleBaseInfo *T[] = {&ffTerminalModuleInfo, &ffTerminalSizeModuleInfo, &ffTitleModuleInfo, NULL};
static FFModuleBaseInfo *U[] = {&ffUptimeModuleInfo, NULL};
static FFModuleBaseInfo *V[] = {&ffVersionModuleInfo, NULL};
static FFModuleBaseInfo *W[] = {NULL};
static FFModuleBaseInfo *X[] = {NULL};
static FFModuleBaseInfo *Y[] = {NULL};
static FFModuleBaseInfo *Z[] = {NULL};

FFModuleBaseInfo **ffModuleInfos[] = {
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
};
