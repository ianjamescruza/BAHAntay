#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <stdio.h>

#include "gpio.h"
#include "uart.h"
#include "sensor.h"
#include "esp.h"
#include "buzzer.h"
#include "lcd_i2c.h"

//CONFIGURATION 
#define WIFI_SSID  "IJBC"
#define WIFI_PASS  "ian12345"
#define THINGSPEAK_API_KEY  "VYAV7M3MXXHXHFVE"

//STATE DEFINITIONS
#define STATE_SAFE      0
#define STATE_PREPARE   1
#define STATE_EVAC      2

//FILTERING VARIABLES
#define FILTER_N 9
static int16_t samples[FILTER_N];
static uint8_t s_idx = 0;
static uint8_t s_count = 0;

//CONFIDENCE / DEBOUNCE VARIABLES 
// 30 samples at 50ms = 1.5 seconds of consistent reading required to switch
#define CONFIDENCE_THRESHOLD 30 
static uint8_t pending_state = STATE_SAFE;
static uint8_t stability_counter = 0;
static uint8_t current_active_state = STATE_SAFE; 

//MEDIAN FILTER 
static void sort_array(int16_t *arr, uint8_t n) {
    for (uint8_t i = 0; i < n - 1; i++) {
        for (uint8_t j = 0; j < n - i - 1; j++) {
            if (arr[j] > arr[j + 1]) {
                int16_t temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}

static int16_t get_median_cm(int16_t new_val) {
    if (new_val <= 0 || new_val > 400) {
        if (s_count == 0) return -1;
        int16_t temp[FILTER_N];
        for(uint8_t k=0; k<s_count; k++) temp[k] = samples[k];
        sort_array(temp, s_count);
        return temp[s_count/2];
    }
    samples[s_idx] = new_val;
    s_idx = (s_idx + 1) % FILTER_N;
    if (s_count < FILTER_N) s_count++;

    int16_t temp[FILTER_N];
    for (uint8_t i = 0; i < s_count; i++) temp[i] = samples[i];
    sort_array(temp, s_count);
    return temp[s_count / 2];
}

//CLASSIFICATION 
static uint8_t classify_instant(int16_t cm) {
    if (cm < 0) return STATE_SAFE;
    if (cm <= 29) return STATE_EVAC;   // 0-29 cm
    if (cm <= 33) return STATE_PREPARE;// 30-33 cm
    return STATE_SAFE;                 // 35+ cm
}

static const char* status_label(uint8_t state) {
    switch(state) {
        case STATE_EVAC:    return "    EVACUATE    ";
        case STATE_PREPARE: return "     PREPARE    ";
        case STATE_SAFE:    return "      SAFE      ";
        default:            return "      SAFE      ";
    }
}

int main(void) {
    gpio_init();
    uart_init(9600);
    timebase_init();
    sensor_init();
    buzzer_init();
    lcd_init();

    sei(); // ENABLE GLOBAL INTERRUPTS

    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_print_16("Connecting WiFi");
    
    esp_begin(WIFI_SSID, WIFI_PASS);

    uint32_t nextMeasure = 0;
    uint32_t lastSend = 0;
    uint32_t lastLcd = 0;
    
    uint8_t showedReady = 0;
    uint32_t readyShownAt = 0;

    int16_t raw_cm = -1;
    int16_t stable_cm = -1; 

    while (1) {
        uint32_t now = millis();
        esp_task(); // WiFi State Machine (UART Interrupt Driven)

        //Trigger Measurement (Timer1 Interrupt Driven)
        if ((int32_t)(now - nextMeasure) >= 0) {
            nextMeasure = now + 50; 
            sensor_start(); 
        }

        //Process Data 
        if (sensor_done()) {
            raw_cm = sensor_get_cm();
            
            //Mathematical Smoothing
            stable_cm = get_median_cm(raw_cm);

            //Check which state this value BELONGS to
            uint8_t detected_state = classify_instant(stable_cm);

            // Confidence Check 
            if (detected_state == pending_state) {
                if (stability_counter < CONFIDENCE_THRESHOLD) {
                    stability_counter++;
                } else {
                    // Confirmed! Update the REAL state
                    current_active_state = pending_state;
                }
            } else {
                // Fluke or Change? Reset and wait for proof
                pending_state = detected_state;
                stability_counter = 0;
            }
            
            //Update LEDs based on CONFIRMED State (Not raw distance)
            switch(current_active_state) {
                case STATE_EVAC:    leds_set_evacuate(); break;
                case STATE_PREPARE: leds_set_prepare();  break;
                case STATE_SAFE:    leds_set_safe();     break;
            }
        }

        //Update Buzzer 
        buzzer_task(current_active_state, now);

        //Update Cloud
        if (esp_ready() && stable_cm > 0 && (now - lastSend) > 20000UL) {
            if (esp_request_send_field1(THINGSPEAK_API_KEY, stable_cm)) {
                lastSend = now;
            }
        }

        //Update LCD
        if (esp_ready() && !showedReady) {
            showedReady = 1;
            readyShownAt = now;
            lcd_clear();
            lcd_print_16("System Ready");
        }
        if (showedReady && readyShownAt != 0 && (now - readyShownAt) >= 1000UL) {
            readyShownAt = 0;
            lcd_clear();
        }

        if (showedReady && readyShownAt == 0 && (now - lastLcd) >= 300UL) {
            lastLcd = now;

            char line1[17];
            if (stable_cm < 0) snprintf(line1, sizeof(line1), "Level: --- cm");
            else snprintf(line1, sizeof(line1), "Level: %d cm", (int)stable_cm);

            lcd_set_cursor(0, 0);
            lcd_print_16(line1);

            lcd_set_cursor(0, 1);
            if (esp_is_uploading()) {
                lcd_print_16(">> UPLOADING >>");
            } else {
                lcd_print_16(status_label(current_active_state));
            }
        }
    }
}