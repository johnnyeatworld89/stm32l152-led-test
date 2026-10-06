#include "font5x7.h"
#include "st7735.h"


/*
 * ============================================================
 * 5x7 FONT
 * ============================================================
 *
 * Jedes Zeichen besteht aus 5 Spalten.
 *
 * Die unteren 7 Bits eines Bytes entsprechen
 * den 7 Pixelzeilen.
 */


/*
 * Alphabet A-Z
 */
static const uint8_t Font5x7_Alphabet[26][5] =
{
    /* A */ {0x1E, 0x05, 0x05, 0x05, 0x1E},
    /* B */ {0x1F, 0x15, 0x15, 0x15, 0x0A},
    /* C */ {0x0E, 0x11, 0x11, 0x11, 0x11},
    /* D */ {0x1F, 0x11, 0x11, 0x11, 0x0E},
    /* E */ {0x1F, 0x15, 0x15, 0x11, 0x11},
    /* F */ {0x1F, 0x05, 0x05, 0x01, 0x01},
    /* G */ {0x0E, 0x11, 0x15, 0x15, 0x1D},
    /* H */ {0x1F, 0x04, 0x04, 0x04, 0x1F},
    /* I */ {0x11, 0x11, 0x1F, 0x11, 0x11},
    /* J */ {0x08, 0x10, 0x10, 0x10, 0x0F},
    /* K */ {0x1F, 0x04, 0x0A, 0x11, 0x11},
    /* L */ {0x1F, 0x10, 0x10, 0x10, 0x10},
    /* M */ {0x1F, 0x02, 0x04, 0x02, 0x1F},
    /* N */ {0x1F, 0x02, 0x04, 0x08, 0x1F},
    /* O */ {0x0E, 0x11, 0x11, 0x11, 0x0E},
    /* P */ {0x1F, 0x05, 0x05, 0x05, 0x02},
    /* Q */ {0x0E, 0x11, 0x19, 0x11, 0x1E},
    /* R */ {0x1F, 0x05, 0x0D, 0x15, 0x12},
    /* S */ {0x12, 0x15, 0x15, 0x15, 0x09},
    /* T */ {0x01, 0x01, 0x1F, 0x01, 0x01},
    /* U */ {0x0F, 0x10, 0x10, 0x10, 0x0F},
    /* V */ {0x07, 0x08, 0x10, 0x08, 0x07},
    /* W */ {0x1F, 0x08, 0x04, 0x08, 0x1F},
    /* X */ {0x11, 0x0A, 0x04, 0x0A, 0x11},
    /* Y */ {0x03, 0x04, 0x18, 0x04, 0x03},
    /* Z */ {0x19, 0x15, 0x15, 0x13, 0x11}
};


/*
 * Zahlen 0-9
 */
static const uint8_t Font5x7_Numbers[10][5] =
{
    /* 0 */ {0x0E, 0x13, 0x15, 0x19, 0x0E},
    /* 1 */ {0x00, 0x12, 0x1F, 0x10, 0x00},
    /* 2 */ {0x12, 0x19, 0x15, 0x15, 0x12},
    /* 3 */ {0x11, 0x15, 0x15, 0x15, 0x0A},
    /* 4 */ {0x07, 0x04, 0x04, 0x1F, 0x04},
    /* 5 */ {0x17, 0x15, 0x15, 0x15, 0x09},
    /* 6 */ {0x0E, 0x15, 0x15, 0x15, 0x08},
    /* 7 */ {0x01, 0x01, 0x19, 0x05, 0x03},
    /* 8 */ {0x0A, 0x15, 0x15, 0x15, 0x0A},
    /* 9 */ {0x02, 0x15, 0x15, 0x15, 0x0E}
};


/*
 * Sonderzeichen
 *
 * ?  -  _  :  /  (  )  #
 */
static const uint8_t Font5x7_Symbols[8][5] =
{
    /* ? */ {0x03, 0x01, 0x11, 0x0E, 0x04},
    /* - */ {0x08, 0x08, 0x08, 0x08, 0x08},
    /* _ */ {0x40, 0x40, 0x40, 0x40, 0x40},
    /* : */ {0x00, 0x0A, 0x00, 0x0A, 0x00},
    /* / */ {0x10, 0x08, 0x04, 0x02, 0x01},
    /* ( */ {0x0E, 0x11, 0x11, 0x00, 0x00},
    /* ) */ {0x00, 0x00, 0x11, 0x11, 0x0E},
    /* # */ {0x0A, 0x1F, 0x0A, 0x1F, 0x0A}
};


/*
 * ============================================================
 * Get character bitmap
 * ============================================================
 */
static const uint8_t *Font5x7_GetBitmap(char c)
{
    /*
     * Kleinbuchstaben werden automatisch
     * auf Großbuchstaben abgebildet.
     */
    if (c >= 'a' && c <= 'z')
    {
        c = c - 'a' + 'A';
    }

    /*
     * A-Z
     */
    if (c >= 'A' && c <= 'Z')
    {
        return Font5x7_Alphabet[c - 'A'];
    }

    /*
     * 0-9
     */
    if (c >= '0' && c <= '9')
    {
        return Font5x7_Numbers[c - '0'];
    }

    /*
     * Sonderzeichen
     */
    switch (c)
    {
        case '?': return Font5x7_Symbols[0];
        case '-': return Font5x7_Symbols[1];
        case '_': return Font5x7_Symbols[2];
        case ':': return Font5x7_Symbols[3];
        case '/': return Font5x7_Symbols[4];
        case '(': return Font5x7_Symbols[5];
        case ')': return Font5x7_Symbols[6];
        case '#': return Font5x7_Symbols[7];
        default:  return NULL;
    }
}


/*
 * ============================================================
 * Draw Character
 * ============================================================
 */
void Font5x7_DrawChar(
    uint16_t x,
    uint16_t y,
    char c,
    uint16_t fg,
    uint16_t bg,
    uint8_t scale)
{
    if (scale == 0)
    {
        scale = 1;
    }

    /*
     * Space
     */
    if (c == ' ')
    {
        ST7735_FillRect(
            x,
            y,
            6 * scale,
            7 * scale,
            bg);

        return;
    }

    const uint8_t *bitmap = Font5x7_GetBitmap(c);

    /*
     * Nicht unterstütztes Zeichen.
     */
    if (bitmap == NULL)
    {
        ST7735_FillRect(
            x,
            y,
            5 * scale,
            7 * scale,
            bg);

        ST7735_DrawRect(
            x,
            y,
            5 * scale,
            7 * scale,
            fg);

        return;
    }

    /*
     * Zeichen zeichnen.
     */
    for (uint8_t column = 0;
         column < 5;
         column++)
    {
        uint8_t columnData = bitmap[column];

        for (uint8_t row = 0;
             row < 7;
             row++)
        {
            uint16_t color;

            if (columnData & (1 << row))
            {
                color = fg;
            }
            else
            {
                color = bg;
            }

            ST7735_FillRect(
                x + column * scale,
                y + row * scale,
                scale,
                scale,
                color);
        }
    }
}


/*
 * ============================================================
 * Draw String
 * ============================================================
 */
void Font5x7_DrawString(
    uint16_t x,
    uint16_t y,
    const char *text,
    uint16_t fg,
    uint16_t bg,
    uint8_t scale)
{
    if (text == NULL)
    {
        return;
    }

    if (scale == 0)
    {
        scale = 1;
    }

    uint16_t cursorX = x;
    uint16_t cursorY = y;

    while (*text)
    {
        /*
         * Zeilenumbruch
         */
        if (*text == '\n')
        {
            cursorX = x;
            cursorY += 8 * scale;

            text++;

            continue;
        }

        Font5x7_DrawChar(
            cursorX,
            cursorY,
            *text,
            fg,
            bg,
            scale);

        /*
         * 5 Pixel Zeichenbreite
         * + 1 Pixel Abstand
         */
        cursorX += 6 * scale;

        text++;
    }
}


/*
 * ============================================================
 * Get String Width
 * ============================================================
 */
uint16_t Font5x7_GetStringWidth(
    const char *text,
    uint8_t scale)
{
    if (text == NULL)
    {
        return 0;
    }

    if (scale == 0)
    {
        scale = 1;
    }

    uint16_t width = 0;
    uint16_t currentWidth = 0;

    while (*text)
    {
        if (*text == '\n')
        {
            if (currentWidth > width)
            {
                width = currentWidth;
            }

            currentWidth = 0;
        }
        else
        {
            currentWidth += 6 * scale;
        }

        text++;
    }

    if (currentWidth > width)
    {
        width = currentWidth;
    }

    /*
     * Letzten Abstand entfernen.
     */
    if (width >= scale)
    {
        width -= scale;
    }

    return width;
}


/*
 * ============================================================
 * Get Character Height
 * ============================================================
 */
uint16_t Font5x7_GetHeight(
    uint8_t scale)
{
    if (scale == 0)
    {
        scale = 1;
    }

    return 7 * scale;
}
