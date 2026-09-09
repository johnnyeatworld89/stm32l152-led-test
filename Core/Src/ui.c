#include "ui.h"

#include "st7735.h"
#include "font5x7.h"


/* -------------------------------------------------------------------------- */
/* UI colors                                                                  */
/* -------------------------------------------------------------------------- */

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


/* -------------------------------------------------------------------------- */
/* Layout constants                                                           */
/* -------------------------------------------------------------------------- */

/*
 * Logical dimensions after ST7735_SetRotation(1).
 */
#define UI_DISPLAY_WIDTH            160
#define UI_DISPLAY_HEIGHT           128

#define UI_MARGIN_LEFT              4
#define UI_MARGIN_RIGHT             4

#define UI_NORMAL_ITEM_WIDTH        24
#define UI_SELECTED_ITEM_WIDTH      48
#define UI_IO_ITEM_WIDTH            20

#define UI_ITEM_HEIGHT              24
#define UI_COLUMN_SPACING           4

#define UI_IO_CIRCLE_RADIUS         4
#define UI_MANUAL_NODE_RADIUS       6
#define UI_AUTO_NODE_RADIUS         3


/* -------------------------------------------------------------------------- */
/* Temporary test items                                                       */
/* -------------------------------------------------------------------------- */

/*
 * These items are temporary display test data.
 *
 * Later this array will be supplied by the preset and routing model.
 */
static UI_Item uiItems[] =
{
   *{
        .id = 1,
        .type =*UI_ITEM_INPUT,
        .order = 0,*        .lane = 0,
        .loopSt*tus = UI_LOOP_STATUS_OFF,
        *focus = UI_FOCUS_NONE,
        .sh*rtName = "In1",
        .longName * "Input 1"
    },

    {
        .*d = 2,
        .type = UI_ITEM_LOO*,
        .order = 1,
        .lan* = 0,
        .loopStatus = UI_LOO*_STATUS_OFF,
        .focus = UI_F*CUS_NONE,
        .shortName = "L0*",
        .longName = "Loop 01"
 *  },

    {
        .id = 3,
     *  .type = UI_ITEM_LOOP,
        .o*der = 2,
        .lane = 0,
      * .loopStatus = UI_LOOP_STATUS_ACTI*E_CONFIRMED,
        .focus = UI_F*CUS_SELECTED,
        .shortName =*"L02",
        .longName = "Loop 0*"
    },

    {
        .id = 4,
 *      .type = UI_ITEM_LOOP,
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


#define UI_ITEM_COUNT \
    (sizeof(uiItems) / sizeof(uiItems[0]))


/* -------------------------------------------------------------------------- */
/* Basic circle drawing                                                       */
/* -------------------------------------------------------------------------- */

static void UI_DrawCircle(
    *nt16_t centerX,
    int16_t center*,
    int16_t radius,
    uint16_t*color)
{
    int16_t x = radius;
 *  int16_t y = 0;
    int16_t error*= 1 - radius;


    while (x >= y)*    {
        ST7735_DrawPixel(
  *         centerX + x,
            *enterY + y,
            color);

 *      ST7735_DrawPixel(
          * centerX + y,
            centerY * x,
            color);

        S*7735_DrawPixel(
            center* - y,
            centerY + x,
   *        color);

        ST7735_Dr*wPixel(
            centerX - x,
 *          centerY + y,
           *color);

        ST7735_DrawPixel(*            centerX - x,
         *  centerY - y,
            color);*
        ST7735_DrawPixel(
            centerX - y,
            centerY - x,
            color);

        ST7735_DrawPixel(
            centerX + y,
            centerY - x,
            color);

        ST7735_DrawPixel(
            centerX + x,
            centerY - y,
            color);


        y++;


        if (error < 0)
        {
            error += (2 * y) + 1;
        }
        else
        {
            x--;

            error +=
                (2 * (y - x)) + 1;
        }
    }
}


/* -------------------------------------------------------------------------- */
/* Filled circle drawing                                                      */
/* -------------------------------------------------------------------------- */

static void UI_FillCircle(
    int16_t centerX,
    int16_t centerY,
    int16_t radius,
    uint16_t color)
{
    for (int16_t y = -radius;
         y <= radius;
         y++)
    {
        for (int16_t x = -radius;
             x <= radius;
             x++)
        {
            if ((x * x) + (y * y) <=
                (radius * radius))
            {
                ST7735_DrawPixel(
                    centerX + x,
                    centerY + y,
                    color);
            }
        }
    }
}


/* -------------------------------------------------------------------------- */
/* Item properties                                                            */
/* -------------------------------------------------------------------------- */

static uint16_t UI_GetLoopFillC*lor(
    const UI_Item *item)
{
  * if (item == NULL)
    {
        r*turn UI_COLOR_BACKGROUND;
    }


*   switch (item->loopStatus)
    {*        case UI_LOOP_STATUS_ACTIVE*CONFIRMED:
            return UI_C*LOR_LOOP_CONFIRMED;

        case *I_LOOP_STATUS_ACTIVE_UNCONFIRMED:
*           return UI_COLOR_LOOP_UN*ONFIRMED;

        case UI_LOOP_ST*TUS_OFF:
        default:
        *   return UI_COLOR_LOOP_OFF;
    }*}


static uint16_t UI_GetBorderCo*or(
    const UI_Item *item)
{
   *if (item == NULL)
    {
        re*urn UI_COLOR_NORMAL_BORDER;
    }
*
    switch (item->focus)
    {
  *     case UI_FOCUS_SELECTED:
     *      return UI_COLOR_SELECTED_BOR*ER;

        case UI_FOCUS_GRABBED*
            return UI_COLOR_GRABB*D_BORDER;

        case UI_FOCUS_N*NE:
        default:
            r*turn UI_COLOR_NORMAL_BORDER;
    }*}


static uint16_t UI_GetTextColo*(
    const UI_Item *item)
{
    i* (item == NULL)
    {
        retu*n UI_COLOR_TEXT_LIGHT;
    }


   *if (item->type == UI_ITEM_LOOP &&
*       item->loopStatus ==
       *    UI_LOOP_STATUS_ACTIVE_UNCONFIR*ED)
    {
        return UI_COLOR_TEXT_DARK;
    }


    return UI_COLOR_TEXT_LIGHT;
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


static uint16_t UI_GetItemWidth(
    const UI_Item *item)
{
    if (item == NULL)
    {
        return UI_NORMAL_ITEM_WIDTH;
    }


    /*
     * Automatic nodes only need a small drawing area.
     */
    if (item->type == UI_ITEM_AUTO_NODE)
    {
        return
            (UI_AUTO_NODE_RADIUS * 2) + 2;
    }


    /*
     * Manual nodes use a small diamond.
     */
    if (item->type == UI_ITEM_MANUAL_NODE)
    {
        return
            (UI_MANUAL_NODE_RADIUS * 2) + 2;
    }


    /*
     * Inputs and outputs use a compact circle plus text.
     */
    if (item->type == UI_ITEM_INPUT ||
        item->type == UI_ITEM_OUTPUT)
    {
        if (item->focus == UI_FOCUS_SELECTED ||
            item->focus == UI_FOCUS_GRABBED)
        {
            return UI_SELECTED_ITEM_WIDTH;
        }


        return UI_IO_ITEM_WIDTH;
    }


    /*
     * Loops use either normal or selected width.
     */
    if (item->focus == UI_FOCUS_SELECTED ||
        item->focus == UI_FOCUS_GRABBED)
    {
        return UI_SELECTED_ITEM_WIDTH;
    }


    return UI_NORMAL_ITEM_WIDTH;
}


/* -------------------------------------------------------------------------- */
/* Text helper                                                                */
/* -------------------------------------------------------------------------- */

static void UI_DrawCenteredText*
    uint16_t x,
    uint16_t y,
 *  uint16_t width,
    uint16_t hei*ht,
    const char *text,
    uint*6_t foreground,
    uint16_t backg*ound,
    uint8_t scale)
{
    if *text == NULL)
    {
        return*
    }


    uint16_t textWidth =
*       Font5x7_GetStringWidth(
   *        text,
            scale);

    uint16_t textHeight =
        Font5x7_GetHeight(
            scale);


    uint16_t textX = x;
    uint16_t textY = y;


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
        text,
        foreground,
        background,
        scale);
}


/* -------------------------------------------------------------------------- */
/* Draw loop                                                                  */
/* -------------------------------------------------------------------------- */

static void UI_DrawLoop(
    co*st UI_Item *item,
    uint16_t x,
*   uint16_t y)
{
    uint16_t width =
        UI_GetItemWidth(item);

    uint16_t fillColor =
        UI_GetLoopFillColor(item);

    uint16_t borderColor =
        UI_GetBorderColor(item);

    uint16_t textColor =
        UI_GetTextColor(item);

    const char *name =
        UI_GetDisplayedName(item);


    ST7735_FillRect(
        x,
        y,
        width,
        UI_ITEM_HEIGHT,
        fillColor);


    ST7735_DrawRect(
        x,
        y,
        width,
        UI_ITEM_HEIGHT,
        borderColor);


    /*
     * Selected and grabbed loops receive a double border.
     */
    if ((item->focus == UI_FOCUS_SELECTED ||
         item->focus == UI_FOCUS_GRABBED) &&
        width > 4 &&
        UI_ITEM_HEIGHT > 4)
    {
        ST7735_DrawRect(
            x + 1,
            y + 1,
            width - 2,
            UI_ITEM_HEIGHT - 2,
            borderColor);
    }


    UI_DrawCenteredText(
        x,
        y,
        width,
        UI_ITEM_HEIGHT,
        name,
        textColor,
        fillColor,
        1);
}


/* -------------------------------------------------------------------------- */
/* Draw input or output                                                       */
/* -------------------------------------------------------------------------- */

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
     * Circle is placed above the label.
     */
    int16_t centerX =
        x + (width / 2);

    int16_t centerY =
        y + UI_IO_CIRCLE_RADIUS + 1;


    UI_DrawCircle(
        centerX,
        centerY,
        UI_IO_CIRCLE_RADIUS,
        borderColor);


    /*
     * A small center pixel improves the visibility
     * of the connection point.
     */
    ST7735_DrawPixel(
        centerX,
        centerY,
        borderColor);


    /*
     * Text below circle.
     */
    uint16_t textY =
        y +
        (UI_IO_CIRCLE_RADIUS * 2) +
        4;


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


/* -------------------------------------------------------------------------- */
/* Draw manual node                                                           */
/* -------------------------------------------------------------------------- */

static void UI_DrawManualNode(
*   const UI_Item *item,
    uint16*t x,
    uint16_t y)
{
    uint16_* width =
        UI_GetItemWidth(i*em);

    uint16_t color =
       *UI_GetBorderColor(item);

    int1*_t centerX =
        x + (width / *);

    int16_t centerY =
        * + (UI_ITEM_HEIGHT / 2);


    ST7*35_DrawLine(
        centerX,
    *   centerY - UI_MANUAL_NODE_RADIUS*
        centerX + UI_MANUAL_NODE_*ADIUS,
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


/* -------------------------------------------------------------------------- */
/* Draw automatic node                                                        */
/* -------------------------------------------------------------------------- */

static void UI_DrawAutoNode(
    uint16_t x,
    uint16_t y)
{
    uint16_t width =
        (UI_AUTO_NODE_RADIUS * 2) + 2;

    int16_t centerX =
        x + (width / 2);

    int16_t centerY =
        y + (UI_ITEM_HEIGHT / 2);


    UI_FillCircle(
        centerX,
        centerY,
        UI_AUTO_NODE_RADIUS,
        UI_COLOR_AUTO_NODE);
}


/* -------------------------------------------------------------------------- */
/* Draw generic item                                                          */
/* -------------------------------------------------------------------------- */

static void UI_DrawItem(
    co*st UI_Item *item,
    uint16_t x,
*   uint16_t y)
{
    if (item == N*LL)
    {
        return;
    }


*   switch (item->type)
    {
     *  case UI_ITEM_LOOP:
            U*_DrawLoop(
                item,
 *              x,
                y);
            break;


        case UI_ITEM_INPUT:
        case UI_ITEM_OUTPUT:
            UI_DrawIO(
                item,
                x,
                y);
            break;


        case UI_ITEM_MANUAL_NODE:
            UI_DrawManualNode(
                item,
                x,
                y);
            break;


        case UI_ITEM_AUTO_NODE:
            UI_DrawAutoNode(
                x,
                y);
            break;


        default:
            break;
    }
}


/* -------------------------------------------------------------------------- */
/* Initialization                                                             */
/* -------------------------------------------------------------------------- */

void UI_Init(void)
{
    /*
     * Dynamic selection and scrolling state
     * will be initialized here later.
     */
}


/* -------------------------------------------------------------------------- */
/* Draw test UI                                                               */
/* -------------------------------------------------------------------------- */

void UI_Draw(void)
{
    ST7735_FillScreen(
        UI_COLOR_BACKGROUND);


    /*
     * All current test items use lane 0.
     *
     * Later the Y position will be calculated
     * from the lane value.
     */
    uint16_t y =
        (UI_DISPLAY_HEIGHT -
         UI_ITEM_HEIGHT) / 2;


    uint16_t x =
        UI_MARGIN_LEFT;


    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        uint16_t itemWidth =
            UI_GetItemWidth(
                &uiItems[i]);


        /*
         * Stop before drawing outside the screen.
         *
         * Proper horizontal scrolling will follow later.
         */
        if ((x + itemWidth) >
            (UI_DISPLAY_WIDTH -
             UI_MARGIN_RIGHT))
        {
            break;
        }


        UI_DrawItem(
            &uiItems[i],
            x,
            y);


        x +=
            itemWidth +
            UI_COLUMN_SPACING;
    }
}
