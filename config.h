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

enum actions
{
	ACTION_NONE = 0,
	ACTION_DRAG = 1,
	ACTION_RESIZE = 2,
	ACTION_MAXIMIZE = 3,
	ACTION_MINIMIZE = 4,
	ACTION_BRINGDOWN = 5,
	ACTION_CLOSE = 6
};

extern const int modkeys[];
extern uint8_t modifier_key;
extern uint8_t modifier_key2;
extern bool focus_window_on_drag;
extern bool closest_corner_on_resize;
extern bool snap_cursor_on_resize;
extern bool hide_titlebars;
extern bool no_thickframe;
extern bool add_to_autostart;
extern bool autostart_as_admin;
extern uint8_t action_lmb;
extern uint8_t action_mmb;
extern uint8_t action_rmb;
extern uint8_t action_m4;
extern uint8_t action_m5;

void ini_parse();
void ini_write();
void license_write();
