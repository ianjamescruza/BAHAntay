#ifndef UART_H
#define UART_H

#include <stdint.h>

void uart_init(uint32_t baud);

void uart_putc(char c);
void uart_puts(const char *s);

uint8_t uart_available(void);
char uart_getc_nb(void);     // nonblocking: call only if uart_available()

#endif
