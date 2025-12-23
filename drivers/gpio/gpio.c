#include "gpio.h"

void gpio_init(void) {
    // LED outputs on PD4/PD5/PD6
    LEDS_DDR  |= (1 << LED1_BIT) | (1 << LED2_BIT) | (1 << LED3_BIT);
    leds_all_off();

    // TRIG output on PD7
    TRIG_DDR  |= (1 << TRIG_BIT);
    trig_low();
}

void leds_all_off(void) {
    LEDS_PORT &= ~((1 << LED1_BIT) | (1 << LED2_BIT) | (1 << LED3_BIT));
}

void leds_set_evacuate(void) {
    leds_all_off();
    LEDS_PORT |= (1 << LED1_BIT); // D4
}

void leds_set_prepare(void) {
    leds_all_off();
    LEDS_PORT |= (1 << LED2_BIT); // D5
}

void leds_set_safe(void) {
    leds_all_off();
    LEDS_PORT |= (1 << LED3_BIT); // D6
}

void leds_update_by_distance(int16_t cm) {
    // If invalid reading, show Safe
    if (cm < 0) {
        leds_set_safe();
        return;
    }

    // 0 - 30 : Evacuate
    // 31 - 32 Prepare
    // 33+    : Safe
    if (cm <= 30) {
        leds_set_evacuate();
    } else if (cm <= 32) {
        leds_set_prepare();
    } else {
        leds_set_safe();
    }
}