#ifndef HSM_H
#define HSM_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    OP_HART_START = 0,
    OP_BOOT_COMPLETE,
    OP_HART_STOP,
    OP_SHUTDOWN_COMPLETE,
    OP_HART_SUSPEND,
    OP_SUSPEND_COMPLETE,
    OP_WAKE_EVENT,
    OP_RESUME_COMPLETE,
    OP_COUNT
} hsm_op_t;

bool hsm_apply(uint64_t hartid, hsm_op_t op);
const char *hsm_state_name(uint64_t hartid);
void hsm_init_all_stopped(void);

bool test_illegal_hart_stop_while_suspended(uint64_t hartid);
bool test_cross_hart_stop_vs_self_suspend(uint64_t target_hart);
bool test_repeated_hart_start(uint64_t hartid);

#endif /* HSM_H */
