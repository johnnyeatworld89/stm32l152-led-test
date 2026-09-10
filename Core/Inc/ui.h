#ifndef UI_H
#define UI_H

#include "stm32l1xx_hal.h"


/* -------------------------------------------------------------------------- */
/* Maximum name lengths                                                       */
/* -------------------------------------------------------------------------- */

/*
 * Loop short names:
 * Up to 3 visible characters.
 *
 * IN/OUT short names:
 * Up to 4 visible characters.
 *
 * We use 5 bytes so that all short names have room
 * for the terminating null character.
 */
#define UI_SHORT_NAME_LENGTH    5

/*
 * Long names:
 * Up to 10 visible characters plus null terminator.
 */
#define UI_LONG_NAME_LENGTH     11


/* -------------------------------------------------------------------------- */
/* Item types                                                                 */
/* -------------------------------------------------------------------------- */

typedef enum
{
    UI_ITEM_LOOP = 0,
    UI_ITEM_INPUT,
    UI_ITEM_OUTPUT,
    UI_ITEM_MANUAL_NODE,
    UI_ITEM_AUTO_NODE

} UI_ItemType;


/* -------------------------------------------------------------------------- */
/* Loop status                                                                */
/* -------------------------------------------------------------------------- */

typedef enum
{
    /*
     * Loop is switched off.
     */
    UI_LOOP_STATUS_OFF = 0,

    /*
     * Loop is active and the effect device provides
     * a confirmed active state.
     */
    UI_LOOP_STATUS_ACTIVE_CONFIRMED,

    /*
     * Loop is active, but no state feedback is available
     * from the effect device.
     */
    UI_LOOP_STATUS_ACTIVE_UNCONFIRMED

} UI_LoopStatus;


/* -------------------------------------------------------------------------- */
/* Focus state                                                                */
/* -------------------------------------------------------------------------- */

typedef enum
{
    /*
     * Item is neither selected nor grabbed.
     */
    UI_FOCUS_NONE = 0,

    /*
     * Item is selected.
     * The item is displayed with a blue border.
     */
    UI_FOCUS_SELECTED,

    /*
     * Item is grabbed for movement.
     * The item is displayed with a pink border.
     */
    UI_FOCUS_GRABBED

} UI_FocusState;


/* -------------------------------------------------------------------------- */
/* UI item                                                                    */
/* -------------------------------------------------------------------------- */

typedef struct
{
    /*
     * Unique and persistent item identifier.
     */
    uint16_t id;

    /*
     * Loop, input, output, manual node or automatic node.
     */
    UI_ItemType type;

    /*
     * Logical horizontal column.
     */
    int16_t order;

    /*
     * Logical vertical lane.
     */
    int16_t lane;

    /*
     * Loop operating state.
     *
     * This field is currently only relevant for loops.
     */
    UI_LoopStatus loopStatus;

    /*
     * Current interaction state.
     */
    UI_FocusState focus;

    /*
     * Short name displayed on non-selected items.
     *
     * Examples:
     * L01
     * In1
     * Out1
     */
    char shortName[UI_SHORT_NAME_LENGTH];

    /*
     * Long name reserved for the enlarged selected item.
     *
     * Examples:
     * Loop 01
     * Input 1
     * Output 1
     */
    char longName[UI_LONG_NAME_LENGTH];

} UI_Item;

/* -------------------------------------------------------------------------- */
/* UI connection                                                              */
/* -------------------------------------------------------------------------- */

typedef struct
{
    /*
     * ID of the source item.
     */
    uint16_t sourceId;

    /*
     * ID of the target item.
     */
    uint16_t targetId;

} UI_Connection;


/* -------------------------------------------------------------------------- */
/* Calculated screen geometry                                                 */
/* -------------------------------------------------------------------------- */

typedef struct
{
    /*
     * Item ID associated with this geometry.
     */
    uint16_t itemId;

    /*
     * Screen position and dimensions.
     */
    int16_t x;
    int16_t y;

    uint16_t width;
    uint16_t height;

    /*
     * True when the item is inside the current viewport.
     */
    uint8_t visible;

} UI_ItemGeometry;


/* -------------------------------------------------------------------------- */
/* Display-dependent layout configuration                                     */
/* -------------------------------------------------------------------------- */

typedef struct
{
    /*
     * Logical display size after rotation.
     */
    uint16_t displayWidth;
    uint16_t displayHeight;

    /*
     * Outer screen margins.
     */
    uint16_t marginLeft;
    uint16_t marginRight;
    uint16_t marginTop;
    uint16_t marginBottom;

    /*
     * Item dimensions.
     */
    uint16_t normalItemWidth;
    uint16_t selectedItemWidth;
    uint16_t itemHeight;

    /*
     * Spacing between logical columns and lanes.
     */
    uint16_t columnSpacing;
    uint16_t laneSpacing;

    /*
     * Text scale for compact and selected items.
     */
    uint8_t normalTextScale;
    uint8_t selectedTextScale;

} UI_Layout;


/* -------------------------------------------------------------------------- */
/* Public functions                                                           */
/* -------------------------------------------------------------------------- */

void UI_Init(void);

void UI_Draw(void);

#endif
