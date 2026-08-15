#include "gfx.h"
#include "ui.h"

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
static int selected;
static int current_page;

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
    beginPage("STATUS");

    gdispDrawString(15, 82, "SIMULATED DATA", font, Yellow);

    drawValue(110, "MODEM", "READY", Green);
    drawValue(140, "NETWORK", "REGISTERED", Green);
    drawValue(170, "CALL", "IDLE", Green);
    drawValue(200, "SIGNAL", "26 / 31", Green);

    drawFooter("LEFT / ESC: BACK");
}

static void drawCallPage(void)
{
    beginPage("CALL");

    gdispDrawString(15, 82, "SIMULATED DATA", font, Yellow);

    drawValue(110, "STATE", "IDLE", Green);
    drawValue(140, "AUTO ANSWER", "ON", Green);
    drawValue(170, "CALLER", "-", White);
    drawValue(200, "DURATION", "00:00", White);

    drawFooter("LEFT / ESC: BACK");
}

static void drawDtmfPage(void)
{
    beginPage("DTMF");

    gdispDrawString(15, 82, "SIMULATED DATA", font, Yellow);

    drawValue(110, "DETECTION", "ENABLED", Green);
    drawValue(140, "LAST KEY", "-", White);
    drawValue(170, "BUFFER", "-", White);
    drawValue(200, "SOURCE", "KEYBOARD", White);

    drawFooter("LEFT / ESC: BACK");
}

static void drawNetworkPage(void)
{
    beginPage("NETWORK");

    gdispDrawString(15, 82, "SIMULATED DATA", font, Yellow);

    drawValue(110, "REGISTRATION", "HOME", Green);
    drawValue(140, "SIGNAL", "26 / 31", Green);
    drawValue(170, "TECHNOLOGY", "GSM", White);
    drawValue(200, "OPERATOR", "-", White);

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
    case 5:
    case 6:
        drawPlaceholderPage(menu_items[current_page]);
        break;

    default:
        break;
    }
}

void uiInit(void)
{
    font = gdispOpenFont("UI2");
    selected = 0;
    current_page = PAGE_MENU;

    drawMenu();
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