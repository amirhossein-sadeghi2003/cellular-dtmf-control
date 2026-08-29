#ifndef UI_H
#define UI_H

#include "ui_model.h"

typedef enum {
    UI_KEY_NONE = 0,
    UI_KEY_UP,
    UI_KEY_DOWN,
    UI_KEY_LEFT,
    UI_KEY_RIGHT,
    UI_KEY_ENTER,
    UI_KEY_BACK
} UiKey;

typedef enum {
    UI_ACTION_NONE = 0,
    UI_ACTION_EXIT
} UiAction;

void uiInit(UiModel *model);
void uiRefresh(void);
void uiRefreshModemStatus(void);
void uiShowError(const char *message);
UiAction uiHandleKey(UiKey key);

#endif
