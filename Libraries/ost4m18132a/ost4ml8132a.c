#include "ost4ml8132a.h"

#define LED_PORT GPIOA
#define LED_PIN  GPIO_PIN_8

static RGB_t leds[LED_COUNT];

/
   Direkter GPIO-Zugriff für PA8.
   Viel schneller als HAL_GPIO_WritePin().
/
#define LED_HIGH()  (GPIOA->BSRR = GPIO_PIN_8)
#define LED_LOW()   (GPIOA->BRR  = GPIO_PIN_8)

static void delay_cycles(volatile uint32_t cycles)
{
    while (cycles--)
    {
        __NOP();
    }
}

void LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin = LED_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

    HAL_GPIO_Init(LED_PORT, &GPIO_InitStruct);

    LED_LOW();

    /
      Latch/Reset-Zeit.
      Datenblatt nennt ca. 3 ms Latch-Zeit.
    /
    HAL_Delay(5);
}

/
   Erster Timing-Versuch, WS2812-ähnlich.
   Bei 32 MHz Systemtakt:
   1 CPU-Zyklus = ca. 31,25 ns.
/
static void LED_SendBit(uint8_t bit)
{
    if (bit)
    {
        /*
          Logische 1:
          High länger, Low kürzer
        /
        LED_HIGH();
        delay_cycles(18);
        LED_LOW();
        delay_cycles(10);
    }
    else
    {
        /
          Logische 0:
          High kürzer, Low länger
        /
        LED_HIGH();
        delay_cycles(7);
        LED_LOW();
        delay_cycles(20);
    }
}

static void LED_SendByte(uint8_t data)
{
    for (int8_t i = 7; i >= 0; i--)
    {
        LED_SendBit((data >> i) & 0x01);
    }
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

    /
      Viele intelligente RGB-LEDs verwenden GRB-Reihenfolge.
      Falls Farben später vertauscht sind, ändern wir hier die Reihenfolge.
    /
    for (uint8_t i = 0; i < LED_COUNT; i++)
    {
        LED_SendByte(leds[i].g);
        LED_SendByte(leds[i].r);
        LED_SendByte(leds[i].b);
    }

    __enable_irq();

    /
      Latch-Zeit laut Datenblatt im ms-Bereich.
    */
    HAL_Delay(5);
}
