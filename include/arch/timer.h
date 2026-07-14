#ifndef __TICK_H__
#define __TICK_H__

#include <kernel/types.h>

#define TIMER_FREQ 10000000

void timer_irq_enable();
void timer_irq_disable();
void timer_irq();
u64 timer_read();
void timer_set_alarm(u64 secs);

bool timer_alarm_pending(); //check and consume whether an alarm has fired and return True

#endif