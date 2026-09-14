/*
 * board_ILI9341.h
 *
 *  Created on: Aug 26, 2026
 *      Author: amir
 */

#ifndef INC_BOARD_ILI9341_H_
#define INC_BOARD_ILI9341_H_

#include "main.h"


extern SPI_HandleTypeDef hspi2;

extern volatile uint32_t lcd_spi_error_count;
extern volatile uint32_t lcd_spi_last_status;
extern volatile uint32_t lcd_spi_last_error_code;
extern volatile uint32_t lcd_spi_last_operation;

#define LCD_SPI_TIMEOUT_MS 20U

#define LCD_SPI_OPERATION_COMMAND 1U
#define LCD_SPI_OPERATION_DATA    2U

static GFXINLINE void init_board(GDisplay *g){
	g->board = 0;

    HAL_GPIO_WritePin(
        LCD_CS_GPIO_Port,
        LCD_CS_Pin,
        GPIO_PIN_SET);

    HAL_GPIO_WritePin(
        LCD_DC_GPIO_Port,
        LCD_DC_Pin,
        GPIO_PIN_SET);

    HAL_GPIO_WritePin(
        LCD_RST_GPIO_Port,
        LCD_RST_Pin,
        GPIO_PIN_SET);

}


static GFXINLINE void setpin_reset(GDisplay *g, gBool state){
	(void)g;
	if (state){
		HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_RESET);
	}
	else{
		HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);
	}
}



static GFXINLINE void acquire_bus(GDisplay *g)
{
    (void)g;

    HAL_GPIO_WritePin(
        LCD_CS_GPIO_Port,
        LCD_CS_Pin,
        GPIO_PIN_RESET);
}

static GFXINLINE void release_bus(GDisplay *g)
{
    (void)g;

    HAL_GPIO_WritePin(
        LCD_CS_GPIO_Port,
        LCD_CS_Pin,
        GPIO_PIN_SET);
}



static GFXINLINE void write_index(
    GDisplay *g,
    gU16 index)
{
    uint8_t command;
    HAL_StatusTypeDef status;

    (void)g;

    command = (uint8_t)index;

    HAL_GPIO_WritePin(
        LCD_DC_GPIO_Port,
        LCD_DC_Pin,
        GPIO_PIN_RESET);

    status = HAL_SPI_Transmit(
        &hspi2,
        &command,
        1U,
        LCD_SPI_TIMEOUT_MS);

    if (status != HAL_OK)
    {
        lcd_spi_error_count++;
        lcd_spi_last_status = (uint32_t)status;
        lcd_spi_last_error_code = hspi2.ErrorCode;
        lcd_spi_last_operation =
            LCD_SPI_OPERATION_COMMAND;

        return;
    }
}


static GFXINLINE void write_data(
    GDisplay *g,
    gU16 data)
{
    uint8_t value;
    HAL_StatusTypeDef status;

    (void)g;

    value = (uint8_t)data;

    HAL_GPIO_WritePin(
        LCD_DC_GPIO_Port,
        LCD_DC_Pin,
        GPIO_PIN_SET);

    status = HAL_SPI_Transmit(
        &hspi2,
        &value,
        1U,
        LCD_SPI_TIMEOUT_MS);

    if (status != HAL_OK)
    {
        lcd_spi_error_count++;
        lcd_spi_last_status = (uint32_t)status;
        lcd_spi_last_error_code = hspi2.ErrorCode;
        lcd_spi_last_operation =
            LCD_SPI_OPERATION_DATA;

        return;
    }
}



static GFXINLINE void setreadmode(GDisplay *g)
{
    (void)g;
}

static GFXINLINE void setwritemode(GDisplay *g)
{
    (void)g;
}

static GFXINLINE gU16 read_data(GDisplay *g)
{
    (void)g;

    return 0U;
}


static GFXINLINE void post_init_board(GDisplay *g)
{
    (void)g;
}

static GFXINLINE void set_backlight(
    GDisplay *g,
    gU8 percent)
{
    (void)g;
    (void)percent;
}



#endif /* INC_BOARD_ILI9341_H_ */
