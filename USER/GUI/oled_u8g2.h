#ifndef __OLED_U8G2_H
#define __OLED_U8G2_H

/*
 * U8g2 display adapter.
 * This layer owns OLED init, buffer transfer, and simple power control.
 */

#include "i2c_bus.h"
#include "main.h"
#include "u8g2.h"

#define OLED_I2C_ADDR 0x3C
#define OLED_WIDTH 128
#define OLED_HEIGHT 32

void OLED_Init(I2C_Bus_t *bus);
void OLED_Clear(void);
void OLED_Update(void);
void OLED_DrawStr(uint8_t x, uint8_t y, const char *str);
void OLED_Sleep(void);
void OLED_WakeUp(void);
u8g2_t *OLED_GetHandle(void);

#endif
