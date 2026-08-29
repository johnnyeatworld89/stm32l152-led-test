#include "ui.h"

#include "st7735.h"
#include "font5x7.h"

#include <string.h>


/* -------------------------------------------------------------------------- */
/* UI colors                                                                  */
/* -------------------------------------------------------------------------- */

/*
 * Custom RGB565 colors.
 */
#define UI_COLOR_SELECTED_BORDER    0x051F
#define UI_COLOR_GRABBED_BORDER     0xF81F
#define UI_COLOR_NORMAL_BORDER      0xFFFF

#define UI_COLOR_LOOP_OFF           0x0000
#define UI_COLOR_LOOP_CONFIRMED     0x07E0
#define UI_COLOR_LOOP_UNCONFIRMED   0xFFE0

#define UI_COLOR_BACKGROUND         0x0000
#define UI_COLOR_TEXT_LIGHT         0xFFFF
#define UI_COLOR_TEXT_DARK          0x0000


/* -------------------------------------------------------------------------- */
/* Test items                                                                 */
/* -------------------------------------------------------------------------- */

/*
 * These are temporary test items.
 *
 * Later this array will be supplied by the preset and routing model.
 */
static UI_Item uiItems[] =
{
    {
        .id = 1,
        .type = UI_ITEM_LOOP,
        .order = 0,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "L01",
        .longName = "Loop 01"
    },

    {
        .id = 2,
        .type = UI_ITEM_LOOP,
        .order = 1,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_ACTIVE_CONFIRMED,
        .focus = UI_FOCUS_SELECTED,
        .shortName = "L02",
        .longName = "Loop 02"
    },

    {
        .id = 3,
        .type = UI_ITEM_LOOP,
        .order = 2,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_ACTIVE_UNCONFIRMED,
        .focus = UI_FOCUS_NONE,
        .shortName = "L03",
        .longName = "Loop 03"
    },

    {
        .id = 4,
        .type = UI_ITEM_LOOP,
        .order = 3,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_ACTIVE_CONFIRMED,
        .focus = UI_FOCUS_GRABBED,
        .shortName = "L04",
        .longName = "Loop 04"
    }
};


#define UI_ITEM_COUNT \
    (sizeof(uiItems) / sizeof(uiItems[0]))


/* -------------------------------------------------------------------------- */
/* Layout                                                                     */
/* -------------------------------------------------------------------------- */

/*
 * Layout for the current 160 x 128 landscape display.
 *
 * Later another layout can be used for a larger display
 * without changing the item or routing model.
 */
static UI_Layout uiLayout =
{
    .displayWidth = 160,
    .displayHeight = 128,

    .marginLeft = 4,
    .marginRight = 4,
    .marginTop = 4,
    .marginBottom = 4,

    .normalItemWidth = 24,
    .selectedItemWidth = 48,
    .itemHeight = 24,

    .columnSpacing = 4,
    .laneSpacing = 8,

    .normalTextScale = 1,
    .selectedTextScale = 1
};


/* -------------------------------------------------------------------------- */
/* Internal functions                                                         */
/* -------------------------------------------------------------------------- */

static uint16_t UI_GetFillColor(
    const UI_Item *item)
{
    if (item == NULL)
    {
        return UI_COLOR_BACKGROUND;
    }


    if (item->type != UI_ITEM_LOOP)
    {
        return UI_COLOR_BACKGROUND;
    }


    switch (item->loopStatus)
    {
        case UI_LOOP_STATUS_ACTIVE_CONFIRMED:
            return UI_COLOR_LOOP_CONFIRMED;

        case UI_LOOP_STATUS_ACTIVE_UNCONFIRMED:
            return UI_COLOR_LOOP_UNCONFIRMED;

        case UI_LOOP_STATUS_OFF:
        default:
            return UI_COLOR_LOOP_OFF;
    }
}


static uint16_t UI_GetBorderColor(
    const UI_Item *item)
{
    if (item == NULL)
    {
        return UI_COLOR_NORMAL_BORDER;
    }


    switch (item->focus)
    {
        case UI_FOCUS_SELECTED:
            return UI_COLOR_SELECTED_BORDER;

        case UI_FOCUS_GRABBED:
            return UI_COLOR_GRABBED_BORDER;

        case UI_FOCUS_NONE:
        default:
            return UI_COLOR_NORMAL_BORDER;
    }
}


static uint16_t UI_GetTextColor(
    const UI_Item *item)
{
    if (item == NULL)
    {
        return UI_COLOR_TEXT_LIGHT;
    }


    /*
     * Black text is easier to read on the yellow fill.
     */
    if (item->loopStatus ==
        UI_LOOP_STATUS_ACTIVE_UNCONFIRMED)
    {
        return UI_COLOR_TEXT_DARK;
    }


    return UI_COLOR_TEXT_LIGHT;
}


static uint16_t UI_GetItemWidth(
    const UI_Item *item)
{
    if (item == NULL)
    {
        return uiLayout.normalItemWidth;
    }


    if (item->focus == UI_FOCUS_SELECTED ||
        item->focus == UI_FOCUS_GRABBED)
    {
        return uiLayout.selectedItemWidth;
    }


    return uiLayout.normalItemWidth;
}


static const char *UI_GetDisplayedName(
    const UI_Item *item)
{
    if (item == NULL)
    {
        return "";
    }


    if (item->focus == UI_FOCUS_SELECTED ||
        item->focus == UI_FOCUS_GRABBED)
    {
        return item->longName;
    }


    return item->shortName;
}


/* -------------------------------------------------------------------------- */
/* Draw a single rectangular item                                             */
/* -------------------------------------------------------------------------- */

static void UI_DrawBoxItem(
    const UI_Item *item,
    uint16_t x,
    uint16_t y)
{
    if (item == NULL)
    {
        return;
    }


    uint16_t width =
        UI_GetItemWidth(item);

    uint16_t height =
        uiLayout.itemHeight;

    uint16_t fillColor =
        UI_GetFillColor(item);

    uint16_t borderColor =
        UI_GetBorderColor(item);

    uint16_t textColor =
        UI_GetTextColor(item);

    const char *displayedName =
        UI_GetDisplayedName(item);


    /*
     * Fill item.
     */
    ST7735_FillRect(
        x,
        y,
        width,
        height,
        fillColor
    );


    /*
     * Draw outer border.
     */
    ST7735_DrawRect(
        x,
        y,
        width,
        height,
        borderColor
    );


    /*
     * Draw a second border for selected and grabbed items.
     * This makes the focus state easier to recognize.
     */
    if ((item->focus == UI_FOCUS_SELECTED ||
         item->focus == UI_FOCUS_GRABBED) &&
        width > 4 &&
        height > 4)
    {
        ST7735_DrawRect(
            x + 1,
            y + 1,
            width - 2,
            height - 2,
            borderColor
        );
    }


    uint8_t textScale =
        uiLayout.normalTextScale;


    if (item->focus == UI_FOCUS_SELECTED ||
        item->focus == UI_FOCUS_GRABBED)
    {
        textScale =
            uiLayout.selectedTextScale;
    }


    uint16_t textWidth =
        Font5x7_GetStringWidth(
            displayedName,
            textScale
        );

    uint16_t textHeight =
        Font5x7_GetHeight(
            textScale
        );


    uint16_t textX = x + 2;
    uint16_t textY = y + 2;


    if (textWidth < width)
    {
        textX =
            x +
            ((width - textWidth) / 2);
    }


    if (textHeight < height)
    {
        textY =
            y +
            ((height - textHeight) / 2);
    }


    Font5x7_DrawString(
        textX,
        textY,
        displayedName,
        textColor,
        fillColor,
        textScale
    );
}


/* -------------------------------------------------------------------------- */
/* Initialization                                                             */
/* -------------------------------------------------------------------------- */

void UI_Init(void)
{
    /*
     * No dynamic UI state is required yet.
     *
     * Later this function will initialize:
     *
     * selected item ID
     * grab state
     * horizontal scroll position
     * vertical scroll position
     * current preset
     */
}


/* -------------------------------------------------------------------------- */
/* Draw complete test UI                                                      */
/* -------------------------------------------------------------------------- */

void UI_Draw(void)
{
    ST7735_FillScreen(
        UI_COLOR_BACKGROUND
    );


    /*
     * The first implementation draws all test items
     * in one horizontal row.
     */
    uint16_t x =
        uiLayout.marginLeft;

    uint16_t y =
        (uiLayout.displayHeight -
         uiLayout.itemHeight) / 2;


    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        uint16_t itemWidth =
            UI_GetItemWidth(&uiItems[i]);


        /*
         * Stop before writing outside the display.
         *
         * Scrolling will be implemented later.
         */
        if ((x + itemWidth) >
            (uiLayout.displayWidth -
             uiLayout.marginRight))
        {
            break;
        }


        UI_DrawBoxItem(
            &uiItems[i],
            x,
            y
        );


        x +=
            itemWidth +
            uiLayout.columnSpacing;
    }
}
