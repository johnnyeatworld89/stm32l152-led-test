#include "st7735.h"

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

    ST7735_WriteCommand(0x2A);

    data[0] = x0 >> 8;
    data[1] = x0;
    data[2] = x1 >> 8;
    data[3] = x1;

    ST7735_WriteData(data, 4);

    ST7735_WriteCommand(0x2B);

    data[0] = y0 >> 8;
    data[1] = y0;
    data[2] = y1 >> 8;
    data[3] = y1;

    ST7735_WriteData(data, 4);

    ST7735_WriteCommand(0x2C);
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
