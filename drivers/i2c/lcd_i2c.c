#include "lcd_i2c.h"
#include "twi.h"
#include <util/delay.h>
#include <string.h>

#define LCD_ADDR 0x27

//Common PCF8574 mapping
#define PIN_RS  (1 << 0)
#define PIN_RW  (1 << 1)
#define PIN_EN  (1 << 2)
#define PIN_BL  (1 << 3)
#define PIN_D4  (1 << 4)
#define PIN_D5  (1 << 5)
#define PIN_D6  (1 << 6)
#define PIN_D7  (1 << 7)

static uint8_t backlight = PIN_BL;

static void expander_write(uint8_t data) {
    if (!twi_start((LCD_ADDR << 1) | 0)) {
        twi_stop();
        return;
    }
    (void)twi_write(data | backlight);
    twi_stop();
}

static void pulse_enable(uint8_t data) {
    expander_write(data | PIN_EN);
    _delay_us(1);
    expander_write(data & ~PIN_EN);
    _delay_us(50);
}

static void write4(uint8_t nibble, uint8_t rs) {
    uint8_t data = 0;

    if (rs) data |= PIN_RS; // RS=1 data, RS=0 cmd
    // RW=0 always

    if (nibble & 0x01) data |= PIN_D4;
    if (nibble & 0x02) data |= PIN_D5;
    if (nibble & 0x04) data |= PIN_D6;
    if (nibble & 0x08) data |= PIN_D7;

    expander_write(data);
    pulse_enable(data);
}

static void lcd_send(uint8_t value, uint8_t rs) {
    write4((value >> 4) & 0x0F, rs);
    write4(value & 0x0F, rs);
}

static void lcd_cmd(uint8_t cmd) {
    lcd_send(cmd, 0);
    if (cmd == 0x01 || cmd == 0x02) _delay_ms(2);
}

static void lcd_data(uint8_t d) {
    lcd_send(d, 1);
}

void lcd_init(void) {
    twi_init(100000);
    _delay_ms(50);

    // Init 4-bit mode sequence
    write4(0x03, 0);
    _delay_ms(5);
    write4(0x03, 0);
    _delay_us(150);
    write4(0x03, 0);
    _delay_us(150);

    write4(0x02, 0); // 4-bit
    _delay_us(150);

    lcd_cmd(0x28); // 4-bit, 2 line, 5x8
    lcd_cmd(0x08); // display off
    lcd_cmd(0x01); // clear
    lcd_cmd(0x06); // entry mode
    lcd_cmd(0x0C); // display on, cursor off, blink off
}

void lcd_clear(void) {
    lcd_cmd(0x01);
}

void lcd_set_cursor(uint8_t col, uint8_t row) {
    static const uint8_t row_offsets[] = {0x00, 0x40};
    if (row > 1) row = 1;
    lcd_cmd(0x80 | (row_offsets[row] + col));
}

void lcd_print(const char *s) {
    while (*s) lcd_data((uint8_t)*s++);
}

void lcd_print_16(const char *s) {
    char buf[17];
    for (uint8_t i = 0; i < 16; i++) buf[i] = ' ';
    buf[16] = '\0';

    if (s) {
        for (uint8_t i = 0; i < 16 && s[i]; i++) buf[i] = s[i];
    }
    lcd_print(buf);
}
