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

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */
uint8_t simResponse[128] = {0};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART3_UART_Init();

    LCD_Init();
    LCD_Clear();

    LCD_SetCursor(0, 0);
    LCD_Print("SIM800C START");

    LCD_SetCursor(0, 1);
    LCD_Print("PLEASE WAIT");

    HAL_Delay(15000);

    uint8_t ddetCommand[] = "AT+DDET=1,0,0\r";
    uint8_t clccCommand[] = "AT+CLCC\r";
    uint8_t answerCommand[] = "ATA\r";
    uint8_t rxByte = 0U;

    char ddetResponse[128] = {0};
    char uartResponse[512] = {0};
    char dtmfDisplay[16] = {0};

    char *clccPosition = NULL;
    char *dtmfPosition = NULL;

    uint16_t responseIndex = 0U;

    uint8_t callState = 0U;
    uint8_t callFound = 0U;
    uint8_t validClccResponse = 0U;
    uint8_t clccQueried = 0U;
    uint8_t noCallCount = 0U;

    char dtmfKey = '\0';

    int callId = 0;
    int callDirection = 0;
    int callStatus = 0;
    int callMode = 0;
    int callMultiparty = 0;

    uint32_t receiveStartedAt = 0U;
    uint32_t nextClccAt = 0U;
    uint32_t currentTime = 0U;

    HAL_StatusTypeDef uartStatus;

    while (HAL_UART_Receive(
               &huart3,
               &rxByte,
               1,
               10
           ) == HAL_OK)
    {
    }

    if (HAL_UART_GetError(&huart3) != HAL_UART_ERROR_NONE)
    {
        __HAL_UART_CLEAR_OREFLAG(&huart3);
        huart3.ErrorCode = HAL_UART_ERROR_NONE;
    }

    uartStatus = HAL_UART_Transmit(
        &huart3,
        ddetCommand,
        sizeof(ddetCommand) - 1U,
        1000
    );

    if (uartStatus != HAL_OK)
    {
        LCD_Clear();
        LCD_SetCursor(0, 0);
        LCD_Print("DDET TX ERROR");

        LCD_SetCursor(0, 1);
        LCD_Print("RESET BOARD");

        while (1)
        {
        }
    }

    responseIndex = 0U;
    memset(ddetResponse, 0, sizeof(ddetResponse));

    receiveStartedAt = HAL_GetTick();

    while ((HAL_GetTick() - receiveStartedAt) < 1500U)
    {
        uartStatus = HAL_UART_Receive(
            &huart3,
            &rxByte,
            1,
            20
        );

        if (uartStatus == HAL_OK)
        {
            if (responseIndex < (sizeof(ddetResponse) - 1U))
            {
                ddetResponse[responseIndex] = (char)rxByte;
                responseIndex++;
                ddetResponse[responseIndex] = '\0';
            }
        }
        else if (uartStatus == HAL_ERROR)
        {
            __HAL_UART_CLEAR_OREFLAG(&huart3);
            huart3.ErrorCode = HAL_UART_ERROR_NONE;
        }
    }

    if (strstr(ddetResponse, "OK") == NULL)
    {
        LCD_Clear();
        LCD_SetCursor(0, 0);
        LCD_Print("DDET FAILED");

        LCD_SetCursor(0, 1);
        LCD_Print("CHECK MODULE");

        while (1)
        {
        }
    }

    LCD_Clear();
    LCD_SetCursor(0, 0);
    LCD_Print("DTMF ENABLED");

    LCD_SetCursor(0, 1);
    LCD_Print("SYSTEM READY");

    HAL_Delay(1500);

    LCD_Clear();
    LCD_SetCursor(0, 0);
    LCD_Print("SYSTEM READY");

    LCD_SetCursor(0, 1);
    LCD_Print("WAITING FOR CALL");

    nextClccAt = HAL_GetTick();

    while (1)
    {
        responseIndex = 0U;
        clccQueried = 0U;
        callFound = 0U;
        validClccResponse = 0U;

        callId = 0;
        callDirection = 0;
        callStatus = 0;
        callMode = 0;
        callMultiparty = 0;

        memset(uartResponse, 0, sizeof(uartResponse));

        receiveStartedAt = HAL_GetTick();

        while ((HAL_GetTick() - receiveStartedAt) < 100U)
        {
            uartStatus = HAL_UART_Receive(
                &huart3,
                &rxByte,
                1,
                5
            );

            if (uartStatus == HAL_OK)
            {
                if (responseIndex < (sizeof(uartResponse) - 1U))
                {
                    uartResponse[responseIndex] = (char)rxByte;
                    responseIndex++;
                    uartResponse[responseIndex] = '\0';
                }
            }
            else if (uartStatus == HAL_ERROR)
            {
                __HAL_UART_CLEAR_OREFLAG(&huart3);
                huart3.ErrorCode = HAL_UART_ERROR_NONE;
            }
        }

        currentTime = HAL_GetTick();

        if ((int32_t)(currentTime - nextClccAt) >= 0)
        {
            clccQueried = 1U;

            uartStatus = HAL_UART_Transmit(
                &huart3,
                clccCommand,
                sizeof(clccCommand) - 1U,
                1000
            );

            if (uartStatus == HAL_OK)
            {
                receiveStartedAt = HAL_GetTick();

                while ((HAL_GetTick() - receiveStartedAt) < 700U)
                {
                    uartStatus = HAL_UART_Receive(
                        &huart3,
                        &rxByte,
                        1,
                        20
                    );

                    if (uartStatus == HAL_OK)
                    {
                        if (responseIndex < (sizeof(uartResponse) - 1U))
                        {
                            uartResponse[responseIndex] = (char)rxByte;
                            responseIndex++;
                            uartResponse[responseIndex] = '\0';
                        }
                    }
                    else if (uartStatus == HAL_ERROR)
                    {
                        __HAL_UART_CLEAR_OREFLAG(&huart3);
                        huart3.ErrorCode = HAL_UART_ERROR_NONE;
                    }
                }
            }

            nextClccAt = HAL_GetTick() + 200U;
        }

        dtmfPosition = uartResponse;

        while ((dtmfPosition = strstr(dtmfPosition, "+DTMF:")) != NULL)
        {
            dtmfPosition += 6;

            while ((*dtmfPosition == ' ') ||
                   (*dtmfPosition == '\t'))
            {
                dtmfPosition++;
            }

            if (((*dtmfPosition >= '0') &&
                 (*dtmfPosition <= '9')) ||
                (*dtmfPosition == '*') ||
                (*dtmfPosition == '#') ||
                (*dtmfPosition == 'A') ||
                (*dtmfPosition == 'B') ||
                (*dtmfPosition == 'C') ||
                (*dtmfPosition == 'D'))
            {
                dtmfKey = *dtmfPosition;

                memset(dtmfDisplay, 0, sizeof(dtmfDisplay));

                dtmfDisplay[0] = 'K';
                dtmfDisplay[1] = 'E';
                dtmfDisplay[2] = 'Y';
                dtmfDisplay[3] = ':';
                dtmfDisplay[4] = ' ';
                dtmfDisplay[5] = dtmfKey;
                dtmfDisplay[6] = '\0';

                LCD_Clear();
                LCD_SetCursor(0, 0);
                LCD_Print("DTMF RECEIVED");

                LCD_SetCursor(0, 1);
                LCD_Print(dtmfDisplay);
            }

            dtmfPosition++;
        }

        if (clccQueried != 0U)
        {
            if (strstr(uartResponse, "OK") != NULL)
            {
                validClccResponse = 1U;
            }

            clccPosition = strstr(uartResponse, "+CLCC:");

            if (clccPosition != NULL)
            {
                if (sscanf(
                        clccPosition,
                        "+CLCC: %d,%d,%d,%d,%d",
                        &callId,
                        &callDirection,
                        &callStatus,
                        &callMode,
                        &callMultiparty
                    ) >= 5)
                {
                    callFound = 1U;
                }
            }

            if ((callFound != 0U) &&
                (callDirection == 1) &&
                (callStatus == 4) &&
                (callState == 0U))
            {
                LCD_Clear();
                LCD_SetCursor(0, 0);
                LCD_Print("INCOMING CALL");

                LCD_SetCursor(0, 1);
                LCD_Print("ANSWERING CALL");

                uartStatus = HAL_UART_Transmit(
                    &huart3,
                    answerCommand,
                    sizeof(answerCommand) - 1U,
                    1000
                );

                if (uartStatus == HAL_OK)
                {
                    callState = 1U;
                    noCallCount = 0U;

                    nextClccAt = HAL_GetTick() + 1500U;

                    LCD_Clear();
                    LCD_SetCursor(0, 0);
                    LCD_Print("CALL CONNECTED");

                    LCD_SetCursor(0, 1);
                    LCD_Print("PRESS A KEY");
                }
                else
                {
                    callState = 0U;

                    LCD_Clear();
                    LCD_SetCursor(0, 0);
                    LCD_Print("ATA TX ERROR");

                    LCD_SetCursor(0, 1);
                    LCD_Print("CALL NOT ANSWER");
                }
            }
            else if (callFound != 0U)
            {
                noCallCount = 0U;

                if ((callStatus == 0) &&
                    (callState == 0U))
                {
                    callState = 1U;

                    LCD_Clear();
                    LCD_SetCursor(0, 0);
                    LCD_Print("CALL CONNECTED");

                    LCD_SetCursor(0, 1);
                    LCD_Print("PRESS A KEY");
                }
            }
            else if ((validClccResponse != 0U) &&
                     (callState != 0U))
            {
                if (noCallCount < 255U)
                {
                    noCallCount++;
                }

                if (noCallCount >= 3U)
                {
                    callState = 0U;
                    noCallCount = 0U;
                    dtmfKey = '\0';

                    LCD_Clear();
                    LCD_SetCursor(0, 0);
                    LCD_Print("CALL ENDED");

                    LCD_SetCursor(0, 1);
                    LCD_Print("PLEASE WAIT");

                    HAL_Delay(1000);

                    LCD_Clear();
                    LCD_SetCursor(0, 0);
                    LCD_Print("SYSTEM READY");

                    LCD_SetCursor(0, 1);
                    LCD_Print("WAITING FOR CALL");
                }
            }
        }

        HAL_Delay(50);
    }
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
