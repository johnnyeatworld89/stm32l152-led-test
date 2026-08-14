#ifndef ST7735_H
#define ST7735_H

#include "stm32l1xx_hal.h"

// Displaygröße
#define ST7735_WIDTH   128
#define ST7735_HEIGHT  160

#define ST7735_XSTART 2
#define ST7735_YSTART 1

// Farben RGB565
#define ST7735_BLACK   0x0000
#define ST7735_BLUE    0x001F
#define ST7735_RED     0xF800
#define ST7735_GREEN   0x07E0
#define ST7735_WHITE   0xFFFF

// SPI Handle kommt aus main.c
extern SPI_HandleTypeDef hspi1;

// API
void ST7735_Init(void);
void ST7735_FillScreen(uint16_t color);

void ST7735_DrawPixel(
uint16_t x,
uint16_t y,
uint16_t color);

void ST7735_DrawLine(
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    uint16_t color);

void ST7735_DrawRect(
    uint16_t x,
    uint16_t y,
    uint16_t w,
    uint16_t h,
    uint16_t color);

void ST7735_FillRect(
    uint16_t x,
    uint16_t y,
    uint16_t w,
    uint16_t h,
    uint16_t color);

void ST7735_DrawArrow(
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    uint16_t color);
#endif
