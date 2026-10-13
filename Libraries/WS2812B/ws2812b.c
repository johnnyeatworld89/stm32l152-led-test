#include "ws2812b.h"

/* SPI bits for two WS2812B bits (one nibble each), index = 2 data bits. */
static const uint8_t ws2812bPattern[4] =
{
    0x88U,  /* 00 */
    0x8EU,  /* 01 */
    0xE8U,  /* 10 */
    0xEEU   /* 11 */
};

void WS2812B_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_SPI2_CLK_ENABLE();

    /*
     * Pull-down keeps DIN low while the SPI is not yet driving
     * the pin.
     */
    GPIO_InitStruct.Pin = WS2812B_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = WS2812B_GPIO_AF;
    HAL_GPIO_Init(WS2812B_GPIO_PORT, &GPIO_InitStruct);

    /*
     * Master, software NSS, CPOL = 0, CPHA = 0, MSB first, 8 bit,
     * baud rate PCLK1 / 8. Registers are used directly, the HAL SPI
     * handle of SPI1 is not involved.
     */
    WS2812B_SPI->CR1 = 0U;
    WS2812B_SPI->CR2 = 0U;
    WS2812B_SPI->CR1 =
        SPI_CR1_MSTR |
        SPI_CR1_SSM |
        SPI_CR1_SSI |
        SPI_CR1_BR_1;
    WS2812B_SPI->CR1 |= SPI_CR1_SPE;
}

void WS2812B_SetColor(uint8_t red, uint8_t green, uint8_t blue)
{
    /* The WS2812B expects green, red, blue, MSB first. */
    const uint8_t colour[3] = { green, red, blue };
    uint8_t buffer[12];
    uint8_t n = 0U;

    for (uint8_t i = 0U; i < 3U; i++)
    {
        for (int8_t shift = 6; shift >= 0; shift -= 2)
        {
            buffer[n++] = ws2812bPattern[(colour[i] >> shift) & 0x03U];
        }
    }

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    for (uint8_t i = 0U; i < n; i++)
    {
        while ((WS2812B_SPI->SR & SPI_SR_TXE) == 0U)
        {
        }

        *(volatile uint8_t *)&WS2812B_SPI->DR = buffer[i];
    }

    while ((WS2812B_SPI->SR & SPI_SR_TXE) == 0U)
    {
    }

    while ((WS2812B_SPI->SR & SPI_SR_BSY) != 0U)
    {
    }

    __set_PRIMASK(primask);

    /* Nothing is received; clear RXNE / OVR. */
    (void)WS2812B_SPI->DR;
    (void)WS2812B_SPI->SR;
}
