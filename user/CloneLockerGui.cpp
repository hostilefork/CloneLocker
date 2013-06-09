//
// CloneLockerGui.cpp
// Copyright (C) 2013 HostileFork.com
//
// CloneLocker's GUI consists of a single window driven mostly by the
// Microsoft "ListView" common control.
//
// This file is part of CloneLocker
//
// CloneLocker is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// CloneLocker is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with CloneLocker.  If not, see <http://www.gnu.org/licenses/>.
//

#include "CloneLocker.h"
#include <commctrl.h>

#include "CloneLockerFilter.h"
#include "CloneLockerClient.h"
#include "CloneLockerGui.h"

#include "cJSON.h"

#pragma comment (lib, "comctl32.lib")

HWND hwndListView;

//
// CreateListView
//
// Creates a list-view control in report view.  Returns the handle to the new control
// Taken from http://msdn.microsoft.com/en-us/library/windows/desktop/hh298360(v=vs.85).aspx
//
HWND CreateListView(HWND hwndParent) 
{
	HWND hwndResult;
    RECT rcClient; // The parent window's client area.

	INITCOMMONCONTROLSEX InitCtrls;

	// Common controls initialization, ok to call more than once and is
	// cleaner to do it here instead of once in WinMain
	InitCtrls.dwICC = ICC_LISTVIEW_CLASSES;
    InitCtrls.dwSize = sizeof(INITCOMMONCONTROLSEX);
    if (!InitCommonControlsEx(&InitCtrls)) {
		MessageBox(
			NULL,
			TEXT("InitCommonControlsEx failed to register List View classes."), 
			Globals.szAppName,
			MB_ICONERROR
		);
	}

    GetClientRect(hwndParent, &rcClient); 

    // Create the list-view window in report view with label editing enabled.
    hwndResult = CreateWindow(
		WC_LISTVIEW,
		TEXT(""),
		WS_CHILD | LVS_REPORT | LVS_EDITLABELS,
		0,
		0,
		rcClient.right - rcClient.left,
		rcClient.bottom - rcClient.top,
		hwndParent,
		(HMENU) NULL,
		Globals.hinst,
		NULL
	);

    return hwndResult;
}

#define LVSTRING_BUFSIZE 1024


//
// MainWndProc
//
LRESULT CALLBACK MainWndProc(HWND hwnd, WM wm, WPARAM wparam, LPARAM lparam)
{
	switch(wm) {
	case WM_CREATE:
		{
			LVCOLUMN lvcolumn; // Make Coluom struct for ListView

			CREATESTRUCT* cs = (CREATESTRUCT*)(lparam);
			LPVOID lpCreateParams = cs->lpCreateParams;
		
			hwndListView = CreateListView(hwnd);

			// This code establishes the column headers.  The items are added
			// and removed based on notifications from the driver, and from
			// the server process.

			memset(&lvcolumn, 0, sizeof(lvcolumn)); // Reset Column
			lvcolumn.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM; // Type of mask

			// Inserting Columns as much as we want
			lvcolumn.cx = 0x200;
			lvcolumn.pszText = TEXT("File Path");
			SendMessage(hwndListView, LVM_INSERTCOLUMN, 0, (LPARAM) &lvcolumn);

			lvcolumn.cx = 0x42;
			lvcolumn.pszText = TEXT("Lock Type");
			SendMessage(hwndListView, LVM_INSERTCOLUMN, 1, (LPARAM) &lvcolumn);

			lvcolumn.cx = 0x100;
			lvcolumn.pszText = TEXT("Held By");
			SendMessage(hwndListView, LVM_INSERTCOLUMN, 2, (LPARAM) &lvcolumn);

			// Default selection style for list view only 
			SendMessage(hwndListView, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT);

			ShowWindow(hwndListView, SW_SHOW);
					                
			return 0;
		}

	case WM_NOTIFY:
		{
			LPNMHDR lpnmhdr = (LPNMHDR) lparam;
			if (lpnmhdr->hwndFrom == hwndListView) {

				// Respond to clicks on list items
				if ((lpnmhdr->code == NM_CLICK) || (lpnmhdr->code == NM_RCLICK)) {

// this was some debugging code to show the network code could work, really clicking should
// not affect things unless there's some sort of lock-breaking
#ifdef BEHAVIOR_ON_CLICKING_LIST
					
					LVITEM lvitem;  // ListView Item struct

					LPNMITEMACTIVATE lpnmitem = (LPNMITEMACTIVATE) lparam;

					ACCESS_TRACK_STRUCT* tracker;
					HRESULT hr;
					TCHAR szLvBuffer[LVSTRING_BUFSIZE];
					BOOL SafeToOpen = (lpnmhdr->code == NM_CLICK);

					DbgPrint(TEXT("We got a left or right click\n"));

					ZeroMemory(&lvitem, sizeof(lvitem));
					lvitem.mask = LVIF_PARAM;
					lvitem.iSubItem = 0;
					lvitem.iItem = lpnmitem->iItem;
                  
					SendMessage(hwndListView, LVM_GETITEM, lvitem.iItem, (LPARAM) &lvitem);

					tracker = (ACCESS_TRACK_STRUCT*) lvitem.lParam;
					if (!tracker) {
						// forget it, callback already done
						break;
					}
					hr = ReplyToFilterAndFreeTracker(tracker, SafeToOpen);
					tracker = NULL;

					// Zero out the tracker from the lParam of the list element
					lvitem.lParam = (LPARAM) 0;
					lvitem.mask = LVIF_PARAM;
					SendMessage(hwndListView, LVM_SETITEM, lpnmitem->iItem, (LPARAM) &lvitem);

					lvitem.mask = LVIF_TEXT;

					lvitem.iSubItem = 1;
					StringCchPrintf(szLvBuffer, LVSTRING_BUFSIZE, SafeToOpen ? TEXT("R") : TEXT("W")); 
					lvitem.pszText = szLvBuffer;
					SendMessage(hwndListView,LVM_SETITEM,0,(LPARAM)&lvitem); // Enter text to SubItems

					lvitem.iSubItem = 2;
					StringCchPrintf(szLvBuffer, LVSTRING_BUFSIZE, SafeToOpen ? TEXT("Brian") : TEXT("Bo")); 
					lvitem.pszText = szLvBuffer;
					SendMessage(hwndListView,LVM_SETITEM,0,(LPARAM)&lvitem); // Enter text to SubItems

					if (SUCCEEDED(hr)) {
						DbgPrint(TEXT("Replied message\n"));
					} else {
						DbgPrint(TEXT("Locker: Error replying message. Error = 0x%X\n"), hr);
						break;
					}
#endif
				}
			}
		}
		break;

	case WM_DESTROY:
		{
			// We need to break our threads out.
			Globals.Quitting = TRUE;
			Globals.hwnd = (HWND) NULL;
			PostQuitMessage(0);
			return 0;
		}
	}

	return DefWindowProc(hwnd, wm, wparam, lparam);
}


//
// InitGui
//
// Just the basic Win32 main window setup, dig out your old Petzold book.
//
HRESULT InitGui(int iCmdShow)
{
	// Can probably just use ordinary wndclass, test that later
	WNDCLASSEX wndclassex;
	wndclassex.cbSize = sizeof(WNDCLASSEX);
	wndclassex.style = CS_HREDRAW | CS_VREDRAW;
	wndclassex.lpfnWndProc = MainWndProc;
	wndclassex.cbClsExtra = 0;
	wndclassex.cbWndExtra = 0;
	wndclassex.hInstance = Globals.hinst;
	wndclassex.hIcon = LoadIcon(Globals.hinst, MAKEINTRESOURCE(IDI_CLONELOCKER));
	wndclassex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndclassex.hbrBackground =(HBRUSH) GetStockObject(WHITE_BRUSH);
	wndclassex.lpszMenuName = NULL;
	wndclassex.lpszClassName = Globals.szAppName;
	wndclassex.hIconSm = LoadIcon(Globals.hinst, MAKEINTRESOURCE(IDI_CLONELOCKER));

	if (!RegisterClassEx(&wndclassex)) {
		MessageBox(
			NULL,
			TEXT("RegisterClassEx failed to register the main window."), 
			Globals.szAppName,
			MB_ICONERROR
		);

		return E_FAIL;
	}
     
	Globals.hwnd = CreateWindow(
		Globals.szAppName, // window class name
		TEXT("CloneLocker Configuration"), // window caption
		WS_OVERLAPPEDWINDOW, // window style
		CW_USEDEFAULT, // initial x position
		CW_USEDEFAULT, // initial y position
		CW_USEDEFAULT, // initial x size
		CW_USEDEFAULT, // initial y size
		NULL, // parent window handle
		NULL, // window menu handle
		Globals.hinst, // program instance handle
		(LPVOID) 0 // creation parameters, none right now
	); 
     
	// Should we show the window on init or when we go to
	// run the main loop?  Showing it has the advantage that people
	// know the program is started in case the worker or network
	// threads are slow.
	ShowWindow(Globals.hwnd, iCmdShow);
	UpdateWindow(Globals.hwnd);

	return S_OK;
}


//
// RunMainGuiLoop
//
int RunMainGuiLoop()
{
	MSG msg;
	while(GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}


//
// ShutdownGui
//
// Currently the window will be destroyed.  This sets in motion all the destruction...
//
HRESULT ShutdownGui()
{
	if (Globals.hwnd != (HWND) NULL) {
		DestroyWindow(Globals.hwnd);
		Globals.hwnd = NULL;
	}
	return S_OK;
}


// 
// GetFileAccessListFromGui
//
// This should be a blocking call.  I am planning on storing all the state in the listview
// widget because that seems the laziest thing to do and means there is no need to keep
// any model in sync with said view.  Note that if a server queries itself, than that
// means it will capture its data... delete it... and replace it with the same data it
// had before.  Well, no one said this was efficient.
//
// For now I'm just building a data structure and passing it back to prove the point.
//
cJSON* GetFileAccessListFromGui()
{
	DbgPrint(TEXT("Entering GetFileAccessListFromGui()\n"));

	cJSON* result = cJSON_CreateObject();
	const char *daysOfWeekStrings[7] = {
		"Sunday",
		"Monday",
		"Tuesday",
		"Wednesday",
		"Thursday",
		"Friday",
		"Saturday"
	};

	cJSON_AddItemToObject(result, "Filenames", cJSON_CreateStringArray(daysOfWeekStrings, 7));

	DbgPrint(TEXT("Exiting GetFileAccessListFromGui()\n"));

	return result;
}


//
// SetGuiFileAccessList
//
// Populate the list view information from a JSON message.
//
void SetGuiFileAccessList(cJSON* json) 			
{
	DbgPrint(TEXT("Entering SetGuiFileAccessList\n"));

	int numFilenames = cJSON_GetArraySize(cJSON_GetObjectItem(json, "Filenames"));

	ListView_DeleteAllItems(hwndListView);

	int iFilename = 0;
	while (iFilename < numFilenames) {
		LVITEM lvitem;
		ZeroMemory(&lvitem, sizeof(lvitem));
		lvitem.mask = LVIF_TEXT | LVIF_PARAM;
		lvitem.lParam = (LPARAM) NULL;
		lvitem.iItem = 0;  
		lvitem.iSubItem = 0;
		lvitem.pszText = TEXT("WcharFilenameGoesHere"); // oh we don't need the days of the week.  clearly we need a WSTR json lib here

		SendMessage(hwndListView, LVM_INSERTITEM, 0, (LPARAM) &lvitem);

		iFilename++;
	}

	DbgPrint(TEXT("exiting SetGuiFileAccessList\n"));
}
