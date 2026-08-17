#include "gfx.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>

#define MENU_COUNT 7
#define PAGE_MENU (-1)

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320

#define STATUS_BAR_Y 0
#define STATUS_BAR_HEIGHT 22

#define TITLE_BAR_Y (STATUS_BAR_Y + STATUS_BAR_HEIGHT)
#define TITLE_BAR_HEIGHT 43

#define CONTENT_TOP (TITLE_BAR_Y + TITLE_BAR_HEIGHT + 10)

#define FOOTER_LINE_Y 285
#define FOOTER_TEXT_Y 295


static const char *menu_items[MENU_COUNT] = {
    "Status",
    "Call",
    "DTMF",
    "Network",
    "Audio",
    "Diagnostics",
    "Settings"
};

static font_t font_body;
static font_t font_status;
static font_t font_menu;
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

static int signalBarCount(unsigned int rssi)
{
    if (rssi == 99 || rssi > 31) {
        return 0;
    }

    if (rssi <= 5) {
        return 1;
    }

    if (rssi <= 11) {
        return 2;
    }

    if (rssi <= 17) {
        return 3;
    }

    if (rssi <= 23) {
        return 4;
    }

    return 5;
}





static void drawSignalBars(int x, int bottom_y, unsigned int rssi)
{
    int i;
    int bar_height;
    int active_bars;
    color_t bar_color;

    active_bars = signalBarCount(rssi);

    for (i = 0; i < 5; i++) {
        bar_height = 4 + (i * 3);

        if (i < active_bars) {
            bar_color = Green;
        } else {
            bar_color = Gray;
        }

        gdispFillArea(
            x + (i * 5),
            bottom_y - bar_height,
            3,
            bar_height,
            bar_color);
    }
}


static void drawStatusBar(void)
{
    color_t modem_color;
    color_t network_color;

    modem_color = modemStateColor(ui_model->modem_state);
    network_color = networkStateColor(ui_model->network_state);

    gdispFillArea(
        0,
        STATUS_BAR_Y,
        SCREEN_WIDTH,
        STATUS_BAR_HEIGHT,
        Black);

    gdispFillArea(
        6,
        STATUS_BAR_Y + 8,
        6,
        6,
        modem_color);

    gdispDrawString(
        18,
        STATUS_BAR_Y + 5,
        networkStateText(ui_model->network_state),
        font_status,
        network_color);

    drawSignalBars(
        210,
        STATUS_BAR_Y + STATUS_BAR_HEIGHT - 2,
        (unsigned int)ui_model->signal_rssi);

    gdispDrawLine(
        0,
        STATUS_BAR_Y + STATUS_BAR_HEIGHT - 1,
        SCREEN_WIDTH - 1,
        STATUS_BAR_Y + STATUS_BAR_HEIGHT - 1,
        Gray);
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

static void drawHeader(const char *title)
{
    drawStatusBar();

    gdispFillArea(
        0,
        TITLE_BAR_Y,
        SCREEN_WIDTH,
        TITLE_BAR_HEIGHT,
        Blue);

    gdispDrawString(
        10,
        TITLE_BAR_Y + 12,
        title,
        font_menu,
        White);
}

static void drawFooter(const char *text)
{
    gdispDrawLine(
        10,
        FOOTER_LINE_Y,
        SCREEN_WIDTH - 10,
        FOOTER_LINE_Y,
        Gray);

    gdispDrawString(
        10,
        FOOTER_TEXT_Y,
        text,
        font_status,
        Gray);
}

static void drawMenu(void)
{
    int i;
    int y;

    gdispClear(Black);
    drawHeader("CELLULAR CONTROL");

    gdispFillArea(
        10,
        CONTENT_TOP,
        SCREEN_WIDTH - 20,
        190,
        Black);

    gdispDrawBox(
        10,
        CONTENT_TOP,
        SCREEN_WIDTH - 20,
        190,
        Gray);

    for (i = 0; i < MENU_COUNT; i++) {
        y = CONTENT_TOP + 10 + (i * 25);

        if (i == selected) {
            gdispFillArea( 12, y - 3,
            SCREEN_WIDTH - 24,
            23,
            Blue);
            
            gdispDrawString( 40, y, menu_items[i],
            font_menu,
            Yellow);
        } else {
            gdispDrawString( 40, y, menu_items[i],
            font_menu,
            White);
            }
    }

    drawFooter("ENTER: OPEN   ESC: EXIT");
}

static void beginPage(const char *title)
{
    int separator_y;

    separator_y = TITLE_BAR_Y + TITLE_BAR_HEIGHT;

    gdispClear(Black);
    drawHeader(title);

    gdispDrawLine(
        10,
        separator_y,
        SCREEN_WIDTH - 10,
        separator_y,
        Gray);
}

static void drawValue(
    int y,
    const char *label,
    const char *value,
    color_t value_color)
{
    gdispDrawString(15, y, label, font_body, White);
    gdispDrawString(115, y, value, font_body, value_color);
}

static void drawStatusPage(void)
{
    char signal_text[16];

    formatSignal(signal_text, sizeof(signal_text));
    beginPage("STATUS");

    gdispDrawString(15, 82, "SYSTEM STATE", font_body, Yellow);

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

    gdispDrawString(15, 82, "CALL STATE", font_body, Yellow);

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

    gdispDrawString(15, 82, "DTMF EVENTS", font_body, Yellow);

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

    gdispDrawString(15, 82, "NETWORK STATE", font_body, Yellow);

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

    gdispDrawString(15, 82, "SYSTEM HEALTH", font_body, Yellow);

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

    gdispDrawString(15, 220, "LAST ERROR", font_body, White);

    gdispDrawString(
        15,
        240,
        ui_model->last_error,
        font_body,
        ui_model->at_error_count ? Red : Green);

    drawFooter("LEFT / ESC: BACK");
}
static void drawPlaceholderPage(const char *title)
{
    beginPage(title);

    gdispDrawString(15, 105, "This page is not", font_body, White);
    gdispDrawString(15, 130, "implemented yet.", font_body, White);

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
    font_body = gdispOpenFont("DejaVuSans12");
    font_status = gdispOpenFont("DejaVuSans10");
    font_menu = gdispOpenFont("DejaVuSans16");

    ui_model = model;
    selected = 0;
    current_page = PAGE_MENU;

    if (!ui_model) {
        uiShowError("MODEL ERROR");
        return;
    }

    drawMenu();
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
    gdispDrawString(10, 255, message, font_body, Red);
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