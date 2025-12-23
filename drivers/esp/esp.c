#include "esp.h"
#include "uart.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>
#include <stdio.h>

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

//millis using Timer0 CTC 1ms 
static volatile uint32_t g_ms = 0;

ISR(TIMER0_COMPA_vect) { g_ms++; }

void timebase_init(void) {
    TCCR0A = (1 << WGM01);
    TCCR0B = (1 << CS01) | (1 << CS00); // /64
    OCR0A  = 249;                       // 1ms
    TIMSK0 = (1 << OCIE0A);
}

uint32_t millis(void) {
    uint32_t m;
    uint8_t s = SREG;
    cli();
    m = g_ms;
    SREG = s;
    return m;
}

//ESP response accumulator (drained from UART RX ring buffer)
#define RESP_SZ 512
static char resp[RESP_SZ];
static uint16_t resp_len = 0;

static void resp_reset(void) {
    resp_len = 0;
    resp[0] = '\0';
}

static void resp_append_from_uart(void) {
    while (uart_available()) {
        char c = uart_getc_nb();
        if (resp_len + 1 < RESP_SZ) {
            resp[resp_len++] = c;
            resp[resp_len] = '\0';
        } else {
            // if full, reset to avoid overflow
            resp_reset();
        }
    }
}

static uint8_t resp_has(const char *needle) {
    return (needle && strstr(resp, needle)) ? 1 : 0;
}

static void esp_send_cmd(const char *cmd) {
    uart_puts(cmd);
    uart_puts("\r\n");
}

// nonblocking state machine 
typedef enum {
    E_IDLE=0,
    E_AT, E_ATE0, E_CWMODE, E_CIPMUX, E_CWJAP,
    E_READY,

    E_SEND_CIPSTART,
    E_SEND_CIPSEND,
    E_SEND_WAIT_HTTP,
    E_SEND_CLOSE
} esp_state_t;

static esp_state_t st = E_IDLE;
static uint32_t deadline = 0;

static const char *g_ssid = 0;
static const char *g_pass = 0;

static const char *pending_key = 0;
static int16_t pending_val = -1;
static uint8_t send_pending = 0;

// NEW: uploading flag for LCD line2
static uint8_t g_uploading = 0;

uint8_t esp_is_uploading(void) { return g_uploading; }

void esp_begin(const char *ssid, const char *pass) {
    g_ssid = ssid;
    g_pass = pass;
    resp_reset();
    st = E_AT;
    deadline = millis() + 1500;
    esp_send_cmd("AT");
}

uint8_t esp_ready(void) {
    return (st == E_READY);
}

uint8_t esp_request_send_field1(const char *api_key, int16_t value_cm) {
    if (st != E_READY) return 0;
    if (send_pending) return 0;

    pending_key = api_key;
    pending_val = value_cm;
    send_pending = 1;
    return 1;
}

void esp_task(void) {
    resp_append_from_uart();
    uint32_t now = millis();

    switch (st) {
    case E_IDLE:
        break;

    case E_AT:
        if (resp_has("OK")) {
            resp_reset();
            st = E_ATE0;
            deadline = now + 1500;
            esp_send_cmd("ATE0");
        } else if (now > deadline) {
            resp_reset();
            deadline = now + 1500;
            esp_send_cmd("AT");
        }
        break;

    case E_ATE0:
        if (resp_has("OK")) {
            resp_reset();
            st = E_CWMODE;
            deadline = now + 1500;
            esp_send_cmd("AT+CWMODE=1");
        } else if (now > deadline) {
            resp_reset();
            deadline = now + 1500;
            esp_send_cmd("ATE0");
        }
        break;

    case E_CWMODE:
        if (resp_has("OK")) {
            resp_reset();
            st = E_CIPMUX;
            deadline = now + 1500;
            esp_send_cmd("AT+CIPMUX=0");
        } else if (now > deadline) {
            resp_reset();
            deadline = now + 1500;
            esp_send_cmd("AT+CWMODE=1");
        }
        break;

    case E_CIPMUX:
        if (resp_has("OK")) {
            resp_reset();
            st = E_CWJAP;
            deadline = now + 25000;

            char cmd[180];
            snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", g_ssid, g_pass);
            esp_send_cmd(cmd);
        } else if (now > deadline) {
            resp_reset();
            deadline = now + 1500;
            esp_send_cmd("AT+CIPMUX=0");
        }
        break;

    case E_CWJAP:
        if (resp_has("WIFI CONNECTED") || resp_has("OK") || resp_has("ALREADY CONNECTED")) {
            resp_reset();
            st = E_READY;
        } else if (resp_has("FAIL")) {
            resp_reset();
            deadline = now + 25000;
            char cmd[180];
            snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", g_ssid, g_pass);
            esp_send_cmd(cmd);
        } else if (now > deadline) {
            resp_reset();
            deadline = now + 25000;
            char cmd[180];
            snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", g_ssid, g_pass);
            esp_send_cmd(cmd);
        }
        break;

    case E_READY:
        g_uploading = 0;
        if (send_pending) {
            send_pending = 0;
            g_uploading = 1;

            resp_reset();
            st = E_SEND_CIPSTART;
            deadline = now + 9000;
            esp_send_cmd("AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",80");
        }
        break;

    case E_SEND_CIPSTART:
        if (resp_has("CONNECT") || resp_has("OK") || resp_has("ALREADY CONNECTED")) {
            resp_reset();
            st = E_SEND_CIPSEND;
            deadline = now + 5000;

            char req[260];
            snprintf(req, sizeof(req),
                     "GET /update?api_key=%s&field1=%d HTTP/1.1\r\n"
                     "Host: api.thingspeak.com\r\n"
                     "Connection: close\r\n\r\n",
                     pending_key, (int)pending_val);

            char cmd[40];
            snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", (unsigned)strlen(req));
            esp_send_cmd(cmd);

        } else if (resp_has("ERROR") || now > deadline) {
            resp_reset();
            st = E_SEND_CLOSE;
            deadline = now + 2500;
            esp_send_cmd("AT+CIPCLOSE");
        }
        break;

    case E_SEND_CIPSEND:
        if (resp_has(">")) {
            resp_reset();
            st = E_SEND_WAIT_HTTP;
            deadline = now + 12000;

            char req[260];
            snprintf(req, sizeof(req),
                     "GET /update?api_key=%s&field1=%d HTTP/1.1\r\n"
                     "Host: api.thingspeak.com\r\n"
                     "Connection: close\r\n\r\n",
                     pending_key, (int)pending_val);

            uart_puts(req);

        } else if (now > deadline) {
            resp_reset();
            st = E_SEND_CLOSE;
            deadline = now + 2500;
            esp_send_cmd("AT+CIPCLOSE");
        }
        break;

    case E_SEND_WAIT_HTTP:
        // accept any HTTP header as "uploaded"
        if (resp_has("200 OK") || resp_has("HTTP/1.1") || resp_has("SEND OK")) {
            resp_reset();
            st = E_SEND_CLOSE;
            deadline = now + 2500;
            esp_send_cmd("AT+CIPCLOSE");
        } else if (resp_has("ERROR") || now > deadline) {
            resp_reset();
            st = E_SEND_CLOSE;
            deadline = now + 2500;
            esp_send_cmd("AT+CIPCLOSE");
        }
        break;

    case E_SEND_CLOSE:
        if (resp_has("OK") || now > deadline) {
            resp_reset();
            st = E_READY;
            g_uploading = 0;
        }
        break;

    default:
        st = E_IDLE;
        g_uploading = 0;
        break;
    }
}
