#ifndef CLKS_PANIC_QR_H
#define CLKS_PANIC_QR_H

#include <clks/types.h>

#ifdef __cplusplus
extern "C" {
#endif

clks_bool clks_panic_qr_prepare(void);
clks_bool clks_panic_qr_show(void);
u64 clks_panic_qr_total_lines(void);
u64 clks_panic_qr_dropped_lines(void);

#ifdef __cplusplus
}
#endif

#endif
