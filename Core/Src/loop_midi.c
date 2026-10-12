#include "loop_midi.h"

#include "midi.h"
#include "preset_store.h"

#include <string.h>

/* -------------------------------------------------------------------------- */
/* State                                                                      */
/* -------------------------------------------------------------------------- */

/*
 * Switching state of a loop. It is only known from the messages sent
 * and received since start-up.
 */
typedef struct
{
    /* Switch that is on in each group, -1 = group off. */
    int8_t activeSwitch[LOOP_MIDI_MAX_GROUPS];

    /* Switch that was on last in each group, -1 = none yet. */
    int8_t lastSwitch[LOOP_MIDI_MAX_GROUPS];

    /* Groups that were on when the loop was switched off. */
    uint8_t restoreMask;

} LoopMidiState;

_Static_assert(
    LOOP_MIDI_MAX_GROUPS <= 8U,
    "restoreMask has 8 bits");

static LoopMidiConfig loopConfigs[LOOP_MIDI_MAX_LOOPS];
static LoopMidiState loopStates[LOOP_MIDI_MAX_LOOPS];

/* Current program of each entry, LOOP_MIDI_PROGRAM_NONE = unknown. */
static uint8_t loopPrograms[LOOP_MIDI_MAX_LOOPS];

/* Image of the EEPROM block, built before writing. */
static uint8_t storeBuffer[PRESET_CONFIG_MAX_BYTES];


/* -------------------------------------------------------------------------- */
/* Echo filter                                                                */
/* -------------------------------------------------------------------------- */

#define ECHO_SLOTS                   16U

/* Controller of an echo entry for a Program Change. */
#define ECHO_PROGRAM                 0x80U

typedef struct
{
    uint8_t used;
    uint8_t channel;
    uint8_t controller;
    uint8_t value;
    uint32_t tick;

} SentMessage;

static SentMessage sentMessages[ECHO_SLOTS];
static uint8_t sentNext = 0U;


static int16_t Echo_Find(
    uint8_t channel,
    uint8_t controller,
    uint8_t value)
{
    uint32_t now = HAL_GetTick();

    for (uint8_t i = 0U; i < ECHO_SLOTS; i++)
    {
        const SentMessage *sent = &sentMessages[i];

        if (sent->used &&
            sent->channel == channel &&
            sent->controller == controller &&
            sent->value == value &&
            (uint32_t)(now - sent->tick) <= LOOP_MIDI_ECHO_TIME_MS)
        {
            return (int16_t)i;
        }
    }

    return -1;
}


static void Echo_Remember(
    uint8_t channel,
    uint8_t controller,
    uint8_t value)
{
    SentMessage *sent = &sentMessages[sentNext];

    sent->used = 1U;
    sent->channel = channel;
    sent->controller = controller;
    sent->value = value;
    sent->tick = HAL_GetTick();

    sentNext = (uint8_t)((sentNext + 1U) % ECHO_SLOTS);
}


static void Send_Message(
    uint8_t channel,
    const LoopMidiMessage *message)
{
    MIDI_ControlChange(channel, message->controller, message->value);

    Echo_Remember(channel, message->controller, message->value);
}


uint8_t LoopMidi_ConsumeEcho(
    uint8_t channel,
    uint8_t controller,
    uint8_t value)
{
    int16_t index = Echo_Find(channel, controller, value);

    if (index < 0)
    {
        return 0U;
    }

    sentMessages[index].used = 0U;

    return 1U;
}


/* -------------------------------------------------------------------------- */
/* Helpers                                                                    */
/* -------------------------------------------------------------------------- */

static uint8_t Message_Equals(
    const LoopMidiMessage *message,
    uint8_t controller,
    uint8_t value)
{
    return (message->controller == controller &&
            message->value == value) ? 1U : 0U;
}


static void State_Reset(uint8_t index)
{
    for (uint8_t g = 0U; g < LOOP_MIDI_MAX_GROUPS; g++)
    {
        loopStates[index].activeSwitch[g] = -1;
        loopStates[index].lastSwitch[g] = -1;
    }

    loopStates[index].restoreMask = 0U;
}


static int16_t Config_Find(uint8_t loopId)
{
    if (loopId == 0U)
    {
        return -1;
    }

    for (uint8_t i = 0U; i < LOOP_MIDI_MAX_LOOPS; i++)
    {
        if (loopConfigs[i].loopId == loopId)
        {
            return (int16_t)i;
        }
    }

    return -1;
}


/*
 * Removes groups without messages and completes an odd last message
 * to a Toggle (the same message twice).
 */
static void Config_Normalize(LoopMidiConfig *config)
{
    uint8_t target = 0U;

    if (config->channel > 15U)
    {
        config->channel = 0U;
    }

    config->programChange = (config->programChange != 0U) ? 1U : 0U;

    if (config->groupCount > LOOP_MIDI_MAX_GROUPS)
    {
        config->groupCount = LOOP_MIDI_MAX_GROUPS;
    }

    for (uint8_t g = 0U; g < config->groupCount; g++)
    {
        LoopMidiGroup *group = &config->groups[g];

        if (group->messageCount > LOOP_MIDI_MAX_MESSAGES)
        {
            group->messageCount = LOOP_MIDI_MAX_MESSAGES;
        }

        if (group->messageCount == 0U)
        {
            continue;
        }

        if ((group->messageCount & 1U) != 0U)
        {
            group->messages[group->messageCount] =
                group->messages[group->messageCount - 1U];
            group->messageCount++;
        }

        if (target != g)
        {
            config->groups[target] = *group;
        }

        target++;
    }

    for (uint8_t g = target; g < LOOP_MIDI_MAX_GROUPS; g++)
    {
        config->groups[g].messageCount = 0U;
    }

    config->groupCount = target;
}


/*
 * A configuration with channel 1 and without groups is the default and
 * needs no entry.
 */
static uint8_t Config_IsDefault(const LoopMidiConfig *config)
{
    return (config->channel == 0U &&
            config->programChange == 0U &&
            config->groupCount == 0U) ? 1U : 0U;
}


/* Size in the EEPROM block of a normalized configuration. */
static uint16_t Config_StoredSize(const LoopMidiConfig *config)
{
    if (config->loopId == 0U || Config_IsDefault(config))
    {
        return 0U;
    }

    uint16_t size = 3U;

    for (uint8_t g = 0U; g < config->groupCount; g++)
    {
        uint8_t count = config->groups[g].messageCount;

        count = (uint8_t)((count + 1U) & ~1U);
        size = (uint16_t)(size + 1U + (2U * count));
    }

    return size;
}


/*
 * Block format, one entry per loop:
 *   loop id, channel (bit 7 = Program Change on), group count,
 *   per group: message count, then controller and value per message
 */
static uint16_t Store_Serialize(void)
{
    uint16_t position = 0U;

    for (uint8_t i = 0U; i < LOOP_MIDI_MAX_LOOPS; i++)
    {
        const LoopMidiConfig *config = &loopConfigs[i];
        uint16_t size = Config_StoredSize(config);

        if (size == 0U ||
            (uint16_t)(position + size) > PRESET_CONFIG_MAX_BYTES)
        {
            continue;
        }

        storeBuffer[position++] = config->loopId;
        storeBuffer[position++] = (uint8_t)(config->channel |
            ((config->programChange != 0U) ? 0x80U : 0x00U));
        storeBuffer[position++] = config->groupCount;

        for (uint8_t g = 0U; g < config->groupCount; g++)
        {
            const LoopMidiGroup *group = &config->groups[g];

            storeBuffer[position++] = group->messageCount;

            for (uint8_t m = 0U; m < group->messageCount; m++)
            {
                storeBuffer[position++] = group->messages[m].controller;
                storeBuffer[position++] = group->messages[m].value;
            }
        }
    }

    return position;
}


static uint8_t Store_Parse(
    const uint8_t *data,
    uint16_t length)
{
    uint16_t position = 0U;
    uint8_t index = 0U;

    while (position < length)
    {
        if (index >= LOOP_MIDI_MAX_LOOPS ||
            (uint16_t)(position + 3U) > length)
        {
            return 0U;
        }

        LoopMidiConfig *config = &loopConfigs[index];

        config->loopId = data[position++];
        config->channel = data[position++];
        config->groupCount = data[position++];

        config->programChange = (uint8_t)(config->channel >> 7);
        config->channel = (uint8_t)(config->channel & 0x7FU);

        if (config->loopId == 0U ||
            config->channel > 15U ||
            config->groupCount > LOOP_MIDI_MAX_GROUPS ||
            Config_Find(config->loopId) != (int16_t)index)
        {
            return 0U;
        }

        for (uint8_t g = 0U; g < config->groupCount; g++)
        {
            LoopMidiGroup *group = &config->groups[g];

            if (position >= length)
            {
                return 0U;
            }

            group->messageCount = data[position++];

            if (group->messageCount == 0U ||
                group->messageCount > LOOP_MIDI_MAX_MESSAGES ||
                (group->messageCount & 1U) != 0U ||
                (uint16_t)(position + (2U * group->messageCount)) >
                    length)
            {
                return 0U;
            }

            for (uint8_t m = 0U; m < group->messageCount; m++)
            {
                group->messages[m].controller = data[position++];
                group->messages[m].value = data[position++];

                if (group->messages[m].controller > 127U ||
                    group->messages[m].value > 127U)
                {
                    return 0U;
                }
            }
        }

        index++;
    }

    return 1U;
}


/* -------------------------------------------------------------------------- */
/* Configuration                                                              */
/* -------------------------------------------------------------------------- */

void LoopMidi_Init(void)
{
    uint16_t length = 0U;

    memset(loopConfigs, 0, sizeof(loopConfigs));

    if (PresetStore_ReadConfig(storeBuffer, &length) ==
            PRESET_STORE_OK &&
        !Store_Parse(storeBuffer, length))
    {
        memset(loopConfigs, 0, sizeof(loopConfigs));
    }

    for (uint8_t i = 0U; i < LOOP_MIDI_MAX_LOOPS; i++)
    {
        State_Reset(i);
        loopPrograms[i] = LOOP_MIDI_PROGRAM_NONE;
    }

    memset(sentMessages, 0, sizeof(sentMessages));
}


void LoopMidi_GetConfig(
    uint8_t loopId,
    LoopMidiConfig *config)
{
    int16_t index = Config_Find(loopId);

    if (index >= 0)
    {
        *config = loopConfigs[index];
        return;
    }

    memset(config, 0, sizeof(*config));
    config->loopId = loopId;
}


/*
 * Size of all stored entries if the entry of config->loopId is
 * replaced by the (normalized) config. Returns 0xFFFF if there is no
 * free entry for a new loop.
 */
static uint16_t Config_TotalSizeWith(const LoopMidiConfig *config)
{
    int16_t index = Config_Find(config->loopId);
    uint16_t total = Config_StoredSize(config);
    uint8_t freeEntry = 0U;

    for (uint8_t i = 0U; i < LOOP_MIDI_MAX_LOOPS; i++)
    {
        if ((int16_t)i == index)
        {
            continue;
        }

        if (loopConfigs[i].loopId == 0U)
        {
            freeEntry = 1U;
            continue;
        }

        total = (uint16_t)(total + Config_StoredSize(&loopConfigs[i]));
    }

    if (index < 0 && !freeEntry && !Config_IsDefault(config))
    {
        return 0xFFFFU;
    }

    return total;
}


uint8_t LoopMidi_ConfigFits(const LoopMidiConfig *config)
{
    if (config == NULL || config->loopId == 0U)
    {
        return 0U;
    }

    LoopMidiConfig normalized = *config;

    Config_Normalize(&normalized);

    return (Config_TotalSizeWith(&normalized) <=
            PRESET_CONFIG_MAX_BYTES) ? 1U : 0U;
}


LoopMidiStatus LoopMidi_StoreConfig(const LoopMidiConfig *config)
{
    if (config == NULL || config->loopId == 0U)
    {
        return LOOP_MIDI_ERR_FULL;
    }

    LoopMidiConfig normalized = *config;

    Config_Normalize(&normalized);

    if (Config_TotalSizeWith(&normalized) > PRESET_CONFIG_MAX_BYTES)
    {
        return LOOP_MIDI_ERR_FULL;
    }

    int16_t index = Config_Find(normalized.loopId);

    if (Config_IsDefault(&normalized))
    {
        if (index >= 0)
        {
            memset(&loopConfigs[index], 0, sizeof(loopConfigs[index]));
            State_Reset((uint8_t)index);
            loopPrograms[index] = LOOP_MIDI_PROGRAM_NONE;
        }
    }
    else
    {
        if (index < 0)
        {
            for (uint8_t i = 0U; i < LOOP_MIDI_MAX_LOOPS; i++)
            {
                if (loopConfigs[i].loopId == 0U)
                {
                    index = (int16_t)i;
                    break;
                }
            }
        }

        if (index < 0)
        {
            return LOOP_MIDI_ERR_FULL;
        }

        if (loopConfigs[index].loopId != normalized.loopId ||
            loopConfigs[index].channel != normalized.channel ||
            !normalized.programChange)
        {
            loopPrograms[index] = LOOP_MIDI_PROGRAM_NONE;
        }

        loopConfigs[index] = normalized;
        State_Reset((uint8_t)index);
    }

    uint16_t length = Store_Serialize();

    if (PresetStore_WriteConfig(storeBuffer, length) != PRESET_STORE_OK)
    {
        return LOOP_MIDI_ERR_WRITE;
    }

    return LOOP_MIDI_OK;
}


uint8_t LoopMidi_SwitchCount(const LoopMidiGroup *group)
{
    return (uint8_t)((group->messageCount + 1U) / 2U);
}


uint8_t LoopMidi_SwitchIsToggle(
    const LoopMidiGroup *group,
    uint8_t switchIndex)
{
    uint8_t on = (uint8_t)(2U * switchIndex);

    if ((uint8_t)(on + 1U) >= group->messageCount)
    {
        return 0U;
    }

    return Message_Equals(
        &group->messages[on + 1U],
        group->messages[on].controller,
        group->messages[on].value);
}


uint8_t LoopMidi_HasGroups(uint8_t loopId)
{
    int16_t index = Config_Find(loopId);

    return (index >= 0 && loopConfigs[index].groupCount > 0U) ? 1U : 0U;
}


/* -------------------------------------------------------------------------- */
/* Receiving                                                                  */
/* -------------------------------------------------------------------------- */

uint8_t LoopMidi_IsAssigned(
    uint8_t channel,
    uint8_t controller,
    uint8_t value)
{
    if (Echo_Find(channel, controller, value) >= 0)
    {
        return 1U;
    }

    for (uint8_t i = 0U; i < LOOP_MIDI_MAX_LOOPS; i++)
    {
        const LoopMidiConfig *config = &loopConfigs[i];

        if (config->loopId == 0U || config->channel != channel)
        {
            continue;
        }

        for (uint8_t g = 0U; g < config->groupCount; g++)
        {
            const LoopMidiGroup *group = &config->groups[g];

            for (uint8_t m = 0U; m < group->messageCount; m++)
            {
                if (Message_Equals(&group->messages[m],
                                   controller, value))
                {
                    return 1U;
                }
            }
        }
    }

    return 0U;
}


static uint8_t State_ActiveMask(uint8_t index)
{
    uint8_t mask = 0U;

    for (uint8_t g = 0U; g < loopConfigs[index].groupCount; g++)
    {
        if (loopStates[index].activeSwitch[g] >= 0)
        {
            mask |= (uint8_t)(1U << g);
        }
    }

    return mask;
}


int8_t LoopMidi_Receive(
    uint8_t loopId,
    uint8_t channel,
    uint8_t controller,
    uint8_t value)
{
    int16_t index = Config_Find(loopId);

    if (index < 0 || loopConfigs[index].channel != channel)
    {
        return -1;
    }

    const LoopMidiConfig *config = &loopConfigs[index];
    LoopMidiState *state = &loopStates[index];
    uint8_t maskBefore = State_ActiveMask((uint8_t)index);
    uint8_t handled = 0U;

    for (uint8_t g = 0U; g < config->groupCount; g++)
    {
        const LoopMidiGroup *group = &config->groups[g];
        uint8_t switchCount = LoopMidi_SwitchCount(group);

        for (uint8_t k = 0U; k < switchCount; k++)
        {
            const LoopMidiMessage *on = &group->messages[2U * k];
            const LoopMidiMessage *off = &group->messages[(2U * k) + 1U];

            if (LoopMidi_SwitchIsToggle(group, k))
            {
                if (!Message_Equals(on, controller, value))
                {
                    continue;
                }

                if (state->activeSwitch[g] == (int8_t)k)
                {
                    state->activeSwitch[g] = -1;
                }
                else
                {
                    state->activeSwitch[g] = (int8_t)k;
                    state->lastSwitch[g] = (int8_t)k;
                }
            }
            else if (Message_Equals(on, controller, value))
            {
                state->activeSwitch[g] = (int8_t)k;
                state->lastSwitch[g] = (int8_t)k;
            }
            else if (Message_Equals(off, controller, value))
            {
                /*
                 * The Off of a switch that is not the one on does not
                 * change the group: the device is still on with the
                 * other switch.
                 */
                if (state->activeSwitch[g] == (int8_t)k)
                {
                    state->activeSwitch[g] = -1;
                }
            }
            else
            {
                continue;
            }

            handled = 1U;
            break;
        }
    }

    if (!handled)
    {
        return -1;
    }

    uint8_t maskAfter = State_ActiveMask((uint8_t)index);

    if (maskAfter == 0U && maskBefore != 0U)
    {
        state->restoreMask = maskBefore;
    }

    return (maskAfter != 0U) ? 1 : 0;
}


/* -------------------------------------------------------------------------- */
/* Sending                                                                    */
/* -------------------------------------------------------------------------- */

void LoopMidi_SetLoopState(
    uint8_t loopId,
    uint8_t active)
{
    int16_t index = Config_Find(loopId);

    if (index < 0 || loopConfigs[index].groupCount == 0U)
    {
        return;
    }

    const LoopMidiConfig *config = &loopConfigs[index];
    LoopMidiState *state = &loopStates[index];
    uint8_t activeMask = State_ActiveMask((uint8_t)index);

    if (active)
    {
        if (activeMask != 0U)
        {
            return;     /* the device is already on */
        }

        uint8_t mask = state->restoreMask;

        if (mask == 0U)
        {
            mask = 1U;
        }

        for (uint8_t g = 0U; g < config->groupCount; g++)
        {
            if ((mask & (1U << g)) == 0U)
            {
                continue;
            }

            const LoopMidiGroup *group = &config->groups[g];
            int8_t k = state->lastSwitch[g];

            if (k < 0 || k >= (int8_t)LoopMidi_SwitchCount(group))
            {
                k = 0;
            }

            Send_Message(config->channel, &group->messages[2U * (uint8_t)k]);

            state->activeSwitch[g] = k;
            state->lastSwitch[g] = k;
        }

        return;
    }

    if (activeMask == 0U)
    {
        return;         /* nothing known to be on */
    }

    for (uint8_t g = 0U; g < config->groupCount; g++)
    {
        int8_t k = state->activeSwitch[g];

        if (k < 0)
        {
            continue;
        }

        Send_Message(
            config->channel,
            &config->groups[g].messages[(2U * (uint8_t)k) + 1U]);

        state->activeSwitch[g] = -1;
    }

    state->restoreMask = activeMask;
}


/* -------------------------------------------------------------------------- */
/* Program Change                                                             */
/* -------------------------------------------------------------------------- */

uint8_t LoopMidi_ProgramIsAssigned(
    uint8_t channel,
    uint8_t program)
{
    if (Echo_Find(channel, ECHO_PROGRAM, program) >= 0)
    {
        return 1U;
    }

    for (uint8_t i = 0U; i < LOOP_MIDI_MAX_LOOPS; i++)
    {
        if (loopConfigs[i].loopId != 0U &&
            loopConfigs[i].programChange &&
            loopConfigs[i].channel == channel)
        {
            return 1U;
        }
    }

    return 0U;
}


void LoopMidi_ReceiveProgram(
    uint8_t channel,
    uint8_t program)
{
    if (program > 127U ||
        LoopMidi_ConsumeEcho(channel, ECHO_PROGRAM, program))
    {
        return;
    }

    for (uint8_t i = 0U; i < LOOP_MIDI_MAX_LOOPS; i++)
    {
        if (loopConfigs[i].loopId != 0U &&
            loopConfigs[i].programChange &&
            loopConfigs[i].channel == channel)
        {
            loopPrograms[i] = program;
        }
    }
}


uint8_t LoopMidi_GetProgram(uint8_t loopId)
{
    int16_t index = Config_Find(loopId);

    if (index < 0 || !loopConfigs[index].programChange)
    {
        return LOOP_MIDI_PROGRAM_NONE;
    }

    return loopPrograms[index];
}


void LoopMidi_SendProgram(
    uint8_t loopId,
    uint8_t program)
{
    int16_t index = Config_Find(loopId);

    if (index < 0 ||
        !loopConfigs[index].programChange ||
        program > 127U)
    {
        return;
    }

    uint8_t channel = loopConfigs[index].channel;

    MIDI_ProgramChange(channel, program);
    Echo_Remember(channel, ECHO_PROGRAM, program);

    /*
     * Every loop on this channel with Program Change on sees the same
     * program (one device, several loops).
     */
    for (uint8_t i = 0U; i < LOOP_MIDI_MAX_LOOPS; i++)
    {
        if (loopConfigs[i].loopId != 0U &&
            loopConfigs[i].programChange &&
            loopConfigs[i].channel == channel)
        {
            loopPrograms[i] = program;
        }
    }
}
