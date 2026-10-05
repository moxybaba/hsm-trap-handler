# Cloud-V CI result — 2026-10-05

The GitHub Actions workflow completed successfully after Cloud-V resolved
a runner registration-cap issue affecting the Banana Pi BPI-F3 pool.

## Workflow

- Repository: `moxybaba/hsm-trap-handler`
- Branch: `hsm-matrix-v2`
- Requested runner label: `banana-pi-f3`
- Workflow: `RISC-V CI`

## Result

The workflow completed successfully.

The CI workflow:

1. checks out the HSM model and trap-handler scaffold;
2. builds the scaffold using the runner's native RISC-V GCC toolchain;
3. runs `make test`;
4. passes all 16 HSM state-model checks with zero failures.

## Scope

This validates that the model and CI workflow run on a RISC-V runner
selected through the Cloud-V BPI-F3 label. It does not by itself prove
execution of the bare-metal trap entry or a live multi-hart HSM race.
Those remain hardware/firmware-level follow-up tasks.
