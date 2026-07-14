#include <kernel/trap.h>
#include <kernel/panic.h>
#include <kernel/printf.h>
#include <arch/csr.h>
#include <arch/timer.h>
#include <arch/plic.h>
#include <kernel/serial.h>

/* defined in src/trap_entry.S */
extern void trap_entry();

static void handle_exception(u64 scause, u64 stval, u64 sepc)
{
	switch (scause) {
	case EXCEPTION_INST_ACCESS_FAULT:
		critical("instruction access fault at %p (sepc=%p)\n", stval, sepc);
		panic("unhandled instruction access fault\n");
		break;
	case EXCEPTION_LOAD_ACCESS_FAULT:
		critical("load access fault at %p (sepc=%p)\n", stval, sepc);
		panic("unhandled load access fault\n");
		break;
	case EXCEPTION_STORE_ACCESS_FAULT:
		critical("store access fault at %p (sepc=%p)\n", stval, sepc);
		panic("unhandled store access fault\n");
		break;
	case EXCEPTION_INST_PAGE_FAULT:
		critical("instruction page fault at %p (sepc=%p)\n", stval, sepc);
		panic("unhandled instruction page fault\n");
		break;
	case EXCEPTION_LOAD_PAGE_FAULT:
		critical("load page fault at %p (sepc=%p)\n", stval, sepc);
		panic("unhandled load page fault\n");
		break;
	case EXCEPTION_STORE_PAGE_FAULT:
		critical("store page fault at %p (sepc=%p)\n", stval, sepc);
		panic("unhandled store page fault\n");
		break;
	default:
		critical("unhandled exception: scause=%p, stval=%p, sepc=%p\n",
			 scause, stval, sepc);
		panic("unhandled exception\n");
	}
}

static void handle_irq(u64 scause)
{
	u32 irq;

	switch (scause) {
	case TRAP_TIMER_IRQ:
		timer_irq();
		break;

	case TRAP_EXTERNAL_IRQ:
		irq = plic_hart_claim_irq(0);
		if (irq == 0) {
			//if some other hart claimed it first
			return;
		}

		switch (irq) {
		case IRQ_SERIAL:
			serial_irq();
			break;
		default:
			warn("trap: unhandled external irq %d\n", irq);
		}

		plic_hart_complete_irq(0, irq);
		break;

	default:
		critical("unhandled interrupt: scause=%p\n", scause);
		panic("unhandled interrupt\n");
	}
}

// called from trap_entry.S after all GPRs have been saved to the stack
void handle_trap()
{
	u64 scause = csr_read(CSR_SCAUSE);
	u64 stval = csr_read(CSR_STVAL);
	u64 sepc = csr_read(CSR_SEPC);

	if (scause & TRAP_IRQ_BIT) {
		handle_irq(scause);
	} else {
		handle_exception(scause, stval, sepc);
	}
}

void trap_setup()
{
	// interrupts stays disabled until timer + serial finishes configuring
	hart_irq_disable();
	csr_write(CSR_STVEC, trap_entry);
}

void hart_irq_enable()
{
	csr_set(CSR_SSTATUS, CSR_SSTATUS_SIE);
}

void hart_irq_disable()
{
	csr_clear(CSR_SSTATUS, CSR_SSTATUS_SIE);
}

u64 hart_irq_save()
{
	u64 sstatus = csr_read(CSR_SSTATUS);
	hart_irq_disable();
	return sstatus & CSR_SSTATUS_SIE;
}

void hart_irq_restore(u64 flags)
{
	if (flags & CSR_SSTATUS_SIE) {
		hart_irq_enable();
	}
}