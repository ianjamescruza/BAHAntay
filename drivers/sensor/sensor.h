#ifndef SENSOR_H
#define SENSOR_H

#include <stdint.h>

void sensor_init(void);

// Start a measurement (nonblocking)
void sensor_start(void);

// Check if measurement is done
uint8_t sensor_done(void);

// Get result (valid only if done). returns -1 if timeout
int16_t sensor_get_cm(void);

#endif
