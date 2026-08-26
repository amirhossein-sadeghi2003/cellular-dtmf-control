/*
 * ugfx_port.c
 *
 *  Created on: Aug 26, 2026
 *      Author: amir
 */


#include "main.h"
#include "gfx.h"

systemticks_t gfxSystemTicks(void)
{
    return (systemticks_t)HAL_GetTick();
}

systemticks_t gfxMillisecondsToTicks(delaytime_t milliseconds)
{
    return (systemticks_t)milliseconds;
}
