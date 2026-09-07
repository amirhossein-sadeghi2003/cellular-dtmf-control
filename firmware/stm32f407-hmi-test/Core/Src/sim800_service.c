/*
 * sim800_service.c
 *
 *  Created on: Aug 26, 2026
 *      Author: amir
 */


#include "sim800_service.h"
#include "sim800_voice_data.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
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
#define SIM800_DTAM_TIMEOUT_MS   2000U
#define SIM800_CMEDPLAY_TIMEOUT_MS 3000U
#define SIM800_MEDIA_END_TIMEOUT_MS 15000U
#define SIM800_VOICE_SIZE_TIMEOUT_MS 3000U
#define SIM800_VOICE_FS_TIMEOUT_MS 3000U
#define SIM800_VOICE_WRITE_PROMPT_TIMEOUT_MS 3000U
#define SIM800_VOICE_WRITE_RESULT_TIMEOUT_MS 12000U
#define SIM800_VOICE_CHUNK_MAX 10240U

typedef enum {
    SIM800_STATE_NOT_INITIALIZED = 0,
    SIM800_STATE_WAIT_BOOT,
    SIM800_STATE_WAIT_AT,
    SIM800_STATE_WAIT_CPIN,
    SIM800_STATE_WAIT_CREG,
    SIM800_STATE_WAIT_CSQ,
    SIM800_STATE_WAIT_VOICE_SIZE,
    SIM800_STATE_WAIT_VOICE_DELETE,
    SIM800_STATE_WAIT_VOICE_CREATE,
    SIM800_STATE_WAIT_VOICE_WRITE_PROMPT,
    SIM800_STATE_WAIT_VOICE_WRITE_RESULT,
    SIM800_STATE_WAIT_VOICE_VERIFY,
    SIM800_STATE_NETWORK_RETRY,
    SIM800_STATE_READY,
	SIM800_STATE_WAIT_ANSWER,
	SIM800_STATE_WAIT_DDET,
    SIM800_STATE_WAIT_DTAM,
    SIM800_STATE_WAIT_CMEDPLAY,
    SIM800_STATE_WAIT_MEDIA_END,
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

static uint32_t voice_upload_offset;
static uint32_t voice_upload_chunk_size;
static char voice_fs_command[96];

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



static bool sendDtamCommand(void)
{
    static const uint8_t command[] =
        "AT+DTAM=1\r";

    clearRxBuffer();

    if (HAL_UART_Transmit(
            sim800_uart,
            (uint8_t *)command,
            sizeof(command) - 1U,
            1000U) != HAL_OK) {

        setLastError("DTAM TX ERROR");
        return false;
    }

    sim800_state = SIM800_STATE_WAIT_DTAM;
    state_started_tick = HAL_GetTick();

    return true;
}


static bool sendMediaPlayCommand(void)
{
    static const uint8_t command[] =
        "AT+CMEDPLAY=1,C:\\voice.wav,0,100\r";

    clearRxBuffer();

    if (HAL_UART_Transmit(
            sim800_uart,
            (uint8_t *)command,
            sizeof(command) - 1U,
            1000U) != HAL_OK) {

        setLastError("CMEDPLAY TX ERROR");
        return false;
    }

    sim800_state = SIM800_STATE_WAIT_CMEDPLAY;
    state_started_tick = HAL_GetTick();

    return true;
}


static bool sendVoiceSizeCommand(void)
{
    static const uint8_t command[] =
        "AT+FSFLSIZE=C:\\voice.wav\r";

    clearRxBuffer();

    if (HAL_UART_Transmit(
            sim800_uart,
            (uint8_t *)command,
            sizeof(command) - 1U,
            1000U) != HAL_OK) {

        setLastError("VOICE CHECK TX");
        return false;
    }

    sim800_state = SIM800_STATE_WAIT_VOICE_SIZE;
    state_started_tick = HAL_GetTick();
    setLastError("CHECKING VOICE");

    return true;
}


static bool parseVoiceFileSize(
    const char *response,
    uint32_t *file_size)
{
    const char *position;
    uint32_t value = 0U;

    if (!response || !file_size)
        return false;

    position = strstr(response, "+FSFLSIZE:");

    if (!position)
        return false;

    position += sizeof("+FSFLSIZE:") - 1U;

    while (*position == ' ')
        position++;

    if (*position < '0' || *position > '9')
        return false;

    while (*position >= '0' && *position <= '9') {
        value = value * 10U +
            (uint32_t)(*position - '0');
        position++;
    }

    *file_size = value;
    return true;
}


static bool sendVoiceDeleteCommand(void)
{
    static const uint8_t command[] =
        "AT+FSDEL=C:\\voice.wav\r";

    clearRxBuffer();

    if (HAL_UART_Transmit(
            sim800_uart,
            (uint8_t *)command,
            sizeof(command) - 1U,
            1000U) != HAL_OK) {

        setLastError("VOICE DELETE TX");
        return false;
    }

    sim800_state = SIM800_STATE_WAIT_VOICE_DELETE;
    state_started_tick = HAL_GetTick();
    setLastError("DELETING VOICE");

    return true;
}


static bool sendVoiceCreateCommand(void)
{
    static const uint8_t command[] =
        "AT+FSCREATE=C:\\voice.wav\r";

    clearRxBuffer();

    if (HAL_UART_Transmit(
            sim800_uart,
            (uint8_t *)command,
            sizeof(command) - 1U,
            1000U) != HAL_OK) {

        setLastError("VOICE CREATE TX");
        return false;
    }

    sim800_state = SIM800_STATE_WAIT_VOICE_CREATE;
    state_started_tick = HAL_GetTick();
    setLastError("CREATING VOICE");

    return true;
}


static bool startNextVoiceWrite(void)
{
    uint32_t remaining;
    uint8_t mode;
    int command_length;

    if (voice_upload_offset >= SIM800_VOICE_DATA_LEN)
        return false;

    remaining =
        SIM800_VOICE_DATA_LEN - voice_upload_offset;

    if (remaining > SIM800_VOICE_CHUNK_MAX)
        voice_upload_chunk_size =
            SIM800_VOICE_CHUNK_MAX;
    else
        voice_upload_chunk_size = remaining;

    mode = (voice_upload_offset == 0U) ?
        0U : 1U;

    command_length = snprintf(
        voice_fs_command,
        sizeof(voice_fs_command),
        "AT+FSWRITE=C:\\voice.wav,%u,%lu,10\r",
        (unsigned int)mode,
        (unsigned long)voice_upload_chunk_size);

    if ((command_length <= 0) ||
        ((size_t)command_length >=
         sizeof(voice_fs_command))) {

        setLastError("VOICE WRITE CMD");
        return false;
    }

    clearRxBuffer();

    if (HAL_UART_Transmit(
            sim800_uart,
            (uint8_t *)voice_fs_command,
            (uint16_t)command_length,
            1000U) != HAL_OK) {

        setLastError("VOICE WRITE TX");
        return false;
    }

    sim800_state =
        SIM800_STATE_WAIT_VOICE_WRITE_PROMPT;

    state_started_tick = HAL_GetTick();
    setLastError("UPLOADING VOICE");

    return true;
}


static bool sendVoiceVerifyCommand(void)
{
    static const uint8_t command[] =
        "AT+FSFLSIZE=C:\\voice.wav\r";

    clearRxBuffer();

    if (HAL_UART_Transmit(
            sim800_uart,
            (uint8_t *)command,
            sizeof(command) - 1U,
            1000U) != HAL_OK) {

        setLastError("VOICE VERIFY TX");
        return false;
    }

    sim800_state = SIM800_STATE_WAIT_VOICE_VERIFY;
    state_started_tick = HAL_GetTick();
    setLastError("VERIFYING VOICE");

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

    voice_upload_offset = 0U;
    voice_upload_chunk_size = 0U;
    memset(voice_fs_command, 0, sizeof(voice_fs_command));

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

            if (!sendVoiceSizeCommand()) {
                sim800_state = SIM800_STATE_READY;
                state_started_tick = HAL_GetTick();
            }

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

    case SIM800_STATE_WAIT_VOICE_SIZE:
        getRxSnapshot(
            snapshot,
            sizeof(snapshot));

        {
            uint32_t voice_file_size;

            if (parseVoiceFileSize(
                    snapshot,
                    &voice_file_size)) {

                if (voice_file_size ==
                    SIM800_VOICE_DATA_LEN) {

                    sim800_state = SIM800_STATE_READY;
                    state_started_tick = HAL_GetTick();
                    setLastError("VOICE READY");
                    clearRxBuffer();

                    return true;
                }

                if (!sendVoiceDeleteCommand()) {
                    sim800_state = SIM800_STATE_READY;
                    state_started_tick = HAL_GetTick();
                }

                return true;
            }
        }

        if (strstr(snapshot, "ERROR") != NULL) {

            if (!sendVoiceDeleteCommand()) {
                sim800_state = SIM800_STATE_READY;
                state_started_tick = HAL_GetTick();
            }

            return true;
        }

        if ((HAL_GetTick() - state_started_tick) >=
            SIM800_VOICE_SIZE_TIMEOUT_MS) {

            sim800_state = SIM800_STATE_READY;
            state_started_tick = HAL_GetTick();
            setLastError("VOICE CHECK TIMEOUT");
            clearRxBuffer();

            return true;
        }

        break;


    case SIM800_STATE_WAIT_VOICE_DELETE:
        getRxSnapshot(
            snapshot,
            sizeof(snapshot));

        if ((strstr(snapshot, "OK") != NULL) ||
            (strstr(snapshot, "ERROR") != NULL) ||
            ((HAL_GetTick() - state_started_tick) >=
             SIM800_VOICE_FS_TIMEOUT_MS)) {

            if (!sendVoiceCreateCommand()) {
                sim800_state = SIM800_STATE_READY;
                state_started_tick = HAL_GetTick();
            }

            return true;
        }

        break;


    case SIM800_STATE_WAIT_VOICE_CREATE:
        getRxSnapshot(
            snapshot,
            sizeof(snapshot));

        if (strstr(snapshot, "OK") != NULL) {

            voice_upload_offset = 0U;

            if (!startNextVoiceWrite()) {
                sim800_state = SIM800_STATE_READY;
                state_started_tick = HAL_GetTick();
                setLastError("VOICE UPLOAD FAIL");
            }

            return true;
        }

        if ((strstr(snapshot, "ERROR") != NULL) ||
            ((HAL_GetTick() - state_started_tick) >=
             SIM800_VOICE_FS_TIMEOUT_MS)) {

            sim800_state = SIM800_STATE_READY;
            state_started_tick = HAL_GetTick();
            setLastError("VOICE CREATE FAIL");
            clearRxBuffer();

            return true;
        }

        break;


    case SIM800_STATE_WAIT_VOICE_WRITE_PROMPT:
        getRxSnapshot(
            snapshot,
            sizeof(snapshot));

        if (strchr(snapshot, '>') != NULL) {

            clearRxBuffer();

            if (HAL_UART_Transmit(
                    sim800_uart,
                    (uint8_t *)&sim800_voice_data[
                        voice_upload_offset],
                    (uint16_t)voice_upload_chunk_size,
                    15000U) != HAL_OK) {

                sim800_state = SIM800_STATE_READY;
                state_started_tick = HAL_GetTick();
                setLastError("VOICE DATA TX");
                return true;
            }

            sim800_state =
                SIM800_STATE_WAIT_VOICE_WRITE_RESULT;

            state_started_tick = HAL_GetTick();

            return true;
        }

        if ((strstr(snapshot, "ERROR") != NULL) ||
            ((HAL_GetTick() - state_started_tick) >=
             SIM800_VOICE_WRITE_PROMPT_TIMEOUT_MS)) {

            sim800_state = SIM800_STATE_READY;
            state_started_tick = HAL_GetTick();
            setLastError("VOICE NO PROMPT");
            clearRxBuffer();

            return true;
        }

        break;


    case SIM800_STATE_WAIT_VOICE_WRITE_RESULT:
        getRxSnapshot(
            snapshot,
            sizeof(snapshot));

        if (strstr(snapshot, "OK") != NULL) {

            voice_upload_offset +=
                voice_upload_chunk_size;

            if (voice_upload_offset <
                SIM800_VOICE_DATA_LEN) {

                if (!startNextVoiceWrite()) {
                    sim800_state = SIM800_STATE_READY;
                    state_started_tick = HAL_GetTick();
                    setLastError("VOICE UPLOAD FAIL");
                }
            } else {

                if (!sendVoiceVerifyCommand()) {
                    sim800_state = SIM800_STATE_READY;
                    state_started_tick = HAL_GetTick();
                }
            }

            return true;
        }

        if ((strstr(snapshot, "ERROR") != NULL) ||
            ((HAL_GetTick() - state_started_tick) >=
             SIM800_VOICE_WRITE_RESULT_TIMEOUT_MS)) {

            sim800_state = SIM800_STATE_READY;
            state_started_tick = HAL_GetTick();
            setLastError("VOICE WRITE FAIL");
            clearRxBuffer();

            return true;
        }

        break;


    case SIM800_STATE_WAIT_VOICE_VERIFY:
        getRxSnapshot(
            snapshot,
            sizeof(snapshot));

        {
            uint32_t voice_file_size;

            if (parseVoiceFileSize(
                    snapshot,
                    &voice_file_size)) {

                sim800_state = SIM800_STATE_READY;
                state_started_tick = HAL_GetTick();

                if (voice_file_size ==
                    SIM800_VOICE_DATA_LEN) {
                    setLastError("VOICE READY");
                } else {
                    setLastError("VOICE VERIFY BAD");
                }

                clearRxBuffer();
                return true;
            }
        }

        if ((strstr(snapshot, "ERROR") != NULL) ||
            ((HAL_GetTick() - state_started_tick) >=
             SIM800_VOICE_FS_TIMEOUT_MS)) {

            sim800_state = SIM800_STATE_READY;
            state_started_tick = HAL_GetTick();
            setLastError("VOICE VERIFY FAIL");
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








    case SIM800_STATE_WAIT_DTAM:
        getRxSnapshot(
            snapshot,
            sizeof(snapshot));

        if (strstr(snapshot, "NO CARRIER") != NULL) {

            sim800_model->call_state = UI_CALL_IDLE;
            sim800_model->dtmf_detection_enabled = false;

            sim800_state = SIM800_STATE_READY;
            state_started_tick = HAL_GetTick();

            setLastError("NONE");
            clearRxBuffer();

            return true;
        }

        if (strstr(snapshot, "OK") != NULL) {

            if (!sendMediaPlayCommand()) {
                sim800_state = SIM800_STATE_READY;
                state_started_tick = HAL_GetTick();
            }

            return true;
        }

        if ((strstr(snapshot, "ERROR") != NULL) ||
            ((HAL_GetTick() - state_started_tick) >=
             SIM800_DTAM_TIMEOUT_MS)) {

            sim800_state = SIM800_STATE_READY;
            state_started_tick = HAL_GetTick();

            setLastError("DTAM FAILED");
            clearRxBuffer();

            return true;
        }

        break;


    case SIM800_STATE_WAIT_CMEDPLAY:
        getRxSnapshot(
            snapshot,
            sizeof(snapshot));

        if (strstr(snapshot, "NO CARRIER") != NULL) {

            sim800_model->call_state = UI_CALL_IDLE;
            sim800_model->dtmf_detection_enabled = false;

            sim800_state = SIM800_STATE_READY;
            state_started_tick = HAL_GetTick();

            setLastError("NONE");
            clearRxBuffer();

            return true;
        }

        if (strstr(snapshot, "OK") != NULL) {

            sim800_state = SIM800_STATE_WAIT_MEDIA_END;
            state_started_tick = HAL_GetTick();

            setLastError("NONE");
            clearRxBuffer();

            return true;
        }

        if ((strstr(snapshot, "ERROR") != NULL) ||
            ((HAL_GetTick() - state_started_tick) >=
             SIM800_CMEDPLAY_TIMEOUT_MS)) {

            sim800_state = SIM800_STATE_READY;
            state_started_tick = HAL_GetTick();

            setLastError("CMEDPLAY FAILED");
            clearRxBuffer();

            return true;
        }

        break;


    case SIM800_STATE_WAIT_MEDIA_END:
        getRxSnapshot(
            snapshot,
            sizeof(snapshot));

        if (strstr(snapshot, "NO CARRIER") != NULL) {

            sim800_model->call_state = UI_CALL_IDLE;
            sim800_model->dtmf_detection_enabled = false;

            sim800_state = SIM800_STATE_READY;
            state_started_tick = HAL_GetTick();

            setLastError("NONE");
            clearRxBuffer();

            return true;
        }

        if ((strstr(snapshot, "+CMEDPLAY: 0") != NULL) ||
            (strstr(snapshot, "+CMEDPLAY:0") != NULL)) {

            sim800_state = SIM800_STATE_READY;
            state_started_tick = HAL_GetTick();

            setLastError("NONE");
            clearRxBuffer();

            return true;
        }

        if ((HAL_GetTick() - state_started_tick) >=
            SIM800_MEDIA_END_TIMEOUT_MS) {

            sim800_state = SIM800_STATE_READY;
            state_started_tick = HAL_GetTick();

            setLastError("PLAYBACK TIMEOUT");
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

            bool dtmf_changed;

            dtmf_changed = uiModelAddDtmf(
                sim800_model,
                dtmf_key);

            clearRxBuffer();

            /*
             * DTMF 1 plays the prerecorded voice file
             * already stored in SIM800 local filesystem.
             */
            if (dtmf_key == '1') {

                if (!sendDtamCommand()) {
                    sim800_state = SIM800_STATE_READY;
                    state_started_tick = HAL_GetTick();
                }

                return true;
            }

            if (dtmf_changed)
                return true;
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
