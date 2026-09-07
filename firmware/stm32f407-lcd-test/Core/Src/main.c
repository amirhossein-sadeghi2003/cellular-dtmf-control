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
#include "sim800_voice_data.h"
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

/* VOICE_UPLOAD_PATCH_V1 */
static uint32_t voiceOffset = 0U;
static uint32_t voiceChunkSize = 0U;
static char fsCommand[96];

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
static uint8_t Voice_StartNextWrite(void);

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


static uint8_t Voice_StartNextWrite(void)
{
    uint32_t remaining;
    uint8_t mode;
    int commandLength;

    if (voiceOffset >= SIM800_VOICE_DATA_LEN)
    {
        return 0U;
    }

    remaining =
        SIM800_VOICE_DATA_LEN - voiceOffset;

    if (remaining > 10240U)
    {
        voiceChunkSize = 10240U;
    }
    else
    {
        voiceChunkSize = remaining;
    }

    /*
     * First chunk: write from beginning.
     * Following chunks: append to end.
     */
    mode = (voiceOffset == 0U) ? 0U : 1U;

    commandLength = snprintf(
        fsCommand,
        sizeof(fsCommand),
        "AT+FSWRITE=C:\\voice.wav,%u,%lu,10\r",
        (unsigned int)mode,
        (unsigned long)voiceChunkSize
    );

    if ((commandLength <= 0) ||
        ((uint32_t)commandLength >= sizeof(fsCommand)))
    {
        return 0U;
    }

    snprintf(
        lcdLine2,
        sizeof(lcdLine2),
        "%lu/%lu",
        (unsigned long)voiceOffset,
        (unsigned long)SIM800_VOICE_DATA_LEN
    );

    LCD_Show(
        "WRITE VOICE",
        lcdLine2
    );

    RX_ResetBuffer();

    if (HAL_UART_Transmit(
            &huart3,
            (uint8_t *)fsCommand,
            (uint16_t)commandLength,
            1000U
        ) != HAL_OK)
    {
        return 0U;
    }

    commandTime = HAL_GetTick();
    state = 22U;

    return 1U;
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
    char driveLetter;

    char smsStorage[8];
    int smsIndex;

    char memDrive;
    unsigned long freeBytes;

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
     * Test CMEDPLAY support.
     */
    RX_ResetBuffer();

    LCD_Show(
        "CMEDPLAY TEST",
        "SENDING..."
    );

    if (HAL_UART_Transmit(
            &huart3,
            (uint8_t *)"AT+CMEDPLAY=?\r",
            sizeof("AT+CMEDPLAY=?\r") - 1U,
            1000U
        ) != HAL_OK)
    {
        LCD_Show(
            "CMEDPLAY",
            "TX ERROR"
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
        	        "GET LOCAL DRIVE",
        	        "FSDRIVE..."
        	    );

        	    if (HAL_UART_Transmit(
        	            &huart3,
        	            (uint8_t *)"AT+FSDRIVE=0\r",
        	            sizeof("AT+FSDRIVE=0\r") - 1U,
        	            1000U
        	        ) != HAL_OK)
        	    {
        	        LCD_Show(
        	            "FSDRIVE",
        	            "TX ERROR"
        	        );

        	        state = 99U;
        	    }
        	    else
        	    {
        	        commandTime = HAL_GetTick();
        	        state = 10U;
        	    }
        	}
            else if (strstr(snapshot, "ERROR") != NULL)
            {
                LCD_Show(
                    "CMEDPLAY",
                    "UNSUPPORTED"
                );

                state = 99U;
            }

            else if ((HAL_GetTick() - commandTime) >= 3000U)
            {
                LCD_Show(
                    "CMEDPLAY",
                    "TIMEOUT"
                );

                state = 99U;
            }
        }




        else if (state == 10U)
        {
            position = strstr(
                snapshot,
                "+FSDRIVE:"
            );

            if (position != NULL)
            {
                driveLetter = '\0';

                if (sscanf(
                        position,
                        "+FSDRIVE: %c",
                        &driveLetter
                    ) == 1)
                {
                    snprintf(
                        lcdLine2,
                        sizeof(lcdLine2),
                        "DRIVE: %c",
                        driveLetter
                    );

                    LCD_Show(
                        "LOCAL STORAGE",
                        lcdLine2
                    );

                    HAL_Delay(1000U);

                    RX_ResetBuffer();

                    LCD_Show(
                        "CHECK MEMORY",
                        "FSMEM..."
                    );

                    if (HAL_UART_Transmit(
                            &huart3,
                            (uint8_t *)"AT+FSMEM\r",
                            sizeof("AT+FSMEM\r") - 1U,
                            1000U
                        ) != HAL_OK)
                    {
                        LCD_Show(
                            "FSMEM",
                            "TX ERROR"
                        );

                        state = 99U;
                    }
                    else
                    {
                        commandTime = HAL_GetTick();
                        state = 11U;
                    }
                }
            }
            else if (strstr(snapshot, "ERROR") != NULL)
            {
                LCD_Show(
                    "FSDRIVE",
                    "ERROR"
                );

                state = 99U;
            }
            else if ((HAL_GetTick() - commandTime) >= 3000U)
            {
                LCD_Show(
                    "FSDRIVE",
                    "TIMEOUT"
                );

                state = 99U;
            }
        }


        else if (state == 11U)
        {
            position = strstr(
                snapshot,
                "+FSMEM:"
            );

            if (position != NULL)
            {
                memDrive = '\0';
                freeBytes = 0UL;

                if (sscanf(
                        position,
                        "+FSMEM: %c:%lubytes",
                        &memDrive,
                        &freeBytes
                    ) == 2)
                {
                    snprintf(
                        lcdLine2,
                        sizeof(lcdLine2),
                        "%lu bytes",
                        freeBytes
                    );

                    LCD_Show(
                        "FREE MEMORY",
                        lcdLine2
                    );

                    HAL_Delay(1000U);

                    RX_ResetBuffer();

                    LCD_Show(
                        "LIST FILES",
                        "FSLS..."
                    );

                    if (HAL_UART_Transmit(
                            &huart3,
                            (uint8_t *)"AT+FSLS=C:\\\r",
                            sizeof("AT+FSLS=C:\\\r") - 1U,
                            1000U
                        ) != HAL_OK)
                    {
                        LCD_Show(
                            "FSLS",
                            "TX ERROR"
                        );

                        state = 99U;
                    }
                    else
                    {
                        commandTime = HAL_GetTick();
                        state = 13U;
                    }
                }
            }
            else if (strstr(snapshot, "ERROR") != NULL)
            {
                LCD_Show(
                    "FSMEM",
                    "ERROR"
                );

                state = 99U;
            }
            else if ((HAL_GetTick() - commandTime) >= 3000U)
            {
                LCD_Show(
                    "FSMEM",
                    "TIMEOUT"
                );

                state = 99U;
            }
        }

        else if (state == 13U)
        {
            if (strstr(snapshot, "test.wav") != NULL)
            {
                RX_ResetBuffer();

                LCD_Show(
                    "WRITE TEST",
                    "WAIT PROMPT"
                );

                if (HAL_UART_Transmit(
                        &huart3,
                        (uint8_t *)"AT+FSWRITE=C:\\test.wav,0,5,10\r",
                        sizeof("AT+FSWRITE=C:\\test.wav,0,5,10\r") - 1U,
                        1000U
                    ) != HAL_OK)
                {
                    LCD_Show(
                        "FSWRITE",
                        "TX ERROR"
                    );

                    state = 99U;
                }
                else
                {
                    commandTime = HAL_GetTick();
                    state = 14U;
                }
            }
            else if (strstr(snapshot, "ERROR") != NULL)
            {
                LCD_Show(
                    "FSLS",
                    "ERROR"
                );

                state = 99U;
            }
            else if ((HAL_GetTick() - commandTime) >= 3000U)
            {
                LCD_Show(
                    "FILE NOT FOUND",
                    "test.wav"
                );

                state = 99U;
            }
        }

        else if (state == 14U)
        {
            if (strchr(snapshot, '>') != NULL)
            {
                RX_ResetBuffer();

                LCD_Show(
                    "SEND DATA",
                    "HELLO"
                );

                if (HAL_UART_Transmit(
                        &huart3,
                        (uint8_t *)"HELLO",
                        5U,
                        1000U
                    ) != HAL_OK)
                {
                    LCD_Show(
                        "DATA TX",
                        "ERROR"
                    );

                    state = 99U;
                }
                else
                {
                    commandTime = HAL_GetTick();
                    state = 15U;
                }
            }
            else if (strstr(snapshot, "ERROR") != NULL)
            {
                LCD_Show(
                    "FSWRITE",
                    "ERROR"
                );

                state = 99U;
            }
            else if ((HAL_GetTick() - commandTime) >= 3000U)
            {
                LCD_Show(
                    "FSWRITE",
                    "NO PROMPT"
                );

                state = 99U;
            }
        }

        else if (state == 15U)
        {
            if (strstr(snapshot, "OK") != NULL)
            {
                LCD_Show(
                    "WRITE OK",
                    "5 BYTES SAVED"
                );

                HAL_Delay(1000U);

                RX_ResetBuffer();

                LCD_Show(
                    "CHECK FILE SIZE",
                    "FSFLSIZE..."
                );

                if (HAL_UART_Transmit(
                        &huart3,
                        (uint8_t *)"AT+FSFLSIZE=C:\\test.wav\r",
                        sizeof("AT+FSFLSIZE=C:\\test.wav\r") - 1U,
                        1000U
                    ) != HAL_OK)
                {
                    LCD_Show(
                        "FSFLSIZE",
                        "TX ERROR"
                    );

                    state = 99U;
                }
                else
                {
                    commandTime = HAL_GetTick();
                    state = 16U;
                }
            }
            else if (strstr(snapshot, "ERROR") != NULL)
            {
                LCD_Show(
                    "WRITE",
                    "ERROR"
                );

                state = 99U;
            }
            else if ((HAL_GetTick() - commandTime) >= 12000U)
            {
                LCD_Show(
                    "WRITE",
                    "TIMEOUT"
                );

                state = 99U;
            }
        }

        else if (state == 16U)
        {
            position = strstr(
                snapshot,
                "+FSFLSIZE:"
            );

            if (position != NULL)
            {
                unsigned long fileSize = 0UL;

                if (sscanf(
                        position,
                        "+FSFLSIZE: %lu",
                        &fileSize
                    ) == 1)
                {
                    snprintf(
                        lcdLine2,
                        sizeof(lcdLine2),
                        "%lu bytes",
                        fileSize
                    );

                    LCD_Show(
                        "FILE SIZE",
                        lcdLine2
                    );

                    RX_ResetBuffer();

                    LCD_Show(
                        "VOICE UPLOAD",
                        "DELETE OLD..."
                    );

                    if (HAL_UART_Transmit(
                            &huart3,
                            (uint8_t *)"AT+FSDEL=C:\\voice.wav\r",
                            sizeof("AT+FSDEL=C:\\voice.wav\r") - 1U,
                            1000U
                        ) != HAL_OK)
                    {
                        LCD_Show(
                            "VOICE DELETE",
                            "TX ERROR"
                        );

                        state = 99U;
                    }
                    else
                    {
                        commandTime = HAL_GetTick();
                        state = 20U;
                    }
                }
            }
            else if (strstr(snapshot, "ERROR") != NULL)
            {
                LCD_Show(
                    "FSFLSIZE",
                    "ERROR"
                );

                state = 99U;
            }
            else if ((HAL_GetTick() - commandTime) >= 3000U)
            {
                LCD_Show(
                    "FSFLSIZE",
                    "TIMEOUT"
                );

                state = 99U;
            }
        }


        /*
         * State 20:
         * Delete old voice.wav.
         *
         * ERROR is allowed here because the file may not
         * exist yet.
         */
        else if (state == 20U)
        {
            if ((strstr(snapshot, "OK") != NULL) ||
                (strstr(snapshot, "ERROR") != NULL))
            {
                RX_ResetBuffer();

                LCD_Show(
                    "VOICE UPLOAD",
                    "CREATE FILE"
                );

                if (HAL_UART_Transmit(
                        &huart3,
                        (uint8_t *)"AT+FSCREATE=C:\\voice.wav\r",
                        sizeof("AT+FSCREATE=C:\\voice.wav\r") - 1U,
                        1000U
                    ) != HAL_OK)
                {
                    LCD_Show(
                        "VOICE CREATE",
                        "TX ERROR"
                    );

                    state = 99U;
                }
                else
                {
                    commandTime = HAL_GetTick();
                    state = 21U;
                }
            }
            else if ((HAL_GetTick() - commandTime) >= 3000U)
            {
                /*
                 * If delete gives no response, try create anyway.
                 */
                RX_ResetBuffer();

                if (HAL_UART_Transmit(
                        &huart3,
                        (uint8_t *)"AT+FSCREATE=C:\\voice.wav\r",
                        sizeof("AT+FSCREATE=C:\\voice.wav\r") - 1U,
                        1000U
                    ) != HAL_OK)
                {
                    LCD_Show(
                        "VOICE CREATE",
                        "TX ERROR"
                    );

                    state = 99U;
                }
                else
                {
                    commandTime = HAL_GetTick();
                    state = 21U;
                }
            }
        }

        /*
         * State 21:
         * Wait until voice.wav is created.
         */
        else if (state == 21U)
        {
            if (strstr(snapshot, "OK") != NULL)
            {
                voiceOffset = 0U;

                if (Voice_StartNextWrite() == 0U)
                {
                    LCD_Show(
                        "VOICE WRITE",
                        "START ERROR"
                    );

                    state = 99U;
                }
            }
            else if (strstr(snapshot, "ERROR") != NULL)
            {
                LCD_Show(
                    "VOICE CREATE",
                    "ERROR"
                );

                state = 99U;
            }
            else if ((HAL_GetTick() - commandTime) >= 3000U)
            {
                LCD_Show(
                    "VOICE CREATE",
                    "TIMEOUT"
                );

                state = 99U;
            }
        }

        /*
         * State 22:
         * Wait for FSWRITE '>' prompt.
         */
        else if (state == 22U)
        {
            if (strchr(snapshot, '>') != NULL)
            {
                RX_ResetBuffer();

                if (HAL_UART_Transmit(
                        &huart3,
                        (uint8_t *)&sim800_voice_data[voiceOffset],
                        (uint16_t)voiceChunkSize,
                        15000U
                    ) != HAL_OK)
                {
                    LCD_Show(
                        "VOICE DATA",
                        "TX ERROR"
                    );

                    state = 99U;
                }
                else
                {
                    commandTime = HAL_GetTick();
                    state = 23U;
                }
            }
            else if (strstr(snapshot, "ERROR") != NULL)
            {
                LCD_Show(
                    "FSWRITE",
                    "ERROR"
                );

                state = 99U;
            }
            else if ((HAL_GetTick() - commandTime) >= 3000U)
            {
                LCD_Show(
                    "FSWRITE",
                    "NO PROMPT"
                );

                state = 99U;
            }
        }

        /*
         * State 23:
         * Wait for one binary chunk to be committed.
         */
        else if (state == 23U)
        {
            if (strstr(snapshot, "OK") != NULL)
            {
                voiceOffset += voiceChunkSize;

                if (voiceOffset < SIM800_VOICE_DATA_LEN)
                {
                    if (Voice_StartNextWrite() == 0U)
                    {
                        LCD_Show(
                            "VOICE WRITE",
                            "NEXT ERROR"
                        );

                        state = 99U;
                    }
                }
                else
                {
                    RX_ResetBuffer();

                    LCD_Show(
                        "VERIFY VOICE",
                        "FSFLSIZE..."
                    );

                    if (HAL_UART_Transmit(
                            &huart3,
                            (uint8_t *)"AT+FSFLSIZE=C:\\voice.wav\r",
                            sizeof("AT+FSFLSIZE=C:\\voice.wav\r") - 1U,
                            1000U
                        ) != HAL_OK)
                    {
                        LCD_Show(
                            "VOICE SIZE",
                            "TX ERROR"
                        );

                        state = 99U;
                    }
                    else
                    {
                        commandTime = HAL_GetTick();
                        state = 24U;
                    }
                }
            }
            else if (strstr(snapshot, "ERROR") != NULL)
            {
                LCD_Show(
                    "VOICE WRITE",
                    "ERROR"
                );

                state = 99U;
            }
            else if ((HAL_GetTick() - commandTime) >= 12000U)
            {
                LCD_Show(
                    "VOICE WRITE",
                    "TIMEOUT"
                );

                state = 99U;
            }
        }

        /*
         * State 24:
         * Verify uploaded binary file size.
         */
        else if (state == 24U)
        {
            position = strstr(
                snapshot,
                "+FSFLSIZE:"
            );

            if (position != NULL)
            {
                unsigned long fileSize = 0UL;

                if (sscanf(
                        position,
                        "+FSFLSIZE: %lu",
                        &fileSize
                    ) == 1)
                {
                    snprintf(
                        lcdLine2,
                        sizeof(lcdLine2),
                        "%lu bytes",
                        fileSize
                    );

                    if (fileSize ==
                        (unsigned long)SIM800_VOICE_DATA_LEN)
                    {
                        LCD_Show(
                            "SMS URC SETUP",
                            "CNMI..."
                        );

                        RX_ResetBuffer();

                        if (HAL_UART_Transmit(
                                &huart3,
                                (uint8_t *)"AT+CNMI=2,1,0,0,0\r",
                                sizeof("AT+CNMI=2,1,0,0,0\r") - 1U,
                                1000U
                            ) != HAL_OK)
                        {
                            LCD_Show(
                                "CNMI",
                                "TX ERROR"
                            );

                            state = 99U;
                        }
                        else
                        {
                            commandTime = HAL_GetTick();
                            state = 34U;
                        }
                    }
                    else
                    {
                        LCD_Show(
                            "SIZE MISMATCH",
                            lcdLine2
                        );
                    }

                    
                }
            }
            else if (strstr(snapshot, "ERROR") != NULL)
            {
                LCD_Show(
                    "VOICE SIZE",
                    "ERROR"
                );

                state = 99U;
            }
            else if ((HAL_GetTick() - commandTime) >= 3000U)
            {
                LCD_Show(
                    "VOICE SIZE",
                    "TIMEOUT"
                );

                state = 99U;
            }
        }

        else if (state == 12U)
        {
            if (strstr(snapshot, "OK") != NULL)
            {
                LCD_Show(
                    "FILE CREATED",
                    "C:\\test.wav"
                );

                state = 99U;
            }
            else if (strstr(snapshot, "ERROR") != NULL)
            {
                LCD_Show(
                    "FSCREATE",
                    "ERROR"
                );

                state = 99U;
            }
            else if ((HAL_GetTick() - commandTime) >= 3000U)
            {
                LCD_Show(
                    "FSCREATE",
                    "TIMEOUT"
                );

                state = 99U;
            }
        }
        /*
         * State 34:
         * Enable SMS new-message URCs.
         */
        else if (state == 34U)
        {
            if (strstr(snapshot, "OK") != NULL)
            {
                RX_ResetBuffer();

                LCD_Show(
                    "SMS URC READY",
                    "WAITING CALL"
                );

                state = 1U;
            }
            else if (strstr(snapshot, "ERROR") != NULL)
            {
                LCD_Show(
                    "CNMI",
                    "ERROR"
                );

                state = 99U;
            }
            else if ((HAL_GetTick() - commandTime) >= 3000U)
            {
                LCD_Show(
                    "CNMI",
                    "TIMEOUT"
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

                        /* DTMF_PLAYBACK_PATCH_V1 */

                        LCD_Show(
                            "CALL ACTIVE",
                            "ENABLE DTMF"
                        );

                        RX_ResetBuffer();

                        if (HAL_UART_Transmit(
                                &huart3,
                                (uint8_t *)"AT+DDET=1,0,0\r",
                                sizeof("AT+DDET=1,0,0\r") - 1U,
                                1000U
                            ) != HAL_OK)
                        {
                            LCD_Show(
                                "DDET",
                                "TX ERROR"
                            );

                            state = 99U;
                        }
                        else
                        {
                            commandTime = HAL_GetTick();
                            state = 33U;
                        }
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

        /*
         * State 33:
         * Wait for DTMF detector enable confirmation.
         */
        else if (state == 33U)
        {
            if (strstr(snapshot, "OK") != NULL)
            {
                RX_ResetBuffer();

                LCD_Show(
                    "WAIT DTMF",
                    "PRESS 1"
                );

                state = 4U;
            }
            else if (strstr(snapshot, "ERROR") != NULL)
            {
                LCD_Show(
                    "DDET",
                    "ERROR"
                );

                state = 99U;
            }
            else if ((HAL_GetTick() - commandTime) >= 3000U)
            {
                LCD_Show(
                    "DDET",
                    "TIMEOUT"
                );

                state = 99U;
            }
        }


        /*
         * State 30:
         * Wait for DTAM=1 confirmation.
         */
        else if (state == 30U)
        {
            if (strstr(snapshot, "OK") != NULL)
            {
                RX_ResetBuffer();

                LCD_Show(
                    "PLAY VOICE",
                    "CMEDPLAY..."
                );

                if (HAL_UART_Transmit(
                        &huart3,
                        (uint8_t *)"AT+CMEDPLAY=1,C:\\voice.wav,0,100\r",
                        sizeof("AT+CMEDPLAY=1,C:\\voice.wav,0,100\r") - 1U,
                        1000U
                    ) != HAL_OK)
                {
                    LCD_Show(
                        "CMEDPLAY",
                        "TX ERROR"
                    );

                    state = 99U;
                }
                else
                {
                    commandTime = HAL_GetTick();
                    state = 31U;
                }
            }
            else if (strstr(snapshot, "ERROR") != NULL)
            {
                LCD_Show(
                    "DTAM",
                    "ERROR"
                );

                state = 99U;
            }
            else if ((HAL_GetTick() - commandTime) >= 3000U)
            {
                LCD_Show(
                    "DTAM",
                    "TIMEOUT"
                );

                state = 99U;
            }
        }

        /*
         * State 31:
         * Wait for playback command response.
         */
        else if (state == 31U)
        {
            if (strstr(snapshot, "OK") != NULL)
            {
                RX_ResetBuffer();

                LCD_Show(
                    "VOICE PLAYING",
                    "REMOTE"
                );

                commandTime = HAL_GetTick();
                state = 32U;
            }
            else if (strstr(snapshot, "+CME ERROR") != NULL ||
                     strstr(snapshot, "ERROR") != NULL)
            {
                LCD_Show(
                    "CMEDPLAY",
                    "ERROR"
                );

                state = 99U;
            }
            else if ((HAL_GetTick() - commandTime) >= 3000U)
            {
                LCD_Show(
                    "CMEDPLAY",
                    "TIMEOUT"
                );

                state = 99U;
            }
        }

        /*
         * State 32:
         * Wait until playback finishes or call ends.
         */
        else if (state == 32U)
        {
            if (strstr(snapshot, "+CMEDPLAY: 0") != NULL)
            {
                LCD_Show(
                    "PLAY FINISHED",
                    "CALL ACTIVE"
                );

                RX_ResetBuffer();
                state = 4U;
            }
            else if (strstr(snapshot, "NO CARRIER") != NULL)
            {
                LCD_Show(
                    "CALL ENDED",
                    "WAITING CALL"
                );

                RX_ResetBuffer();
                state = 1U;
            }
        }

        else if (state == 4U)
        {
            /*
             * SMS arrival during an active voice call.
             * Example:
             * +CMTI: "SM",3
             */
            position = strstr(
                snapshot,
                "+CMTI:"
            );

            if (position != NULL)
            {
                memset(smsStorage, 0, sizeof(smsStorage));
                smsIndex = -1;

                if (sscanf(
                        position,
                        "+CMTI: \"%7[^\"]\",%d",
                        smsStorage,
                        &smsIndex
                    ) == 2)
                {
                    snprintf(
                        lcdLine2,
                        sizeof(lcdLine2),
                        "%s IDX:%d",
                        smsStorage,
                        smsIndex
                    );

                    LCD_Show(
                        "SMS RECEIVED",
                        lcdLine2
                    );

                    RX_ResetBuffer();
                }
            }

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
                    RX_ResetBuffer();

                    if (dtmfKey == '1')
                    {
                        LCD_Show(
                            "DTMF 1",
                            "PLAY VOICE"
                        );

                        if (HAL_UART_Transmit(
                                &huart3,
                                (uint8_t *)"AT+DTAM=1\r",
                                sizeof("AT+DTAM=1\r") - 1U,
                                1000U
                            ) != HAL_OK)
                        {
                            LCD_Show(
                                "DTAM",
                                "TX ERROR"
                            );

                            state = 99U;
                        }
                        else
                        {
                            commandTime = HAL_GetTick();
                            state = 30U;
                        }
                    }
                    else
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
                    }
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
