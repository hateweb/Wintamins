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

#include <stdio.h>

#include <windows.h>
#include <process.h>
#include <commctrl.h>

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
		return EXIT_SUCCESS;
	}

	open_log();
	ini_parse();
	license_write();
	autostart();

	if (hide_titlebars)
	{
		EnumWindows(enum_win_proc, 0);
		init_win_event_hk();
	}

	drag_work_ev = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (drag_work_ev == NULL)
	{
		log_msg(STATUS_ERROR, "failed to create drag event");
		goodbye();
		return EXIT_FAILURE;
	}

	unsigned thread_id;
	drag_thread_h = (HANDLE)_beginthreadex(NULL, 0, &drag_thread, NULL, 0, &thread_id);

	if (drag_thread_h == 0)
	{
		log_msg(STATUS_ERROR, "failed to begin drag thread");
		goodbye();
		return EXIT_FAILURE;
	}
	
	init_keyboard_hk();

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

	WNDCLASS wnd_class = {};
	wnd_class.hIcon = icon_light;
	wnd_class.lpfnWndProc = DefWindowProc;
	wnd_class.hInstance = hinstance;
	wnd_class.lpszClassName = name;
	RegisterClass(&wnd_class);

	HWND settings_dlg = CreateDialogParam(hinstance,
		MAKEINTRESOURCE(IDD_SETTINGS), NULL, dlg_proc, (LPARAM)hinstance);
	if (!settings_dlg)
	{
		log_msg(STATUS_ERROR, "L%d -> failed to create dialog", __LINE__);
		goodbye();
		return EXIT_FAILURE;
	}

	ShowWindow(settings_dlg, SW_HIDE);

	HWND hwnd = CreateWindowEx(
		0, name, name, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hinstance, NULL);

	if (!hwnd)
	{
		log_msg(STATUS_ERROR, "L%d -> failed to create window", __LINE__);
		goodbye();
		return EXIT_FAILURE;
	}

	log_msg(STATUS_INFO, "started", __LINE__);

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
