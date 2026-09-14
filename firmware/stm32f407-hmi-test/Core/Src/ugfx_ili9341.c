/*
 * ugfx_ili9341.c
 *
 *  Created on: Aug 26, 2026
 *      Author: amir
 */

#include <stdint.h>

volatile uint32_t lcd_spi_error_count = 0U;
volatile uint32_t lcd_spi_last_status = 0U;
volatile uint32_t lcd_spi_last_error_code = 0U;
volatile uint32_t lcd_spi_last_operation = 0U;

#include "drivers/gdisp/ILI9341/gdisp_lld_ILI9341.c"
