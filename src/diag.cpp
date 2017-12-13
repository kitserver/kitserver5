/* KitServer 5 Diagnostics */

#include <windows.h>
#include <windef.h>
#include <string.h>
#include <stdio.h>

#define BUFLEN 4096

#include "diag.h"
#include "diaggui.h"

HWND hWnd = NULL;


void MyMessageBox(char* fmt, DWORD value);
void MyMessageBox2(char* fmt, char* value);

void MyMessageBox(char* fmt, DWORD value)
{
	/*
	// show message box with error msg
	char buf[BUFLEN];
	ZeroMemory(buf, BUFLEN);
	sprintf(buf, fmt, value);
	MessageBox(hWnd, buf, "KitServer 5 DEBUG MyMessage", 0);
	*/
}

void MyMessageBox2(char* fmt, char* value)
{
	/*
	// show message box with error msg
	char buf[BUFLEN];
	ZeroMemory(buf, BUFLEN);
	sprintf(buf, fmt, value);
	MessageBox(hWnd, buf, "KitServer 5 DEBUG MyMessage", 0);
	*/
}


/**
 * Initializes all controls
 */
void InitControls(void)
{
	return;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int home, away, timecode;
	char buf[BUFLEN];

	switch(uMsg)
	{
		case WM_DESTROY:
			// Exit the application when the window closes
			PostQuitMessage(1);
			return true;

		case WM_COMMAND:
			if (HIWORD(wParam) == BN_CLICKED)
			{
				if ((HWND)lParam == g_saveButtonControl)
				{
					//InstallKserv();
				}
			}
			break;
	}
	return DefWindowProc(hwnd,uMsg,wParam,lParam);
}

bool InitApp(HINSTANCE hInstance, LPSTR lpCmdLine)
{
	WNDCLASSEX wcx;

	// cbSize - the size of the structure.
	wcx.cbSize = sizeof(WNDCLASSEX);
	wcx.style = CS_HREDRAW | CS_VREDRAW;
	wcx.lpfnWndProc = (WNDPROC)WindowProc;
	wcx.cbClsExtra = 0;
	wcx.cbWndExtra = 0;
	wcx.hInstance = hInstance;
	wcx.hIcon = LoadIcon(NULL,IDI_APPLICATION);
	wcx.hCursor = LoadCursor(NULL,IDC_ARROW);
	wcx.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wcx.lpszMenuName = NULL;
	wcx.lpszClassName = "DIAGCLS";
	wcx.hIconSm = NULL;

	// Register the class with Windows
	if(!RegisterClassEx(&wcx))
		return false;

	return true;
}

HWND BuildWindow(int nCmdShow)
{
	DWORD style, xstyle;
	HWND retval;

	style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
	xstyle = WS_EX_LEFT;

	retval = CreateWindowEx(xstyle,
        "DIAGCLS",      // class name
        DIAG_WINDOW_TITLE, // title for our window (appears in the titlebar)
        style,
        CW_USEDEFAULT,  // initial x coordinate
        CW_USEDEFAULT,  // initial y coordinate
        WIN_WIDTH, WIN_HEIGHT,   // width and height of the window
        NULL,           // no parent window.
        NULL,           // no menu
        NULL,           // no creator
        NULL);          // no extra data

	if (retval == NULL) return NULL;  // BAD.

	ShowWindow(retval,nCmdShow);  // Show the window
	return retval; // return its handle for future use.
}

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
	MSG msg; int retval;

 	if(InitApp(hInstance, lpCmdLine) == false)
		return 0;

	hWnd = BuildWindow(nCmdShow);
	if(hWnd == NULL)
		return 0;

	// build GUI
	if (!BuildControls(hWnd))
		return 0;

	// Initialize all controls
	InitControls();

	// show credits
	char buf[BUFLEN];
	ZeroMemory(buf, BUFLEN);
	strncpy(buf, CREDITS, BUFLEN-1);
	SendMessage(g_statusTextControl, WM_SETTEXT, 0, (LPARAM)buf);

	while((retval = GetMessage(&msg,NULL,0,0)) != 0)
	{
		if(retval == -1)
			return 0;	// an error occured while getting a message

		if (!IsDialogMessage(hWnd, &msg)) // need to call this to make WS_TABSTOP work
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return 0;
}

