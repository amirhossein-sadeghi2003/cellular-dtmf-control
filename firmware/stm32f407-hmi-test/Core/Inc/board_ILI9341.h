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
#define LCD_SPI_TIMEOUT_MS 20U

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

    (void)g;

    command = (uint8_t)index;

    HAL_GPIO_WritePin(
        LCD_DC_GPIO_Port,
        LCD_DC_Pin,
        GPIO_PIN_RESET);

    if (HAL_SPI_Transmit(
            &hspi2,
            &command,
            1U,
            LCD_SPI_TIMEOUT_MS) != HAL_OK)
    {
        return;
    }
}


static GFXINLINE void write_data(
    GDisplay *g,
    gU16 data)
{
    uint8_t value;

    (void)g;

    value = (uint8_t)data;

    HAL_GPIO_WritePin(
        LCD_DC_GPIO_Port,
        LCD_DC_Pin,
        GPIO_PIN_SET);

    if (HAL_SPI_Transmit(
            &hspi2,
            &value,
            1U,
            LCD_SPI_TIMEOUT_MS) != HAL_OK)
    {
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
