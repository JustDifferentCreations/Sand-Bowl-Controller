#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>

#define SSD1306_ADDR       0x3C
#define SSD1306_WIDTH      128
#define SSD1306_HEIGHT     32

void SSD1306_Init(void);
void SSD1306_Clear(void);
void SSD1306_Display(void);
void SSD1306_SetCursor(uint8_t x, uint8_t y);
void SSD1306_WriteChar(uint8_t x, uint8_t y, char c);
void SSD1306_WriteString(uint8_t x, uint8_t y, const char *str);

#endif
