/*
 * sim800_service.h
 *
 *  Created on: Aug 26, 2026
 *      Author: amir
 */

#ifndef INC_SIM800_SERVICE_H_
#define INC_SIM800_SERVICE_H_

#include "stm32f4xx_hal.h"
#include "ui_model.h"

#include <stdbool.h>

bool Sim800Service_Init(
    UART_HandleTypeDef *uart,
    UiModel *model);

bool Sim800Service_Process(void);

#endif /* INC_SIM800_SERVICE_H_ */
