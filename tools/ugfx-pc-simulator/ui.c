#include "gfx.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>

#define MENU_COUNT 7
#define PAGE_MENU (-1)

static const char *menu_items[MENU_COUNT] = {
    "Status",
    "Call",
    "DTMF",
    "Network",
    "Audio",
    "Diagnostics",
    "Settings"
};

static font_t font;
static UiModel *ui_model;
static int selected;
static int current_page;

static const char *modemStateText(UiModemState state)
{
    switch (state) {
    case UI_MODEM_OFFLINE:
        return "OFFLINE";

    case UI_MODEM_INITIALIZING:
        return "INITIALIZING";

    case UI_MODEM_READY:
        return "READY";

    case UI_MODEM_ERROR:
        return "ERROR";

    default:
        return "UNKNOWN";
    }
}

static color_t modemStateColor(UiModemState state)
{
    switch (state) {
    case UI_MODEM_READY:
        return Green;

    case UI_MODEM_INITIALIZING:
        return Yellow;

    case UI_MODEM_ERROR:
        return Red;

    default:
        return Gray;
    }
}

static const char *networkStateText(UiNetworkState state)
{
    switch (state) {
    case UI_NETWORK_NOT_REGISTERED:
        return "NOT REGISTERED";

    case UI_NETWORK_SEARCHING:
        return "SEARCHING";

    case UI_NETWORK_HOME:
        return "HOME";

    case UI_NETWORK_ROAMING:
        return "ROAMING";

    case UI_NETWORK_DENIED:
        return "DENIED";

    default:
        return "UNKNOWN";
    }
}

static color_t networkStateColor(UiNetworkState state)
{
    switch (state) {
    case UI_NETWORK_HOME:
        return Green;

    case UI_NETWORK_ROAMING:
    case UI_NETWORK_SEARCHING:
        return Yellow;

    case UI_NETWORK_DENIED:
        return Red;

    default:
        return Gray;
    }
}

static const char *callStateText(UiCallState state)
{
    switch (state) {
    case UI_CALL_IDLE:
        return "IDLE";

    case UI_CALL_RINGING:
        return "RINGING";

    case UI_CALL_ANSWERING:
        return "ANSWERING";

    case UI_CALL_ACTIVE:
        return "ACTIVE";

    case UI_CALL_ENDED:
        return "ENDED";

    default:
        return "UNKNOWN";
    }
}

static color_t callStateColor(UiCallState state)
{
    switch (state) {
    case UI_CALL_IDLE:
    case UI_CALL_ACTIVE:
        return Green;

    case UI_CALL_RINGING:
    case UI_CALL_ANSWERING:
        return Yellow;

    case UI_CALL_ENDED:
        return Gray;

    default:
        return Red;
    }
}

static void formatSignal(char *buffer, size_t buffer_size)
{
    if (ui_model->signal_rssi == 99) {
        snprintf(buffer, buffer_size, "UNKNOWN");
        return;
    }

    snprintf(
        buffer,
        buffer_size,
        "%u / 31",
        (unsigned int)ui_model->signal_rssi);
}

static void formatDuration(char *buffer, size_t buffer_size)
{
    uint32_t minutes;
    uint32_t seconds;

    minutes = ui_model->call_duration_seconds / 60;
    seconds = ui_model->call_duration_seconds % 60;

    snprintf(
        buffer,
        buffer_size,
        "%02lu:%02lu",
        (unsigned long)minutes,
        (unsigned long)seconds);
}

static void drawHeader(void)
{
    gdispFillArea(0, 0, 240, 35, Blue);
    gdispDrawString(10, 10, "CELLULAR CONTROL", font, White);
}

static void drawFooter(const char *text)
{
    gdispDrawLine(10, 285, 230, 285, Gray);
    gdispDrawString(10, 295, text, font, Gray);
}

static void drawMenu(void)
{
    int i;
    int y;

    gdispClear(Black);
    drawHeader();

    gdispFillArea(10, 45, 220, 190, Black);
    gdispDrawBox(10, 45, 220, 190, Gray);

    for (i = 0; i < MENU_COUNT; i++) {
        y = 58 + (i * 25);

        if (i == selected) {
            gdispDrawString(20, y, ">", font, Yellow);
            gdispDrawString(32, y, menu_items[i], font, Yellow);
        } else {
            gdispDrawString(32, y, menu_items[i], font, White);
        }
    }

    drawFooter("ENTER: OPEN   ESC: EXIT");
}

static void beginPage(const char *title)
{
    gdispClear(Black);
    drawHeader();

    gdispDrawString(10, 50, title, font, Cyan);
    gdispDrawLine(10, 70, 230, 70, Gray);
}

static void drawValue(
    int y,
    const char *label,
    const char *value,
    color_t value_color)
{
    gdispDrawString(15, y, label, font, White);
    gdispDrawString(115, y, value, font, value_color);
}

static void drawStatusPage(void)
{
    char signal_text[16];

    formatSignal(signal_text, sizeof(signal_text));
    beginPage("STATUS");

    gdispDrawString(15, 82, "SYSTEM STATE", font, Yellow);

    drawValue(
        110,
        "MODEM",
        modemStateText(ui_model->modem_state),
        modemStateColor(ui_model->modem_state));

    drawValue(
        140,
        "NETWORK",
        networkStateText(ui_model->network_state),
        networkStateColor(ui_model->network_state));

    drawValue(
        170,
        "CALL",
        callStateText(ui_model->call_state),
        callStateColor(ui_model->call_state));

    drawValue(200, "SIGNAL", signal_text, Green);

    drawFooter("LEFT / ESC: BACK");
}

static void drawCallPage(void)
{
    char duration_text[16];

    formatDuration(duration_text, sizeof(duration_text));
    beginPage("CALL");

    gdispDrawString(15, 82, "CALL STATE", font, Yellow);

    drawValue(
        110,
        "STATE",
        callStateText(ui_model->call_state),
        callStateColor(ui_model->call_state));

    drawValue(
        140,
        "AUTO ANSWER",
        ui_model->auto_answer_enabled ? "ON" : "OFF",
        ui_model->auto_answer_enabled ? Green : Gray);

    drawValue(170, "CALLER", ui_model->caller, White);
    drawValue(200, "DURATION", duration_text, White);

    drawFooter("LEFT / ESC: BACK");
}

static void drawDtmfPage(void)
{
    char last_key_text[2];
    const char *buffer_text;
    const char *status_text;

    last_key_text[0] = ui_model->last_dtmf;
    last_key_text[1] = '\0';

    buffer_text = ui_model->dtmf_buffer[0]
        ? ui_model->dtmf_buffer
        : "-";

    status_text = ui_model->last_dtmf == '-'
        ? "WAITING"
        : "RECEIVED";

    beginPage("DTMF");

    gdispDrawString(15, 82, "DTMF EVENTS", font, Yellow);

    drawValue(
        110,
        "DETECTION",
        ui_model->dtmf_detection_enabled ? "ENABLED" : "DISABLED",
        ui_model->dtmf_detection_enabled ? Green : Gray);

    drawValue(140, "LAST KEY", last_key_text, White);
    drawValue(170, "BUFFER", buffer_text, White);
    drawValue(200, "STATUS", status_text, Green);

    drawFooter("LEFT / ESC: BACK");
}

static void drawNetworkPage(void)
{
    char signal_text[16];

    formatSignal(signal_text, sizeof(signal_text));
    beginPage("NETWORK");

    gdispDrawString(15, 82, "NETWORK STATE", font, Yellow);

    drawValue(
        110,
        "REGISTRATION",
        networkStateText(ui_model->network_state),
        networkStateColor(ui_model->network_state));

    drawValue(140, "SIGNAL", signal_text, Green);
    drawValue(170, "TECHNOLOGY", "GSM", White);
    drawValue(200, "OPERATOR", ui_model->operator_name, White);

    drawFooter("LEFT / ESC: BACK");
}
static void drawDiagnosticsPage(void)
{
    char reset_count_text[16];
    char at_error_count_text[16];
    bool no_active_error;

    snprintf(
        reset_count_text,
        sizeof(reset_count_text),
        "%lu",
        (unsigned long)ui_model->modem_reset_count);

    snprintf(
        at_error_count_text,
        sizeof(at_error_count_text),
        "%lu",
        (unsigned long)ui_model->at_error_count);
        no_active_error =
    strcmp(ui_model->last_error, "NONE") == 0;

    beginPage("DIAGNOSTICS");

    gdispDrawString(15, 82, "SYSTEM HEALTH", font, Yellow);

    drawValue(
        105,
        "UART",
        ui_model->uart_ready ? "READY" : "ERROR",
        ui_model->uart_ready ? Green : Red);

    drawValue(
        135,
        "SIM",
        ui_model->sim_ready ? "READY" : "NOT READY",
        ui_model->sim_ready ? Green : Red);

    drawValue(
        165,
        "RESETS",
        reset_count_text,
        ui_model->modem_reset_count ? Yellow : Green);

    drawValue(
        195,
        "AT ERRORS",
        at_error_count_text,
        ui_model->at_error_count ? Green : Red);

    gdispDrawString(15, 220, "LAST ERROR", font, White);

    gdispDrawString(
        15,
        240,
        ui_model->last_error,
        font,
        ui_model->at_error_count ? Red : Green);

    drawFooter("LEFT / ESC: BACK");
}
static void drawPlaceholderPage(const char *title)
{
    beginPage(title);

    gdispDrawString(15, 105, "This page is not", font, White);
    gdispDrawString(15, 130, "implemented yet.", font, White);

    drawFooter("LEFT / ESC: BACK");
}

static void drawPage(void)
{
    if (!ui_model) {
        uiShowError("MODEL ERROR");
        return;
    }

    switch (current_page) {
    case 0:
        drawStatusPage();
        break;

    case 1:
        drawCallPage();
        break;

    case 2:
        drawDtmfPage();
        break;

    case 3:
        drawNetworkPage();
        break;

    case 4:
        drawPlaceholderPage(menu_items[current_page]);
        break;

    case 5:
        drawDiagnosticsPage();
        break;

    case 6:
        drawPlaceholderPage(menu_items[current_page]);
        break;

    default:
        break;
    }
}

void uiInit(UiModel *model)
{
    font = gdispOpenFont("UI2");
    ui_model = model;
    selected = 0;
    current_page = PAGE_MENU;

    drawMenu();

    if (!ui_model)
        uiShowError("MODEL ERROR");
}

void uiRefresh(void)
{
    if (current_page == PAGE_MENU)
        drawMenu();
    else
        drawPage();
}

void uiShowError(const char *message)
{
    gdispFillArea(10, 250, 220, 30, Black);
    gdispDrawString(10, 255, message, font, Red);
}

UiAction uiHandleKey(UiKey key)
{
    if (current_page == PAGE_MENU) {
        switch (key) {
        case UI_KEY_DOWN:
            selected++;

            if (selected >= MENU_COUNT)
                selected = 0;

            drawMenu();
            break;

        case UI_KEY_UP:
            selected--;

            if (selected < 0)
                selected = MENU_COUNT - 1;

            drawMenu();
            break;

        case UI_KEY_ENTER:
            current_page = selected;
            drawPage();
            break;

        case UI_KEY_BACK:
            return UI_ACTION_EXIT;

        default:
            break;
        }
    } else {
        switch (key) {
        case UI_KEY_LEFT:
        case UI_KEY_BACK:
            current_page = PAGE_MENU;
            drawMenu();
            break;

        default:
            break;
        }
    }

    return UI_ACTION_NONE;
}