/*
 * trap_entry.S - Minimal M-mode trap entry point (RISC-V rv64gc)
 *
 * Modeled on the save/restore pattern used by OpenSBI's
 * sbi_trap.c / sbi_expected_trap.S, trimmed to the minimum needed
 * to test HSM broadcast/IPI behavior on a single hart and across
 * two harts.
 *
 * On trap:
 *   1. Save all general-purpose registers to a per-hart scratch stack.
 *   2. Call the C-level dispatcher (trap_handler) with a pointer
 *      to the saved register frame.
 *   3. Restore registers and mret back to the interrupted context.
 *
 * mscratch is assumed to hold a pointer to this hart's trap stack,
 * set up during boot before traps are enabled.
 */

.section .text
.global trap_entry
.align 4

# Frame layout: 32 general-purpose regs + mepc + mstatus = 34 x 8 bytes
.equ FRAME_SIZE, 34*8

trap_entry:
    csrrw   sp, mscratch, sp   # swap sp with per-hart trap stack, save user sp in mscratch
    addi    sp, sp, -FRAME_SIZE

    sd      x1,  1*8(sp)
    sd      x3,  3*8(sp)
    sd      x4,  4*8(sp)
    sd      x5,  5*8(sp)
    sd      x6,  6*8(sp)
    sd      x7,  7*8(sp)
    sd      x8,  8*8(sp)
    sd      x9,  9*8(sp)
    sd      x10, 10*8(sp)
    sd      x11, 11*8(sp)
    sd      x12, 12*8(sp)
    sd      x13, 13*8(sp)
    sd      x14, 14*8(sp)
    sd      x15, 15*8(sp)
    sd      x16, 16*8(sp)
    sd      x17, 17*8(sp)
    sd      x18, 18*8(sp)
    sd      x19, 19*8(sp)
    sd      x20, 20*8(sp)
    sd      x21, 21*8(sp)
    sd      x22, 22*8(sp)
    sd      x23, 23*8(sp)
    sd      x24, 24*8(sp)
    sd      x25, 25*8(sp)
    sd      x26, 26*8(sp)
    sd      x27, 27*8(sp)
    sd      x28, 28*8(sp)
    sd      x29, 29*8(sp)
    sd      x30, 30*8(sp)
    sd      x31, 31*8(sp)

    csrr    t0, mscratch       # recover original sp (saved via csrrw above)
    sd      t0, 2*8(sp)        # store original x2 (sp) into frame slot

    csrr    t0, mepc
    sd      t0, 32*8(sp)
    csrr    t0, mstatus
    sd      t0, 33*8(sp)

    mv      a0, sp             # a0 = pointer to trap frame, arg to trap_handler
    call    trap_handler

    ld      t0, 32*8(sp)
    csrw    mepc, t0
    ld      t0, 33*8(sp)
    csrw    mstatus, t0

    ld      x1,  1*8(sp)
    ld      x3,  3*8(sp)
    ld      x4,  4*8(sp)
    ld      x5,  5*8(sp)
    ld      x6,  6*8(sp)
    ld      x7,  7*8(sp)
    ld      x8,  8*8(sp)
    ld      x9,  9*8(sp)
    ld      x10, 10*8(sp)
    ld      x11, 11*8(sp)
    ld      x12, 12*8(sp)
    ld      x13, 13*8(sp)
    ld      x14, 14*8(sp)
    ld      x15, 15*8(sp)
    ld      x16, 16*8(sp)
    ld      x17, 17*8(sp)
    ld      x18, 18*8(sp)
    ld      x19, 19*8(sp)
    ld      x20, 20*8(sp)
    ld      x21, 21*8(sp)
    ld      x22, 22*8(sp)
    ld      x23, 23*8(sp)
    ld      x24, 24*8(sp)
    ld      x25, 25*8(sp)
    ld      x26, 26*8(sp)
    ld      x27, 27*8(sp)
    ld      x28, 28*8(sp)
    ld      x29, 29*8(sp)
    ld      x30, 30*8(sp)
    ld      x31, 31*8(sp)

    ld      x2,  2*8(sp)       # restore original sp
    addi    sp, sp, FRAME_SIZE
    csrrw   sp, mscratch, sp  # restore mscratch to point at trap stack again

    mret
