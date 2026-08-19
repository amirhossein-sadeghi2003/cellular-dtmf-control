#include "ui_model.h"

#include <stddef.h>
#include <string.h>

static bool isValidDtmfKey(char key)
{
    if (key >= '0' && key <= '9')
        return true;

    if (key == '*' || key == '#')
        return true;

    if (key >= 'A' && key <= 'D')
        return true;

    return false;
}

void uiModelInit(UiModel *model)
{
    if (!model)
        return;

    memset(model, 0, sizeof(*model));

    model->modem_state = UI_MODEM_INITIALIZING;
    model->network_state = UI_NETWORK_NOT_REGISTERED;
    model->call_state = UI_CALL_IDLE;

    model->signal_rssi = 99;
    model->call_duration_seconds = 0;

    model->auto_answer_enabled = false;
    model->dtmf_detection_enabled = false;

    model->display_brightness_percent = 70;
    model->screen_timeout_seconds = 30;
    model->status_refresh_interval_seconds = 1;

    model->caller[0] = '-';
    model->caller[1] = '\0';

    model->operator_name[0] = '-';
    model->operator_name[1] = '\0';

    model->last_dtmf = '-';
    model->dtmf_buffer[0] = '\0';
    model->uart_ready = false;
    model->sim_ready = false;

    model->modem_reset_count = 0;
    model->at_error_count = 0;

    model->last_error[0] = 'N';
    model->last_error[1] = 'O';
    model->last_error[2] = 'N';
    model->last_error[3] = 'E';
    model->last_error[4] = '\0';
}

bool uiModelAddDtmf(UiModel *model, char key)
{
    size_t length;

    if (!model)
        return false;

    if (!model->dtmf_detection_enabled)
        return false;

    if (!isValidDtmfKey(key))
        return false;

    length = strlen(model->dtmf_buffer);

    if (length >= UI_DTMF_BUFFER_LENGTH - 1) {
        memmove(
            model->dtmf_buffer,
            model->dtmf_buffer + 1,
            UI_DTMF_BUFFER_LENGTH - 2);

        length = UI_DTMF_BUFFER_LENGTH - 2;
    }

    model->dtmf_buffer[length] = key;
    model->dtmf_buffer[length + 1] = '\0';
    model->last_dtmf = key;

    return true;
}

void uiModelClearDtmf(UiModel *model)
{
    if (!model)
        return;

    model->last_dtmf = '-';
    model->dtmf_buffer[0] = '\0';
}