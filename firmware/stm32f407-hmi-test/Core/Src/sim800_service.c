/*
 * sim800_service.c
 *
 *  Created on: Aug 26, 2026
 *      Author: amir
 */


#include "sim800_service.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define SIM800_RX_BUFFER_SIZE    512U
#define SIM800_BOOT_DELAY_MS     3000U
#define SIM800_AT_TIMEOUT_MS     2000U
#define SIM800_AT_MAX_ATTEMPTS   3U
#define SIM800_RETRY_INTERVAL_MS 5000U
#define SIM800_CPIN_TIMEOUT_MS     2000U
#define SIM800_CPIN_MAX_ATTEMPTS   3U
#define SIM800_CREG_TIMEOUT_MS   5000U
#define SIM800_CREG_RETRY_MS     5000U
#define SIM800_CSQ_TIMEOUT_MS    2000U
#define SIM800_HEALTH_CHECK_MS   10000U
#define SIM800_ANSWER_TIMEOUT_MS 3000U
#define SIM800_DDET_TIMEOUT_MS   2000U

typedef enum {
    SIM800_STATE_NOT_INITIALIZED = 0,
    SIM800_STATE_WAIT_BOOT,
    SIM800_STATE_WAIT_AT,
    SIM800_STATE_WAIT_CPIN,
    SIM800_STATE_WAIT_CREG,
    SIM800_STATE_WAIT_CSQ,
    SIM800_STATE_NETWORK_RETRY,
    SIM800_STATE_READY,
	SIM800_STATE_WAIT_ANSWER,
	SIM800_STATE_WAIT_DDET,
    SIM800_STATE_ERROR,
    SIM800_STATE_SIM_ERROR
} Sim800State_t;

static UART_HandleTypeDef *sim800_uart;
static UiModel *sim800_model;

static uint8_t rx_byte;
static volatile uint16_t rx_index;
static volatile uint8_t rx_error_pending;

static char rx_buffer[SIM800_RX_BUFFER_SIZE];

static Sim800State_t sim800_state;
static uint32_t state_started_tick;
static uint8_t at_attempt_count;
static uint8_t cpin_attempt_count;

static void setLastError(const char *message)
{
    if (!sim800_model || !message)
        return;

    strncpy(
        sim800_model->last_error,
        message,
        sizeof(sim800_model->last_error) - 1U);

    sim800_model->last_error[
        sizeof(sim800_model->last_error) - 1U] = '\0';
}


static void clearRxBuffer(void)
{
    __disable_irq();

    rx_index = 0U;
    memset(rx_buffer, 0, sizeof(rx_buffer));

    __enable_irq();
}


static void getRxSnapshot(
    char *destination,
    size_t destination_size)
{
    uint16_t count;

    if (!destination || destination_size == 0U)
        return;

    __disable_irq();

    count = rx_index;

    if (count >= destination_size)
        count = (uint16_t)(destination_size - 1U);

    memcpy(destination, rx_buffer, count);
    destination[count] = '\0';

    __enable_irq();
}


static bool sendAnswerCommand(void)
{
    static const uint8_t command[] = {
        'A', 'T', 'A', '\r'
    };

    clearRxBuffer();

    if (HAL_UART_Transmit(
            sim800_uart,
            (uint8_t *)command,
            sizeof(command),
            1000U) != HAL_OK) {

        setLastError("ATA TX ERROR");
        return false;
    }

    sim800_model->call_state = UI_CALL_ANSWERING;
    sim800_state = SIM800_STATE_WAIT_ANSWER;
    state_started_tick = HAL_GetTick();

    return true;
}


static bool sendDdetCommand(void)
{
    static const uint8_t command[] = {
        'A', 'T', '+', 'D', 'D', 'E', 'T', '=', '1', '\r'
    };

    clearRxBuffer();

    if (HAL_UART_Transmit(
            sim800_uart,
            (uint8_t *)command,
            sizeof(command),
            1000U) != HAL_OK) {

        setLastError("DDET TX ERROR");
        return false;
    }

    sim800_state = SIM800_STATE_WAIT_DDET;
    state_started_tick = HAL_GetTick();

    return true;
}


static bool sendAtCommand(void)
{
    static const uint8_t command[] = {
        'A',
        'T',
        '\r'
    };

    clearRxBuffer();

    if (HAL_UART_Transmit(
            sim800_uart,
            (uint8_t *)command,
            sizeof(command),
            1000U) != HAL_OK) {

        sim800_state = SIM800_STATE_ERROR;
        sim800_model->modem_state = UI_MODEM_ERROR;
        sim800_model->at_error_count++;
        setLastError("AT TX ERROR");

        return false;
    }

    at_attempt_count++;
    state_started_tick = HAL_GetTick();
    sim800_state = SIM800_STATE_WAIT_AT;

    return true;
}



static bool sendCpinCommand(void)
{
    static const uint8_t command[] = {
        'A', 'T', '+', 'C', 'P', 'I', 'N', '?', '\r'
    };

    clearRxBuffer();

    if (HAL_UART_Transmit(
            sim800_uart,
            (uint8_t *)command,
            sizeof(command),
            1000U) != HAL_OK) {

        sim800_state = SIM800_STATE_ERROR;
        sim800_model->modem_state = UI_MODEM_ERROR;
        sim800_model->sim_ready = false;
        sim800_model->at_error_count++;
        setLastError("CPIN TX ERROR");

        return false;
    }

    cpin_attempt_count++;
    state_started_tick = HAL_GetTick();
    sim800_state = SIM800_STATE_WAIT_CPIN;

    return true;
}





static bool sendCregCommand(void)
{
	static const uint8_t command[] = {
	    'A', 'T', '+', 'C', 'R', 'E', 'G', '?', '\r', '\n'
	};

    clearRxBuffer();

    if (HAL_UART_Transmit(
            sim800_uart,
            (uint8_t *)command,
            sizeof(command),
            1000U) != HAL_OK) {

        sim800_state = SIM800_STATE_ERROR;
        sim800_model->modem_state = UI_MODEM_ERROR;
        sim800_model->network_state =
            UI_NETWORK_NOT_REGISTERED;
        sim800_model->at_error_count++;
        setLastError("CREG TX ERROR");

        return false;
    }

    state_started_tick = HAL_GetTick();
    sim800_state = SIM800_STATE_WAIT_CREG;

    return true;
}



static bool sendCsqCommand(void)
{
    static const uint8_t command[] = {
        'A', 'T', '+', 'C', 'S', 'Q', '\r', '\n'
    };

    clearRxBuffer();

    if (HAL_UART_Transmit(
            sim800_uart,
            (uint8_t *)command,
            sizeof(command),
            1000U) != HAL_OK) {

        sim800_state = SIM800_STATE_ERROR;
        sim800_model->modem_state = UI_MODEM_ERROR;
        sim800_model->signal_rssi = 99U;
        sim800_model->at_error_count++;
        setLastError("CSQ TX ERROR");

        return false;
    }

    state_started_tick = HAL_GetTick();
    sim800_state = SIM800_STATE_WAIT_CSQ;

    return true;
}


static bool parseCsqResponse(
    const char *response,
    uint8_t *rssi)
{
    const char *position;
    unsigned int value;

    if (!response || !rssi)
        return false;

    position = strstr(response, "+CSQ:");

    if (!position)
        return false;

    position += 5;

    while (*position == ' ')
        position++;

    if (*position < '0' || *position > '9')
        return false;

    value = 0U;

    while (*position >= '0' &&
           *position <= '9') {

        value =
            (value * 10U) +
            (unsigned int)(*position - '0');

        position++;
    }

    /*
     * SIM800 CSQ RSSI is normally 0..31.
     * 99 means unknown / not detectable.
     */
    if (value > 31U && value != 99U)
        return false;

    *rssi = (uint8_t)value;

    return true;
}




static void markAtFailure(const char *message)
{
	sim800_state = SIM800_STATE_ERROR;
	    state_started_tick = HAL_GetTick();

	    sim800_model->modem_state = UI_MODEM_ERROR;
	    sim800_model->at_error_count++;

	    setLastError(message);
}

static void markCpinFailure(const char *message)
{
    sim800_state = SIM800_STATE_SIM_ERROR;
    state_started_tick = HAL_GetTick();

    sim800_model->sim_ready = false;
    setLastError(message);
}


bool Sim800Service_Init(
    UART_HandleTypeDef *uart,
    UiModel *model)
{
    if (!uart || !model)
        return false;

    sim800_uart = uart;
    sim800_model = model;

    rx_byte = 0U;
    rx_index = 0U;
    rx_error_pending = 0U;
    at_attempt_count = 0U;
    cpin_attempt_count = 0U;

    memset(rx_buffer, 0, sizeof(rx_buffer));

    sim800_model->modem_state =
        UI_MODEM_INITIALIZING;

    sim800_model->uart_ready = false;
    sim800_model->sim_ready = false;

    sim800_model->network_state =
        UI_NETWORK_NOT_REGISTERED;

    if (HAL_UART_Receive_IT(
            sim800_uart,
            &rx_byte,
            1U) != HAL_OK) {

        sim800_state = SIM800_STATE_ERROR;
        sim800_model->modem_state = UI_MODEM_ERROR;
        sim800_model->at_error_count++;
        setLastError("RX IRQ ERROR");

        return false;
    }

    sim800_model->uart_ready = true;
    sim800_state = SIM800_STATE_WAIT_BOOT;
    state_started_tick = HAL_GetTick();

    return true;
}


static bool parseDtmfEvent(
    const char *response,
    char *key)
{
    const char *position;
    char value;

    if (!response || !key)
        return false;

    position = strstr(response, "+DTMF:");

    if (!position)
        return false;

    position += 6;

    while (*position == ' ')
        position++;

    value = *position;

    if ((value >= '0' && value <= '9') ||
        value == '*' ||
        value == '#' ||
        (value >= 'A' && value <= 'D')) {

        *key = value;
        return true;
    }

    return false;
}


bool Sim800Service_Process(void)
{
	char snapshot[SIM800_RX_BUFFER_SIZE];
	uint8_t parsed_rssi;
	char dtmf_key;
	bool ui_changed;

    if (!sim800_uart || !sim800_model)
        return false;

    ui_changed = false;

    /*
     * Move UART errors out of the interrupt context
     * and process them safely in the main loop.
     */
    if (rx_error_pending != 0U) {
        __disable_irq();
        rx_error_pending = 0U;
        __enable_irq();

        sim800_model->at_error_count++;
        setLastError("UART RX ERROR");

        ui_changed = true;
    }

    switch (sim800_state) {
    case SIM800_STATE_WAIT_BOOT:
        /*
         * Give the modem enough time to complete
         * its initial startup sequence.
         */
        if ((HAL_GetTick() - state_started_tick) >=
            SIM800_BOOT_DELAY_MS) {

            if (!sendAtCommand()) {
                state_started_tick = HAL_GetTick();
                return true;
            }
        }
        break;

    case SIM800_STATE_WAIT_AT:
        getRxSnapshot(
            snapshot,
            sizeof(snapshot));

        /*
         * A successful AT response confirms that
         * the modem and UART connection are alive.
         * Continue by checking the SIM card.
         */
        if (strstr(snapshot, "OK") != NULL) {
            sim800_model->modem_state = UI_MODEM_READY;
            sim800_model->sim_ready = false;

            cpin_attempt_count = 0U;
            setLastError("CHECKING SIM");
            clearRxBuffer();

            if (!sendCpinCommand()) {
                state_started_tick = HAL_GetTick();
            }

            return true;
        }

        /*
         * The modem explicitly rejected the AT command.
         */
        if (strstr(snapshot, "ERROR") != NULL) {
            if (at_attempt_count <
                SIM800_AT_MAX_ATTEMPTS) {

                sim800_model->at_error_count++;

                if (!sendAtCommand()) {
                    state_started_tick = HAL_GetTick();
                    return true;
                }
            } else {
                markAtFailure("AT ERROR");
                return true;
            }
        }

        /*
         * No complete response was received before
         * the command timeout.
         */
        else if (
            (HAL_GetTick() - state_started_tick) >=
            SIM800_AT_TIMEOUT_MS) {

            if (at_attempt_count <
                SIM800_AT_MAX_ATTEMPTS) {

                sim800_model->at_error_count++;

                if (!sendAtCommand()) {
                    state_started_tick = HAL_GetTick();
                    return true;
                }
            } else {
                markAtFailure("AT TIMEOUT");
                return true;
            }
        }
        break;






    case SIM800_STATE_WAIT_CPIN:
        getRxSnapshot(
            snapshot,
            sizeof(snapshot));

        /*
         * The SIM card is available and does not
         * require a PIN.
         */
        if (strstr(snapshot, "+CPIN: READY") != NULL) {
            sim800_model->sim_ready = true;
            sim800_model->network_state =
                UI_NETWORK_SEARCHING;

            setLastError("CHECKING NETWORK");
            clearRxBuffer();

            sim800_state = SIM800_STATE_NETWORK_RETRY;
            state_started_tick = HAL_GetTick();

            return true;
        }

        /*
         * The modem responded, but the SIM card
         * is not currently ready.
         */
        if ((strstr(snapshot, "+CPIN:") != NULL) ||
            (strstr(snapshot, "ERROR") != NULL)) {

            if (cpin_attempt_count <
                SIM800_CPIN_MAX_ATTEMPTS) {

                sim800_model->at_error_count++;
                (void)sendCpinCommand();
            } else {
                markCpinFailure("SIM NOT READY");
            }

            return true;
        }

        /*
         * No CPIN response was received in time.
         */
        if ((HAL_GetTick() - state_started_tick) >=
            SIM800_CPIN_TIMEOUT_MS) {

            if (cpin_attempt_count <
                SIM800_CPIN_MAX_ATTEMPTS) {

                sim800_model->at_error_count++;
                (void)sendCpinCommand();
            } else {
                markCpinFailure("CPIN TIMEOUT");
            }

            return true;
        }
        break;



    case SIM800_STATE_WAIT_CREG:
        getRxSnapshot(
            snapshot,
            sizeof(snapshot));

        if ((strstr(snapshot, "+CREG: 0,1") != NULL) ||
            (strstr(snapshot, "+CREG: 1") != NULL)) {

            sim800_model->network_state =
                UI_NETWORK_HOME;

            sim800_model->signal_rssi = 99U;

            setLastError("CHECKING SIGNAL");
            clearRxBuffer();

            if (!sendCsqCommand()) {
                state_started_tick = HAL_GetTick();
            }

            return true;
        }

        if ((strstr(snapshot, "+CREG: 0,5") != NULL) ||
            (strstr(snapshot, "+CREG: 5") != NULL)) {

            sim800_model->network_state =
                UI_NETWORK_ROAMING;

            sim800_model->signal_rssi = 99U;

            setLastError("CHECKING SIGNAL");
            clearRxBuffer();

            if (!sendCsqCommand()) {
                state_started_tick = HAL_GetTick();
            }

            return true;
        }
        if ((strstr(snapshot, "+CREG: 0,3") != NULL) ||
            (strstr(snapshot, "+CREG: 3") != NULL)) {

            sim800_model->network_state = UI_NETWORK_DENIED;
            sim800_state = SIM800_STATE_NETWORK_RETRY;
            state_started_tick = HAL_GetTick();
            setLastError("NETWORK DENIED");
            clearRxBuffer();

            return true;
        }

        if (strstr(snapshot, "+CREG:") != NULL) {
            if ((strstr(snapshot, "0,2") != NULL) ||
                (strstr(snapshot, ": 2") != NULL)) {

                sim800_model->network_state =
                    UI_NETWORK_SEARCHING;
                setLastError("NETWORK SEARCHING");
            } else {
                sim800_model->network_state =
                    UI_NETWORK_NOT_REGISTERED;
                setLastError("NOT REGISTERED");
            }

            sim800_state = SIM800_STATE_NETWORK_RETRY;
            state_started_tick = HAL_GetTick();
            clearRxBuffer();

            return true;
        }

        if (strstr(snapshot, "ERROR") != NULL) {
            sim800_model->network_state =
                UI_NETWORK_NOT_REGISTERED;
            sim800_model->at_error_count++;
            sim800_state = SIM800_STATE_NETWORK_RETRY;
            state_started_tick = HAL_GetTick();
            setLastError("CREG ERROR");
            clearRxBuffer();

            return true;
        }

        if ((HAL_GetTick() - state_started_tick) >=
            SIM800_CREG_TIMEOUT_MS) {

            sim800_model->modem_state =
                UI_MODEM_ERROR;

            sim800_model->sim_ready = false;

            sim800_model->network_state =
                UI_NETWORK_NOT_REGISTERED;

            sim800_model->call_state =
                UI_CALL_IDLE;

            sim800_model->at_error_count++;

            sim800_state = SIM800_STATE_ERROR;
            state_started_tick = HAL_GetTick();

            setLastError("MODEM NO RESPONSE");
            clearRxBuffer();

            return true;
        }
        break;



    case SIM800_STATE_WAIT_CSQ:
        getRxSnapshot(
            snapshot,
            sizeof(snapshot));

        /*
         * Expected response:
         *
         * +CSQ: <rssi>,<ber>
         *
         * RSSI:
         * 0..31 = valid signal level
         * 99    = unknown
         */
        if (parseCsqResponse(
                snapshot,
                &parsed_rssi)) {

            sim800_model->signal_rssi =
                parsed_rssi;

            sim800_state = SIM800_STATE_READY;
            state_started_tick = HAL_GetTick();

            setLastError("NONE");
            clearRxBuffer();

            return true;
        }

        /*
         * CSQ is useful diagnostic information,
         * but a CSQ error should not prevent the
         * modem from entering READY state.
         */
        if (strstr(snapshot, "ERROR") != NULL) {

            sim800_model->signal_rssi = 99U;

            sim800_state = SIM800_STATE_READY;
            state_started_tick = HAL_GetTick();

            setLastError("CSQ ERROR");
            clearRxBuffer();

            return true;
        }

        if ((HAL_GetTick() - state_started_tick) >=
            SIM800_CSQ_TIMEOUT_MS) {

            sim800_model->signal_rssi = 99U;

            sim800_state = SIM800_STATE_READY;
            state_started_tick = HAL_GetTick();

            setLastError("CSQ TIMEOUT");
            clearRxBuffer();

            return true;
        }

        break;

    case SIM800_STATE_NETWORK_RETRY:
        if ((HAL_GetTick() - state_started_tick) >=
            SIM800_CREG_RETRY_MS) {

            setLastError("RETRYING CREG");
            (void)sendCregCommand();

            return true;
        }
        break;

    case SIM800_STATE_WAIT_ANSWER:
        getRxSnapshot(
            snapshot,
            sizeof(snapshot));

        if (strstr(snapshot, "OK") != NULL) {

            sim800_model->call_state =
                UI_CALL_ACTIVE;

            clearRxBuffer();

            if (!sendDdetCommand()) {
                sim800_model->dtmf_detection_enabled = false;
                sim800_state = SIM800_STATE_READY;
                state_started_tick = HAL_GetTick();
            }

            return true;
        }

        if ((strstr(snapshot, "ERROR") != NULL) ||
            (strstr(snapshot, "NO CARRIER") != NULL) ||
            ((HAL_GetTick() - state_started_tick) >=
             SIM800_ANSWER_TIMEOUT_MS)) {

            sim800_model->call_state =
                UI_CALL_IDLE;

            sim800_model->dtmf_detection_enabled =
                false;

            sim800_state = SIM800_STATE_READY;
            state_started_tick = HAL_GetTick();

            setLastError("ANSWER FAILED");
            clearRxBuffer();

            return true;
        }

        break;


    case SIM800_STATE_WAIT_DDET:
        getRxSnapshot(
            snapshot,
            sizeof(snapshot));

        if (strstr(snapshot, "OK") != NULL) {

            sim800_model->dtmf_detection_enabled =
                true;

            sim800_model->call_state =
                UI_CALL_ACTIVE;

            sim800_state = SIM800_STATE_READY;
            state_started_tick = HAL_GetTick();

            setLastError("NONE");
            clearRxBuffer();

            return true;
        }

        if ((strstr(snapshot, "ERROR") != NULL) ||
            ((HAL_GetTick() - state_started_tick) >=
             SIM800_DDET_TIMEOUT_MS)) {

            sim800_model->dtmf_detection_enabled =
                false;

            sim800_state = SIM800_STATE_READY;
            state_started_tick = HAL_GetTick();

            setLastError("DDET FAILED");
            clearRxBuffer();

            return true;
        }

        break;







    case SIM800_STATE_ERROR:
        /*
         * Keep retrying periodically so a modem that
         * is powered on later can be detected.
         */
        if ((HAL_GetTick() - state_started_tick) >=
            SIM800_RETRY_INTERVAL_MS) {

            at_attempt_count = 0U;

            sim800_model->modem_state =
                UI_MODEM_INITIALIZING;

            setLastError("RETRYING AT");

            if (!sendAtCommand()) {
                state_started_tick = HAL_GetTick();
                return true;
            }

            return true;
        }
        break;




    case SIM800_STATE_SIM_ERROR:
        /*
         * Retry the SIM check periodically so a SIM
         * that becomes ready later can be detected.
         */
        if ((HAL_GetTick() - state_started_tick) >=
            SIM800_RETRY_INTERVAL_MS) {

            cpin_attempt_count = 0U;
            setLastError("RETRYING CPIN");

            (void)sendCpinCommand();
            return true;
        }
        break;

    case SIM800_STATE_READY:
        getRxSnapshot(
            snapshot,
            sizeof(snapshot));

        if (strstr(snapshot, "NO CARRIER") != NULL) {

            sim800_model->call_state =
                UI_CALL_IDLE;

            sim800_model->dtmf_detection_enabled =
                false;

            state_started_tick = HAL_GetTick();

            setLastError("NONE");
            clearRxBuffer();

            return true;
        }


        if (sim800_model->call_state == UI_CALL_ACTIVE &&
            parseDtmfEvent(snapshot, &dtmf_key)) {

            if (uiModelAddDtmf(
                    sim800_model,
                    dtmf_key)) {

                clearRxBuffer();
                return true;
            }

            clearRxBuffer();
        }


        if (strstr(snapshot, "RING") != NULL) {

            sim800_model->call_state =
                UI_CALL_RINGING;

            if (!sendAnswerCommand()) {
                sim800_model->call_state =
                    UI_CALL_IDLE;
            }

            return true;
        }

        if ((sim800_model->call_state == UI_CALL_IDLE) &&
            ((HAL_GetTick() - state_started_tick) >=
             SIM800_HEALTH_CHECK_MS)) {

            setLastError("CHECKING NETWORK");

            if (!sendCregCommand()) {
                state_started_tick = HAL_GetTick();
            }

            return true;
        }

        break;
    case SIM800_STATE_NOT_INITIALIZED:
    default:
        break;
    }

    return ui_changed;
}

void HAL_UART_RxCpltCallback(
    UART_HandleTypeDef *uart)
{
    if (!sim800_uart ||
        uart->Instance != sim800_uart->Instance) {
        return;
    }

    if (rx_index <
        (SIM800_RX_BUFFER_SIZE - 1U)) {

        rx_buffer[rx_index] = (char)rx_byte;
        rx_index++;
        rx_buffer[rx_index] = '\0';
    }

    if (HAL_UART_Receive_IT(
            sim800_uart,
            &rx_byte,
            1U) != HAL_OK) {

        rx_error_pending = 1U;
    }
}


void HAL_UART_ErrorCallback(
    UART_HandleTypeDef *uart)
{
    if (!sim800_uart ||
        uart->Instance != sim800_uart->Instance) {
        return;
    }

    if (__HAL_UART_GET_FLAG(
            sim800_uart,
            UART_FLAG_ORE) != RESET) {

        __HAL_UART_CLEAR_OREFLAG(sim800_uart);
    }

    rx_error_pending = 1U;

    (void)HAL_UART_Receive_IT(
        sim800_uart,
        &rx_byte,
        1U);
}
