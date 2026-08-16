#include "st7735.h"
#include <stdlib.h>


/* ============================================================
 * GPIO
 * ============================================================ */

/* CS */
#define ST7735_CS_PORT     GPIOA
#define ST7735_CS_PIN      GPIO_PIN_4

/* DC */
#define ST7735_DC_PORT     GPIOB
#define ST7735_DC_PIN      GPIO_PIN_0

/* RESET */
#define ST7735_RST_PORT    GPIOB
#define ST7735_RST_PIN     GPIO_PIN_1


/* ============================================================
 * Display dimensions
 * ============================================================ */

uint16_t ST7735_WIDTH  = ST7735_PHYSICAL_WIDTH;
uint16_t ST7735_HEIGHT = ST7735_PHYSICAL_HEIGHT;


/*
 * Offsets for the particular ST7735 module.
 *
 * Your current working configuration used:
 *
 * XSTART = 2
 * YSTART = 1
 */
static uint8_t xStart = 2;
static uint8_t yStart = 1;


/* ============================================================
 * Low-level GPIO
 * ============================================================ */

static void ST7735_Select(void)
{
    HAL_GPIO_WritePin(
        ST7735_CS_PORT,
        ST7735_CS_PIN,
        GPIO_PIN_RESET);
}


static void ST7735_Unselect(void)
{
    HAL_GPIO_WritePin(
        ST7735_CS_PORT,
        ST7735_CS_PIN,
        GPIO_PIN_SET);
}


static void ST7735_DC_Command(void)
{
    HAL_GPIO_WritePin(
        ST7735_DC_PORT,
        ST7735_DC_PIN,
        GPIO_PIN_RESET);
}


static void ST7735_DC_Data(void)
{
    HAL_GPIO_WritePin(
        ST7735_DC_PORT,
        ST7735_DC_PIN,
        GPIO_PIN_SET);
}


/* ============================================================
 * SPI
 * ============================================================ */

static void ST7735_WriteCommand(uint8_t cmd)
{
    ST7735_Select();

    ST7735_DC_Command();

    HAL_SPI_Transmit(
        &hspi1,
        &cmd,
        1,
        HAL_MAX_DELAY);

    ST7735_Unselect();
}


static void ST7735_WriteData(
    uint8_t *data,
    uint16_t size)
{
    ST7735_Select();

    ST7735_DC_Data();

    HAL_SPI_Transmit(
        &hspi1,
        data,
        size,
        HAL_MAX_DELAY);

    ST7735_Unselect();
}


/* ============================================================
 * Reset
 * ============================================================ */

static void ST7735_Reset(void)
{
    HAL_GPIO_WritePin(
        ST7735_RST_PORT,
        ST7735_RST_PIN,
        GPIO_PIN_RESET);

    HAL_Delay(20);

    HAL_GPIO_WritePin(
        ST7735_RST_PORT,
        ST7735_RST_PIN,
        GPIO_PIN_SET);

    HAL_Delay(150);
}


/* ============================================================
 * Initialization
 * ============================================================ */

void ST7735_Init(void)
{
    ST7735_Reset();

    /* Software reset */
    ST7735_WriteCommand(0x01);
    HAL_Delay(150);

    /* Sleep out */
    ST7735_WriteCommand(0x11);
    HAL_Delay(150);

    /*
     * Default orientation.
     *
     * Rotation 0:
     * 128 x 160
     */
    ST7735_SetRotation(1);

    /*
     * 16-bit RGB565
     */
    ST7735_WriteCommand(0x3A);

    uint8_t colorMode = 0x05;

    ST7735_WriteData(
        &colorMode,
        1);

    /* Display ON */
    ST7735_WriteCommand(0x29);

    HAL_Delay(50);
}


/* ============================================================
 * Rotation
 * ============================================================ */

void ST7735_SetRotation(uint8_t rotation)
{
    rotation &= 0x03;

    uint8_t madctl;

    switch (rotation)
    {
        /*
         * Portrait
         *
         * 128 x 160
         */
        case 0:

            madctl = 0xC0;

            ST7735_WIDTH  = 128;
            ST7735_HEIGHT = 160;

            xStart = 2;
            yStart = 1;

            break;


        /*
         * Landscape
         *
         * 160 x 128
         */
        case 1:

            madctl = 0xA0;

            ST7735_WIDTH  = 160;
            ST7735_HEIGHT = 128;

            xStart = 1;
            yStart = 2;

            break;


        /*
         * Portrait 180°
         *
         * 128 x 160
         */
        case 2:

            madctl = 0x00;

            ST7735_WIDTH  = 128;
            ST7735_HEIGHT = 160;

            xStart = 2;
            yStart = 1;

            break;


        /*
         * Landscape 180°
         *
         * 160 x 128
         */
        case 3:

            madctl = 0x60;

            ST7735_WIDTH  = 160;
            ST7735_HEIGHT = 128;

            xStart = 1;
            yStart = 2;

            break;
    }


    /*
     * MADCTL
     */
    ST7735_WriteCommand(0x36);

    ST7735_WriteData(
        &madctl,
        1);
}


/* ============================================================
 * Address Window
 * ============================================================ */

static void ST7735_SetAddressWindow(
    uint16_t x0,
    uint16_t y0,
    uint16_t x1,
    uint16_t y1)
{
    uint8_t data[4];

    /*
     * Apply module-specific offsets.
     */
    x0 += xStart;
    x1 += xStart;

    y0 += yStart;
    y1 += yStart;


    /*
     * Column Address Set
     */
    ST7735_WriteCommand(0x2A);

    data[0] = (x0 >> 8) & 0xFF;
    data[1] = x0 & 0xFF;
    data[2] = (x1 >> 8) & 0xFF;
    data[3] = x1 & 0xFF;

    ST7735_WriteData(
        data,
        4);


    /*
     * Row Address Set
     */
    ST7735_WriteCommand(0x2B);

    data[0] = (y0 >> 8) & 0xFF;
    data[1] = y0 & 0xFF;
    data[2] = (y1 >> 8) & 0xFF;
    data[3] = y1 & 0xFF;

    ST7735_WriteData(
        data,
        4);


    /*
     * Memory Write
     */
    ST7735_WriteCommand(0x2C);
}


/* ============================================================
 * Fill Screen
 * ============================================================ */

void ST7735_FillScreen(uint16_t color)
{
    ST7735_FillRect(
        0,
        0,
        ST7735_WIDTH,
        ST7735_HEIGHT,
        color);
}


/* ============================================================
 * Fill Rectangle
 * ============================================================ */

void ST7735_FillRect(
    uint16_t x,
    uint16_t y,
    uint16_t w,
    uint16_t h,
    uint16_t color)
{
    if (w == 0 || h == 0)
        return;


    /*
     * Clipping
     */

    if (x >= ST7735_WIDTH ||
        y >= ST7735_HEIGHT)
    {
        return;
    }


    if ((x + w) > ST7735_WIDTH)
    {
        w = ST7735_WIDTH - x;
    }


    if ((y + h) > ST7735_HEIGHT)
    {
        h = ST7735_HEIGHT - y;
    }


    /*
     * Set drawing area.
     */
    ST7735_SetAddressWindow(
        x,
        y,
        x + w - 1,
        y + h - 1);


    /*
     * Prepare one complete line of pixels.
     *
     * Maximum width is 160 pixels.
     *
     * 160 pixels × 2 bytes = 320 bytes.
     */
    uint8_t lineBuffer[160 * 2];


    uint8_t high = color >> 8;
    uint8_t low  = color & 0xFF;


    for (uint16_t i = 0; i < w; i++)
    {
        lineBuffer[i * 2]     = high;
        lineBuffer[i * 2 + 1] = low;
    }


    /*
     * Send every line as one SPI transfer.
     */
    ST7735_Select();

    ST7735_DC_Data();

    for (uint16_t row = 0; row < h; row++)
    {
        HAL_SPI_Transmit(
            &hspi1,
            lineBuffer,
            w * 2,
            HAL_MAX_DELAY);
    }

    ST7735_Unselect();
}


/* ============================================================
 * Draw Pixel
 * ============================================================ */

void ST7735_DrawPixel(
    uint16_t x,
    uint16_t y,
    uint16_t color)
{
    if (x >= ST7735_WIDTH ||
        y >= ST7735_HEIGHT)
    {
        return;
    }


    ST7735_SetAddressWindow(
        x,
        y,
        x,
        y);


    uint8_t pixel[2];

    pixel[0] = color >> 8;
    pixel[1] = color & 0xFF;


    ST7735_Select();

    ST7735_DC_Data();

    HAL_SPI_Transmit(
        &hspi1,
        pixel,
        2,
        HAL_MAX_DELAY);

    ST7735_Unselect();
}


/* ============================================================
 * Draw Line
 * ============================================================ */

void ST7735_DrawLine(
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    uint16_t color)
{
    int16_t dx = abs(x1 - x0);
    int16_t sx = x0 < x1 ? 1 : -1;

    int16_t dy = -abs(y1 - y0);
    int16_t sy = y0 < y1 ? 1 : -1;

    int16_t err = dx + dy;


    while (1)
    {
        ST7735_DrawPixel(
            x0,
            y0,
            color);


        if (x0 == x1 &&
            y0 == y1)
        {
            break;
        }


        int16_t e2 = 2 * err;


        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }


        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}


/* ============================================================
 * Draw Rectangle
 * ============================================================ */

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


/* ============================================================
 * Draw Arrow
 * ============================================================ */

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


    int16_t dx = x1 - x0;
    int16_t dy = y1 - y0;


    if (dx == 0 && dy == 0)
        return;


    const int16_t arrowSize = 4;


    /*
     * Horizontal arrow
     */
    if (abs(dx) >= abs(dy))
    {
        if (dx > 0)
        {
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


    /*
     * Vertical arrow
     */
    else
    {
        if (dy > 0)
        {
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
