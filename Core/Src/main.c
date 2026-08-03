/**
  ******************************************************************************
  * @file    main.c
  * @brief   STM32L152RE
  *          - USER Button (PC13) schaltet interne LED (PA5)
  *          - Externer Taster (PB1) dient als Tap-Tempo
  *          - Externe LED (PB0) blinkt mit der getappten Frequenz
  ******************************************************************************
  */

#include "main.h"

GPIO_InitTypeDef GPIO_InitStruct = {0};

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

    /* ===========================================================
       Interne LED (PA5)
       =========================================================== */

    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* ===========================================================
       Externe LED (PB0)
       =========================================================== */

    GPIO_InitStruct.Pin = GPIO_PIN_0;

    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);

    /* ===========================================================
       Interner USER Button (PC13)
       =========================================================== */

    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;

    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* ===========================================================
       Externer Taster (PB1)
       gegen GND
       =========================================================== */

    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;

    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* ===========================================================
       Variablen
       =========================================================== */

    uint8_t intButtonOld = GPIO_PIN_SET;
    uint8_t extButtonOld = GPIO_PIN_SET;

    uint8_t internalLedState = 0;
    uint8_t externalLedState = 0;

    /* ---------- Tap Tempo ---------- */

    uint32_t lastTap = 0;
    uint32_t intervalSum = 0;
    uint32_t intervalCount = 0;

    uint32_t blinkPeriod = 0;
    uint32_t lastBlink = 0;

    while (1)
    {
        uint32_t now = HAL_GetTick();

        /* =======================================================
           Interner Button -> interne LED umschalten
           ======================================================= */

        uint8_t intButton = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13);

        if ((intButtonOld == GPIO_PIN_SET) &&
            (intButton == GPIO_PIN_RESET))
        {
            internalLedState ^= 1;

            HAL_GPIO_WritePin(GPIOA,
                              GPIO_PIN_5,
                              internalLedState ? GPIO_PIN_SET : GPIO_PIN_RESET);
        }

        intButtonOld = intButton;


        /* =======================================================
           Externer Taster -> Tap Tempo
           ======================================================= */

        uint8_t extButton = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1);

        if ((extButtonOld == GPIO_PIN_SET) &&
            (extButton == GPIO_PIN_RESET))
        {
            /* Erste Betätigung oder Timeout (>2 s) */
            if ((lastTap == 0) || ((now - lastTap) > 2000))
            {
                lastTap = now;
                intervalSum = 0;
                intervalCount = 0;
            }
            else
            {
                uint32_t interval = now - lastTap;

                lastTap = now;

                intervalSum += interval;
                intervalCount++;

                blinkPeriod = intervalSum / intervalCount;
            }
        }

        extButtonOld = extButton;


        /* =======================================================
           Externe LED blinken
           ======================================================= */

        if (blinkPeriod > 0)
        {
            if ((now - lastBlink) >= (blinkPeriod / 2))
            {
                lastBlink = now;

                externalLedState ^= 1;

                HAL_GPIO_WritePin(GPIOB,
                                  GPIO_PIN_0,
                                  externalLedState ? GPIO_PIN_SET : GPIO_PIN_RESET);
            }
        }

        /* einfache Entprellung */
        HAL_Delay(10);
    }
}

/**
  * @brief System Clock Configuration
  *        (unverändert aus deinem bisherigen Projekt übernehmen)
  */
void SystemClock_Config(void)
{
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;

    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL6;
    RCC_OscInitStruct.PLL.PLLDIV = RCC_PLL_DIV3;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    __HAL_RCC_PWR_CLK_ENABLE();

    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    while (__HAL_PWR_GET_FLAG(PWR_FLAG_VOS) != RESET)
    {
    }

    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
    {
        Error_Handler();
    }
}

static void Error_Handler(void)
{
    while (1)
    {
    }
}

#ifdef USE_FULL_ASSERT

void assert_failed(uint8_t *file, uint32_t line)
{
    while (1)
    {
    }
}

#endif
