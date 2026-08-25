#include "ost4ml8132a.h"

#define LED_H_PORT GPIOA
#define LED_H_PIN  GPIO_PIN_8

#define LED_L_PORT GPIOA
#define LED_L_PIN  GPIO_PIN_9

static RGB_t leds[LED_COUNT];

static void delay_us_soft(volatile uint32_t us)
{
    while (us--)
    {
        for (volatile uint32_t i = 0; i < 32; i++)
        {
            __NOP();
        }
    }
}

static void DIN_Middle(void)
{
    HAL_GPIO_WritePin(LED_H_PORT, LED_H_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_L_PORT, LED_L_PIN, GPIO_PIN_SET);
}

static void SendHighBit(void)
{
    HAL_GPIO_WritePin(LED_H_PORT, LED_H_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_L_PORT, LED_L_PIN, GPIO_PIN_SET);

    delay_us_soft(50);

    DIN_Middle();

    delay_us_soft(50);
}

static void SendLowBit(void)
{
    HAL_GPIO_WritePin(LED_H_PORT, LED_H_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_L_PORT, LED_L_PIN, GPIO_PIN_RESET);

    delay_us_soft(50);

    DIN_Middle();

    delay_us_soft(50);
}

static void LED_SendByte(uint8_t data)
{
    for (int8_t i = 7; i >= 0; i--)
    {
        if ((data >> i) & 0x01)
        {
            SendHighBit();
        }
        else
        {
            SendLowBit();
        }
    }
}

void LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin = LED_H_PIN | LED_L_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    DIN_Middle();

    HAL_Delay(5);
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
    for (uint8_t i = 0; i < LED_COUNT; i++)
    {
        LED_SendByte(leds[i].b);
        LED_SendByte(leds[i].g);
        LED_SendByte(leds[i].r);
    }

    HAL_Delay(5);
}
