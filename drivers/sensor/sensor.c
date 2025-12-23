#include "sensor.h"
#include "gpio.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

// Timer1 prescaler 8 => 2MHz => 0.5us per tick
// 30ms timeout => 60000 ticks
#define TIMEOUT_TICKS 60000U

typedef enum { S_IDLE=0, S_WAIT_RISE, S_WAIT_FALL, S_DONE, S_TIMEOUT } s_state_t;

static volatile s_state_t st = S_IDLE;
static volatile uint16_t t_rise = 0;
static volatile uint16_t t_fall = 0;
static volatile int16_t cm_val = -1;

static inline void t1_setup_running(void) {
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1  = 0;

    // normal mode, prescaler /8
    TCCR1B = (1 << CS11);

    // Input Capture noise canceller ON (optional)
    TCCR1B |= (1 << ICNC1);

    // Start on rising edge
    TCCR1B |= (1 << ICES1);

    // Clear flags
    TIFR1 = (1 << ICF1) | (1 << OCF1A) | (1 << TOV1);

    // Enable Input Capture + CompareA (timeout)
    TIMSK1 = (1 << ICIE1) | (1 << OCIE1A);

    // timeout at TIMEOUT_TICKS
    OCR1A = TIMEOUT_TICKS;
}

void sensor_init(void) {
    // PB0/ICP1 (D8) input
    DDRB &= ~(1 << PB0);
    PORTB &= ~(1 << PB0); // no pullup

    st = S_IDLE;
    cm_val = -1;
}

void sensor_start(void) {
    if (st == S_WAIT_RISE || st == S_WAIT_FALL) return;

    cm_val = -1;
    st = S_WAIT_RISE;

    // Trigger pulse (10us) — tiny delay is fine
    trig_low();
    _delay_us(3);
    trig_high();
    _delay_us(10);
    trig_low();

    t1_setup_running();
}

uint8_t sensor_done(void) {
    return (st == S_DONE || st == S_TIMEOUT);
}

int16_t sensor_get_cm(void) {
    int16_t v;

    uint8_t s = SREG;
    cli();
    v = cm_val;

    // ✅ consume the result so sensor_done() won't stay true forever
    if (st == S_DONE || st == S_TIMEOUT) {
        st = S_IDLE;
    }

    SREG = s;
    return v;
}


ISR(TIMER1_CAPT_vect) {
    uint16_t cap = ICR1;

    if (st == S_WAIT_RISE) {
        t_rise = cap;
        // switch to falling edge
        TCCR1B &= ~(1 << ICES1);
        st = S_WAIT_FALL;
    } else if (st == S_WAIT_FALL) {
        t_fall = cap;

        // stop interrupts for this measurement
        TIMSK1 = 0;

        uint16_t dt = (uint16_t)(t_fall - t_rise); // wraps OK
        // cm = (dt*0.5us)/58us ≈ dt/116
        cm_val = (int16_t)(dt / 116U);

        st = S_DONE;
    }
}

ISR(TIMER1_COMPA_vect) {
    // timeout
    if (st == S_WAIT_RISE || st == S_WAIT_FALL) {
        TIMSK1 = 0;
        cm_val = -1;
        st = S_TIMEOUT;
    }
}
