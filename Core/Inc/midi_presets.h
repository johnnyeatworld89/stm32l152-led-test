#ifndef MIDI_PRESETS_H
#define MIDI_PRESETS_H

#include <stdint.h>

/* -------------------------------------------------------------------------- */
/* MIDI control of the presets                                                */
/* -------------------------------------------------------------------------- */

/*
 * Incoming:
 *   Program Change (0 to 63)      selects the bank
 *   Control Change (number)       loads a preset of the selected bank,
 *                                 the value (0 to 127) is ignored
 *
 * Outgoing, whenever a preset is saved or loaded (menu or MIDI):
 *   Program Change (bank), then Control Change (preset number, value 127)
 *
 * Feedback loops: a Control Change for a preset whose layout is already
 * active does nothing and sends nothing. If MIDI OUT comes back into
 * MIDI IN, the returning messages ask for the state the device is
 * already in, so the loop ends after one round.
 */

/*
 * MIDI channel for receiving, 0 to 15 (= channel 1 to 16).
 * MIDI_PRESETS_CHANNEL_OMNI receives on all channels.
 */
#define MIDI_PRESETS_CHANNEL_OMNI    0xFFU

#ifndef MIDI_PRESETS_CHANNEL
#define MIDI_PRESETS_CHANNEL         0U
#endif

/*
 * MIDI channel for sending. By default the receive channel (channel 0
 * if the receive channel is omni).
 *
 * A different send channel is a simple second protection against
 * feedback loops: if MIDI OUT is wired back to MIDI IN, the messages
 * come back on a channel that is not received.
 */
#ifndef MIDI_PRESETS_TX_CHANNEL
#define MIDI_PRESETS_TX_CHANNEL \
    ((MIDI_PRESETS_CHANNEL == MIDI_PRESETS_CHANNEL_OMNI) ? \
     0U : MIDI_PRESETS_CHANNEL)
#endif

/*
 * Control Change number of preset 1. Preset n uses the number
 * MIDI_PRESETS_CC_FIRST + n - 1, so presets 1 to 24 use the numbers
 * 1 to 24 with the default.
 */
#ifndef MIDI_PRESETS_CC_FIRST
#define MIDI_PRESETS_CC_FIRST        1U
#endif

/* Value of the Control Change that is sent after a save or load. */
#define MIDI_PRESETS_SEND_VALUE      127U


/*
 * Receiving: this module defines MIDI_MessageReceived() of midi.c, which
 * the MIDI driver calls from the USART2 interrupt for every complete
 * message. Do not define it a second time in main.c.
 *
 * The two functions below are called by it. They only store the message
 * in a queue, so they can also be called from an interrupt.
 */
void MidiPresets_OnProgramChange(
    uint8_t channel,
    uint8_t program
);

void MidiPresets_OnControlChange(
    uint8_t channel,
    uint8_t controller,
    uint8_t value
);

/*
 * Handles the queued messages. Call this regularly from the main loop.
 * Changing a preset redraws the display, which must not happen in an
 * interrupt.
 */
void MidiPresets_Process(void);

/*
 * Sends Program Change (bank) and Control Change (preset, value 127)
 * for a preset that was just saved or loaded. slot is 0 to 23.
 */
void MidiPresets_NotifyPresetChanged(
    uint8_t bank,
    uint8_t slot
);

#endif
