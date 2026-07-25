#include "lcd.h"

static void LCD_PulseEnable(void)
{
    HAL_GPIO_WritePin(LCD_EN_GPIO_Port, LCD_EN_Pin, GPIO_PIN_SET);
    HAL_Delay(1);

    HAL_GPIO_WritePin(LCD_EN_GPIO_Port, LCD_EN_Pin, GPIO_PIN_RESET);
    HAL_Delay(1);
}

static void LCD_Write4Bits(uint8_t data)
{
    HAL_GPIO_WritePin(
        LCD_D4_GPIO_Port,
        LCD_D4_Pin,
        (data & 0x01U) ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        LCD_D5_GPIO_Port,
        LCD_D5_Pin,
        (data & 0x02U) ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        LCD_D6_GPIO_Port,
        LCD_D6_Pin,
        (data & 0x04U) ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        LCD_D7_GPIO_Port,
        LCD_D7_Pin,
        (data & 0x08U) ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    LCD_PulseEnable();
}

static void LCD_Send(uint8_t value, GPIO_PinState registerSelect)
{
    HAL_GPIO_WritePin(LCD_RS_GPIO_Port, LCD_RS_Pin, registerSelect);


    HAL_GPIO_WritePin(LCD_RW_GPIO_Port, LCD_RW_Pin, GPIO_PIN_RESET);

    LCD_Write4Bits((value >> 4) & 0x0FU);
    LCD_Write4Bits(value & 0x0FU);
}

static void LCD_Command(uint8_t command)
{
    LCD_Send(command, GPIO_PIN_RESET);
    HAL_Delay(2);
}

static void LCD_Data(uint8_t data)
{
    LCD_Send(data, GPIO_PIN_SET);
    HAL_Delay(1);
}

void LCD_Init(void)
{
    HAL_Delay(50);

    HAL_GPIO_WritePin(LCD_RS_GPIO_Port, LCD_RS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_RW_GPIO_Port, LCD_RW_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_EN_GPIO_Port, LCD_EN_Pin, GPIO_PIN_RESET);


    LCD_Write4Bits(0x03);
    HAL_Delay(5);

    LCD_Write4Bits(0x03);
    HAL_Delay(1);

    LCD_Write4Bits(0x03);
    HAL_Delay(1);

    LCD_Write4Bits(0x02);
    HAL_Delay(1);

    LCD_Command(0x28); /* 4-bit, 2-line, 5x8 font */
    LCD_Command(0x0C); /* Display on, cursor off */
    LCD_Command(0x06); /* Cursor moves right */
    LCD_Clear();
}

void LCD_Clear(void)
{
    LCD_Command(0x01);
    HAL_Delay(2);
}

void LCD_SetCursor(uint8_t column, uint8_t row)
{
    static const uint8_t rowOffsets[] = {
        0x00,
        0x40,
        0x14,
        0x54
    };

    if (row > 3U)
    {
        row = 0U;
    }

    LCD_Command(0x80U | (rowOffsets[row] + column));
}

void LCD_Print(const char *text)
{
    if (text == NULL)
    {
        return;
    }

    while (*text != '\0')
    {
        LCD_Data((uint8_t)*text);
        text++;
    }
}
