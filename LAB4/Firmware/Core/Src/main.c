/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "scheduler.h"
#include <stdarg.h>
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define UART_TX_BUFFER_SIZE 512U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
static char uart_tx_buffer[UART_TX_BUFFER_SIZE];
static uint32_t uart_tx_head;
static uint32_t uart_tx_tail;
static volatile uint32_t last_tick_callback_ms;
static uint32_t cancelled_task_id;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */


/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void UART_Printf(const char *format, ...)
{
    char message[96];
    va_list arguments;
    va_start(arguments, format);
    int length = vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);

    if (length < 0 || (uint32_t)length >= sizeof(message)) {
        Error_Handler();
    }

    uint32_t free_bytes =
        (uart_tx_tail + UART_TX_BUFFER_SIZE - uart_tx_head - 1U)
        % UART_TX_BUFFER_SIZE;
    if ((uint32_t)length > free_bytes) {
        Error_Handler();
    }

    for (int i = 0; i < length; i++) {
        uart_tx_buffer[uart_tx_head] = message[i];
        uart_tx_head = (uart_tx_head + 1U) % UART_TX_BUFFER_SIZE;
    }
}

static void UART_Poll(void)
{
    if (uart_tx_tail != uart_tx_head &&
        __HAL_UART_GET_FLAG(&huart1, UART_FLAG_TXE) != RESET) {
        huart1.Instance->DR = (uint8_t)uart_tx_buffer[uart_tx_tail];
        uart_tx_tail = (uart_tx_tail + 1U) % UART_TX_BUFFER_SIZE;
    }
}

static void Log_Event(const char *name)
{
    UART_Printf("%s t=%lu ms\r\n", name, (unsigned long)get_time());
}

static uint32_t Add_Task(void (*function)(void),
                         uint32_t delay_ticks, uint32_t period_ticks)
{
    uint32_t id = SCH_Add_Task(function, delay_ticks, period_ticks);
    if (id == 0U) {
        Error_Handler();
    }
    return id;
}

static void Toggle_LED_And_Log(uint16_t pin, const char *name)
{
    HAL_GPIO_TogglePin(GPIOA, pin);
    const char *state = HAL_GPIO_ReadPin(GPIOA, pin) == GPIO_PIN_SET ? "ON" : "OFF";
    UART_Printf("%s %s t=%lu ms\r\n", name, state, (unsigned long)get_time());
}

static void Task_LED1(void)
{
    Toggle_LED_And_Log(GPIO_PIN_1, "LED1");
}

static void Task_LED2(void)
{
    Toggle_LED_And_Log(GPIO_PIN_2, "LED2");
}

static void Task_LED3(void)
{
    Toggle_LED_And_Log(GPIO_PIN_3, "LED3");
}

static void Task_LED4(void)
{
    Toggle_LED_And_Log(GPIO_PIN_4, "LED4");
}

static void Task_LED5(void)
{
    Toggle_LED_And_Log(GPIO_PIN_5, "LED5");
}

static void Task_Tick10ms(void)
{
    last_tick_callback_ms = get_time();
}

static void Task_OneShot70ms(void)
{
    Log_Event("ONESHOT 70ms");
}

static void Task_OneShot130ms(void)
{
    Log_Event("ONESHOT 130ms");
}

static void Task_OneShot210ms(void)
{
    Log_Event("ONESHOT 210ms");
}

static void Task_Cancelled(void)
{
    Log_Event("ERROR: cancelled task ran");
    Error_Handler();
}

static void Task_DeleteDemo(void)
{
    uint8_t result = SCH_Delete_Task(cancelled_task_id);
    UART_Printf("DELETE one-shot result=%u t=%lu ms\r\n",
                (unsigned int)result, (unsigned long)get_time());
    if (result != 0U) {
        Error_Handler();
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  SCH_Init();
  UART_Printf("LAB4: tick=10ms, UART=115200 8N1\r\n");
  UART_Printf("LED periods: 500 1000 1500 2000 2500 ms\r\n");

  Add_Task(Task_LED1, 50, 50);
  Add_Task(Task_LED2, 100, 100);
  Add_Task(Task_LED3, 150, 150);
  Add_Task(Task_LED4, 200, 200);
  Add_Task(Task_LED5, 250, 250);
  Add_Task(Task_Tick10ms, 1, 1);

  Add_Task(Task_OneShot210ms, 21, 0);
  Add_Task(Task_OneShot70ms, 7, 0);
  Add_Task(Task_OneShot130ms, 13, 0);

  cancelled_task_id = Add_Task(Task_Cancelled, 100, 0);
  Add_Task(Task_DeleteDemo, 60, 0);

  __HAL_TIM_SET_COUNTER(&htim2, 0);
  __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);
  if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK) {
      Error_Handler();
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
    /* USER CODE BEGIN 3 */
    SCH_Dispatch_Tasks();
    UART_Poll();
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 7999;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 9;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4
                          |GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pins : PA1 PA2 PA3 PA4
                           PA5 */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4
                          |GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */


void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) {
        SCH_Update();
    }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
