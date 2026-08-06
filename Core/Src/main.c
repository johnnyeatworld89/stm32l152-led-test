#include "main.h"
#include "st7735.h"

extern SPI_HandleTypeDef hspi1;

int main(void)
{
    HAL_Init();

    SystemClock_Config();

    MX_GPIO_Init();
    MX_SPI1_Init();

    ST7735_Init();

    while (1)
    {
        ST7735_FillScreen(ST7735_RED);
        HAL_Delay(1000);

        ST7735_FillScreen(ST7735_GREEN);
        HAL_Delay(1000);

        ST7735_FillScreen(ST7735_BLUE);
        HAL_Delay(1000);

        ST7735_FillScreen(ST7735_WHITE);
        HAL_Delay(1000);

        ST7735_FillScreen(ST7735_BLACK);
        HAL_Delay(1000);
    }
}
