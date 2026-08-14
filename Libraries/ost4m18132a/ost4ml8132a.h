#ifndef OST4ML8132A_H
#define OST4ML8132A_H

#include "stm32l1xx_hal.h"

#define LED_COUNT 3

typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} RGB_t;

void LED_Init(void);

void LED_SetColor(uint8_t index,
                  uint8_t r,
                  uint8_t g,
                  uint8_t b);

void LED_Show(void);

#endif
