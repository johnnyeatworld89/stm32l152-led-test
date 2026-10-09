#ifndef MIDI_H
#define MIDI_H

#include "stm32l1xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * MIDI 1.0
 * UART: 31250 baud, 8 data bits, no parity, 1 stop bit
 *
 * Choose the UART with MIDI_UART_PORT (1 or 2), for example in the
 * project settings with -DMIDI_UART_PORT=2:
 *
 *   MIDI_UART_PORT 1 (default)       MIDI_UART_PORT 2
 *   USART1_TX = PA9  (Nucleo D8)     USART2_TX = PA2  (Nucleo D1)
 *   USART1_RX = PA10 (Nucleo D2)     USART2_RX = PA3  (Nucleo D0)
 *
 * On a Nucleo-64 board PA2/PA3 are connected to the ST-LINK by default
 * (solder bridges SB13/SB14), PA9/PA10 are free. With USART1 no solder
 * bridge has to be changed.
 *
 * MIDI IN:
 *   H11L1 output -> RX pin (PA10 or PA3)
 *
 * MIDI OUT:
 *   TX pin (PA9 or PA2) -> MIDI OUT driver
 */

#ifndef MIDI_UART_PORT
#define MIDI_UART_PORT 1
#endif

#if MIDI_UART_PORT == 1

#define MIDI_UART_INSTANCE          USART1
#define MIDI_UART_IRQn              USART1_IRQn
#define MIDI_UART_IRQ_HANDLER       USART1_IRQHandler
#define MIDI_UART_CLK_ENABLE()      __HAL_RCC_USART1_CLK_ENABLE()
#define MIDI_UART_CLK_DISABLE()     __HAL_RCC_USART1_CLK_DISABLE()
#define MIDI_UART_GPIO_PORT         GPIOA
#define MIDI_UART_GPIO_PINS         (GPIO_PIN_9 | GPIO_PIN_10)
#define MIDI_UART_GPIO_AF           GPIO_AF7_USART1

#elif MIDI_UART_PORT == 2

#define MIDI_UART_INSTANCE          USART2
#define MIDI_UART_IRQn              USART2_IRQn
#define MIDI_UART_IRQ_HANDLER       USART2_IRQHandler
#define MIDI_UART_CLK_ENABLE()      __HAL_RCC_USART2_CLK_ENABLE()
#define MIDI_UART_CLK_DISABLE()     __HAL_RCC_USART2_CLK_DISABLE()
#define MIDI_UART_GPIO_PORT         GPIOA
#define MIDI_UART_GPIO_PINS         (GPIO_PIN_2 | GPIO_PIN_3)
#define MIDI_UART_GPIO_AF           GPIO_AF7_USART2

#else
#error "MIDI_UART_PORT must be 1 (USART1, PA9/PA10) or 2 (USART2, PA2/PA3)"
#endif

/* Initialize the MIDI UART (see MIDI_UART_PORT). */
HAL_StatusTypeDef MIDI_Init(void);

/* Start interrupt-driven MIDI reception. */
HAL_StatusTypeDef MIDI_StartReceive(void);

/* Send one raw MIDI byte. */
HAL_StatusTypeDef MIDI_SendByte(uint8_t byte);

/* Send a complete MIDI message. */
HAL_StatusTypeDef MIDI_SendMessage(const uint8_t *data, uint16_t length);

/* Convenience functions for common MIDI Channel Voice messages. */
HAL_StatusTypeDef MIDI_NoteOn(uint8_t channel, uint8_t note, uint8_t velocity);
HAL_StatusTypeDef MIDI_NoteOff(uint8_t channel, uint8_t note, uint8_t velocity);
HAL_StatusTypeDef MIDI_ControlChange(uint8_t channel, uint8_t controller, uint8_t value);
HAL_StatusTypeDef MIDI_ProgramChange(uint8_t channel, uint8_t program);
HAL_StatusTypeDef MIDI_PitchBend(uint8_t channel, int16_t value);

/*
 * Called automatically by HAL_UART_RxCpltCallback().
 * The function is kept public so it can also be called from
 * an existing application callback if required.
 */
void MIDI_UART_RxCpltCallback(UART_HandleTypeDef *huart);

/*
 * Called automatically by HAL_UART_ErrorCallback().
 */
void MIDI_UART_ErrorCallback(UART_HandleTypeDef *huart);

/*
 * User callback.
 *
 * Override this weak function in main.c if you want to react to
 * received MIDI bytes/messages.
 *
 * A complete MIDI message is passed when it has been assembled.
 */
void MIDI_MessageReceived(const uint8_t *data, uint8_t length);

extern UART_HandleTypeDef hMIDI_UART;

#ifdef __cplusplus
}
#endif

#endif /* MIDI_H */
