#ifndef LCD_I2C_H
#define LCD_I2C_H

#include <stdint.h>

void lcd_init(void);
void lcd_clear(void);

void lcd_set_cursor(uint8_t col, uint8_t row); // row: 0 or 1
void lcd_print(const char *s);

// Print exactly 16 chars (pads with spaces / truncates)
void lcd_print_16(const char *s);

#endif
