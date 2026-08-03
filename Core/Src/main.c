#include "main.h"

/* Private variables ---------------------------------------------------------*/
GPIO_InitTypeDef GPIO_InitStruct = {0};

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void Error_Handler(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    /* GPIO Clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* ---------- Interne LED PA5 ---------- */
    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* ---------- Externe LED PB0 ---------- */
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* LEDs aus */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);

    /* ---------- Interner Button PC13 ---------- */
    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* ---------- Externer Schalter PB1 ---------- */
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    uint8_t intButtonOld = GPIO_PIN_SET;
    uint8_t extButtonOld = GPIO_PIN_SET;

    uint8_t intLedState = 0;
    uint8_t extLedState = 0;

    while (1)
    {
        /* ===== Interner Button =====
           PC13: gedrückt = LOW
        */
        uint8_t intButton = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13);

        if ((intButtonOld == GPIO_PIN_SET) &&
            (intButton == GPIO_PIN_RESET))
        {
            intLedState ^= 1;

            HAL_GPIO_WritePin(GPIOA,
                              GPIO_PIN_5,
                              intLedState ? GPIO_PIN_SET : GPIO_PIN_RESET);
        }

        intButtonOld = intButton;

        /* ===== Externer Schalter =====
           PB1: gedrückt = LOW
        */
        uint8_t extButton = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1);

        if ((extButtonOld == GPIO_PIN_SET) &&
            (extButton == GPIO_PIN_RESET))
        {
            extLedState ^= 1;

            HAL_GPIO_WritePin(GPIOB,
                              GPIO_PIN_0,
                              extLedState ? GPIO_PIN_SET : GPIO_PIN_RESET);
        }

        extButtonOld = extButton;

        /* Einfache Entprellung */
        HAL_Delay(20);
    }
}

/**
  * @brief System Clock Configuration
  * (Deine bisherige Funktion unverändert übernehmen)
  */
void SystemClock_Config(void)
{
    /* Deine vorhandene SystemClock_Config() hier unverändert einfügen */
}

static void Error_Handler(void)
{
    while (1)
    {
    }
}
