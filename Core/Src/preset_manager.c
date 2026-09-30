#include "preset_manager.h"

#include <stddef.h>
#include <string.h>


/* -------------------------------------------------------------------------- */
/* Stored states                                                              */
/* -------------------------------------------------------------------------- */

static PresetState presetStates[
    PRESET_MAX_STATES
];


/* -------------------------------------------------------------------------- */
/* Active MIDI and preset state                                               */
/* -------------------------------------------------------------------------- */

static uint8_t activeMidiProgram = 0U;

static uint16_t activeStateIndex =
    PRESET_INVALID_STATE_INDEX;


/* -------------------------------------------------------------------------- */
/* Deferred UI update                                                         */
/* -------------------------------------------------------------------------- */

static const PresetState *pendingUiState =
    NULL;

static volatile uint8_t uiUpdatePending =
    0U;


/* -------------------------------------------------------------------------- */
/* Internal helpers                                                           */
/* -------------------------------------------------------------------------- */

static uint8_t PresetManager_IsMidiBindingMatching(
    const PresetState *state,
    uint8_t channel,
    uint8_t programNumber,
    uint8_t controlNumber,
    uint8_t controlValue)
{
    if (state == NULL ||
        !state->active)
    {
        return 0U;
    }


    if (state->midi.channel !=
        channel)
    {
        return 0U;
    }


    if (state->midi.programNumber !=
        programNumber)
    {
        return 0U;
    }


    if (state->midi.controlNumber !=
        controlNumber)
    {
        return 0U;
    }


    /*
     * Exakter Wert oder Wildcard.
     */
    if (state->midi.controlValue !=
            PRESET_MIDI_VALUE_WILDCARD &&
        state->midi.controlValue !=
            controlValue)
    {
        return 0U;
    }


    return 1U;
}


/*
 * Lineare Suche.
 *
 * Bei maximal 32 Zuständen ist die Laufzeit klein
 * und vollständig deterministisch.
 *
 * Später kann pro Programm eine sortierte
 * Binding-Tabelle aufgebaut werden, ohne die
 * öffentliche Schnittstelle zu verändern.
 */
static uint16_t PresetManager_FindState(
    uint8_t channel,
    uint8_t programNumber,
    uint8_t controlNumber,
    uint8_t controlValue)
{
    /*
     * Exakte Zuordnung hat Vorrang vor Wildcard.
     */
    uint16_t wildcardIndex =
        PRESET_INVALID_STATE_INDEX;


    for (uint16_t i = 0U;
         i < PRESET_MAX_STATES;
         i++)
    {
        const PresetState *state =
            &presetStates[i];


        if (!PresetManager_IsMidiBindingMatching(
                state,
                channel,
                programNumber,
                controlNumber,
                controlValue))
        {
            continue;
        }


        if (state->midi.controlValue ==
            controlValue)
        {
            return i;
        }


        if (state->midi.controlValue ==
            PRESET_MIDI_VALUE_WILDCARD)
        {
            wildcardIndex = i;
        }
    }


    return wildcardIndex;
}


/* -------------------------------------------------------------------------- */
/* Initialization                                                             */
/* -------------------------------------------------------------------------- */

void PresetManager_Init(void)
{
    memset(
        presetStates,
        0,
        sizeof(presetStates)
    );


    activeMidiProgram = 0U;

    activeStateIndex =
        PRESET_INVALID_STATE_INDEX;


    pendingUiState = NULL;

    uiUpdatePending = 0U;
}


/* -------------------------------------------------------------------------- */
/* State management                                                           */
/* -------------------------------------------------------------------------- */

uint8_t PresetManager_StoreState(
    uint16_t stateIndex,
    const PresetState *state)
{
    if (state == NULL ||
        stateIndex >=
            PRESET_MAX_STATES)
    {
        return 0U;
    }


    /*
     * MIDI-Datenbytes dürfen nur Werte von
     * 0 bis 127 enthalten.
     *
     * PRESET_MIDI_VALUE_WILDCARD ist für
     * controlValue zusätzlich erlaubt.
     */
    if (state->midi.channel > 15U ||
        state->midi.programNumber > 127U ||
        state->midi.controlNumber > 127U)
    {
        return 0U;
    }


    if (state->midi.controlValue > 127U &&
        state->midi.controlValue !=
            PRESET_MIDI_VALUE_WILDCARD)
    {
        return 0U;
    }


    presetStates[stateIndex] =
        *state;


    presetStates[stateIndex].active = 1U;


    return 1U;
}


const PresetState *PresetManager_GetState(
    uint16_t stateIndex)
{
    if (stateIndex >=
        PRESET_MAX_STATES)
    {
        return NULL;
    }


    if (!presetStates[stateIndex].active)
    {
        return NULL;
    }


    return &presetStates[stateIndex];
}


/* -------------------------------------------------------------------------- */
/* Program Change                                                             */
/* -------------------------------------------------------------------------- */

void PresetManager_HandleProgramChange(
    uint8_t channel,
    uint8_t programNumber)
{
    if (channel > 15U ||
        programNumber > 127U)
    {
        return;
    }


    /*
     * Program Change ändert nur den Suchkontext
     * für den nächsten Control Change.
     *
     * Kein Audiozugriff.
     * Keine UI-Aktualisierung.
     */
    activeMidiProgram =
        programNumber;
}


/* -------------------------------------------------------------------------- */
/* Control Change                                                             */
/* -------------------------------------------------------------------------- */

void PresetManager_HandleControlChange(
    uint8_t channel,
    uint8_t controlNumber,
    uint8_t controlValue)
{
    if (channel > 15U ||
        controlNumber > 127U ||
        controlValue > 127U)
    {
        return;
    }


    uint16_t stateIndex =
        PresetManager_FindState(
            channel,
            activeMidiProgram,
            controlNumber,
            controlValue
        );


    if (stateIndex ==
        PRESET_INVALID_STATE_INDEX)
    {
        /*
         * Keine Zuordnung im aktiven Programm.
         *
         * Hörbarer und sichtbarer Zustand bleiben
         * unverändert.
         */
        return;
    }


    const PresetState *state =
        &presetStates[stateIndex];


    /*
     * ZEITKRITISCHER ABSCHNITT
     *
     * Die Audioschaltung wird zuerst 
