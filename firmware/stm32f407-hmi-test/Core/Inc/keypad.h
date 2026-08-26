/*
 * keypad.h
 *
 *  Created on: Aug 26, 2026
 *      Author: amir
 */

#ifndef INC_KEYPAD_H_
#define INC_KEYPAD_H_

typedef enum {
    KEYPAD_EVENT_NONE = 0,
    KEYPAD_EVENT_UP,
    KEYPAD_EVENT_DOWN,
    KEYPAD_EVENT_LEFT,
    KEYPAD_EVENT_RIGHT,
    KEYPAD_EVENT_ENTER
} KeypadEvent_t;

void Keypad_Init(void);
KeypadEvent_t Keypad_Poll(void);

#endif /* INC_KEYPAD_H_ */
