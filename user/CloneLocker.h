//
// CloneLocker.h
// Copyright (C) 2013 HostileFork.com
//
// General includes and definitions used by all the parts of the user-mode
// component.
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

#ifndef __CLONELOCKER_H__
#define __CLONELOCKER_H__

// Note that this inclusion order is important: WinSock2 must come before Windows
#include <WinSock2.h>
#include <Windows.h>

// Standard C includes
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

// Leak detecting replacements of malloc/free
// (not currently used)
#include <crtdbg.h>

// Safe string routines, which replace a lot of deprecated routines that
// dontuse.h removes from the API
#include <strsafe.h>
#include <dontuse.h>

// Safe string routines, which replace a lot of deprecated routines
// (This is the user mode version, ntstrsafe.h is for kernel mode)
#include <StrSafe.h>

// Leak detecting replacements of malloc/free
// (not currently used)
#include <crtdbg.h>

// DDK-specific headers, you have to have the 7.1.0 Driver Kit for these
// http://www.microsoft.com/en-us/download/confirmation.aspx?id=11800
#include <winioctl.h>
#include <fltuser.h>

// Don't underestimate this line.  I got it from the DDK sample for
// SCANNER and you can really mess things up by having it before
// or after the inclusion of header files.  I know what it does, of
// course, but I will remind anyone who isn't familiar that all
// #pragma directives exist outside of the language standard.
// Someone on the GCC team apparently took this idea of compiler-defined
// behavior to an extreme and if you used any pragmas the compiler
// would launch a game of Tetris.  Because well, it is up to the
// compiler how to handle them.  But this totally can muck up the
// interactions between the kernel mode component and user mode
// component.  I have reached a delicate balance by putting it...here
// (if it is needed at all).  I'm not on a timescale where I can 
// really experiment with understanding well enough if I can take 
// it out, this is what we call "copypasta" code.  And the NT DDK
// has samples that I will call a disgrace to programming.

#pragma pack(1)


// Resource IDs.  This should be a common include.
#define IDI_CLONELOCKER 101


// 
// Globals structure
//

typedef struct {
	TCHAR* szAppName;
	HINSTANCE hinst;
	DWORD guiThreadId;

	// Main Window stuff
	HWND hwnd;
	BOOL Quitting;
} GLOBALS_TYPE;

extern GLOBALS_TYPE Globals;


void
DbgPrint(
	LPCTSTR FormatString,
	...
);

#endif // __CLONELOCKER_H__