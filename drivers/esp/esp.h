#ifndef ESP_H
#define ESP_H

#include <stdint.h>

void timebase_init(void);
uint32_t millis(void);

// Call frequently (main loop)
void esp_task(void);

// Start connecting WiFi
void esp_begin(const char *ssid, const char *pass);

// True when WiFi init finished and ESP is ready to send
uint8_t esp_ready(void);

// Request sending a value (nonblocking). returns 1 if accepted.
uint8_t esp_request_send_field1(const char *api_key, int16_t value_cm);

// NEW: 1 when ESP is currently doing CIPSTART/CIPSEND/HTTP/CIPCLOSE sequence
uint8_t esp_is_uploading(void);

#endif
