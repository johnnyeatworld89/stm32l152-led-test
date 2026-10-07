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
 * overwritten. Blocks while the EEPROM is written.
 *
 * Returns PRESET_STORE_ERR_FULL if the slot is new and all
 * PRESET_MAX_RECORDS records are in use.
 */
PresetStoreStatus PresetStore_SavePreset(
    uint8_t bank,
    uint8_t slot,
    const char *name,
    const uint8_t *layout,
    uint8_t layoutLength
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

#endif
