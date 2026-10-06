#include "preset_store.h"

#include <string.h>

/* -------------------------------------------------------------------------- */
/* EEPROM map                                                                 */
/* -------------------------------------------------------------------------- */

/*
 * 0x0000  bank names     64 * 16 bytes   (reserved, see GetBankName)
 * 0x0400  preset records 30 * 480 bytes
 */
#define STORE_EEPROM_BASE            0x08080000UL
#define STORE_EEPROM_SIZE            16384UL

#define STORE_BANK_NAMES_OFFSET      0UL
#define STORE_RECORDS_OFFSET \
    (STORE_BANK_NAMES_OFFSET + \
     (PRESET_BANK_COUNT * PRESET_BANK_NAME_LENGTH))

/*
 * Every record reserves 480 bytes. Only the first
 * RECORD_USED_SIZE bytes are used so far. The rest is reserved for
 * the switching plan, so that the record size does not have to change
 * (and existing presets stay valid) when the switching plan is added.
 */
#define RECORD_SIZE                  480UL

/* Record layout (all offsets are multiples of 4). */
#define RECORD_OFFSET_HEADER         0U     /* magic, bank, slot        */
#define RECORD_OFFSET_INFO           4U     /* version, layout length   */
#define RECORD_OFFSET_NAME           8U
#define RECORD_OFFSET_LAYOUT         24U
#define RECORD_OFFSET_LOOP_STATUS    216U   /* placeholder, written as 0 */
#define RECORD_USED_SIZE             220U
#define RECORD_USED_WORDS            (RECORD_USED_SIZE / 4U)

#define RECORD_MAGIC                 0x5052U
#define RECORD_VERSION               1U

_Static_assert(
    (RECORD_OFFSET_NAME + PRESET_NAME_LENGTH) ==
        RECORD_OFFSET_LAYOUT,
    "name block size does not match layout offset");

_Static_assert(
    (RECORD_OFFSET_LAYOUT + PRESET_LAYOUT_MAX_BYTES) ==
        RECORD_OFFSET_LOOP_STATUS,
    "layout block size does not match loop status offset");

_Static_assert(
    (RECORD_USED_SIZE % 4U) == 0U,
    "used record size must be a multiple of 4");

_Static_assert(
    (STORE_RECORDS_OFFSET +
     (PRESET_MAX_RECORDS * RECORD_SIZE)) <= STORE_EEPROM_SIZE,
    "preset records do not fit into the data EEPROM");


/* -------------------------------------------------------------------------- */
/* Helpers                                                                    */
/* -------------------------------------------------------------------------- */

static uint32_t Store_RecordAddress(uint8_t index)
{
    return STORE_EEPROM_BASE +
           STORE_RECORDS_OFFSET +
           ((uint32_t)index * RECORD_SIZE);
}


/*
 * Returns 1 if the record holds a valid preset and optionally
 * reports its bank and slot.
 */
static uint8_t Store_RecordIsValid(
    uint8_t index,
    uint8_t *bank,
    uint8_t *slot)
{
    const volatile uint8_t *record =
        (const volatile uint8_t *)Store_RecordAddress(index);

    uint16_t magic =
        (uint16_t)(record[0] | ((uint16_t)record[1] << 8));

    if (magic != RECORD_MAGIC ||
        record[RECORD_OFFSET_INFO] != RECORD_VERSION)
    {
        return 0U;
    }

    if (record[2] >= PRESET_BANK_COUNT ||
        record[3] >= PRESET_SLOTS_PER_BANK)
    {
        return 0U;
    }

    if (bank != NULL)
    {
        *bank = record[2];
    }

    if (slot != NULL)
    {
        *slot = record[3];
    }

    return 1U;
}


/* Returns the record index of a preset or -1. */
static int16_t Store_FindRecord(uint8_t bank, uint8_t slot)
{
    for (uint8_t i = 0U; i < PRESET_MAX_RECORDS; i++)
    {
        uint8_t recordBank;
        uint8_t recordSlot;

        if (Store_RecordIsValid(i, &recordBank, &recordSlot) &&
            recordBank == bank &&
            recordSlot == slot)
        {
            return (int16_t)i;
        }
    }

    return -1;
}


/* Returns the index of the first unused record or -1. */
static int16_t Store_FindFreeRecord(void)
{
    for (uint8_t i = 0U; i < PRESET_MAX_RECORDS; i++)
    {
        if (!Store_RecordIsValid(i, NULL, NULL))
        {
            return (int16_t)i;
        }
    }

    return -1;
}


/*
 * Writes one word. Words that already hold the value are skipped,
 * which saves time and EEPROM wear.
 */
static HAL_StatusTypeDef Store_WriteWord(
    uint32_t address,
    uint32_t value)
{
    if (*(const volatile uint32_t *)address == value)
    {
        return HAL_OK;
    }

    return HAL_FLASHEx_DATAEEPROM_Program(
        FLASH_TYPEPROGRAMDATA_WORD,
        address,
        value
    );
}


static void Store_AppendUInt(
    char *buffer,
    uint8_t *position,
    uint8_t value)
{
    char digits[3];
    uint8_t count = 0U;

    do
    {
        digits[count++] = (char)('0' + (value % 10U));
        value = (uint8_t)(value / 10U);
    }
    while (value != 0U && count < sizeof(digits));

    while (count > 0U)
    {
        buffer[(*position)++] = digits[--count];
    }
}


/* -------------------------------------------------------------------------- */
/* Public functions                                                           */
/* -------------------------------------------------------------------------- */

uint8_t PresetStore_GetSlotName(
    uint8_t bank,
    uint8_t slot,
    char *name)
{
    name[0] = '\0';

    if (bank >= PRESET_BANK_COUNT ||
        slot >= PRESET_SLOTS_PER_BANK)
    {
        return 0U;
    }

    int16_t index = Store_FindRecord(bank, slot);

    if (index < 0)
    {
        return 0U;
    }

    const volatile uint8_t *record =
        (const volatile uint8_t *)Store_RecordAddress(
            (uint8_t)index);

    for (uint8_t i = 0U; i < (PRESET_NAME_LENGTH - 1U); i++)
    {
        char c = (char)record[RECORD_OFFSET_NAME + i];

        if (c == '\0')
        {
            break;
        }

        name[i] = c;
        name[i + 1U] = '\0';
    }

    return 1U;
}


void PresetStore_GetBankName(
    uint8_t bank,
    char *name)
{
    if (bank >= PRESET_BANK_COUNT)
    {
        bank = 0U;
    }

    /*
     * A stored bank name is used if its first byte is a printable
     * character. Otherwise (erased EEPROM reads as 0) the default
     * name is generated.
     */
    const volatile uint8_t *stored =
        (const volatile uint8_t *)(STORE_EEPROM_BASE +
            STORE_BANK_NAMES_OFFSET +
            ((uint32_t)bank * PRESET_BANK_NAME_LENGTH));

    if (stored[0] >= 0x20U && stored[0] <= 0x7EU)
    {
        uint8_t length = 0U;

        while (length < (PRESET_BANK_NAME_LENGTH - 1U) &&
               stored[length] >= 0x20U &&
               stored[length] <= 0x7EU)
        {
            name[length] = (char)stored[length];
            length++;
        }

        name[length] = '\0';
        return;
    }

    uint8_t position = 0U;

    memcpy(name, "bank ", 5U);
    position = 5U;
    Store_AppendUInt(name, &position, bank);
    name[position] = '\0';
}


uint8_t PresetStore_GetUsedCount(void)
{
    uint8_t count = 0U;

    for (uint8_t i = 0U; i < PRESET_MAX_RECORDS; i++)
    {
        if (Store_RecordIsValid(i, NULL, NULL))
        {
            count++;
        }
    }

    return count;
}


PresetStoreStatus PresetStore_SavePreset(
    uint8_t bank,
    uint8_t slot,
    const char *name,
    const uint8_t *layout,
    uint8_t layoutLength)
{
    if (bank >= PRESET_BANK_COUNT ||
        slot >= PRESET_SLOTS_PER_BANK ||
        name == NULL ||
        layout == NULL ||
        layoutLength > PRESET_LAYOUT_MAX_BYTES)
    {
        return PRESET_STORE_ERR_PARAM;
    }

    int16_t index = Store_FindRecord(bank, slot);
    uint8_t overwriting = (index >= 0) ? 1U : 0U;

    if (!overwriting)
    {
        index = Store_FindFreeRecord();

        if (index < 0)
        {
            return PRESET_STORE_ERR_FULL;
        }
    }

    /*
     * Build the record in RAM.
     */
    uint32_t words[RECORD_USED_WORDS];
    uint8_t *bytes = (uint8_t *)words;

    memset(words, 0, sizeof(words));

    bytes[0] = (uint8_t)(RECORD_MAGIC & 0xFFU);
    bytes[1] = (uint8_t)(RECORD_MAGIC >> 8);
    bytes[2] = bank;
    bytes[3] = slot;
    bytes[RECORD_OFFSET_INFO] = RECORD_VERSION;
    bytes[RECORD_OFFSET_INFO + 1U] = layoutLength;

    for (uint8_t i = 0U; i < (PRESET_NAME_LENGTH - 1U); i++)
    {
        if (name[i] == '\0')
        {
            break;
        }

        bytes[RECORD_OFFSET_NAME + i] = (uint8_t)name[i];
    }

    memcpy(&bytes[RECORD_OFFSET_LAYOUT], layout, layoutLength);

    /*
     * Write sequence: invalidate the old record (word 0 = 0), write
     * all data words, then write word 0 with the valid marker last.
     * A power loss in between leaves an invalid record that is
     * ignored. In that case an overwritten preset is lost.
     */
    uint32_t address = Store_RecordAddress((uint8_t)index);

    if (HAL_FLASHEx_DATAEEPROM_Unlock() != HAL_OK)
    {
        return PRESET_STORE_ERR_WRITE;
    }

    HAL_StatusTypeDef status = HAL_OK;

    if (overwriting)
    {
        status = Store_WriteWord(address, 0U);
    }

    for (uint8_t i = 1U;
         i < RECORD_USED_WORDS && status == HAL_OK;
         i++)
    {
        status = Store_WriteWord(address + (4UL * i), words[i]);
    }

    if (status == HAL_OK)
    {
        status = Store_WriteWord(address, words[0]);
    }

    HAL_FLASHEx_DATAEEPROM_Lock();

    if (status != HAL_OK)
    {
        return PRESET_STORE_ERR_WRITE;
    }

    /*
     * Verify.
     */
    if (memcmp((const void *)address, words, RECORD_USED_SIZE) != 0)
    {
        return PRESET_STORE_ERR_WRITE;
    }

    return PRESET_STORE_OK;
}
