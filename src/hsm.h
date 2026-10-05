#ifndef HSM_H
#define HSM_H

#include <stdbool.h>
#include <stdint.h>

#define HSM_MAX_HARTS 8U

typedef enum {
    HSM_STARTED = 0,
    HSM_STOPPED = 1,
    HSM_START_PENDING = 2,
    HSM_STOP_PENDING = 3,
    HSM_SUSPENDED = 4,
    HSM_SUSPEND_PENDING = 5,
    HSM_RESUME_PENDING = 6,
    HSM_STATE_INVALID = 7
} hsm_state_t;

typedef enum {
    HSM_EVENT_HART_START = 0,
    HSM_EVENT_BOOT_COMPLETE,
    HSM_EVENT_HART_STOP_SELF,
    HSM_EVENT_STOP_COMPLETE,
    HSM_EVENT_HART_SUSPEND_SELF,
    HSM_EVENT_SUSPEND_COMPLETE,
    HSM_EVENT_WAKE,
    HSM_EVENT_RESUME_COMPLETE,
    HSM_EVENT_COUNT
} hsm_event_t;

typedef enum {
    HSM_OK = 0,
    HSM_ERR_HART_RANGE = -1,
    HSM_ERR_ILLEGAL_TRANSITION = -2,
    HSM_ERR_STATE_RACE = -3
} hsm_result_t;

void hsm_init_all_stopped(void);

hsm_state_t hsm_get_state(uint64_t hartid);
const char *hsm_state_name(hsm_state_t state);
const char *hsm_event_name(hsm_event_t event);

/*
 * Model one state transition.
 *
 * This function is intentionally non-blocking. It returns HSM_OK only
 * when the requested event is legal for the current state. In a real
 * OpenSBI implementation the equivalent operation must be protected by
 * atomics or a per-hart lock.
 */
hsm_result_t hsm_apply_event(uint64_t hartid, hsm_event_t event,
                             hsm_state_t *before,
                             hsm_state_t *after);

/*
 * Model two different caller harts racing to start the same target.
 * Exactly one should claim STOPPED -> START_PENDING.
 */
bool hsm_test_dual_start_race(uint64_t target_hart);

/* Illegal / repeated / out-of-order test family. */
bool hsm_test_start_from_started_rejected(uint64_t hartid);
bool hsm_test_start_from_start_pending_rejected(uint64_t hartid);
bool hsm_test_stop_from_suspended_rejected(uint64_t hartid);
bool hsm_test_suspend_from_stopped_rejected(uint64_t hartid);
bool hsm_test_wake_from_started_rejected(uint64_t hartid);
bool hsm_test_completion_before_request_rejected(uint64_t hartid);
bool hsm_test_duplicate_completion_rejected(uint64_t hartid);

#endif
