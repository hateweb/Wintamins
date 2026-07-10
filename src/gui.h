#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <windows.h>

typedef struct
{
	const char* name;
	HWND hwnd;
	int id;
} tab;

extern bool should_elevate;
extern bool hide_tray;
extern bool no_updater;

extern tab tabs[];
extern const uint8_t max_tabs;
extern uint8_t current_tab;

extern HICON icon_dark;
extern HICON icon_light;

extern NOTIFYICONDATA tray_data;
INT_PTR CALLBACK dlg_proc(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam);
