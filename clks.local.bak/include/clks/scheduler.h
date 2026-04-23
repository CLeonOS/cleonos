#ifndef CLKS_SCHEDULER_H
#define CLKS_SCHEDULER_H

#include <clks/task.h>
#include <clks/types.h>

struct clks_scheduler_stats {
    u32 task_count;
    u32 current_task_id;
    u64 total_timer_ticks;
    u64 context_switch_count;
};

void clks_scheduler_init(void);
clks_bool clks_scheduler_add_kernel_task(const char *name, u32 time_slice_ticks);
clks_bool clks_scheduler_add_kernel_task_ex(const char *name, u32 time_slice_ticks, clks_task_entry_fn entry);
void clks_scheduler_on_timer_tick(u64 tick);
void clks_scheduler_dispatch_current(u64 tick);
struct clks_scheduler_stats clks_scheduler_get_stats(void);
const struct clks_task_descriptor *clks_scheduler_get_task(u32 task_id);

#endif
