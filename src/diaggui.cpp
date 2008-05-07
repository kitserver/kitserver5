#include <windows.h>
#include <stdio.h>
#include "diaggui.h"

HWND g_saveButtonControl;

HWND g_statusTextControl;           // displays status messages

/**
 * build all controls
 */
bool BuildControls(HWND parent)
{
	HGDIOBJ hObj;
	DWORD style, xstyle;
	int x, y, spacer;
	int boxW, boxH, statW, statH, borW, borH, butW, butH;

	spacer = 5; 
	x = spacer, y = spacer;
	butH = 20;

	// use default extended style
	xstyle = WS_EX_LEFT;
	style = WS_CHILD | WS_VISIBLE;

	// TOP SECTION
	borW = WIN_WIDTH - spacer*3;
	statW = 120;
	boxW = borW - statW - spacer*3; boxH = 20; statH = 16;
	borH = spacer*3 + boxH*2;

	HWND staticBorderTopControl = CreateWindowEx(
			xstyle, "Static", "", WS_CHILD | WS_VISIBLE | SS_ETCHEDFRAME,
			x, y, borW, borH,
			parent, NULL, NULL, NULL);
			
	x+=spacer;
	
	HWND topLabel = CreateWindowEx(
			xstyle, "Static", "Game executable:", style,
			x, y+4, statW, statH, 
			parent, NULL, NULL, NULL);

	// BOTTOM SECTION - button

	butW = 80;
	butH = 25;
	x = 400 - butW - spacer - 3;
	y = 400 - butH - spacer - 30;

	g_saveButtonControl = CreateWindowEx(
			xstyle, "Button", "Save report", style | WS_TABSTOP,
			x, y, butW, butH,
			parent, NULL, NULL, NULL);

	x = spacer;
	statW = WIN_WIDTH - spacer*4 - 166;

	g_statusTextControl = CreateWindowEx(
			xstyle, "Static", "", style,
			x, y+6, statW, statH,
			parent, NULL, NULL, NULL);

	// If any control wasn't created, return false
	if (topLabel == NULL ||
		g_statusTextControl == NULL ||
		g_saveButtonControl == NULL)
		return false;

	// Get a handle to the stock font object
	hObj = GetStockObject(DEFAULT_GUI_FONT);

	SendMessage(g_statusTextControl, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_saveButtonControl, WM_SETFONT, (WPARAM)hObj, true);

	// enable the button by default
	EnableWindow(g_saveButtonControl, true);

	return true;
}

