#include "printf.h"
#include "utils.h"
#include "timer.h"
#include "irq.h"
#include "fork.h"
#include "sched.h"
#include "mini_uart.h"
#include "mm.h"

extern unsigned long text_start;
extern unsigned long data_start;
extern unsigned long bss_begin;
extern unsigned long bss_end;

void process(char *array)
{
	while (1){
		for (int i = 0; i < 5; i++){
			uart_send(array[i]);
			delay(1000000);
		}
	}
}

unsigned int fib(unsigned long n) {
    if (n == 0 || n == 1)
        return n;
    return fib(n - 2) + fib(n - 1);
}

void proc_fib(void) {
    for (unsigned long n=0; ; n++) {
        printf("%u, ", fib(n));
        delay(10000000);
    }
}

void kernel_main(void)
{
	uart_init();
	init_printf(0, putc);
	irq_vector_init();
	timer_init();
	enable_interrupt_controller();
	enable_irq();

    printf("text  start: 0x%x\r\n", text_start);
    printf("data  start: 0x%x\r\n", data_start);
    printf("bss   begin: 0x%x\r\n", bss_begin);
    printf("bss     end: 0x%x\r\n", bss_end);
    printf("HIGH_MEMORY: 0x%x\r\n", HIGH_MEMORY);
    printf("init        [%d]: pointer: 0x%x, stack: 0x%x\r\n", 0, current, LOW_MEMORY);

	int res = copy_process((unsigned long)&process, (unsigned long)"12345");
	if (res != 0) {
		printf("error while starting process 1");
		return;
	}
	res = copy_process((unsigned long)&process, (unsigned long)"abcde");
	if (res != 0) {
		printf("error while starting process 2");
		return;
	}
    res = copy_process((unsigned long)&proc_fib, 0);
	if (res != 0) {
		printf("error while starting process 3");
		return;
	}

	while (1){
		schedule();
	}
}
