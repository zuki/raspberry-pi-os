#include "utils.h"
#include "peripherals/uart.h"
#include "peripherals/gpio.h"

void uart_send ( char c )
{
	while (get32(UART0_FR) & (1<<5));
	put32(UART0_DR, c);
}

char uart_recv ( void )
{
	while(get32(UART0_FR) & (1<<4));
	return (get32(UART0_DR) & 0xFF);
}

void uart_send_string(char* str)
{
	for (int i = 0; str[i] != '\0'; i ++) {
		uart_send((char)str[i]);
	}
}

void uart_init ( void )
{
	unsigned int selector;

	selector = get32(GPFSEL1);
	selector &= ~(7<<12);                   // clean gpio14
	selector |= 4<<12;                      // set alt0 for gpio14
	selector &= ~(7<<15);                   // clean gpio15
	selector |= 4<<15;                      // set alt0 for gpio15
	put32(GPFSEL1,selector);

	put32(GPPUD, 0);
	delay(150);
	put32(GPPUDCLK0, (1<<14)|(1<<15));
	delay(150);
	put32(GPPUDCLK0, 0);

	put32(UART0_CR, 0);             // UARTを無効に
    put32(UART0_ICR, 0x7ff);        // 割り込みを全クリア
    put32(UART0_IBRD, 1);           // 通信速度の除数整数部
    put32(UART0_FBRD, 40);          // 通信速度の除数小数部
    put32(UART0_LCRH, 0x70);        // FIFOを有効化, フレーム長を8bits
    put32(UART0_IMSC, 0x7f2);       // すべての割り込みをマスク
    put32(UART0_CR, 0x31);          // UART, 送信、受信を有効化
}
