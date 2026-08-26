#include "ost4ml8132a.h"

#define DIN_H_PORT GPIOA
#define DIN_H_PIN  GPIO_PIN_8

#define DIN_L_PORT GPIOA
#define DIN_L_PIN  GPIO_PIN_9

static RGB_t leds[LED_COUNT];

#define DIN_H_HIGH() (DIN_H_PORT->BSRR = DIN_H_PIN)
#define DIN_H_LOW()  (DIN_H_PORT->BRR  = DIN_H_PIN)

#define DIN_L_HIGH() (DIN_L_PORT->BSRR = DIN_L_PIN)
#define DIN_L_LOW()  (DIN_L_PORT->BRR  = DIN_L_PIN)

static void LED_DelayUs(uint32_t microseconds)
{
    /*
     * Der Systemtakt beträgt 32 MHz.
     * Diese Softwareverzögerung ist zunächst für den Funktionstest gedacht.
     */
    while (microseconds--)
    {
        for (volatile uint32_t i = 0; i < 7; i++)
        {
            __NOP();
        }
    }
}

static void LED_Middle(void)
{
    DIN_H_LOW();
    DIN_L_HIGH();
}

static void LED_HighBit(void)
{
    DIN_H_HIGH();
    DIN_L_HIGH();

    LED_DelayUs(1);

    LED_Middle();

    LED_DelayUs(1);
}

static void LED_LowBit(void)
{
    DIN_H_LOW();
    DIN_L_LOW();

    LED_DelayUs(1);

    LED_Middle();

    LED_DelayUs(1);
}

static void LED_SendByte(uint8_t value)
{
    uint8_t mask = 0x80;

    while (mask != 0)
    {
        if ((value & mask) != 0)
        {
            LED_HighBit();
        }
        else
        {
            LED_LowBit();
        }

        mask >>= 1;
    }
}

void LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    /*
     * Ausgangspegel vor der GPIO-Initialisierung setzen:
     * DIN_H = LOW
     * DIN_L = HIGH
     */
    DIN_H_LOW();
    DIN_L_HIGH();

    GPIO_InitStruct.Pin = DIN_H_PIN | DIN_L_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    LED_Middle();

    HAL_Delay(10);
}

void LED_SetColor(uint8_t index,
                  uint8_t r,
                  uint8_t g,
                  uint8_t b)
{
    if (index >= LED_COUNT)
    {
        return;
    }

    leds[index].r = r;
    leds[index].g = g;
    leds[index].b = b;
}

void LED_Show(void)
{
    __disable_irq();

    for (uint8_t i = 0; i < LED_COUNT; i++)
    {
        /*
         * OST4ML8132A erwartet BGR:
         * Blau, Grün, Rot
         */
        LED_SendByte(leds[i].b);
        LED_SendByte(leds[i].g);
        LED_SendByte(leds[i].r);
    }

    LED_Middle();

    __enable_irq();

    /*
     * Set-/Latch-Zeit.
     * Das Arduino-Beispiel verwendet 1 ms.
     */
    HAL_Delay(1);
}
