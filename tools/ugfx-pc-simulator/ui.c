#include "gfx.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>

#define MENU_COUNT 7
#define PAGE_MENU (-1)

#define PAGE_ADMIN_LOGIN (-2)
#define ADMIN_PIN_LENGTH 4

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320

#define STATUS_BAR_Y 0
#define STATUS_BAR_HEIGHT 22

#define TITLE_BAR_Y (STATUS_BAR_Y + STATUS_BAR_HEIGHT)
#define TITLE_BAR_HEIGHT 43

#define CONTENT_TOP (TITLE_BAR_Y + TITLE_BAR_HEIGHT + 10)

#define FOOTER_LINE_Y 285
#define FOOTER_TEXT_Y 295

#define DISPLAY_SETTING_COUNT 3

typedef enum{
    UI_ROLE_USER = 0,
    UI_ROLE_ADMIN
} UiRole;

typedef enum{
    UI_ACCESS_DENIED,
    UI_ACCESS_READ_ONLY,
    UI_ACCESS_LIMITED,
    UI_ACCESS_EDITABLE
} UiPageAccess;


static const char *menu_items[MENU_COUNT] = {
    "Status",
    "Call",
    "DTMF",
    "Network",
    "Display",
    "Diagnostics",
    "Settings"
};

static font_t font_body;
static font_t font_status;
static font_t font_menu;
static UiModel *ui_model;
static int selected;
static int current_page;
static UiRole current_role;
static char pin_digits[ADMIN_PIN_LENGTH + 1];
static int pin_cursor;
static int pending_page;
static const char admin_pin[] = "1234";
static int display_selected;
static bool display_editing;


static const char *roleText(UiRole role)
{
    switch (role) {
    case UI_ROLE_USER:
        return "USER";

    case UI_ROLE_ADMIN:
        return "ADMIN";

    default:
        return "UNKNOWN";
    }
}


static UiPageAccess pageAccessForRole(UiRole role, int page_index){
    switch(page_index){
        case 0:
        case 1:
        case 2:
            return UI_ACCESS_READ_ONLY;
        case 3:
            return role == UI_ROLE_ADMIN ? UI_ACCESS_EDITABLE : UI_ACCESS_READ_ONLY;
        case 4:
            return role == UI_ROLE_ADMIN ? UI_ACCESS_EDITABLE : UI_ACCESS_LIMITED;
        case 5:
            return role == UI_ROLE_ADMIN ? UI_ACCESS_READ_ONLY : UI_ACCESS_DENIED;
        case 6:
            return role == UI_ROLE_ADMIN ? UI_ACCESS_EDITABLE : UI_ACCESS_DENIED;
        default:
            return UI_ACCESS_DENIED; 
    }
}


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

    gdispDrawString(
        165,
        STATUS_BAR_Y + 5,
        roleText(current_role),
        font_status,
        current_role == UI_ROLE_ADMIN ? Yellow : White);

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
    gdispFillArea(
        0,
        FOOTER_LINE_Y,
        SCREEN_WIDTH,
        SCREEN_HEIGHT - FOOTER_LINE_Y,
        Black);


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

static void drawMenuIcon(int item_index, int x, int y, 
    color_t color){
    int row;
    int column;
    
    switch (item_index) {
        case 0:
            gdispDrawLine(x, y + 8, x + 3, y + 8, color);
            gdispDrawLine(x + 3, y + 8, x + 6, y + 4, color);
            gdispDrawLine(x + 6, y + 4, x + 9, y + 12, color);
            gdispDrawLine(x + 9, y + 12, x + 12, y + 8, color);
            gdispDrawLine(x + 12, y + 8, x + 15, y + 8, color);
            break;

        case 1:
            gdispDrawBox(x + 3, y + 1, 10, 14, color);
            gdispDrawLine(x + 5, y + 3, x + 10, y + 3, color);
            gdispFillArea(x + 7, y + 12, 2, 2, color);
            break;

        case 2:
            for (row = 0; row < 4; row++) {
                for (column = 0; column < 3; column++) {
                    gdispFillArea( x + 2 + (column * 5),
                        y + 1 + (row * 4),
                        2,
                        2,
                        color);
                }
            }
            break;

        case 3:
            gdispFillArea(x + 1, y + 11, 2, 4, color);
            gdispFillArea(x + 5, y + 8, 2, 7, color);
            gdispFillArea(x + 9, y + 5, 2, 10, color);
            gdispFillArea(x + 13, y + 2, 2, 13, color);
            break; 

        case 4:
            gdispDrawBox(x + 1, y + 1, 14, 10, color);
            gdispDrawLine(x + 8, y + 11, x + 8, y + 14, color);
            gdispDrawLine(x + 4, y + 14, x + 12, y + 14, color);
            break;       
        
        case 5:         
            gdispDrawBox(x + 1, y + 1, 9, 9, color);
            gdispDrawLine(x + 9, y + 9, x + 15, y + 15, color);
            gdispDrawLine(x + 5, y + 3, x + 5, y + 7, color);
            gdispDrawLine(x + 3, y + 5, x + 7, y + 5, color);
            break;
        
        case 6:
            gdispDrawLine(x + 1, y + 3, x + 15, y + 3, color);
            gdispFillArea(x + 4, y + 1, 3, 5, color);

            gdispDrawLine(x + 1, y + 8, x + 15, y + 8, color);
            gdispFillArea(x + 10, y + 6, 3, 5, color);

            gdispDrawLine(x + 1, y + 13, x + 15, y + 13, color);
            gdispFillArea(x + 6, y + 11, 3, 5, color);
            break;   

        
        default:
            break;
    }
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
            gdispFillArea(
                12,
                y - 3,
                SCREEN_WIDTH - 24,
                23,
                Blue);
        }

        drawMenuIcon(
            i,
            20,
            y + 1,
            White);

        gdispDrawString(
            40,
            y,
            menu_items[i],
            font_menu,
            White);
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


static void drawAdminLoginPage(void)
{
    int i;
    int box_x;
    char digit_text[2];

    beginPage("ADMIN LOGIN");

    gdispDrawString(
        15,
        82,
        "ENTER ADMIN PIN",
        font_body,
        Yellow);

    for (i = 0; i < ADMIN_PIN_LENGTH; i++) {
        box_x = 33 + (i * 46);

        if (i == pin_cursor) {
            gdispFillArea(
                box_x,
                125,
                36,
                44,
                Blue);
        }

        gdispDrawBox(
            box_x,
            125,
            36,
            44,
            i == pin_cursor ? White : Gray);

        digit_text[0] = pin_digits[i];
        digit_text[1] = '\0';

        gdispDrawString(
            box_x + 12,
            137,
            digit_text,
            font_menu,
            White);
    }

    gdispDrawString(
        35,
        200,
        "UP/DOWN: CHANGE",
        font_status,
        Gray);

    gdispDrawString(
        35,
        220,
        "LEFT/RIGHT: MOVE",
        font_status,
        Gray);

    drawFooter("ENTER: LOGIN   ESC: CANCEL");
}


static void startAdminLogin(int page_index)
{
    pending_page = page_index;
    pin_cursor = 0;

    memcpy(
        pin_digits,
        "0000",
        ADMIN_PIN_LENGTH + 1);

    current_page = PAGE_ADMIN_LOGIN;
    drawAdminLoginPage();
}




static void drawValue(int y, const char *label, const char *value,
    color_t value_color)
{
    gdispDrawString(15, y, label, font_body, White);
    gdispDrawString(115, y, value, font_body, value_color);
}



static void drawAccessBadge(int page_index)
{
    UiPageAccess access;
    const char *access_text;
    color_t access_color;

    access = pageAccessForRole(
        current_role,
        page_index);

    switch (access) {
    case UI_ACCESS_READ_ONLY:
        access_text = "READ ONLY";
        access_color = Gray;
        break;

    case UI_ACCESS_LIMITED:
        access_text = "LIMITED";
        access_color = Yellow;
        break;

    case UI_ACCESS_EDITABLE:
        access_text = "EDITABLE";
        access_color = Green;
        break;

    default:
        access_text = "DENIED";
        access_color = Red;
        break;
    }

    gdispDrawBox(
        145,
        80,
        80,
        20,
        access_color);

    gdispDrawString(
        151,
        85,
        access_text,
        font_status,
        access_color);
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
    drawAccessBadge(3);
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




static void drawDisplaySelection(
    int item_index,
    int value_y)
{
    if (display_selected != item_index)
        return;

    gdispFillArea(
        10,
        value_y - 7,
        SCREEN_WIDTH - 20,
        28,
        Blue);
}






static void drawDisplayPage(void)
{
    char brightness_text[8];
    char timeout_text[12];
    char refresh_text[12];
    const char *mode_text;
    color_t mode_color;
    UiPageAccess access;

    access = pageAccessForRole(
        current_role,
        4);

    if (display_editing) {
        mode_text = "EDIT MODE";
        mode_color = Yellow;
    } else if (access == UI_ACCESS_EDITABLE) {
        mode_text = "ALL SETTINGS UNLOCKED";
        mode_color = Green;
    } else {
        mode_text = "BRIGHTNESS ONLY";
        mode_color = Gray;
    }

    snprintf(
        brightness_text,
        sizeof(brightness_text),
        "%u%%",
        (unsigned int)ui_model->display_brightness_percent);

    snprintf(
        timeout_text,
        sizeof(timeout_text),
        "%u s",
        (unsigned int)ui_model->screen_timeout_seconds);

    snprintf(
        refresh_text,
        sizeof(refresh_text),
        "%u s",
        (unsigned int)ui_model->status_refresh_interval_seconds);

    beginPage("DISPLAY");

    gdispDrawString(
        15,
        82,
        "DISPLAY CONTROL",
        font_body,
        Yellow);

    drawAccessBadge(4);

    drawDisplaySelection(0, 120);
    drawValue(
        120,
        "BRIGHTNESS",
        brightness_text,
        Green);

    drawDisplaySelection(1, 155);
    drawValue(
        155,
        "TIMEOUT",
        timeout_text,
        access == UI_ACCESS_EDITABLE ? Green : Gray);

    drawDisplaySelection(2, 190);
    drawValue(
        190,
        "REFRESH",
        refresh_text,
        access == UI_ACCESS_EDITABLE ? Green : Gray);

    gdispDrawString(
        15,
        225,
        mode_text,
        font_status,
        mode_color);

    drawFooter(
        display_editing
            ? "LEFT/RIGHT: CHANGE   ENTER: DONE"
            : "UP/DOWN: SELECT   ENTER: EDIT");
}

static bool displaySettingEditable(int item_index)
{
    if (item_index == 0)
        return true;

    if (current_role == UI_ROLE_ADMIN)
        return true;

    return false;
}


static void adjustDisplaySetting(int direction)
{
    switch (display_selected) {
    case 0:
        if (direction > 0 &&
            ui_model->display_brightness_percent < 100) {
            ui_model->display_brightness_percent += 10;
        } else if (
            direction < 0 &&
            ui_model->display_brightness_percent > 10) {
            ui_model->display_brightness_percent -= 10;
        }
        break;

    case 1:
        if (direction > 0 &&
            ui_model->screen_timeout_seconds < 120) {
            ui_model->screen_timeout_seconds += 15;
        } else if (
            direction < 0 &&
            ui_model->screen_timeout_seconds > 15) {
            ui_model->screen_timeout_seconds -= 15;
        }
        break;

    case 2:
        if (direction > 0 &&
            ui_model->status_refresh_interval_seconds < 10) {
            ui_model->status_refresh_interval_seconds++;
        } else if (
            direction < 0 &&
            ui_model->status_refresh_interval_seconds > 1) {
            ui_model->status_refresh_interval_seconds--;
        }
        break;

    default:
        break;
    }
}



static void handleDisplayKey(UiKey key)
{
    if (display_editing) {
        switch (key) {
        case UI_KEY_LEFT:
            adjustDisplaySetting(-1);
            drawDisplayPage();
            break;

        case UI_KEY_RIGHT:
            adjustDisplaySetting(1);
            drawDisplayPage();
            break;

        case UI_KEY_ENTER:
        case UI_KEY_BACK:
            display_editing = false;
            drawDisplayPage();
            break;

        default:
            break;
        }

        return;
    }

    switch (key) {
    case UI_KEY_UP:
        display_selected--;

        if (display_selected < 0)
            display_selected = DISPLAY_SETTING_COUNT - 1;

        drawDisplayPage();
        break;

    case UI_KEY_DOWN:
        display_selected++;

        if (display_selected >= DISPLAY_SETTING_COUNT)
            display_selected = 0;

        drawDisplayPage();
        break;

    case UI_KEY_ENTER:
        if (!displaySettingEditable(display_selected)) {
            uiShowError("ADMIN ONLY");
            break;
        }

        display_editing = true;
        drawDisplayPage();
        break;

    case UI_KEY_LEFT:
    case UI_KEY_BACK:
        current_page = PAGE_MENU;
        drawMenu();
        break;

    default:
        break;
    }
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


static void drawSettingsPage(void)
{
    beginPage("SETTINGS");

    gdispDrawString(
        15,
        82,
        "SESSION SETTINGS",
        font_body,
        Yellow);

    drawValue(
        110,
        "CURRENT ROLE",
        roleText(current_role),
        current_role == UI_ROLE_ADMIN ? Yellow : White);

    gdispFillArea(
        15,
        150,
        210,
        42,
        Blue);

    gdispDrawBox(
        15,
        150,
        210,
        42,
        White);

    gdispDrawString(
        28,
        164,
        "LOCK ADMIN SESSION",
        font_body,
        White);

    drawFooter("ENTER: LOCK   LEFT / ESC: BACK");
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

    case PAGE_ADMIN_LOGIN:
        drawAdminLoginPage();
        break;

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
        drawDisplayPage();
        break;

    case 5:
        drawDiagnosticsPage();
        break;

    case 6:
        drawSettingsPage();
        break;

    default:
        break;
    }
}


static void handleAdminLoginKey(UiKey key)
{
    switch (key) {
    case UI_KEY_UP:
        if (pin_digits[pin_cursor] == '9')
            pin_digits[pin_cursor] = '0';
        else
            pin_digits[pin_cursor]++;

        drawAdminLoginPage();
        break;

    case UI_KEY_DOWN:
        if (pin_digits[pin_cursor] == '0')
            pin_digits[pin_cursor] = '9';
        else
            pin_digits[pin_cursor]--;

        drawAdminLoginPage();
        break;

    case UI_KEY_LEFT:
        if (pin_cursor > 0)
            pin_cursor--;

        drawAdminLoginPage();
        break;

    case UI_KEY_RIGHT:
        if (pin_cursor < ADMIN_PIN_LENGTH - 1)
            pin_cursor++;

        drawAdminLoginPage();
        break;

    case UI_KEY_ENTER:
        if (strcmp(pin_digits, admin_pin) == 0) {
            current_role = UI_ROLE_ADMIN;
            current_page = pending_page;
            drawPage();
        } else {
            memcpy(
                pin_digits,
                "0000",
                ADMIN_PIN_LENGTH + 1);

            pin_cursor = 0;
            drawAdminLoginPage();
            uiShowError("INCORRECT PIN");
        }
        break;

    case UI_KEY_BACK:
        pending_page = PAGE_MENU;
        current_page = PAGE_MENU;
        drawMenu();
        break;

    default:
        break;
    }
}



static void lockAdminSession(void)
{
    current_role = UI_ROLE_USER;
    pending_page = PAGE_MENU;
    pin_cursor = 0;

    memcpy(
        pin_digits,
        "0000",
        ADMIN_PIN_LENGTH + 1);

    current_page = PAGE_MENU;
    drawMenu();
}




void uiInit(UiModel *model)
{
    font_body = gdispOpenFont("DejaVuSans12");
    font_status = gdispOpenFont("DejaVuSans10");
    font_menu = gdispOpenFont("DejaVuSans16");

    ui_model = model;
    selected = 0;
    current_page = PAGE_MENU;
    current_role = UI_ROLE_USER;

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
            if (pageAccessForRole(
                    current_role,
                    selected) == UI_ACCESS_DENIED) {
                startAdminLogin(selected);
                break;
            }

            if (selected == 4){
                display_selected = 0;
                display_editing = false;
            }
            current_page = selected;
            drawPage();
            break;

        case UI_KEY_BACK:
            return UI_ACTION_EXIT;

        default:
            break;
        }
    } else if (current_page == PAGE_ADMIN_LOGIN) {
        handleAdminLoginKey(key);
    } else if (current_page == 4) {
        handleDisplayKey(key);
    } else {
        switch (key) {
        case UI_KEY_ENTER:
            if (current_page == 6 &&
                current_role == UI_ROLE_ADMIN) {
                lockAdminSession();
            }
            break;

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