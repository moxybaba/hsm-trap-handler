#include <stdio.h>

#include "hsm.h"

static int failures;

#define CHECK(name, condition)                                      \
    do {                                                            \
        if (condition)                                              \
            printf("[PASS] %s\n", name);                           \
        else {                                                      \
            printf("[FAIL] %s\n", name);                           \
            ++failures;                                             \
        }                                                           \
    } while (0)

static int test_base_lifecycle(void)
{
    hsm_state_t before;
    hsm_state_t after;

    hsm_init_all_stopped();

    CHECK("base: STOPPED -> START_PENDING",
          hsm_apply_event(0, HSM_EVENT_HART_START, &before, &after)
              == HSM_OK &&
          before == HSM_STOPPED &&
          after == HSM_START_PENDING);

    CHECK("base: START_PENDING -> STARTED",
          hsm_apply_event(0, HSM_EVENT_BOOT_COMPLETE, &before, &after)
              == HSM_OK &&
          before == HSM_START_PENDING &&
          after == HSM_STARTED);

    CHECK("base: STARTED -> STOP_PENDING",
          hsm_apply_event(0, HSM_EVENT_HART_STOP_SELF, &before, &after)
              == HSM_OK &&
          before == HSM_STARTED &&
          after == HSM_STOP_PENDING);

    CHECK("base: STOP_PENDING -> STOPPED",
          hsm_apply_event(0, HSM_EVENT_STOP_COMPLETE, &before, &after)
              == HSM_OK &&
          before == HSM_STOP_PENDING &&
          after == HSM_STOPPED);

    hsm_init_all_stopped();

    CHECK("base: STARTED -> SUSPEND_PENDING",
          hsm_apply_event(1, HSM_EVENT_HART_START, NULL, NULL)
              == HSM_OK &&
          hsm_apply_event(1, HSM_EVENT_BOOT_COMPLETE, NULL, NULL)
              == HSM_OK &&
          hsm_apply_event(1, HSM_EVENT_HART_SUSPEND_SELF, &before, &after)
              == HSM_OK &&
          before == HSM_STARTED &&
          after == HSM_SUSPEND_PENDING);

    CHECK("base: SUSPEND_PENDING -> SUSPENDED",
          hsm_apply_event(1, HSM_EVENT_SUSPEND_COMPLETE, &before, &after)
              == HSM_OK &&
          before == HSM_SUSPEND_PENDING &&
          after == HSM_SUSPENDED);

    CHECK("base: SUSPENDED -> RESUME_PENDING",
          hsm_apply_event(1, HSM_EVENT_WAKE, &before, &after)
              == HSM_OK &&
          before == HSM_SUSPENDED &&
          after == HSM_RESUME_PENDING);

    CHECK("base: RESUME_PENDING -> STARTED",
          hsm_apply_event(1, HSM_EVENT_RESUME_COMPLETE, &before, &after)
              == HSM_OK &&
          before == HSM_RESUME_PENDING &&
          after == HSM_STARTED);

    return failures;
}

int main(void)
{
    test_base_lifecycle();

    CHECK("family A: hart_start from STARTED is rejected",
          hsm_test_start_from_started_rejected(0));

    CHECK("family A: repeated hart_start from START_PENDING is rejected",
          hsm_test_start_from_start_pending_rejected(1));

    CHECK("family A: hart_stop from SUSPENDED is rejected",
          hsm_test_stop_from_suspended_rejected(2));

    CHECK("family A: hart_suspend from STOPPED is rejected",
          hsm_test_suspend_from_stopped_rejected(3));

    CHECK("family A: wake from STARTED is rejected",
          hsm_test_wake_from_started_rejected(4));

    CHECK("family C: completion before initiating request is rejected",
          hsm_test_completion_before_request_rejected(5));

    CHECK("family C: duplicate boot completion is rejected",
          hsm_test_duplicate_completion_rejected(6));

    CHECK("family B: competing cross-hart starts have one winner",
          hsm_test_dual_start_race(7));

    printf("\n%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}

