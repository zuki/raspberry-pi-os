#include "mini_uart.h"
#include "utils.h"

void kernel_main(int cpuid)
{
    static int need_init = 1;

    if (cpuid == 0) {
        uart_send('a');         // これらのuar_send()を削除すると動かない
        delay(1);
        uart_send('b');
    } else {
        uart_send('c');
        delay(cpuid * 10000000);    // cpu_0がuart_init()を処理するための最適delay
        uart_send('d');
    }

    if (need_init) {
        uart_send('y');
        uart_init();
        need_init = 0;
        uart_send('z');
    }

    //uart_init();
	uart_send_string("Hello, from processor ");
    uart_send('0'+cpuid);
    uart_send_string("\r\n");

    if (cpuid == 0) {
        while (1) {
            uart_send(uart_recv());
        }
    } else {
        asm("wfe");
    }

}
