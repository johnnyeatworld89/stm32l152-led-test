#ifndef PRESET_STORE_H
#define PRESET_STORE_H

#include "stm32l1xx_hal.h"

/* -------------------------------------------------------------------------- */
/* Preset storage in the data EEPROM of the STM32L152RE                       */
/* -------------------------------------------------------------------------- */

/*
 * Logical preset space: 64 banks with 24 presets each.
 * Only PRESET_MAX_RECORDS of these 1536 slots can be stored in the
 * EEPROM at the same time. The rest stays "empty" until an external
 * memory is added.
 */
#define PRESET_BANK_COUNT            64U
#define PRESET_SLOTS_PER_BANK        24U
#define PRESET_MAX_RECORDS           30U

/* Name lengths including the terminating null character. */
#define PRESET_NAME_LENGTH           16U
#define PRESET_BANK_NAME_LENGTH      16U

/*
 * Maximum size of the layout block of one preset in bytes.
 * Block format (written by ui.c):
 *   [0]            number of entries
 *   [1 + 3 * n]    item id (uint8)
 *   [2 + 3 * n]    order   (int8)
 *   [3 + 3 * n]    lane    (int8)
 */
#define PRESET_LAYOUT_MAX_BYTES      192U

/*
 * Loop status stored with a preset: bit n = n-th loop (see ui.c),
 * at most 31 loops. PRESET_LOOP_STATUS_VALID marks a stored status;
 * presets saved before the loop status existed have 0 and leave the
 * loops unchanged when loaded.
 */
#define PRESET_LOOP_STATUS_VALID     0x80000000UL
#define PRESET_LOOP_STATUS_MAX_LOOPS 31U

/*
 * Programs of the effect devices stored with a preset (loops with
 * Program Change on, see loop_midi.h): one byte per loop, index n =
 * n-th loop as for the loop status. PRESET_LOOP_PROGRAM_VALID | program
 * (0 to 127) marks a stored program, 0 = none.
 */
#define PRESET_LOOP_PROGRAM_BYTES    32U
#define PRESET_LOOP_PROGRAM_VALID    0x80U

/*
 * Additional MIDI messages of a preset (edited in Manage Presets), sent
 * when the preset is loaded. A message is stored as it is sent: status
 * byte (0xB0 | channel = Control Change, 0xC0 | channel = Program
 * Change) and two data bytes (data2 = 0 for a Program Change).
 */
#define PRESET_MIDI_MAX_MESSAGES     16U

typedef struct
{
    uint8_t status;
    uint8_t data1;
    uint8_t data2;

} PresetMidiMessage;

/*
 * Size of the configuration block behind the preset records (loop
 * MIDI settings, see loop_midi.c).
 */
#define PRESET_CONFIG_MAX_BYTES      952U

typedef enum
{
    PRESET_STORE_OK = 0,
    PRESET_STORE_ERR_PARAM,
    PRESET_STORE_ERR_FULL,
    PRESET_STORE_ERR_WRITE,
    PRESET_STORE_ERR_NOT_FOUND

} PresetStoreStatus;


/*
 * Copies the name of the preset into name (at least
 * PRESET_NAME_LENGTH bytes). Returns 1 if the slot is used,
 * otherwise 0 (name is then an empty string).
 */
uint8_t PresetStore_GetSlotName(
    uint8_t bank,
    uint8_t slot,
    char *name
);

/*
 * Copies the bank name into name (at least PRESET_BANK_NAME_LENGTH
 * bytes). Without a stored name the default "bank <number>" is used.
 */
void PresetStore_GetBankName(
    uint8_t bank,
    char *name
);

/* Number of used records (0 to PRESET_MAX_RECORDS). */
uint8_t PresetStore_GetUsedCount(void);

/*
 * Stores a preset. An existing preset in the same bank and slot is
 * overwritten, its additional MIDI messages (see
 * PresetStore_WriteMidi) are kept. Blocks while the EEPROM is written.
 *
 * Returns PRESET_STORE_ERR_FULL if the slot is new and all
 * PRESET_MAX_RECORDS records are in use.
 */
PresetStoreStatus PresetStore_SavePreset(
    uint8_t bank,
    uint8_t slot,
    const char *name,
    const uint8_t *layout,
    uint8_t layoutLength,
    uint32_t loopStatus,
    const uint8_t *loopPrograms     /* PRESET_LOOP_PROGRAM_BYTES or NULL */
);

/*
 * Reads the layout block of a preset into layout (at least
 * PRESET_LAYOUT_MAX_BYTES bytes) and its length into length.
 * Returns PRESET_STORE_ERR_NOT_FOUND for an empty slot.
 */
PresetStoreStatus PresetStore_ReadLayout(
    uint8_t bank,
    uint8_t slot,
    uint8_t *layout,
    uint8_t *length
);

/*
 * Reads the loop status of a preset (see PRESET_LOOP_STATUS_VALID).
 * Returns PRESET_STORE_ERR_NOT_FOUND for an empty slot.
 */
PresetStoreStatus PresetStore_ReadLoopStatus(
    uint8_t bank,
    uint8_t slot,
    uint32_t *loopStatus
);

/*
 * Reads the loop programs of a preset into programs (at least
 * PRESET_LOOP_PROGRAM_BYTES bytes). Presets saved before the programs
 * existed return all 0. Returns PRESET_STORE_ERR_NOT_FOUND for an empty
 * slot.
 */
PresetStoreStatus PresetStore_ReadLoopPrograms(
    uint8_t bank,
    uint8_t slot,
    uint8_t *programs
);

/*
 * Reads the additional MIDI messages of a preset into messages (at
 * least PRESET_MIDI_MAX_MESSAGES entries) and their number into count.
 * Presets saved before the messages existed have none. Returns
 * PRESET_STORE_ERR_NOT_FOUND for an empty slot.
 */
PresetStoreStatus PresetStore_ReadMidiMessages(
    uint8_t bank,
    uint8_t slot,
    PresetMidiMessage *messages,
    uint8_t *count
);

/*
 * Replaces the loop programs (PRESET_LOOP_PROGRAM_BYTES) and the
 * additional MIDI messages of an existing preset. Layout, name and loop
 * status stay untouched. A power loss while writing leaves the preset
 * without programs or messages. Returns PRESET_STORE_ERR_NOT_FOUND for
 * an empty slot.
 */
PresetStoreStatus PresetStore_WriteMidi(
    uint8_t bank,
    uint8_t slot,
    const uint8_t *loopPrograms,
    const PresetMidiMessage *messages,
    uint8_t count
);

/*
 * Changes only the name of an existing preset. The layout stays
 * untouched. Returns PRESET_STORE_ERR_NOT_FOUND for an empty slot.
 */
PresetStoreStatus PresetStore_SetPresetName(
    uint8_t bank,
    uint8_t slot,
    const char *name
);

/*
 * Stores a bank name. An empty name restores the default
 * "bank <number>".
 */
PresetStoreStatus PresetStore_SetBankName(
    uint8_t bank,
    const char *name
);

/*
 * Deletes a preset and frees its record.
 * Returns PRESET_STORE_ERR_NOT_FOUND for an empty slot.
 */
PresetStoreStatus PresetStore_DeletePreset(
    uint8_t bank,
    uint8_t slot
);

/*
 * Configuration block (independent of the presets).
 *
 * ReadConfig copies the stored block into data (at least
 * PRESET_CONFIG_MAX_BYTES bytes). Returns PRESET_STORE_ERR_NOT_FOUND if
 * nothing valid is stored.
 *
 * WriteConfig replaces the block. Blocks while the EEPROM is written.
 * A power loss while writing leaves no valid block.
 */
PresetStoreStatus PresetStore_ReadConfig(
    uint8_t *data,
    uint16_t *length
);

PresetStoreStatus PresetStore_WriteConfig(
    const uint8_t *data,
    uint16_t length
);

#endif
