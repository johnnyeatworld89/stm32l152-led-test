#ifndef ST7735_H
#define ST7735_H

#include "stm32l1xx_hal.h"

/*
 * Physische Displaygröße
 *
 * Das Display besitzt 128 x 160 Pixel.
 */
#define ST7735_PHYSICAL_WIDTH   128
#define ST7735_PHYSICAL_HEIGHT  160


/*
 * Aktuelle logische Displaygröße.
 *
 * Diese Werte werden durch ST7735_SetRotation()
 * automatisch angepasst.
 */
extern uint16_t ST7735_WIDTH;
extern uint16_t ST7735_HEIGHT;


/* RGB565 Farben */
#define ST7735_BLACK   0x0000
#define ST7735_BLUE    0x001F
#define ST7735_RED     0xF800
#define ST7735_GREEN   0x07E0
#define ST7735_WHITE   0xFFFF
#define ST7735_CYAN    0x07FF
#define ST7735_MAGENTA 0xF81F
#define ST7735_YELLOW  0xFFE0


/* SPI Handle aus main.c */
extern SPI_HandleTypeDef hspi1;


/* Initialisierung */
void ST7735_Init(void);


/* Rotation */
void ST7735_SetRotation(uint8_t rotation);


/* Grundlegende Grafikfunktionen */
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
