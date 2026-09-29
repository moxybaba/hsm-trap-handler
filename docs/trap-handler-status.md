# Minimal Trap-Handler Status

## Current scaffold

The project includes:

- `src/trap_entry.S`: a draft M-mode trap-entry scaffold
- `src/trap_handler.c`: a C-level trap dispatcher scaffold
- per-hart software-interrupt acknowledgement counters
- a placeholder platform hook for clearing a machine software interrupt

## Intended control flow

1. Hardware enters the address held in `mtvec`.
2. Assembly trap entry saves an execution context.
3. The assembly layer calls the C dispatcher.
4. The dispatcher reads `mcause` and distinguishes interrupt from exception.
5. Machine software interrupts are counted per hart for later
   one-hart versus two-hart acknowledgement comparisons.
6. Platform-specific code clears the pending interrupt source.
7. Assembly restores state and returns with `mret`.

## Current limitations

This is not yet a deployable K1/OpenSBI patch.

- The actual trap frame must be checked against OpenSBI's register-save
  convention before use.
- `mscratch` ownership and stack switching must be integrated with the
  surrounding firmware boot path.
- `clear_soft_irq()` is intentionally a placeholder until the correct
  K1 interrupt-controller/ACLINT interface is confirmed.
- The actual broadcast test requires a running multi-hart environment,
  an interrupt delivery mechanism, and an observable acknowledgement
  channel such as per-hart counters or a shared trace buffer.

## Next hardware-dependent work

1. Confirm K1's IPI/MSIP mechanism from the platform device tree and
   OpenSBI's SpacemiT platform code.
2. Replace the placeholder clear operation with the platform mechanism.
3. Configure `mtvec`, per-hart trap stacks, and `mscratch`.
4. Run a one-target-hart interrupt test.
5. Run a two-target-hart broadcast test.
6. Record acknowledgement sequence, counts, and final HSM states.
