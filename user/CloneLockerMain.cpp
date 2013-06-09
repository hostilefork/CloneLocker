//
// CloneLockerMain.cpp
// Copyright (C) 2013 HostileFork.com
//
// Entry point for the user mode component of CloneLocker, which
// basically just prevents more than one instance from running at a
// time and kicks off the code from the other parts of the system.
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

// Project includes
#include "CloneLocker.h"
#include "CloneLockerGui.h"
#include "CloneLockerServer.h"
#include "CloneLockerFilter.h"
#include "CloneLockerClient.h"


GLOBALS_TYPE Globals;


//
// Utility function for consistency with kernel mode DbgPrint in
// the DDK, except for user mode.  See:
//
//    http://www.cheatengine.org/forum/viewtopic.php?t=346550&sid=03a813c90dfc86a658ab6bb66e63a0de
//

#define DBGOUT_BUFSIZE 1024
void
DbgPrint(
	LPCTSTR FormatString,
	...
) 
{ 
   TCHAR dbgout[DBGOUT_BUFSIZE]; 
   va_list vaList; 

   va_start(vaList, FormatString); 
   StringCchVPrintf(dbgout, DBGOUT_BUFSIZE, FormatString, vaList); 
   OutputDebugString(dbgout); 
   va_end(vaList); 
} 


//
// The original scanner example was a console-mode program that used DbgPrint to give
// feedback about the attempts that were tracked to access files that had the
// forbidden word in it.  We want a Win32 GUI to set up the network configuration,
// so the first step is just to switch over to that.
//
// The "main loop" of the main thread was that after the worker threads had been
// set up, it would sit in an infinite wait for all of those threads to send a 
// finished signal.  The call looked like this:
//
//     WaitForMultipleObjectsEx(i, threads, TRUE, INFINITE, FALSE);
//
// If there is going to be an application wm pump like a typical program,
// then the easiest way to implement this exit logic is to have a WM_USER 
// wm which is used to convey thread signals to the GUI thread. (All UI should
// be done from the GUI thread.  Windows doesn't expliticly enforce this, but
// it is essential on other platforms and just a good practice in general.)
//
// Note the return policy for WinMain is as follows:
//
//     "If the function succeeds, terminating when it receives a 
//     WM_QUIT message, it should return the exit value contained in that
//     message's wParam parameter. If the function terminates before
//     entering the message loop, it should return zero.
//
int
WINAPI WinMain(
	HINSTANCE hinst,
	HINSTANCE hinstPrev,
	PSTR szCmdLine,
	int iCmdShow
	)
{
	//
	// Initialize basic variables from Petzold "Hello, Windows" C example
	//

    int result = 0;
	BOOL WinSockInitialized = FALSE;

	// Save hinstance 

	Globals.hinst = hinst;
	Globals.szAppName = TEXT("CloneLocker");
	Globals.Quitting = FALSE;
	Globals.guiThreadId = GetCurrentThreadId();

	//
	// Only allow one instance of the ClientLocker to run per user
	//

	// Open mutex and keep it open, it will implicitly close if 
	// this process closes
	{
		HANDLE mutexSingleInstance;

		mutexSingleInstance = CreateMutex(NULL, TRUE, TEXT("ClientLocker_SingleInstance"));
		if (mutexSingleInstance == NULL) {
			MessageBox(
				NULL,
				TEXT("Internal error trying to get access to single instance mutex."), 
				Globals.szAppName,
				MB_ICONERROR
			);
			goto main_cleanup;
		}
		if (GetLastError() == ERROR_ALREADY_EXISTS) {
			MessageBox(
				NULL,
				TEXT("Only one instance of CloneLocker can run at a time."), 
				Globals.szAppName,
				MB_ICONERROR
			);
			goto main_cleanup;
		}
	}


    // Initialize Winsock.  We implement both client and server in the same executable,
	// so it doesn't make sense to delegate this bit to one or the other.  We just say
	// "Hey, we use winsock services" and leave it at that.
	{
		WSADATA wsaData;
		int iResultInit = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (iResultInit != 0) {
			DbgPrint(TEXT("Server WSAStartup failed with error: %d\n"), iResultInit);
			goto main_cleanup;
		}
		WinSockInitialized = TRUE;
	}

	//
	// Now do the initialization of all the threads to handle the requests
	// from the driver.
	//
	{
		HRESULT hr = InitGui(iCmdShow);
		if (IS_ERROR(hr)) {
			DbgPrint(TEXT("InitGui() failed, hresult = 0x%08x\n"), hr);
			goto main_cleanup;
		}
	}

    //  Open a commuication channel to the filter
	{
		HRESULT hr = InitFilter();
		if (IS_ERROR(hr)) {
			DbgPrint(TEXT("InitFilter() failed, hresult = 0x%08x\n"), hr);
			goto main_cleanup;
		}
	}

	// If we are a server instance, then load the server first
	// This can be changed while the program is running, also if a
	// connection can't be connected to the server then warnings
	// need to be presented in the UI to say "No server, do you
	// want to connect anyway."

	{
		HRESULT hr = InitServer();
		if (IS_ERROR(hr)) {
			DbgPrint(TEXT("InitServer() failed, hresult = 0x%08x\n"), hr);
			goto main_cleanup;
		}
	}

	{
		HRESULT hr = InitClient();
		if (IS_ERROR(hr)) {
			DbgPrint(TEXT("InitClient() failed, hresult = 0x%08x\n"), hr);
			goto main_cleanup;
		}
	}

	result = RunMainGuiLoop();

main_cleanup:

	if (WinSockInitialized) {
		WSACleanup();
		WinSockInitialized = FALSE;
	}

	if (
		IS_ERROR(ShutdownGui()) 
		|| IS_ERROR(ShutdownFilter())
		|| IS_ERROR(ShutdownServer())
		|| IS_ERROR(ShutdownClient())
	) {
		MessageBox(
			NULL,
			TEXT("Could not shutdown cleanly, using TerminateProcess."), 
			Globals.szAppName,
			MB_ICONERROR
		);

		TerminateProcess(GetCurrentProcess(), 1);
	}

    return result;
}