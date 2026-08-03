/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "lcd.h"
#include <string.h>
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define CALL_IDLE 0U
#define CALL_RINGING 1U
#define CALL_ANSWERING 2U
#define CALL_ACTIVE 3U
#define UART_RX_RING_SIZE 512U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */
static uint8_t uartRxByte = 0U;
static uint8_t uartRxRing[UART_RX_RING_SIZE];
static volatile uint16_t uartRxHead = 0U;
static volatile uint16_t uartRxTail = 0U;
static volatile uint8_t uartRxArmed = 0U;
static volatile uint32_t uartRxOverflowCount = 0U;

static char uartLine[128];
static uint16_t uartLineIndex = 0U;
static char lcdLine2[17];

static uint8_t commandPending = 0U;
static int8_t commandResult = 0;

static uint8_t modemConfigured = 0U;
static uint8_t monitorResetsEnabled = 0U;
static uint32_t modemResetCount = 0U;
static uint32_t nextConfigAttemptAt = 0U;

static uint8_t callState = CALL_IDLE;
static uint8_t clccCallFound = 0U;
static uint8_t noCallCount = 0U;
static uint32_t nextClccAt = 0U;
static uint32_t nextAnswerAttemptAt = 0U;

static uint8_t dtmfVisible = 0U;
static uint32_t dtmfVisibleUntil = 0U;
static uint8_t dtmfRearmRequested = 0U;
static uint8_t dtmfRearmAttempts = 0U;
static uint32_t nextDtmfRearmAt = 0U;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);
/* USER CODE BEGIN PFP */
static void LCD_Show(const char *line1, const char *line2);
static void LCD_ShowWaiting(void);
static void LCD_ShowConnected(void);
static void LCD_ShowDTMF(char key);
static void LCD_ShowReboot(void);
static void SIM_ClearUARTError(void);
static void SIM_StartUARTReceiver(void);
static void SIM_PumpUART(void);
static void SIM_ProcessLine(char *line);
static void SIM_ProcessByte(uint8_t rxByte);
static void SIM_WatchUART(uint32_t duration);
static int8_t SIM_SendCommandAndWait(const char *command, uint32_t timeout);
static uint8_t SIM_ConfigureModem(void);
static void SIM_EndCall(void);
static void SIM_PollCallState(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void LCD_Show(const char *line1, const char *line2)
{
    LCD_Clear();
    LCD_SetCursor(0, 0);
    LCD_Print((char *)line1);
    LCD_SetCursor(0, 1);
    LCD_Print((char *)line2);
}

static void LCD_ShowWaiting(void)
{
    snprintf(
        lcdLine2,
        sizeof(lcdLine2),
        "RESETS:%lu",
        (unsigned long)modemResetCount
    );

    LCD_Show("WAITING FOR CALL", lcdLine2);
}

static void LCD_ShowConnected(void)
{
    LCD_Show("CALL CONNECTED", "PRESS A KEY");
}

static void LCD_ShowDTMF(char key)
{
    snprintf(lcdLine2, sizeof(lcdLine2), "KEY: %c", key);
    LCD_Show("DTMF RECEIVED", lcdLine2);
    dtmfVisible = 1U;
    dtmfVisibleUntil = HAL_GetTick() + 2000U;
}

static void LCD_ShowReboot(void)
{
    snprintf(
        lcdLine2,
        sizeof(lcdLine2),
        "COUNT:%lu",
        (unsigned long)modemResetCount
    );

    LCD_Show("MODEM REBOOT", lcdLine2);
}

static void SIM_ClearUARTError(void)
{
    __HAL_UART_CLEAR_PEFLAG(&huart3);
    huart3.ErrorCode = HAL_UART_ERROR_NONE;
}

static void SIM_StartUARTReceiver(void)
{
    if (uartRxArmed == 0U)
    {
        if (HAL_UART_Receive_IT(
                &huart3,
                &uartRxByte,
                1U
            ) == HAL_OK)
        {
            uartRxArmed = 1U;
        }
    }
}

static void SIM_PumpUART(void)
{
    while (uartRxTail != uartRxHead)
    {
        uint8_t rxByte = uartRxRing[uartRxTail];

        uartRxTail++;

        if (uartRxTail >= UART_RX_RING_SIZE)
        {
            uartRxTail = 0U;
        }

        SIM_ProcessByte(rxByte);
    }

    SIM_StartUARTReceiver();
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    uint16_t nextHead;

    if (huart->Instance != USART3)
    {
        return;
    }

    uartRxArmed = 0U;
    nextHead = uartRxHead + 1U;

    if (nextHead >= UART_RX_RING_SIZE)
    {
        nextHead = 0U;
    }

    if (nextHead != uartRxTail)
    {
        uartRxRing[uartRxHead] = uartRxByte;
        uartRxHead = nextHead;
    }
    else
    {
        uartRxOverflowCount++;
    }

    SIM_StartUARTReceiver();
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART3)
    {
        return;
    }

    uartRxArmed = 0U;
    SIM_ClearUARTError();
    SIM_StartUARTReceiver();
}

__weak void USART3_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart3);
}

static void SIM_EndCall(void)
{
    callState = CALL_IDLE;
    clccCallFound = 0U;
    noCallCount = 0U;
    dtmfVisible = 0U;
    dtmfRearmRequested = 0U;
    dtmfRearmAttempts = 0U;
    LCD_Show("CALL ENDED", "PLEASE WAIT");
    SIM_WatchUART(800U);
    LCD_ShowWaiting();
}

static void SIM_ProcessLine(char *line)
{
    char *position;
    int callId;
    int callDirection;
    int callStatus;
    int callMode;
    int callMultiparty;
    char key;

    if (line[0] == '\0')
    {
        return;
    }

    if (strcmp(line, "OK") == 0)
    {
        if ((commandPending != 0U) && (commandResult == 0))
        {
            commandResult = 1;
        }

        return;
    }

    if (strcmp(line, "ERROR") == 0)
    {
        if ((commandPending != 0U) && (commandResult == 0))
        {
            commandResult = -1;
        }

        return;
    }

    if (strcmp(line, "RDY") == 0)
    {
        if (monitorResetsEnabled != 0U)
        {
            modemResetCount++;
            LCD_ShowReboot();
        }

        modemConfigured = 0U;
        callState = CALL_IDLE;
        clccCallFound = 0U;
        noCallCount = 0U;
        dtmfVisible = 0U;
        dtmfRearmRequested = 0U;
        dtmfRearmAttempts = 0U;
        nextConfigAttemptAt = HAL_GetTick() + 1500U;
        return;
    }

    if (strcmp(line, "RING") == 0)
    {
        if (callState == CALL_IDLE)
        {
            callState = CALL_RINGING;
            dtmfRearmRequested = 1U;
            dtmfRearmAttempts = 0U;
            nextDtmfRearmAt = HAL_GetTick();
            nextAnswerAttemptAt = HAL_GetTick();
            LCD_Show("INCOMING CALL", "DTMF SETUP...");
        }

        return;
    }

    if ((strcmp(line, "NO CARRIER") == 0) ||
        (strcmp(line, "BUSY") == 0) ||
        (strcmp(line, "NO ANSWER") == 0))
    {
        if (callState != CALL_IDLE)
        {
            SIM_EndCall();
        }

        return;
    }

    position = strstr(line, "+DTMF:");

    if (position != NULL)
    {
        position += 6;

        while ((*position == ' ') || (*position == '\t'))
        {
            position++;
        }

        key = *position;

        if (((key >= '0') && (key <= '9')) ||
            (key == '*') ||
            (key == '#') ||
            (key == 'A') ||
            (key == 'B') ||
            (key == 'C') ||
            (key == 'D'))
        {
            LCD_ShowDTMF(key);
        }

        return;
    }

    position = strstr(line, "+CLCC:");

    if (position != NULL)
    {
        if (sscanf(
                position,
                "+CLCC: %d,%d,%d,%d,%d",
                &callId,
                &callDirection,
                &callStatus,
                &callMode,
                &callMultiparty
            ) >= 5)
        {
            clccCallFound = 1U;
            noCallCount = 0U;

            if ((callDirection == 1) && (callStatus == 4))
            {
                if (callState == CALL_IDLE)
                {
                    callState = CALL_RINGING;
                    dtmfRearmRequested = 1U;
                    dtmfRearmAttempts = 0U;
                    nextDtmfRearmAt = HAL_GetTick();
                    nextAnswerAttemptAt = HAL_GetTick();
                    LCD_Show("INCOMING CALL", "DTMF SETUP...");
                }
            }
            else if (callStatus == 0)
            {
                if (callState != CALL_ACTIVE)
                {
                    callState = CALL_ACTIVE;
                    dtmfRearmRequested = 1U;
                    dtmfRearmAttempts = 0U;
                    nextDtmfRearmAt = HAL_GetTick();

                    if (dtmfVisible == 0U)
                    {
                        LCD_Show("CALL CONNECTED", "DTMF SETUP...");
                    }
                }
            }
        }
    }
}

static void SIM_ProcessByte(uint8_t rxByte)
{
    if ((rxByte == 0x00U) || (rxByte == 0xFFU))
    {
        return;
    }

    if ((rxByte == '\r') || (rxByte == '\n'))
    {
        if (uartLineIndex > 0U)
        {
            uartLine[uartLineIndex] = '\0';
            SIM_ProcessLine(uartLine);
            uartLineIndex = 0U;
            uartLine[0] = '\0';
        }

        return;
    }

    if (uartLineIndex < (sizeof(uartLine) - 1U))
    {
        uartLine[uartLineIndex] = (char)rxByte;
        uartLineIndex++;
        uartLine[uartLineIndex] = '\0';
    }
    else
    {
        uartLineIndex = 0U;
        uartLine[0] = '\0';
    }
}

static void SIM_WatchUART(uint32_t duration)
{
    uint32_t startedAt;

    startedAt = HAL_GetTick();

    while ((HAL_GetTick() - startedAt) < duration)
    {
        SIM_PumpUART();
        HAL_Delay(1U);
    }

    SIM_PumpUART();
}

static int8_t SIM_SendCommandAndWait(
    const char *command,
    uint32_t timeout
)
{
    HAL_StatusTypeDef status;

    SIM_WatchUART(50U);

    commandResult = 0;
    commandPending = 1U;

    status = HAL_UART_Transmit(
        &huart3,
        (uint8_t *)command,
        strlen(command),
        1000U
    );

    if (status != HAL_OK)
    {
        commandPending = 0U;
        return -2;
    }

    SIM_WatchUART(timeout);

    commandPending = 0U;
    return commandResult;
}

static uint8_t SIM_ConfigureModem(void)
{
    int8_t result;
    uint32_t resetCountBefore;

    resetCountBefore = modemResetCount;

    LCD_Show("MODEM SETUP", "CHECKING AT");

    result = SIM_SendCommandAndWait("AT\r", 800U);

    if ((result != 1) ||
        (modemResetCount != resetCountBefore))
    {
        return 0U;
    }

    SIM_SendCommandAndWait("ATE0\r", 800U);

    if (modemResetCount != resetCountBefore)
    {
        return 0U;
    }

    LCD_Show("MODEM SETUP", "ENABLING DTMF");

    result = SIM_SendCommandAndWait(
        "AT+DDET=1,0,0\r",
        1500U
    );

    if ((result != 1) ||
        (modemResetCount != resetCountBefore))
    {
        return 0U;
    }

    modemConfigured = 1U;
    callState = CALL_IDLE;
    clccCallFound = 0U;
    noCallCount = 0U;
    dtmfRearmRequested = 0U;
    dtmfRearmAttempts = 0U;
    nextClccAt = HAL_GetTick();

    LCD_Show("DTMF ENABLED", "SYSTEM READY");
    SIM_WatchUART(800U);
    LCD_ShowWaiting();

    return 1U;
}

static void SIM_PollCallState(void)
{
    int8_t result;

    clccCallFound = 0U;

    result = SIM_SendCommandAndWait(
        "AT+CLCC\r",
        700U
    );

    if (result == 1)
    {
        if ((clccCallFound == 0U) &&
            (callState != CALL_IDLE))
        {
            if (noCallCount < 255U)
            {
                noCallCount++;
            }

            if (noCallCount >= 3U)
            {
                SIM_EndCall();
            }
        }
        else if (clccCallFound != 0U)
        {
            noCallCount = 0U;
        }
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
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_NVIC_SetPriority(USART3_IRQn, 0U, 0U);
  HAL_NVIC_EnableIRQ(USART3_IRQn);
  SIM_StartUARTReceiver();

  LCD_Init();
  LCD_Show("SIM800C START", "PLEASE WAIT");

  SIM_WatchUART(15000U);

  modemResetCount = 0U;
  monitorResetsEnabled = 1U;
  nextConfigAttemptAt = HAL_GetTick();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      uint32_t currentTime;
      int8_t answerResult;

      SIM_WatchUART(20U);
      currentTime = HAL_GetTick();

      if ((dtmfVisible != 0U) &&
          ((int32_t)(currentTime - dtmfVisibleUntil) >= 0))
      {
          dtmfVisible = 0U;

          if (callState == CALL_ACTIVE)
          {
              LCD_ShowConnected();
          }
      }

      if (modemConfigured == 0U)
      {
          if ((int32_t)(currentTime - nextConfigAttemptAt) >= 0)
          {
              if (SIM_ConfigureModem() == 0U)
              {
                  LCD_Show("MODEM NOT READY", "RETRYING...");
                  nextConfigAttemptAt = HAL_GetTick() + 2000U;
              }
          }

          continue;
      }

      if ((dtmfRearmRequested != 0U) &&
          ((int32_t)(currentTime - nextDtmfRearmAt) >= 0))
      {
          int8_t dtmfResult;

          dtmfResult = SIM_SendCommandAndWait(
              "AT+DDET=1,0,0\r",
              1500U
          );

          if (dtmfResult == 1)
          {
              dtmfRearmRequested = 0U;
              dtmfRearmAttempts = 0U;

              if (callState == CALL_ACTIVE)
              {
                  LCD_ShowConnected();
              }
              else if (callState == CALL_RINGING)
              {
                  LCD_Show("DTMF ENABLED", "ANSWERING...");
              }
          }
          else
          {
              dtmfRearmAttempts++;

              if (dtmfRearmAttempts >= 3U)
              {
                  dtmfRearmRequested = 0U;
                  dtmfRearmAttempts = 0U;

                  if (callState == CALL_ACTIVE)
                  {
                      LCD_ShowConnected();
                  }
              }
              else
              {
                  nextDtmfRearmAt = HAL_GetTick() + 500U;
              }
          }

          continue;
      }

      if ((callState == CALL_RINGING) &&
          ((int32_t)(currentTime - nextAnswerAttemptAt) >= 0))
      {
          LCD_Show("INCOMING CALL", "ANSWERING...");

          answerResult = SIM_SendCommandAndWait(
              "ATA\r",
              1500U
          );

          if (answerResult == 1)
          {
              callState = CALL_ANSWERING;
              noCallCount = 0U;
              LCD_Show("ANSWER SENT", "WAITING...");
              nextClccAt = HAL_GetTick();
          }
          else
          {
              nextAnswerAttemptAt = HAL_GetTick() + 1500U;
          }
      }

      currentTime = HAL_GetTick();

      if ((int32_t)(currentTime - nextClccAt) >= 0)
      {
          SIM_PollCallState();
          nextClccAt = HAL_GetTick() + 1000U;
      }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters in the
  * RCC_OscInitTypeDef structure.
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
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, LCD_RS_Pin|LCD_RW_Pin|LCD_EN_Pin|LCD_D4_Pin
                          |LCD_D5_Pin|LCD_D6_Pin|LCD_D7_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : LCD_RS_Pin LCD_RW_Pin LCD_EN_Pin LCD_D4_Pin
                           LCD_D5_Pin LCD_D6_Pin LCD_D7_Pin */
  GPIO_InitStruct.Pin = LCD_RS_Pin|LCD_RW_Pin|LCD_EN_Pin|LCD_D4_Pin
                          |LCD_D5_Pin|LCD_D6_Pin|LCD_D7_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: source line number
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
