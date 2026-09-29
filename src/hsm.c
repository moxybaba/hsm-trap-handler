#include "hsm.h"

#include <stddef.h>

static hsm_state_t hart_state[HSM_MAX_HARTS];

static const char *const state_names[] = {
    [HSM_STARTED] = "STARTED",
    [HSM_STOPPED] = "STOPPED",
    [HSM_START_PENDING] = "START_PENDING",
    [HSM_STOP_PENDING] = "STOP_PENDING",
    [HSM_SUSPENDED] = "SUSPENDED",
    [HSM_SUSPEND_PENDING] = "SUSPEND_PENDING",
    [HSM_RESUME_PENDING] = "RESUME_PENDING",
    [HSM_STATE_INVALID] = "INVALID"
};

static const char *const event_names[] = {
    [HSM_EVENT_HART_START] = "hart_start",
    [HSM_EVENT_BOOT_COMPLETE] = "boot_complete",
    [HSM_EVENT_HART_STOP_SELF] = "hart_stop_self",
    [HSM_EVENT_STOP_COMPLETE] = "stop_complete",
    [HSM_EVENT_HART_SUSPEND_SELF] = "hart_suspend_self",
    [HSM_EVENT_SUSPEND_COMPLETE] = "suspend_complete",
    [HSM_EVENT_WAKE] = "wake",
    [HSM_EVENT_RESUME_COMPLETE] = "resume_complete"
};

static bool transition_for(hsm_state_t current, hsm_event_t event,
                           hsm_state_t *next)
{
    if (!next)
        return false;

    switch (current) {
    case HSM_STOPPED:
        if (event == HSM_EVENT_HART_START) {
            *next = HSM_START_PENDING;
            return true;
        }
        break;

    case HSM_START_PENDING:
        if (event == HSM_EVENT_BOOT_COMPLETE) {
            *next = HSM_STARTED;
            return true;
        }
        break;

    case HSM_STARTED:
        if (event == HSM_EVENT_HART_STOP_SELF) {
            *next = HSM_STOP_PENDING;
            return true;
        }
        if (event == HSM_EVENT_HART_SUSPEND_SELF) {
            *next = HSM_SUSPEND_PENDING;
            return true;
        }
        break;

    case HSM_STOP_PENDING:
        if (event == HSM_EVENT_STOP_COMPLETE) {
            *next = HSM_STOPPED;
            return true;
        }
        break;

    case HSM_SUSPEND_PENDING:
        if (event == HSM_EVENT_SUSPEND_COMPLETE) {
            *next = HSM_SUSPENDED;
            return true;
        }
        break;

    case HSM_SUSPENDED:
        if (event == HSM_EVENT_WAKE) {
            *next = HSM_RESUME_PENDING;
            return true;
        }
        break;

    case HSM_RESUME_PENDING:
        if (event == HSM_EVENT_RESUME_COMPLETE) {
            *next = HSM_STARTED;
            return true;
        }
        break;

    case HSM_STATE_INVALID:
    default:
        break;
    }

    return false;
}

void hsm_init_all_stopped(void)
{
    uint64_t hartid;

    for (hartid = 0; hartid < HSM_MAX_HARTS; ++hartid)
        hart_state[hartid] = HSM_STOPPED;
}

hsm_state_t hsm_get_state(uint64_t hartid)
{
    if (hartid >= HSM_MAX_HARTS)
        return HSM_STATE_INVALID;

    return hart_state[hartid];
}

const char *hsm_state_name(hsm_state_t state)
{
    if (state > HSM_STATE_INVALID)
        return "INVALID";

    return state_names[state];
}

const char *hsm_event_name(hsm_event_t event)
{
    if (event >= HSM_EVENT_COUNT)
        return "invalid_event";

    return event_names[event];
}

hsm_result_t hsm_apply_event(uint64_t hartid, hsm_event_t event,
                             hsm_state_t *before,
                             hsm_state_t *after)
{
    hsm_state_t current;
    hsm_state_t next;

    if (hartid >= HSM_MAX_HARTS)
        return HSM_ERR_HART_RANGE;

    current = hart_state[hartid];

    if (before)
        *before = current;

    if (!transition_for(current, event, &next)) {
        if (after)
            *after = current;
        return HSM_ERR_ILLEGAL_TRANSITION;
    }

    hart_state[hartid] = next;

    if (after)
        *after = next;

    return HSM_OK;
}

static bool apply_ok(uint64_t hartid, hsm_event_t event)
{
    return hsm_apply_event(hartid, event, NULL, NULL) == HSM_OK;
}

static bool apply_rejected(uint64_t hartid, hsm_event_t event)
{
    return hsm_apply_event(hartid, event, NULL, NULL) ==
           HSM_ERR_ILLEGAL_TRANSITION;
}

static bool bring_to_started(uint64_t hartid)
{
    return apply_ok(hartid, HSM_EVENT_HART_START) &&
           apply_ok(hartid, HSM_EVENT_BOOT_COMPLETE);
}

static bool bring_to_suspended(uint64_t hartid)
{
    return bring_to_started(hartid) &&
           apply_ok(hartid, HSM_EVENT_HART_SUSPEND_SELF) &&
           apply_ok(hartid, HSM_EVENT_SUSPEND_COMPLETE);
}

bool hsm_test_dual_start_race(uint64_t target_hart)
{
    hsm_state_t before;
    hsm_state_t after;
    hsm_result_t first;
    hsm_result_t second;

    hsm_init_all_stopped();

    /*
     * In firmware this must use atomic compare-and-exchange or a
     * per-hart lock. This deterministic model represents two caller
     * harts reaching the same claim point: exactly one can change the
     * target from STOPPED to START_PENDING; the other sees the target
     * no longer STOPPED and is rejected.
     */
    first = hsm_apply_event(target_hart, HSM_EVENT_HART_START,
                            &before, &after);
    second = hsm_apply_event(target_hart, HSM_EVENT_HART_START,
                             &before, &after);

    return first == HSM_OK &&
           second == HSM_ERR_ILLEGAL_TRANSITION &&
           hsm_get_state(target_hart) == HSM_START_PENDING;
}

bool hsm_test_start_from_started_rejected(uint64_t hartid)
{
    hsm_init_all_stopped();

    return bring_to_started(hartid) &&
           apply_rejected(hartid, HSM_EVENT_HART_START) &&
           hsm_get_state(hartid) == HSM_STARTED;
}

bool hsm_test_start_from_start_pending_rejected(uint64_t hartid)
{
    hsm_init_all_stopped();

    return apply_ok(hartid, HSM_EVENT_HART_START) &&
           apply_rejected(hartid, HSM_EVENT_HART_START) &&
           hsm_get_state(hartid) == HSM_START_PENDING;
}

bool hsm_test_stop_from_suspended_rejected(uint64_t hartid)
{
    hsm_init_all_stopped();

    return bring_to_suspended(hartid) &&
           apply_rejected(hartid, HSM_EVENT_HART_STOP_SELF) &&
           hsm_get_state(hartid) == HSM_SUSPENDED;
}

bool hsm_test_suspend_from_stopped_rejected(uint64_t hartid)
{
    hsm_init_all_stopped();

    return apply_rejected(hartid, HSM_EVENT_HART_SUSPEND_SELF) &&
           hsm_get_state(hartid) == HSM_STOPPED;
}

bool hsm_test_wake_from_started_rejected(uint64_t hartid)
{
    hsm_init_all_stopped();

    return bring_to_started(hartid) &&
           apply_rejected(hartid, HSM_EVENT_WAKE) &&
           hsm_get_state(hartid) == HSM_STARTED;
}

bool hsm_test_completion_before_request_rejected(uint64_t hartid)
{
    hsm_init_all_stopped();

    return apply_rejected(hartid, HSM_EVENT_BOOT_COMPLETE) &&
           apply_rejected(hartid, HSM_EVENT_STOP_COMPLETE) &&
           apply_rejected(hartid, HSM_EVENT_SUSPEND_COMPLETE) &&
           apply_rejected(hartid, HSM_EVENT_RESUME_COMPLETE) &&
           hsm_get_state(hartid) == HSM_STOPPED;
}

bool hsm_test_duplicate_completion_rejected(uint64_t hartid)
{
    hsm_init_all_stopped();

    if (!apply_ok(hartid, HSM_EVENT_HART_START) ||
        !apply_ok(hartid, HSM_EVENT_BOOT_COMPLETE))
        return false;

    return apply_rejected(hartid, HSM_EVENT_BOOT_COMPLETE) &&
           hsm_get_state(hartid) == HSM_STARTED;
}
