/*
	Wintamins - for healthy window management
	Copyright (C) 2026  hateweb

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; If not, see <http://www.gnu.org/licenses/>.
*/

#include <windows.h>

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <handleapi.h>
#include <libloaderapi.h>
#include <minwindef.h>
#include <processenv.h>
#include <processthreadsapi.h>
#include <securitybaseapi.h>
#include <commctrl.h>
#include <wingdi.h>
#include <winnt.h>
#include <winreg.h>

#include "resources.h"
#include "config.h"
#include "gui.h"
#include "wintamins.h"

int WINAPI WinMain(HINSTANCE hinstance,
	HINSTANCE hprevinstance,
	LPSTR lpcmdline,
	int ncmdshow)
{
	UNREFERENCED_PARAMETER(hprevinstance);
	UNREFERENCED_PARAMETER(ncmdshow);

	if (lpcmdline && strlen(lpcmdline) > 0 && !strcmp(lpcmdline, "--elevated"))
	{
		elevate();
		return 0;
	}

	AllocConsole();
	console_wnd = GetConsoleWindow();
	if (!console_wnd)
	{
		printf("L%d -> failed to allocate console\n", __LINE__);
		return 1;
	}

	ShowWindow(console_wnd, SW_HIDE);
	SetWindowText(console_wnd, contitle);

	HMENU menu = GetSystemMenu(console_wnd, FALSE);
	if (!menu)
	{
		printf("L%d -> failed to get console menu\n", __LINE__);
		return 1;
	}

	DeleteMenu(menu, SC_CLOSE, MF_BYCOMMAND);

	freopen("CONIN$", "r", stdin);
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);

	ini_parse();
	license_write();

	autostart();

	if (hide_titlebars || no_thickframe)
	{
		EnumWindows(enum_win_proc, 0);
		init_win_event_hk();
	}

	HHOOK hk_keyboard =
		SetWindowsHookEx(WH_KEYBOARD_LL, keyboard_proc, NULL, 0);
	if (!hk_keyboard)
	{
		printf("L%d -> failed to initialize keyboard hook", __LINE__);
		return 1;
	}

	INITCOMMONCONTROLSEX iccx;
	iccx.dwSize = sizeof(INITCOMMONCONTROLSEX);
	iccx.dwICC = ICC_LINK_CLASS;
	InitCommonControlsEx(&iccx);

	icon_dark = (HICON)LoadImage(hinstance, MAKEINTRESOURCE(IDI_TRAYDARK),
		IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
		GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR | LR_SHARED);
	icon_light = (HICON)LoadImage(hinstance, MAKEINTRESOURCE(IDI_TRAYLIGHT),
		IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
		GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR | LR_SHARED);

	cursor_drag = LoadCursor(NULL, IDC_SIZEALL);
	cursor_resize_tl_br = LoadCursor(NULL, IDC_SIZENWSE);
	cursor_resize_tr_bl = LoadCursor(NULL, IDC_SIZENESW);

	WNDCLASS wnd_class = {0};
	wnd_class.hIcon = icon_light;
	wnd_class.lpfnWndProc = DefWindowProc;
	wnd_class.hInstance = hinstance;
	wnd_class.lpszClassName = name;
	RegisterClass(&wnd_class);

	HWND settings_dlg = CreateDialogParamA(hinstance,
		MAKEINTRESOURCE(IDD_SETTINGS), NULL, dlg_proc, (LPARAM)hinstance);
	if (!settings_dlg)
	{
		printf("L%d -> failed to create dialog\n", __LINE__);
		return 1;
	}

	ShowWindow(settings_dlg, SW_HIDE);

	HWND hwnd = CreateWindowEx(
		0, name, name, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hinstance, NULL);

	if (!hwnd)
	{
		printf("L%d -> failed to create window\n", __LINE__);
		return 1;
	}

	puts("initialized");

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if ((!tabs[current_tab].hwnd ||
				!IsDialogMessage(tabs[current_tab].hwnd, &msg)) &&
			!IsDialogMessage(settings_dlg, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	goodbye();
	return (int)msg.wParam;
}
