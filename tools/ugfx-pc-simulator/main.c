#include "gfx.h"
#include "ui.h"

static UiKey mapKeyboardKey(gU8 key)
{
    switch (key) {
    case GKEY_UP:
        return UI_KEY_UP;

    case GKEY_DOWN:
        return UI_KEY_DOWN;

    case GKEY_LEFT:
        return UI_KEY_LEFT;

    case GKEY_RIGHT:
        return UI_KEY_RIGHT;

    case GKEY_ENTER:
        return UI_KEY_ENTER;

    case GKEY_ESC:
        return UI_KEY_BACK;

    default:
        return UI_KEY_NONE;
    }
}

int main(void)
{
    GListener listener;
    GSourceHandle keyboard;

    GEvent *event;
    GEventKeyboard *key_event;

    UiKey ui_key;
    UiAction action;

    gfxInit();
    uiInit();

    keyboard = ginputGetKeyboard(0);

    if (!keyboard) {
        uiShowError("KEYBOARD ERROR");

        while (1)
            gfxSleepMilliseconds(1000);
    }

    geventListenerInit(&listener);

    if (!geventAttachSource(
            &listener,
            keyboard,
            GLISTEN_KEYREPEATSOFF)) {

        uiShowError("EVENT ERROR");

        while (1)
            gfxSleepMilliseconds(1000);
    }

    while (1) {
        event = geventEventWait(&listener, gDelayForever);

        if (!event)
            continue;

        if (event->type != GEVENT_KEYBOARD)
            continue;

        key_event = (GEventKeyboard *)event;

        if ((key_event->keystate & GKEYSTATE_KEYUP) ||
            !key_event->bytecount) {
            continue;
        }

        ui_key = mapKeyboardKey((gU8)key_event->c[0]);

        if (ui_key == UI_KEY_NONE)
            continue;

        action = uiHandleKey(ui_key);

        if (action == UI_ACTION_EXIT)
            return 0;
    }
}