#include "ui.h"

#include "st7735.h"
#include "font5x7.h"

void UI_Init(void)
{
    /* später für UI-Zustände */
}

void UI_Draw(void)
{
    ST7735_FillScreen(ST7735_BLACK);

    /* INPUT */
    ST7735_FillRect(
        10, 10, 108, 40,
        ST7735_BLUE);

    ST7735_DrawRect(
        10, 10, 108, 40,
        ST7735_WHITE);

    Font5x7_DrawString(
        20, 22,
        "INPUT",
        ST7735_WHITE,
        ST7735_BLUE,
        2);

    /* OUTPUT */
    ST7735_FillRect(
        10, 90, 108, 40,
        ST7735_RED);

    ST7735_DrawRect(
        10, 90, 108, 40,
        ST7735_WHITE);

    Font5x7_DrawString(
        28, 102,
        "OUTPUT",
        ST7735_WHITE,
        ST7735_RED,
        2);

    /* Verbindung */
    ST7735_DrawArrow(
        64, 50,
        64, 90,
        ST7735_WHITE);
}
