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

typedef enum {
    SIM800_STATE_NOT_INITIALIZED = 0,
    SIM800_STATE_WAIT_BOOT,
    SIM800_STATE_WAIT_AT,
    SIM800_STATE_WAIT_CPIN,
    SIM800_STATE_READY,
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

bool Sim800Service_Process(void)
{
    char snapshot[SIM800_RX_BUFFER_SIZE];
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
            sim800_state = SIM800_STATE_READY;
            sim800_model->sim_ready = true;

            setLastError("NONE");
            clearRxBuffer();

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
        /*
         * The initial AT health check has completed.
         * Network, call and DTMF processing will be
         * added in later states.
         */
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
