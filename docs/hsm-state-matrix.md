# HSM State Matrix and Test Rationale

## Scope

This document derives a test matrix from the RISC-V SBI Hart State
Management (HSM) extension. It covers the state model, legal lifecycle
edges, rejection expectations, concurrent-operation scenarios, and
repeated/out-of-order requests.

This is a model and test harness, not a replacement for OpenSBI's
platform implementation. The implementation reference is the local
OpenSBI tree, especially:

- `lib/sbi/sbi_hsm.c`
- `include/sbi/sbi_hsm.h`
- `lib/utils/hsm/fdt_hsm_spacemit.c`
- `lib/sbi/sbi_trap.c`

## SBI HSM states

| State | Meaning |
|---|---|
| STARTED | Hart is powered up and executing supervisor-mode code normally |
| STOPPED | Hart is not executing supervisor-mode or lower-privilege code |
| START_PENDING | A request has been accepted to start a stopped hart |
| STOP_PENDING | The calling started hart has requested to stop itself |
| SUSPENDED | Hart is in a platform-specific low-power state |
| SUSPEND_PENDING | The calling started hart has requested suspension |
| RESUME_PENDING | An interrupt or platform event is resuming a suspended hart |

## Base lifecycle edges

| Current state | Event | Expected next state |
|---|---|---|
| STOPPED | hart_start accepted | START_PENDING |
| START_PENDING | target boot completes | STARTED |
| STARTED | current hart requests hart_stop | STOP_PENDING |
| STOP_PENDING | stop completes | STOPPED |
| STARTED | current hart requests hart_suspend | SUSPEND_PENDING |
| SUSPEND_PENDING | suspend completes | SUSPENDED |
| SUSPENDED | interrupt or platform wake event | RESUME_PENDING |
| RESUME_PENDING | resume completes | STARTED |

## Important ownership constraints

`hart_start(target_hart, start_addr, opaque)` is a cross-hart operation:
a running caller requests that a target hart transition from STOPPED to
START_PENDING.

`hart_stop()` is self-only: it stops the calling hart. It is not a
standard SBI operation for hart 0 to stop hart 1.

`hart_suspend()` is also self-only: it suspends the calling hart.

The model therefore separates:

1. target-hart start races,
2. self-stop and self-suspend lifecycle edges,
3. external observation of races between independently executing harts.

## Test families

### Family A: Illegal transitions

These tests verify that invalid edges are rejected without mutating the
current state.

Examples:

- hart_start against a START_PENDING target
- hart_start against a STARTED target
- hart_stop attempted when the current hart is SUSPENDED
- hart_suspend attempted when the current hart is STOPPED
- wake event delivered to a STARTED hart
- lifecycle completion event delivered from the wrong pending state

### Family B: Cross-hart operations and races

These tests model multiple harts acting concurrently.

Examples:

- hart 0 and hart 2 issue hart_start for the same STOPPED target hart 1:
  exactly one caller may claim STOPPED -> START_PENDING.
- hart 0 observes hart 1 while hart 1 transitions through
  SUSPEND_PENDING -> SUSPENDED -> RESUME_PENDING. Observed status may
  vary because the SBI specification permits concurrent state changes.
- an external test harness sends an IPI/broadcast wake event while target
  harts are suspended. One-hart and two-hart runs record per-hart
  acknowledgements and final state snapshots.

### Family C: Repeated and out-of-order operations

Examples:

- two consecutive hart_start calls for one target
- completion event before its initiating operation
- duplicate boot completion
- duplicate suspend completion
- duplicate resume completion
- wake event before suspension has completed

## Evidence to record on hardware

For each hardware run:

- Board/platform and firmware revision
- Number of harts participating
- Initial state of each hart
- Requested operations in order
- Per-hart acknowledgement counter or trace
- Observed final state of each hart
- Return/error values
- Whether the result is spec-defined, implementation-defined, or a
  rejected invalid request

Negative results are recorded as results: a verified non-transition or
rejected edge establishes a boundary for further work.
