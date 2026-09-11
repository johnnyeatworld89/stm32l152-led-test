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

#define UI_DISPLAY_WIDTH              160
#define UI_DISPLAY_HEIGHT             128

#define UI_VISIBLE_ELEMENT_COLUMNS    4
#define UI_VISIBLE_LANES              3

#define UI_ELEMENT_WIDTH              24
#define UI_ELEMENT_HEIGHT             24

#define UI_CONNECTION_WIDTH           18
#define UI_VERTICAL_CONNECTION_HEIGHT 16

#define UI_GRID_CONTENT_WIDTH \
    ((UI_VISIBLE_ELEMENT_COLUMNS * UI_ELEMENT_WIDTH) + \
     ((UI_VISIBLE_ELEMENT_COLUMNS - 1) * UI_CONNECTION_WIDTH))

#define UI_GRID_CONTENT_HEIGHT \
    ((UI_VISIBLE_LANES * UI_ELEMENT_HEIGHT) + \
     ((UI_VISIBLE_LANES - 1) * UI_VERTICAL_CONNECTION_HEIGHT))

#define UI_GRID_LEFT \
    ((UI_DISPLAY_WIDTH - UI_GRID_CONTENT_WIDTH) / 2)

#define UI_GRID_TOP                   2

#define UI_FOOTER_TOP \
    (UI_GRID_TOP + UI_GRID_CONTENT_HEIGHT + 2)

#define UI_FOOTER_HEIGHT \
    (UI_DISPLAY_HEIGHT - UI_FOOTER_TOP)

#define UI_IO_CIRCLE_RADIUS           7
#define UI_IO_DIAGONAL_OFFSET         5

#define UI_MANUAL_NODE_RADIUS         6
#define UI_AUTO_NODE_RADIUS 6
#define UI_AUTO_NODE_DIAGONAL_OFFSET 4
#define UI_MANUAL_NODE_DIAGONAL_OFFSET 3


#define UI_LOOP_CORNER_RADIUS         2
#define UI_LOOP_DIAGONAL_INSET        1

#define UI_FOCUS_FRAME_GAP            2
#define UI_FOCUS_CORNER_RADIUS        3

#define UI_ARROW_LENGTH               5
#define UI_ARROW_HALF_WIDTH           3
#define UI_ARROW_FIXED_SCALE          256


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
        .type = UI_ITEM_MANUAL_NODE,
        .order = 1,
        .lane = 1,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_SELECTED,
        .shortName = "N01",
        .longName = "Node 01"
    },
    {
        .id = 3,
        .type = UI_ITEM_LOOP,
        .order = 1,
        .lane = -1,
        .loopStatus = UI_LOOP_STATUS_ACTIVE_CONFIRMED,
        .focus = UI_FOCUS_NONE,
        .shortName = "L01",
        .longName = "Loop 01"
    },
    {
        .id = 4,
        .type = UI_ITEM_AUTO_NODE,
        .order = 2,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "",
        .longName = ""
    },
    {
        .id = 5,
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
    /*
     * Split after IN1.
     */
    {
        .sourceId = 1,
   *    .targetId = 2
    },
    {
   *    .sourceId = 1,
        .targetId = 3
    },

    /*
     * Merge at the automatic node.
     */
    {
        .source*d = 2,
*       .targetId = 4
    },
    {
*       .sourceId*= 3,
        .targetId = 4
    },

    /*
     * Automatic node to OUT1.
     */
    {
        .sourceId = 4,
        .targetId = 5
    }
};
#define UI_CONNECTION_COUNT \
    (sizeof(uiConnections) / sizeof(uiConnections[0]))


static UI_ItemGeometry uiGeometry[UI_ITEM_COUNT];


/* -------------------------------------------------------------------------- */
/* Local types                                                                */
/* -------------------------------------------------------------------------- */

typedef struct
{
    int16_t x;
    int16_t y;

} UI_ConnectionPoint;


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


static void UI_FillRoundedRect(
    int16_t x,
    int16_t y,
    uint16_t width,
    uint16_t height,
    uint16_t radius,
    uint16_t color)
{
    if (width == 0 || height == 0)
    {
        return;
    }

    if (radius == 0)
    {
        ST7735_FillRect(
            (uint16_t)x,
            (uint16_t)y,
            width,
            height,
            color);
        return;
    }

    if ((radius * 2) >= width)
    {
        radius = (width - 1) / 2;
    }

    if ((radius * 2) >= height)
    {
        radius = (height - 1) / 2;
    }

    ST7735_FillRect(
        (uint16_t)(x + radius),
        (uint16_t)y,
        width - (2 * radius),
        height,
        color);

    ST7735_FillRect(
        (uint16_t)x,
        (uint16_t)(y + radius),
        radius,
        height - (2 * radius),
        color);

    ST7735_FillRect(
        (uint16_t)(x + width - radius),
        (uint16_t)(y + radius),
        radius,
        height - (2 * radius),
        color);

    for (uint16_t offsetY = 0; offsetY < radius; offsetY++)
    {
        uint16_t inset = radius - offsetY - 1;
        uint16_t lineWidth = width - (2 * inset);

        ST7735_FillRect(
            (uint16_t)(x + inset),
            (uint16_t)(y + offsetY),
            lineWidth,
            1,
            color);

        ST7735_FillRect(
            (uint16_t)(x + inset),
            (uint16_t)(y + height - 1 - offsetY),
            lineWidth,
            1,
            color);
    }
}


static void UI_DrawRoundedRect(
    int16_t x,
    int16_t y,
    uint16_t width,
    uint16_t height,
    uint16_t radius,
    uint16_t color)
{
    if (width == 0 || height == 0)
    {
        return;
    }

    if (radius == 0)
    {
        ST7735_DrawRect(
            (uint16_t)x,
            (uint16_t)y,
            width,
            height,
            color);
        return;
    }

    if ((radius * 2) >= width)
    {
        radius = (width - 1) / 2;
    }

    if ((radius * 2) >= height)
    {
        radius = (height - 1) / 2;
    }

    ST7735_DrawLine(
        x + radius,
        y,
        x + width - radius - 1,
        y,
        color);

    ST7735_DrawLine(
        x + radius,
        y + height - 1,
        x + width - radius - 1,
        y + height - 1,
        color);

    ST7735_DrawLine(
        x,
        y + radius,
        x,
        y + height - radius - 1,
        color);

    ST7735_DrawLine(
        x + width - 1,
        y + radius,
        x + width - 1,
        y + height - radius - 1,
        color);

    ST7735_DrawPixel(x + 1, y + 1, color);
    ST7735_DrawPixel(x + width - 2, y + 1, color);
    ST7735_DrawPixel(x + 1, y + height - 2, color);
    ST7735_DrawPixel(x + width - 2, y + height - 2, color);
}


static int32_t UI_TriangleEdge(
    int16_t ax,
    int16_t ay,
    int16_t bx,
    int16_t by,
    int16_t px,
    int16_t py)
{
    return
        ((int32_t)(px - ax) * (int32_t)(by - ay)) -
        ((int32_t)(py - ay) * (int32_t)(bx - ax));
}


static void UI_FillTriangle(
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    int16_t x2,
    int16_t y2,
    uint16_t color)
{
    int16_t minX = x0;
    int16_t maxX = x0;
    int16_t minY = y0;
    int16_t maxY = y0;

    if (x1 < minX) minX = x1;
    if (x2 < minX) minX = x2;
    if (x1 > maxX) maxX = x1;
    if (x2 > maxX) maxX = x2;

    if (y1 < minY) minY = y1;
    if (y2 < minY) minY = y2;
    if (y1 > maxY) maxY = y1;
    if (y2 > maxY) maxY = y2;

    if (minX < 0) minX = 0;
    if (minY < 0) minY = 0;
    if (maxX >= UI_DISPLAY_WIDTH) maxX = UI_DISPLAY_WIDTH - 1;
    if (maxY >= UI_DISPLAY_HEIGHT) maxY = UI_DISPLAY_HEIGHT - 1;

    for (int16_t y = minY; y <= maxY; y++)
    {
        for (int16_t x = minX; x <= maxX; x++)
        {
            int32_t edge0 = UI_TriangleEdge(x0, y0, x1, y1, x, y);
            int32_t edge1 = UI_TriangleEdge(x1, y1, x2, y2, x, y);
            int32_t edge2 = UI_TriangleEdge(x2, y2, x0, y0, x, y);

            uint8_t hasNegative =
                (edge0 < 0) || (edge1 < 0) || (edge2 < 0);

            uint8_t hasPositive =
                (edge0 > 0) || (edge1 > 0) || (edge2 > 0);

            if (!(hasNegative && hasPositive))
            {
                ST7735_DrawPixel(x, y, color);
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


static uint16_t UI_GetFocusColor(const UI_Item *item)
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

    uint16_t textWidth = Font5x7_GetStringWidth(text, scale);
    uint16_t textHeight = Font5x7_GetHeight(scale);

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

        uiGeometry[i].x = UI_GetElementX(uiItems[i].order);
        uiGeometry[i].y = UI_GetLaneY(uiItems[i].lane);
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
    uint16_t textColor = UI_GetTextColor(item);
    uint16_t focusColor = UI_GetFocusColor(item);

    if (item->focus == UI_FOCUS_SELECTED ||
        item->focus == UI_FOCUS_GRABBED)
    {
        UI_DrawRoundedRect(
            x - UI_FOCUS_FRAME_GAP,
            y - UI_FOCUS_FRAME_GAP,
            UI_ELEMENT_WIDTH + (2 * UI_FOCUS_FRAME_GAP),
            UI_ELEMENT_HEIGHT + (2 * UI_FOCUS_FRAME_GAP),
            UI_FOCUS_CORNER_RADIUS,
            focusColor);
    }

    UI_FillRoundedRect(
        x,
        y,
        UI_ELEMENT_WIDTH,
        UI_ELEMENT_HEIGHT,
        UI_LOOP_CORNER_RADIUS,
        fillColor);

    UI_DrawRoundedRect(
        x,
        y,
        UI_ELEMENT_WIDTH,
        UI_ELEMENT_HEIGHT,
        UI_LOOP_CORNER_RADIUS,
        UI_COLOR_BORDER_NORMAL);

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
    uint16_t borderColor = UI_GetFocusColor(item);

    int16_t centerX = x + (UI_ELEMENT_WIDTH / 2);
    int16_t centerY = y + (UI_ELEMENT_HEIGHT / 2);

    UI_DrawCircle(
        centerX,
        centerY,
        UI_IO_CIRCLE_RADIUS,
        borderColor);

    ST7735_DrawPixel(centerX, centerY, borderColor);

    uint16_t textWidth =
        Font5x7_GetStringWidth(item->shortName, 1);

    int16_t textX =
        centerX - ((int16_t)textWidth / 2);

    if (textX < x)
    {
        textX = x;
    }

    if ((textX + (int16_t)textWidth) >
        (x + UI_ELEMENT_WIDTH))
    {
        textX =
            x + UI_ELEMENT_WIDTH - (int16_t)textWidth;
    }

    uint16_t textY =
        (uint16_t)(centerY + UI_IO_CIRCLE_RADIUS + 3);

    Font5x7_DrawString(
        (uint16_t)textX,
        textY,
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
    int16_t centerX =
        x + (UI_ELEMENT_WIDTH / 2);

    int16_t centerY =
        y + (UI_ELEMENT_HEIGHT / 2);


    /*
     * Separate focus diamond outside the actual node.
     */
    if (item->focus == UI_FOCUS_SELECTED ||
        item->focus == UI_FOCUS_GRABBED)
    {
        uint16_t focusColor =
            UI_GetFocusColor(item);

        int16_t focusRadius =
            UI_MANUAL_NODE_RADIUS +
            UI_FOCUS_FRAME_GAP;


        ST7735_DrawLine(
            centerX,
            centerY - focusRadius,
            centerX + focusRadius,
            centerY,
            focusColor
        );

        ST7735_DrawLine(
            centerX + focusRadius,
            centerY,
            centerX,
            centerY + focusRadius,
            focusColor
        );

        ST7735_DrawLine(
            centerX,
            centerY + focusRadius,
            centerX - focusRadius,
            centerY,
            focusColor
        );

        ST7735_DrawLine(
            centerX - focusRadius,
            centerY,
            centerX,
            centerY - focusRadius,
            focusColor
        );
    }


    /*
     * Actual manual node remains white.
     */
    ST7735_DrawLine(
        centerX,
        centerY - UI_MANUAL_NODE_RADIUS,
        centerX + UI_MANUAL_NODE_RADIUS,
        centerY,
        UI_COLOR_BORDER_NORMAL
    );

    ST7735_DrawLine(
        centerX + UI_MANUAL_NODE_RADIUS,
        centerY,
        centerX,
        centerY + UI_MANUAL_NODE_RADIUS,
        UI_COLOR_BORDER_NORMAL
    );

    ST7735_DrawLine(
        centerX,
        centerY + UI_MANUAL_NODE_RADIUS,
        centerX - UI_MANUAL_NODE_RADIUS,
        centerY,
        UI_COLOR_BORDER_NORMAL
    );

    ST7735_DrawLine(
        centerX - UI_MANUAL_NODE_RADIUS,
        centerY,
        centerX,
        centerY - UI_MANUAL_NODE_RADIUS,
        UI_COLOR_BORDER_NORMAL
    );
}


static void UI_DrawAutoNode(
    int16_t x,
    int16_t y)
{
    int16_t centerX =
        x + (UI_ELEMENT_WIDTH / 2);

    int16_t centerY =
        y + (UI_ELEMENT_HEIGHT / 2);


    /*
     * Automatic node:
     * white circular outline with a center point.
     *
     * The larger circle provides separate connection
     * points for horizontal and diagonal arrows.
     */
    UI_DrawCircle(
        centerX,
        centerY,
        UI_AUTO_NODE_RADIUS,
        UI_COLOR_AUTO_NODE
    );


    UI_FillCircle(
        centerX,
        centerY,
        1,
        UI_COLOR_AUTO_NODE
    );
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

static uint8_t UI_IsCircularItem(
    const UI_Item *item)
{
    if (item == NULL)
    {
        return 0;
    }

    return
        (item->type == UI_ITEM_INPUT ||
         item->type == UI_ITEM_OUTPUT ||
         item->type == UI_ITEM_AUTO_NODE);
}


static int16_t UI_GetCircularRadius(
    const UI_Item *item)
{
    if (item != NULL &&
        item->type == UI_ITEM_AUTO_NODE)
    {
        return UI_AUTO_NODE_RADIUS;
    }

    return UI_IO_CIRCLE_RADIUS;
}


static int16_t UI_GetCircularDiagonalOffset(
    const UI_Item *item)
{
    if (item != NULL &&
        item->type == UI_ITEM_AUTO_NODE)
    {
        return UI_AUTO_NODE_DIAGONAL_OFFSET;
    }

    return UI_IO_DIAGONAL_OFFSET;
}

static UI_ConnectionPoint UI_GetSourceConnectionPoint(
    const UI_ItemGeometry *sourceGeometry,
    const UI_Item *sourceItem,
    int16_t targetY)
{
    UI_ConnectionPoint point;

    int16_t centerX =
        sourceGeometry->x +
        (sourceGeometry->width / 2);

    int16_t centerY =
        sourceGeometry->y +
        (sourceGeometry->height / 2);


    /*
     * Horizontal connection.
     */
    if (targetY == centerY)
    {
        if (UI_IsCircularItem(sourceItem))
        {
            point.x =
                centerX +
                UI_GetCircularRadius(sourceItem);
        }
        else if (sourceItem->type ==
                 UI_ITEM_MANUAL_NODE)
        {
            point.x =
                centerX +
                UI_MANUAL_NODE_RADIUS;
        }
        else
        {
            point.x =
                sourceGeometry->x +
                sourceGeometry->width -
                1;
        }

        point.y = centerY;

        return point;
    }


    /*
     * Connection goes to the upper-right.
     */
    if (targetY < centerY)
    {
        if (UI_IsCircularItem(sourceItem))
        {
            int16_t offset =
                UI_GetCircularDiagonalOffset(
                    sourceItem
                );

            point.x = centerX + offset;
            point.y = centerY - offset;
        }
        else if (sourceItem->type ==
                 UI_ITEM_MANUAL_NODE)
        {
            point.x =
                centerX +
                UI_MANUAL_NODE_DIAGONAL_OFFSET;

            point.y =
                centerY -
                UI_MANUAL_NODE_DIAGONAL_OFFSET;
        }
        else
        {
            point.x =
                sourceGeometry->x +
                sourceGeometry->width -
                1 -
                UI_LOOP_DIAGONAL_INSET;

            point.y =
                sourceGeometry->y +
                UI_LOOP_DIAGONAL_INSET;
        }

        return point;
    }


    /*
     * Connection goes to the lower-right.
     */
    if (UI_IsCircularItem(sourceItem))
    {
        int16_t offset =
            UI_GetCircularDiagonalOffset(
                sourceItem
            );

        point.x = centerX + offset;
        point.y = centerY + offset;
    }
    else if (sourceItem->type ==
             UI_ITEM_MANUAL_NODE)
    {
        point.x =
            centerX +
            UI_MANUAL_NODE_DIAGONAL_OFFSET;

        point.y =
            centerY +
            UI_MANUAL_NODE_DIAGONAL_OFFSET;
    }
    else
    {
        point.x =
            sourceGeometry->x +
            sourceGeometry->width -
            1 -
            UI_LOOP_DIAGONAL_INSET;

        point.y =
            sourceGeometry->y +
            sourceGeometry->height -
            1 -
            UI_LOOP_DIAGONAL_INSET;
    }


    return point;
}


static UI_ConnectionPoint UI_GetTargetConnectionPoint(
    const UI_ItemGeometry *targetGeometry,
    const UI_Item *targetItem,
    int16_t sourceY)
{
    UI_ConnectionPoint point;

    int16_t centerX =
        targetGeometry->x +
        (targetGeometry->width / 2);

    int16_t centerY =
        targetGeometry->y +
        (targetGeometry->height / 2);


    /*
     * Horizontal connection.
     */
    if (sourceY == centerY)
   *{
        if (UI_IsCircularItem(ta*getItem))
        {
            po*nt.x =
                centerX -
 *              UI_GetCircularRadius*targetItem);
        }
        els* if (targetItem->type ==
         *       UI_ITEM_MANUAL_NODE)
      * {
            point.x =
         *      centerX -
                UI_MANUAL_NODE_RADIUS;
        }
        else
        {
            point.x =
                targetGeometry->x;
        }

        point.y = centerY;

        return point;
    }


    /*
     * Source lies below target.
     * Connection rises to the upper-right.
     */
    if (sourceY > centerY)
    {
        if (UI_IsCircularItem(targetItem))
        {
            int16_t offset =
                UI_GetCircularDiagonalOffset(
                    targetItem
                );

            point.x = centerX - offset;
            point.y = centerY + offset;
        }
        else if (targetItem->type ==
                 UI_ITEM_MANUAL_NODE)
        {
            point.x =
                centerX -
                UI_MANUAL_NODE_DIAGONAL_OFFSET;

            point.y =
                centerY +
                UI_MANUAL_NODE_DIAGONAL_OFFSET;
        }
        else
        {
            point.x =
                targetGeometry->x +
                UI_LOOP_DIAGONAL_INSET;

            point.y =
                targetGeometry->y +
                targetGeometry->height -
                1 -
                UI_LOOP_DIAGONAL_INSET;
        }

        return point;
    }


    /*
     * Source lies above target.
     * Connection falls to the lower-right.
     */
    if (UI_IsCircularItem(targetItem))
    {
        int16_t offset =
            UI_GetCircularDiagonalOffset(
                targetItem
            );

        point.x = centerX - offset;
        point.y = centerY - offset;
    }
    else if (targetItem->type ==
             UI_ITEM_MANUAL_NODE)
    {
        point.x =
            centerX -
            UI_MANUAL_NODE_DIAGONAL_OFFSET;

        point.y =
            centerY -
            UI_MANUAL_NODE_DIAGONAL_OFFSET;
    }
    else
    {
        point.x =
            targetGeometry->x +
            UI_LOOP_DIAGONAL_INSET;

        point.y =
            targetGeometry->y +
            UI_LOOP_DIAGONAL_INSET;
    }


    return point;
}


/* -------------------------------------------------------------------------- */
/* Connections                                                                */
/* -------------------------------------------------------------------------- */

static void UI_DrawConnectionLine(
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    uint16_t color)
{
    int16_t dx =
        x1 - x0;

    int16_t dy =
        y1 - y0;

    int16_t absDx =
        (dx >= 0) ? dx : -dx;

    int16_t absDy =
        (dy >= 0) ? dy : -dy;

    int16_t steps =
        (absDx > absDy) ? absDx : absDy;


    if (steps == 0)
    {
        ST7735_DrawPixel(
            x0,
            y0,
            color
        );

        return;
    }


    int32_t currentX =
        ((int32_t)x0 << 8);

    int32_t currentY =
        ((int32_t)y0 << 8);

    int32_t stepX =
        ((int32_t)dx << 8) /
        steps;

    int32_t stepY =
        ((int32_t)dy << 8) /
        steps;


    int16_t previousX = x0;
    int16_t previousY = y0;


    ST7735_DrawPixel(
        previousX,
        previousY,
        color
    );


    for (int16_t step = 1;
         step <= steps;
         step++)
    {
        currentX += stepX;
        currentY += stepY;

        int16_t pixelX =
            (int16_t)(
                (currentX + 128) >> 8
            );

        int16_t pixelY =
            (int16_t)(
                (currentY + 128) >> 8
            );


        /*
         * Wenn sich X und Y gleichzeitig ändern,
         * wird ein Verbindungspixel ergänzt.
         *
         * Dadurch entstehen keine sichtbaren
         * Ein-Pixel-Versätze in der Diagonale.
         */
        if (pixelX != previousX &&
            pixelY != previousY)
        {
            ST7735_DrawPixel(
                pixelX,
                previousY,
                color
            );
        }


        ST7735_DrawPixel(
            pixelX,
            pixelY,
            color
        );


        previousX = pixelX;
        previousY = pixelY;
    }
}

static void UI_DrawDirectConnection(
    int16_t startX,
    int16_t startY,
    int16_t targetX,
    int16_t targetY)
{
    int16_t deltaX = targetX - startX;
    int16_t deltaY = targetY - startY;

    if (deltaX <= 0)
    {
        return;
    }

    int16_t absoluteX = deltaX;
    int16_t absoluteY = (deltaY >= 0) ? deltaY : -deltaY;

    int16_t normalization =
        (absoluteX > absoluteY) ? absoluteX : absoluteY;

    if (normalization == 0)
    {
        return;
    }

    int32_t directionX =
        ((int32_t)deltaX * UI_ARROW_FIXED_SCALE) /
        normalization;

    int32_t directionY =
        ((int32_t)deltaY * UI_ARROW_FIXED_SCALE) /
        normalization;

    int16_t tipX =
        targetX -
        (int16_t)(directionX / UI_ARROW_FIXED_SCALE);

    int16_t tipY =
        targetY -
        (int16_t)(directionY / UI_ARROW_FIXED_SCALE);

    int16_t baseCenterX =
        tipX -
        (int16_t)((directionX * UI_ARROW_LENGTH) /
                  UI_ARROW_FIXED_SCALE);

    int16_t baseCenterY =
        tipY -
        (int16_t)((directionY * UI_ARROW_LENGTH) /
                  UI_ARROW_FIXED_SCALE);

    int16_t perpendicularX =
        (int16_t)(((-directionY) * UI_ARROW_HALF_WIDTH) /
                  UI_ARROW_FIXED_SCALE);

    int16_t perpendicularY =
        (int16_t)((directionX * UI_ARROW_HALF_WIDTH) /
                  UI_ARROW_FIXED_SCALE);

    int16_t base1X = baseCenterX + perpendicularX;
    int16_t base1Y = baseCenterY + perpendicularY;
    int16_t base2X = baseCenterX - perpendicularX;
    int16_t base2Y = baseCenterY - perpendicularY;

    /*
     * Draw through to the tip. The filled triangle is placed over the final
     * portion of the shaft. This avoids a visible seam at the arrowhead.
     */
    UI_DrawConnectionLine(
        startX,
        startY,
        tipX,
        tipY,
        UI_COLOR_CONNECTION
    );

    UI_FillTriangle(
        tipX,
        tipY,
        base1X,
        base1Y,
        base2X,
        base2Y,
        UI_COLOR_CONNECTION);
}


static void UI_DrawConnections(void)
{
    for (uint16_t i = 0; i < UI_CONNECTION_COUNT; i++)
    {
        int16_t sourceIndex =
            UI_FindItemIndexById(uiConnections[i].sourceId);

        int16_t targetIndex =
            UI_FindItemIndexById(uiConnections[i].targetId);

        if (sourceIndex < 0 || targetIndex < 0)
        {
            continue;
        }

        const UI_ItemGeometry *sourceGeometry =
            &uiGeometry[sourceIndex];

        const UI_ItemGeometry *targetGeometry =
            &uiGeometry[targetIndex];

        const UI_Item *sourceItem = &uiItems[sourceIndex];
        const UI_Item *targetItem = &uiItems[targetIndex];

        if (!sourceGeometry->visible || !targetGeometry->visible)
        {
            continue;
        }

        int16_t sourceCenterY =
            sourceGeometry->y + (sourceGeometry->height / 2);

        int16_t targetCenterY =
            targetGeometry->y + (targetGeometry->height / 2);

        UI_ConnectionPoint sourcePoint =
            UI_GetSourceConnectionPoint(
                sourceGeometry,
                sourceItem,
                targetCenterY);

        UI_ConnectionPoint targetPoint =
            UI_GetTargetConnectionPoint(
                targetGeometry,
                targetItem,
                sourceCenterY);

        UI_DrawDirectConnection(
            sourcePoint.x,
            sourcePoint.y,
            targetPoint.x,
            targetPoint.y);
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
    const UI_Item *focusedItem = UI_GetFocusedItem();

    ST7735_FillRect(
        0,
        UI_FOOTER_TOP,
        UI_DISPLAY_WIDTH,
        UI_FOOTER_HEIGHT,
        UI_COLOR_BACKGROUND);

    if (focusedItem == NULL)
    {
        return;
    }

    int16_t focusedIndex =
        UI_FindItemIndexById(focusedItem->id);

    if (focusedIndex < 0 || !uiGeometry[focusedIndex].visible)
    {
        return;
    }

    const UI_ItemGeometry *geometry =
        &uiGeometry[focusedIndex];

    uint16_t textWidth =
        Font5x7_GetStringWidth(focusedItem->longName, 1);

    int16_t itemCenterX =
        geometry->x + (geometry->width / 2);

    int16_t textX =
        itemCenterX - ((int16_t)textWidth / 2);

    if (textX < 0)
    {
        textX = 0;
    }

    if ((textX + (int16_t)textWidth) > UI_DISPLAY_WIDTH)
    {
        textX = UI_DISPLAY_WIDTH - (int16_t)textWidth;
    }

    uint16_t textHeight = Font5x7_GetHeight(1);
    uint16_t textY = UI_FOOTER_TOP;

    if (textHeight < UI_FOOTER_HEIGHT)
    {
        textY += (UI_FOOTER_HEIGHT - textHeight) / 2;
    }

    /* Same color as the selected or grabbed focus frame. */
    uint16_t footerTextColor = UI_GetFocusColor(focusedItem);

    Font5x7_DrawString(
        (uint16_t)textX,
        textY,
        focusedItem->longName,
        footerTextColor,
        UI_COLOR_BACKGROUND,
        1);
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
