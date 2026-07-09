#pragma once

#include <stdbool.h>

#include <windows.h>

enum
{
	ACTION_NONE,
	ACTION_DRAG,
	ACTION_RESIZE,
	ACTION_MAXIMIZE,
	ACTION_MINIMIZE,
	ACTION_BRINGDOWN,
	ACTION_CLOSE
};

enum
{
	SCROLL_NONE,
	SCROLL_VOLUME,
};

enum
{
	CORNER_TOPLEFT = 4,
	CORNER_TOPRIGHT = 5,
	CORNER_BOTTOMLEFT = 7,
	CORNER_BOTTOMRIGHT = 8,
};

extern const char name[];
extern const char contitle[];

extern bool console_state;
extern HWND console_wnd;

extern HCURSOR cursor_drag;
extern HCURSOR cursor_resize_tl_br;
extern HCURSOR cursor_resize_tr_bl;

extern HHOOK hk_mouse;
extern HHOOK hk_keyboard;

extern HWINEVENTHOOK hk_win_ev;

extern HANDLE drag_work_ev;
extern HANDLE drag_thread_h;

bool compare(HWND hwnd);
void override_style(HWND hwnd);
void adjust_wnd_rect(HWND hwnd,
	LONG_PTR from_style,
	LONG_PTR to_style,
	int* d_left,
	int* d_top,
	int* d_right,
	int* d_bottom);
void restore();
void goodbye();
void winkey();
bool elevate();
void autostart();
unsigned __stdcall drag_thread(void* arg);
void init_win_event_hk();
void destroy_win_event_hk();
void init_keyboard_hk();
void destroy_keyboard_hk();
void init_mouse_hk();
void destroy_mouse_hk();

BOOL CALLBACK enum_win_proc(HWND hwnd, LPARAM lparam);
LRESULT CALLBACK keyboard_proc(int ncode, WPARAM wparam, LPARAM lparam);
