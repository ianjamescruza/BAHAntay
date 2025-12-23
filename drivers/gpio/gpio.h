#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>
#include <avr/io.h>

#define LEDS_DDR   DDRD
#define LEDS_PORT  PORTD

#define LED1_BIT   PD4   // EVACUATE (Red)
#define LED2_BIT   PD5   // PREPARE  (Yellow)
#define LED3_BIT   PD6   // SAFE     (Green)

// Ultrasonic TRIG = D7 = PD7
#define TRIG_DDR   DDRD
#define TRIG_PORT  PORTD
#define TRIG_BIT   PD7

void gpio_init(void);

//LED Helpers
void leds_all_off(void);
void leds_set_evacuate(void); // LED1
void leds_set_prepare(void);  // LED2
void leds_set_safe(void);     // LED3

// Update LEDs based on distance in cm
void leds_update_by_distance(int16_t cm);

// Trigger helpers
static inline void trig_low(void){ TRIG_PORT &= ~(1 << TRIG_BIT); }
static inline void trig_high(void){ TRIG_PORT |=  (1 << TRIG_BIT); }

#endif