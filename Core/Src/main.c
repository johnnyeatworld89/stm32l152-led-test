/**
  ******************************************************************************
  * @file    GPIO/GPIO_IOToggle/Src/main.c
  * @brief   STM32L152RE LED/Button test
  ******************************************************************************
  */

#include "main.h"

/* Private variables ---------------------------------------------------------*/
static GPIO_InitTypeDef GPIO_InitStruct;

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void Error_Handler(void);


/**
  * @brief  Main program
  */
int main(void)
{
  HAL_Init();

  SystemClock_Config();


  /* Enable GPIO clocks */
  LED2_GPIO_CLK_ENABLE();
  USER_BUTTON_GPIO_CLK_ENABLE();


  /* Configure LED2 pin */
  GPIO_InitStruct.Pin = LED2_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

  HAL_GPIO_Init(LED2_GPIO_PORT, &GPIO_InitStruct);


  /* Configure User Button pin */
  GPIO_InitStruct.Pin = USER_BUTTON_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;

  HAL_GPIO_Init(USER_BUTTON_GPIO_PORT, &GPIO_InitStruct);



  while (1)
  {

    /*
     * User Button on NUCLEO-L152RE:
     * pressed  = GPIO_PIN_RESET
     * released = GPIO_PIN_SET
     */

    if (HAL_GPIO_ReadPin(USER_BUTTON_GPIO_PORT, USER_BUTTON_PIN) == GPIO_PIN_RESET)
    {
      /* Fast blinking */
      HAL_GPIO_TogglePin(LED2_GPIO_PORT, LED2_PIN);
      HAL_Delay(100);
    }
    else
    {
      /* Slow blinking */
      HAL_GPIO_TogglePin(LED2_GPIO_PORT, LED2_PIN);
      HAL_Delay(500);
    }

  }
}


/**
  * @brief System Clock Configuration
  *        SYSCLK = 32 MHz
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


/**
  * @brief Error Handler
  */
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
