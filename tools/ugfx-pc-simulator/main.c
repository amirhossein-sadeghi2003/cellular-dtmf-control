#include "gfx.h"

#define MENU_COUNT 7

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

static void draw_menu(int selected)
{
    int i;
    int y;

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
}

static void draw_selected(const char *text)
{
    gdispFillArea(10, 250, 220, 30, Black);
    gdispDrawString(10, 255, text, font, Green);
}

int main(void)
{
    int selected = 0;

    GListener listener;
    GSourceHandle keyboard;

    GEvent *event;
    GEventKeyboard *key_event;

    gU8 key;

    gfxInit();

    font = gdispOpenFont("UI2");

    gdispClear(Black);

    draw_header();
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
            draw_selected(menu_items[selected]);
            break;

        case GKEY_ESC:
            return 0;

        default:
            break;
        }
    }
}
