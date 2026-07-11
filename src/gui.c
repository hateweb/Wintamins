#include <stdio.h>
#include <stdbool.h>

#include <windows.h>
#include <commctrl.h>

#include "resources.h"
#include "gui.h"
#include "io.h"
#include "wintamins.h"

#define WM_TRAYICON (WM_APP + 1)

bool master_switch = true;

bool should_elevate = false;
bool hide_tray = false;
bool no_updater = false;

bool start_capture = false;

tab tabs[] = {{"General", NULL, IDD_GENERAL}, {"Mouse", NULL, IDD_MOUSE},
	{"Exclusions", NULL, IDD_EXCLUSIONS}, {"About", NULL, IDD_ABOUT}};
const uint8_t max_tabs = sizeof(tabs) / sizeof(tab);
uint8_t current_tab = 0;

static const uint8_t title_font_size = 20;
static HFONT title_font;
static HBRUSH backbrush;

HICON icon_dark;
HICON icon_light;

NOTIFYICONDATAA tray_data;

void switch_tab(HWND hwnd, int index)
{
	if (hwnd)
	{
		HWND tab_wnd = GetDlgItem(hwnd, IDC_TAB1);
		if (tab_wnd)
			TabCtrl_SetCurSel(tab_wnd, index);
	}

	if (current_tab != index)
	{
		ShowWindow(tabs[current_tab].hwnd, SW_HIDE);
		current_tab = index;
		ShowWindow(tabs[current_tab].hwnd, SW_SHOW);
	}
}

BOOL CALLBACK hide_focus(HWND hwnd, LPARAM lparam)
{
	UNREFERENCED_PARAMETER(lparam)
	SendMessage(hwnd, WM_UPDATEUISTATE, MAKEWPARAM(UIS_SET, UISF_HIDEFOCUS), 0);
	return TRUE;
}

INT_PTR CALLBACK child_dlg_proc(HWND hwnd,
	UINT umsg,
	WPARAM wparam,
	LPARAM lparam)
{
	switch (umsg)
	{
		case WM_INITDIALOG:
		{
			EnumChildWindows(hwnd, hide_focus, 0);

			HWND title_ctrl = GetDlgItem(hwnd, IDC_TITLE);
			if (title_ctrl && title_font)
				SendMessage(title_ctrl, WM_SETFONT, (WPARAM)title_font, TRUE);

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

					// syslinks always use unicode
					ShellExecuteW(NULL, L"open", pnmLink->item.szUrl, NULL,
						NULL, SW_SHOWNORMAL);
					return TRUE;
				}
			}
			else if (pnmhdr->idFrom == IDC_LICENSE)
			{
				if (pnmhdr->code == NM_CLICK || pnmhdr->code == NM_RETURN)
				{
					ShellExecuteW(
						NULL, L"open", L"LICENSE", NULL, NULL, SW_SHOWNORMAL);
					return TRUE;
				}
			}
			break;
		}

		case WM_COMMAND:
		{
			if (LOWORD(wparam) == IDB_ELEVATE && HIWORD(wparam) == BN_CLICKED)
			{
				elevate();
				goodbye();
				PostQuitMessage(0);
				return TRUE;
			}

			if (LOWORD(wparam) == IDC_AUTOSTART && HIWORD(wparam) == BN_CLICKED)
			{
				HWND autostartadmin = GetDlgItem(hwnd, IDC_AUTOSTARTADMIN);
				if (autostartadmin)
					EnableWindow(autostartadmin,
						IsDlgButtonChecked(tabs[0].hwnd, IDC_AUTOSTART) ==
							BST_CHECKED);
				return TRUE;
			}

			if (LOWORD(wparam) == IDB_CAPTURE && HIWORD(wparam) == BN_CLICKED)
			{
				start_capture = true;
				mod_active = false;
				HWND capture_btn = GetDlgItem(tabs[2].hwnd, IDB_CAPTURE);
				EnableWindow(capture_btn, FALSE);
				destroy_keyboard_hk();
				init_mouse_hk();
				set_cursor(&cursor_cross);
				return TRUE;
			}
			break;
		}

		case WM_CTLCOLORDLG:
			return (INT_PTR)backbrush;

		case WM_CTLCOLORSTATIC:
		{
			HDC hdc = (HDC)wparam;
			SetBkMode(hdc, TRANSPARENT);
			return (INT_PTR)backbrush;
		}
	}
	return FALSE;
}

int get_combo_value(HWND tab, int id)
{
	HWND item = GetDlgItem(tab, id);
	return item ? (int)SendMessage(item, CB_GETCURSEL, 0, 0) : 0;
}

void set_combo_value(HWND tab, int id, int v)
{
	HWND item = GetDlgItem(tab, id);
	if (item)
		SendMessage(item, CB_SETCURSEL, v, 0);
}

void apply_config()
{
	if (tabs[0].hwnd && tabs[1].hwnd && tabs[2].hwnd)
	{
		for (int i = 0; i < entries_size; i++)
		{
			// too much table lookup. would this help?
			config_e entry = entries[i];

			if (entry.t == CFG_BOOL)
			{
				*(bool*)entry.value_ptr =
					IsDlgButtonChecked(*entry.tab, entry.id) ==
					BST_CHECKED;
			}
			
			else if (entry.t == CFG_UINT8)
			{
				*(uint8_t*)entry.value_ptr =
					get_combo_value(*entry.tab, entry.id);
			}

			else if (entry.t == CFG_ARR)
			{
				char buf[1024];
				GetDlgItemText(*entry.tab, entry.id, buf, sizeof(buf));

				char** arr = (char**)entry.value_ptr;

				for (int j = 0; j < MAX_EXCLUDE; j++)
					arr[j] = NULL;

				char* tok = strtok(buf, ",");
				int k = 0;
				while (tok != NULL && k < MAX_EXCLUDE)
				{
					arr[k] = strdup(tok); 
					tok = strtok(NULL, ",");
					k++;
				}
			}
		}
	}

	ini_write();

	EnumWindows(restore, 0);
	autostart();

	if (cfg.hide_titlebars)
	{
		if (!hk_win_ev)
			init_win_event_hk();

		EnumWindows(enum_win_proc, 0);
	}
	else if (hk_win_ev)
	{
		UnhookWinEvent(hk_win_ev);
		hk_win_ev = NULL;
	}
}

void revert_config()
{
	if (!tabs[0].hwnd || !tabs[1].hwnd || !tabs[2].hwnd)
		return;

	for (int i = 0; i < entries_size; i++)
	{
		// too much table lookup. would this help?
		config_e entry = entries[i];

		if (entry.t == CFG_BOOL)
			CheckDlgButton(*entry.tab, entry.id,
				*(bool*)entry.value_ptr ? BST_CHECKED : BST_UNCHECKED);

		else if (entry.t == CFG_UINT8)
			set_combo_value(*entry.tab, entry.id, *(uint8_t*)entry.value_ptr);

		else if (entry.t == CFG_ARR)
		{
			char buf[1024];
			char** arr = (char**)entry.value_ptr;

			for (int j = 0; j < MAX_EXCLUDE; j++)
			{
				if (arr[j] == NULL)
					break;

				if (j > 0)
					strcat(buf, ",");

				strcat(buf, arr[j]);
			}

			SetDlgItemText(*entry.tab, entry.id, buf);
		}
	}
}

bool is_light_mode()
{
	HKEY key;
	DWORD lightmode = 1;
	DWORD lightmode_size = sizeof(DWORD);

	LSTATUS status = RegOpenKeyExW(HKEY_CURRENT_USER,
		L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personaliz"
		L"e",
		0, KEY_READ, &key);

	if (status == ERROR_SUCCESS)
	{
		status = RegQueryValueExW(key, L"SystemUsesLightTheme", NULL, NULL,
			(LPBYTE)&lightmode, &lightmode_size);
		RegCloseKey(key);
	}

	return lightmode;
}

void setup_tray(HWND hwnd, int msg)
{
	if (hide_tray)
		return;

	tray_data.cbSize = sizeof(NOTIFYICONDATAA);
	tray_data.hWnd = hwnd;
	tray_data.uID = 1;
	tray_data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	tray_data.uCallbackMessage = WM_TRAYICON;
	tray_data.hIcon = is_light_mode() ? icon_dark : icon_light;
	snprintf(tray_data.szTip, sizeof(tray_data.szTip), "%s", name);

	Shell_NotifyIcon(msg, &tray_data);
}

void tray_menu(HWND hwnd)
{
	POINT pt;
	GetCursorPos(&pt);

	HMENU menu = LoadMenu(GetModuleHandle(NULL), MAKEINTRESOURCE(IDM_TRAYMENU));
	if (!menu)
		return;

	HMENU submenu = GetSubMenu(menu, 0);

	if (!submenu)
	{
		DestroyMenu(menu);
		return;
	}

	UINT check_state = master_switch ? MF_CHECKED : MF_UNCHECKED;
	CheckMenuItem(submenu, ID_TRAY_ENABLED, MF_BYCOMMAND | check_state);

	// won't close if we don't force focus it
	SetForegroundWindow(hwnd);

	TrackPopupMenu(
		submenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);

	// workaround: send an empty message to force process event queues
	PostMessage(hwnd, WM_NULL, 0, 0);

	DestroyMenu(menu);
}

INT_PTR CALLBACK dlg_proc(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam)
{
	static UINT shellrestartmsg = 0;
	if (!shellrestartmsg)
		shellrestartmsg = RegisterWindowMessageW(L"TaskbarCreated");

	if (umsg == shellrestartmsg && shellrestartmsg)
	{
		log_msg(STATUS_ERROR, "restartmsg");
		setup_tray(hwnd, NIM_ADD);
		return TRUE;
	}

	switch (umsg)
	{
		case WM_INITDIALOG:
		{
			EnumChildWindows(hwnd, hide_focus, 0);

			backbrush = CreateSolidBrush(RGB(249, 249, 249));

			title_font = CreateFont(title_font_size, 0, 0, 0, FW_NORMAL, FALSE,
				FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
				CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
				DEFAULT_PITCH | FF_DONTCARE, "MS Shell Dlg");

			HINSTANCE hinstance = GetModuleHandle(NULL);
			setup_tray(hwnd, NIM_ADD);

			HICON small_icon = icon_dark;
			if (small_icon)
				SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)small_icon);

			HICON big_icon = (HICON)LoadImageW(GetModuleHandleW(NULL),
				MAKEINTRESOURCEW(IDI_TRAYLIGHT), IMAGE_ICON,
				GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
			if (big_icon)
				SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)big_icon);

			HWND tab_wnd = GetDlgItem(hwnd, IDC_TAB1);
			if (!tab_wnd)
				return TRUE;

			TCITEMA tie;
			tie.mask = TCIF_TEXT;
			for (int i = 0; i < max_tabs; i++)
			{
				tie.pszText = (LPSTR)tabs[i].name;
				TabCtrl_InsertItem(tab_wnd, i, &tie);

				tabs[i].hwnd = CreateDialog(hinstance,
					MAKEINTRESOURCE(tabs[i].id), hwnd, child_dlg_proc);
			}

			RECT tab_rect;
			GetClientRect(tab_wnd, &tab_rect);
			TabCtrl_AdjustRect(tab_wnd, FALSE, &tab_rect);

			MapWindowPoints(tab_wnd, hwnd, (LPPOINT)&tab_rect, 2);

			for (int i = 0; i < max_tabs; i++)
				if (tabs[i].hwnd)
					SetWindowPos(tabs[i].hwnd, HWND_TOP, tab_rect.left,
						tab_rect.top, tab_rect.right - tab_rect.left,
						tab_rect.bottom - tab_rect.top,
						i == 0 ? SWP_SHOWWINDOW : SWP_HIDEWINDOW);

			static const char* mod_names[] = {
				"None", "Left Win", "Right Win", "Left Alt", "Right Alt"};
			static const uint8_t mod_names_size =
				sizeof(mod_names) / sizeof(mod_names[0]);

			HWND modkey = GetDlgItem(tabs[1].hwnd, IDC_MODIFIER);
			if (modkey)
				for (int i = 1; i < mod_names_size; i++)
					SendMessage(modkey, CB_ADDSTRING, 0, (LPARAM)mod_names[i]);

			HWND modkey2 = GetDlgItem(tabs[1].hwnd, IDC_MODIFIER2);
			if (modkey2)
				for (int i = 0; i < mod_names_size; i++)
					SendMessage(modkey2, CB_ADDSTRING, 0, (LPARAM)mod_names[i]);

			static const int key_ids[] = {
				IDC_LMB, IDC_MMB, IDC_RMB, IDC_M4, IDC_M5};
			static const int key_ids_size =
				sizeof(key_ids) / sizeof(key_ids[0]);

			static const char* act_names[] = {"Do nothing", "Move", "Resize",
				"Toggle maximize", "Minimize", "Center", "Bring to foreground",
				"Send to bottom", "Toggle always on top", "Close"};
			static const uint8_t act_names_size =
				sizeof(act_names) / sizeof(act_names[0]);

			for (int i = 0; i < key_ids_size; i++)
			{
				HWND item = GetDlgItem(tabs[1].hwnd, key_ids[i]);
				if (item)
					for (int j = 0; j < act_names_size; j++)
						SendMessage(
							item, CB_ADDSTRING, 0, (LPARAM)act_names[j]);
			}

			static const char* scr_act_names[] = {"Do nothing", "Volume control"};
			static const uint8_t scr_act_names_size =
				sizeof(scr_act_names) / sizeof(scr_act_names[0]);

			HWND scroll = GetDlgItem(tabs[1].hwnd, IDC_SCR);
			if (scroll)
				for (int i = 0; i < scr_act_names_size; i++)
					SendMessage(scroll, CB_ADDSTRING, 0, (LPARAM)scr_act_names[i]);

			revert_config();
			return TRUE;
		}

		case WM_SETTINGCHANGE:
			if (lparam && !strcmp((const char*)lparam, "ImmersiveColorSet"))
			{
				setup_tray(hwnd, NIM_MODIFY);
				return TRUE;
			}
			break;

		case WM_NOTIFY:
		{
			LPNMHDR lpnm = (LPNMHDR)lparam;
			if (lpnm && lpnm->idFrom == IDC_TAB1 && lpnm->code == TCN_SELCHANGE)
			{
				HWND tab = lpnm->hwndFrom;
				int cur_tab = TabCtrl_GetCurSel(tab);
				switch_tab(NULL, cur_tab);
			}
			break;
		}

		case WM_TRAYICON:
		{
			if (lparam == WM_RBUTTONUP)
			{
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
						switch_tab(hwnd, 0);
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
						switch_tab(hwnd, 0);
						return TRUE;
					}
				}
				break;

				case IDB_APPLY:
				{
					if (HIWORD(wparam) == BN_CLICKED)
					{
						apply_config();
						return TRUE;
					}
				}
				break;

				case ID_TRAY_ENABLED:
					master_switch = !master_switch;

					if (master_switch)
					{
						init_keyboard_hk();

						if (cfg.hide_titlebars)
						{
							init_win_event_hk();
							EnumWindows(enum_win_proc, 0);
						}
					}
					else
					{
						destroy_keyboard_hk();
						destroy_mouse_hk();

						if (cfg.hide_titlebars)
						{
							EnumWindows(restore, 0);
							destroy_win_event_hk();
						}
					}

					return TRUE;

				case ID_TRAY_HIDE:
					Shell_NotifyIcon(NIM_DELETE, &tray_data);
					return TRUE;

				case ID_TRAY_SHOW:
					ShowWindow(hwnd, SW_SHOW);
					SetForegroundWindow(hwnd);
					return TRUE;

				case ID_TRAY_ABOUT:
					ShowWindow(hwnd, SW_SHOW);
					SetForegroundWindow(hwnd);
					switch_tab(hwnd, 2);
					return TRUE;

				case ID_TRAY_EXIT:
					goodbye();
					DestroyWindow(hwnd);
					return TRUE;
			}
			break;
		}

		case WM_CLOSE:
			ShowWindow(hwnd, SW_HIDE);
			switch_tab(hwnd, 0);
			return TRUE;

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

			goodbye();
			PostQuitMessage(0);
			return TRUE;
	}
	return FALSE;
}
