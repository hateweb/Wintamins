#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define OEMRESOURCE
#include <windows.h>

#include "wintamins.h"
#include "io.h"
#include "gui.h"

#define VK_UNASSIGNED 0xE8

const char name[] = "Wintamins";
const char contitle[] = "Wintamins Console";

typedef struct
{
	HWND hwnd;
	LONG_PTR original_style;
} modwnd;

#define MAX_TRACKED_WINDOWS 256
modwnd tracked_wnds[MAX_TRACKED_WINDOWS];
int tracked_count = 0;

HCURSOR cursor_drag;
HCURSOR cursor_resize_tl_br;
HCURSOR cursor_resize_tr_bl;

static const DWORD cursors[] = {OCR_NORMAL, OCR_IBEAM, OCR_WAIT, OCR_CROSS,
	OCR_UP, OCR_SIZEALL, OCR_SIZENS, OCR_SIZEWE, OCR_HAND};
static const int cursor_amt = sizeof(cursors) / sizeof(cursors[0]);

HWINEVENTHOOK hk_win_ev;

bool console_state = false;
HWND console_wnd;

uint8_t state = 0;
bool drag_occurred = false;
bool mod_active = false;
bool was_maximized = false;

HWND target_wnd;
RECT window_start;

POINT mouse_start;
POINT mouse_current;

HHOOK hk_mouse;
HHOOK hk_keyboard;

HANDLE drag_work_ev;
HANDLE drag_thread_h;
volatile int run_thread = 1;

const char* whitelist[] = {"#32768", "Progman", "WorkerW", "Shell_TrayWnd",
	"Shell_SecondaryTrayWnd", "Windows.UI.Core.CoreWindow",
	"EdgeUiInputWndClass", "NotifyIconOverflowWindow", "TaskSwitcherWnd",
	"TaskSwitcherOverlayWnd", "MultitaskingViewFrame",
	"XamlExplorerHostIslandWindow"};

int wl_size = sizeof(whitelist) / sizeof(whitelist[0]);

bool compare(HWND hwnd)
{
	char class[256];
	GetClassName(hwnd, class, sizeof(class));

	for (int i = 0; i < wl_size; i++)
		if (!strcmp(whitelist[i], class))
			return true;

	return false;
}

void override_style(HWND hwnd)
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

	if (!GetWindowTextLength(hwnd))
		return;

	char class[256];
	GetClassName(hwnd, class, sizeof(class));

	if (compare(hwnd))
		return;

	LONG_PTR style_override = original_style;

	if (cfg.hide_titlebars)
		style_override &= ~WS_CAPTION;

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

	AdjustWindowRectEx(
		&rect_from, (DWORD)from_style, has_menu, (DWORD)ex_style);
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

	if (tray_data.hWnd != NULL)
		Shell_NotifyIcon(NIM_DELETE, &tray_data);

	if (hk_win_ev)
		UnhookWinEvent(hk_win_ev);

	run_thread = 0;

	if (drag_work_ev != NULL)
		SetEvent(drag_work_ev);

	if (drag_thread_h != NULL)
	{
		WaitForSingleObject(drag_thread_h, INFINITE);
		CloseHandle(drag_thread_h);
		drag_thread_h = NULL;
	}

	if (drag_work_ev != NULL)
	{
		CloseHandle(drag_work_ev);
		drag_work_ev = NULL;
	}

	log_msg(STATUS_INFO, "finished");
	close_log();
}

void winkey()
{
	INPUT inputs[2] = {0};
	inputs[0].type = INPUT_KEYBOARD;
	inputs[0].ki.wVk = VK_UNASSIGNED;  // unassigned
	inputs[0].ki.dwFlags = 0;

	inputs[1].type = INPUT_KEYBOARD;
	inputs[1].ki.wVk = VK_LMENU;
	inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

	SendInput(2, inputs, sizeof(INPUT));
}

bool elevate()
{
	char path[MAX_PATH];
	if (!GetModuleFileName(NULL, path, MAX_PATH))
		return false;

	bool elevated = false;
	HANDLE token;
	TOKEN_ELEVATION elevation;
	DWORD size = sizeof(TOKEN_ELEVATION);
	if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
	{
		if (GetTokenInformation(
				token, TokenElevation, &elevation, sizeof(elevation), &size))
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

	if (!ShellExecuteEx(&info))
	{
		log_msg(STATUS_ERROR, "failed to elevate");
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
	if (!cfg.add_to_autostart)
	{
		if (RegOpenKeyEx(HKEY_CURRENT_USER,
			"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0,
			KEY_SET_VALUE, &key) == ERROR_SUCCESS)
		{
			RegDeleteValue(key, name);
			RegCloseKey(key);
		}
		return;
	}

	char path[MAX_PATH];
	if (!GetModuleFileName(NULL, path, MAX_PATH))
		return;

	if (cfg.autostart_as_admin)
		strcat(path, " --elevated");

	if (RegOpenKeyEx(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ | KEY_SET_VALUE, &key) == ERROR_SUCCESS)
	{
		char buffer[MAX_PATH];
		DWORD buffer_sz = sizeof(buffer);

		LONG query_status =
			RegQueryValueEx(key, name, NULL, NULL, (LPBYTE)buffer, &buffer_sz);

		if (query_status != ERROR_SUCCESS || strcmp(path, buffer) != 0)
		{
			RegSetValueEx(key, name, 0, REG_SZ, (const BYTE*)path,
				(DWORD)(strlen(path) + 1));
		}

		RegCloseKey(key);
	}
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

	override_style(hwnd);
}

void init_win_event_hk()
{
	if (hk_win_ev)
		return;

	hk_win_ev = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW, NULL,
		&handle_win_ev, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
	if (!hk_win_ev)
		log_msg(STATUS_ERROR, "failed to initialize event hook");
}

void destroy_win_event_hk()
{
	if (!hk_win_ev)
		return;

	UnhookWinEvent(hk_win_ev);
	hk_win_ev = NULL;
}

BOOL CALLBACK enum_win_proc(HWND hwnd, LPARAM lparam)
{
	UNREFERENCED_PARAMETER(lparam);

	override_style(hwnd);
	return TRUE;
}

bool get_unmax_bounds(HWND hwnd, RECT* rect_out)
{
	if (!hwnd || !rect_out)
		return false;

	WINDOWPLACEMENT wp;
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
	HMONITOR monitor = MonitorFromRect(
		&wp.rcNormalPosition, MONITOR_DEFAULTTONEAREST);
	if (!monitor)
	{
		*rect_out = wp.rcNormalPosition;
		return true;
	}

	MONITORINFO mon_info = {0};
	mon_info.cbSize = sizeof(MONITORINFO);
	if (!GetMonitorInfo(monitor, &mon_info))
	{
		*rect_out = wp.rcNormalPosition;
		return true;
	}

	int dx = mon_info.rcWork.left - mon_info.rcMonitor.left;
	int dy = mon_info.rcWork.top - mon_info.rcMonitor.top;

	rect_out->left = wp.rcNormalPosition.left + dx;
	rect_out->top = wp.rcNormalPosition.top + dy;
	rect_out->right = wp.rcNormalPosition.right + dx;
	rect_out->bottom = wp.rcNormalPosition.bottom + dy;

	return true;
}

void reset_values()
{
	state = ACTION_NONE;
	was_maximized = false;
	target_wnd = NULL;

	SystemParametersInfo(SPI_SETCURSORS, 0, NULL, 0);
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

void begin_resize()
{
	// don't resize if the window doesn't allow to.
	LONG_PTR original_style = GetWindowLongPtr(target_wnd, GWL_STYLE);
	if ((original_style & WS_THICKFRAME) == 0)
	{
		state = ACTION_NONE;
		return;
	}

	int dir = CORNER_BOTTOMRIGHT;
	HCURSOR* cur = &cursor_resize_tl_br;
	if (cfg.closest_corner_on_resize)
	{
		double dx_l = (double)mouse_start.x - window_start.left;
		double dx_r = (double)mouse_start.x - window_start.right;
		double dy_t = (double)mouse_start.y - window_start.top;
		double dy_b = (double)mouse_start.y - window_start.bottom;

		double d_tl = dx_l * dx_l + dy_t * dy_t;
		double d_tr = dx_r * dx_r + dy_t * dy_t;
		double d_bl = dx_l * dx_l + dy_b * dy_b;
		double d_br = dx_r * dx_r + dy_b * dy_b;

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

	state = ACTION_RESIZE;

	if (cfg.snap_cursor_on_resize)
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

void click_logic(WINDOWPLACEMENT* wp, int option)
{
	if (option == ACTION_DRAG)
	{
		state = ACTION_DRAG;
		if (wp->showCmd == SW_SHOWMAXIMIZED)
			was_maximized = true;

		set_cursor(&cursor_drag);
	}
	else if (option == ACTION_RESIZE)
		begin_resize();

	else if (option == ACTION_MAXIMIZE)
		PostMessage(target_wnd, WM_SYSCOMMAND,
			wp->showCmd == SW_SHOWMAXIMIZED ? SC_RESTORE : SC_MAXIMIZE, 0);

	else if (option == ACTION_MINIMIZE)
		PostMessage(target_wnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);

	else if (option == ACTION_BRINGDOWN)
		SetWindowPos(target_wnd, HWND_BOTTOM, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

	else if (option == ACTION_CLOSE)
		PostMessage(target_wnd, WM_CLOSE, 0, 0);
}

void release_logic(int option, MSLLHOOKSTRUCT* mouse_struct)
{
	if (option == ACTION_DRAG && state == ACTION_DRAG)
		reset_values();

	else if (option == ACTION_RESIZE && state == ACTION_RESIZE)
	{
		POINT pt = mouse_struct->pt;
		PostMessage(target_wnd, WM_LBUTTONUP, 0, MAKELPARAM(pt.x, pt.y));
		
		reset_values();
	}
}

bool process_clicks(const WPARAM* p_wparam, MSLLHOOKSTRUCT* p_mouse_struct)
{
	if (state != ACTION_NONE)
		return false;

	WPARAM wparam = *p_wparam;
	POINT pt = p_mouse_struct->pt;
	HWND hwnd = WindowFromPoint(pt);

	if (!hwnd)
		return false;

	HWND root_hwnd = GetAncestor(hwnd, GA_ROOT);
	HWND desktop_hwnd = GetDesktopWindow();

	if (!root_hwnd || root_hwnd == desktop_hwnd || compare(root_hwnd))
		return false;

	WINDOWPLACEMENT wp;
	wp.length = sizeof(WINDOWPLACEMENT);

	if (!GetWindowPlacement(root_hwnd, &wp) || state != ACTION_NONE)
		return false;

	target_wnd = root_hwnd;

	if (cfg.focus_window_on_drag)
		SetForegroundWindow(root_hwnd);

	mouse_start = pt;
	GetWindowRect(target_wnd, &window_start);

	// this is so bad...
	if (wparam == WM_LBUTTONDOWN)
		click_logic(&wp, cfg.action_lmb);
	else if (wparam == WM_MBUTTONDOWN)
		click_logic(&wp, cfg.action_mmb);
	else if (wparam == WM_RBUTTONDOWN)
		click_logic(&wp, cfg.action_rmb);
	else if (wparam == WM_XBUTTONDOWN)
	{
		WORD xbutton = HIWORD(p_mouse_struct->mouseData);
		if (xbutton == XBUTTON1)
			click_logic(&wp, cfg.action_m4);
		else if (xbutton == XBUTTON2)
			click_logic(&wp, cfg.action_m5);
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

void drag()
{
	int x, y;

	int dx = mouse_current.x - mouse_start.x;
	int dy = mouse_current.y - mouse_start.y;

	if (was_maximized)
	{
		PostMessage(target_wnd, WM_SYSCOMMAND, SC_RESTORE, 0);
		RECT rect_out;
		get_unmax_bounds(target_wnd, &rect_out);

		x = mouse_current.x - (rect_out.right - rect_out.left) / 2;
		y = mouse_current.y - (rect_out.bottom - rect_out.top) / 2;
	}
	else
	{
		x = window_start.left + dx;
		y = window_start.top + dy;
	}

	SetWindowPos(target_wnd, NULL, x, y, 0, 0,
		SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

unsigned __stdcall drag_thread(void* arg)
{
	while (run_thread)
	{
		DWORD wait_result = WaitForSingleObject(drag_work_ev, INFINITE);
		if (wait_result == WAIT_OBJECT_0 && run_thread)
			drag();
	}

	return 0;
}

bool any_key_down(WPARAM wparam)
{
	return (wparam == WM_LBUTTONDOWN && cfg.action_lmb != ACTION_NONE) ||
		   (wparam == WM_MBUTTONDOWN && cfg.action_mmb != ACTION_NONE) ||
		   (wparam == WM_RBUTTONDOWN && cfg.action_rmb != ACTION_NONE) ||
		   (wparam == WM_XBUTTONDOWN &&
			   (cfg.action_m4 != ACTION_NONE || cfg.action_m5 != ACTION_NONE));
}

bool any_key_up(WPARAM wparam)
{
	return (wparam == WM_LBUTTONUP && cfg.action_lmb != ACTION_NONE) ||
		   (wparam == WM_MBUTTONUP && cfg.action_mmb != ACTION_NONE) ||
		   (wparam == WM_RBUTTONUP && cfg.action_rmb != ACTION_NONE) ||
		   (wparam == WM_XBUTTONUP &&
			   (cfg.action_m4 != ACTION_NONE || cfg.action_m5 != ACTION_NONE));
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

	if (wparam == WM_MOUSEMOVE && target_wnd && state == ACTION_DRAG)
	{
		mouse_current = mouse_struct->pt;
		SetEvent(drag_work_ev);
	}

	else if (any_key_up(wparam) && state != ACTION_NONE)
	{
		if (wparam == WM_LBUTTONUP)
			release_logic(cfg.action_lmb, mouse_struct);
		else if (wparam == WM_MBUTTONUP)
			release_logic(cfg.action_mmb, mouse_struct);
		else if (wparam == WM_RBUTTONUP)
			release_logic(cfg.action_rmb, mouse_struct);
		else if (wparam == WM_XBUTTONUP)
		{
			WORD xbutton = HIWORD(mouse_struct->mouseData);
			if (xbutton == XBUTTON1)
				release_logic(cfg.action_m4, mouse_struct);
			else if (xbutton == XBUTTON2)
				release_logic(cfg.action_m5, mouse_struct);
		}

		return 1;
	}
	return CallNextHookEx(hk_mouse, ncode, wparam, lparam);
}

void init_keyboard_hk()
{
	if (hk_keyboard)
		return;

	hk_keyboard = SetWindowsHookEx(WH_KEYBOARD_LL, keyboard_proc, NULL, 0);
	if (!hk_keyboard)
		log_msg(STATUS_ERROR, "failed to initialize keyboard hook");
}

void destroy_keyboard_hk()
{
	if (!hk_keyboard)
		return;

	UnhookWindowsHookEx(hk_keyboard);
	hk_keyboard = NULL;
}

void init_mouse_hk()
{
	if (hk_mouse)
		return;

	hk_mouse = SetWindowsHookEx(WH_MOUSE_LL, mouse_proc, NULL, 0);
	if (!hk_mouse)
		log_msg(STATUS_ERROR, "failed to initialize mouse hook");
}

void destroy_mouse_hk()
{
	if (!hk_mouse)
		return;

	UnhookWindowsHookEx(hk_mouse);
	hk_mouse = NULL;
}

LRESULT CALLBACK keyboard_proc(int ncode, WPARAM wparam, LPARAM lparam)
{
	if (ncode == HC_ACTION)
	{
		KBDLLHOOKSTRUCT* pKey = (KBDLLHOOKSTRUCT*)lparam;

		if (pKey->vkCode == (DWORD)modkeys[cfg.modifier_key + 1] ||
			(cfg.modifier_key2 != 0 ? pKey->vkCode == (DWORD)modkeys[cfg.modifier_key2]
								: false))
		{
			if (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN)
			{
				mod_active = true;
				init_mouse_hk();
			}

			else if (wparam == WM_KEYUP || wparam == WM_SYSKEYUP)
			{
				mod_active = false;

				if (state == ACTION_NONE)
					destroy_mouse_hk();

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
