/*
 * display_power.c
 *
 *  Created on: Aug 26, 2026
 *      Author: amir
 */



/*
 * Local power control is used because the upstream uGFX ILI9341
 * control path sends command 0x10 for both sleep and wake.
 * The ILI9341 requires command 0x10 for Sleep In and 0x11 for
 * Sleep Out, followed by the appropriate display off/on command.
 */

#include "display_power.h"
#include "main.h"

#include <stdint.h>

extern SPI_HandleTypeDef hspi2;
#define DISPLAY_SPI_TIMEOUT_MS 20U


static void sendCommand(uint8_t command)
{
    HAL_GPIO_WritePin(
        LCD_CS_GPIO_Port,
        LCD_CS_Pin,
        GPIO_PIN_RESET);

    HAL_GPIO_WritePin(
        LCD_DC_GPIO_Port,
        LCD_DC_Pin,
        GPIO_PIN_RESET);

    (void)HAL_SPI_Transmit(
        &hspi2,
        &command,
        1U,
        DISPLAY_SPI_TIMEOUT_MS);

    HAL_GPIO_WritePin(
        LCD_CS_GPIO_Port,
        LCD_CS_Pin,
        GPIO_PIN_SET);
}


void DisplayPower_Sleep(void)
{
    /*
     * ILI9341 Display OFF.
     */
    sendCommand(0x28U);
    HAL_Delay(20U);

    /*
     * ILI9341 Sleep IN.
     */
    sendCommand(0x10U);
    HAL_Delay(120U);
}


void DisplayPower_Wake(void)
{
    /*
     * ILI9341 Sleep OUT.
     */
    sendCommand(0x11U);
    HAL_Delay(120U);

    /*
     * ILI9341 Display ON.
     */
    sendCommand(0x29U);
    HAL_Delay(20U);
}
