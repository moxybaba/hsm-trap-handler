/*
 * test_hsm_matrix.c - Runs the three edge-case families against the
 * HSM state matrix in src/hsm.c, plus the base legal-path checks.
 * Built and run on the host/native toolchain via `make test`
 * (see Makefile), independent of the bare-metal trap handler build.
 */

#include <stdio.h>
#include <stdbool.h>
#include "hsm.h"

static int failures = 0;

#define CHECK(name, cond) do { \
    if (cond) { \
        printf("[PASS] %s\n", name); \
    } else { \
        printf("[FAIL] %s\n", name); \
        failures++; \
    } \
} while (0)

int main(void)
{
    /* Base path sanity: STOPPED -> START_PENDING -> STARTED -> STOP_PENDING -> STOPPED */
    hsm_init_all_stopped();
    CHECK("base: hart_start from STOPPED succeeds",
          hsm_apply(0, OP_HART_START));
    CHECK("base: boot_complete reaches STARTED",
          hsm_apply(0, OP_BOOT_COMPLETE));
    CHECK("base: hart_stop from STARTED succeeds",
          hsm_apply(0, OP_HART_STOP));
    CHECK("base: shutdown_complete returns to STOPPED",
          hsm_apply(0, OP_SHUTDOWN_COMPLETE));

    /* Family 1: illegal transitions */
    CHECK("family1: hart_stop while SUSPENDED is rejected",
          test_illegal_hart_stop_while_suspended(1));

    /* Family 2: cross-hart operations */
    CHECK("family2: concurrent stop vs self-suspend resolves consistently",
          test_cross_hart_stop_vs_self_suspend(2));

    /* Family 3: repeated / out-of-order transitions */
    CHECK("family3: second consecutive hart_start is rejected",
          test_repeated_hart_start(3));

    printf("\n%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
