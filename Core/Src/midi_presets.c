#include "midi_presets.h"

#include "midi.h"
#include "preset_store.h"
#include "ui.h"

/* -------------------------------------------------------------------------- */
/* Event queue                                                                */
/* -------------------------------------------------------------------------- */

typedef enum
{
    EVENT_BANK = 1U,
    EVENT_PRESET

} EventType;

typedef struct
{
    uint8_t type;
    uint8_t value;      /* bank (0 to 63) or slot (0 to 23) */

} Event;

/*
 * Single producer (MIDI receive), single consumer (main loop).
 * head is written only by the producer, tail only by the consumer.
 * One slot stays unused to tell "full" from "empty".
 *
 * Size must be a power of two and at most 128.
 */
#define EVENT_QUEUE_SIZE      16U
#define EVENT_QUEUE_MASK      (EVENT_QUEUE_SIZE - 1U)

static volatile Event eventQueue[EVENT_QUEUE_SIZE];
static volatile uint8_t eventHead = 0U;
static volatile uint8_t eventTail = 0U;

/* Messages dropped because the queue was full. */
volatile uint8_t debugMidiPresetOverflows = 0U;


static void Queue_Push(uint8_t type, uint8_t value)
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
    eventQueue[head].value = value;
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
    event->value = eventQueue[tail].value;
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

void MidiPresets_OnProgramChange(
    uint8_t channel,
    uint8_t program)
{
    if (!Channel_Matches(channel) ||
        program >= PRESET_BANK_COUNT)
    {
        return;
    }

    Queue_Push(EVENT_BANK, program);
}


void MidiPresets_OnControlChange(
    uint8_t channel,
    uint8_t controller,
    uint8_t value)
{
    (void)value;    /* every value selects the preset */

    if (!Channel_Matches(channel) ||
        controller < MIDI_PRESETS_CC_FIRST ||
        controller >= (MIDI_PRESETS_CC_FIRST + PRESET_SLOTS_PER_BANK))
    {
        return;
    }

    Queue_Push(EVENT_PRESET,
               (uint8_t)(controller - MIDI_PRESETS_CC_FIRST));
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
     * loads a preset. A Program Change in between changes the bank, so
     * the pending preset has to be loaded first.
     */
    int16_t pendingSlot = -1;

    while (Queue_Pop(&event))
    {
        if (event.type == EVENT_PRESET)
        {
            pendingSlot = event.value;
            continue;
        }

        if (pendingSlot >= 0)
        {
            UI_RecallPreset((uint8_t)pendingSlot);
            pendingSlot = -1;
        }

        UI_SelectBank(event.value);
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
