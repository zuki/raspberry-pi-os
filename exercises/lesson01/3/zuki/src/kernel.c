#include "mini_uart.h"

void kernel_main(int cpuid)
{
	uart_init();
	uart_send_string("Hello, from processor ");
    uart_send('0'+cpuid);
    uart_send_string("\r\n");

	while (1) {
		uart_send(uart_recv());
	}
}
