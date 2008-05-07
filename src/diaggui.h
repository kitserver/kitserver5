// setupgui.h

#ifndef _DIAGGUI
#define _DIAGGUI

#include <windows.h>

#define WIN_WIDTH 400
#define WIN_HEIGHT 400

extern HWND g_saveButtonControl;

extern HWND g_statusTextControl;           // displays status messages

// functions
bool BuildControls(HWND parent);

#endif
