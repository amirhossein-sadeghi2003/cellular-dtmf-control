/*
 * keypad.c
 *
 *  Created on: Aug 26, 2026
 *      Author: amir
 */


#include "keypad.h"
#include "main.h"

#include <stdbool.h>
#include <stdint.h>

#define KEY_DEBOUNCE_SAMPLES 3U

typedef struct {
    uint8_t last_raw_state;
    uint8_t stable_state;
    uint8_t stable_count;
    uint8_t previous_stable_state;
} KeyState_t;

static KeyState_t key_up;
static KeyState_t key_down;
static KeyState_t key_left;
static KeyState_t key_right;
static KeyState_t key_enter;


static uint8_t readDebounced(
    KeyState_t *state,
    GPIO_TypeDef *gpio_port,
    uint16_t gpio_pin)
{
    uint8_t raw_state;

    raw_state =
        HAL_GPIO_ReadPin(gpio_port, gpio_pin) ==
        GPIO_PIN_RESET;

    if (raw_state == state->last_raw_state) {
        if (state->stable_count < KEY_DEBOUNCE_SAMPLES)
            state->stable_count++;
    } else {
        state->last_raw_state = raw_state;
        state->stable_count = 1U;
    }

    if (state->stable_count >= KEY_DEBOUNCE_SAMPLES)
        state->stable_state = raw_state;

    return state->stable_state;
}


static bool pressedEdge(
    KeyState_t *state,
    GPIO_TypeDef *gpio_port,
    uint16_t gpio_pin)
{
    uint8_t current_state;
    bool pressed;

    current_state = readDebounced(
        state,
        gpio_port,
        gpio_pin);

    pressed =
        current_state != 0U &&
        state->previous_stable_state == 0U;

    state->previous_stable_state = current_state;

    return pressed;
}


void Keypad_Init(void)
{
    key_up = (KeyState_t){0};
    key_down = (KeyState_t){0};
    key_left = (KeyState_t){0};
    key_right = (KeyState_t){0};
    key_enter = (KeyState_t){0};
}


KeypadEvent_t Keypad_Poll(void)
{
    bool up_pressed;
    bool down_pressed;
    bool left_pressed;
    bool right_pressed;
    bool enter_pressed;

    /*
     * Read every key on every pass so that release states
     * are updated even if another key generates an event.
     */
    up_pressed = pressedEdge(
        &key_up,
        KEY_UP_GPIO_Port,
        KEY_UP_Pin);

    down_pressed = pressedEdge(
        &key_down,
        KEY_DOWN_GPIO_Port,
        KEY_DOWN_Pin);

    left_pressed = pressedEdge(
        &key_left,
        KEY_LEFT_GPIO_Port,
        KEY_LEFT_Pin);

    right_pressed = pressedEdge(
        &key_right,
        KEY_RIGHT_GPIO_Port,
        KEY_RIGHT_Pin);

    enter_pressed = pressedEdge(
        &key_enter,
        KEY_OK_GPIO_Port,
        KEY_OK_Pin);

    if (up_pressed)
        return KEYPAD_EVENT_UP;

    if (down_pressed)
        return KEYPAD_EVENT_DOWN;

    if (left_pressed)
        return KEYPAD_EVENT_LEFT;

    if (right_pressed)
        return KEYPAD_EVENT_RIGHT;

    if (enter_pressed)
        return KEYPAD_EVENT_ENTER;

    return KEYPAD_EVENT_NONE;
}
