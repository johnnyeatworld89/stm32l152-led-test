#ifndef WS2812B_H
#define WS2812B_H

#include "stm32l1xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * WS2812B status LED on SPI2_MOSI.
 *
 *   PB15 = SPI2_MOSI (AF5), Nucleo-64 morpho connector CN10 pin 26
 *
 * SPI2 runs alone on PB15 (no SCK/MISO pin), so SPI1 with the display
 * and the MCP23S17 is not affected. APB1 = 32 MHz / 8 = 4 MHz, one SPI
 * bit = 250 ns, one WS2812B bit = 4 SPI bits = 1 us:
 *
 *   0 -> 1000  (250 ns high, 750 ns low)
 *   1 -> 1110  (750 ns high, 250 ns low)
 *
 * Every SPI byte ends low, so a short pause between two bytes only
 * lengthens a low phase. The frame is still sent with interrupts
 * disabled (24 us per LED), the latch pause (> 280 us low) follows
 * on its own.
 *
 * Wiring: WS2812B at 5 V needs at least 0.7 x VDD = 3.5 V at DIN, so
 * use a level shifter (e.g. 74AHCT1G125) between PB15 and DIN, plus
 * about 330 ohm in series to DIN and 100 nF at the LED.
 */

#define WS2812B_GPIO_PORT           GPIOB
#define WS2812B_GPIO_PIN            GPIO_PIN_15
#define WS2812B_GPIO_AF             GPIO_AF5_SPI2
#define WS2812B_SPI                 SPI2

void WS2812B_Init(void);

/*
 * Sets the colour of the (single) LED. Must not be called more often
 * than every 300 us, otherwise the LED does not latch.
 */
void WS2812B_SetColor(uint8_t red, uint8_t green, uint8_t blue);

#ifdef __cplusplus
}
#endif

#endif /* WS2812B_H */
