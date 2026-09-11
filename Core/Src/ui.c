#include "ui.h"

#include "st7735.h"
#include "font5x7.h"

#define UI_COLOR_SELECTED_BORDER    0x051F
#define UI_COLOR_GRABBED_BORDER     0xF81F
#define UI_COLOR_NORMAL_BORDER      0xFFFF
#define UI_COLOR_LOOP_OFF           0x0000
#define UI_COLOR_LOOP_CONFIRMED     0x07E0
#define UI_COLOR_LOOP_UNCONFIRMED   0xFFE0
#define UI_COLOR_BACKGROUND         0x0000
#define UI_COLOR_TEXT_LIGHT         0xFFFF
#define UI_COLOR_TEXT_DARK          0x0000
#define UI_COLOR_NODE               0xFFFF
#define UI_COLOR_AUTO_NODE          0xFFFF

#define UI_DISPLAY_WIDTH            160
#define UI_DISPLAY_HEIGHT           128
#define UI_MARGIN_LEFT              4
#define UI_MARGIN_RIGHT             4
#define UI_NORMAL_ITEM_WIDTH        22
#define UI_SELECTED_ITEM_WIDTH      44
#define UI_IO_ITEM_WIDTH            18

#define UI_ITEM_HEIGHT              24
#define UI_COLUMN_SPACING           7
#define UI_IO_CIRCLE_RADIUS         4
#define UI_MANUAL_NODE_RADIUS       6
#define UI_AUTO_NODE_RADIUS         3

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
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "L01",
        .longName = "Loop 01"
    },
    {
        .id = 3,
        .type = UI_ITEM_LOOP,
        .order = 2,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_ACTIVE_CONFIRMED,
        .focus = UI_FOCUS_SELECTED,
        .shortName = "L02",
        .longName = "Loop 02"
    },
    {
        .id = 4,
        .type = UI_ITEM_LOOP,
        .order = 3,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_ACTIVE_UNCONFIRMED,
        .focus = UI_FOCUS_NONE,
        .shortName = "L03",
        .longName = "Loop 03"
    },
    {
        .id = 5,
        .type = UI_ITEM_OUTPUT,
        .order = 4,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "Out1",
        .longName = "Output 1"
    }
};


#define UI_ITEM_COUNT (sizeof(uiItems) / sizeof(uiItems[0]))

/* -------------------------------------------------------------------------- */
/* Temporary test connections                                                 */
/* -------------------------------------------------------------------------- */

/*
 * The connections are still manually defined.
 *
 * The routing algorithm will generate this list later.
 */
static UI_Connection uiConnections[] =
{
    {
        .sourceId = 1,
        .targetId = 2
    },

    {
        .sourceId = 2,
        .targetId = 3
    },

    {
        .sourceId = 3,
        .targetId = 4
    },

    {
        .sourceId = 4,
        .targetId = 5
    }
};


#define UI_CONNECTION_COUNT \
    (sizeof(uiConnections) / sizeof(uiConnections[0]))


/*
 * Calculated screen geometry for every test item.
 */
static UI_ItemGeometry uiGeometry[UI_ITEM_COUNT];

static void UI_DrawCircle(int16_t centerX, int16_t centerY,
                          int16_t radius, uint16_t color)
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

static void UI_FillCircle(int16_t centerX, int16_t centerY,
                          int16_t radius, uint16_t color)
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

static uint16_t UI_GetLoopFillColor(const UI_Item *item)
{
    if (item == NULL)
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

static uint16_t UI_GetTextColor(const UI_Item *item)
{
    if (item == NULL)
    {
        return UI_COLOR_TEXT_LIGHT;
    }

    if (item->type == UI_ITEM_LOOP &&
        item->loopStatus == UI_LOOP_STATUS_ACTIVE_UNCONFIRMED)
    {
        return UI_COLOR_TEXT_DARK;
    }

    return UI_COLOR_TEXT_LIGHT;
}

static const char *UI_GetDisplayedName(const UI_Item *item)
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

static uint16_t UI_GetItemWidth(const UI_Item *item)
{
    if (item == NULL)
    {
        return UI_NORMAL_ITEM_WIDTH;
    }

    if (item->type == UI_ITEM_AUTO_NODE)
    {
        return (UI_AUTO_NODE_RADIUS * 2) + 2;
    }

    if (item->type == UI_ITEM_MANUAL_NODE)
    {
        return (UI_MANUAL_NODE_RADIUS * 2) + 2;
    }

    if (item->type == UI_ITEM_INPUT || item->type == UI_ITEM_OUTPUT)
    {
        if (item->focus == UI_FOCUS_SELECTED ||
            item->focus == UI_FOCUS_GRABBED)
        {
            return UI_SELECTED_ITEM_WIDTH;
        }
        return UI_IO_ITEM_WIDTH;
    }

    if (item->focus == UI_FOCUS_SELECTED ||
        item->focus == UI_FOCUS_GRABBED)
    {
        return UI_SELECTED_ITEM_WIDTH;
    }

    return UI_NORMAL_ITEM_WIDTH;
}

static void UI_DrawCenteredText(uint16_t x, uint16_t y,
                                uint16_t width, uint16_t height,
                                const char *text, uint16_t foreground,
                                uint16_t background, uint8_t scale)
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

    Font5x7_DrawString(textX, textY, text,
                       foreground, background, scale);
}

static void UI_DrawLoop(const UI_Item *item, uint16_t x, uint16_t y)
{
    uint16_t width = UI_GetItemWidth(item);
    uint16_t fillColor = UI_GetLoopFillColor(item);
    uint16_t borderColor = UI_GetBorderColor(item);
    uint16_t textColor = UI_GetTextColor(item);
    const char *name = UI_GetDisplayedName(item);

    ST7735_FillRect(x, y, width, UI_ITEM_HEIGHT, fillColor);
    ST7735_DrawRect(x, y, width, UI_ITEM_HEIGHT, borderColor);

    if ((item->focus == UI_FOCUS_SELECTED ||
         item->focus == UI_FOCUS_GRABBED) &&
        width > 4 && UI_ITEM_HEIGHT > 4)
    {
        ST7735_DrawRect(x + 1, y + 1,
                        width - 2, UI_ITEM_HEIGHT - 2,
                        borderColor);
    }

    UI_DrawCenteredText(x, y, width, UI_ITEM_HEIGHT,
                        name, textColor, fillColor, 1);
}

static void UI_DrawIO(
    const UI_Item *item,
    uint16_t x,
    uint16_t y)
{
    uint16_t width =
        UI_GetItemWidth(item);

    uint16_t borderColor =
        UI_GetBorderColor(item);

    const char *name =
        UI_GetDisplayedName(item);


    /*
     * Der Kreis liegt exakt auf der horizontalen
     * Verbindungsebene der Loop-Kästen.
     */
    int16_t centerX =
        x + (width / 2);

    int16_t centerY =
        y + (UI_ITEM_HEIGHT / 2);


    UI_DrawCircle(
        centerX,
        centerY,
        UI_IO_CIRCLE_RADIUS,
        borderColor);


    /*
     * Kleiner Mittelpunkt als sichtbarer Anschluss.
     */
    ST7735_DrawPixel(
        centerX,
        centerY,
        borderColor);


    /*
     * Kurz- oder Langname unterhalb des Kreises.
     */
    uint16_t textY =
        (uint16_t)(
            centerY +
            UI_IO_CIRCLE_RADIUS +
            3
        );


    UI_DrawCenteredText(
        x,
        textY,
        width,
        7,
        name,
        UI_COLOR_TEXT_LIGHT,
        UI_COLOR_BACKGROUND,
        1);
}

static void UI_DrawManualNode(const UI_Item *item,
                              uint16_t x, uint16_t y)
{
    uint16_t width = UI_GetItemWidth(item);
    uint16_t color = UI_GetBorderColor(item);
    int16_t centerX = x + (width / 2);
    int16_t centerY = y + (UI_ITEM_HEIGHT / 2);

    ST7735_DrawLine(centerX, centerY - UI_MANUAL_NODE_RADIUS,
                    centerX + UI_MANUAL_NODE_RADIUS, centerY, color);
    ST7735_DrawLine(centerX + UI_MANUAL_NODE_RADIUS, centerY,
                    centerX, centerY + UI_MANUAL_NODE_RADIUS, color);
    ST7735_DrawLine(centerX, centerY + UI_MANUAL_NODE_RADIUS,
                    centerX - UI_MANUAL_NODE_RADIUS, centerY, color);
    ST7735_DrawLine(centerX - UI_MANUAL_NODE_RADIUS, centerY,
                    centerX, centerY - UI_MANUAL_NODE_RADIUS, color);
}

static void UI_DrawAutoNode(uint16_t x, uint16_t y)
{
    uint16_t width = (UI_AUTO_NODE_RADIUS * 2) + 2;
    int16_t centerX = x + (width / 2);
    int16_t centerY = y + (UI_ITEM_HEIGHT / 2);

    UI_FillCircle(centerX, centerY,
                  UI_AUTO_NODE_RADIUS, UI_COLOR_AUTO_NODE);
}

static void UI_DrawItem(const UI_Item *item, uint16_t x, uint16_t y)
{
    if (item == NULL)
    {
        return;
    }

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

void UI_Init(void)
{
}

/* -------------------------------------------------------------------------- */
/* Find item index by ID                                                      */
/* -------------------------------------------------------------------------- */

static int16_t UI_FindItemIndexById(uint16_t itemId)
{
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (uiItems[i].id == itemId)
        {
            return (int16_t)i;
        }
    }

    return -1;
}

/* -------------------------------------------------------------------------- */
/* Calculate item geometry                                                    */
/* -------------------------------------------------------------------------- */

static void UI_CalculateGeometry(void)
{
    uint16_t x = UI_MARGIN_LEFT;

    int16_t centerY =
        UI_DISPLAY_HEIGHT / 2;

    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        uint16_t itemWidth =
            UI_GetItemWidth(&uiItems[i]);

        int16_t itemY =
            centerY -
            (UI_ITEM_HEIGHT / 2);

        uiGeometry[i].itemId =
            uiItems[i].id;

        uiGeometry[i].x =
            (int16_t)x;

        uiGeometry[i].y =
            itemY;

        uiGeometry[i].width =
            itemWidth;

        uiGeometry[i].height =
            UI_ITEM_HEIGHT;

        uiGeometry[i].visible =
            0;


        if ((x + itemWidth) <=
            (UI_DISPLAY_WIDTH -
             UI_MARGIN_RIGHT))
        {
            uiGeometry[i].visible = 1;
        }


        x +=
            itemWidth +
            UI_COLUMN_SPACING;
    }
}

/* -------------------------------------------------------------------------- */
/* Connection points                                                          */
/* -------------------------------------------------------------------------- */

static int16_t UI_GetInputX(
    const UI_ItemGeometry *geometry,
    const UI_Item *item)
{
    if (item->type == UI_ITEM_INPUT ||
        item->type == UI_ITEM_OUTPUT)
    {
        int16_t centerX =
            geometry->x +
            (geometry->width / 2);

        return
            centerX -
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
        int16_t centerX =
            geometry->x +
            (geometry->width / 2);

        return
            centerX +
            UI_IO_CIRCLE_RADIUS;
    }


    return
        geometry->x +
        geometry->width -
        1;
}


static int16_t UI_GetConnectionY(
    const UI_ItemGeometry *geometry)
{
    return
        geometry->y +
        (geometry->height / 2);
}

/* -------------------------------------------------------------------------- */
/* Draw horizontal connection                                                 */
/* -------------------------------------------------------------------------- */

static void UI_DrawHorizontalConnection(
    int16_t x0,
    int16_t y,
    int16_t x1,
    uint16_t color)
{
    if (x1 <= x0)
    {
        return;
    }


    /*
     * Pfeilspitze endet unmittelbar vor dem Ziel-Item.
     */
    int16_t arrowTipX =
        x1 - 1;

    /*
     * Vier Pixel lange Pfeilspitze.
     */
    int16_t arrowBaseX =
        arrowTipX - 4;


    /*
     * Bei zu kurzem Abstand nur eine Linie zeichnen.
     */
    if (arrowBaseX <= x0)
    {
        ST7735_DrawLine(
            x0,
            y,
            arrowTipX,
            y,
            color);

        return;
    }


    /*
     * Horizontale Hauptlinie.
     */
    ST7735_DrawLine(
        x0,
        y,
        arrowBaseX,
        y,
        color);


    /*
     * Größere Pfeilspitze.
     */
    ST7735_DrawLine(
        arrowBaseX,
        y - 3,
        arrowTipX,
        y,
        color);

    ST7735_DrawLine(
        arrowBaseX,
        y + 3,
        arrowTipX,
        y,
        color);
}

/* -------------------------------------------------------------------------- */
/* Draw all connections                                                       */
/* -------------------------------------------------------------------------- */

static void UI_DrawConnections(void)
{
    for (uint16_t i = 0;
         i < UI_CONNECTION_COUNT;
         i++)
    {
        int16_t sourceIndex =
            UI_FindItemIndexById(
                uiConnections[i].sourceId);

        int16_t targetIndex =
            UI_FindItemIndexById(
                uiConnections[i].targetId);


        if (sourceIndex < 0 ||
            targetIndex < 0)
        {
            continue;
        }


        const UI_ItemGeometry *source =
            &uiGeometry[sourceIndex];

        const UI_ItemGeometry *target =
            &uiGeometry[targetIndex];


        /*
         * Connections are currently only drawn when
         * both items are inside the viewport.
         */
        if (!source->visible ||
            !target->visible)
        {
            continue;
        }


     int16_t sourceX =
    UI_GetOutputX(
        source,
        &uiItems[sourceIndex]
    );

int16_t targetX =
    UI_GetInputX(
        target,
        &uiItems[targetIndex]
    );

        int16_t sourceY =
            UI_GetConnectionY(source);

        int16_t targetY =
            UI_GetConnectionY(target);


        /*
         * Current test structure uses one lane only.
         */
        if (sourceY == targetY)
        {
            UI_DrawHorizontalConnection(
                sourceX + 1,
                sourceY,
                targetX,
                ST7735_WHITE);
        }
    }
}

void UI_Draw(void)
{
    /*
     * Clear complete display.
     */
    ST7735_FillScreen(
        UI_COLOR_BACKGROUND);


    /*
     * Calculate every item position before drawing.
     *
     * This is necessary because connections must know
     * the positions of source and target items.
     */
    UI_CalculateGeometry();


    /*
     * Draw connections first.
     *
     * Items are drawn afterward so that item borders
     * cover the endpoints of the connection lines.
     */
    UI_DrawConnections();


    /*
     * Draw items above the connection layer.
     */
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (!uiGeometry[i].visible)
        {
            continue;
        }


        UI_DrawItem(
            &uiItems[i],
            (uint16_t)uiGeometry[i].x,
            (uint16_t)uiGeometry[i].y);
    }
}
