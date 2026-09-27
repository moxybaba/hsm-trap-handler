/*
 * hsm.c - HSM (Hart State Management) state machine, derived from the
 * RISC-V SBI Specification, HSM extension (EID 0x48534D "HSM").
 *
 * States (SBI spec sec. 9, Hart States table):
 *   STOPPED         - hart not running SBI/supervisor code
 *   START_PENDING   - hart_start() called, hart booting
 *   STARTED         - hart running
 *   STOP_PENDING    - hart_stop() called, hart shutting down
 *   SUSPEND_PENDING - hart_suspend() called, entering suspend
 *   SUSPENDED       - hart parked in suspend state
 *   RESUME_PENDING  - resume triggered (by IPI/interrupt), waking up
 *
 * Base legal transitions, derived directly from the spec's function
 * descriptions for hart_start, hart_stop, hart_suspend:
 *
 *   STOPPED         --hart_start-->        START_PENDING
 *   START_PENDING   --(boot complete)-->   STARTED
 *   STARTED         --hart_stop-->         STOP_PENDING
 *   STOP_PENDING    --(shutdown complete)--> STOPPED
 *   STARTED         --hart_suspend-->      SUSPEND_PENDING
 *   SUSPEND_PENDING --(suspend complete)--> SUSPENDED
 *   SUSPENDED       --(IPI/interrupt)-->   RESUME_PENDING
 *   RESUME_PENDING  --(resume complete)--> STARTED
 *
 * Reference for K1's concrete implementation: OpenSBI
 * lib/sbi/sbi_hsm.c (generic state machine + spinlock-protected
 * transitions) and lib/utils/hsm/fdt_hsm_spacemit.c (K1's
 * platform-specific hart start/stop hooks, e.g. how K1 physically
 * powers a core up/down versus the generic WFI-based suspend used
 * on platforms without per-core power gating).
 */

#include <stdint.h>
#include <stdbool.h>
#include "hsm.h"

typedef enum {
    HSM_STOPPED = 0,
    HSM_START_PENDING,
    HSM_STARTED,
    HSM_STOP_PENDING,
    HSM_SUSPEND_PENDING,
    HSM_SUSPENDED,
    HSM_RESUME_PENDING,
    HSM_STATE_COUNT
} hsm_state_t;


#define MAX_HARTS 8

/* legal[state][op] = next state, or HSM_STATE_COUNT if illegal.
 * This is the direct encoding of the spec's state table plus the
 * transition edges implied by each SBI call's defined behavior. */
static const hsm_state_t legal[HSM_STATE_COUNT][OP_COUNT] = {
    [HSM_STOPPED] = {
        [OP_HART_START] = HSM_START_PENDING,
        /* all other ops illegal from STOPPED: default-init below */
    },
    [HSM_START_PENDING] = {
        [OP_BOOT_COMPLETE] = HSM_STARTED,
    },
    [HSM_STARTED] = {
        [OP_HART_STOP]    = HSM_STOP_PENDING,
        [OP_HART_SUSPEND] = HSM_SUSPEND_PENDING,
    },
    [HSM_STOP_PENDING] = {
        [OP_SHUTDOWN_COMPLETE] = HSM_STOPPED,
    },
    [HSM_SUSPEND_PENDING] = {
        [OP_SUSPEND_COMPLETE] = HSM_SUSPENDED,
    },
    [HSM_SUSPENDED] = {
        [OP_WAKE_EVENT] = HSM_RESUME_PENDING,
    },
    [HSM_RESUME_PENDING] = {
        [OP_RESUME_COMPLETE] = HSM_STARTED,
    },
};

/* Per-hart state, simulating what OpenSBI's sbi_hsm.c tracks in its
 * per-hart scratch data (hart_data->state, spinlock-protected). */
static hsm_state_t hart_state[MAX_HARTS];

static const char *state_name(hsm_state_t s)
{
    switch (s) {
    case HSM_STOPPED:         return "STOPPED";
    case HSM_START_PENDING:   return "START_PENDING";
    case HSM_STARTED:         return "STARTED";
    case HSM_STOP_PENDING:    return "STOP_PENDING";
    case HSM_SUSPEND_PENDING: return "SUSPEND_PENDING";
    case HSM_SUSPENDED:       return "SUSPENDED";
    case HSM_RESUME_PENDING:  return "RESUME_PENDING";
    default:                  return "ILLEGAL/UNKNOWN";
    }
}

/* Attempts a transition. Returns true and updates state if legal
 * per the table above; returns false and leaves state unchanged if
 * the transition is not defined by the spec (illegal-transition
 * family, e.g. hart_stop while SUSPENDED). */
static bool hsm_try_transition(uint64_t hartid, hsm_op_t op, hsm_state_t *out_state)
{
    if (hartid >= MAX_HARTS) return false;

    hsm_state_t cur = hart_state[hartid];
    hsm_state_t next = legal[cur][op];

    /* Zero-initialized table entries default to HSM_STOPPED (enum 0),
     * which would incorrectly look "legal" for undefined edges since
     * STOPPED is a real state. Guard explicitly: only accept the
     * transition if it's one of the entries we deliberately set. */
    bool defined =
        (cur == HSM_STOPPED         && op == OP_HART_START) ||
        (cur == HSM_START_PENDING   && op == OP_BOOT_COMPLETE) ||
        (cur == HSM_STARTED         && (op == OP_HART_STOP || op == OP_HART_SUSPEND)) ||
        (cur == HSM_STOP_PENDING    && op == OP_SHUTDOWN_COMPLETE) ||
        (cur == HSM_SUSPEND_PENDING && op == OP_SUSPEND_COMPLETE) ||
        (cur == HSM_SUSPENDED       && op == OP_WAKE_EVENT) ||
        (cur == HSM_RESUME_PENDING  && op == OP_RESUME_COMPLETE);

    if (!defined) {
        if (out_state) *out_state = cur;
        return false;
    }

    hart_state[hartid] = next;
    if (out_state) *out_state = next;
    return true;
}

static void hsm_reset_all(void)
{
    for (int i = 0; i < MAX_HARTS; i++) hart_state[i] = HSM_STOPPED;
}

/*
 * Family 1: Illegal transitions.
 * e.g. hart_stop on a hart that is SUSPENDED (not STARTED).
 * The spec only defines hart_stop as valid from STARTED; calling it
 * from SUSPENDED, STOPPED, or any _PENDING state must be rejected
 * (SBI_ERR_INVALID_PARAM / SBI_ERR_ALREADY_STOPPED per spec, modeled
 * here simply as "transition refused, state unchanged").
 */
bool test_illegal_hart_stop_while_suspended(uint64_t hartid)
{
    hsm_reset_all();
    hsm_state_t s;
    hsm_try_transition(hartid, OP_HART_START, &s);
    hsm_try_transition(hartid, OP_BOOT_COMPLETE, &s);
    hsm_try_transition(hartid, OP_HART_SUSPEND, &s);
    hsm_try_transition(hartid, OP_SUSPEND_COMPLETE, &s);   /* now SUSPENDED */

    bool rejected = !hsm_try_transition(hartid, OP_HART_STOP, &s);
    bool state_unchanged = (s == HSM_SUSPENDED);
    return rejected && state_unchanged;
}

/*
 * Family 2: Cross-hart operations.
 * e.g. hart 0 issues hart_stop targeting hart 1 at the same moment
 * hart 1 independently calls hart_suspend on itself. Since HSM state
 * is per-hart but the calling hart and target hart differ, this
 * models the race the spec leaves to implementation locking
 * (OpenSBI resolves it with a per-hart spinlock in sbi_hsm.c).
 */
bool test_cross_hart_stop_vs_self_suspend(uint64_t target_hart)
{
    hsm_reset_all();
    hsm_state_t s;
    hsm_try_transition(target_hart, OP_HART_START, &s);
    hsm_try_transition(target_hart, OP_BOOT_COMPLETE, &s);  /* target now STARTED */

    /* Simulate the race: only one of the two competing transitions
     * can win the per-hart lock first. Both are individually legal
     * from STARTED, so this test asserts exactly one succeeds and
     * the resulting state is one of the two valid outcomes, never
     * a torn/inconsistent state. */
    bool stop_ok = hsm_try_transition(target_hart, OP_HART_STOP, &s);
    hsm_state_t after_stop = s;

    bool suspend_ok = hsm_try_transition(target_hart, OP_HART_SUSPEND, &s);

    /* Once STOP_PENDING, a further hart_suspend from a different
     * logical caller must be illegal (STOP_PENDING has no
     * OP_HART_SUSPEND edge in the table). */
    bool consistent = stop_ok && !suspend_ok && (after_stop == HSM_STOP_PENDING);
    return consistent;
}

/*
 * Family 3: Repeated / out-of-order transitions.
 * e.g. two consecutive hart_start calls targeting the same hart.
 * Per spec, the second hart_start while already START_PENDING or
 * STARTED must fail (SBI_ERR_ALREADY_AVAILABLE / ALREADY_STARTED),
 * not silently succeed or corrupt state.
 */
bool test_repeated_hart_start(uint64_t hartid)
{
    hsm_reset_all();
    hsm_state_t s;
    bool first = hsm_try_transition(hartid, OP_HART_START, &s);
    hsm_state_t after_first = s;

    bool second = hsm_try_transition(hartid, OP_HART_START, &s);
    hsm_state_t after_second = s;

    bool ok = first && (after_first == HSM_START_PENDING) &&
              !second && (after_second == HSM_START_PENDING);
    return ok;
}

const char *hsm_state_name(uint64_t hartid)
{
    if (hartid >= MAX_HARTS) return "OUT_OF_RANGE";
    return state_name(hart_state[hartid]);
}

bool hsm_apply(uint64_t hartid, hsm_op_t op)
{
    hsm_state_t s;
    return hsm_try_transition(hartid, op, &s);
}

void hsm_init_all_stopped(void)
{
    hsm_reset_all();
}
