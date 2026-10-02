/**
  ******************************************************************************
  * @file    main.c
  * @brief   STM32L152RE Nucleo
  *          ST7735 display, rotary encoder and MCP23S17
  ******************************************************************************
  */

#include "main.h"

#include "st7735.h"
#include "ui.h"
#include "mcp23s17.h"


/* -------------------------------------------------------------------------- */
/* Configuration                                                              */
/* -------------------------------------------------------------------------- */

#define MCP23S17_HW_ADDRESS            0U

#define MCP23S17_CS_GPIO_PORT          GPIOB
#define MCP23S17_CS_PIN                GPIO_PIN_6


/*
 * Shift button:
 *
 * MCP23S17 GPB7 -> button -> GND
 */
#define SHIFT_MCP_PORT                 MCP23S17_PORT_B
#define SHIFT_MCP_PIN                  7U


#define ENCODER_BUTTON_DEBOUNCE_MS     30U
#define SHIFT_DEBOUNCE_MS              30U


/*
 * MCP23S17 registers used for diagnostics.
 *
 * Register map with IOCON.BANK = 0.
 */
#define MCP23S17_TEST_REG_IODIRB       0x01U
#define MCP23S17_TEST_REG_GPPUB        0x0DU
#define MCP23S17_TEST_REG_GPIOB        0x13U


/* -------------------------------------------------------------------------- */
/* Global peripheral handles                                                  */
/* -------------------------------------------------------------------------- */

SPI_HandleTypeDef hspi1;


/* -------------------------------------------------------------------------- */
/* MCP23S17 instance                                                          */
/* -------------------------------------------------------------------------- */

static MCP23S17_HandleTypeDef mcp23s17_1;


/* -------------------------------------------------------------------------- */
/* Rotary encoder state                                                       */
/* -------------------------------------------------------------------------- */

static uint8_t encoderState = 0U;
static int8_t encoderAccumulator = 0;


/* -------------------------------------------------------------------------- */
/* Encoder button state                                                       */
/* -------------------------------------------------------------------------- */

static GPIO_PinState buttonRawState =
    GPIO_PIN_SET;

static GPIO_PinState buttonStableState =
    GPIO_PIN_SET;

static uint32_t buttonLastChangeTick = 0U;


/* -------------------------------------------------------------------------- */
/* Shift button state                                                         */
/* -------------------------------------------------------------------------- */

/*
 * Logical states:
 *
 * 0 = Shift not pressed
 * 1 = Shift pressed
 */
static uint8_t shiftRawState = 0U;
static uint8_t shiftStableState = 0U;

static uint32_t shiftLastChangeTick = 0U;

static uint8_t shiftInitialized = 0U;


/* -------------------------------------------------------------------------- */
/* Diagnostic variables                                                       */
/* -------------------------------------------------------------------------- */

/*
 * These variables can later also be inspected
 * through SWD or printed through UART.
 */
volatile uint8_t debugShiftPressed = 0U;

volatile uint8_t debugMcpIODIRB = 0U;
volatile uint8_t debugMcpGPPUB = 0U;
volatile uint8_t debugMcpGPIOB = 0U;

volatile uint8_t debugMcpInitOk = 0U;
volatile uint8_t debugMcpReadOk = 0U;


/* -------------------------------------------------------------------------- */
/* Private function prototypes                                                */
/* -------------------------------------------------------------------------- */

void SystemClock_Config(void);

static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);

static void Encoder_Init(void);
static void Encoder_Update(void);

static void Button_Init(void);
static void Button_Update(void);

static HAL_StatusTypeDef MCP23S17_ApplicationInit(void);

static void Shift_Update(void);
static uint8_t Shift_IsPressed(void);

static void Error_Handler(void);


/* -------------------------------------------------------------------------- */
/* Rotary encoder initialization                                              */
/* -------------------------------------------------------------------------- */

static void Encoder_Init(void)
{
    uint8_t encoderA =
        (HAL_GPIO_ReadPin(
            GPIOC,
            GPIO_PIN_1
        ) == GPIO_PIN_SET) ?
        1U :
        0U;


    uint8_t encoderB =
        (HAL_GPIO_ReadPin(
            GPIOA,
            GPIO_PIN_1
        ) == GPIO_PIN_SET) ?
        1U :
        0U;


    encoderState =
        (uint8_t)(
            (encoderA << 1) |
            encoderB
        );


    encoderAccumulator = 0;
}


/* -------------------------------------------------------------------------- */
/* Rotary encoder polling                                                     */
/* -------------------------------------------------------------------------- */

static void Encoder_Update(void)
{
    uint8_t encoderA =
        (HAL_GPIO_ReadPin(
            GPIOC,
            GPIO_PIN_1
        ) == GPIO_PIN_SET) ?
        1U :
        0U;


    uint8_t encoderB =
        (HAL_GPIO_ReadPin(
            GPIOA,
            GPIO_PIN_1
        ) == GPIO_PIN_SET) ?
        1U :
        0U;


    uint8_t currentState =
        (uint8_t)(
            (encoderA << 1) |
            encoderB
        );


    uint8_t transition =
        (uint8_t)(
            (encoderState << 2) |
            currentState
        );


    switch (transition)
    {
        /*
         * Clockwise transitions.
         */
        case 0x01U:
        case 0x07U:
        case 0x0EU:
        case 0x08U:
            encoderAccumulator++;
            break;


        /*
         * Counter-clockwise transitions.
         */
        case 0x02U:
        case 0x0BU:
        case 0x0DU:
        case 0x04U:
            encoderAccumulator--;
            break;


        default:
            break;
    }


    encoderState =
        currentState;


    /*
     * One encoder detent corresponds to four
     * valid quadrature transitions.
     */
    while (encoderAccumulator >= 4)
    {
        encoderAccumulator -= 4;

        /*
         * Shift is not yet used for vertical
         * movement. Existing horizontal behavior
         * remains active during this hardware test.
         */
        UI_HandleEncoderStep(
    1,
    Shift_IsPressed()
);
    }


    while (encoderAccumulator <= -4)
    {
        encoderAccumulator += 4;

        UI_HandleEncoderStep(
    -1,
    Shift_IsPressed()
);
    }
}


/* -------------------------------------------------------------------------- */
/* Encoder button initialization                                              */
/* -------------------------------------------------------------------------- */

static void Button_Init(void)
{
    buttonRawState =
        HAL_GPIO_ReadPin(
            GPIOA,
            GPIO_PIN_0
        );


    buttonStableState =
        buttonRawState;


    buttonLastChangeTick =
        HAL_GetTick();
}


/* -------------------------------------------------------------------------- */
/* Encoder button polling                                                     */
/* -------------------------------------------------------------------------- */

static void Button_Update(void)
{
    GPIO_PinState currentRawState =
        HAL_GPIO_ReadPin(
            GPIOA,
            GPIO_PIN_0
        );


    uint32_t currentTick =
        HAL_GetTick();


    /*
     * Raw state changed:
     * restart debounce timer.
     */
    if (currentRawState !=
        buttonRawState)
    {
        buttonRawState =
            currentRawState;

        buttonLastChangeTick =
            currentTick;
    }


    /*
     * Accept the new state after it has remained
     * stable for the debounce period.
     */
    if ((currentTick -
         buttonLastChangeTick) >=
        ENCODER_BUTTON_DEBOUNCE_MS)
    {
        if (buttonStableState !=
            buttonRawState)
        {
            buttonStableState =
                buttonRawState;


            /*
             * Encoder button is active-low.
             *
             * Only a confirmed press toggles
             * the grab state.
             */
            if (buttonStableState ==
    GPIO_PIN_RESET)
{
    UI_HandleEncoderButton(
        Shift_IsPressed()
    );
}
        }
    }
}


/* -------------------------------------------------------------------------- */
/* MCP23S17 application initialization                                        */
/* -------------------------------------------------------------------------- */

static HAL_StatusTypeDef MCP23S17_ApplicationInit(void)
{
    HAL_StatusTypeDef status;


    debugMcpInitOk = 0U;
    debugMcpReadOk = 0U;


    /*
     * Ensure that neither SPI device is selected
     * before initialization starts.
     */
    HAL_GPIO_WritePin(
        GPIOA,
        GPIO_PIN_4,
        GPIO_PIN_SET
    );


    HAL_GPIO_WritePin(
        MCP23S17_CS_GPIO_PORT,
        MCP23S17_CS_PIN,
        GPIO_PIN_SET
    );


    /*
     * Initialize MCP23S17.
     *
     * A2:A0 = 000.
     */
    status =
        MCP23S17_Init(
            &mcp23s17_1,
            &hspi1,
            MCP23S17_CS_GPIO_PORT,
            MCP23S17_CS_PIN,
            MCP23S17_HW_ADDRESS
        );


    if (status != HAL_OK)
    {
        return status;
    }


    /*
     * Configure GPB7 as input.
     */
    status =
        MCP23S17_ConfigurePinAsInput(
            &mcp23s17_1,
            SHIFT_MCP_PORT,
            SHIFT_MCP_PIN
        );


    if (status != HAL_OK)
    {
        return status;
    }


    /*
     * Do not invert GPB7.
     */
    status =
        MCP23S17_SetInputPolarity(
            &mcp23s17_1,
            SHIFT_MCP_PORT,
            SHIFT_MCP_PIN,
            0U
        );


    if (status != HAL_OK)
    {
        return status;
    }


    /*
     * Enable the MCP23S17 internal pull-up
     * for GPB7.
     *
     * Button open:
     * GPB7 = HIGH
     *
     * Button pressed:
     * GPB7 = LOW
     */
    status =
        MCP23S17_SetPullUp(
            &mcp23s17_1,
            SHIFT_MCP_PORT,
            SHIFT_MCP_PIN,
            1U
        );


    if (status != HAL_OK)
    {
        return status;
    }


    /*
     * Read back configuration registers.
     *
     * Expected:
     *
     * IODIRB = 0xFF
     * GPPUB bit 7 = 1
     */
    status =
        MCP23S17_ReadRegister(
            &mcp23s17_1,
            MCP23S17_TEST_REG_IODIRB,
            (uint8_t *)&debugMcpIODIRB
        );


    if (status != HAL_OK)
    {
        return status;
    }


    status =
        MCP23S17_ReadRegister(
            &mcp23s17_1,
            MCP23S17_TEST_REG_GPPUB,
            (uint8_t *)&debugMcpGPPUB
        );


    if (status != HAL_OK)
    {
        return status;
    }


    status =
        MCP23S17_ReadRegister(
            &mcp23s17_1,
            MCP23S17_TEST_REG_GPIOB,
            (uint8_t *)&debugMcpGPIOB
        );


    if (status != HAL_OK)
    {
        return status;
    }


    debugMcpReadOk = 1U;


    /*
     * Read the initial GPB7 pin state.
     */
    uint8_t pinState = 1U;


    status =
        MCP23S17_ReadPin(
            &mcp23s17_1,
            SHIFT_MCP_PORT,
            SHIFT_MCP_PIN,
            &pinState
        );


    if (status != HAL_OK)
    {
        return status;
    }


    /*
     * Convert active-low input into logical
     * Shift state.
     */
    shiftRawState =
        (pinState == 0U) ?
        1U :
        0U;


    shiftStableState =
        shiftRawState;


    shiftLastChangeTick =
        HAL_GetTick();


    shiftInitialized = 1U;

    debugShiftPressed =
        shiftStableState;

    debugMcpInitOk = 1U;


    return HAL_OK;
}


/* -------------------------------------------------------------------------- */
/* Shift button polling                                                       */
/* -------------------------------------------------------------------------- */

static void Shift_Update(void)
{
    if (!shiftInitialized)
    {
        return;
    }


    /*
     * Ensure that the display remains deselected
     * while the MCP23S17 is accessed.
     */
    HAL_GPIO_WritePin(
        GPIOA,
        GPIO_PIN_4,
        GPIO_PIN_SET
    );


    uint8_t gpioB = 0xFFU;


    HAL_StatusTypeDef status =
        MCP23S17_ReadPort(
            &mcp23s17_1,
            MCP23S17_PORT_B,
            &gpioB
        );


    /*
     * Keep the previous state if reading failed.
     */
    if (status != HAL_OK)
    {
        debugMcpReadOk = 0U;

        return;
    }


    debugMcpReadOk = 1U;
    debugMcpGPIOB = gpioB;


    /*
     * Extract GPB7.
     *
     * GPB7 HIGH:
     * Shift not pressed.
     *
     * GPB7 LOW:
     * Shift pressed.
     */
    uint8_t pinState =
        (gpioB &
         (uint8_t)(1U << SHIFT_MCP_PIN)) ?
        1U :
        0U;


    uint8_t currentPressedState =
        (pinState == 0U) ?
        1U :
        0U;


    uint32_t currentTick =
        HAL_GetTick();


    /*
     * Raw state changed:
     * restart the debounce timer.
     */
    if (currentPressedState !=
        shiftRawState)
    {
        shiftRawState =
            currentPressedState;

        shiftLastChangeTick =
            currentTick;
    }


    /*
     * Update the stable state after the debounce
     * period has expired.
     */
    if ((currentTick -
         shiftLastChangeTick) >=
        SHIFT_DEBOUNCE_MS)
    {
        if (shiftStableState !=
            shiftRawState)
        {
            shiftStableState =
                shiftRawState;


            /*
             * Redraw only the small Shift indicator.
             */
            UI_SetShiftDebugState(
                shiftStableState
            );
        }
    }


    debugShiftPressed =
        shiftStableState;
}


/* -------------------------------------------------------------------------- */
/* Shift state                                                                */
/* -------------------------------------------------------------------------- */

static uint8_t Shift_IsPressed(void)
{
    if (!shiftInitialized)
    {
        return 0U;
    }


    return shiftStableState;
}


/* -------------------------------------------------------------------------- */
/* Main                                                                       */
/* -------------------------------------------------------------------------- */

int main(void)
{
    HAL_Init();


    SystemClock_Config();


    MX_GPIO_Init();

    MX_SPI1_Init();


    Encoder_Init();

    Button_Init();


    /*
     * Allow the external hardware and reset
     * pull-up to stabilize.
     */
    HAL_Delay(200);


    /*
     * Initialize the display before the MCP test
     * so that the UI is available for diagnostics.
     */
    ST7735_Init();

    ST7735_SetRotation(1);


    UI_Init();

    UI_Draw();


    /*
     * Initialize the MCP23S17 after the UI has
     * been drawn.
     */
    if (MCP23S17_ApplicationInit() !=
        HAL_OK)
    {
        /*
         * Keep the UI running with Shift inactive.
         *
         * The diagnostic variables remain zero.
         */
        shiftInitialized = 0U;
        shiftRawState = 0U;
        shiftStableState = 0U;

        debugShiftPressed = 0U;
        debugMcpInitOk = 0U;

        UI_SetShiftDebugState(0U);
    }
    else
    {
        UI_SetShiftDebugState(
            Shift_IsPressed()
        );
    }


    while (1)
    {
        /*
         * Poll the MCP23S17 before evaluating
         * the encoder.
         */
        Shift_Update();


        Encoder_Update();

        Button_Update();
    }
}


/* -------------------------------------------------------------------------- */
/* GPIO initialization                                                        */
/* -------------------------------------------------------------------------- */

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct =
    {
        0
    };


    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();


    /* ---------------------------------------------------------------------- */
    /* Default output levels                                                  */
    /* ---------------------------------------------------------------------- */

    /*
     * Display CS high.
     */
    HAL_GPIO_WritePin(
        GPIOA,
        GPIO_PIN_4,
        GPIO_PIN_SET
    );


    /*
     * Display DC low.
     */
    HAL_GPIO_WritePin(
        GPIOB,
        GPIO_PIN_0,
        GPIO_PIN_RESET
    );


    /*
     * Display reset high.
     */
    HAL_GPIO_WritePin(
        GPIOB,
        GPIO_PIN_1,
        GPIO_PIN_SET
    );


    /*
     * MCP23S17 CS high.
     */
    HAL_GPIO_WritePin(
        MCP23S17_CS_GPIO_PORT,
        MCP23S17_CS_PIN,
        GPIO_PIN_SET
    );


    /* ---------------------------------------------------------------------- */
    /* PA4 = display CS                                                       */
    /* ---------------------------------------------------------------------- */

    GPIO_InitStruct.Pin =
        GPIO_PIN_4;

    GPIO_InitStruct.Mode =
        GPIO_MODE_OUTPUT_PP;

    GPIO_InitStruct.Pull =
        GPIO_NOPULL;

    GPIO_InitStruct.Speed =
        GPIO_SPEED_FREQ_HIGH;


    HAL_GPIO_Init(
        GPIOA,
        &GPIO_InitStruct
    );


    /* ---------------------------------------------------------------------- */
    /* PB0 = display DC                                                       */
    /* PB1 = display reset                                                    */
    /* PB6 = MCP23S17 CS                                                      */
    /* ---------------------------------------------------------------------- */

    GPIO_InitStruct.Pin =
        GPIO_PIN_0 |
        GPIO_PIN_1 |
        GPIO_PIN_6;

    GPIO_InitStruct.Mode =
        GPIO_MODE_OUTPUT_PP;

    GPIO_InitStruct.Pull =
        GPIO_NOPULL;

    GPIO_InitStruct.Speed =
        GPIO_SPEED_FREQ_HIGH;


    HAL_GPIO_Init(
        GPIOB,
        &GPIO_InitStruct
    );


    /* ---------------------------------------------------------------------- */
    /* PA5 = SPI1_SCK                                                         */
    /* PA6 = SPI1_MISO                                                        */
    /* PA7 = SPI1_MOSI                                                        */
    /* ---------------------------------------------------------------------- */

    GPIO_InitStruct.Pin =
        GPIO_PIN_5 |
        GPIO_PIN_6 |
        GPIO_PIN_7;

    GPIO_InitStruct.Mode =
        GPIO_MODE_AF_PP;

    GPIO_InitStruct.Pull =
        GPIO_NOPULL;

    GPIO_InitStruct.Speed =
        GPIO_SPEED_FREQ_HIGH;

    GPIO_InitStruct.Alternate =
        GPIO_AF5_SPI1;


    HAL_GPIO_Init(
        GPIOA,
        &GPIO_InitStruct
    );


    /* ---------------------------------------------------------------------- */
    /* Encoder inputs                                                         */
    /*                                                                        */
    /* PA0 = encoder switch                                                   */
    /* PA1 = encoder B                                                        */
    /* PC1 = encoder A                                                        */
    /* ---------------------------------------------------------------------- */

    GPIO_InitStruct.Pin =
        GPIO_PIN_0 |
        GPIO_PIN_1;

    GPIO_InitStruct.Mode =
        GPIO_MODE_INPUT;

    GPIO_InitStruct.Pull =
        GPIO_PULLUP;


    HAL_GPIO_Init(
        GPIOA,
        &GPIO_InitStruct
    );


    GPIO_InitStruct.Pin =
        GPIO_PIN_1;

    GPIO_InitStruct.Mode =
        GPIO_MODE_INPUT;

    GPIO_InitStruct.Pull =
        GPIO_PULLUP;


    HAL_GPIO_Init(
        GPIOC,
        &GPIO_InitStruct
    );
}


/* -------------------------------------------------------------------------- */
/* SPI1 initialization                                                        */
/* -------------------------------------------------------------------------- */

static void MX_SPI1_Init(void)
{
    __HAL_RCC_SPI1_CLK_ENABLE();


    hspi1.Instance =
        SPI1;


    hspi1.Init.Mode =
        SPI_MODE_MASTER;


    /*
     * Full duplex is required because the
     * MCP23S17 uses a separate MISO line.
     */
    hspi1.Init.Direction =
        SPI_DIRECTION_2LINES;


    hspi1.Init.DataSize =
        SPI_DATASIZE_8BIT;


    hspi1.Init.CLKPolarity =
        SPI_POLARITY_LOW;


    hspi1.Init.CLKPhase =
        SPI_PHASE_1EDGE;


    hspi1.Init.NSS =
        SPI_NSS_SOFT;


    /*
     * System clock = 32 MHz
     * Prescaler     = 16
     * SPI clock     = approximately 2 MHz
     */
    hspi1.Init.BaudRatePrescaler =
        SPI_BAUDRATEPRESCALER_16;


    hspi1.Init.FirstBit =
        SPI_FIRSTBIT_MSB;


    hspi1.Init.TIMode =
        SPI_TIMODE_DISABLED;


    hspi1.Init.CRCCalculation =
        SPI_CRCCALCULATION_DISABLED;


    hspi1.Init.CRCPolynomial =
        7;


    if (HAL_SPI_Init(
            &hspi1) !=
        HAL_OK)
    {
        Error_Handler();
    }
}


/* -------------------------------------------------------------------------- */
/* System clock configuration                                                 */
/* -------------------------------------------------------------------------- */

void SystemClock_Config(void)
{
    RCC_ClkInitTypeDef RCC_ClkInitStruct =
    {
        0
    };


    RCC_OscInitTypeDef RCC_OscInitStruct =
    {
        0
    };


    RCC_OscInitStruct.OscillatorType =
        RCC_OSCILLATORTYPE_HSI;


    RCC_OscInitStruct.HSIState =
        RCC_HSI_ON;


    RCC_OscInitStruct.HSICalibrationValue =
        RCC_HSICALIBRATION_DEFAULT;


    RCC_OscInitStruct.PLL.PLLState =
        RCC_PLL_ON;


    RCC_OscInitStruct.PLL.PLLSource =
        RCC_PLLSOURCE_HSI;


    RCC_OscInitStruct.PLL.PLLMUL =
        RCC_PLL_MUL6;


    RCC_OscInitStruct.PLL.PLLDIV =
        RCC_PLL_DIV3;


    if (HAL_RCC_OscConfig(
            &RCC_OscInitStruct) !=
        HAL_OK)
    {
        Error_Handler();
    }


    __HAL_RCC_PWR_CLK_ENABLE();


    __HAL_PWR_VOLTAGESCALING_CONFIG(
        PWR_REGULATOR_VOLTAGE_SCALE1
    );


    while (__HAL_PWR_GET_FLAG(
               PWR_FLAG_VOS) !=
           RESET)
    {
    }


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
            FLASH_LATENCY_1) !=
        HAL_OK)
    {
        Error_Handler();
    }
}


/* -------------------------------------------------------------------------- */
/* Error handler                                                              */
/* -------------------------------------------------------------------------- */

static void Error_Handler(void)
{
    /*
     * Deselect both SPI devices.
     */
    HAL_GPIO_WritePin(
        GPIOA,
        GPIO_PIN_4,
        GPIO_PIN_SET
    );


    HAL_GPIO_WritePin(
        MCP23S17_CS_GPIO_PORT,
        MCP23S17_CS_PIN,
        GPIO_PIN_SET
    );


    while (1)
    {
    }
}


/* -------------------------------------------------------------------------- */
/* Assert failed                                                              */
/* -------------------------------------------------------------------------- */

#ifdef USE_FULL_ASSERT

void assert_failed(
    uint8_t *file,
    uint32_t line)
{
    (void)file;
    (void)line;


    while (1)
    {
    }
}

#endif
