/*
	Wintamins - for healthy window management
	Copyright (C) 2026 hateweb

	This program is free software: you can redistribute it
	and/or modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation,
	either version 3 of the License, or (at your option) any
	later version.

	This program is distributed in the hope that it will be
	useful, but WITHOUT ANY WARRANTY; without even the implied
	warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
	PURPOSE. See the GNU General Public License for more
	details.

	You should have received a copy of the GNU General Public
	License along with this program. If not, see
	<https://www.gnu.org/licenses/>.
*/
#ifndef RESOURCE_H
#define RESOURCE_H

#define VERSION_MAJOR 0
#define VERSION_MINOR 1

#if defined(__x86_64__) || defined(_M_X64)
#define BUILD_ARCH 64
#else
#define BUILD_ARCH 32
#endif

#define IDD_SETTINGS 101
#define IDD_GENERAL 102
#define IDD_MOUSE 103
#define IDD_ABOUT 104

#define IDR_LICENSE 105

#define IDB_APPLY 201
#define IDB_ELEVATE 202

#define IDC_FOCUSWINDOW 1001
#define IDC_CLOSESTCORNER 1002
#define IDC_SNAPCURSOR 1003
#define IDC_HIDEBARS 1004
#define IDC_AUTOSTART 1005
#define IDC_HIDETRAY 1006
#define IDC_AUTOSTARTADMIN 1007

#define IDC_MODIFIER 1008
#define IDC_MODIFIER2 1009

#define IDC_LMB 1010
#define IDC_MMB 1011
#define IDC_RMB 1012
#define IDC_M4 1013
#define IDC_M5 1014
#define IDC_SCR 1015

#define IDC_TITLE 1016
#define IDC_LINK 1017
#define IDC_LICENSE 1018

#define IDC_GROUP_MANAGEMENT 1019
#define IDC_GROUP_AUTOSTART 1020
#define IDC_GROUP_EXPERIMENTAL 1021
#define IDC_GROUP_MODKEY 1022
#define IDC_GROUP_REGULARACTIONS 1023
#define IDC_GROUP_SCRACTIONS 1024
#define IDC_GROUP_ABOUT 1025
#define IDC_TAB1 1026

// New IDs for the tray menu
#define IDM_TRAYMENU 201
#define ID_TRAY_ENABLED 2001
#define ID_TRAY_HIDE 2002
#define ID_TRAY_SHOW 2003
#define ID_TRAY_ABOUT 2004
#define ID_TRAY_EXIT 2005

#define IDI_TRAYDARK 101
#define IDI_TRAYLIGHT 102
#endif
