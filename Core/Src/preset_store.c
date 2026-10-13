#include "preset_store.h"

#include <string.h>

/* -------------------------------------------------------------------------- */
/* EEPROM map                                                                 */
/* -------------------------------------------------------------------------- */

/*
 * 0x0000  bank names     64 * 16 bytes   (reserved, see GetBankName)
 * 0x0400  preset records 30 * 480 bytes  (380 used, see RECORD_*)
 * 0x3C40  config block   960 bytes       (loop MIDI settings)
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
#define RECORD_OFFSET_LOOP_STATUS    216U   /* see PRESET_LOOP_STATUS_VALID */
#define RECORD_OFFSET_PROGRAM_MARKER 220U   /* RECORD_PROGRAM_MARKER        */
#define RECORD_OFFSET_PROGRAMS       224U   /* see PRESET_LOOP_PROGRAM_VALID */
#define RECORD_OFFSET_MIDI_HEADER    256U   /* RECORD_MIDI_MARKER, count    */
#define RECORD_OFFSET_MIDI           260U   /* PresetMidiMessage, 3 bytes   */
#define RECORD_MIDI_MESSAGE_SIZE     3U
#define RECORD_OFFSET_STATE_MARKER   308U   /* RECORD_STATE_MARKER          */
#define RECORD_OFFSET_STATE_MASK     312U   /* loops with a stored state    */
#define RECORD_OFFSET_STATES         316U   /* one word per loop in mask    */
#define RECORD_USED_SIZE             380U
#define RECORD_USED_WORDS            (RECORD_USED_SIZE / 4U)

#define RECORD_MAGIC                 0x5052U
#define RECORD_VERSION               1U

/*
 * Marks the loop programs as written. Records of older firmware have
 * anything there, so their programs are taken as "none".
 */
#define RECORD_PROGRAM_MARKER        0x0050474CUL   /* "LGP", 0 */

/*
 * Marks the additional MIDI messages as written (low 24 bits, "MSG"),
 * the high byte holds their number.
 */
#define RECORD_MIDI_MARKER           0x0047534DUL
#define RECORD_MIDI_MARKER_MASK      0x00FFFFFFUL

/* Marks the On/Bypass states as written. */
#define RECORD_STATE_MARKER          0x0054534CUL   /* "LST", 0 */

_Static_assert(
    (RECORD_OFFSET_NAME + PRESET_NAME_LENGTH) ==
        RECORD_OFFSET_LAYOUT,
    "name block size does not match layout offset");

_Static_assert(
    (RECORD_OFFSET_LAYOUT + PRESET_LAYOUT_MAX_BYTES) ==
        RECORD_OFFSET_LOOP_STATUS,
    "layout block size does not match loop status offset");

_Static_assert(
    (RECORD_OFFSET_PROGRAMS + PRESET_LOOP_PROGRAM_BYTES) ==
        RECORD_OFFSET_MIDI_HEADER,
    "loop program block size does not match MIDI header offset");

_Static_assert(
    (RECORD_OFFSET_MIDI +
     (PRESET_MIDI_MAX_MESSAGES * RECORD_MIDI_MESSAGE_SIZE)) ==
        RECORD_OFFSET_STATE_MARKER,
    "MIDI message block size does not match state offset");

_Static_assert(
    (RECORD_OFFSET_STATES + (4U * PRESET_LOOP_STATE_MAX_LOOPS)) ==
        RECORD_USED_SIZE,
    "state block size does not match used record size");

_Static_assert(
    RECORD_USED_SIZE <= RECORD_SIZE,
    "used record size exceeds the record size");

_Static_assert(
    (RECORD_USED_SIZE % 4U) == 0U,
    "used record size must be a multiple of 4");

_Static_assert(
    (STORE_RECORDS_OFFSET +
     (PRESET_MAX_RECORDS * RECORD_SIZE)) <= STORE_EEPROM_SIZE,
    "preset records do not fit into the data EEPROM");

/*
 * Config block behind the records:
 *   word 0   magic (16 bit), version, 0     valid marker, written last
 *   word 1   length (16 bit), checksum (16 bit, sum of the data bytes)
 *   data     PRESET_CONFIG_MAX_BYTES
 */
#define STORE_CONFIG_OFFSET \
    (STORE_RECORDS_OFFSET + (PRESET_MAX_RECORDS * RECORD_SIZE))
#define STORE_CONFIG_HEADER_SIZE     8UL
#define STORE_CONFIG_MAGIC           0x434DU
#define STORE_CONFIG_VERSION         1U

_Static_assert(
    (STORE_CONFIG_OFFSET + STORE_CONFIG_HEADER_SIZE +
     PRESET_CONFIG_MAX_BYTES) <= STORE_EEPROM_SIZE,
    "config block does not fit into the data EEPROM");

_Static_assert(
    (PRESET_CONFIG_MAX_BYTES % 4U) == 0U,
    "config block size must be a multiple of 4");


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


/* Returns 1 for a Control Change or Program Change with valid data. */
static uint8_t Store_MidiMessageIsValid(
    const PresetMidiMessage *message)
{
    uint8_t type = (uint8_t)(message->status & 0xF0U);

    if (message->data1 > 127U ||
        message->data2 > 127U)
    {
        return 0U;
    }

    if (type == 0xC0U)
    {
        return (message->data2 == 0U) ? 1U : 0U;
    }

    return (type == 0xB0U) ? 1U : 0U;
}


/*
 * Writes marker, mask and the states of the (at most
 * PRESET_LOOP_STATE_MAX_LOOPS lowest) loops in mask into the record
 * image. NULL stores no states.
 */
static void Store_PackStates(
    const PresetLoopStates *loopStates,
    uint32_t *words)
{
    uint32_t mask = 0UL;
    uint8_t count = 0U;

    words[RECORD_OFFSET_STATE_MARKER / 4U] = RECORD_STATE_MARKER;

    for (uint8_t n = 0U;
         loopStates != NULL &&
         n < PRESET_LOOP_STATUS_MAX_LOOPS &&
         count < PRESET_LOOP_STATE_MAX_LOOPS;
         n++)
    {
        if ((loopStates->mask & (1UL << n)) == 0UL)
        {
            continue;
        }

        mask |= (1UL << n);
        words[(RECORD_OFFSET_STATES / 4U) + count] = loopStates->state[n];
        count++;
    }

    words[RECORD_OFFSET_STATE_MASK / 4U] = mask;
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
    uint8_t layoutLength,
    uint32_t loopStatus,
    const uint8_t *loopPrograms,
    const PresetLoopStates *loopStates)
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

    /*
     * An overwritten preset keeps its additional MIDI messages.
     */
    if (overwriting)
    {
        const volatile uint8_t *record =
            (const volatile uint8_t *)Store_RecordAddress(
                (uint8_t)index);

        for (uint16_t i = RECORD_OFFSET_MIDI_HEADER;
             i < RECORD_OFFSET_STATE_MARKER;
             i++)
        {
            bytes[i] = record[i];
        }
    }

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

    words[RECORD_OFFSET_LOOP_STATUS / 4U] = loopStatus;
    words[RECORD_OFFSET_PROGRAM_MARKER / 4U] = RECORD_PROGRAM_MARKER;

    if (loopPrograms != NULL)
    {
        memcpy(&bytes[RECORD_OFFSET_PROGRAMS], loopPrograms,
               PRESET_LOOP_PROGRAM_BYTES);
    }

    Store_PackStates(loopStates, words);

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


/* -------------------------------------------------------------------------- */
/* Read layout, rename, delete                                                */
/* -------------------------------------------------------------------------- */

/*
 * Copies a name into 4 words, zero padded. Stops at the first
 * character that is not printable.
 */
static void Store_PackName(
    const char *name,
    uint32_t *words)
{
    uint8_t *bytes = (uint8_t *)words;

    memset(words, 0, PRESET_NAME_LENGTH);

    for (uint8_t i = 0U; i < (PRESET_NAME_LENGTH - 1U); i++)
    {
        uint8_t c = (uint8_t)name[i];

        if (c < 0x20U || c > 0x7EU)
        {
            break;
        }

        bytes[i] = c;
    }
}


/*
 * Writes the 4 words of a name block and verifies them.
 */
static PresetStoreStatus Store_WriteNameBlock(
    uint32_t address,
    const uint32_t *words)
{
    if (HAL_FLASHEx_DATAEEPROM_Unlock() != HAL_OK)
    {
        return PRESET_STORE_ERR_WRITE;
    }

    HAL_StatusTypeDef status = HAL_OK;

    for (uint8_t i = 0U;
         i < (PRESET_NAME_LENGTH / 4U) && status == HAL_OK;
         i++)
    {
        status = Store_WriteWord(address + (4UL * i), words[i]);
    }

    HAL_FLASHEx_DATAEEPROM_Lock();

    if (status != HAL_OK ||
        memcmp((const void *)address, words, PRESET_NAME_LENGTH) != 0)
    {
        return PRESET_STORE_ERR_WRITE;
    }

    return PRESET_STORE_OK;
}


PresetStoreStatus PresetStore_ReadLayout(
    uint8_t bank,
    uint8_t slot,
    uint8_t *layout,
    uint8_t *length)
{
    if (bank >= PRESET_BANK_COUNT ||
        slot >= PRESET_SLOTS_PER_BANK ||
        layout == NULL ||
        length == NULL)
    {
        return PRESET_STORE_ERR_PARAM;
    }

    int16_t index = Store_FindRecord(bank, slot);

    if (index < 0)
    {
        return PRESET_STORE_ERR_NOT_FOUND;
    }

    const volatile uint8_t *record =
        (const volatile uint8_t *)Store_RecordAddress(
            (uint8_t)index);

    uint8_t layoutLength = record[RECORD_OFFSET_INFO + 1U];

    if (layoutLength > PRESET_LAYOUT_MAX_BYTES)
    {
        return PRESET_STORE_ERR_PARAM;
    }

    for (uint8_t i = 0U; i < layoutLength; i++)
    {
        layout[i] = record[RECORD_OFFSET_LAYOUT + i];
    }

    *length = layoutLength;

    return PRESET_STORE_OK;
}


PresetStoreStatus PresetStore_ReadLoopStatus(
    uint8_t bank,
    uint8_t slot,
    uint32_t *loopStatus)
{
    if (bank >= PRESET_BANK_COUNT ||
        slot >= PRESET_SLOTS_PER_BANK ||
        loopStatus == NULL)
    {
        return PRESET_STORE_ERR_PARAM;
    }

    int16_t index = Store_FindRecord(bank, slot);

    if (index < 0)
    {
        return PRESET_STORE_ERR_NOT_FOUND;
    }

    *loopStatus = *(const volatile uint32_t *)(
        Store_RecordAddress((uint8_t)index) +
        RECORD_OFFSET_LOOP_STATUS);

    return PRESET_STORE_OK;
}


PresetStoreStatus PresetStore_ReadLoopPrograms(
    uint8_t bank,
    uint8_t slot,
    uint8_t *programs)
{
    if (bank >= PRESET_BANK_COUNT ||
        slot >= PRESET_SLOTS_PER_BANK ||
        programs == NULL)
    {
        return PRESET_STORE_ERR_PARAM;
    }

    int16_t index = Store_FindRecord(bank, slot);

    if (index < 0)
    {
        return PRESET_STORE_ERR_NOT_FOUND;
    }

    uint32_t address = Store_RecordAddress((uint8_t)index);

    if (*(const volatile uint32_t *)(
            address + RECORD_OFFSET_PROGRAM_MARKER) !=
        RECORD_PROGRAM_MARKER)
    {
        memset(programs, 0, PRESET_LOOP_PROGRAM_BYTES);
        return PRESET_STORE_OK;
    }

    const volatile uint8_t *record =
        (const volatile uint8_t *)(address + RECORD_OFFSET_PROGRAMS);

    for (uint8_t i = 0U; i < PRESET_LOOP_PROGRAM_BYTES; i++)
    {
        programs[i] = record[i];
    }

    return PRESET_STORE_OK;
}


PresetStoreStatus PresetStore_ReadLoopStates(
    uint8_t bank,
    uint8_t slot,
    PresetLoopStates *loopStates)
{
    if (bank >= PRESET_BANK_COUNT ||
        slot >= PRESET_SLOTS_PER_BANK ||
        loopStates == NULL)
    {
        return PRESET_STORE_ERR_PARAM;
    }

    memset(loopStates, 0, sizeof(*loopStates));

    int16_t index = Store_FindRecord(bank, slot);

    if (index < 0)
    {
        return PRESET_STORE_ERR_NOT_FOUND;
    }

    uint32_t address = Store_RecordAddress((uint8_t)index);

    if (*(const volatile uint32_t *)(
            address + RECORD_OFFSET_STATE_MARKER) != RECORD_STATE_MARKER)
    {
        return PRESET_STORE_OK;
    }

    uint32_t mask = *(const volatile uint32_t *)(
        address + RECORD_OFFSET_STATE_MASK);
    uint8_t count = 0U;

    for (uint8_t n = 0U;
         n < PRESET_LOOP_STATUS_MAX_LOOPS &&
         count < PRESET_LOOP_STATE_MAX_LOOPS;
         n++)
    {
        if ((mask & (1UL << n)) == 0UL)
        {
            continue;
        }

        loopStates->mask |= (1UL << n);
        loopStates->state[n] = *(const volatile uint32_t *)(
            address + RECORD_OFFSET_STATES + (4UL * count));
        count++;
    }

    return PRESET_STORE_OK;
}


PresetStoreStatus PresetStore_ReadMidiMessages(
    uint8_t bank,
    uint8_t slot,
    PresetMidiMessage *messages,
    uint8_t *count)
{
    if (bank >= PRESET_BANK_COUNT ||
        slot >= PRESET_SLOTS_PER_BANK ||
        messages == NULL ||
        count == NULL)
    {
        return PRESET_STORE_ERR_PARAM;
    }

    *count = 0U;

    int16_t index = Store_FindRecord(bank, slot);

    if (index < 0)
    {
        return PRESET_STORE_ERR_NOT_FOUND;
    }

    uint32_t address = Store_RecordAddress((uint8_t)index);
    uint32_t header = *(const volatile uint32_t *)(
        address + RECORD_OFFSET_MIDI_HEADER);
    uint8_t stored = (uint8_t)(header >> 24);

    if ((header & RECORD_MIDI_MARKER_MASK) != RECORD_MIDI_MARKER ||
        stored > PRESET_MIDI_MAX_MESSAGES)
    {
        return PRESET_STORE_OK;
    }

    const volatile uint8_t *record =
        (const volatile uint8_t *)(address + RECORD_OFFSET_MIDI);

    for (uint8_t i = 0U; i < stored; i++)
    {
        PresetMidiMessage message;

        message.status = record[(RECORD_MIDI_MESSAGE_SIZE * i)];
        message.data1 = record[(RECORD_MIDI_MESSAGE_SIZE * i) + 1U];
        message.data2 = record[(RECORD_MIDI_MESSAGE_SIZE * i) + 2U];

        if (Store_MidiMessageIsValid(&message))
        {
            messages[(*count)++] = message;
        }
    }

    return PRESET_STORE_OK;
}


PresetStoreStatus PresetStore_WriteMidi(
    uint8_t bank,
    uint8_t slot,
    uint32_t loopStatus,
    const uint8_t *loopPrograms,
    const PresetLoopStates *loopStates,
    const PresetMidiMessage *messages,
    uint8_t count)
{
    if (bank >= PRESET_BANK_COUNT ||
        slot >= PRESET_SLOTS_PER_BANK ||
        loopPrograms == NULL ||
        (messages == NULL && count > 0U) ||
        count > PRESET_MIDI_MAX_MESSAGES)
    {
        return PRESET_STORE_ERR_PARAM;
    }

    for (uint8_t i = 0U; i < count; i++)
    {
        if (!Store_MidiMessageIsValid(&messages[i]))
        {
            return PRESET_STORE_ERR_PARAM;
        }
    }

    int16_t index = Store_FindRecord(bank, slot);

    if (index < 0)
    {
        return PRESET_STORE_ERR_NOT_FOUND;
    }

    /*
     * Image of the record from the loop status to the end of the used
     * part.
     */
    uint32_t words[RECORD_USED_WORDS];
    uint8_t *bytes = (uint8_t *)words;

    memset(words, 0, sizeof(words));

    words[RECORD_OFFSET_LOOP_STATUS / 4U] = loopStatus;
    words[RECORD_OFFSET_PROGRAM_MARKER / 4U] = RECORD_PROGRAM_MARKER;
    memcpy(&bytes[RECORD_OFFSET_PROGRAMS], loopPrograms,
           PRESET_LOOP_PROGRAM_BYTES);

    words[RECORD_OFFSET_MIDI_HEADER / 4U] =
        RECORD_MIDI_MARKER | ((uint32_t)count << 24);

    for (uint8_t i = 0U; i < count; i++)
    {
        uint16_t offset = (uint16_t)(RECORD_OFFSET_MIDI +
            (RECORD_MIDI_MESSAGE_SIZE * i));

        bytes[offset] = messages[i].status;
        bytes[offset + 1U] = messages[i].data1;
        bytes[offset + 2U] = messages[i].data2;
    }

    Store_PackStates(loopStates, words);

    /*
     * Write sequence: clear the markers, write the data, write the
     * markers last.
     */
    uint32_t address = Store_RecordAddress((uint8_t)index);

    if (HAL_FLASHEx_DATAEEPROM_Unlock() != HAL_OK)
    {
        return PRESET_STORE_ERR_WRITE;
    }

    HAL_StatusTypeDef status = Store_WriteWord(
        address + RECORD_OFFSET_PROGRAM_MARKER, 0U);

    if (status == HAL_OK)
    {
        status = Store_WriteWord(
            address + RECORD_OFFSET_MIDI_HEADER, 0U);
    }

    if (status == HAL_OK)
    {
        status = Store_WriteWord(
            address + RECORD_OFFSET_STATE_MARKER, 0U);
    }

    for (uint16_t offset = RECORD_OFFSET_LOOP_STATUS;
         offset < RECORD_USED_SIZE && status == HAL_OK;
         offset = (uint16_t)(offset + 4U))
    {
        if (offset == RECORD_OFFSET_PROGRAM_MARKER ||
            offset == RECORD_OFFSET_MIDI_HEADER ||
            offset == RECORD_OFFSET_STATE_MARKER)
        {
            continue;
        }

        status = Store_WriteWord(address + offset, words[offset / 4U]);
    }

    if (status == HAL_OK)
    {
        status = Store_WriteWord(
            address + RECORD_OFFSET_STATE_MARKER,
            words[RECORD_OFFSET_STATE_MARKER / 4U]);
    }

    if (status == HAL_OK)
    {
        status = Store_WriteWord(
            address + RECORD_OFFSET_MIDI_HEADER,
            words[RECORD_OFFSET_MIDI_HEADER / 4U]);
    }

    if (status == HAL_OK)
    {
        status = Store_WriteWord(
            address + RECORD_OFFSET_PROGRAM_MARKER,
            RECORD_PROGRAM_MARKER);
    }

    HAL_FLASHEx_DATAEEPROM_Lock();

    if (status != HAL_OK ||
        memcmp((const void *)(address + RECORD_OFFSET_LOOP_STATUS),
               &bytes[RECORD_OFFSET_LOOP_STATUS],
               RECORD_USED_SIZE - RECORD_OFFSET_LOOP_STATUS) != 0)
    {
        return PRESET_STORE_ERR_WRITE;
    }

    return PRESET_STORE_OK;
}


PresetStoreStatus PresetStore_SetPresetName(
    uint8_t bank,
    uint8_t slot,
    const char *name)
{
    if (bank >= PRESET_BANK_COUNT ||
        slot >= PRESET_SLOTS_PER_BANK ||
        name == NULL)
    {
        return PRESET_STORE_ERR_PARAM;
    }

    int16_t index = Store_FindRecord(bank, slot);

    if (index < 0)
    {
        return PRESET_STORE_ERR_NOT_FOUND;
    }

    uint32_t words[PRESET_NAME_LENGTH / 4U];

    Store_PackName(name, words);

    return Store_WriteNameBlock(
        Store_RecordAddress((uint8_t)index) + RECORD_OFFSET_NAME,
        words);
}


PresetStoreStatus PresetStore_SetBankName(
    uint8_t bank,
    const char *name)
{
    if (bank >= PRESET_BANK_COUNT ||
        name == NULL)
    {
        return PRESET_STORE_ERR_PARAM;
    }

    uint32_t words[PRESET_BANK_NAME_LENGTH / 4U];

    Store_PackName(name, words);

    return Store_WriteNameBlock(
        STORE_EEPROM_BASE +
            STORE_BANK_NAMES_OFFSET +
            ((uint32_t)bank * PRESET_BANK_NAME_LENGTH),
        words);
}


PresetStoreStatus PresetStore_DeletePreset(
    uint8_t bank,
    uint8_t slot)
{
    if (bank >= PRESET_BANK_COUNT ||
        slot >= PRESET_SLOTS_PER_BANK)
    {
        return PRESET_STORE_ERR_PARAM;
    }

    int16_t index = Store_FindRecord(bank, slot);

    if (index < 0)
    {
        return PRESET_STORE_ERR_NOT_FOUND;
    }

    uint32_t address = Store_RecordAddress((uint8_t)index);

    if (HAL_FLASHEx_DATAEEPROM_Unlock() != HAL_OK)
    {
        return PRESET_STORE_ERR_WRITE;
    }

    /*
     * Word 0 holds the valid marker. Clearing it frees the record.
     */
    HAL_StatusTypeDef status = Store_WriteWord(address, 0U);

    HAL_FLASHEx_DATAEEPROM_Lock();

    if (status != HAL_OK ||
        Store_RecordIsValid((uint8_t)index, NULL, NULL))
    {
        return PRESET_STORE_ERR_WRITE;
    }

    return PRESET_STORE_OK;
}


/* -------------------------------------------------------------------------- */
/* Config block                                                               */
/* -------------------------------------------------------------------------- */

static uint16_t Store_Checksum(
    const uint8_t *data,
    uint16_t length)
{
    uint16_t sum = 0U;

    for (uint16_t i = 0U; i < length; i++)
    {
        sum = (uint16_t)(sum + data[i]);
    }

    return sum;
}


PresetStoreStatus PresetStore_ReadConfig(
    uint8_t *data,
    uint16_t *length)
{
    if (data == NULL || length == NULL)
    {
        return PRESET_STORE_ERR_PARAM;
    }

    const volatile uint8_t *block =
        (const volatile uint8_t *)(STORE_EEPROM_BASE +
                                   STORE_CONFIG_OFFSET);

    uint16_t magic =
        (uint16_t)(block[0] | ((uint16_t)block[1] << 8));
    uint16_t blockLength =
        (uint16_t)(block[4] | ((uint16_t)block[5] << 8));
    uint16_t checksum =
        (uint16_t)(block[6] | ((uint16_t)block[7] << 8));

    if (magic != STORE_CONFIG_MAGIC ||
        block[2] != STORE_CONFIG_VERSION ||
        blockLength > PRESET_CONFIG_MAX_BYTES)
    {
        return PRESET_STORE_ERR_NOT_FOUND;
    }

    for (uint16_t i = 0U; i < blockLength; i++)
    {
        data[i] = block[STORE_CONFIG_HEADER_SIZE + i];
    }

    if (Store_Checksum(data, blockLength) != checksum)
    {
        return PRESET_STORE_ERR_NOT_FOUND;
    }

    *length = blockLength;

    return PRESET_STORE_OK;
}


PresetStoreStatus PresetStore_WriteConfig(
    const uint8_t *data,
    uint16_t length)
{
    if ((data == NULL && length > 0U) ||
        length > PRESET_CONFIG_MAX_BYTES)
    {
        return PRESET_STORE_ERR_PARAM;
    }

    uint32_t address =
        STORE_EEPROM_BASE + STORE_CONFIG_OFFSET;

    uint32_t header0 =
        (uint32_t)STORE_CONFIG_MAGIC |
        ((uint32_t)STORE_CONFIG_VERSION << 16);

    uint32_t header1 =
        (uint32_t)length |
        ((uint32_t)Store_Checksum(data, length) << 16);

    if (HAL_FLASHEx_DATAEEPROM_Unlock() != HAL_OK)
    {
        return PRESET_STORE_ERR_WRITE;
    }

    /*
     * Same sequence as for the records: invalidate, write the data,
     * write the valid marker last. Words are assembled byte by byte,
     * so data does not need to be aligned.
     */
    HAL_StatusTypeDef status = Store_WriteWord(address, 0U);

    for (uint16_t offset = 0U;
         offset < length && status == HAL_OK;
         offset = (uint16_t)(offset + 4U))
    {
        uint32_t word = 0U;

        for (uint8_t i = 0U;
             i < 4U && (offset + i) < length;
             i++)
        {
            word |= (uint32_t)data[offset + i] << (8U * i);
        }

        status = Store_WriteWord(
            address + STORE_CONFIG_HEADER_SIZE + offset, word);
    }

    if (status == HAL_OK)
    {
        status = Store_WriteWord(address + 4UL, header1);
    }

    if (status == HAL_OK)
    {
        status = Store_WriteWord(address, header0);
    }

    HAL_FLASHEx_DATAEEPROM_Lock();

    if (status != HAL_OK ||
        (length > 0U &&
         memcmp((const void *)(address + STORE_CONFIG_HEADER_SIZE),
                data, length) != 0))
    {
        return PRESET_STORE_ERR_WRITE;
    }

    return PRESET_STORE_OK;
}
