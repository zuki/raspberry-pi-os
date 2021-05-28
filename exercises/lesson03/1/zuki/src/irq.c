#include "utils.h"
#include "printf.h"
#include "timer.h"
#include "entry.h"
#include "peripherals/irq.h"

const char *entry_error_messages[] = {
	"SYNC_INVALID_EL1t",
	"IRQ_INVALID_EL1t",
	"FIQ_INVALID_EL1t",
	"ERROR_INVALID_EL1T",

	"SYNC_INVALID_EL1h",
	"IRQ_INVALID_EL1h",
	"FIQ_INVALID_EL1h",
	"ERROR_INVALID_EL1h",

	"SYNC_INVALID_EL0_64",
	"IRQ_INVALID_EL0_64",
	"FIQ_INVALID_EL0_64",
	"ERROR_INVALID_EL0_64",

	"SYNC_INVALID_EL0_32",
	"IRQ_INVALID_EL0_32",
	"FIQ_INVALID_EL0_32",
	"ERROR_INVALID_EL0_32"
};

void enable_interrupt_controller()
{
	// "BCM2836 ARM-local peripherals", p.6
    //   Note that these interrupts do not have a 'disable' code.
    //   They are expected to have an enable/disable bit at the source
    //   where they are generated. After a reset all bits are zero thus
    //   all interrupts are send to the IRQ of core 0
}

void show_invalid_entry_message(int type, unsigned long esr, unsigned long address)
{
	printf("%s, ESR: %x, address: %x\r\n", entry_error_messages[type], esr, address);
}

void handle_irq(void)
{
	unsigned int irq = get32(CORE0_INTERRUP_SOURCE);
	switch (irq) {
		case (CORE0_LTIMER):
			handle_local_timer_irq();
			break;
		default:
			printf("Unknown pending irq: %x\r\n", irq);
	}
}
