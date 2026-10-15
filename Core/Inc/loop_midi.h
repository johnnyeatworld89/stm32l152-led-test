#ifndef LOOP_MIDI_H
#define LOOP_MIDI_H

#include <stdint.h>

/* -------------------------------------------------------------------------- */
/* MIDI control of the effect devices in the loops                            */
/* -------------------------------------------------------------------------- */

/*
 * Every loop can have its own MIDI channel and up to
 * LOOP_MIDI_MAX_GROUPS "On/Bypass" groups. A group holds up to
 * LOOP_MIDI_MAX_SWITCHES switches of the effect device. A switch is
 * defined by two Control Change messages:
 *
 *   On/Off:  first message switches on, second switches off
 *   Toggle:  both messages are the same, every message toggles
 *
 * Within a group at most one switch is on (like the exclusive footswitches
 * of a multi effect). Several groups can be on at the same time. The loop
 * is active while at least one group is on.
 *
 * Receiving (effect device -> loop):
 *   On of a switch        the group is on with this switch
 *   Off of a switch       the group is off, if this switch was the one on
 *                         (or none was known to be on)
 *   Toggle of a switch    the same toggle twice in a row switches the
 *                         group off, otherwise the group is on with it
 *
 * Sending (loop switched by the router, e.g. preset load -> effect device):
 *   loop on:   the groups that were on when the loop was switched off get
 *              the On (or Toggle) of their last switch. If nothing is
 *              known yet, group 1 gets the On of its first switch.
 *   loop off:  every group that is on gets the Off (or Toggle) of its
 *              switch.
 *
 * Feedback Toggle (optional, per loop):
 *   Some devices (e.g. Boss DD-500) send On and Off of their footswitch
 *   alternately, no matter whether the effect was switched by MIDI in
 *   between. After the router switched the device, the values are
 *   swapped. With feedbackToggle on, every received On or Off of a
 *   switch toggles it (like a Toggle switch). Sending stays On/Off.
 *
 * Program Change (optional, per loop):
 *   With programChange on, a Program Change received on the channel of
 *   the loop is taken as the current program of the effect device. It
 *   is stored with a preset and sent again when the preset is loaded.
 *   Like the Control Changes of the loops, such a Program Change is not
 *   used for the presets (bank select), even on the preset channel.
 */

#define LOOP_MIDI_MAX_LOOPS          31U
#define LOOP_MIDI_MAX_GROUPS          8U
#define LOOP_MIDI_MAX_SWITCHES        8U
#define LOOP_MIDI_MAX_MESSAGES       (2U * LOOP_MIDI_MAX_SWITCHES)

/*
 * Messages that come back within this time after they were sent are
 * taken as an echo (MIDI OUT wired back to MIDI IN, MIDI thru of the
 * effect devices) and ignored.
 */
#define LOOP_MIDI_ECHO_TIME_MS      200U

/* No program known (or Program Change off). */
#define LOOP_MIDI_PROGRAM_NONE      0xFFU

typedef struct
{
    uint8_t controller;     /* 0 to 127 */
    uint8_t value;          /* 0 to 127 */

} LoopMidiMessage;

/*
 * Messages 2n and 2n+1 form switch n. While editing, the number of
 * messages can be odd (the Off of the last switch is still missing).
 * LoopMidi_StoreConfig completes such a switch as Toggle.
 */
typedef struct
{
    uint8_t messageCount;
    LoopMidiMessage messages[LOOP_MIDI_MAX_MESSAGES];

} LoopMidiGroup;

typedef struct
{
    uint8_t loopId;         /* item id of the loop, 0 = unused */
    uint8_t channel;        /* 0 to 15 (= MIDI channel 1 to 16) */
    uint8_t programChange;  /* 1 = Program Change on */
    uint8_t feedbackToggle; /* 1 = received On/Off both toggle */
    uint8_t groupCount;
    LoopMidiGroup groups[LOOP_MIDI_MAX_GROUPS];

} LoopMidiConfig;

typedef enum
{
    LOOP_MIDI_OK = 0,
    LOOP_MIDI_ERR_FULL,
    LOOP_MIDI_ERR_WRITE

} LoopMidiStatus;


/* Reads the configuration from the EEPROM. Call once at start-up. */
void LoopMidi_Init(void);

/*
 * Copies the configuration of a loop into config. A loop without
 * configuration gets channel 1 and no groups.
 */
void LoopMidi_GetConfig(
    uint8_t loopId,
    LoopMidiConfig *config
);

/*
 * Replaces the configuration of config->loopId and stores all
 * configurations in the EEPROM (blocks while writing). Groups without
 * messages are removed, an odd last message becomes a Toggle.
 * The switching state of this loop is reset. The current program is
 * kept unless the channel changes or Program Change is switched off.
 *
 * LOOP_MIDI_ERR_FULL: no free loop entry or the EEPROM block is too
 * small. Nothing was changed.
 */
LoopMidiStatus LoopMidi_StoreConfig(
    const LoopMidiConfig *config
);

/*
 * Returns 1 if the configuration with this content would still fit
 * into the EEPROM block (used while editing).
 */
uint8_t LoopMidi_ConfigFits(
    const LoopMidiConfig *config
);

/* Number of switches of a group (an incomplete one counts). */
uint8_t LoopMidi_SwitchCount(
    const LoopMidiGroup *group
);

/* Returns 1 if both messages of the switch are the same. */
uint8_t LoopMidi_SwitchIsToggle(
    const LoopMidiGroup *group,
    uint8_t switchIndex
);

/*
 * Returns 1 if the message is used by any loop or is an echo of a
 * message sent shortly before. Does not change anything.
 */
uint8_t LoopMidi_IsAssigned(
    uint8_t channel,
    uint8_t controller,
    uint8_t value
);

/*
 * Returns 1 and forgets the entry if the message is an echo of a
 * message sent within LOOP_MIDI_ECHO_TIME_MS.
 */
uint8_t LoopMidi_ConsumeEcho(
    uint8_t channel,
    uint8_t controller,
    uint8_t value
);

/*
 * Applies a received message to the groups of one loop.
 *
 * Returns -1 if the message does not belong to this loop, otherwise
 * the new state of the loop (0 = all groups off, 1 = a group is on).
 */
int8_t LoopMidi_Receive(
    uint8_t loopId,
    uint8_t channel,
    uint8_t controller,
    uint8_t value
);

/*
 * The loop was switched by the router: sends the messages that put the
 * effect device into the same state (see above). Does nothing for a
 * loop without groups.
 */
void LoopMidi_SetLoopState(
    uint8_t loopId,
    uint8_t active
);

/*
 * On/Bypass state of a loop for the presets: 4 bits per group (bits 4g
 * to 4g+3 = group g), 0 = group off, k = switch k (1 to 8) on. A loop
 * without groups returns 0.
 */
uint32_t LoopMidi_GetGroupState(
    uint8_t loopId
);

/*
 * Puts the effect device of the loop into the state (format as above):
 * a group that is off but should be on gets the On (or Toggle) of the
 * switch, one on with another switch gets the On of the new switch, one
 * on that should be off gets the Off (or Toggle) of its switch. Groups
 * already in the wanted state get nothing (a Toggle would switch them
 * back). Switch numbers the group does not have leave the group
 * unchanged.
 *
 * Returns -1 for a loop without groups, otherwise the new state of the
 * loop (0 = all groups off, 1 = a group is on).
 */
int8_t LoopMidi_ApplyGroupState(
    uint8_t loopId,
    uint32_t state
);

/*
 * Returns 1 if a Program Change on this channel belongs to a loop with
 * Program Change on, or is an echo of one sent shortly before. Does not
 * change anything.
 */
uint8_t LoopMidi_ProgramIsAssigned(
    uint8_t channel,
    uint8_t program
);

/*
 * Takes a received Program Change as the current program of every loop
 * with Program Change on and this channel. An echo is ignored.
 */
void LoopMidi_ReceiveProgram(
    uint8_t channel,
    uint8_t program
);

/*
 * Current program of the loop, LOOP_MIDI_PROGRAM_NONE if Program Change
 * is off or no program is known yet.
 */
uint8_t LoopMidi_GetProgram(
    uint8_t loopId
);

/*
 * MIDI channel (0 to 15) of the loop if Program Change is on for it,
 * otherwise LOOP_MIDI_PROGRAM_NONE.
 */
uint8_t LoopMidi_GetProgramChannel(
    uint8_t loopId
);

/*
 * Sends the program (0 to 127) to the effect device of the loop and
 * takes it as the current program. Does nothing if Program Change is
 * off for this loop.
 */
void LoopMidi_SendProgram(
    uint8_t loopId,
    uint8_t program
);

/*
 * Sends a Control Change or Program Change that is not bound to a loop
 * (additional MIDI messages of a preset). Like the messages of the
 * loops it is remembered for the echo filter. A Program Change is taken
 * as the current program of every loop with Program Change on and this
 * channel.
 */
void LoopMidi_SendControlChange(
    uint8_t channel,
    uint8_t controller,
    uint8_t value
);

void LoopMidi_SendChannelProgram(
    uint8_t channel,
    uint8_t program
);

/* Returns 1 if the loop has at least one group. */
uint8_t LoopMidi_HasGroups(
    uint8_t loopId
);

#endif
