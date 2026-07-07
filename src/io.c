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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <time.h>
#include <windows.h>

#include "resources.h"
#include "wintamins.h"
#include "io.h"
#include "gui.h"

#define MAX_LINE_LEN 256

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

bool config_created = false;

config cfg;

const config_e entries[] = {
	{"focus_window_on_drag", CFG_BOOL, &cfg.focus_window_on_drag, {.b = true},
		IDC_FOCUSWINDOW, &tabs[0].hwnd},
	{"closest_corner_on_resize", CFG_BOOL, &cfg.closest_corner_on_resize,
		{.b = true}, IDC_CLOSESTCORNER, &tabs[0].hwnd},
	{"snap_cursor_on_resize", CFG_BOOL, &cfg.snap_cursor_on_resize,
		{.b = false}, IDC_SNAPCURSOR, &tabs[0].hwnd},
	{"hide_titlebars", CFG_BOOL, &cfg.hide_titlebars, {.b = false},
		IDC_HIDEBARS, &tabs[0].hwnd},
	{"add_to_autostart", CFG_BOOL, &cfg.add_to_autostart, {.b = false},
		IDC_AUTOSTART, &tabs[0].hwnd},
	{"autostart_as_admin", CFG_BOOL, &cfg.autostart_as_admin, {.b = false},
		IDC_AUTOSTARTADMIN, &tabs[0].hwnd},
	{"modifier_key", CFG_UINT8, &cfg.modifier_key, {.u8 = 0}, IDC_MODIFIER,
		&tabs[1].hwnd},
	{"modifier_key2", CFG_UINT8, &cfg.modifier_key2, {.u8 = 0}, IDC_MODIFIER2,
		&tabs[1].hwnd},
	{"action_lmb", CFG_UINT8, &cfg.action_lmb, {.u8 = ACTION_DRAG}, IDC_LMB,
		&tabs[1].hwnd},
	{"action_mmb", CFG_UINT8, &cfg.action_mmb, {.u8 = ACTION_MAXIMIZE}, IDC_MMB,
		&tabs[1].hwnd},
	{"action_rmb", CFG_UINT8, &cfg.action_rmb, {.u8 = ACTION_RESIZE}, IDC_RMB,
		&tabs[1].hwnd},
	{"action_m4", CFG_UINT8, &cfg.action_m4, {.u8 = ACTION_NONE}, IDC_M4,
		&tabs[1].hwnd},
	{"action_m5", CFG_UINT8, &cfg.action_m5, {.u8 = ACTION_NONE}, IDC_M5,
		&tabs[1].hwnd},
};

const int entries_size = sizeof(entries) / sizeof(entries[0]);

const int modkeys[] = {0x0, VK_LWIN, VK_RWIN, VK_LMENU, VK_RMENU};

FILE* log_file;
const char* status[] = {"E", "W", "I"};

void open_log()
{
	if (log_file != NULL)
		return;

	log_file = fopen("Wintamins.log", "a");
}

void close_log()
{
	if (log_file == NULL)
		return;

	fclose(log_file);
	log_file = NULL;
}

void log_msg(const int level, const char* fmt, ...)
{
	if (log_file == NULL)
		return;

	char t_buf[80];
	time_t t_raw = time(NULL);
	struct tm* t_info = localtime(&t_raw);

	strftime(t_buf, sizeof(t_buf), "%Y-%m-%d %H:%M:%S", t_info);
	fprintf(log_file, "[%s] [%s] ", t_buf, status[level]);

	va_list args;
	va_start(args, fmt);
	vfprintf(log_file, fmt, args);
	va_end(args);

	fprintf(log_file, "\n");

	if (level == STATUS_ERROR)
		fflush(log_file);
}

char* trim(char* str)
{
	while (isspace((unsigned char)*str))
		str++;

	if (*str == '\0')
		return str;

	// Trim trailing space
	char* end = str + strlen(str) - 1;
	while (end > str && isspace((unsigned char)*end))
		end--;

	end[1] = '\0';
	return str;
}

void reset_config()
{
	for (int i = 0; i < entries_size; i++)
	{
		if (entries[i].t == CFG_BOOL)
			*(bool*)entries[i].value_ptr = entries[i].def.b;

		else if (entries[i].t == CFG_UINT8)
			*(uint8_t*)entries[i].value_ptr = entries[i].def.u8;
	}
}

// make this key value stuff a structure?
void assign_config(const char* key, const char* value)
{
	for (int i = 0; i < entries_size; i++)
	{
		if (strcmp(key, entries[i].name) == 0)
		{
			if (entries[i].t == CFG_BOOL)
				*(bool*)entries[i].value_ptr = (strcasecmp(value, "true") == 0);

			else if (entries[i].t == CFG_UINT8)
				*(uint8_t*)entries[i].value_ptr = (uint8_t)atoi(value);

			return;
		}
	}
}

void ini_parse()
{
	reset_config();

	FILE* file = fopen("Wintamins.ini", "r");
	if (file == NULL)
	{
		if (!config_created)
		{
			ini_write();
			config_created = true;
			ini_parse();
		}
		else
			log_msg(STATUS_ERROR, "failed to open config file");
	}

	char line[MAX_LINE_LEN];

	while (fgets(line, sizeof(line), file) != NULL)
	{
		char* trimmed = trim(line);

		if (trimmed[0] == '\0' || trimmed[0] == ';' || trimmed[0] == '#')
			continue;

		char* equal = strchr(trimmed, '=');
		if (equal == NULL)
			continue;

		*equal = '\0';

		char* key = trim(trimmed);
		char* value = trim(equal + 1);

		assign_config(key, value);
	}

	fclose(file);
}

void ini_write()
{
	FILE* file = fopen("Wintamins.ini", "w");
	if (file == NULL)
	{
		log_msg(STATUS_ERROR, "failed to write config");
		return;
	}

	for (int i = 0; i < entries_size; i++)
	{
		if (entries[i].t == CFG_BOOL)
			fprintf(file, "%s = %s\n", entries[i].name,
				*(bool*)entries[i].value_ptr ? "true" : "false");

		else if (entries[i].t == CFG_UINT8)
			fprintf(file, "%s = %d\n", entries[i].name,
				*(uint8_t*)entries[i].value_ptr);
	}

	fclose(file);
}

void license_write()
{
	FILE* file_chk = fopen("LICENSE", "r");
	if (file_chk != NULL)
	{
		fclose(file_chk);
		return;
	}

	fclose(file_chk);

	HRSRC res =
		FindResource(NULL, MAKEINTRESOURCE(IDR_LICENSE), "LICENSE_DATA");
	if (!res)
		return;

	HGLOBAL data = LoadResource(NULL, res);
	if (!data)
		return;

	const char* license_text = LockResource(data);
	DWORD license_sz = SizeofResource(NULL, res);

	if (!license_text || license_sz == 0)
		return;

	FILE* file = fopen("LICENSE", "wb");
	if (file == NULL)
		return;

	fwrite(license_text, 1, license_sz, file);
	fclose(file);
}
