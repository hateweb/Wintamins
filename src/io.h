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
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <windows.h>

#define MAX_EXCLUDE 128

enum
{
	STATUS_ERROR,
	STATUS_WARN,
	STATUS_INFO
};

typedef struct
{
	bool focus_window_on_drag;
	bool closest_corner_on_resize;
	bool snap_cursor_on_resize;
	bool hide_titlebars;
	bool add_to_autostart;
	bool hide_tray;
	bool autostart_as_admin;
	uint8_t modifier_key;
	uint8_t modifier_key2;
	uint8_t action_lmb;
	uint8_t action_mmb;
	uint8_t action_rmb;
	uint8_t action_m4;
	uint8_t action_m5;
	uint8_t action_scr;
	char* action_exclude[MAX_EXCLUDE];
	bool exclude_without_titlebar;
	char* titlebar_exclude[MAX_EXCLUDE];
} config;

extern config cfg;

typedef enum
{
	CFG_BOOL,
	CFG_UINT8,
	CFG_ARR
} config_t;

typedef struct
{
	const char* name;
	config_t t;
	void* value_ptr;
	union
	{
		bool b;
		uint8_t u8;
		const char* s[MAX_EXCLUDE];
	} def;
	int id;
	HWND* tab;
} config_e;

extern const config_e entries[];
extern const int entries_size;

extern const int modkeys[];

void open_log();
void close_log();
void log_msg(const int level, const char* fmt, ...);

void cleanup();
bool update();

void ini_parse();
void ini_write();
void license_write();
