/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : SIM800C DTMF diagnostic with interrupt RX
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"

/* USER CODE BEGIN Includes */
#include "lcd.h"
#include <string.h>
#include <stdio.h>
/* USER CODE END Includes */

UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */

static uint8_t rxByteIT;

static volatile uint16_t rxIndex = 0U;
static volatile uint32_t oreCount = 0U;

static char rxBuffer[512];
static char lcdLine2[17];

static uint8_t state = 0U;

static uint32_t commandTime = 0U;

/* USER CODE END PV */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);

/* USER CODE BEGIN PFP */

static void LCD_Show(const char *line1, const char *line2);
static void RX_ResetBuffer(void);
static uint16_t RX_GetSnapshot(char *dest, uint16_t size);

/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

static void LCD_Show(const char *line1, const char *line2)
{
    LCD_Clear();

    LCD_SetCursor(0, 0);
    LCD_Print((char *)line1);

    LCD_SetCursor(0, 1);
    LCD_Print((char *)line2);
}

static void RX_ResetBuffer(void)
{
    __disable_irq();

    rxIndex = 0U;
    memset(rxBuffer, 0, sizeof(rxBuffer));

    __enable_irq();
}

static uint16_t RX_GetSnapshot(char *dest, uint16_t size)
{
    uint16_t count;

    __disable_irq();

    count = rxIndex;

    if (count >= size)
    {
        count = size - 1U;
    }

    memcpy(dest, rxBuffer, count);
    dest[count] = '\0';

    __enable_irq();

    return count;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3)
    {
        if (rxIndex < (sizeof(rxBuffer) - 1U))
        {
            rxBuffer[rxIndex] = (char)rxByteIT;
            rxIndex++;

            rxBuffer[rxIndex] = '\0';
        }

        HAL_UART_Receive_IT(
            &huart3,
            &rxByteIT,
            1U
        );
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3)
    {
        if (__HAL_UART_GET_FLAG(
                &huart3,
                UART_FLAG_ORE
            ) != RESET)
        {
            oreCount++;

            __HAL_UART_CLEAR_OREFLAG(
                &huart3
            );
        }

        HAL_UART_Receive_IT(
            &huart3,
            &rxByteIT,
            1U
        );
    }
}

__weak void USART3_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart3);
}

/* USER CODE END 0 */

int main(void)
{
    char snapshot[512];
    char *position;

    int callId;
    int direction;
    int callStatus;
    int mode;
    int multiparty;

    char dtmfKey;

    HAL_Init();

    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART3_UART_Init();

    /* USER CODE BEGIN 2 */

    LCD_Init();

    HAL_NVIC_SetPriority(
        USART3_IRQn,
        0,
        0
    );

    HAL_NVIC_EnableIRQ(
        USART3_IRQn
    );

    if (HAL_UART_Receive_IT(
            &huart3,
            &rxByteIT,
            1U
        ) != HAL_OK)
    {
        LCD_Show(
            "RX IRQ ERROR",
            "START FAILED"
        );

        while (1)
        {
        }
    }

    LCD_Show(
        "DTMF TEST",
        "WAIT 3 SEC"
    );

    HAL_Delay(3000U);

    /*
     * Disable echo.
     */
    RX_ResetBuffer();

    HAL_UART_Transmit(
        &huart3,
        (uint8_t *)"ATE0\r",
        5U,
        1000U
    );

    HAL_Delay(1000U);

    /*
     * Enable DTMF detection.
     */
    RX_ResetBuffer();

    LCD_Show(
        "ENABLE DTMF",
        "DDET..."
    );

    if (HAL_UART_Transmit(
            &huart3,
            (uint8_t *)"AT+DDET=1,0,0\r",
            14U,
            1000U
        ) != HAL_OK)
    {
        LCD_Show(
            "DDET TX ERROR",
            "UART FAILED"
        );

        while (1)
        {
        }
    }

    commandTime = HAL_GetTick();
    state = 0U;

    /* USER CODE END 2 */

    while (1)
    {
        /* USER CODE BEGIN WHILE */

        RX_GetSnapshot(
            snapshot,
            sizeof(snapshot)
        );

        /*
         * State 0:
         * Wait for DDET response.
         */
        if (state == 0U)
        {
            if (strstr(snapshot, "OK") != NULL)
            {
                RX_ResetBuffer();

                LCD_Show(
                    "DDET OK",
                    "WAITING CALL"
                );

                state = 1U;
            }
            else if (strstr(snapshot, "ERROR") != NULL)
            {
                LCD_Show(
                    "DDET ERROR",
                    "COMMAND FAILED"
                );

                state = 99U;
            }
            else if ((HAL_GetTick() - commandTime) >= 3000U)
            {
                LCD_Show(
                    "DDET TIMEOUT",
                    "NO RESPONSE"
                );

                state = 99U;
            }
        }

        /*
         * State 1:
         * Wait for RING.
         */
        else if (state == 1U)
        {
            if (strstr(snapshot, "RING") != NULL)
            {
                RX_ResetBuffer();

                LCD_Show(
                    "RING DETECTED",
                    "SENDING ATA"
                );

                if (HAL_UART_Transmit(
                        &huart3,
                        (uint8_t *)"ATA\r",
                        4U,
                        1000U
                    ) != HAL_OK)
                {
                    LCD_Show(
                        "ATA TX ERROR",
                        "UART FAILED"
                    );

                    state = 99U;
                }
                else
                {
                    commandTime = HAL_GetTick();
                    state = 2U;

                    LCD_Show(
                        "ATA SENT",
                        "WAIT 2 SEC"
                    );
                }
            }
        }

        /*
         * State 2:
         * Wait 2 seconds after ATA, then query CLCC.
         */
        else if (state == 2U)
        {
            if ((HAL_GetTick() - commandTime) >= 2000U)
            {
                RX_ResetBuffer();

                if (HAL_UART_Transmit(
                        &huart3,
                        (uint8_t *)"AT+CLCC\r",
                        8U,
                        1000U
                    ) != HAL_OK)
                {
                    LCD_Show(
                        "CLCC TX ERROR",
                        "UART FAILED"
                    );

                    state = 99U;
                }
                else
                {
                    commandTime = HAL_GetTick();
                    state = 3U;

                    LCD_Show(
                        "CLCC SENT",
                        "WAIT ACTIVE"
                    );
                }
            }
        }

        /*
         * State 3:
         * Confirm active call.
         */
        else if (state == 3U)
        {
            position = strstr(
                snapshot,
                "+CLCC:"
            );

            if (position != NULL)
            {
                if (sscanf(
                        position,
                        "+CLCC: %d,%d,%d,%d,%d",
                        &callId,
                        &direction,
                        &callStatus,
                        &mode,
                        &multiparty
                    ) >= 5)
                {
                    if (callStatus == 0)
                    {
                        RX_ResetBuffer();

                        LCD_Show(
                            "CALL ACTIVE",
                            "PRESS A KEY"
                        );

                        state = 4U;
                    }
                    else
                    {
                        snprintf(
                            lcdLine2,
                            sizeof(lcdLine2),
                            "STATE:%d",
                            callStatus
                        );

                        LCD_Show(
                            "CALL NOT ACTIVE",
                            lcdLine2
                        );
                    }
                }
            }
            else if ((HAL_GetTick() - commandTime) >= 3000U)
            {
                LCD_Show(
                    "CLCC TIMEOUT",
                    "NO CALL DATA"
                );

                state = 99U;
            }
        }

        /*
         * State 4:
         * Wait for +DTMF URC.
         */
        else if (state == 4U)
        {
            position = strstr(
                snapshot,
                "+DTMF:"
            );

            if (position != NULL)
            {
                dtmfKey = '\0';

                if (sscanf(
                        position,
                        "+DTMF: %c",
                        &dtmfKey
                    ) == 1)
                {
                    snprintf(
                        lcdLine2,
                        sizeof(lcdLine2),
                        "KEY: %c",
                        dtmfKey
                    );

                    LCD_Show(
                        "DTMF RECEIVED",
                        lcdLine2
                    );

                    /*
                     * Clear old DTMF so another key can be detected.
                     */
                    RX_ResetBuffer();
                }
            }

            if (strstr(snapshot, "NO CARRIER") != NULL)
            {
                RX_ResetBuffer();

                LCD_Show(
                    "CALL ENDED",
                    "WAITING CALL"
                );

                state = 1U;
            }

            if (strstr(snapshot, "RDY") != NULL)
            {
                LCD_Show(
                    "MODEM REBOOT",
                    "RDY RECEIVED"
                );

                state = 99U;
            }
        }

        HAL_Delay(10U);

        /* USER CODE END WHILE */
    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();

    __HAL_PWR_VOLTAGESCALING_CONFIG(
        PWR_REGULATOR_VOLTAGE_SCALE1
    );

    RCC_OscInitStruct.OscillatorType =
        RCC_OSCILLATORTYPE_HSI;

    RCC_OscInitStruct.HSIState =
        RCC_HSI_ON;

    RCC_OscInitStruct.HSICalibrationValue =
        RCC_HSICALIBRATION_DEFAULT;

    RCC_OscInitStruct.PLL.PLLState =
        RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(
            &RCC_OscInitStruct
        ) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource =
        RCC_SYSCLKSOURCE_HSI;

    RCC_ClkInitStruct.AHBCLKDivider =
        RCC_SYSCLK_DIV1;

    RCC_ClkInitStruct.APB1CLKDivider =
        RCC_HCLK_DIV1;

    RCC_ClkInitStruct.APB2CLKDivider =
        RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(
            &RCC_ClkInitStruct,
            FLASH_LATENCY_0
        ) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_USART3_UART_Init(void)
{
    huart3.Instance = USART3;

    huart3.Init.BaudRate = 115200;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;

    huart3.Init.Mode =
        UART_MODE_TX_RX;

    huart3.Init.HwFlowCtl =
        UART_HWCONTROL_NONE;

    huart3.Init.OverSampling =
        UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart3) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    HAL_GPIO_WritePin(
        GPIOE,
        LCD_RS_Pin |
        LCD_RW_Pin |
        LCD_EN_Pin |
        LCD_D4_Pin |
        LCD_D5_Pin |
        LCD_D6_Pin |
        LCD_D7_Pin,
        GPIO_PIN_RESET
    );

    GPIO_InitStruct.Pin =
        LCD_RS_Pin |
        LCD_RW_Pin |
        LCD_EN_Pin |
        LCD_D4_Pin |
        LCD_D5_Pin |
        LCD_D6_Pin |
        LCD_D7_Pin;

    GPIO_InitStruct.Mode =
        GPIO_MODE_OUTPUT_PP;

    GPIO_InitStruct.Pull =
        GPIO_NOPULL;

    GPIO_InitStruct.Speed =
        GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(
        GPIOE,
        &GPIO_InitStruct
    );
}

void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}

#ifdef USE_FULL_ASSERT

void assert_failed(uint8_t *file, uint32_t line)
{
}

#endif
