#include "ui.h"
#include "st7735.h"
#include "font5x7.h"

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

#define UI_DISPLAY_WIDTH              160
#define UI_DISPLAY_HEIGHT             128
#define UI_VISIBLE_ELEMENT_COLUMNS    4
#define UI_ELEMENT_WIDTH              24
#define UI_ELEMENT_HEIGHT             24
#define UI_CONNECTION_WIDTH           18
#define UI_VERTICAL_CONNECTION_HEIGHT 16
#define UI_GRID_CONTENT_WIDTH \
    ((UI_VISIBLE_ELEMENT_COLUMNS * UI_ELEMENT_WIDTH) + \
     ((UI_VISIBLE_ELEMENT_COLUMNS - 1) * UI_CONNECTION_WIDTH))
#define UI_GRID_CONTENT_HEIGHT \
    ((3 * UI_ELEMENT_HEIGHT) + (2 * UI_VERTICAL_CONNECTION_HEIGHT))
#define UI_GRID_LEFT ((UI_DISPLAY_WIDTH - UI_GRID_CONTENT_WIDTH) / 2)
#define UI_GRID_TOP 2
#define UI_FOOTER_TOP (UI_GRID_TOP + UI_GRID_CONTENT_HEIGHT + 2)
#define UI_FOOTER_HEIGHT (UI_DISPLAY_HEIGHT - UI_FOOTER_TOP)

#define UI_IO_CIRCLE_RADIUS             7
#define UI_IO_DIAGONAL_OFFSET           5
#define UI_AUTO_NODE_RADIUS             6
#define UI_AUTO_NODE_DIAGONAL_OFFSET    4
#define UI_MANUAL_NODE_RADIUS           6
#define UI_MANUAL_NODE_DIAGONAL_OFFSET  3
#define UI_LOOP_CORNER_RADIUS           2
#define UI_LOOP_DIAGONAL_INSET          1
#define UI_FOCUS_FRAME_GAP              2
#define UI_FOCUS_CORNER_RADIUS          3
#define UI_ARROW_LENGTH                 5
#define UI_ARROW_HALF_WIDTH             3
#define UI_ARROW_FIXED_SCALE            256

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
        .lane = 0,
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


#define UI_ITEM_COUNT (sizeof(uiItems) / sizeof(uiItems[0]))

static UI_Connection uiConnections[] =
{
    /*
     * IN1 supplies the manual node and L01.
     */
    {
        .sourceId = 1,
        .targetId = 2
    },

    {
        .sourceId = 1,
        .targetId = 3
    },


    /*
     * One diagonal and one horizontal arrow
     * enter the automatic node.
     */
    {
        .sourceId = 2,
        .targetId = 4
    },

    {
        .sourceId = 3,
        .targetId = 4
    },


    /*
     * Horizontal output from automatic node.
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
/* Internal function prototypes                                               */
/* -------------------------------------------------------------------------- */

static void UI_DrawConnections(void);

static void UI_DrawConnectionsForItem(
    int16_t itemIndex
);

static void UI_UpdateChangedStructureDisplay(void);

static void UI_DrawItem(
    const UI_Item *item,
    int16_t x,
    int16_t y
);

static void UI_DrawFooter(void);



typedef struct
{
    int16_t x;
    int16_t y;
} UI_ConnectionPoint;

typedef struct
{
    int16_t order;
    int16_t lane;
    UI_FocusState focus;

} UI_ItemPositionState;


static UI_ItemPositionState previousStepState[
    UI_ITEM_COUNT
];

static UI_ItemGeometry previousStepGeometry[
    UI_ITEM_COUNT
];

static void UI_DrawCircle(int16_t cx, int16_t cy, int16_t radius,
                          uint16_t color)
{
    int16_t x = radius;
    int16_t y = 0;
    int16_t error = 1 - radius;

    while (x >= y)
    {
        ST7735_DrawPixel(cx + x, cy + y, color);
        ST7735_DrawPixel(cx + y, cy + x, color);
        ST7735_DrawPixel(cx - y, cy + x, color);
        ST7735_DrawPixel(cx - x, cy + y, color);
        ST7735_DrawPixel(cx - x, cy - y, color);
        ST7735_DrawPixel(cx - y, cy - x, color);
        ST7735_DrawPixel(cx + y, cy - x, color);
        ST7735_DrawPixel(cx + x, cy - y, color);
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

static void UI_FillCircle(int16_t cx, int16_t cy, int16_t radius,
                          uint16_t color)
{
    for (int16_t y = -radius; y <= radius; y++)
    {
        for (int16_t x = -radius; x <= radius; x++)
        {
            if ((x * x) + (y * y) <= (radius * radius))
            {
                ST7735_DrawPixel(cx + x, cy + y, color);
            }
        }
    }
}

static void UI_FillRoundedRect(int16_t x, int16_t y, uint16_t width,
                               uint16_t height, uint16_t radius,
                               uint16_t color)
{
    if (width == 0 || height == 0)
    {
        return;
    }

    if (radius == 0)
    {
        ST7735_FillRect((uint16_t)x, (uint16_t)y, width, height, color);
        return;
    }

    ST7735_FillRect((uint16_t)(x + radius), (uint16_t)y,
                    width - (2 * radius), height, color);
    ST7735_FillRect((uint16_t)x, (uint16_t)(y + radius),
                    radius, height - (2 * radius), color);
    ST7735_FillRect((uint16_t)(x + width - radius),
                    (uint16_t)(y + radius), radius,
                    height - (2 * radius), color);

    for (uint16_t row = 0; row < radius; row++)
    {
        uint16_t inset = radius - row - 1;
        uint16_t lineWidth = width - (2 * inset);
        ST7735_FillRect((uint16_t)(x + inset), (uint16_t)(y + row),
                        lineWidth, 1, color);
        ST7735_FillRect((uint16_t)(x + inset),
                        (uint16_t)(y + height - 1 - row),
                        lineWidth, 1, color);
    }
}

static void UI_DrawRoundedRect(int16_t x, int16_t y, uint16_t width,
                               uint16_t height, uint16_t radius,
                               uint16_t color)
{
    if (width == 0 || height == 0)
    {
        return;
    }

    ST7735_DrawLine(x + radius, y,
                    x + width - radius - 1, y, color);
    ST7735_DrawLine(x + radius, y + height - 1,
                    x + width - radius - 1, y + height - 1, color);
    ST7735_DrawLine(x, y + radius,
                    x, y + height - radius - 1, color);
    ST7735_DrawLine(x + width - 1, y + radius,
                    x + width - 1, y + height - radius - 1, color);

    ST7735_DrawPixel(x + 1, y + 1, color);
    ST7735_DrawPixel(x + width - 2, y + 1, color);
    ST7735_DrawPixel(x + 1, y + height - 2, color);
    ST7735_DrawPixel(x + width - 2, y + height - 2, color);
}

static int32_t UI_TriangleEdge(int16_t ax, int16_t ay,
                               int16_t bx, int16_t by,
                               int16_t px, int16_t py)
{
    return ((int32_t)(px - ax) * (int32_t)(by - ay)) -
           ((int32_t)(py - ay) * (int32_t)(bx - ax));
}

static void UI_FillTriangle(int16_t x0, int16_t y0,
                            int16_t x1, int16_t y1,
                            int16_t x2, int16_t y2,
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
            int32_t e0 = UI_TriangleEdge(x0, y0, x1, y1, x, y);
            int32_t e1 = UI_TriangleEdge(x1, y1, x2, y2, x, y);
            int32_t e2 = UI_TriangleEdge(x2, y2, x0, y0, x, y);
            uint8_t hasNegative = (e0 < 0) || (e1 < 0) || (e2 < 0);
            uint8_t hasPositive = (e0 > 0) || (e1 > 0) || (e2 > 0);

            if (!(hasNegative && hasPositive))
            {
                ST7735_DrawPixel(x, y, color);
            }
        }
    }
}

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

static uint16_t UI_GetTextColor(
    const UI_Item *item)
{
    if (item == NULL)
    {
        return UI_COLOR_TEXT_LIGHT;
    }

    if (item->type == UI_ITEM_LOOP)
    {
        if (item->loopStatus ==
                UI_LOOP_STATUS_ACTIVE_CONFIRMED ||
            item->loopStatus ==
                UI_LOOP_STATUS_ACTIVE_UNCONFIRMED)
        {
            return UI_COLOR_TEXT_DARK;
        }
    }

    return UI_COLOR_TEXT_LIGHT;
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

static int16_t UI_GetElementX(int16_t visibleOrder)
{
    return UI_GRID_LEFT +
           (visibleOrder * (UI_ELEMENT_WIDTH + UI_CONNECTION_WIDTH));
}

static int16_t UI_GetLaneY(int16_t lane)
{
    int16_t laneIndex = 1 - lane;
    return UI_GRID_TOP +
           (laneIndex *
            (UI_ELEMENT_HEIGHT + UI_VERTICAL_CONNECTION_HEIGHT));
}

static uint8_t UI_IsLaneVisible(int16_t lane)
{
    return (lane >= -1 && lane <= 1) ? 1 : 0;
}

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
/* Clear one item area                                                        */
/* -------------------------------------------------------------------------- */

static void UI_EraseFocusFrame(
    int16_t itemIndex)
{
    if (itemIndex < 0 ||
        itemIndex >= (int16_t)UI_ITEM_COUNT)
    {
        return;
    }


    if (!uiGeometry[itemIndex].visible)
    {
        return;
    }


    const UI_Item *item =
        &uiItems[itemIndex];

    const UI_ItemGeometry *geometry =
        &uiGeometry[itemIndex];


    int16_t centerX =
        geometry->x +
        (geometry->width / 2);

    int16_t centerY =
        geometry->y +
        (geometry->height / 2);


    switch (item->type)
    {
        case UI_ITEM_LOOP:
        {
            /*
             * Nur den äußeren Fokusrahmen löschen.
             *
             * Der eigentliche Loopkasten bleibt
             * vollständig unangetastet.
             */
            UI_DrawRoundedRect(
                geometry->x -
                    UI_FOCUS_FRAME_GAP,

                geometry->y -
                    UI_FOCUS_FRAME_GAP,

                geometry->width +
                    (2 * UI_FOCUS_FRAME_GAP),

                geometry->height +
                    (2 * UI_FOCUS_FRAME_GAP),

                UI_FOCUS_CORNER_RADIUS,

                UI_COLOR_BACKGROUND
            );

            break;
        }


        case UI_ITEM_MANUAL_NODE:
        {
            /*
             * Äußeren Fokusdiamanten löschen.
             */
            int16_t radius =
                UI_MANUAL_NODE_RADIUS +
                UI_FOCUS_FRAME_GAP;


            ST7735_DrawLine(
                centerX,
                centerY - radius,
                centerX + radius,
                centerY,
                UI_COLOR_BACKGROUND
            );

            ST7735_DrawLine(
                centerX + radius,
                centerY,
                centerX,
                centerY + radius,
                UI_COLOR_BACKGROUND
            );

            ST7735_DrawLine(
                centerX,
                centerY + radius,
                centerX - radius,
                centerY,
                UI_COLOR_BACKGROUND
            );

            ST7735_DrawLine(
                centerX - radius,
                centerY,
                centerX,
                centerY - radius,
                UI_COLOR_BACKGROUND
            );

            break;
        }


        case UI_ITEM_INPUT:
        case UI_ITEM_OUTPUT:
        {
            /*
             * IN und OUT besitzen keinen separaten Rahmen.
             *
             * Der Kreis wird später mit der normalen
             * weißen Farbe neu gezeichnet.
             */
            break;
        }


        case UI_ITEM_AUTO_NODE:
        default:
            break;
    }
}

static void UI_ClearGeometryArea(
    const UI_ItemGeometry *geometry)
{
    if (geometry == NULL ||
        !geometry->visible)
    {
        return;
    }

    int16_t x =
        geometry->x -
        UI_FOCUS_FRAME_GAP -
        1;

    int16_t y =
        geometry->y -
        UI_FOCUS_FRAME_GAP -
        1;

    int16_t width =
        geometry->width +
        (2 * UI_FOCUS_FRAME_GAP) +
        2;

    int16_t height =
        geometry->height +
        (2 * UI_FOCUS_FRAME_GAP) +
        2;


    if (x < 0)
    {
        width += x;
        x = 0;
    }

    if (y < 0)
    {
        height += y;
        y = 0;
    }

    if ((x + width) >
        UI_DISPLAY_WIDTH)
    {
        width =
            UI_DISPLAY_WIDTH - x;
    }

    if ((y + height) >
        UI_FOOTER_TOP)
    {
        height =
            UI_FOOTER_TOP - y;
    }

    if (width <= 0 ||
        height <= 0)
    {
        return;
    }


    ST7735_FillRect(
        (uint16_t)x,
        (uint16_t)y,
        (uint16_t)width,
        (uint16_t)height,
        UI_COLOR_BACKGROUND
    );
}

/* -------------------------------------------------------------------------- */
/* Redraw one item                                                            */
/* -------------------------------------------------------------------------- */



static void UI_RedrawItem(int16_t itemIndex)
{
    if (itemIndex < 0 ||
        itemIndex >= (int16_t)UI_ITEM_COUNT)
    {
        return;
    }


    if (!uiGeometry[itemIndex].visible)
    {
        return;
    }


    UI_DrawItem(
        &uiItems[itemIndex],
        uiGeometry[itemIndex].x,
        uiGeometry[itemIndex].y
    );
}

/* -------------------------------------------------------------------------- */
/* Partial selection update                                                   */
/* -------------------------------------------------------------------------- */

static void UI_UpdateSelectionDisplay(
    int16_t oldIndex,
    int16_t newIndex)
{
    /*
     * Alten äußeren Auswahl- oder Greifrahmen
     * gezielt entfernen.
     */
    UI_EraseFocusFrame(oldIndex);


    /*
     * Verbindungen des alten Elements können durch
     * das Entfernen des Fokusrahmens teilweise
     * überzeichnet worden sein.
     */
    UI_DrawConnectionsForItem(
        oldIndex
    );


    /*
     * Verbindungen des neuen Elements vorsorglich
     * vor dem Item neu zeichnen.
     */
    if (newIndex != oldIndex)
    {
        UI_DrawConnectionsForItem(
            newIndex
        );
    }


    /*
     * Items werden über den Verbindungslinien
     * gezeichnet.
     */
    UI_RedrawItem(
        oldIndex
    );


    if (newIndex != oldIndex)
    {
        UI_RedrawItem(
            newIndex
        );
    }


    /*
     * Footerbereich aktualisieren.
     */
    UI_DrawFooter();
}


/* -------------------------------------------------------------------------- */
/* Selection                                                                  */
/* -------------------------------------------------------------------------- */

static uint8_t UI_IsSelectable(
    const UI_Item *item)
{
    if (item == NULL)
    {
        return 0;
    }

    switch (item->type)
    {
        case UI_ITEM_INPUT:
        case UI_ITEM_OUTPUT:
        case UI_ITEM_LOOP:
        case UI_ITEM_MANUAL_NODE:
            return 1;

        case UI_ITEM_AUTO_NODE:
        default:
            return 0;
    }
}

static uint8_t UI_IsPermanentItem(
    const UI_Item *item)
{
    if (item == NULL)
    {
        return 0;
    }

    switch (item->type)
    {
        case UI_ITEM_INPUT:
        case UI_ITEM_OUTPUT:
        case UI_ITEM_LOOP:
        case UI_ITEM_MANUAL_NODE:
            return 1;

        case UI_ITEM_AUTO_NODE:
        default:
            return 0;
    }
}

static int8_t UI_CompareItemPositions(
    const UI_Item *itemA,
    const UI_Item *itemB)
{
    if (itemA == NULL || itemB == NULL)
    {
        return 0;
    }

    /*
     * Kleinere order-Werte liegen weiter links
     * und kommen zuerst.
     */
    if (itemA->order < itemB->order)
    {
        return -1;
    }

    if (itemA->order > itemB->order)
    {
        return 1;
    }

    /*
     * Bei gleicher Spalte:
     * höhere Lane kommt zuerst.
     *
     * Beispiel:
     * lane +1
     * lane  0
     * lane -1
     */
    if (itemA->lane > itemB->lane)
    {
        return -1;
    }

    if (itemA->lane < itemB->lane)
    {
        return 1;
    }

    /*
     * Sicherheitsregel für unerwartet gleiche
     * Rasterpositionen.
     */
    if (itemA->id < itemB->id)
    {
        return -1;
    }

    if (itemA->id > itemB->id)
    {
        return 1;
    }

    return 0;
}

static int16_t UI_FindNextSelectableIndex(
    int16_t currentIndex)
{
    if (currentIndex < 0 ||
        currentIndex >= (int16_t)UI_ITEM_COUNT)
    {
        return -1;
    }

    int16_t bestIndex = -1;

    const UI_Item *currentItem =
        &uiItems[currentIndex];

    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if ((int16_t)i == currentIndex)
        {
            continue;
        }

        if (!UI_IsSelectable(&uiItems[i]))
        {
            continue;
        }

        /*
         * Der Kandidat muss hinter dem aktuellen
         * Item in der Rasterreihenfolge liegen.
         */
        if (UI_CompareItemPositions(
                &uiItems[i],
                currentItem) <= 0)
        {
            continue;
        }

        /*
         * Ersten passenden Kandidaten übernehmen
         * oder einen näheren Kandidaten finden.
         */
        if (bestIndex < 0 ||
            UI_CompareItemPositions(
                &uiItems[i],
                &uiItems[bestIndex]) < 0)
        {
            bestIndex = (int16_t)i;
        }
    }

    return bestIndex;
}

static int16_t UI_FindPreviousSelectableIndex(
    int16_t currentIndex)
{
    if (currentIndex < 0 ||
        currentIndex >= (int16_t)UI_ITEM_COUNT)
    {
        return -1;
    }

    int16_t bestIndex = -1;

    const UI_Item *currentItem =
        &uiItems[currentIndex];

    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if ((int16_t)i == currentIndex)
        {
            continue;
        }

        if (!UI_IsSelectable(&uiItems[i]))
        {
            continue;
        }

        /*
         * Der Kandidat muss vor dem aktuellen Item
         * in der Rasterreihenfolge liegen.
         */
        if (UI_CompareItemPositions(
                &uiItems[i],
                currentItem) >= 0)
        {
            continue;
        }

        /*
         * Gesucht wird der größte Kandidat,
         * der noch vor dem aktuellen Item liegt.
         */
        if (bestIndex < 0 ||
            UI_CompareItemPositions(
                &uiItems[i],
                &uiItems[bestIndex]) > 0)
        {
            bestIndex = (int16_t)i;
        }
    }

    return bestIndex;
}

static int16_t UI_FindFirstSelectableIndex(void)
{
    int16_t bestIndex = -1;

    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (!UI_IsSelectable(&uiItems[i]))
        {
            continue;
        }

        if (bestIndex < 0 ||
            UI_CompareItemPositions(
                &uiItems[i],
                &uiItems[bestIndex]) < 0)
        {
            bestIndex = (int16_t)i;
        }
    }

    return bestIndex;
}

static int16_t UI_FindLastSelectableIndex(void)
{
    int16_t bestIndex = -1;

    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (!UI_IsSelectable(&uiItems[i]))
        {
            continue;
        }

        if (bestIndex < 0 ||
            UI_CompareItemPositions(
                &uiItems[i],
                &uiItems[bestIndex]) > 0)
        {
            bestIndex = (int16_t)i;
        }
    }

    return bestIndex;
}

static int16_t UI_FindPermanentItemAt(
    int16_t order,
    int16_t lane,
    int16_t excludedIndex)
{
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if ((int16_t)i == excludedIndex)
        {
            continue;
        }

        if (!UI_IsPermanentItem(
                &uiItems[i]))
        {
            continue;
        }

        if (uiItems[i].order == order &&
            uiItems[i].lane == lane)
        {
            return (int16_t)i;
        }
    }

    return -1;
}


static void UI_RemoveAllAutoNodes(void)
{
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (uiItems[i].type ==
            UI_ITEM_AUTO_NODE)
        {
            /*
             * Testlösung:
             * Automatischen Knoten außerhalb der
             * sichtbaren Struktur ablegen.
             */
            uiItems[i].order = -100;
            uiItems[i].lane = 0;
        }
    }
}

static void UI_NormalizeOrders(void)
{
    int16_t oldOrders[UI_ITEM_COUNT];


    /*
     * Ursprüngliche order-Werte sichern.
     *
     * Die Sicherung ist notwendig, weil die neuen
     * Werte nicht während der Berechnung die noch
     * auszuwertenden alten Werte verändern dürfen.
     */
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        oldOrders[i] =
            uiItems[i].order;
    }


    for (uint16_t itemIndex = 0;
         itemIndex < UI_ITEM_COUNT;
         itemIndex++)
    {
        if (!UI_IsPermanentItem(
                &uiItems[itemIndex]))
        {
            continue;
        }

        if (oldOrders[itemIndex] < 0)
        {
            continue;
        }


        int16_t normalizedOrder = 0;


        /*
         * Anzahl der unterschiedlichen belegten
         * Spalten links vom aktuellen Item bestimmen.
         */
        for (uint16_t candidateIndex = 0;
             candidateIndex < UI_ITEM_COUNT;
             candidateIndex++)
        {
            if (!UI_IsPermanentItem(
                    &uiItems[candidateIndex]))
            {
                continue;
            }

            int16_t candidateOrder =
                oldOrders[candidateIndex];


            if (candidateOrder < 0 ||
                candidateOrder >=
                    oldOrders[itemIndex])
            {
                continue;
            }


            /*
             * Prüfen, ob dieser kleinere order-Wert
             * bereits zuvor gezählt wurde.
             */
            uint8_t alreadyCounted = 0;


            for (uint16_t previousIndex = 0;
                 previousIndex <
                    candidateIndex;
                 previousIndex++)
            {
                if (!UI_IsPermanentItem(
                        &uiItems[previousIndex]))
                {
                    continue;
                }

                if (oldOrders[previousIndex] ==
                    candidateOrder)
                {
                    alreadyCounted = 1;
                    break;
                }
            }


            if (!alreadyCounted)
            {
                normalizedOrder++;
            }
        }


        uiItems[itemIndex].order =
            normalizedOrder;
    }
}

static int16_t UI_GetMinimumPermanentOrder(void)
{
    int16_t minimumOrder = 32767;
    uint8_t found = 0;


    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (!UI_IsPermanentItem(
                &uiItems[i]))
        {
            continue;
        }

        if (uiItems[i].order < 0)
        {
            continue;
        }

        if (!found ||
            uiItems[i].order <
                minimumOrder)
        {
            minimumOrder =
                uiItems[i].order;

            found = 1;
        }
    }


    return found ? minimumOrder : -1;
}

static int16_t UI_GetMaximumPermanentOrder(void)
{
    int16_t maximumOrder = -32768;
    uint8_t found = 0;


    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (!UI_IsPermanentItem(
                &uiItems[i]))
        {
            continue;
        }

        if (uiItems[i].order < 0)
        {
            continue;
        }

        if (!found ||
            uiItems[i].order >
                maximumOrder)
        {
            maximumOrder =
                uiItems[i].order;

            found = 1;
        }
    }


    return found ? maximumOrder : -1;
}

static uint8_t UI_ValidateEdgeRules(void)
{
    int16_t minimumOrder =
        UI_GetMinimumPermanentOrder();

    int16_t maximumOrder =
        UI_GetMaximumPermanentOrder();


    /*
     * Es muss mindestens eine permanente Spalte geben.
     */
    if (minimumOrder < 0 ||
        maximumOrder < 0)
    {
        return 0;
    }


    /*
     * Linke und rechte Randspalte müssen
     * voneinander verschieden sein.
     *
     * In einer gültigen Struktur existieren
     * mindestens eine IN- und eine OUT-Spalte.
     */
    if (minimumOrder >= maximumOrder)
    {
        return 0;
    }


    uint8_t inputAtLeftEdge = 0;
    uint8_t outputAtRightEdge = 0;


    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (!UI_IsPermanentItem(
                &uiItems[i]))
        {
            continue;
        }


        /*
         * Äußerste linke Spalte:
         * Hier dürfen ausschließlich Inputs liegen.
         */
        if (uiItems[i].order ==
            minimumOrder)
        {
            if (uiItems[i].type !=
                UI_ITEM_INPUT)
            {
                return 0;
            }

            inputAtLeftEdge = 1;
        }


        /*
         * Äußerste rechte Spalte:
         * Hier dürfen ausschließlich Outputs liegen.
         */
        if (uiItems[i].order ==
            maximumOrder)
        {
            if (uiItems[i].type !=
                UI_ITEM_OUTPUT)
            {
                return 0;
            }

            outputAtRightEdge = 1;
        }
    }


    /*
     * Zusätzlich muss tatsächlich mindestens
     * ein Input links und ein Output rechts liegen.
     */
    if (!inputAtLeftEdge ||
        !outputAtRightEdge)
    {
        return 0;
    }


    return 1;
}

static void UI_EnsureEdgeColumns(void)
{
    int16_t minimumOrder =
        UI_GetMinimumPermanentOrder();

    int16_t maximumOrder =
        UI_GetMaximumPermanentOrder();


    if (minimumOrder < 0 ||
        maximumOrder < 0)
    {
        return;
    }


    uint8_t invalidLeftEdge = 0;
    uint8_t invalidRightEdge = 0;


    /*
     * Prüfen, ob die äußersten Spalten noch
     * unerlaubte Itemtypen enthalten.
     */
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (!UI_IsPermanentItem(
                &uiItems[i]))
        {
            continue;
        }


        if (uiItems[i].order ==
                minimumOrder &&
            uiItems[i].type !=
                UI_ITEM_INPUT)
        {
            invalidLeftEdge = 1;
        }


        if (uiItems[i].order ==
                maximumOrder &&
            uiItems[i].type !=
                UI_ITEM_OUTPUT)
        {
            invalidRightEdge = 1;
        }
    }


    /*
     * Ungültige linke Randspalte:
     *
     * Alle Nicht-Inputs werden um eine Spalte
     * nach rechts verschoben.
     *
     * Inputs bleiben in order 0.
     */
    if (invalidLeftEdge)
    {
        for (uint16_t i = 0;
             i < UI_ITEM_COUNT;
             i++)
        {
            if (!UI_IsPermanentItem(
                    &uiItems[i]))
            {
                continue;
            }


            if (uiItems[i].type ==
                UI_ITEM_INPUT)
            {
                uiItems[i].order = 0;
            }
            else
            {
                uiItems[i].order++;
            }
        }
    }


    /*
     * Nach einer möglichen linken Erweiterung
     * die äußerste Nicht-Output-Spalte neu bestimmen.
     */
    if (invalidRightEdge)
    {
        int16_t maximumNonOutputOrder = -1;


        for (uint16_t i = 0;
             i < UI_ITEM_COUNT;
             i++)
        {
            if (!UI_IsPermanentItem(
                    &uiItems[i]))
            {
                continue;
            }


            if (uiItems[i].type ==
                UI_ITEM_OUTPUT)
            {
                continue;
            }


            if (uiItems[i].order >
                maximumNonOutputOrder)
            {
                maximumNonOutputOrder =
                    uiItems[i].order;
            }
        }


        if (maximumNonOutputOrder >= 0)
        {
            int16_t newOutputOrder =
                maximumNonOutputOrder + 1;


            for (uint16_t i = 0;
                 i < UI_ITEM_COUNT;
                 i++)
            {
                if (uiItems[i].type ==
                    UI_ITEM_OUTPUT)
                {
                    uiItems[i].order =
                        newOutputOrder;
                }
            }
        }
    }


    /*
     * Vollständig leere Spalten schließen.
     */
    UI_NormalizeOrders();
}

static int16_t UI_GetFocusedIndex(void)
{
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (uiItems[i].focus == UI_FOCUS_SELECTED ||
            uiItems[i].focus == UI_FOCUS_GRABBED)
        {
            return (int16_t)i;
        }
    }

    return -1;
}

static uint8_t UI_IsItemGrabbed(void)
{
    int16_t focusedIndex =
        UI_GetFocusedIndex();

    if (focusedIndex < 0)
    {
        return 0;
    }

    return
        (uiItems[focusedIndex].focus ==
         UI_FOCUS_GRABBED) ?
        1 :
        0;
}


uint8_t UI_IsGrabbed(void)
{
    return UI_IsItemGrabbed();
}

static void UI_SetSelectedIndex(
    int16_t newIndex)
{
    if (newIndex < 0 ||
        newIndex >= (int16_t)UI_ITEM_COUNT)
    {
        return;
    }

    if (!UI_IsSelectable(
            &uiItems[newIndex]))
    {
        return;
    }

    int16_t oldIndex =
        UI_GetFocusedIndex();

    /*
     * Solange ein Item gegriffen ist, darf der
     * Fokus nicht auf ein anderes Item wechseln.
     */
    if (oldIndex >= 0 &&
        uiItems[oldIndex].focus ==
            UI_FOCUS_GRABBED)
    {
        return;
    }

    if (oldIndex == newIndex)
    {
        return;
    }

    /*
     * Invariante:
     * Es darf immer nur genau ein Item ausgewählt
     * oder gegriffen sein.
     */
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        uiItems[i].focus =
            UI_FOCUS_NONE;
    }

    uiItems[newIndex].focus =
        UI_FOCUS_SELECTED;

    UI_UpdateSelectionDisplay(
        oldIndex,
        newIndex
    );
}



void UI_SelectNext(void)
{
    /*
     * Im Grab-Modus darf der Fokus nicht wandern.
     */
    if (UI_IsItemGrabbed())
    {
        return;
    }

    int16_t currentIndex =
        UI_GetFocusedIndex();

    /*
     * Falls noch kein Fokus existiert,
     * linkestes/oberstes Item auswählen.
     */
    if (currentIndex < 0)
    {
        int16_t firstIndex =
            UI_FindFirstSelectableIndex();

        if (firstIndex >= 0)
        {
            UI_SetSelectedIndex(firstIndex);
        }

        return;
    }

    int16_t nextIndex =
        UI_FindNextSelectableIndex(
            currentIndex
        );

    /*
     * Kein Wrap-around.
     * Am letzten Item bleibt die Auswahl stehen.
     */
    if (nextIndex < 0)
    {
        return;
    }

    UI_SetSelectedIndex(nextIndex);
}



void UI_SelectPrevious(void)
{
    /*
     * Im Grab-Modus darf der Fokus nicht wandern.
     */
    if (UI_IsItemGrabbed())
    {
        return;
    }

    int16_t currentIndex =
        UI_GetFocusedIndex();

    /*
     * Falls noch kein Fokus existiert,
     * rechtest/unterstes Item auswählen.
     */
    if (currentIndex < 0)
    {
        int16_t lastIndex =
            UI_FindLastSelectableIndex();

        if (lastIndex >= 0)
        {
            UI_SetSelectedIndex(lastIndex);
        }

        return;
    }

    int16_t previousIndex =
        UI_FindPreviousSelectableIndex(
            currentIndex
        );

    /*
     * Kein Wrap-around.
     */
    if (previousIndex < 0)
    {
        return;
    }

    UI_SetSelectedIndex(previousIndex);
}

void UI_ToggleGrab(void)
{
    int16_t focusedIndex =
        UI_GetFocusedIndex();

    if (focusedIndex < 0)
    {
        return;
    }

    /*
     * Automatische Knoten können grundsätzlich
     * nicht ausgewählt oder gegriffen werden.
     */
    if (!UI_IsSelectable(
            &uiItems[focusedIndex]))
    {
        return;
    }

    /*
     * Alten Fokusrahmen in der bisherigen Farbe
     * entfernen, bevor der Zustand geändert wird.
     */
    UI_EraseFocusFrame(
        focusedIndex
    );

    if (uiItems[focusedIndex].focus ==
        UI_FOCUS_SELECTED)
    {
        uiItems[focusedIndex].focus =
            UI_FOCUS_GRABBED;
    }
    else if (uiItems[focusedIndex].focus ==
             UI_FOCUS_GRABBED)
    {
        uiItems[focusedIndex].focus =
            UI_FOCUS_SELECTED;
    }

    /*
     * Verbindungen neu zeichnen, falls beim Löschen
     * des äußeren Rahmens einzelne Pixel betroffen
     * waren.
     */
    UI_DrawConnectionsForItem(
        focusedIndex
    );

    /*
     * Dasselbe Item mit der neuen Fokusfarbe
     * wieder zeichnen.
     */
    UI_RedrawItem(
        focusedIndex
    );

    /*
     * Footer auf Blau oder Pink aktualisieren.
     */
    UI_DrawFooter();
}

/* -------------------------------------------------------------------------- */
/* Movement snapshots                                                         */
/* -------------------------------------------------------------------------- */

static uint8_t UI_ItemPositionChanged(
    uint16_t itemIndex)
{
    if (itemIndex >= UI_ITEM_COUNT)
    {
        return 0;
    }

    if (!UI_IsPermanentItem(
            &uiItems[itemIndex]))
    {
        return 0;
    }

    return
        (uiItems[itemIndex].order !=
             previousStepState[itemIndex].order ||
         uiItems[itemIndex].lane !=
             previousStepState[itemIndex].lane) ?
        1 :
        0;
}

static void UI_SavePreviousStepState(void)
{
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        previousStepState[i].order =
            uiItems[i].order;

        previousStepState[i].lane =
            uiItems[i].lane;

        previousStepState[i].focus =
            uiItems[i].focus;


        /*
         * Alte Bildschirmposition speichern.
         */
        previousStepGeometry[i] =
            uiGeometry[i];
    }
}



static void UI_RestorePreviousStepState(void)
{
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        uiItems[i].order =
            previousStepState[i].order;

        uiItems[i].lane =
            previousStepState[i].lane;

        uiItems[i].focus =
            previousStepState[i].focus;
    }
}


static uint8_t
UI_PermanentStructureChangedSincePreviousStep(void)
{
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (!UI_IsPermanentItem(
                &uiItems[i]))
        {
            continue;
        }

        if (uiItems[i].order !=
                previousStepState[i].order ||
            uiItems[i].lane !=
                previousStepState[i].lane)
        {
            return 1;
        }
    }

    return 0;
}


static void UI_MoveGrabbedHorizontal(
    int8_t direction)
{
    if (direction == 0)
    {
        return;
    }


    int16_t grabbedIndex =
        UI_GetFocusedIndex();


    if (grabbedIndex < 0)
    {
        return;
    }


    if (uiItems[grabbedIndex].focus !=
        UI_FOCUS_GRABBED)
    {
        return;
    }


    UI_Item *grabbedItem =
        &uiItems[grabbedIndex];


    if (!UI_IsPermanentItem(
            grabbedItem))
    {
        return;
    }


    /*
     * Inputs und Outputs selbst bleiben vorerst
     * fest in ihren Randspalten.
     */
    if (grabbedItem->type ==
            UI_ITEM_INPUT ||
        grabbedItem->type ==
            UI_ITEM_OUTPUT)
    {
        return;
    }


    /*
     * Vollständigen Zustand vor diesem einzelnen
     * Encoderschritt sichern.
     */
    UI_SavePreviousStepState();


    int16_t targetOrder =
        grabbedItem->order +
        ((direction > 0) ? 1 : -1);


    /*
     * Links von order 0 existiert zunächst keine
     * direkte Zielposition.
     *
     * Die neue Input-Randspalte entsteht erst,
     * wenn ein Item die bisherige order 0 betritt.
     */
    if (targetOrder < 0)
    {
        return;
    }


    /*
     * Das gegriffene Item darf zunächst nur eine
     * aktuell existierende sichtbare Spalte betreten.
     *
     * Die Randkorrektur kann danach eine weitere
     * Spalte erzeugen.
     */
    if (targetOrder >=
        UI_VISIBLE_ELEMENT_COLUMNS)
    {
        return;
    }


    int16_t targetPermanentIndex =
        UI_FindPermanentItemAt(
            targetOrder,
            grabbedItem->lane,
            grabbedIndex
        );


    /*
     * Ziel ist ein normales permanentes Item.
     */
    if (targetPermanentIndex >= 0)
    {
        UI_ItemType targetType =
            uiItems[
                targetPermanentIndex
            ].type;


        if ((targetType ==
                 UI_ITEM_INPUT &&
             direction < 0) ||
            (targetType ==
                 UI_ITEM_OUTPUT &&
             direction > 0))
        {
            /*
             * Das gegriffene Item betritt die
             * bisherige Randspalte.
             *
             * Kein Tausch mit IN beziehungsweise OUT.
             * UI_EnsureEdgeColumns() verschiebt den
             * Rand anschließend nach außen.
             */
            grabbedItem->order =
                uiItems[
                    targetPermanentIndex
                ].order;
        }
        else
        {
            /*
             * Zwei normale permanente Items tauschen
             * ihre vollständigen Rasterpositionen.
             */
            int16_t grabbedOldOrder =
                grabbedItem->order;

            int16_t grabbedOldLane =
                grabbedItem->lane;

            int16_t targetOldOrder =
                uiItems[
                    targetPermanentIndex
                ].order;

            int16_t targetOldLane =
                uiItems[
                    targetPermanentIndex
                ].lane;


            uiItems[
                targetPermanentIndex
            ].order =
                grabbedOldOrder;

            uiItems[
                targetPermanentIndex
            ].lane =
                grabbedOldLane;


            grabbedItem->order =
                targetOldOrder;

            grabbedItem->lane =
                targetOldLane;
        }
    }
    else
    {
        /*
         * Zielposition ist leer oder enthält nur
         * einen automatischen Knoten.
         */
        grabbedItem->order =
            targetOrder;
    }


    /*
     * Nach jeder Strukturänderung werden alle
     * automatischen Knoten entfernt.
     */
/*
 * Automatische Knoten nach einer vorläufigen
 * Strukturänderung entfernen.
 */
UI_RemoveAllAutoNodes();


/*
 * Gültige IN-/OUT-Randspalten herstellen und
 * vollständig leere Spalten schließen.
 */
UI_EnsureEdgeColumns();


/*
 * Der normalisierte Zustand muss gültige
 * Randspalten besitzen.
 */
if (!UI_ValidateEdgeRules())
{
    UI_RestorePreviousStepState();

    return;
}


/*
 * Entscheidend ist der normalisierte Endzustand.
 *
 * Falls alle permanenten Items wieder dieselben
 * Positionen besitzen wie vor dem Encoderschritt,
 * hatte die Bewegung keine dauerhafte Wirkung.
 *
 * Der komplette Snapshot wird wiederhergestellt,
 * damit auch die vorläufig entfernten automatischen
 * Knoten erhalten bleiben.
 */
if (!UI_PermanentStructureChangedSincePreviousStep())
{
    UI_RestorePreviousStepState();

    /*
     * Kein UI_Draw():
     * Auf dem Display wurde bisher noch nichts
     * verändert.
     */
    return;
}


/*
 * Nur bei einer tatsächlich wirksamen Änderung
 * den Bildschirm neu aufbauen.
 */
UI_UpdateChangedStructureDisplay();
}

void UI_HandleEncoderStep(
    int8_t direction)
{
    if (direction == 0)
    {
        return;
    }

    if (UI_IsGrabbed())
    {
        UI_MoveGrabbedHorizontal(
            direction
        );

        return;
    }

    if (direction > 0)
    {
        UI_SelectNext();
    }
    else
    {
        UI_SelectPrevious();
    }
}

static void UI_DrawLoop(const UI_Item *item, int16_t x, int16_t y)
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

    UI_FillRoundedRect(x, y, UI_ELEMENT_WIDTH, UI_ELEMENT_HEIGHT,
                       UI_LOOP_CORNER_RADIUS, fillColor);
    UI_DrawRoundedRect(x, y, UI_ELEMENT_WIDTH, UI_ELEMENT_HEIGHT,
                       UI_LOOP_CORNER_RADIUS, UI_COLOR_BORDER_NORMAL);

    UI_DrawCenteredText((uint16_t)x, (uint16_t)y,
                        UI_ELEMENT_WIDTH, UI_ELEMENT_HEIGHT,
                        item->shortName, textColor, fillColor, 1);
}

static void UI_DrawIO(const UI_Item *item, int16_t x, int16_t y)
{
    uint16_t borderColor = UI_GetFocusColor(item);
    int16_t centerX = x + (UI_ELEMENT_WIDTH / 2);
    int16_t centerY = y + (UI_ELEMENT_HEIGHT / 2);

    UI_DrawCircle(centerX, centerY, UI_IO_CIRCLE_RADIUS, borderColor);
    ST7735_DrawPixel(centerX, centerY, borderColor);

    uint16_t textWidth = Font5x7_GetStringWidth(item->shortName, 1);
    int16_t textX = centerX - ((int16_t)textWidth / 2);

    if (textX < x)
    {
        textX = x;
    }

    if ((textX + (int16_t)textWidth) > (x + UI_ELEMENT_WIDTH))
    {
        textX = x + UI_ELEMENT_WIDTH - (int16_t)textWidth;
    }

    uint16_t textY =
        (uint16_t)(centerY + UI_IO_CIRCLE_RADIUS + 3);

    Font5x7_DrawString((uint16_t)textX, textY, item->shortName,
                       UI_COLOR_TEXT_LIGHT, UI_COLOR_BACKGROUND, 1);
}

static void UI_DrawManualNode(const UI_Item *item,
                              int16_t x, int16_t y)
{
    int16_t centerX = x + (UI_ELEMENT_WIDTH / 2);
    int16_t centerY = y + (UI_ELEMENT_HEIGHT / 2);

    if (item->focus == UI_FOCUS_SELECTED ||
        item->focus == UI_FOCUS_GRABBED)
    {
        uint16_t focusColor = UI_GetFocusColor(item);
        int16_t r = UI_MANUAL_NODE_RADIUS + UI_FOCUS_FRAME_GAP;

        ST7735_DrawLine(centerX, centerY - r,
                        centerX + r, centerY, focusColor);
        ST7735_DrawLine(centerX + r, centerY,
                        centerX, centerY + r, focusColor);
        ST7735_DrawLine(centerX, centerY + r,
                        centerX - r, centerY, focusColor);
        ST7735_DrawLine(centerX - r, centerY,
                        centerX, centerY - r, focusColor);
    }

    int16_t r = UI_MANUAL_NODE_RADIUS;
    ST7735_DrawLine(centerX, centerY - r,
                    centerX + r, centerY, UI_COLOR_BORDER_NORMAL);
    ST7735_DrawLine(centerX + r, centerY,
                    centerX, centerY + r, UI_COLOR_BORDER_NORMAL);
    ST7735_DrawLine(centerX, centerY + r,
                    centerX - r, centerY, UI_COLOR_BORDER_NORMAL);
    ST7735_DrawLine(centerX - r, centerY,
                    centerX, centerY - r, UI_COLOR_BORDER_NORMAL);
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
     * Automatic nodes are displayed as completely
     * filled white circles.
     *
     * The radius is large enough to provide separate
     * horizontal and diagonal connection points.
     */
    UI_FillCircle(
        centerX,
        centerY,
        UI_AUTO_NODE_RADIUS,
        UI_COLOR_BORDER_NORMAL
    );
}


static void UI_DrawItem(const UI_Item *item, int16_t x, int16_t y)
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

static uint8_t UI_IsCircularItem(const UI_Item *item)
{
    if (item == NULL)
    {
        return 0;
    }

    return (item->type == UI_ITEM_INPUT ||
            item->type == UI_ITEM_OUTPUT ||
            item->type == UI_ITEM_AUTO_NODE);
}

static int16_t UI_GetCircularRadius(const UI_Item *item)
{
    if (item != NULL && item->type == UI_ITEM_AUTO_NODE)
    {
        return UI_AUTO_NODE_RADIUS;
    }

    return UI_IO_CIRCLE_RADIUS;
}

static int16_t UI_GetCircularDiagonalOffset(const UI_Item *item)
{
    if (item != NULL && item->type == UI_ITEM_AUTO_NODE)
    {
        return UI_AUTO_NODE_DIAGONAL_OFFSET;
    }

    return UI_IO_DIAGONAL_OFFSET;
}

static UI_ConnectionPoint UI_GetSourceConnectionPoint(
    const UI_ItemGeometry *geometry,
    const UI_Item *item,
    int16_t targetY)
{
    UI_ConnectionPoint point;
    int16_t centerX = geometry->x + (geometry->width / 2);
    int16_t centerY = geometry->y + (geometry->height / 2);

    if (targetY == centerY)
    {
        if (UI_IsCircularItem(item))
        {
            point.x = centerX + UI_GetCircularRadius(item);
        }
        else if (item->type == UI_ITEM_MANUAL_NODE)
        {
            point.x = centerX + UI_MANUAL_NODE_RADIUS;
        }
        else
        {
            point.x = geometry->x + geometry->width - 1;
        }
        point.y = centerY;
        return point;
    }

    if (targetY < centerY)
    {
        if (UI_IsCircularItem(item))
        {
            int16_t offset = UI_GetCircularDiagonalOffset(item);
            point.x = centerX + offset;
            point.y = centerY - offset;
        }
        else if (item->type == UI_ITEM_MANUAL_NODE)
        {
            point.x = centerX + UI_MANUAL_NODE_DIAGONAL_OFFSET;
            point.y = centerY - UI_MANUAL_NODE_DIAGONAL_OFFSET;
        }
        else
        {
            point.x = geometry->x + geometry->width - 1 -
                      UI_LOOP_DIAGONAL_INSET;
            point.y = geometry->y + UI_LOOP_DIAGONAL_INSET;
        }
        return point;
    }

    if (UI_IsCircularItem(item))
    {
        int16_t offset = UI_GetCircularDiagonalOffset(item);
        point.x = centerX + offset;
        point.y = centerY + offset;
    }
    else if (item->type == UI_ITEM_MANUAL_NODE)
    {
        point.x = centerX + UI_MANUAL_NODE_DIAGONAL_OFFSET;
        point.y = centerY + UI_MANUAL_NODE_DIAGONAL_OFFSET;
    }
    else
    {
        point.x = geometry->x + geometry->width - 1 -
                  UI_LOOP_DIAGONAL_INSET;
        point.y = geometry->y + geometry->height - 1 -
                  UI_LOOP_DIAGONAL_INSET;
    }

    return point;
}

static UI_ConnectionPoint UI_GetTargetConnectionPoint(
    const UI_ItemGeometry *geometry,
    const UI_Item *item,
    int16_t sourceY)
{
    UI_ConnectionPoint point;
    int16_t centerX = geometry->x + (geometry->width / 2);
    int16_t centerY = geometry->y + (geometry->height / 2);

    if (sourceY == centerY)
    {
        if (UI_IsCircularItem(item))
        {
            point.x = centerX - UI_GetCircularRadius(item);
        }
        else if (item->type == UI_ITEM_MANUAL_NODE)
        {
            point.x = centerX - UI_MANUAL_NODE_RADIUS;
        }
        else
        {
            point.x = geometry->x;
        }
        point.y = centerY;
        return point;
    }

    if (sourceY > centerY)
    {
        if (UI_IsCircularItem(item))
        {
            int16_t offset = UI_GetCircularDiagonalOffset(item);
            point.x = centerX - offset;
            point.y = centerY + offset;
        }
        else if (item->type == UI_ITEM_MANUAL_NODE)
        {
            point.x = centerX - UI_MANUAL_NODE_DIAGONAL_OFFSET;
            point.y = centerY + UI_MANUAL_NODE_DIAGONAL_OFFSET;
        }
        else
        {
            point.x = geometry->x + UI_LOOP_DIAGONAL_INSET;
            point.y = geometry->y + geometry->height - 1 -
                      UI_LOOP_DIAGONAL_INSET;
        }
        return point;
    }

    if (UI_IsCircularItem(item))
    {
        int16_t offset = UI_GetCircularDiagonalOffset(item);
        point.x = centerX - offset;
        point.y = centerY - offset;
    }
    else if (item->type == UI_ITEM_MANUAL_NODE)
    {
        point.x = centerX - UI_MANUAL_NODE_DIAGONAL_OFFSET;
        point.y = centerY - UI_MANUAL_NODE_DIAGONAL_OFFSET;
    }
    else
    {
        point.x = geometry->x + UI_LOOP_DIAGONAL_INSET;
        point.y = geometry->y + UI_LOOP_DIAGONAL_INSET;
    }

    return point;
}


static void UI_DrawConnectionLine(int16_t x0, int16_t y0,
                                  int16_t x1, int16_t y1,
                                  uint16_t color)
{
    int16_t dx = x1 - x0;
    int16_t dy = y1 - y0;
    int16_t absDx = (dx >= 0) ? dx : -dx;
    int16_t absDy = (dy >= 0) ? dy : -dy;
    int16_t steps = (absDx > absDy) ? absDx : absDy;

    if (steps == 0)
    {
        ST7735_DrawPixel(x0, y0, color);
        return;
    }

    int32_t currentX = ((int32_t)x0 << 8);
    int32_t currentY = ((int32_t)y0 << 8);
    int32_t stepX = ((int32_t)dx << 8) / steps;
    int32_t stepY = ((int32_t)dy << 8) / steps;
    int16_t previousX = x0;
    int16_t previousY = y0;

    ST7735_DrawPixel(previousX, previousY, color);

    for (int16_t step = 1; step <= steps; step++)
    {
        currentX += stepX;
        currentY += stepY;

        int16_t pixelX = (int16_t)((currentX + 128) >> 8);
        int16_t pixelY = (int16_t)((currentY + 128) >> 8);

        if (pixelX != previousX && pixelY != previousY)
        {
            ST7735_DrawPixel(pixelX, previousY, color);
        }

        ST7735_DrawPixel(pixelX, pixelY, color);
        previousX = pixelX;
        previousY = pixelY;
    }
}

static void UI_DrawDirectConnection(
    int16_t startX,
    int16_t startY,
    int16_t targetX,
    int16_t targetY,
    uint16_t color)
{
    int16_t deltaX = targetX - startX;
    int16_t deltaY = targetY - startY;

    if (deltaX <= 0)
    {
        return;
    }

    int16_t absX = deltaX;
    int16_t absY = (deltaY >= 0) ? deltaY : -deltaY;
    int16_t normalization = (absX > absY) ? absX : absY;

    if (normalization == 0)
    {
        return;
    }

    int32_t directionX =
        ((int32_t)deltaX * UI_ARROW_FIXED_SCALE) / normalization;
    int32_t directionY =
        ((int32_t)deltaY * UI_ARROW_FIXED_SCALE) / normalization;

    int16_t tipX =
        targetX - (int16_t)(directionX / UI_ARROW_FIXED_SCALE);
    int16_t tipY =
        targetY - (int16_t)(directionY / UI_ARROW_FIXED_SCALE);

    int16_t baseCenterX =
        tipX - (int16_t)((directionX * UI_ARROW_LENGTH) /
                         UI_ARROW_FIXED_SCALE);
    int16_t baseCenterY =
        tipY - (int16_t)((directionY * UI_ARROW_LENGTH) /
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

    UI_DrawConnectionLine(startX, startY, tipX, tipY,
                          color);

    UI_FillTriangle(tipX, tipY,
                    base1X, base1Y,
                    base2X, base2Y,
                    color);
}

static void UI_DrawConnectionByIndex(
    uint16_t connectionIndex,
    uint16_t color)
{
    if (connectionIndex >=
        UI_CONNECTION_COUNT)
    {
        return;
    }


    int16_t sourceIndex =
        UI_FindItemIndexById(
            uiConnections[
                connectionIndex
            ].sourceId
        );

    int16_t targetIndex =
        UI_FindItemIndexById(
            uiConnections[
                connectionIndex
            ].targetId
        );


    if (sourceIndex < 0 ||
        targetIndex < 0)
    {
        return;
    }


    const UI_ItemGeometry *sourceGeometry =
        &uiGeometry[sourceIndex];

    const UI_ItemGeometry *targetGeometry =
        &uiGeometry[targetIndex];

    const UI_Item *sourceItem =
        &uiItems[sourceIndex];

    const UI_Item *targetItem =
        &uiItems[targetIndex];


    if (!sourceGeometry->visible ||
        !targetGeometry->visible)
    {
        return;
    }


    int16_t sourceCenterY =
        sourceGeometry->y +
        (sourceGeometry->height / 2);

    int16_t targetCenterY =
        targetGeometry->y +
        (targetGeometry->height / 2);


    UI_ConnectionPoint sourcePoint =
        UI_GetSourceConnectionPoint(
            sourceGeometry,
            sourceItem,
            targetCenterY
        );

    UI_ConnectionPoint targetPoint =
        UI_GetTargetConnectionPoint(
            targetGeometry,
            targetItem,
            sourceCenterY
        );


    UI_DrawDirectConnection(
        sourcePoint.x,
        sourcePoint.y,
        targetPoint.x,
        targetPoint.y,
        color
    );
}

static void UI_EraseConnectionsForChangedItems(void)
{
    /*
     * Zu diesem Zeitpunkt enthält uiGeometry noch
     * die alten Bildschirmpositionen.
     *
     * Deshalb können die alten Verbindungen exakt
     * mit der Hintergrundfarbe überzeichnet werden.
     */
    for (uint16_t connectionIndex = 0;
         connectionIndex < UI_CONNECTION_COUNT;
         connectionIndex++)
    {
        int16_t sourceIndex =
            UI_FindItemIndexById(
                uiConnections[
                    connectionIndex
                ].sourceId
            );

        int16_t targetIndex =
            UI_FindItemIndexById(
                uiConnections[
                    connectionIndex
                ].targetId
            );


        if (sourceIndex < 0 ||
            targetIndex < 0)
        {
            continue;
        }


        if (UI_ItemPositionChanged(
                (uint16_t)sourceIndex) ||
            UI_ItemPositionChanged(
                (uint16_t)targetIndex))
        {
            UI_DrawConnectionByIndex(
                connectionIndex,
                UI_COLOR_BACKGROUND
            );
        }
    }
}

static void UI_DrawConnectionsForChangedItems(void)
{
    for (uint16_t connectionIndex = 0;
         connectionIndex <
             UI_CONNECTION_COUNT;
         connectionIndex++)
    {
        int16_t sourceIndex =
            UI_FindItemIndexById(
                uiConnections[
                    connectionIndex
                ].sourceId
            );

        int16_t targetIndex =
            UI_FindItemIndexById(
                uiConnections[
                    connectionIndex
                ].targetId
            );

        if (sourceIndex < 0 ||
            targetIndex < 0)
        {
            continue;
        }

        if (UI_ItemPositionChanged(
                (uint16_t)sourceIndex) ||
            UI_ItemPositionChanged(
                (uint16_t)targetIndex))
        {
            UI_DrawConnectionByIndex(
                connectionIndex,
                UI_COLOR_CONNECTION
            );
        }
    }
}

static void UI_DrawConnectionsForItem(
    int16_t itemIndex)
{
    if (itemIndex < 0 ||
        itemIndex >= (int16_t)UI_ITEM_COUNT)
    {
        return;
    }


    uint16_t itemId =
        uiItems[itemIndex].id;


    for (uint16_t i = 0;
         i < UI_CONNECTION_COUNT;
         i++)
    {
        if (uiConnections[i].sourceId ==
                itemId ||
            uiConnections[i].targetId ==
                itemId)
        {
            UI_DrawConnectionByIndex(
                i,
                UI_COLOR_CONNECTION
            );
        }
    }
}


static void UI_DrawConnections(void)
{
    for (uint16_t i = 0;
         i < UI_CONNECTION_COUNT;
         i++)
    {
        UI_DrawConnectionByIndex(
            i,
            UI_COLOR_CONNECTION
        );
    }
}

static void UI_UpdateChangedStructureDisplay(void)
{
    /*
     * uiGeometry enthält hier noch die alten
     * Bildschirmpositionen.
     *
     * Zuerst alte Verbindungen gezielt schwarz
     * überzeichnen.
     */
    UI_EraseConnectionsForChangedItems();


    /*
     * Alte Itempositionen einschließlich Rahmen
     * vollständig löschen.
     */
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (UI_ItemPositionChanged(i))
        {
            UI_ClearGeometryArea(
                &previousStepGeometry[i]
            );
        }
    }


    /*
     * Neue Bildschirmpositionen berechnen.
     */
    UI_CalculateGeometry();


    /*
     * Neue Zielbereiche säubern.
     *
     * Das ist insbesondere beim Tausch zweier Items
     * wichtig, weil dort noch die alte Darstellung
     * des jeweils anderen Items stehen kann.
     */
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (UI_ItemPositionChanged(i))
        {
            UI_ClearGeometryArea(
                &uiGeometry[i]
            );
        }
    }


    /*
     * Neue Verbindungen in Weiß zeichnen.
     */
    UI_DrawConnectionsForChangedItems();

/*
 * Unveränderte Verbindungen an möglichen
 * Kreuzungspunkten wiederherstellen.
 *
 * Das ist weiterhin deutlich schneller als ein
 * kompletter Bildschirmaufbau, weil keine großen
 * Flächen oder Texte neu gezeichnet werden.
 */
UI_DrawConnections();
    /*
     * Bewegte Items über den Verbindungen zeichnen.
     */
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (!UI_ItemPositionChanged(i) ||
            !uiGeometry[i].visible)
        {
            continue;
        }

        UI_DrawItem(
            &uiItems[i],
            uiGeometry[i].x,
            uiGeometry[i].y
        );
    }


    /*
     * Langname bleibt unter dem gegriffenen Item
     * zentriert.
     */
    UI_DrawFooter();
}

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

    ST7735_FillRect(0, UI_FOOTER_TOP,
                    UI_DISPLAY_WIDTH, UI_FOOTER_HEIGHT,
                    UI_COLOR_BACKGROUND);

    if (focusedItem == NULL)
    {
        return;
    }

    int16_t focusedIndex = UI_FindItemIndexById(focusedItem->id);

    if (focusedIndex < 0 || !uiGeometry[focusedIndex].visible)
    {
        return;
    }

    const UI_ItemGeometry *geometry = &uiGeometry[focusedIndex];
    uint16_t textWidth =
        Font5x7_GetStringWidth(focusedItem->longName, 1);
    int16_t itemCenterX = geometry->x + (geometry->width / 2);
    int16_t textX = itemCenterX - ((int16_t)textWidth / 2);

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

    Font5x7_DrawString((uint16_t)textX, textY,
                       focusedItem->longName,
                       UI_GetFocusColor(focusedItem),
                       UI_COLOR_BACKGROUND, 1);
}

void UI_Init(void)
{
    int16_t firstFocusedIndex = -1;

    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (uiItems[i].focus == UI_FOCUS_SELECTED ||
            uiItems[i].focus == UI_FOCUS_GRABBED)
        {
            if (firstFocusedIndex < 0 &&
                UI_IsSelectable(&uiItems[i]))
            {
                firstFocusedIndex =
                    (int16_t)i;
            }
            else
            {
                uiItems[i].focus =
                    UI_FOCUS_NONE;
            }
        }
    }

    /*
     * Falls kein gültiges Item fokussiert ist,
     * erstes auswählbares Item verwenden.
     */
    if (firstFocusedIndex < 0)
    {
        for (uint16_t i = 0;
             i < UI_ITEM_COUNT;
             i++)
        {
            if (UI_IsSelectable(&uiItems[i]))
            {
                uiItems[i].focus =
                    UI_FOCUS_SELECTED;

                break;
            }
        }
    }
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

        UI_DrawItem(&uiItems[i],
                    uiGeometry[i].x,
                    uiGeometry[i].y);
    }

    UI_DrawFooter();
}
