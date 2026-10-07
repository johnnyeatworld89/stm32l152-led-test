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
 * STM32L152RET6 Nucleo:
 *   USART2_TX = PA2
 *   USART2_RX = PA3
 *
 * MIDI IN:
 *   H11L1 output -> PA3 / USART2_RX
 *
 * MIDI OUT:
 *   PA2 / USART2_TX -> MIDI OUT driver
 */

/* Initialize USART2 for MIDI. */
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
