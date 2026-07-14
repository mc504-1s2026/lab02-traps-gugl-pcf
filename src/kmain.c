#include <kernel/printf.h>
#include <kernel/mm.h>
#include <kernel/string.h>
#include <arch/timer.h>
#include <kernel/trap.h>
#include <kernel/serial.h>

extern int _hartid[];

#define SHELL_LINE_MAX	256
#define SHELL_RX_MAX	256

static void shell_prompt()
{
	serial_puts("> ");
}

static void cmd_uptime()
{
	char buf[32];
	u64 secs = timer_read() / TIMER_FREQ;

	snprintf(buf, sizeof(buf), "%us\r\n", secs);
	serial_puts(buf);
}

static void cmd_echo(char *arg)
{
	serial_puts(arg);
	serial_puts("\r\n");
}

static void cmd_alarm(char *arg)
{
	u64 secs = strtou64(arg, 10);
	timer_set_alarm(secs);
	serial_puts("\r\n");
}

static void shell_exec(char *line)
{
	if (strncmp(line, "uptime", 6) == 0) {
		cmd_uptime();
	} else if (strncmp(line, "echo ", 5) == 0) {
		cmd_echo(line + 5);
	} else if (strncmp(line, "alarm ", 6) == 0) {
		cmd_alarm(line + 6);
	} else if (strlen(line) > 0) {
		serial_puts("unknown command: ");
		serial_puts(line);
		serial_puts("\r\n");
	}
}

void kmain()
{
	char line[SHELL_LINE_MAX];
	size_t line_len = 0;
	char rxbuf[SHELL_RX_MAX];
	size_t n;

	printk_set_level(LOG_DEBUG);
	info("entered S-mode\n");
	info("booting on hart %d\n", _hartid[0]);
	info("setting up virtual memory...\n");
	vm_init();

	info("enabling traps...\n");
	trap_setup();
	info("enabling timer...\n");
	timer_irq_enable();
	info("enabling serial...\n");
	serial_init();
	serial_irq_enable();

	serial_puts("\r\nlab02 shell\r\n");
	shell_prompt();

	while (1) {
		//print "alarm" whenever a previously scheduled timer interrupt has fired
		if (timer_alarm_pending()) {
			serial_puts("alarm\r\n");
		}

		// drain whatever serial_irq() has buffered so far
		n = serial_read(rxbuf);
		for (size_t i = 0; i < n; i++) {
			char c = rxbuf[i];

			if (c == '\r') {
				line[line_len] = '\0';
				serial_puts("\r\n");
				shell_exec(line);
				line_len = 0;
				shell_prompt();
			} else if (line_len < SHELL_LINE_MAX - 1) {
				line[line_len++] = c;
			}
		}
	}
}