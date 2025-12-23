#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>

// OC2B pin: PD3 
void buzzer_init(void);

// Nonblocking pattern driver:
// state:
// 0 = SAFE (off)
// 1 = NOTICE (off)
// 2 = PREPARE (200ms ON / 200ms OFF)
// 3 = EVACUATE (siren sweep, 2s full cycle)
void buzzer_task(uint8_t state, uint32_t now_ms);

#endif
