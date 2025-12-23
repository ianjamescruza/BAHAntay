#include "twi.h"
#include <avr/io.h>

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

void twi_init(uint32_t scl_hz) {
    // Prescaler = 1
    TWSR = 0x00;

    // SCL = F_CPU / (16 + 2*TWBR*prescaler)
    // => TWBR = ((F_CPU/SCL) - 16) / 2
    uint32_t twbr = ((F_CPU / scl_hz) - 16UL) / 2UL;
    if (twbr > 255) twbr = 255;
    TWBR = (uint8_t)twbr;

    // Enable TWI
    TWCR = (1 << TWEN);
}

static uint8_t twi_wait(void) {
    while (!(TWCR & (1 << TWINT))) {}
    return (TWSR & 0xF8);
}

uint8_t twi_start(uint8_t address_rw) {
    // Send START
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    uint8_t st = twi_wait();
    if (st != 0x08 && st != 0x10) return 0; // START / REPEATED START

    // Send SLA+W or SLA+R
    TWDR = address_rw;
    TWCR = (1 << TWINT) | (1 << TWEN);
    st = twi_wait();

    // SLA+W ACK = 0x18, SLA+R ACK = 0x40
    if (st != 0x18 && st != 0x40) return 0;
    return 1;
}

uint8_t twi_write(uint8_t data) {
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    uint8_t st = twi_wait();
    // DATA ACK = 0x28
    return (st == 0x28);
}

void twi_stop(void) {
    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);
}
