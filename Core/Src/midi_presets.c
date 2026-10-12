#include "midi_presets.h"

#include "midi.h"
#include "preset_store.h"
#include "ui.h"

#include <stddef.h>

/* -------------------------------------------------------------------------- */
/* Event queue                                                                */
/* -------------------------------------------------------------------------- */

typedef enum
{
    EVENT_PROGRAM_CHANGE = 1U,
    EVENT_CONTROL_CHANGE

} EventType;

_Static_assert(
    (MIDI_PRESETS_CC_SAVE < MIDI_PRESETS_CC_FIRST) ||
    (MIDI_PRESETS_CC_SAVE >=
        (MIDI_PRESETS_CC_FIRST + PRESET_SLOTS_PER_BANK)),
    "MIDI_PRESETS_CC_SAVE must not be a preset number");

typedef struct
{
    uint8_t type;
    uint8_t channel;
    uint8_t data1;      /* program or controller */
    uint8_t data2;      /* value of a Control Change */

} Event;

/*
 * Single producer (MIDI receive), single consumer (main loop).
 * head is written only by the producer, tail only by the consumer.
 * One slot stays unused to tell "full" from "empty".
 *
 * Size must be a power of two and at most 128.
 */
#define EVENT_QUEUE_SIZE      32U
#define EVENT_QUEUE_MASK      (EVENT_QUEUE_SIZE - 1U)

static volatile Event eventQueue[EVENT_QUEUE_SIZE];
static volatile uint8_t eventHead = 0U;
static volatile uint8_t eventTail = 0U;

/* Messages dropped because the queue was full. */
volatile uint8_t debugMidiPresetOverflows = 0U;


static void Queue_Push(
    uint8_t type,
    uint8_t channel,
    uint8_t data1,
    uint8_t data2)
{
    uint8_t head = eventHead;
    uint8_t next = (uint8_t)((head + 1U) & EVENT_QUEUE_MASK);

    if (next == eventTail)
    {
        if (debugMidiPresetOverflows < 0xFFU)
        {
            debugMidiPresetOverflows++;
        }

        return;
    }

    eventQueue[head].type = type;
    eventQueue[head].channel = channel;
    eventQueue[head].data1 = data1;
    eventQueue[head].data2 = data2;
    eventHead = next;
}


static uint8_t Queue_Pop(Event *event)
{
    uint8_t tail = eventTail;

    if (tail == eventHead)
    {
        return 0U;
    }

    event->type = eventQueue[tail].type;
    event->channel = eventQueue[tail].channel;
    event->data1 = eventQueue[tail].data1;
    event->data2 = eventQueue[tail].data2;
    eventTail = (uint8_t)((tail + 1U) & EVENT_QUEUE_MASK);

    return 1U;
}


static uint8_t Channel_Matches(uint8_t channel)
{
    return (MIDI_PRESETS_CHANNEL == MIDI_PRESETS_CHANNEL_OMNI ||
            channel == MIDI_PRESETS_CHANNEL) ? 1U : 0U;
}


/* -------------------------------------------------------------------------- */
/* Receiving                                                                  */
/* -------------------------------------------------------------------------- */

/*
 * Program and Control Changes are queued unfiltered: whether a message
 * belongs to a loop (loop_midi.h) or to the presets is decided in the
 * main loop, where the loop configuration may be read and changed.
 */
void MidiPresets_OnProgramChange(
    uint8_t channel,
    uint8_t program)
{
    Queue_Push(EVENT_PROGRAM_CHANGE, channel, program, 0U);
}


void MidiPresets_OnControlChange(
    uint8_t channel,
    uint8_t controller,
    uint8_t value)
{
    Queue_Push(EVENT_CONTROL_CHANGE, channel, controller, value);
}


/*
 * Called by the MIDI driver (midi.c) from the USART2 interrupt for every
 * complete channel message. Replaces the weak default of midi.c.
 *
 * data[0] is the status byte, followed by the data bytes.
 */
void MIDI_MessageReceived(const uint8_t *data, uint8_t length)
{
    if (data == NULL || length < 2U)
    {
        return;
    }

    uint8_t channel = (uint8_t)(data[0] & 0x0FU);

    switch (data[0] & 0xF0U)
    {
        case 0xC0U:     /* Program Change: program */
            MidiPresets_OnProgramChange(channel, data[1]);
            break;

        case 0xB0U:     /* Control Change: controller, value */
            if (length >= 3U)
            {
                MidiPresets_OnControlChange(channel, data[1], data[2]);
            }
            break;

        default:
            break;
    }
}


/* -------------------------------------------------------------------------- */
/* Processing (main loop)                                                     */
/* -------------------------------------------------------------------------- */

void MidiPresets_Process(void)
{
    Event event;

    /*
     * Several Control Changes in a row are merged: only the last one
     * loads a preset. A Program Change, CC0 or a loop message in between
     * has to see the preset before it, so a pending preset is loaded
     * first.
     *
     * While saving is armed, the next preset number does not load but
     * saves. The events are handled in the order they arrived.
     */
    int16_t pendingSlot = -1;

    while (Queue_Pop(&event))
    {
        /*
         * A Control Change that belongs to a loop (or is learned in its
         * MIDI menu) is not used for the presets. The same holds for a
         * Program Change on the channel of a loop with Program Change on.
         */
        uint8_t loopMessage = 0U;

        if (event.type == EVENT_PROGRAM_CHANGE)
        {
            loopMessage = UI_LoopMidiClaimsProgram(
                event.channel, event.data1);

            if (!loopMessage &&
                (!Channel_Matches(event.channel) ||
                 event.data1 >= PRESET_BANK_COUNT))
            {
                continue;
            }
        }
        else if (event.type == EVENT_CONTROL_CHANGE)
        {
            loopMessage = UI_LoopMidiClaims(
                event.channel, event.data1, event.data2);

            if (!loopMessage)
            {
                if (!Channel_Matches(event.channel))
                {
                    continue;
                }

                if (event.data1 != MIDI_PRESETS_CC_SAVE)
                {
                    if (event.data1 < MIDI_PRESETS_CC_FIRST ||
                        event.data1 >= (MIDI_PRESETS_CC_FIRST +
                                        PRESET_SLOTS_PER_BANK))
                    {
                        continue;
                    }

                    uint8_t slot =
                        (uint8_t)(event.data1 - MIDI_PRESETS_CC_FIRST);

                    if (UI_SaveArmed())
                    {
                        UI_SavePresetFromMidi(slot);
                    }
                    else
                    {
                        pendingSlot = slot;
                    }

                    continue;
                }
            }
        }

        if (pendingSlot >= 0)
        {
            UI_RecallPreset((uint8_t)pendingSlot);
            pendingSlot = -1;
        }

        if (event.type == EVENT_PROGRAM_CHANGE)
        {
            if (loopMessage)
            {
                UI_LoopMidiProgramChange(event.channel, event.data1);
            }
            else
            {
                UI_SelectBank(event.data1);
            }
        }
        else if (loopMessage)
        {
            UI_LoopMidiControlChange(
                event.channel, event.data1, event.data2);
        }
        else
        {
            UI_SaveArmToggle();
        }
    }

    if (pendingSlot >= 0)
    {
        UI_RecallPreset((uint8_t)pendingSlot);
    }
}


/* -------------------------------------------------------------------------- */
/* Sending                                                                    */
/* -------------------------------------------------------------------------- */

void MidiPresets_NotifyPresetChanged(
    uint8_t bank,
    uint8_t slot)
{
    uint8_t channel = (uint8_t)MIDI_PRESETS_TX_CHANNEL;

    MIDI_ProgramChange(channel, bank);

    MIDI_ControlChange(
        channel,
        (uint8_t)(MIDI_PRESETS_CC_FIRST + slot),
        MIDI_PRESETS_SEND_VALUE
    );
}
