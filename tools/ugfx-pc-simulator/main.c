#include "gfx.h"
#include "ui.h"

#include <string.h>

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

static char normalizeDtmfKey(gU8 key)
{
    if (key >= '0' && key <= '9')
        return (char)key;

    if (key == '*' || key == '#')
        return (char)key;

    if (key >= 'A' && key <= 'D')
        return (char)key;

    if (key >= 'a' && key <= 'd')
        return (char)(key - 'a' + 'A');

    return '\0';
}

static void initializeSimulatorModel(UiModel *model)
{
    uiModelInit(model);

    model->modem_state = UI_MODEM_READY;
    model->network_state = UI_NETWORK_HOME;
    model->call_state = UI_CALL_IDLE;

    model->signal_rssi = 26;
    model->call_duration_seconds = 0;

    model->auto_answer_enabled = true;
    model->dtmf_detection_enabled = true;

    strncpy(
        model->operator_name,
        "DEMO GSM",
        UI_OPERATOR_MAX_LENGTH - 1);

    model->operator_name[UI_OPERATOR_MAX_LENGTH - 1] = '\0';
}

static UiAction processRawKey(UiModel *model, gU8 raw_key)
{
    char dtmf_key;
    UiKey ui_key;

    dtmf_key = normalizeDtmfKey(raw_key);

    if (dtmf_key != '\0') {
        if (uiModelAddDtmf(model, dtmf_key))
            uiRefresh();

        return UI_ACTION_NONE;
    }

    ui_key = mapKeyboardKey(raw_key);

    if (ui_key == UI_KEY_NONE)
        return UI_ACTION_NONE;

    return uiHandleKey(ui_key);
}

int main(void)
{
    GListener listener;
    GSourceHandle keyboard;

    GEvent *event;
    GEventKeyboard *key_event;

    UiModel model;
    UiAction action;

    gU16 i;

    gfxInit();

    initializeSimulatorModel(&model);
    uiInit(&model);

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

        for (i = 0; i < key_event->bytecount; i++) {
            action = processRawKey(
                &model,
                (gU8)key_event->c[i]);

            if (action == UI_ACTION_EXIT)
                return 0;
        }
    }
}