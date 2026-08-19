#ifndef UI_MODEL_H
#define UI_MODEL_H

#include <stdbool.h>
#include <stdint.h>

#define UI_CALLER_MAX_LENGTH 24
#define UI_OPERATOR_MAX_LENGTH 20
#define UI_DTMF_BUFFER_LENGTH 17
#define UI_ERROR_MAX_LENGTH 24

typedef enum {
    UI_MODEM_OFFLINE = 0,
    UI_MODEM_INITIALIZING,
    UI_MODEM_READY,
    UI_MODEM_ERROR
} UiModemState;

typedef enum {
    UI_NETWORK_NOT_REGISTERED = 0,
    UI_NETWORK_SEARCHING,
    UI_NETWORK_HOME,
    UI_NETWORK_ROAMING,
    UI_NETWORK_DENIED
} UiNetworkState;

typedef enum {
    UI_CALL_IDLE = 0,
    UI_CALL_RINGING,
    UI_CALL_ANSWERING,
    UI_CALL_ACTIVE,
    UI_CALL_ENDED
} UiCallState;

typedef struct {
    UiModemState modem_state;
    UiNetworkState network_state;
    UiCallState call_state;

    uint8_t signal_rssi;
    uint32_t call_duration_seconds;

    bool auto_answer_enabled;
    bool dtmf_detection_enabled;
    uint8_t display_brightness_percent;
    uint16_t screen_timeout_seconds;
    uint8_t status_refresh_interval_seconds;

    char caller[UI_CALLER_MAX_LENGTH];
    char operator_name[UI_OPERATOR_MAX_LENGTH];

    char last_dtmf;
    char dtmf_buffer[UI_DTMF_BUFFER_LENGTH];

    bool uart_ready;
    bool sim_ready;

    uint32_t modem_reset_count;
    uint32_t at_error_count;

    char last_error[UI_ERROR_MAX_LENGTH];
} UiModel;

void uiModelInit(UiModel *model);
bool uiModelAddDtmf(UiModel *model, char key);
void uiModelClearDtmf(UiModel *model);

#endif