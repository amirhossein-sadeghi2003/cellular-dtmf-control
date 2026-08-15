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

static void copyText(
    char *destination,
    size_t destination_size,
    const char *source)
{
    if (!destination || !destination_size || !source)
        return;

    strncpy(destination, source, destination_size - 1);
    destination[destination_size - 1] = '\0';
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
    model->uart_ready = true;
    model->sim_ready = true;

    copyText(
        model->operator_name,
        sizeof(model->operator_name),
        "DEMO GSM");
}

static void cycleModemState(UiModel *model)
{
    switch (model->modem_state) {
    case UI_MODEM_OFFLINE:
        model->modem_state = UI_MODEM_INITIALIZING;
        break;

    case UI_MODEM_INITIALIZING:
        model->modem_state = UI_MODEM_READY;
        break;

    case UI_MODEM_READY:
        model->modem_state = UI_MODEM_ERROR;
        break;

    case UI_MODEM_ERROR:
    default:
        model->modem_state = UI_MODEM_OFFLINE;
        break;
    }
}

static void cycleCallState(UiModel *model)
{
    switch (model->call_state) {
    case UI_CALL_IDLE:
        model->call_state = UI_CALL_RINGING;
        model->call_duration_seconds = 0;

        copyText(
            model->caller,
            sizeof(model->caller),
            "+989123456789");
        break;

    case UI_CALL_RINGING:
        model->call_state = UI_CALL_ANSWERING;
        break;

    case UI_CALL_ANSWERING:
        model->call_state = UI_CALL_ACTIVE;
        break;

    case UI_CALL_ACTIVE:
        model->call_state = UI_CALL_ENDED;
        break;

    case UI_CALL_ENDED:
    default:
        model->call_state = UI_CALL_IDLE;
        model->call_duration_seconds = 0;

        copyText(
            model->caller,
            sizeof(model->caller),
            "-");
        break;
    }
}

static void cycleNetworkState(UiModel *model)
{
    switch (model->network_state) {
    case UI_NETWORK_NOT_REGISTERED:
        model->network_state = UI_NETWORK_SEARCHING;
        model->signal_rssi = 10;

        copyText(
            model->operator_name,
            sizeof(model->operator_name),
            "-");
        break;

    case UI_NETWORK_SEARCHING:
        model->network_state = UI_NETWORK_HOME;
        model->signal_rssi = 26;

        copyText(
            model->operator_name,
            sizeof(model->operator_name),
            "DEMO GSM");
        break;

    case UI_NETWORK_HOME:
        model->network_state = UI_NETWORK_ROAMING;
        model->signal_rssi = 18;

        copyText(
            model->operator_name,
            sizeof(model->operator_name),
            "DEMO ROAM");
        break;

    case UI_NETWORK_ROAMING:
        model->network_state = UI_NETWORK_DENIED;
        model->signal_rssi = 5;

        copyText(
            model->operator_name,
            sizeof(model->operator_name),
            "-");
        break;

    case UI_NETWORK_DENIED:
    default:
        model->network_state = UI_NETWORK_NOT_REGISTERED;
        model->signal_rssi = 99;

        copyText(
            model->operator_name,
            sizeof(model->operator_name),
            "-");
        break;
    }
}

static void adjustSignal(UiModel *model, int change)
{
    if (model->signal_rssi == 99) {
        model->signal_rssi = change > 0 ? 0 : 31;
        return;
    }

    if (change > 0 && model->signal_rssi < 31)
        model->signal_rssi++;

    if (change < 0 && model->signal_rssi > 0)
        model->signal_rssi--;
}


static void cycleDiagnosticScenario(UiModel *model)
{
    static unsigned int diagnostic_step;

    diagnostic_step++;

    if (diagnostic_step > 4)
        diagnostic_step = 0;

    switch (diagnostic_step) {
    case 1:
        model->uart_ready = true;
        model->sim_ready = true;
        model->modem_state = UI_MODEM_ERROR;
        model->at_error_count++;

        copyText(
            model->last_error,
            sizeof(model->last_error),
            "AT TIMEOUT");
        break;

    case 2:
        model->uart_ready = false;
        model->sim_ready = true;
        model->modem_state = UI_MODEM_ERROR;
        model->at_error_count++;

        copyText(
            model->last_error,
            sizeof(model->last_error),
            "UART ERROR");
        break;

    case 3:
        model->uart_ready = true;
        model->sim_ready = false;
        model->modem_state = UI_MODEM_READY;

        copyText(
            model->last_error,
            sizeof(model->last_error),
            "SIM NOT READY");
        break;

    case 4:
        model->uart_ready = true;
        model->sim_ready = true;
        model->modem_state = UI_MODEM_INITIALIZING;
        model->modem_reset_count++;

        copyText(
            model->last_error,
            sizeof(model->last_error),
            "MODEM RESET");
        break;

    case 0:
    default:
        model->uart_ready = true;
        model->sim_ready = true;
        model->modem_state = UI_MODEM_READY;

        copyText(
            model->last_error,
            sizeof(model->last_error),
            "NONE");
        break;
    }
}



static bool processSimulatorCommand(UiModel *model, gU8 raw_key)
{
    switch (raw_key) {
    case 'm':
    case 'M':
        cycleModemState(model);
        break;

    case 'n':
    case 'N':
        cycleNetworkState(model);
        break;

    case 'r':
    case 'R':
        cycleCallState(model);
        break;

    case 'f':
    case 'F':
        cycleDiagnosticScenario(model);
        break;



    case '+':
        adjustSignal(model, 1);
        break;

    case '-':
        adjustSignal(model, -1);
        break;

    case 'x':
    case 'X':
        uiModelClearDtmf(model);
        break;

    default:
        return false;
    }

    uiRefresh();

    return true;
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

    if (processSimulatorCommand(model, raw_key))
        return UI_ACTION_NONE;

    ui_key = mapKeyboardKey(raw_key);

    if (ui_key == UI_KEY_NONE)
        return UI_ACTION_NONE;

    return uiHandleKey(ui_key);
}
static void updateCallDuration(
    UiModel *model,
    gTicks *last_tick,
    gTicks ticks_per_second)
{
    gTicks now;
    gTicks elapsed_ticks;
    gTicks elapsed_seconds;

    now = gfxSystemTicks();

    if (model->call_state != UI_CALL_ACTIVE) {
        *last_tick = now;
        return;
    }

    elapsed_ticks = now - *last_tick;

    if (!ticks_per_second ||
        elapsed_ticks < ticks_per_second) {
        return;
    }

    elapsed_seconds = elapsed_ticks / ticks_per_second;

    model->call_duration_seconds +=
        (uint32_t)elapsed_seconds;

    *last_tick += elapsed_seconds * ticks_per_second;

    uiRefresh();
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
    gTicks last_duration_tick;
    gTicks ticks_per_second;

    gfxInit();

    initializeSimulatorModel(&model);
    uiInit(&model);
    last_duration_tick = gfxSystemTicks();
    ticks_per_second = gfxMillisecondsToTicks(1000);

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
        event = geventEventWait(&listener, 100);

        updateCallDuration(
            &model,
            &last_duration_tick,
            ticks_per_second);

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