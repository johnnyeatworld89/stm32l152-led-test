#ifndef PRESET_MANAGER_H
#define PRESET_MANAGER_H


#include <stdint.h>


/* -------------------------------------------------------------------------- */
/* Configuration                                                              */
/* -------------------------------------------------------------------------- */

#define PRESET_MAX_STATES            32U

#define PRESET_MIDI_VALUE_WILDCARD   0xFFU
#define PRESET_INVALID_STATE_INDEX   0xFFFFU


/* -------------------------------------------------------------------------- */
/* Audio switch image                                                         */
/* -------------------------------------------------------------------------- */

/*
 * Vorberechnetes Hardware-Schaltbild.
 *
 * Die genaue Bedeutung der Felder wird später an
 * die konkrete Switcher-Hardware angepasst.
 *
 * Beim Abruf eines Presets darf aus der UI-Struktur
 * nichts mehr berechnet werden. Alle benötigten
 * Schaltinformationen müssen bereits hier vorliegen.
 */
typedef struct
{
    /*
     * Lokale STM32-GPIO-Ausgänge.
     */
    uint32_t localGpioSetMask;
    uint32_t localGpioResetMask;


    /*
     * Vorberechnete OLAT-Werte für bis zu acht
     * MCP23S17.
     *
     * Index 0:
     * erster MCP23S17
     *
     * [x][0\]:
     * Port A
     *
     * [x][1\]:
     * Port B
     */
    uint8_t mcp23s17Olat[8][2];

    uint8_t mcp23s17Count;


    /*
     * Reserve für spätere Analog-Switch-Matrix
     * beziehungsweise weitere Schalthardware.
     */
    uint32_t switchMatrixWords[4];

    uint8_t switchMatrixWordCount;

} PresetAudioImage;


/* -------------------------------------------------------------------------- */
/* MIDI binding                                                               */
/* -------------------------------------------------------------------------- */

typedef struct
{
    /*
     * MIDI-Kanal intern:
     *
     * 0 ... 15 entsprechen MIDI-Kanal 1 ... 16.
     */
    uint8_t channel;

    uint8_t programNumber;
    uint8_t controlNumber;

    /*
     * 0 ... 127:
     * exakter Vergleich
     *
     * PRESET_MIDI_VALUE_WILDCARD:
     * jeder CC-Wert löst den Zustand aus
     */
    uint8_t controlValue;

} PresetMidiBinding;


/* -------------------------------------------------------------------------- */
/* Stored state                                                               */
/* -------------------------------------------------------------------------- */

typedef struct
{
    uint8_t active;

    PresetMidiBinding midi;

    PresetAudioImage audioImage;


    /*
     * Referenz auf den gespeicherten UI-Zustand.
     *
     * Die eigentlichen UI-Daten werden im nächsten
     * Schritt ergänzt. Für den ersten Test reicht
     * eine eindeutige ID.
     */
    uint16_t uiStateId;

} PresetState;


/* -------------------------------------------------------------------------- */
/* Initialization                                                             */
/* -------------------------------------------------------------------------- */

void PresetManager_Init(void);


/* -------------------------------------------------------------------------- */
/* State management                                                           */
/* -------------------------------------------------------------------------- */

uint8_t PresetManager_StoreState(
    uint16_t stateIndex,
    const PresetState *state
);


const PresetState *PresetManager_GetState(
    uint16_t stateIndex
);


/* -------------------------------------------------------------------------- */
/* MIDI handling                                                              */
/* -------------------------------------------------------------------------- */

/*
 * Program Change wählt nur den MIDI-Programmkontext.
 *
 * Die Audioschaltung und die UI bleiben unverändert.
 */
void PresetManager_HandleProgramChange(
    uint8_t channel,
    uint8_t programNumber
);


/*
 * Control Change sucht im aktuellen MIDI-Programm
 * einen verknüpften Zustand.
 *
 * Bei Erfolg:
 *
 * 1. Audio sofort schalten
 * 2. UI-Aktualisierung vormerken
 */
void PresetManager_HandleControlChange(
    uint8_t channel,
    uint8_t controlNumber,
    uint8_t controlValue
);


/* -------------------------------------------------------------------------- */
/* Pending UI update                                                          */
/* -------------------------------------------------------------------------- */

uint8_t PresetManager_IsUiUpdatePending(void);


/*
 * Liefert den bereits hörbar aktivierten Zustand,
 * dessen UI noch geladen werden muss.
 *
 * Der Aufruf löscht das Pending-Flag.
 */
const PresetState *
PresetManager_TakePendingUiState(void);


/* -------------------------------------------------------------------------- */
/* Status                                                                     */
/* -------------------------------------------------------------------------- */

uint8_t PresetManager_GetActiveMidiProgram(void);

uint16_t PresetManager_GetActiveStateIndex(void);


/* -------------------------------------------------------------------------- */
/* Hardware hook                                                              */
/* -------------------------------------------------------------------------- */

/*
 * Diese Funktion wird beim passenden CC unmittelbar
 * aufgerufen.
 *
 * Sie muss:
 *
 * - kurz und deterministisch sein
 * - keinerlei Displayzugriff durchführen
 * - keine Pfeile berechnen
 * - keine dynamische Speicherverwaltung verwenden
 * - ausschließlich das vorberechnete Schaltbild
 *   an die Hardware ausgeben
 *
 * Die konkrete Implementierung folgt nach Definition
 * der finalen Audio-Schalthardware.
 */
void AudioSwitch_ApplyPresetImage(
    const PresetAudioImage *image
);


#endif /* PRESET_MANAGER_H */
