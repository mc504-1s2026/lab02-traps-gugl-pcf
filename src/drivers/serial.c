#include <kernel/serial.h>
#include <kernel/panic.h>
#include <kernel/printf.h>
#include <kernel/string.h>
#include <kernel/trap.h>
#include <arch/io.h>
#include <arch/csr.h>
#include <arch/spinlock.h>
#include <arch/plic.h>

#define SERIAL_RX_BUF_SIZE 256

 //internal state for the serial driver
static struct serialdev {
	char buf[SERIAL_RX_BUF_SIZE];	//buffer storing bytes received via serial_irq()
	size_t len;						//number of bytes currently stored in buf
	struct spinlock lock;			//protects buf/len
} dev;

static inline void *serial_reg(u64 offset)
{
	return (void *)((u64) SERIAL_BASE + offset);
}

static inline u8 serial_read_reg(u64 offset)
{
	return ioread8(serial_reg(offset));
}

static inline void serial_write_reg(u64 offset, u8 val)
{
	iowrite8(val, serial_reg(offset));
}

void serial_init()
{
	spin_init(&dev.lock);
	dev.len = 0;

	// disable all interrupts while we configure the device
	serial_write_reg(SERIAL_IER, 0);

	//enable the FIFOs and reset both the RX and TX ones
	serial_write_reg(SERIAL_FCR, SERIAL_FCR_FIFO_ENABLE | SERIAL_FCR_RX_FIFO_CLEAR | SERIAL_FCR_TX_FIFO_CLEAR);

	// enable "Received Data Available" interrupts
	serial_write_reg(SERIAL_IER, SERIAL_IER_ERBFI);
}

void serial_irq_enable()
{
	// configure the PLIC to route the serial IRQ to hart 0
	plic_irq_set_priority((u32) IRQ_SERIAL, 1);
	plic_hart_set_threshold(0, 0);
	plic_hart_enable_irq(0, (u32) IRQ_SERIAL);

	// enable external interrupts for this hart
	csr_set(CSR_SIE, CSR_SIE_SEIE);
}

void serial_irq_disable()
{
	csr_clear(CSR_SIE, CSR_SIE_SEIE);
}

void serial_irq()
{
	u8 lsr;
	char c;

	//keep draining the FIFOs while data is available in case passed 8 bytes since last interrupt.
	while ((lsr = serial_read_reg(SERIAL_LSR)) & SERIAL_LSR_DTR) {
		c = (char) serial_read_reg(SERIAL_RBR);

		spin_lock(&dev.lock);
		if (dev.len < SERIAL_RX_BUF_SIZE) {
			dev.buf[dev.len++] = c;
		} else {
			warn("serial: rx buffer full, dropping byte 0x%x\n", c);
		}
		spin_unlock(&dev.lock);

		//echo the character back so the user can see what they typed
		serial_putc(c);
	}
}

size_t serial_read(char *buf)
{
	size_t size;
	u64 flags;

	// disable interrupts to avoid deadlocking against serial_irq() over dev.lock
	flags = hart_irq_save();
	spin_lock(&dev.lock);
	size = dev.len;
	memcpy(buf, dev.buf, size);
	dev.len = 0;
	spin_unlock(&dev.lock);
	hart_irq_restore(flags);

	return size;
}

void serial_putc(char c)
{
	// busy-wait until the Transmitter Holding Register is empty
	while (!(serial_read_reg(SERIAL_LSR) & SERIAL_LSR_THRE)) {}
	serial_write_reg(SERIAL_THR, (u8) c);
}

void serial_puts(char *s)
{
	while (*s != '\0') {
		serial_putc(*s++);
	}
}