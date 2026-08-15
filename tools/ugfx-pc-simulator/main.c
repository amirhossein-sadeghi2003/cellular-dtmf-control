#include "gfx.h"

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

static void draw_header(void)
{
    gdispFillArea(0, 0, 240, 35, Blue);
    gdispDrawString(10, 10, "CELLULAR CONTROL", font, White);
}

static void draw_footer(const char *text)
{
    gdispDrawLine(10, 285, 230, 285, Gray);
    gdispDrawString(10, 295, text, font, Gray);
}

static void draw_menu(int selected)
{
    int i;
    int y;

    gdispClear(Black);
    draw_header();

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

    draw_footer("ENTER: OPEN   ESC: EXIT");
}

static void begin_page(const char *title)
{
    gdispClear(Black);
    draw_header();

    gdispDrawString(10, 50, title, font, Cyan);
    gdispDrawLine(10, 70, 230, 70, Gray);
}

static void draw_value(
    int y,
    const char *label,
    const char *value,
    color_t value_color)
{
    gdispDrawString(15, y, label, font, White);
    gdispDrawString(115, y, value, font, value_color);
}

static void draw_status_page(void)
{
    begin_page("STATUS");

    gdispDrawString(15, 82, "SIMULATED DATA", font, Yellow);

    draw_value(110, "MODEM", "READY", Green);
    draw_value(140, "NETWORK", "REGISTERED", Green);
    draw_value(170, "CALL", "IDLE", Green);
    draw_value(200, "SIGNAL", "26 / 31", Green);

    draw_footer("LEFT / ESC: BACK");
}

static void draw_call_page(void)
{
    begin_page("CALL");

    gdispDrawString(15, 82, "SIMULATED DATA", font, Yellow);

    draw_value(110, "STATE", "IDLE", Green);
    draw_value(140, "AUTO ANSWER", "ON", Green);
    draw_value(170, "CALLER", "-", White);
    draw_value(200, "DURATION", "00:00", White);

    draw_footer("LEFT / ESC: BACK");
}

static void draw_dtmf_page(void)
{
    begin_page("DTMF");

    gdispDrawString(15, 82, "SIMULATED DATA", font, Yellow);

    draw_value(110, "DETECTION", "ENABLED", Green);
    draw_value(140, "LAST KEY", "-", White);
    draw_value(170, "BUFFER", "-", White);
    draw_value(200, "SOURCE", "KEYBOARD", White);

    draw_footer("LEFT / ESC: BACK");
}

static void draw_network_page(void)
{
    begin_page("NETWORK");

    gdispDrawString(15, 82, "SIMULATED DATA", font, Yellow);

    draw_value(110, "REGISTRATION", "HOME", Green);
    draw_value(140, "SIGNAL", "26 / 31", Green);
    draw_value(170, "TECHNOLOGY", "GSM", White);
    draw_value(200, "OPERATOR", "-", White);

    draw_footer("LEFT / ESC: BACK");
}

static void draw_placeholder_page(const char *title)
{
    begin_page(title);

    gdispDrawString(15, 105, "This page is not", font, White);
    gdispDrawString(15, 130, "implemented yet.", font, White);

    draw_footer("LEFT / ESC: BACK");
}

static void draw_page(int page)
{
    switch (page) {
    case 0:
        draw_status_page();
        break;

    case 1:
        draw_call_page();
        break;

    case 2:
        draw_dtmf_page();
        break;

    case 3:
        draw_network_page();
        break;

    case 4:
    case 5:
    case 6:
        draw_placeholder_page(menu_items[page]);
        break;

    default:
        break;
    }
}

int main(void)
{
    int selected = 0;
    int current_page = PAGE_MENU;

    GListener listener;
    GSourceHandle keyboard;

    GEvent *event;
    GEventKeyboard *key_event;

    gU8 key;

    gfxInit();

    font = gdispOpenFont("UI2");

    draw_menu(selected);

    keyboard = ginputGetKeyboard(0);

    if (!keyboard) {
        gdispDrawString(10, 255, "KEYBOARD ERROR", font, Red);

        while (1)
            gfxSleepMilliseconds(1000);
    }

    geventListenerInit(&listener);

    if (!geventAttachSource(
            &listener,
            keyboard,
            GLISTEN_KEYREPEATSOFF)) {

        gdispDrawString(10, 255, "EVENT ERROR", font, Red);

        while (1)
            gfxSleepMilliseconds(1000);
    }

    while (1) {
        event = geventEventWait(&listener, gDelayForever);

        if (!event)
            continue;

        if (event->type != GEVENT_KEYBOARD)
            continue;

        key_event = (GEventKeyboard *)event;

        if ((key_event->keystate & GKEYSTATE_KEYUP) ||
            !key_event->bytecount) {
            continue;
        }

        key = (gU8)key_event->c[0];

        if (current_page == PAGE_MENU) {
            switch (key) {
            case GKEY_DOWN:
                selected++;

                if (selected >= MENU_COUNT)
                    selected = 0;

                draw_menu(selected);
                break;

            case GKEY_UP:
                selected--;

                if (selected < 0)
                    selected = MENU_COUNT - 1;

                draw_menu(selected);
                break;

            case GKEY_ENTER:
                current_page = selected;
                draw_page(current_page);
                break;

            case GKEY_ESC:
                return 0;

            default:
                break;
            }
        } else {
            switch (key) {
            case GKEY_LEFT:
            case GKEY_ESC:
                current_page = PAGE_MENU;
                draw_menu(selected);
                break;

            default:
                break;
            }
        }
    }
}