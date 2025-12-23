#include "buzzer.h"
#include <avr/io.h>

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

// Timer2 prescaler: 64
#define T2_PRESC  64UL
#define T2_BASE   (F_CPU / (2UL * T2_PRESC))

static inline uint8_t freq_to_ocr(uint16_t f_hz) {
    if (f_hz < 100) f_hz = 100;
    uint32_t ocr = (T2_BASE / (uint32_t)f_hz);
    if (ocr == 0) ocr = 1;
    ocr -= 1;
    if (ocr > 255) ocr = 255;
    return (uint8_t)ocr;
}

static inline void buzzer_hw_off(void) {
    // Disconnect toggle on OC2B
    TCCR2A &= ~(1 << COM2B0);
    // Force pin low
    PORTD &= ~(1 << PD3);
}

static inline void buzzer_hw_on(uint16_t f_hz) {
    uint8_t o = freq_to_ocr(f_hz);
    OCR2A = o;   
    OCR2B = o;   
    TCCR2A |= (1 << COM2B0); // Toggle OC2B on compare match
}

void buzzer_init(void) {
    // PD3 output (OC2B)
    DDRD |= (1 << PD3);
    PORTD &= ~(1 << PD3);

    // Timer2: CTC mode (TOP=OCR2A)
    TCCR2A = (1 << WGM21);
    
    // Prescaler /64
    TCCR2B = (1 << CS22);

    OCR2A = 200;
    OCR2B = 200;

    buzzer_hw_off();
}

// 3 States
#define STATE_SAFE      0
#define STATE_PREPARE   1
#define STATE_EVAC      2

void buzzer_task(uint8_t state, uint32_t now_ms) {
    static uint8_t prev_state = 255;
    static uint32_t state_start = 0;

    if (state != prev_state) {
        prev_state = state;
        state_start = now_ms;
        buzzer_hw_off();
    }

    if (state == STATE_EVAC) {
        // Siren sweep: 2 seconds full cycle (up then down)
        uint32_t t = (now_ms - state_start) % 2000UL;
        uint32_t tri = (t < 1000UL) ? t : (2000UL - t); 

        const uint16_t F_MIN = 600;
        const uint16_t F_MAX = 1800;

        uint16_t f = (uint16_t)(F_MIN + ((uint32_t)(F_MAX - F_MIN) * tri) / 1000UL);
        buzzer_hw_on(f);
        return;
    }

    if (state == STATE_PREPARE) {
        // Beep: 200ms ON, 200ms OFF
        uint32_t t = (now_ms - state_start) % 400UL;
        if (t < 200UL) buzzer_hw_on(1200);
        else buzzer_hw_off();
        return;
    }

    // STATE_SAFE => OFF
    buzzer_hw_off();
}