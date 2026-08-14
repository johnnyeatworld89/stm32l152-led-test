#include "st7735.h"
#include <stdlib.h>

// CS
#define ST7735_CS_PORT     GPIOA
#define ST7735_CS_PIN      GPIO_PIN_4

// DC
#define ST7735_DC_PORT     GPIOB
#define ST7735_DC_PIN      GPIO_PIN_0

// RESET
#define ST7735_RST_PORT    GPIOB
#define ST7735_RST_PIN     GPIO_PIN_1
static void ST7735_Select(void)
{
    HAL_GPIO_WritePin(ST7735_CS_PORT,
                      ST7735_CS_PIN,
                      GPIO_PIN_RESET);
}

static void ST7735_Unselect(void)
{
    HAL_GPIO_WritePin(ST7735_CS_PORT,
                      ST7735_CS_PIN,
                      GPIO_PIN_SET);
}

static void ST7735_DC_Command(void)
{
    HAL_GPIO_WritePin(ST7735_DC_PORT,
                      ST7735_DC_PIN,
                      GPIO_PIN_RESET);
}

static void ST7735_DC_Data(void)
{
    HAL_GPIO_WritePin(ST7735_DC_PORT,
                      ST7735_DC_PIN,
                      GPIO_PIN_SET);
}
static void ST7735_WriteCommand(uint8_t cmd)
{
    ST7735_Select();

    ST7735_DC_Command();

    HAL_SPI_Transmit(&hspi1,
                     &cmd,
                     1,
                     HAL_MAX_DELAY);

    ST7735_Unselect();
}
static void ST7735_WriteData(uint8_t *data,
                             uint16_t size)
{
    ST7735_Select();

    ST7735_DC_Data();

    HAL_SPI_Transmit(&hspi1,
                     data,
                     size,
                     HAL_MAX_DELAY);

    ST7735_Unselect();
}
static void ST7735_Reset(void)
{
    HAL_GPIO_WritePin(ST7735_RST_PORT,
                      ST7735_RST_PIN,
                      GPIO_PIN_RESET);

    HAL_Delay(20);

    HAL_GPIO_WritePin(ST7735_RST_PORT,
                      ST7735_RST_PIN,
                      GPIO_PIN_SET);

    HAL_Delay(150);
}
void ST7735_Init(void)
{
    ST7735_Reset();

    ST7735_WriteCommand(0x01);
    HAL_Delay(150);

    ST7735_WriteCommand(0x11);
    HAL_Delay(150);

    ST7735_WriteCommand(0x3A);
    uint8_t colorMode = 0x05;
    ST7735_WriteData(&colorMode, 1);

    ST7735_WriteCommand(0x29);
    HAL_Delay(50);
}
static void ST7735_SetAddressWindow(
    uint16_t x0,
    uint16_t y0,
    uint16_t x1,
    uint16_t y1)
{
    uint8_t data[4];

    x0 += ST7735_XSTART;
    x1 += ST7735_XSTART;

    y0 += ST7735_YSTART;
    y1 += ST7735_YSTART;

    ST7735_WriteCommand(0x2A);     // CASET

    data[0] = (x0 >> 8) & 0xFF;
    data[1] = x0 & 0xFF;
    data[2] = (x1 >> 8) & 0xFF;
    data[3] = x1 & 0xFF;

    ST7735_WriteData(data, 4);

    ST7735_WriteCommand(0x2B);     // RASET

    data[0] = (y0 >> 8) & 0xFF;
    data[1] = y0 & 0xFF;
    data[2] = (y1 >> 8) & 0xFF;
    data[3] = y1 & 0xFF;

    ST7735_WriteData(data, 4);

    ST7735_WriteCommand(0x2C);     // RAMWR
}
void ST7735_FillScreen(uint16_t color)
{
    ST7735_SetAddressWindow(
        0,
        0,
        ST7735_WIDTH - 1,
        ST7735_HEIGHT - 1);

    uint8_t pixel[2];

    pixel[0] = color >> 8;
    pixel[1] = color & 0xFF;

    ST7735_Select();
    ST7735_DC_Data();

    for(uint32_t i = 0;
        i < (ST7735_WIDTH * ST7735_HEIGHT);
        i++)
    {
        HAL_SPI_Transmit(&hspi1,
                         pixel,
                         2,
                         HAL_MAX_DELAY);
    }

    ST7735_Unselect();
}
void ST7735_DrawPixel(
    uint16_t x,
    uint16_t y,
    uint16_t color)
{
    if (x >= ST7735_WIDTH || y >= ST7735_HEIGHT)
    {
        return;
    }

    uint8_t pixel[2];

    pixel[0] = color >> 8;
    pixel[1] = color & 0xFF;

    ST7735_SetAddressWindow(x, y, x, y);

    ST7735_Select();
    ST7735_DC_Data();

    HAL_SPI_Transmit(&hspi1,
                     pixel,
                     2,
                     HAL_MAX_DELAY);

    ST7735_Unselect();
}

void ST7735_DrawLine(
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    uint16_t color)
{
    int16_t dx = x1 - x0;
    int16_t dy = y1 - y0;

    int16_t sx = (dx >= 0) ? 1 : -1;
    int16_t sy = (dy >= 0) ? 1 : -1;

    dx = (dx >= 0) ? dx : -dx;
    dy = (dy >= 0) ? dy : -dy;

    int16_t err = dx - dy;

    while (1)
    {
        ST7735_DrawPixel(x0, y0, color);

        if (x0 == x1 && y0 == y1)
            break;

        int16_t e2 = 2 * err;

        if (e2 > -dy)
        {
            err -= dy;
            x0 += sx;
        }

        if (e2 < dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}


void ST7735_DrawRect(
    uint16_t x,
    uint16_t y,
    uint16_t w,
    uint16_t h,
    uint16_t color)
{
    if (w == 0 || h == 0)
        return;

    ST7735_DrawLine(
        x,
        y,
        x + w - 1,
        y,
        color);

    ST7735_DrawLine(
        x,
        y + h - 1,
        x + w - 1,
        y + h - 1,
        color);

    ST7735_DrawLine(
        x,
        y,
        x,
        y + h - 1,
        color);

    ST7735_DrawLine(
        x + w - 1,
        y,
        x + w - 1,
        y + h - 1,
        color);
}


void ST7735_FillRect(
    uint16_t x,
    uint16_t y,
    uint16_t w,
    uint16_t h,
    uint16_t color)
{
    if (w == 0 || h == 0)
        return;

    for (uint16_t yy = y; yy < y + h; yy++)
    {
        for (uint16_t xx = x; xx < x + w; xx++)
        {
            ST7735_DrawPixel(xx, yy, color);
        }
    }
}


void ST7735_DrawArrow(
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    uint16_t color)
{
    ST7735_DrawLine(
        x0,
        y0,
        x1,
        y1,
        color);

    /*
     * Einfacher Pfeilkopf.
     *
     * Die Richtung wird anhand der dominanten
     * X/Y-Richtung bestimmt.
     */

    int16_t dx = x1 - x0;
    int16_t dy = y1 - y0;

    if (dx == 0 && dy == 0)
        return;

    const int16_t arrowSize = 4;

    if (abs(dx) >= abs(dy))
    {
        if (dx > 0)
        {
            /* Pfeil nach rechts */

            ST7735_DrawLine(
                x1,
                y1,
                x1 - arrowSize,
                y1 - arrowSize,
                color);

            ST7735_DrawLine(
                x1,
                y1,
                x1 - arrowSize,
                y1 + arrowSize,
                color);
        }
        else
        {
            /* Pfeil nach links */

            ST7735_DrawLine(
                x1,
                y1,
                x1 + arrowSize,
                y1 - arrowSize,
                color);

            ST7735_DrawLine(
                x1,
                y1,
                x1 + arrowSize,
                y1 + arrowSize,
                color);
        }
    }
    else
    {
        if (dy > 0)
        {
            /* Pfeil nach unten */

            ST7735_DrawLine(
                x1,
                y1,
                x1 - arrowSize,
                y1 - arrowSize,
                color);

            ST7735_DrawLine(
                x1,
                y1,
                x1 + arrowSize,
                y1 - arrowSize,
                color);
        }
        else
        {
            /* Pfeil nach oben */

            ST7735_DrawLine(
                x1,
                y1,
                x1 - arrowSize,
                y1 + arrowSize,
                color);

            ST7735_DrawLine(
                x1,
                y1,
                x1 + arrowSize,
                y1 + arrowSize,
                color);
        }
    }
}
