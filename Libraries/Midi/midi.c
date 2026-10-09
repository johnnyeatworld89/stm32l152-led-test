#include "midi.h"

/*
 * STM32L152RET6:
 * The UART and its pins are selected with MIDI_UART_PORT in midi.h
 * (USART1: PA9/PA10, USART2: PA2/PA3).
 */

UART_HandleTypeDef hMIDI_UART;

static uint8_t midi_rx_byte;

/* Running-status parser state */
static uint8_t midi_running_status = 0;
static uint8_t midi_message[3];
static uint8_t midi_message_length = 0;
static uint8_t midi_message_expected = 0;

/* Forward declarations */
static uint8_t MIDI_MessageLength(uint8_t status);
static void MIDI_ProcessByte(uint8_t byte);
static void MIDI_ResetParser(void);

HAL_StatusTypeDef MIDI_Init(void)
{
    hMIDI_UART.Instance = MIDI_UART_INSTANCE;
    hMIDI_UART.Init.BaudRate = 31250;
    hMIDI_UART.Init.WordLength = UART_WORDLENGTH_8B;
    hMIDI_UART.Init.StopBits = UART_STOPBITS_1;
    hMIDI_UART.Init.Parity = UART_PARITY_NONE;
    hMIDI_UART.Init.Mode = UART_MODE_TX_RX;
    hMIDI_UART.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    hMIDI_UART.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&hMIDI_UART) != HAL_OK)
    {
        return HAL_ERROR;
    }

    MIDI_ResetParser();

    return HAL_OK;
}

HAL_StatusTypeDef MIDI_StartReceive(void)
{
    return HAL_UART_Receive_IT(&hMIDI_UART, &midi_rx_byte, 1);
}

HAL_StatusTypeDef MIDI_SendByte(uint8_t byte)
{
    return HAL_UART_Transmit(&hMIDI_UART, &byte, 1, HAL_MAX_DELAY);
}

HAL_StatusTypeDef MIDI_SendMessage(const uint8_t *data, uint16_t length)
{
    if (data == NULL || length == 0)
    {
        return HAL_ERROR;
    }

    return HAL_UART_Transmit(&hMIDI_UART,
                             (uint8_t *)data,
                             length,
                             HAL_MAX_DELAY);
}

HAL_StatusTypeDef MIDI_NoteOn(uint8_t channel,
                              uint8_t note,
                              uint8_t velocity)
{
    uint8_t message[3];

    channel &= 0x0F;
    note &= 0x7F;
    velocity &= 0x7F;

    message[0] = 0x90 | channel;
    message[1] = note;
    message[2] = velocity;

    return MIDI_SendMessage(message, 3);
}

HAL_StatusTypeDef MIDI_NoteOff(uint8_t channel,
                               uint8_t note,
                               uint8_t velocity)
{
    uint8_t message[3];

    channel &= 0x0F;
    note &= 0x7F;
    velocity &= 0x7F;

    message[0] = 0x80 | channel;
    message[1] = note;
    message[2] = velocity;

    return MIDI_SendMessage(message, 3);
}

HAL_StatusTypeDef MIDI_ControlChange(uint8_t channel,
                                     uint8_t controller,
                                     uint8_t value)
{
    uint8_t message[3];

    channel &= 0x0F;
    controller &= 0x7F;
    value &= 0x7F;

    message[0] = 0xB0 | channel;
    message[1] = controller;
    message[2] = value;

    return MIDI_SendMessage(message, 3);
}

HAL_StatusTypeDef MIDI_ProgramChange(uint8_t channel,
                                     uint8_t program)
{
    uint8_t message[2];

    channel &= 0x0F;
    program &= 0x7F;

    message[0] = 0xC0 | channel;
    message[1] = program;

    return MIDI_SendMessage(message, 2);
}

HAL_StatusTypeDef MIDI_PitchBend(uint8_t channel,
                                 int16_t value)
{
    uint8_t message[3];
    uint16_t bend;

    /*
     * MIDI Pitch Bend is a 14-bit unsigned value:
     * 0x0000 = minimum
     * 0x2000 = center
     * 0x3FFF = maximum
     *
     * The public API accepts -8192 ... +8191.
     */
    if (value < -8192)
    {
        value = -8192;
    }

    if (value > 8191)
    {
        value = 8191;
    }

    bend = (uint16_t)(value + 8192);

    channel &= 0x0F;

    message[0] = 0xE0 | channel;
    message[1] = bend & 0x7F;
    message[2] = (bend >> 7) & 0x7F;

    return MIDI_SendMessage(message, 3);
}

void MIDI_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != MIDI_UART_INSTANCE)
    {
        return;
    }

    MIDI_ProcessByte(midi_rx_byte);

    /*
     * Re-arm reception for the next MIDI byte.
     */
    HAL_UART_Receive_IT(&hMIDI_UART, &midi_rx_byte, 1);
}

void MIDI_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != MIDI_UART_INSTANCE)
    {
        return;
    }

    /*
     * Restart reception after a UART error.
     */
    HAL_UART_AbortReceive(huart);
    HAL_UART_Receive_IT(&hMIDI_UART, &midi_rx_byte, 1);
}

__weak void MIDI_MessageReceived(const uint8_t *data, uint8_t length)
{
    /*
     * User-defined callback.
     *
     * Example:
     *
     * void MIDI_MessageReceived(const uint8_t *data, uint8_t length)
     * {
     *     if ((data[0] & 0xF0) == 0x90 && data[2] != 0)
     *     {
     *         // Note On
     *     }
     * }
     */
    (void)data;
    (void)length;
}

static void MIDI_ProcessByte(uint8_t byte)
{
    uint8_t status;
    uint8_t expected;

    /*
     * Real-time MIDI messages (0xF8-0xFF) may occur at any
     * point in the stream and must not disturb running status.
     *
     * For now they are ignored by the message parser.
     */
    if (byte >= 0xF8)
    {
        return;
    }

    /*
     * System Common messages.
     * This simple implementation ignores them rather than passing
     * them through the Channel Voice parser.
     */
    if (byte >= 0xF0)
    {
        MIDI_ResetParser();
        return;
    }

    /*
     * Status byte.
     */
    if (byte & 0x80)
    {
        status = byte;
        expected = MIDI_MessageLength(status);

        if (expected == 0)
        {
            MIDI_ResetParser();
            return;
        }

        midi_running_status = status;
        midi_message[0] = status;
        midi_message_length = 1;
        midi_message_expected = expected;

        /*
         * Program Change and Channel Pressure have only two bytes.
         */
        if (midi_message_length >= midi_message_expected)
        {
            MIDI_MessageReceived(midi_message, midi_message_length);
            midi_message_length = 1;
        }

        return;
    }

    /*
     * Data byte.
     */
    if (midi_running_status == 0)
    {
        return;
    }

    midi_message[midi_message_length++] = byte & 0x7F;

    if (midi_message_length >= midi_message_expected)
    {
        MIDI_MessageReceived(midi_message, midi_message_length);

        /*
         * Running status remains active. Start the next message
         * with the same status byte.
         */
        midi_message[0] = midi_running_status;
        midi_message_length = 1;
    }
}

static uint8_t MIDI_MessageLength(uint8_t status)
{
    uint8_t type = status & 0xF0;

    switch (type)
    {
        case 0x80: /* Note Off */
        case 0x90: /* Note On */
        case 0xA0: /* Polyphonic Key Pressure */
        case 0xB0: /* Control Change */
        case 0xE0: /* Pitch Bend */
            return 3;

        case 0xC0: /* Program Change */
        case 0xD0: /* Channel Pressure */
            return 2;

        default:
            return 0;
    }
}

static void MIDI_ResetParser(void)
{
    midi_running_status = 0;
    midi_message_length = 0;
    midi_message_expected = 0;
    midi_message[0] = 0;
    midi_message[1] = 0;
    midi_message[2] = 0;
}

/*
 * HAL callback hooks.
 *
 * If your main.c already contains these callbacks, do NOT create
 * duplicate functions there. Instead call the MIDI functions from
 * your existing callbacks.
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    MIDI_UART_RxCpltCallback(huart);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    MIDI_UART_ErrorCallback(huart);
}

/*
 * GPIO initialization for the MIDI UART.
 *
 * USART1: PA9 = TX, PA10 = RX
 * USART2: PA2 = TX, PA3  = RX
 */

/*
 * MIDI UART interrupt handler (USART1_IRQHandler or USART2_IRQHandler,
 * depending on MIDI_UART_PORT).
 *
 * This is kept here so no additional modification of
 * stm32l1xx_it.c is required.
 */
void MIDI_UART_IRQ_HANDLER(void)
{
    HAL_UART_IRQHandler(&hMIDI_UART);
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (huart->Instance == MIDI_UART_INSTANCE)
    {
        MIDI_UART_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        GPIO_InitStruct.Pin = MIDI_UART_GPIO_PINS;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        GPIO_InitStruct.Alternate = MIDI_UART_GPIO_AF;
        HAL_GPIO_Init(MIDI_UART_GPIO_PORT, &GPIO_InitStruct);

        HAL_NVIC_SetPriority(MIDI_UART_IRQn, 1, 0);
        HAL_NVIC_EnableIRQ(MIDI_UART_IRQn);
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == MIDI_UART_INSTANCE)
    {
        MIDI_UART_CLK_DISABLE();

        HAL_GPIO_DeInit(MIDI_UART_GPIO_PORT, MIDI_UART_GPIO_PINS);

        HAL_NVIC_DisableIRQ(MIDI_UART_IRQn);
    }
}
