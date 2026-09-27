/*
 * trap_handler.c - Minimal M-mode trap dispatcher (RISC-V rv64gc)
 *
 * Structure mirrors OpenSBI's sbi_trap.c dispatch pattern: inspect
 * mcause to decide interrupt vs exception, then route to the
 * relevant handler. This skeleton implements just enough to support
 * the HSM broadcast test: it recognizes machine software interrupts
 * (used to deliver IPIs for hart_start/hart_stop/hart_suspend) and
 * counts/logs them per hart, which is what the single-hart vs
 * two-hart acknowledgment-shape comparison needs.
 *
 * Reference: RISC-V Privileged Spec sec. 3.1.9 (mcause encoding),
 * OpenSBI lib/sbi/sbi_trap.c, lib/sbi/sbi_ecall_hsm.c.
 */

#include <stdint.h>

#define MCAUSE_INTERRUPT_BIT   (1UL << 63)
#define MCAUSE_CODE_MASK       0x7fffffffffffffffUL

/* Machine-mode interrupt cause codes (subset relevant to HSM/IPI) */
#define IRQ_M_SOFT   3   /* Machine software interrupt: used for IPI (hart_start/stop/suspend wakeups) */
#define IRQ_M_TIMER  7
#define IRQ_M_EXT    11

/* Trap frame layout must match trap_entry.S exactly */
struct trap_frame {
    uint64_t regs[32];
    uint64_t mepc;
    uint64_t mstatus;
};

/* Per-hart IPI counters, indexed by hart id. Sized generously for
 * K1's 8 cores; extend if testing on larger core-count boards. */
#define MAX_HARTS 8
static volatile uint64_t ipi_count[MAX_HARTS];

/* Reads the current hart id (mhartid CSR). */
static inline uint64_t read_hartid(void)
{
    uint64_t hartid;
    __asm__ volatile ("csrr %0, mhartid" : "=r"(hartid));
    return hartid;
}

/* Clears the machine software interrupt pending bit for this hart.
 * On a real platform this is done via the CLINT/ACLINT MSIP register
 * or the HSM-specific IPI-clear mechanism; wire this up to match the
 * board's actual interrupt controller before running on hardware. */
static void clear_soft_irq(void)
{
    /* Placeholder: replace with platform-specific MSIP clear,
     * e.g. writing 0 to CLINT->MSIP[hartid] on K1. */
}

/* Called from trap_entry.S with a pointer to the saved register frame.
 * This is the single point where HSM broadcast test instrumentation
 * hooks in: every IPI delivery increments this hart's counter, so a
 * one-hart run and a two-hart run can be compared by inspecting
 * ipi_count[] after the test completes. */
void trap_handler(struct trap_frame *frame)
{
    uint64_t mcause;
    __asm__ volatile ("csrr %0, mcause" : "=r"(mcause));

    uint64_t hartid = read_hartid();
    int is_interrupt = (mcause & MCAUSE_INTERRUPT_BIT) != 0;
    uint64_t code = mcause & MCAUSE_CODE_MASK;

    if (is_interrupt) {
        switch (code) {
        case IRQ_M_SOFT:
            /* This is the IPI path HSM uses to wake a hart for
             * hart_start, or to signal hart_stop/hart_suspend
             * completion. Broadcast test: trigger this on hart 0
             * alone, then on hart 0 + hart 1 together, and diff
             * the resulting ipi_count[] and any HSM state snapshot. */
            if (hartid < MAX_HARTS) {
                ipi_count[hartid]++;
            }
            clear_soft_irq();
            break;
        case IRQ_M_TIMER:
            /* Not exercised by this skeleton; extend if HSM suspend
             * timeouts need to be tested. */
            break;
        case IRQ_M_EXT:
            break;
        default:
            break;
        }
    } else {
        /* Synchronous exception (illegal instruction, misaligned
         * access, ecall, etc). Not the focus of the HSM broadcast
         * test, but a real trap handler must not silently drop these.
         * frame->mepc points at the faulting instruction. */
        (void)frame;
    }
}

/* Exposed for the test harness to read counters after a broadcast run. */
uint64_t trap_handler_get_ipi_count(uint64_t hartid)
{
    if (hartid >= MAX_HARTS) return 0;
    return ipi_count[hartid];
}
