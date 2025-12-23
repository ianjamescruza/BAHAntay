#ifndef TWI_H
#define TWI_H

#include <stdint.h>

// Init AVR TWI (I2C) hardware at desired SCL (e.g., 100000)
void twi_init(uint32_t scl_hz);

// Start and address device: address_rw = (addr<<1)|0 for write, |1 for read
uint8_t twi_start(uint8_t address_rw);

// Write one byte, returns 1 if ACK
uint8_t twi_write(uint8_t data);

// Stop condition
void twi_stop(void);

#endif
