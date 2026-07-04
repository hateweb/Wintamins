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

#define OEMRESOURCE
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

#define WM_TRAYICON (WM_APP + 1)

#define IN_NONE 0
#define IN_DRAG 1
#define IN_RESIZE 2

typedef struct
{
	HWND hwnd;
	LONG_PTR original_style;
} modwnd;

#define MAX_TRACKED_WINDOWS 256
modwnd tracked_wnds[MAX_TRACKED_WINDOWS];
int tracked_count = 0;

#define MAX_PAGES 4
HWND tabs[MAX_PAGES];
uint8_t current_tab = 0;

const char name[] = "Wintamins";
const char contitle[] = "Wintamins Console";

enum
{
	CORNER_TOPLEFT = 4,
	CORNER_TOPRIGHT,
	CORNER_BOTTOMLEFT = 7,
	CORNER_BOTTOMRIGHT,
};

HICON icon_dark;
HICON icon_light;

HCURSOR cursor_drag;
HCURSOR cursor_resize_tl_br;
HCURSOR cursor_resize_tr_bl;

static const DWORD cursors[] = {
    OCR_NORMAL, OCR_IBEAM, OCR_WAIT, OCR_CROSS, 
    OCR_UP, OCR_SIZEALL, OCR_SIZENS, OCR_SIZEWE, OCR_HAND
};
static const int cursor_amt = sizeof(cursors) / sizeof(cursors[0]);

static HFONT title_font;

NOTIFYICONDATA tray_data;
HWINEVENTHOOK hk_win_ev;

bool console_state = false;
HWND console_wnd;

uint8_t state = 0;
bool drag_occurred = false;
bool mod_active = false;
bool was_maximized = false;

HWND target_wnd;
POINT mouse_start;
RECT window_start;

HHOOK hk_mouse;

const char* whitelist[] = {"Progman", "WorkerW", "Shell_TrayWnd",
	"Shell_SecondaryTrayWnd", "Windows.UI.Core.CoreWindow",
	"EdgeUiInputWndClass", "NotifyIconOverflowWindow"};

int wl_size = sizeof(whitelist) / sizeof(whitelist[0]);

bool compare(HWND hwnd)
{
	char class[256];
	GetClassName(hwnd, class, sizeof(class));

	for (int i = 0; i < wl_size; i++)
		if (strcmp(whitelist[i], class) == 0)
			return true;

	return false;
}

void hello(HWND hwnd)
{
	if (tracked_count >= MAX_TRACKED_WINDOWS || !hwnd || !IsWindow(hwnd) ||
		!IsWindowVisible(hwnd) || GetAncestor(hwnd, GA_ROOT) != hwnd)
		return;

	// causes run dialog to be ignored
	// if (GetParent(hwnd))
	//   return;

	LONG_PTR exstyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
	if ((exstyle & WS_EX_TOOLWINDOW) || (exstyle & WS_EX_NOACTIVATE))
		return;

	LONG_PTR original_style = GetWindowLongPtr(hwnd, GWL_STYLE);
	if (original_style & WS_CHILD)
		return;

	if (GetWindowTextLength(hwnd) == 0)
		return;

	char class[256];
	GetClassName(hwnd, class, sizeof(class));

	if (compare(hwnd))
		return;

	LONG_PTR style_override = original_style;

	if (hide_titlebars)
		style_override &= ~WS_CAPTION;

	if (no_thickframe)
	{
		style_override &= ~WS_THICKFRAME;
		style_override |= WS_BORDER;
	}

	if (style_override == original_style)
		return;

	tracked_wnds[tracked_count].hwnd = hwnd;
	tracked_wnds[tracked_count].original_style = original_style;
	tracked_count++;

	SetWindowLongPtr(hwnd, GWL_STYLE, style_override);
	SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
			SWP_FRAMECHANGED | SWP_ASYNCWINDOWPOS);
}

void adjust_wnd_rect(HWND hwnd,
	LONG_PTR from_style,
	LONG_PTR to_style,
	int* d_left,
	int* d_top,
	int* d_right,
	int* d_bottom)
{
	RECT rect_from = {0, 0, 100, 100};
	RECT rect_to = {0, 0, 100, 100};

	LONG_PTR ex_style = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
	BOOL has_menu = GetMenu(hwnd) != NULL;

	AdjustWindowRectEx(&rect_from, (DWORD)from_style, has_menu, (DWORD)ex_style);
	AdjustWindowRectEx(&rect_to, (DWORD)to_style, has_menu, (DWORD)ex_style);

	*d_left = rect_to.left - rect_from.left;
	*d_top = rect_to.top - rect_from.top;
	*d_right = rect_to.right - rect_from.right;
	*d_bottom = rect_to.bottom - rect_from.bottom;
}

void restore()
{
	for (int i = 0; i < tracked_count; i++)
	{
		HWND hwnd = tracked_wnds[i].hwnd;
		if (!IsWindow(hwnd))
			continue;

		LONG_PTR original_style = tracked_wnds[i].original_style;

		// the weird size thing doesn't affect minimized/maximized apps.
		if (IsZoomed(hwnd) || IsIconic(hwnd))
		{
			SetWindowLongPtr(hwnd, GWL_STYLE, original_style);
			SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
				SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
					SWP_FRAMECHANGED | SWP_ASYNCWINDOWPOS);
		}
		else
		{
			LONG_PTR current_style = GetWindowLongPtr(hwnd, GWL_STYLE);

			RECT rect;
			GetWindowRect(hwnd, &rect);

			int d_left = 0;
			int d_top = 0;
			int d_right = 0;
			int d_bottom = 0;
			adjust_wnd_rect(hwnd, current_style, original_style, &d_left,
				&d_top, &d_right, &d_bottom);

			int new_x = rect.left + d_left;
			int new_y = rect.top + d_top;
			int new_w = (rect.right - rect.left) + (d_right - d_left);
			int new_h = (rect.bottom - rect.top) + (d_bottom - d_top);

			SetWindowLongPtr(hwnd, GWL_STYLE, original_style);
			SetWindowPos(hwnd, NULL, new_x, new_y, new_w, new_h,
				SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED |
					SWP_ASYNCWINDOWPOS);
		}
	}

	tracked_count = 0;
}

void goodbye()
{
	restore();

	// should be checked if it exists even...
	Shell_NotifyIcon(NIM_DELETE, &tray_data);

	if (hk_win_ev)
		UnhookWinEvent(hk_win_ev);
}

void winkey()
{
	INPUT inputs[2] = {0};
	inputs[0].type = INPUT_KEYBOARD;
	inputs[0].ki.wVk = 0xE8;  // unassigned
	inputs[0].ki.dwFlags = 0;

	inputs[1].type = INPUT_KEYBOARD;
	inputs[1].ki.wVk = VK_LMENU;
	inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

	SendInput(2, inputs, sizeof(INPUT));
}

bool elevate()
{
	char path[MAX_PATH];
	if (GetModuleFileNameA(NULL, path, MAX_PATH) == 0)
		return false;

	bool elevated = false;
	HANDLE token;
	TOKEN_ELEVATION elevation;
	DWORD sz = sizeof(TOKEN_ELEVATION);
	if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
	{
		if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &sz))
			elevated = elevation.TokenIsElevated;

		CloseHandle(token);
	}

	if (elevated)
		return false;

	SHELLEXECUTEINFOA info = {};
	info.cbSize = sizeof(info);
	info.lpVerb = "runas";
	info.lpFile = path;
	info.nShow = SW_NORMAL;

	if (!ShellExecuteExA(&info))
	{
		printf("L%d -> failed to elevate\n", __LINE__);
		return false;
	}
	else
	{
		goodbye();
		PostQuitMessage(0);
		return true;
	}
}

void autostart()
{
	HKEY key;
	if (!add_to_autostart)
	{
		RegOpenKeyExA(HKEY_CURRENT_USER,
			"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0,
			KEY_SET_VALUE, &key);

		RegDeleteValue(key, name);

		RegCloseKey(key);
		return;
	}

	LONG read_status = RegOpenKeyExA(HKEY_CURRENT_USER,
		"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ,
		&key);

	char path[MAX_PATH];

	if (GetModuleFileNameA(NULL, path, MAX_PATH) == 0)
		return;

	if (autostart_as_admin)
		strcat(path, " --elevated");

	if (read_status == ERROR_SUCCESS)
	{
		char buffer[MAX_PATH];
		DWORD buffer_sz = sizeof(buffer);

		LONG query_status = RegQueryValueExA(
				key, name, NULL, NULL, (LPBYTE)buffer, &buffer_sz);

		if (query_status == ERROR_SUCCESS)
			if (strcmp(path, buffer) == 0)
				return;
	}

	RegCloseKey(key);

	LONG write_status = RegOpenKeyExA(HKEY_CURRENT_USER,
		"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE,
		&key);

	if (write_status != ERROR_SUCCESS)
		return;

	RegSetValueExA(key, name, 0, REG_SZ,
		(const BYTE*)path, (DWORD)(strlen(path) + 1));

	RegCloseKey(key);
}

void CALLBACK handle_win_ev(HWINEVENTHOOK hook,
	DWORD event,
	HWND hwnd,
	LONG idobj,
	LONG idchild,
	DWORD dw_event_thread,
	DWORD dwms_event_time)
{
	UNREFERENCED_PARAMETER(hook);
	UNREFERENCED_PARAMETER(event);
	UNREFERENCED_PARAMETER(dw_event_thread);
	UNREFERENCED_PARAMETER(dwms_event_time);

	if (idobj != OBJID_WINDOW || idchild != CHILDID_SELF)
		return;

	hello(hwnd);
}

void init_win_event_hook()
{
	hk_win_ev = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW, NULL,
		&handle_win_ev, 0, 0,
		WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

	if (!hk_win_ev)
		printf("L%d -> failed to initialize event hook", __LINE__);
}

BOOL CALLBACK enum_win_proc(HWND hwnd, LPARAM lparam)
{
	UNREFERENCED_PARAMETER(lparam);

	hello(hwnd);
	return TRUE;
}

bool get_unmax_bounds(HWND hwnd, RECT* rect_out)
{
	if (!hwnd || !rect_out)
		return false;

	WINDOWPLACEMENT wp = {0};
	wp.length = sizeof(WINDOWPLACEMENT);

	if (!GetWindowPlacement(hwnd, &wp))
		return false;

	LONG_PTR exstyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
	if (exstyle & WS_EX_TOOLWINDOW)
	{
		*rect_out = wp.rcNormalPosition;
		return true;
	}

	// "workspace" coords to monitor coords
	HMONITOR monitor =
		MonitorFromRect(&wp.rcNormalPosition, MONITOR_DEFAULTTONEAREST);
	if (!monitor)
	{
		*rect_out = wp.rcNormalPosition;
		return true;
	}

	MONITORINFO mi = {0};
	mi.cbSize = sizeof(MONITORINFO);
	if (!GetMonitorInfo(monitor, &mi))
	{
		*rect_out = wp.rcNormalPosition;
		return true;
	}

	int dx = mi.rcWork.left - mi.rcMonitor.left;
	int dy = mi.rcWork.top - mi.rcMonitor.top;

	rect_out->left = wp.rcNormalPosition.left + dx;
	rect_out->top = wp.rcNormalPosition.top + dy;
	rect_out->right = wp.rcNormalPosition.right + dx;
	rect_out->bottom = wp.rcNormalPosition.bottom + dy;

	return true;
}

void set_cursor(HCURSOR* cur)
{
	for (int i = 0; i < cursor_amt; i++)
	{
		HCURSOR cur_copy = CopyCursor(*cur);
		if (cur_copy)
			SetSystemCursor(cur_copy, cursors[i]);
	}
}

void click_logic(int option)
{
	if (option == ACTION_DRAG)
	{
		state = IN_DRAG;
		if (IsZoomed(target_wnd))
			was_maximized = true;

		set_cursor(&cursor_drag);
	}
	else if (option == ACTION_RESIZE)
	{
		int dir = CORNER_BOTTOMRIGHT;
		HCURSOR* cur = &cursor_resize_tl_br;
		if (closest_corner_on_resize)
		{
			double d_tl = (double)(mouse_start.x - window_start.left) *
							  (mouse_start.x - window_start.left) +
						  (double)(mouse_start.y - window_start.top) *
							  (mouse_start.y - window_start.top);

			double d_tr = (double)(mouse_start.x - window_start.right) *
							  (mouse_start.x - window_start.right) +
						  (double)(mouse_start.y - window_start.top) *
							  (mouse_start.y - window_start.top);

			double d_bl = (double)(mouse_start.x - window_start.left) *
							  (mouse_start.x - window_start.left) +
						  (double)(mouse_start.y - window_start.bottom) *
							  (mouse_start.y - window_start.bottom);

			double d_br = (double)(mouse_start.x - window_start.right) *
							  (mouse_start.x - window_start.right) +
						  (double)(mouse_start.y - window_start.bottom) *
							  (mouse_start.y - window_start.bottom);

			double closest = d_tl;
			dir = CORNER_TOPLEFT;

			if (d_tr < closest)
			{
				closest = d_tr;
				dir = CORNER_TOPRIGHT;
				cur = &cursor_resize_tr_bl;
			}
			if (d_bl < closest)
			{
				closest = d_bl;
				dir = CORNER_BOTTOMLEFT;
				cur = &cursor_resize_tr_bl;
			}
			if (d_br < closest)
			{
				closest = d_br;
				dir = CORNER_BOTTOMRIGHT;
				cur = &cursor_resize_tl_br;
			}
		}

		state = IN_RESIZE;

		if (snap_cursor_on_resize)
		{
			int x = (dir == CORNER_TOPLEFT || dir == CORNER_BOTTOMLEFT)
						? window_start.left
						: window_start.right;
			int y = (dir == CORNER_TOPLEFT || dir == CORNER_TOPRIGHT)
						? window_start.top
						: window_start.bottom;

			SetCursorPos(x, y);
		}

		PostMessage(target_wnd, WM_SYSCOMMAND, SC_SIZE + dir, 0);
		set_cursor(cur);
	}
	else if (option == ACTION_MAXIMIZE)
		ShowWindow(
			target_wnd, IsZoomed(target_wnd) ? SW_RESTORE : SW_MAXIMIZE);

	else if (option == ACTION_MINIMIZE)
		ShowWindow(target_wnd, SW_MINIMIZE);

	else if (option == ACTION_BRINGDOWN)
		SetWindowPos(target_wnd, HWND_BOTTOM, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

	else if (option == ACTION_CLOSE)
		SendMessage(target_wnd, WM_CLOSE, 0, 0);
}

void release_logic(int option, MSLLHOOKSTRUCT* mouse_struct)
{
	if (option == ACTION_RESIZE && state == IN_RESIZE)
	{
		POINT pt = mouse_struct->pt;
		PostMessage(target_wnd, WM_LBUTTONUP, 0, MAKELPARAM(pt.x, pt.y));
	}

	state = IN_NONE;
	was_maximized = false;
	target_wnd = NULL;

	SystemParametersInfo(SPI_SETCURSORS, 0, NULL, 0);
}

bool process_clicks(WPARAM* p_wparam, MSLLHOOKSTRUCT* mouse_struct)
{
	WPARAM wparam = *p_wparam;
	POINT pt = mouse_struct->pt;
	HWND hwnd = WindowFromPoint(pt);

	if (!hwnd)
		return false;

	HWND root_hwnd = GetAncestor(hwnd, GA_ROOT);
	HWND desktop_hwnd = GetDesktopWindow();

	if (!root_hwnd || root_hwnd == desktop_hwnd || compare(root_hwnd))
		return false;

	WINDOWPLACEMENT wp;
	wp.length = sizeof(WINDOWPLACEMENT);

	if (!GetWindowPlacement(root_hwnd, &wp) || state != IN_NONE)
		return false;

	target_wnd = root_hwnd;

	if (focus_window_on_drag)
		SetForegroundWindow(root_hwnd);

	mouse_start = pt;
	GetWindowRect(target_wnd, &window_start);

	// this is so bad...
	if (wparam == WM_LBUTTONDOWN || wparam == WM_LBUTTONDBLCLK)
	{
		// ReleaseCapture();
		//
		// // Get the exact screen coordinates where this mouse message
		// // occurred
		// DWORD dwPos = GetMessagePos();
		//
		// // Pass the actual coordinates instead of 0
		// SendMessage(target_wnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
		// SendMessage(target_wnd, WM_NCLBUTTONUP, HTCAPTION, 0);
		click_logic(action_lmb);
	}
	else if (wparam == WM_MBUTTONDOWN)
		click_logic(action_mmb);
	else if (wparam == WM_RBUTTONDOWN)
		click_logic(action_rmb);
	else if (wparam == WM_XBUTTONDOWN)
	{
		WORD xbutton = HIWORD(mouse_struct->mouseData);
		if (xbutton == XBUTTON1)
			click_logic(action_m4);
		else if (xbutton == XBUTTON2)
			click_logic(action_m5);
	}
	else if (wparam == WM_MOUSEWHEEL)
	{
		// WORD deltascroll = HIWORD(mouse_struct->mouseData);
		// if deltascroll > 0...
	}

	drag_occurred = true;

	// make sure the click doesn't get sent
	return true;
}

void drag(MSLLHOOKSTRUCT* mouse_struct)
{
	POINT pt = mouse_struct->pt;

	int dx = pt.x - mouse_start.x;
	int dy = pt.y - mouse_start.y;

	int x, y;

	if (was_maximized)
	{
		ShowWindow(target_wnd, SW_RESTORE);
		RECT rect_out;
		get_unmax_bounds(target_wnd, &rect_out);

		x = pt.x - (rect_out.right - rect_out.left) / 2;
		y = pt.y - (rect_out.bottom - rect_out.top) / 2;
	}
	else
	{
		x = window_start.left + dx;
		y = window_start.top + dy;
	}

	SetWindowPos(target_wnd, NULL, x, y, 0, 0,
		SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS |
			SWP_DEFERERASE);
}

bool any_key_down(WPARAM wparam)
{
	return (wparam == WM_LBUTTONDOWN && action_lmb != ACTION_NONE) ||
		   (wparam == WM_MBUTTONDOWN && action_mmb != ACTION_NONE) ||
		   (wparam == WM_RBUTTONDOWN && action_rmb != ACTION_NONE) ||
		   (wparam == WM_XBUTTONDOWN &&
			   (action_m4 != ACTION_NONE || action_m5 != ACTION_NONE));
}

bool any_key_up(WPARAM wparam)
{
	return (wparam == WM_LBUTTONUP && action_lmb != ACTION_NONE) ||
		   (wparam == WM_MBUTTONUP && action_mmb != ACTION_NONE) ||
		   (wparam == WM_RBUTTONUP && action_rmb != ACTION_NONE) ||
		   (wparam == WM_XBUTTONUP &&
			   (action_m4 != ACTION_NONE || action_m5 != ACTION_NONE));
}

LRESULT CALLBACK mouse_proc(int ncode, WPARAM wparam, LPARAM lparam)
{
	if (ncode < HC_ACTION)
		return CallNextHookEx(hk_mouse, ncode, wparam, lparam);

	MSLLHOOKSTRUCT* mouse_struct = (MSLLHOOKSTRUCT*)lparam;
	if (!mouse_struct)
		return CallNextHookEx(hk_mouse, ncode, wparam, lparam);

	if (any_key_down(wparam) && mod_active &&
		process_clicks(&wparam, mouse_struct))
		return 1;

	else if (wparam == WM_MOUSEMOVE && target_wnd &&
			 state == IN_DRAG)
		drag(mouse_struct);

	else if (any_key_up(wparam) && state != IN_NONE)
	{
		if (wparam == WM_LBUTTONUP)
			release_logic(action_lmb, mouse_struct);
		else if (wparam == WM_MBUTTONUP)
			release_logic(action_mmb, mouse_struct);
		else if (wparam == WM_RBUTTONUP)
			release_logic(action_rmb, mouse_struct);
		else if (wparam == WM_XBUTTONUP)
		{
			WORD xbutton = HIWORD(mouse_struct->mouseData);
			if (xbutton == XBUTTON1)
				release_logic(action_m4, mouse_struct);
			else if (xbutton == XBUTTON2)
				release_logic(action_m5, mouse_struct);
		}

		return 1;
	}
	return CallNextHookEx(hk_mouse, ncode, wparam, lparam);
}

LRESULT CALLBACK keyboard_proc(int ncode, WPARAM wparam, LPARAM lparam)
{
	if (ncode == HC_ACTION)
	{
		KBDLLHOOKSTRUCT* pKey = (KBDLLHOOKSTRUCT*)lparam;

		if (pKey->vkCode == (DWORD)modkeys[modifier_key + 1] || (modifier_key2 != 0 ? pKey->vkCode == (DWORD)modkeys[modifier_key2] : false))
		{
			if (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN)
			{
				mod_active = true;

				if (!hk_mouse)
					hk_mouse =
						SetWindowsHookEx(WH_MOUSE_LL, mouse_proc, NULL, 0);

				if (!hk_mouse)
				{
					printf("L%d -> failed to initialize mouse hook",
						__LINE__);
					return 1;
				}
			}

			else if (wparam == WM_KEYUP || wparam == WM_SYSKEYUP)
			{
				mod_active = false;

				UnhookWindowsHookEx(hk_mouse);
				hk_mouse = NULL;

				if (drag_occurred)
				{
					drag_occurred = false;
					winkey();
				}
			}
		}
	}
	return CallNextHookEx(NULL, ncode, wparam, lparam);
}

void tray_menu(HWND hwnd)
{
	POINT pt;
	GetCursorPos(&pt);

	HMENU menu =
		LoadMenu(GetModuleHandle(NULL), MAKEINTRESOURCE(IDM_TRAYMENU));
	if (!menu)
		return;

	HMENU submenu = GetSubMenu(menu, 0);
	if (!submenu)
	{
		DestroyMenu(menu);
		return;
	}

	// wont close if we don't force focus it
	SetForegroundWindow(hwnd);

	TrackPopupMenu(submenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0,
		hwnd, NULL);

	// workaround: send an empty message to force process event queues
	PostMessage(hwnd, WM_NULL, 0, 0);

	DestroyMenu(menu);
}

void switch_tab(int index)
{
	ShowWindow(tabs[current_tab], SW_HIDE);
	current_tab = index;
	ShowWindow(tabs[current_tab], SW_SHOW);
}

BOOL CALLBACK hide_focus(HWND hwnd, LPARAM lparam)
{
	UNREFERENCED_PARAMETER(lparam)
	SendMessage(
		hwnd, WM_UPDATEUISTATE, MAKEWPARAM(UIS_SET, UISF_HIDEFOCUS), 0);
	return TRUE;
}

static HBRUSH backbrush = NULL;
INT_PTR CALLBACK child_dlg_proc(HWND hwnd,
	UINT umsg,
	WPARAM wparam,
	LPARAM lparam)
{
	switch (umsg)
	{
		case WM_INITDIALOG:
		{
			backbrush = CreateSolidBrush(RGB(249, 249, 249));

			EnumChildWindows(hwnd, hide_focus, lparam);
			HWND autostartadmin = GetDlgItem(hwnd, IDC_AUTOSTARTADMIN);
			EnableWindow(autostartadmin,
				IsDlgButtonChecked(tabs[0], IDC_AUTOSTART) == BST_CHECKED);

			HWND title_ctrl = GetDlgItem(hwnd, IDC_TITLE);
			if (title_ctrl)
			{
				title_font = CreateFontA(20, 0, 0, 0, FW_NORMAL, FALSE,
					FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
					CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
					DEFAULT_PITCH | FF_DONTCARE, "Ms Shell Dlg");

				if (title_font)
				{
					SendMessage(
						title_ctrl, WM_SETFONT, (WPARAM)title_font, TRUE);
				}
			}
			return TRUE;
		}

		case WM_NOTIFY:
		{
			LPNMHDR pnmhdr = (LPNMHDR)lparam;
			if (pnmhdr->idFrom == IDC_LINK)
			{
				if (pnmhdr->code == NM_CLICK || pnmhdr->code == NM_RETURN)
				{
					PNMLINK pnmLink = (PNMLINK)lparam;

					// Note: SysLink controls operate internally only in
					// Unicode. The szUrl member is always a wide string
					// (WCHAR), so ShellExecuteW must be used to open it.
					ShellExecuteW(NULL, L"open", pnmLink->item.szUrl, NULL,
						NULL, SW_SHOWNORMAL);
					return TRUE;
				}
			}
			else if (pnmhdr->idFrom == IDC_LICENSE)
			{
				if (pnmhdr->code == NM_CLICK || pnmhdr->code == NM_RETURN)
				{
					// Note: SysLink controls operate internally only in
					// Unicode. The szUrl member is always a wide string
					// (WCHAR), so ShellExecuteW must be used to open it.
					ShellExecuteW(NULL, L"open", L"LICENSE", NULL,
						NULL, SW_SHOWNORMAL);
					return TRUE;
				}
			}
			break;
		}

		case WM_COMMAND:
		{
			if (LOWORD(wparam) == IDB_ELEVATE &&
				HIWORD(wparam) == BN_CLICKED)
			{
				elevate();
				goodbye();
				PostQuitMessage(0);
				return TRUE;
			}
			else if (LOWORD(wparam) == IDC_AUTOSTART &&
					 HIWORD(wparam) == BN_CLICKED)
			{
				HWND autostartadmin = GetDlgItem(hwnd, IDC_AUTOSTARTADMIN);
				EnableWindow(autostartadmin,
					IsDlgButtonChecked(tabs[0], IDC_AUTOSTART) ==
						BST_CHECKED);
			}
		}

		case WM_CTLCOLORDLG:
			return (INT_PTR)backbrush;

		case WM_CTLCOLORSTATIC:
		{
			HDC hdc = (HDC)wparam;
			SetBkMode(hdc, TRANSPARENT);
			return (INT_PTR)backbrush;
		}

		case WM_DESTROY:
			if (backbrush)
			{
				DeleteObject(backbrush);
				backbrush = NULL;
			}

			if (title_font)
			{
				DeleteObject(title_font);
				title_font = NULL;
			}
			break;
	}
	return FALSE;
}

int get_combo_value(HWND tab, int id)
{
	HWND item = GetDlgItem(tab, id);
	return (int)SendMessage(item, CB_GETCURSEL, 0, 0);
}

void set_combo_value(HWND tab, int id, int v)
{
	HWND item = GetDlgItem(tab, id);
	SendMessage(item, CB_SETCURSEL, v, 0);
}

void apply_config()
{
	focus_window_on_drag =
		IsDlgButtonChecked(tabs[0], IDC_FOCUSWINDOW) == BST_CHECKED;
	closest_corner_on_resize =
		IsDlgButtonChecked(tabs[0], IDC_CLOSESTCORNER) == BST_CHECKED;
	snap_cursor_on_resize =
		IsDlgButtonChecked(tabs[0], IDC_SNAPCURSOR) == BST_CHECKED;
	hide_titlebars =
		IsDlgButtonChecked(tabs[0], IDC_HIDEBARS) == BST_CHECKED;
	no_thickframe =
		IsDlgButtonChecked(tabs[0], IDC_NOTHICKFRAME) == BST_CHECKED;
	add_to_autostart = IsDlgButtonChecked(tabs[0], IDC_AUTOSTART) == BST_CHECKED;
	autostart_as_admin =
		IsDlgButtonChecked(tabs[0], IDC_AUTOSTARTADMIN) == BST_CHECKED;
	modifier_key = get_combo_value(tabs[1], IDC_MODIFIER);
	modifier_key2 = get_combo_value(tabs[1], IDC_MODIFIER2);
	action_lmb = get_combo_value(tabs[1], IDC_LMB);
	action_mmb = get_combo_value(tabs[1], IDC_MMB);
	action_rmb = get_combo_value(tabs[1], IDC_RMB);
	action_m4 = get_combo_value(tabs[1], IDC_M4);
	action_m5 = get_combo_value(tabs[1], IDC_M5);

	ini_write();

	restore();
	autostart();

	if (hide_titlebars || no_thickframe)
	{
		if (!hk_win_ev)
			init_win_event_hook();

		EnumWindows(enum_win_proc, 0);
	}
	else if (hk_win_ev)
		UnhookWinEvent(hk_win_ev);
}

void revert_config()
{
	CheckDlgButton(tabs[0], IDC_FOCUSWINDOW,
		focus_window_on_drag ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(tabs[0], IDC_HIDEBARS,
		hide_titlebars ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(tabs[0], IDC_NOTHICKFRAME,
		no_thickframe ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(
		tabs[0], IDC_AUTOSTART, add_to_autostart ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(tabs[0], IDC_AUTOSTARTADMIN,
		autostart_as_admin ? BST_CHECKED : BST_UNCHECKED);

	set_combo_value(tabs[1], IDC_MODIFIER, modifier_key);
	set_combo_value(tabs[1], IDC_MODIFIER2, modifier_key2);
	set_combo_value(tabs[1], IDC_LMB, action_lmb);
	set_combo_value(tabs[1], IDC_MMB, action_mmb);
	set_combo_value(tabs[1], IDC_RMB, action_rmb);
	set_combo_value(tabs[1], IDC_M4, action_m4);
	set_combo_value(tabs[1], IDC_M5, action_m5);
}

bool in_light_mode()
{
	HKEY key;
	DWORD lightmode = 0;
	DWORD lightmode_sz = sizeof(DWORD);

	printf("%lu\n", lightmode);

	// Open the personalization registry key
	LSTATUS status = RegOpenKeyExW(HKEY_CURRENT_USER,
		L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personaliz"
		L"e",
		0, KEY_READ, &key);

	if (status == ERROR_SUCCESS)
	{
		// "SystemUsesLightTheme" determines the color mode of the taskbar
		// and system tray
		status = RegQueryValueExW(key, L"SystemUsesLightTheme", NULL, NULL,
			(LPBYTE)&lightmode, &lightmode_sz);
		RegCloseKey(key);
	}
	return lightmode == 0;
}

void setup_tray(HWND hwnd, bool update)
{
	tray_data.cbSize = sizeof(NOTIFYICONDATAA);
	tray_data.hWnd = hwnd;
	tray_data.uID = 1;
	tray_data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	tray_data.uCallbackMessage = WM_TRAYICON;
	tray_data.hIcon = in_light_mode() ? icon_light : icon_dark;
	strcpy_s(tray_data.szTip, sizeof(tray_data.szTip), name);

	Shell_NotifyIconA(update ? NIM_MODIFY : NIM_ADD, &tray_data);
}

INT_PTR CALLBACK dlg_proc(HWND hwnd,
	UINT umsg,
	WPARAM wparam,
	LPARAM lparam)
{
	static UINT shellrestartmsg = 0;
	if (shellrestartmsg == 0)
		shellrestartmsg = RegisterWindowMessageW(L"TaskbarCreated");

	if (umsg == shellrestartmsg && shellrestartmsg != 0)
	{
		setup_tray(hwnd, true);
		return TRUE;
	}

	switch (umsg)
	{
		case WM_INITDIALOG:
		{
			EnumChildWindows(hwnd, hide_focus, lparam);
			HINSTANCE hinstance = GetModuleHandle(NULL);
			setup_tray(hwnd, false);

			HICON small_icon = icon_dark;
			if (small_icon)
				SendMessageW(
					hwnd, WM_SETICON, ICON_SMALL, (LPARAM)small_icon);

			HICON big_icon = (HICON)LoadImageW(GetModuleHandleW(NULL),
				MAKEINTRESOURCEW(IDI_TRAYLIGHT), IMAGE_ICON,
				GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON),
				0);
			if (big_icon)
				SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)big_icon);

			HWND tab_wnd = GetDlgItem(hwnd, IDC_TAB1);

			TCITEM tie;
			tie.mask = TCIF_TEXT;
			tie.pszText = (LPSTR) "General";
			TabCtrl_InsertItem(tab_wnd, 0, &tie);
			tie.pszText = (LPSTR) "Mouse";
			TabCtrl_InsertItem(tab_wnd, 1, &tie);
			tie.pszText = (LPSTR) "About";
			TabCtrl_InsertItem(tab_wnd, 3, &tie);

			tabs[0] = CreateDialog(hinstance, MAKEINTRESOURCE(IDD_GENERAL),
				hwnd, child_dlg_proc);
			tabs[1] = CreateDialog(
				hinstance, MAKEINTRESOURCE(IDD_MOUSE), hwnd, child_dlg_proc);
			tabs[2] = CreateDialog(
				hinstance, MAKEINTRESOURCE(IDD_ABOUT), hwnd, child_dlg_proc);

			RECT tab_rect;
			GetClientRect(tab_wnd, &tab_rect);
			TabCtrl_AdjustRect(tab_wnd, FALSE, &tab_rect);

			MapWindowPoints(tab_wnd, hwnd, (LPPOINT)&tab_rect, 2);

			for (int i = 0; i < MAX_PAGES; i++)
				SetWindowPos(tabs[i], HWND_TOP, tab_rect.left,
					tab_rect.top, tab_rect.right - tab_rect.left,
					tab_rect.bottom - tab_rect.top, i == 0 ? SWP_SHOWWINDOW : SWP_HIDEWINDOW);

			HWND modkey = GetDlgItem(tabs[1], IDC_MODIFIER);
			SendMessage(modkey, CB_ADDSTRING, 0, (LPARAM) "Left Win");
			SendMessage(modkey, CB_ADDSTRING, 0, (LPARAM) "Right Win");
			SendMessage(modkey, CB_ADDSTRING, 0, (LPARAM) "Left Alt");
			SendMessage(modkey, CB_ADDSTRING, 0, (LPARAM) "Right Alt");
			SendMessage(modkey, CB_SETCURSEL, 0, 0);

			HWND modkey2 = GetDlgItem(tabs[1], IDC_MODIFIER2);
			SendMessage(modkey2, CB_ADDSTRING, 0, (LPARAM) "None");
			SendMessage(modkey2, CB_ADDSTRING, 0, (LPARAM) "Left Win");
			SendMessage(modkey2, CB_ADDSTRING, 0, (LPARAM) "Right Win");
			SendMessage(modkey2, CB_ADDSTRING, 0, (LPARAM) "Left Alt");
			SendMessage(modkey2, CB_ADDSTRING, 0, (LPARAM) "Right Alt");
			SendMessage(modkey2, CB_SETCURSEL, 0, 0);

			int hello[] = {IDC_LMB, IDC_MMB, IDC_RMB, IDC_M4, IDC_M5};
			int len = sizeof(hello) / sizeof(hello[0]);

			for (int i = 0; i < len; i++)
			{
				HWND item = GetDlgItem(tabs[1], hello[i]);
				SendMessage(item, CB_ADDSTRING, 0, (LPARAM) "Do nothing");
				SendMessage(item, CB_ADDSTRING, 0, (LPARAM) "Move");
				SendMessage(item, CB_ADDSTRING, 0, (LPARAM) "Resize");
				SendMessage(
					item, CB_ADDSTRING, 0, (LPARAM) "Toggle maximize");
				SendMessage(item, CB_ADDSTRING, 0, (LPARAM) "Minimize");
				SendMessage(item, CB_ADDSTRING, 0, (LPARAM) "Send to bottom");
				SendMessage(item, CB_ADDSTRING, 0, (LPARAM) "Close");
			}

			revert_config();

			return TRUE;
		}

		case WM_SETTINGCHANGE:
			if (lparam &&
				strcmp((const char*)lparam, "ImmersiveColorSet") == 0)
			{
				setup_tray(hwnd, true);
				return TRUE;
			}
			break;

		case WM_NOTIFY:
		{
			LPNMHDR lpnm = (LPNMHDR)lparam;
			if (lpnm->idFrom == IDC_TAB1 && lpnm->code == TCN_SELCHANGE)
			{
				HWND hTab = lpnm->hwndFrom;
				int iPage = TabCtrl_GetCurSel(hTab);
				switch_tab(iPage);
			}
			break;
		}
		case WM_TRAYICON:
		{
			switch (lparam)
			{
				case WM_RBUTTONUP:
					tray_menu(hwnd);
					return TRUE;
			}
			break;
		}

		case WM_COMMAND:
		{
			switch (LOWORD(wparam))
			{
				case IDOK:
				{
					if (HIWORD(wparam) == BN_CLICKED)
					{
						apply_config();
						ShowWindow(hwnd, SW_HIDE);

						HWND tab_wnd = GetDlgItem(hwnd, IDC_TAB1);
						TabCtrl_SetCurSel(tab_wnd, 0);
						switch_tab(0);

						return TRUE;
					}
				}
				break;

				case IDCANCEL:
				{
					if (HIWORD(wparam) == BN_CLICKED)
					{
						revert_config();
						ShowWindow(hwnd, SW_HIDE);

						HWND tab_wnd = GetDlgItem(hwnd, IDC_TAB1);
						TabCtrl_SetCurSel(tab_wnd, 0);
						switch_tab(0);

						return TRUE;
					}
				}
				break;

				case IDB_APPLY:
				{
					if (HIWORD(wparam) == BN_CLICKED)
					{
						apply_config();

						HWND tab_wnd = GetDlgItem(hwnd, IDC_TAB1);
						TabCtrl_SetCurSel(tab_wnd, 0);
						switch_tab(0);

						return TRUE;
					}
				}
				break;

				case ID_TRAY_CON:
					console_state = !console_state;
					if (console_state)
					{
						ShowWindow(console_wnd, SW_SHOW);
						SetForegroundWindow(console_wnd);
					}
					else
						ShowWindow(console_wnd, SW_HIDE);

					return TRUE;

				case ID_TRAY_SHOW:
					ShowWindow(hwnd, SW_SHOW);
					SetForegroundWindow(hwnd);
					return TRUE;

				case ID_TRAY_ABOUT:
					ShowWindow(hwnd, SW_SHOW);
					SetForegroundWindow(hwnd);

					HWND tab_wnd = GetDlgItem(hwnd, IDC_TAB1);
					TabCtrl_SetCurSel(tab_wnd, 2);
					switch_tab(2);

					return TRUE;

				case ID_TRAY_EXIT:
					goodbye();
					DestroyWindow(hwnd);
					return TRUE;
			}
			break;
		}

		case WM_CLOSE:
			// Intercept window title-bar close ('X' button) and hide
			ShowWindow(hwnd, SW_HIDE);

			HWND tab_wnd = GetDlgItem(hwnd, IDC_TAB1);
			TabCtrl_SetCurSel(tab_wnd, 0);
			switch_tab(0);

			return TRUE;

		case WM_DESTROY:
			goodbye();
			PostQuitMessage(0);
			return TRUE;
	}
	return FALSE;
}

int WINAPI WinMain(HINSTANCE hinstance,
	HINSTANCE hprevinstance,
	LPSTR lpcmdline,
	int ncmdshow)
{
	UNREFERENCED_PARAMETER(hprevinstance);
	UNREFERENCED_PARAMETER(ncmdshow);

	if (lpcmdline && strlen(lpcmdline) > 0 &&
		strcmp(lpcmdline, "--elevated") == 0)
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
		init_win_event_hook();
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
	icon_light =
		(HICON)LoadImage(hinstance, MAKEINTRESOURCE(IDI_TRAYLIGHT),
			IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
			GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR | LR_SHARED);

	cursor_drag = LoadCursor(NULL, IDC_SIZEALL);
	cursor_resize_tl_br = LoadCursor(NULL, IDC_SIZENWSE);
	cursor_resize_tr_bl = LoadCursor(NULL, IDC_SIZENESW);

	WNDCLASS wc = {0};
	wc.hIcon = icon_light;
	wc.lpfnWndProc = DefWindowProc;
	wc.hInstance = hinstance;
	wc.lpszClassName = name;
	RegisterClass(&wc);

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
		if ((!tabs[current_tab] ||
				!IsDialogMessage(tabs[current_tab], &msg)) &&
			!IsDialogMessage(settings_dlg, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	goodbye();
	return (int)msg.wParam;
}
