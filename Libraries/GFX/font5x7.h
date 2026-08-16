#ifndef FONT5X7_H
#define FONT5X7_H

#include "stm32l1xx_hal.h"

/*
 * 5x7 Bitmap Font
 *
 * Jede Zeichenposition ist:
 *
 * 5 Pixel breit
 * 7 Pixel hoch
 *
 * Zusätzlich kommt 1 Pixel Abstand
 * zum nächsten Zeichen.
 */


/*
 * Zeichnet ein einzelnes Zeichen.
 *
 * x, y     = obere linke Ecke
 * c        = Zeichen
 * fg       = Vordergrundfarbe
 * bg       = Hintergrundfarbe
 * scale    = Vergrößerungsfaktor
 *
 * scale = 1 -> 5x7 Pixel
 * scale = 2 -> 10x14 Pixel
 * scale = 3 -> 15x21 Pixel
 */
void Font5x7_DrawChar(
    uint16_t x,
    uint16_t y,
    char c,
    uint16_t fg,
    uint16_t bg,
    uint8_t scale);


/*
 * Zeichnet einen String.
 *
 * Die Zeichen werden automatisch
 * horizontal hintereinander gesetzt.
 */
void Font5x7_DrawString(
    uint16_t x,
    uint16_t y,
    const char *text,
    uint16_t fg,
    uint16_t bg,
    uint8_t scale);


/*
 * Gibt die Breite eines Strings zurück.
 */
uint16_t Font5x7_GetStringWidth(
    const char *text,
    uint8_t scale);


/*
 * Höhe eines Zeichens.
 */
uint16_t Font5x7_GetHeight(
    uint8_t scale);

#endif
