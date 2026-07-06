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

#include <windows.h>

#include "resources.h"
#include "config.h"

#define MAX_LINE_LEN 256

bool config_created = false;

#define PARSE_STR(name)                         \
	if (strcmp(key, #name) == 0)                \
	{                                           \
		strncpy(name, value, sizeof(name) - 1); \
		(name)[sizeof(name) - 1] = '\0';        \
	}

#define PARSE_INT(name)          \
	if (strcmp(key, #name) == 0) \
	{                            \
		(name) = atoi(value);    \
	}

#define PARSE_HEX(name)                        \
	if (strcmp(key, #name) == 0)               \
	{                                          \
		(name) = (int)strtol(value, NULL, 16); \
	}

#define PARSE_BOOL(name)                         \
	if (strcmp(key, #name) == 0)                 \
	{                                            \
		(name) = strcasecmp(value, "true") == 0; \
	}

#define STRINGIFY(name) #name
#define STRINGIFY_NEW(name) str_##name

#define SERIALIZE_INT(name)                                             \
	char new_##name[MAX_LINE_LEN];                                      \
	snprintf(new_##name, sizeof(new_##name), "%s = %d\n", #name, name); \
	fputs(new_##name, file);

#define SERIALIZE_HEX(name)                                             \
	char new_##name[MAX_LINE_LEN];                                      \
	snprintf(new_##name, sizeof(new_##name), "%s = %x\n", #name, name); \
	fputs(new_##name, file);

#define SERIALIZE_BOOL(name)                                     \
	char new_##name[MAX_LINE_LEN];                               \
	snprintf(new_##name, sizeof(new_##name), "%s = %s\n", #name, \
		(name) ? "true" : "false");                              \
	fputs(new_##name, file);

// ...what?

const int modkeys[] = {0x0, VK_LWIN, VK_RWIN, VK_LMENU, VK_RMENU};
uint8_t modifier_key = 0;
uint8_t modifier_key2 = 0;
bool focus_window_on_drag = true;
bool closest_corner_on_resize = true;
bool snap_cursor_on_resize = false;
bool hide_titlebars = false;
bool no_thickframe = false;
bool add_to_autostart = false;
bool autostart_as_admin = false;
uint8_t action_lmb = ACTION_DRAG;
uint8_t action_mmb = ACTION_MAXIMIZE;
uint8_t action_rmb = ACTION_RESIZE;
uint8_t action_m4 = ACTION_NONE;
uint8_t action_m5 = ACTION_NONE;

const char* default_config =
	"; "
	"https://learn.microsoft.com/en-us/windows/win32/inputdev/"
	"virtual-key-codes\n";

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

// make this key value stuff a structure?
void assign_config(const char* key, const char* value)
{  // clang-format off
	PARSE_BOOL(focus_window_on_drag)
	else PARSE_BOOL(closest_corner_on_resize)
	else PARSE_BOOL(snap_cursor_on_resize)
	else PARSE_BOOL(hide_titlebars)
	else PARSE_BOOL(no_thickframe)
	else PARSE_BOOL(add_to_autostart)
	else PARSE_BOOL(autostart_as_admin)
	else PARSE_INT(modifier_key)
	else PARSE_INT(modifier_key2)
	else PARSE_INT(action_lmb)
	else PARSE_INT(action_mmb)
	else PARSE_INT(action_rmb)
	else PARSE_INT(action_m4)
	else PARSE_INT(action_m5)
}  // clang-format on

void ini_parse()
{
	FILE* file = fopen("wintamins.ini", "r");
	if (file == NULL)
	{
		if (!config_created)
		{
			ini_write();
			config_created = true;
			ini_parse();
		}
		else
			printf("L%d -> failed to open config file\n", __LINE__);
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
	FILE* file = fopen("wintamins.ini", "w");
	if (file == NULL)
	{
		printf("L%d -> failed to write config\n", __LINE__);
		return;
	}

	fputs(default_config, file);
	SERIALIZE_BOOL(focus_window_on_drag);
	SERIALIZE_BOOL(closest_corner_on_resize);
	SERIALIZE_BOOL(snap_cursor_on_resize);
	SERIALIZE_BOOL(hide_titlebars);
	SERIALIZE_BOOL(no_thickframe);
	SERIALIZE_BOOL(add_to_autostart);
	SERIALIZE_BOOL(autostart_as_admin);
	SERIALIZE_INT(modifier_key);
	SERIALIZE_INT(modifier_key2);
	SERIALIZE_INT(action_lmb);
	SERIALIZE_INT(action_mmb);
	SERIALIZE_INT(action_rmb);
	SERIALIZE_INT(action_m4);
	SERIALIZE_INT(action_m5);
	fclose(file);
}

void license_write()
{
	FILE* file_chk = fopen("LICENSE", "r");
	if (file_chk != NULL)
		return;

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
	fprintf(file, "%s", license_text);

	fclose(file);
}
