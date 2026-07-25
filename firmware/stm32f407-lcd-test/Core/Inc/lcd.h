#ifndef INC_LCD_H_
#define INC_LCD_H_

#include "main.h"

void LCD_Init(void);
void LCD_Clear(void);
void LCD_SetCursor(uint8_t column, uint8_t row);
void LCD_Print(const char *text);

#endif /* INC_LCD_H_ */
