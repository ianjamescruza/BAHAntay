#include "uart.h"
#include <avr/io.h>
#include <avr/interrupt.h>

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#define RX_BUF_SZ 256

static volatile uint8_t rx_buf[RX_BUF_SZ];
static volatile uint8_t rx_head = 0;
static volatile uint8_t rx_tail = 0;

ISR(USART_RX_vect) {
    uint8_t c = UDR0;
    uint8_t next = (uint8_t)(rx_head + 1);
    if (next == rx_tail) {
        // overflow: drop byte
        return;
    }
    rx_buf[rx_head] = c;
    rx_head = next;
}

void uart_init(uint32_t baud) {
    uint16_t ubrr = (uint16_t)((F_CPU / (16UL * baud)) - 1UL);

    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)(ubrr & 0xFF);

    UCSR0A = 0;
    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);  // RX/TX + RX interrupt
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);                // 8N1
}

void uart_putc(char c) {
    while (!(UCSR0A & (1 << UDRE0))) {}
    UDR0 = (uint8_t)c;
}

void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}

uint8_t uart_available(void) {
    return (rx_head != rx_tail);
}

char uart_getc_nb(void) {
    char c = (char)rx_buf[rx_tail];
    rx_tail = (uint8_t)(rx_tail + 1);
    return c;
}
