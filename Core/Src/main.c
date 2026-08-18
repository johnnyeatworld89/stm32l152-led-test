/**
  ******************************************************************************
  * @file    main.c
  * @brief   STM32L152RE Nucleo - ST7735S SPI Display Test
  ******************************************************************************
  */

#include "main.h"
#include "st7735.h"
#include "ost4ml8132a.h"
#include "ui.h"


/* -------------------------------------------------------------------------- */
/* Global variables                                                           */
/* -------------------------------------------------------------------------- */

/* Global SPI handle used by st7735.c */
SPI_HandleTypeDef hspi1;


/* -------------------------------------------------------------------------- */
/* Private function prototypes                                                */
/* -------------------------------------------------------------------------- */

void SystemClock_Config(void);

static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void Error_Handler(void);


/* -------------------------------------------------------------------------- */
/* Main                                                                       */
/* -------------------------------------------------------------------------- */

int main(void)
{
    /* ---------------------------------------------------------------------- */
    /* HAL initialization                                                     */
    /* ---------------------------------------------------------------------- */

    HAL_Init();


    /* ---------------------------------------------------------------------- */
    /* System clock                                                           */
    /* ---------------------------------------------------------------------- */

    SystemClock_Config();


    /* ---------------------------------------------------------------------- */
    /* Hardware initialization                                                */
    /* ---------------------------------------------------------------------- */

    MX_GPIO_Init();
    MX_SPI1_Init();


    /* Give the hardware a short moment to stabilize */
    HAL_Delay(200);


    /* ---------------------------------------------------------------------- */
    /* Display initialization                                                 */
    /* ---------------------------------------------------------------------- */

    ST7735_Init();


    /*
     * Display orientation
     *
     * 0 = current orientation
     *
     * The rotation is deliberately configured here in main.c.
     * This means the ST7735 driver itself does not impose an orientation.
     *
     * Later we can simply change this value to:
     *
     *     ST7735_SetRotation(0);
     *     ST7735_SetRotation(1);
     *     ST7735_SetRotation(2);
     *     ST7735_SetRotation(3);
     *
     * without changing the UI code.
     */
    ST7735_SetRotation(0);


    /* ---------------------------------------------------------------------- */
    /* Other hardware initialization                                          */
    /* ---------------------------------------------------------------------- */

    LED_Init();


    /* ---------------------------------------------------------------------- */
    /* User interface initialization                                          */
    /* ---------------------------------------------------------------------- */

    UI_Init();

    UI_Draw();


    /* ---------------------------------------------------------------------- */
    /* Main loop                                                              */
    /* ---------------------------------------------------------------------- */

    while (1)
    {
        /*
         * Main application code will be added here later.
         *
         * For now the UI remains static.
         */
    }
}


/* -------------------------------------------------------------------------- */
/* GPIO Initialization                                                        */
/* -------------------------------------------------------------------------- */

/**
  * @brief GPIO Initialization Function
  *
  * Display wiring:
  *
  * PA4 = CS
  * PA5 = SPI1_SCK
  * PA7 = SPI1_MOSI
  *
  * PB0 = DC / A0
  * PB1 = RST
  */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};


    /* Enable GPIO clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();


    /* ---------------------------------------------------------------------- */
    /* Default output levels                                                  */
    /* ---------------------------------------------------------------------- */

    /* CS high = display not selected */
    HAL_GPIO_WritePin(
        GPIOA,
        GPIO_PIN_4,
        GPIO_PIN_SET
    );

    /* DC low */
    HAL_GPIO_WritePin(
        GPIOB,
        GPIO_PIN_0,
        GPIO_PIN_RESET
    );

    /* RST high = display not in reset */
    HAL_GPIO_WritePin(
        GPIOB,
        GPIO_PIN_1,
        GPIO_PIN_SET
    );


    /* ---------------------------------------------------------------------- */
    /* PA4 = CS                                                               */
    /* ---------------------------------------------------------------------- */

    GPIO_InitStruct.Pin = GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

    HAL_GPIO_Init(
        GPIOA,
        &GPIO_InitStruct
    );


    /* ---------------------------------------------------------------------- */
    /* PB0 = DC, PB1 = RST                                                    */
    /* ---------------------------------------------------------------------- */

    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

    HAL_GPIO_Init(
        GPIOB,
        &GPIO_InitStruct
    );


    /* ---------------------------------------------------------------------- */
    /* PA5 = SPI1_SCK, PA7 = SPI1_MOSI                                        */
    /* ---------------------------------------------------------------------- */

    GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;

    HAL_GPIO_Init(
        GPIOA,
        &GPIO_InitStruct
    );
}


/* -------------------------------------------------------------------------- */
/* SPI1 Initialization                                                        */
/* -------------------------------------------------------------------------- */

/**
  * @brief SPI1 Initialization Function
  *
  * ST7735S uses write-only SPI here:
  *
  * SCK  = PA5
  * MOSI = PA7
  * MISO = unused
  */
static void MX_SPI1_Init(void)
{
    /* Enable SPI1 clock */
    __HAL_RCC_SPI1_CLK_ENABLE();


    /* SPI1 configuration */
    hspi1.Instance = SPI1;

    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_1LINE;

    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;

    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;

    hspi1.Init.NSS = SPI_NSS_SOFT;


    /*
     * Conservative SPI speed.
     *
     * System clock = 32 MHz
     * Prescaler     = 16
     * SPI clock     ≈ 2 MHz
     *
     * We can increase this later once the display code is stable.
     */
    hspi1.Init.BaudRatePrescaler =
        SPI_BAUDRATEPRESCALER_16;


    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;

    hspi1.Init.TIMode = SPI_TIMODE_DISABLED;

    hspi1.Init.CRCCalculation =
        SPI_CRCCALCULATION_DISABLED;

    hspi1.Init.CRCPolynomial = 7;


    /* Initialize SPI */
    if (HAL_SPI_Init(&hspi1) != HAL_OK)
    {
        Error_Handler();
    }
}


/* -------------------------------------------------------------------------- */
/* System Clock Configuration                                                 */
/* -------------------------------------------------------------------------- */

/**
  * @brief System Clock Configuration
  */
void SystemClock_Config(void)
{
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};


    /* ---------------------------------------------------------------------- */
    /* HSI configuration                                                      */
    /* ---------------------------------------------------------------------- */

    RCC_OscInitStruct.OscillatorType =
        RCC_OSCILLATORTYPE_HSI;

    RCC_OscInitStruct.HSIState =
        RCC_HSI_ON;

    RCC_OscInitStruct.HSICalibrationValue =
        RCC_HSICALIBRATION_DEFAULT;


    /* ---------------------------------------------------------------------- */
    /* PLL configuration                                                      */
    /* ---------------------------------------------------------------------- */

    RCC_OscInitStruct.PLL.PLLState =
        RCC_PLL_ON;

    RCC_OscInitStruct.PLL.PLLSource =
        RCC_PLLSOURCE_HSI;

    RCC_OscInitStruct.PLL.PLLMUL =
        RCC_PLL_MUL6;

    RCC_OscInitStruct.PLL.PLLDIV =
        RCC_PLL_DIV3;


    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }


    /* ---------------------------------------------------------------------- */
    /* Power configuration                                                    */
    /* ---------------------------------------------------------------------- */

    __HAL_RCC_PWR_CLK_ENABLE();

    __HAL_PWR_VOLTAGESCALING_CONFIG(
        PWR_REGULATOR_VOLTAGE_SCALE1
    );


    while (__HAL_PWR_GET_FLAG(PWR_FLAG_VOS) != RESET)
    {
    }


    /* ---------------------------------------------------------------------- */
    /* Clock configuration                                                    */
    /* ---------------------------------------------------------------------- */

    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource =
        RCC_SYSCLKSOURCE_PLLCLK;

    RCC_ClkInitStruct.AHBCLKDivider =
        RCC_SYSCLK_DIV1;

    RCC_ClkInitStruct.APB1CLKDivider =
        RCC_HCLK_DIV1;

    RCC_ClkInitStruct.APB2CLKDivider =
        RCC_HCLK_DIV1;


    if (HAL_RCC_ClockConfig(
            &RCC_ClkInitStruct,
            FLASH_LATENCY_1
        ) != HAL_OK)
    {
        Error_Handler();
    }
}


/* -------------------------------------------------------------------------- */
/* Error Handler                                                              */
/* -------------------------------------------------------------------------- */

static void Error_Handler(void)
{
    while (1)
    {
    }
}


/* -------------------------------------------------------------------------- */
/* Assert failed                                                              */
/* -------------------------------------------------------------------------- */

#ifdef USE_FULL_ASSERT

void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;

    while (1)
    {
    }
}

#endif
