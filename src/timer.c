#include <arch/timer.h>
#include <arch/csr.h>
#include <arch/spinlock.h>
#include <kernel/trap.h>
#include <kernel/bits.h>
#include <kernel/printf.h>

// protects alarm_pending
static struct spinlock alarm_lock;
static volatile bool alarm_pending;

u64 timer_read()
{
	return csr_read(CSR_TIME);
}

void timer_irq_enable()
{
	spin_init(&alarm_lock);
	alarm_pending = false;

	// push stimecmp far into the future so we don't get a false timer interrupt before the user actuall sets an alarm
	csr_write(CSR_STIMECMP, UINT64_MAX);

	// enable the timer interrupt source and interrupts globally for this hart
	csr_set(CSR_SIE, CSR_SIE_STIE);
	hart_irq_enable();
}

void timer_irq_disable()
{
	csr_clear(CSR_SIE, CSR_SIE_STIE);
}

void timer_set_alarm(u64 secs)
{
	u64 now = csr_read(CSR_TIME);
	csr_write(CSR_STIMECMP, now + secs * TIMER_FREQ);
}

void timer_irq()
{
	//push stimecmp into the future so this interrupt doesn't keep re-firing
	csr_write(CSR_STIMECMP, UINT64_MAX);

	spin_lock(&alarm_lock);
	alarm_pending = true;
	spin_unlock(&alarm_lock);
}

bool timer_alarm_pending()
{
	bool pending;

	spin_lock(&alarm_lock);
	pending = alarm_pending;
	alarm_pending = false;
	spin_unlock(&alarm_lock);

	return pending;
}