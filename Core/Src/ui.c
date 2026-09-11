#include "ui.h"

#include "st7735.h"
#include "font5x7.h"

/* -------------------------------------------------------------------------- */
/* Colors                                                                     */
/* -------------------------------------------------------------------------- */

#define UI_COLOR_BACKGROUND          0x0000
#define UI_COLOR_TEXT_LIGHT          0xFFFF
#define UI_COLOR_TEXT_DARK           0x0000

#define UI_COLOR_BORDER_NORMAL       0xFFFF
#define UI_COLOR_BORDER_SELECTED     0x051F
#define UI_COLOR_BORDER_GRABBED      0xF81F

#define UI_COLOR_LOOP_OFF            0x0000
#define UI_COLOR_LOOP_CONFIRMED      0x07E0
#define UI_COLOR_LOOP_UNCONFIRMED    0xFFE0

#define UI_COLOR_CONNECTION          0xFFFF
#define UI_COLOR_AUTO_NODE           0xFFFF

/* -------------------------------------------------------------------------- */
/* Display and grid                                                           */
/* -------------------------------------------------------------------------- */

#define UI_DISPLAY_WIDTH             160
#define UI_DISPLAY_HEIGHT            128

#define UI_VISIBLE_ELEMENT_COLUMNS   4
#define UI_VISIBLE_LANES             3

#define UI_ELEMENT_WIDTH             24
#define UI_ELEMENT_HEIGHT            24

#define UI_CONNECTION_WIDTH          18
#define UI_VERTICAL_CONNECTION_HEIGHT 16

#define UI_GRID_CONTENT_WIDTH        \
    ((UI_VISIBLE_ELEMENT_COLUMNS * UI_ELEMENT_WIDTH) + \
     ((UI_VISIBLE_ELEMENT_COLUMNS - 1) * UI_CONNECTION_WIDTH))

#define UI_GRID_CONTENT_HEIGHT       \
    ((UI_VISIBLE_LANES * UI_ELEMENT_HEIGHT) + \
     ((UI_VISIBLE_LANES - 1) * UI_VERTICAL_CONNECTION_HEIGHT))

#define UI_GRID_LEFT                 \
    ((UI_DISPLAY_WIDTH - UI_GRID_CONTENT_WIDTH) / 2)

#define UI_GRID_TOP                  2

#define UI_FOOTER_TOP                \
    (UI_GRID_TOP + UI_GRID_CONTENT_HEIGHT + 2)

#define UI_FOOTER_HEIGHT             \
    (UI_DISPLAY_HEIGHT - UI_FOOTER_TOP)

#define UI_IO_CIRCLE_RADIUS          7
#define UI_MANUAL_NODE_RADIUS        6
#define UI_AUTO_NODE_RADIUS          3

/* -------------------------------------------------------------------------- */
/* Temporary test data                                                        */
/* -------------------------------------------------------------------------- */

static UI_Item uiItems[] =
{
    {
        .id = 1,
        .type = UI_ITEM_INPUT,
        .order = 0,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "In1",
        .longName = "Input 1"
    },

    {
        .id = 2,
        .type = UI_ITEM_LOOP,
        .order = 1,
        .lane = 1,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "L01",
        .longName = "Loop 01"
    },

    {
        .id = 3,
        .type = UI_ITEM_LOOP,
        .order = 1,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_ACTIVE_CONFIRMED,
        .focus = UI_FOCUS_SELECTED,
        .shortName = "L02",
        .longName = "Loop 02"
    },

    {
        .id = 4,
        .type = UI_ITEM_LOOP,
        .order = 2,
        .lane = 1,
        .loopStatus = UI_LOOP_STATUS_ACTIVE_UNCONFIRMED,
        .focus = UI_FOCUS_NONE,
        .shortName = "L03",
        .longName = "Loop 03"
    },

    {
        .id = 5,
        .type = UI_ITEM_LOOP,
        .order = 2,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "L04",
        .longName = "Loop 04"
    },

    {
        .id = 6,
        .type = UI_ITEM_OUTPUT,
        .order = 3,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "Out1",
        .longName = "Output 1"
    }
};

#define UI_ITEM_COUNT \
    (sizeof(uiItems) / sizeof(uiItems[0]))

static UI_Connection uiConnections[] =
{
    /* Aufteilung hinter IN1 */

    {
        .sourceId = 1,
        .targetId = 2
    },

    {
        .sourceId = 1,
        .targetId = 3
    },


    /* Zwei parallele Signalwege */

    {
        .sourceId = 2,
        .targetId = 4
    },

    {
        .sourceId = 3,
        .targetId = 5
    },


    /* Zusammenführung vor OUT1 */

    {
        .sourceId = 4,
        .targetId = 6
    },

    {
        .sourceId = 5,
        .targetId = 6
    }
};

#define UI_CONNECTION_COUNT \
    (sizeof(uiConnections) / sizeof(uiConnections[0]))

static UI_ItemGeometry uiGeometry[UI_ITEM_COUNT];

/* -------------------------------------------------------------------------- */
/* Primitive shapes                                                           */
/* -------------------------------------------------------------------------- */

static void UI_DrawCircle(
    int16_t centerX,
    int16_t centerY,
    int16_t radius,
    uint16_t color)
{
    int16_t x = radius;
    int16_t y = 0;
    int16_t error = 1 - radius;

    while (x >= y)
    {
        ST7735_DrawPixel(centerX + x, centerY + y, color);
        ST7735_DrawPixel(centerX + y, centerY + x, color);
        ST7735_DrawPixel(centerX - y, centerY + x, color);
        ST7735_DrawPixel(centerX - x, centerY + y, color);
        ST7735_DrawPixel(centerX - x, centerY - y, color);
        ST7735_DrawPixel(centerX - y, centerY - x, color);
        ST7735_DrawPixel(centerX + y, centerY - x, color);
        ST7735_DrawPixel(centerX + x, centerY - y, color);

        y++;

        if (error < 0)
        {
            error += (2 * y) + 1;
        }
        else
        {
            x--;
            error += (2 * (y - x)) + 1;
        }
    }
}

static void UI_FillCircle(
    int16_t centerX,
    int16_t centerY,
    int16_t radius,
    uint16_t color)
{
    for (int16_t y = -radius; y <= radius; y++)
    {
        for (int16_t x = -radius; x <= radius; x++)
        {
            if ((x * x) + (y * y) <= (radius * radius))
            {
                ST7735_DrawPixel(centerX + x, centerY + y, color);
            }
        }
    }
}

/* -------------------------------------------------------------------------- */
/* Item helpers                                                               */
/* -------------------------------------------------------------------------- */

static uint16_t UI_GetFillColor(const UI_Item *item)
{
    if (item == NULL || item->type != UI_ITEM_LOOP)
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

static uint16_t UI_GetBorderColor(const UI_Item *item)
{
    if (item == NULL)
    {
        return UI_COLOR_BORDER_NORMAL;
    }

    switch (item->focus)
    {
        case UI_FOCUS_SELECTED:
            return UI_COLOR_BORDER_SELECTED;

        case UI_FOCUS_GRABBED:
            return UI_COLOR_BORDER_GRABBED;

        case UI_FOCUS_NONE:
        default:
            return UI_COLOR_BORDER_NORMAL;
    }
}

static uint16_t UI_GetTextColor(const UI_Item *item)
{
    if (item != NULL &&
        item->type == UI_ITEM_LOOP &&
        item->loopStatus == UI_LOOP_STATUS_ACTIVE_UNCONFIRMED)
    {
        return UI_COLOR_TEXT_DARK;
    }

    return UI_COLOR_TEXT_LIGHT;
}

static void UI_DrawCenteredText(
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height,
    const char *text,
    uint16_t foreground,
    uint16_t background,
    uint8_t scale)
{
    if (text == NULL)
    {
        return;
    }

    uint16_t textWidth =
        Font5x7_GetStringWidth(text, scale);

    uint16_t textHeight =
        Font5x7_GetHeight(scale);

    uint16_t textX = x;
    uint16_t textY = y;

    if (textWidth < width)
    {
        textX = x + ((width - textWidth) / 2);
    }

    if (textHeight < height)
    {
        textY = y + ((height - textHeight) / 2);
    }

    Font5x7_DrawString(
        textX,
        textY,
        text,
        foreground,
        background,
        scale);
}

/* -------------------------------------------------------------------------- */
/* Grid conversion                                                            */
/* -------------------------------------------------------------------------- */

static int16_t UI_GetElementX(int16_t visibleOrder)
{
    return
        UI_GRID_LEFT +
        (visibleOrder *
         (UI_ELEMENT_WIDTH + UI_CONNECTION_WIDTH));
}

static int16_t UI_GetLaneY(int16_t lane)
{
    /*
     * Visible lanes:
     * +1 = upper lane
     *  0 = center lane
     * -1 = lower lane
     */
    int16_t laneIndex = 1 - lane;

    return
        UI_GRID_TOP +
        (laneIndex *
         (UI_ELEMENT_HEIGHT + UI_VERTICAL_CONNECTION_HEIGHT));
}

static uint8_t UI_IsLaneVisible(int16_t lane)
{
    return (lane >= -1 && lane <= 1) ? 1 : 0;
}

/* -------------------------------------------------------------------------- */
/* Geometry                                                                   */
/* -------------------------------------------------------------------------- */

static void UI_CalculateGeometry(void)
{
    for (uint16_t i = 0; i < UI_ITEM_COUNT; i++)
    {
        uiGeometry[i].itemId = uiItems[i].id;
        uiGeometry[i].width = UI_ELEMENT_WIDTH;
        uiGeometry[i].height = UI_ELEMENT_HEIGHT;
        uiGeometry[i].visible = 0;

        if (uiItems[i].order < 0 ||
            uiItems[i].order >= UI_VISIBLE_ELEMENT_COLUMNS ||
            !UI_IsLaneVisible(uiItems[i].lane))
        {
            continue;
        }

        uiGeometry[i].x =
            UI_GetElementX(uiItems[i].order);

        uiGeometry[i].y =
            UI_GetLaneY(uiItems[i].lane);

        uiGeometry[i].visible = 1;
    }
}

static int16_t UI_FindItemIndexById(uint16_t itemId)
{
    for (uint16_t i = 0; i < UI_ITEM_COUNT; i++)
    {
        if (uiItems[i].id == itemId)
        {
            return (int16_t)i;
        }
    }

    return -1;
}

/* -------------------------------------------------------------------------- */
/* Item drawing                                                               */
/* -------------------------------------------------------------------------- */

static void UI_DrawLoop(
    const UI_Item *item,
    int16_t x,
    int16_t y)
{
    uint16_t fillColor = UI_GetFillColor(item);
    uint16_t borderColor = UI_GetBorderColor(item);
    uint16_t textColor = UI_GetTextColor(item);

    ST7735_FillRect(
        (uint16_t)x,
        (uint16_t)y,
        UI_ELEMENT_WIDTH,
        UI_ELEMENT_HEIGHT,
        fillColor);

    ST7735_DrawRect(
        (uint16_t)x,
        (uint16_t)y,
        UI_ELEMENT_WIDTH,
        UI_ELEMENT_HEIGHT,
        borderColor);

    if (item->focus == UI_FOCUS_SELECTED ||
        item->focus == UI_FOCUS_GRABBED)
    {
        ST7735_DrawRect(
            (uint16_t)(x + 1),
            (uint16_t)(y + 1),
            UI_ELEMENT_WIDTH - 2,
            UI_ELEMENT_HEIGHT - 2,
            borderColor);
    }

    UI_DrawCenteredText(
        (uint16_t)x,
        (uint16_t)y,
        UI_ELEMENT_WIDTH,
        UI_ELEMENT_HEIGHT,
        item->shortName,
        textColor,
        fillColor,
        1);
}

static void UI_DrawIO(
    const UI_Item *item,
    int16_t x,
    int16_t y)
{
    uint16_t borderColor = UI_GetBorderColor(item);

    int16_t centerX =
        x + (UI_ELEMENT_WIDTH / 2);

    int16_t centerY =
        y + (UI_ELEMENT_HEIGHT / 2);

    UI_DrawCircle(
        centerX,
        centerY,
        UI_IO_CIRCLE_RADIUS,
        borderColor);

    ST7735_DrawPixel(
        centerX,
        centerY,
        borderColor);

    UI_DrawCenteredText(
        (uint16_t)x,
        (uint16_t)(centerY + UI_IO_CIRCLE_RADIUS + 3),
        UI_ELEMENT_WIDTH,
        7,
        item->shortName,
        UI_COLOR_TEXT_LIGHT,
        UI_COLOR_BACKGROUND,
        1);
}

static void UI_DrawManualNode(
    const UI_Item *item,
    int16_t x,
    int16_t y)
{
    uint16_t color = UI_GetBorderColor(item);

    int16_t centerX =
        x + (UI_ELEMENT_WIDTH / 2);

    int16_t centerY =
        y + (UI_ELEMENT_HEIGHT / 2);

    ST7735_DrawLine(
        centerX,
        centerY - UI_MANUAL_NODE_RADIUS,
        centerX + UI_MANUAL_NODE_RADIUS,
        centerY,
        color);

    ST7735_DrawLine(
        centerX + UI_MANUAL_NODE_RADIUS,
        centerY,
        centerX,
        centerY + UI_MANUAL_NODE_RADIUS,
        color);

    ST7735_DrawLine(
        centerX,
        centerY + UI_MANUAL_NODE_RADIUS,
        centerX - UI_MANUAL_NODE_RADIUS,
        centerY,
        color);

    ST7735_DrawLine(
        centerX - UI_MANUAL_NODE_RADIUS,
        centerY,
        centerX,
        centerY - UI_MANUAL_NODE_RADIUS,
        color);
}

static void UI_DrawAutoNode(
    int16_t x,
    int16_t y)
{
    UI_FillCircle(
        x + (UI_ELEMENT_WIDTH / 2),
        y + (UI_ELEMENT_HEIGHT / 2),
        UI_AUTO_NODE_RADIUS,
        UI_COLOR_AUTO_NODE);
}

static void UI_DrawItem(
    const UI_Item *item,
    int16_t x,
    int16_t y)
{
    switch (item->type)
    {
        case UI_ITEM_LOOP:
            UI_DrawLoop(item, x, y);
            break;

        case UI_ITEM_INPUT:
        case UI_ITEM_OUTPUT:
            UI_DrawIO(item, x, y);
            break;

        case UI_ITEM_MANUAL_NODE:
            UI_DrawManualNode(item, x, y);
            break;

        case UI_ITEM_AUTO_NODE:
            UI_DrawAutoNode(x, y);
            break;

        default:
            break;
    }
}

/* -------------------------------------------------------------------------- */
/* Connection points                                                          */
/* -------------------------------------------------------------------------- */

static int16_t UI_GetConnectionY(
    const UI_ItemGeometry *geometry)
{
    return geometry->y + (geometry->height / 2);
}

static int16_t UI_GetInputX(
    const UI_ItemGeometry *geometry,
    const UI_Item *item)
{
    if (item->type == UI_ITEM_INPUT ||
        item->type == UI_ITEM_OUTPUT)
    {
        return
            geometry->x +
            (geometry->width / 2) -
            UI_IO_CIRCLE_RADIUS;
    }

    return geometry->x;
}

static int16_t UI_GetOutputX(
    const UI_ItemGeometry *geometry,
    const UI_Item *item)
{
    if (item->type == UI_ITEM_INPUT ||
        item->type == UI_ITEM_OUTPUT)
    {
        return
            geometry->x +
            (geometry->width / 2) +
            UI_IO_CIRCLE_RADIUS;
    }

    return
        geometry->x +
        geometry->width - 1;
}

/* -------------------------------------------------------------------------- */
/* Connections                                                                */
/* -------------------------------------------------------------------------- */

static void UI_DrawHorizontalArrow(
    int16_t startX,
    int16_t y,
    int16_t targetX)
{
    if (targetX <= startX)
    {
        return;
    }

    ST7735_DrawLine(
        startX,
        y,
        targetX - 1,
        y,
        UI_COLOR_CONNECTION
    );
}

static void UI_DrawConnectionArrow(
    int16_t startX,
    int16_t startY,
    int16_t targetX,
    int16_t targetY)
{
    if (targetX <= startX)
    {
        return;
    }

    if (startY == targetY)
    {
        UI_DrawHorizontalArrow(
            startX,
            startY,
            targetX
        );

        return;
    }

    ST7735_DrawLine(
        startX,
        startY,
        targetX - 1,
        targetY,
        UI_COLOR_CONNECTION
    );
}

static void UI_DrawConnections(void)
{
    for (uint16_t i = 0; i < UI_CONNECTION_COUNT; i++)
    {
        int16_t sourceIndex =
            UI_FindItemIndexById(
                uiConnections[i].sourceId);

        int16_t targetIndex =
            UI_FindItemIndexById(
                uiConnections[i].targetId);

        if (sourceIndex < 0 || targetIndex < 0)
        {
            continue;
        }

        const UI_ItemGeometry *sourceGeometry =
            &uiGeometry[sourceIndex];

        const UI_ItemGeometry *targetGeometry =
            &uiGeometry[targetIndex];

        if (!sourceGeometry->visible ||
            !targetGeometry->visible)
        {
            continue;
        }

        int16_t sourceY =
            UI_GetConnectionY(sourceGeometry);

        int16_t targetY =
            UI_GetConnectionY(targetGeometry);


        int16_t sourceX =
            UI_GetOutputX(
                sourceGeometry,
                &uiItems[sourceIndex]);

        int16_t targetX =
            UI_GetInputX(
                targetGeometry,
                &uiItems[targetIndex]);

        UI_DrawConnectionArrow(
        sourceX + 1,
        sourceY,
        targetX,
        targetY);
    }
}

/* -------------------------------------------------------------------------- */
/* Footer                                                                     */
/* -------------------------------------------------------------------------- */

static const UI_Item *UI_GetFocusedItem(void)
{
    for (uint16_t i = 0; i < UI_ITEM_COUNT; i++)
    {
        if (uiItems[i].focus == UI_FOCUS_SELECTED ||
            uiItems[i].focus == UI_FOCUS_GRABBED)
        {
            return &uiItems[i];
        }
    }

    return NULL;
}

static void UI_DrawFooter(void)
{
    int16_t focusedIndex = -1;

    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (uiItems[i].focus == UI_FOCUS_SELECTED ||
            uiItems[i].focus == UI_FOCUS_GRABBED)
        {
            focusedIndex = (int16_t)i;
            break;
        }
    }


    ST7735_FillRect(
        0,
        UI_FOOTER_TOP,
        UI_DISPLAY_WIDTH,
        UI_FOOTER_HEIGHT,
        UI_COLOR_BACKGROUND
    );


    if (focusedIndex < 0)
    {
        return;
    }


    if (!uiGeometry[focusedIndex].visible)
    {
        return;
    }


    const UI_Item *focusedItem =
        &uiItems[focusedIndex];

    const UI_ItemGeometry *geometry =
        &uiGeometry[focusedIndex];


    uint16_t textWidth =
        Font5x7_GetStringWidth(
            focusedItem->longName,
            1
        );


    int16_t itemCenterX =
        geometry->x +
        (geometry->width / 2);

    int16_t textX =
        itemCenterX -
        ((int16_t)textWidth / 2);


    /*
     * Text am linken und rechten Displayrand begrenzen.
     */
    if (textX < 0)
    {
        textX = 0;
    }

    if ((textX + textWidth) >
        UI_DISPLAY_WIDTH)
    {
        textX =
            UI_DISPLAY_WIDTH -
            textWidth;
    }


    uint16_t textHeight =
        Font5x7_GetHeight(1);

    uint16_t textY =
        UI_FOOTER_TOP;

    if (textHeight < UI_FOOTER_HEIGHT)
    {
        textY +=
            (UI_FOOTER_HEIGHT -
             textHeight) / 2;
    }


    Font5x7_DrawString(
        (uint16_t)textX,
        textY,
        focusedItem->longName,
        UI_COLOR_TEXT_LIGHT,
        UI_COLOR_BACKGROUND,
        1
    );
}

/* -------------------------------------------------------------------------- */
/* Public interface                                                           */
/* -------------------------------------------------------------------------- */

void UI_Init(void)
{
}

void UI_Draw(void)
{
    ST7735_FillScreen(UI_COLOR_BACKGROUND);

    UI_CalculateGeometry();

    UI_DrawConnections();

    for (uint16_t i = 0; i < UI_ITEM_COUNT; i++)
    {
        if (!uiGeometry[i].visible)
        {
            continue;
        }

        UI_DrawItem(
            &uiItems[i],
            uiGeometry[i].x,
            uiGeometry[i].y);
    }

    UI_DrawFooter();
}
