//
// CloneLockerFilter.cpp
// Copyright (C) 2013 HostileFork.com
//
// This portion of the CloneLocker code is designed to speak to the
// notifications coming back from the driver.
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
#include "CloneLockerShared.h"
#include "CloneLockerGui.h"
#include "CloneLockerFilter.h"

// REVIEW: Obviously wrong way to do this, I just had the WDK on the
// W: drive in the virtual machine and this got the XP VM to build the
// user mode component.
#pragma comment (lib, "W:\\lib\\wxp\\i386\\fltlib.lib")

// The listener thread, polls quitting
HANDLE filterThread = (HANDLE) NULL;

// miniFilter communication variables
HANDLE Port = (HANDLE) NULL;
HANDLE Completion = (HANDLE) NULL;

typedef struct _LOCKER_MESSAGE {

    // Required structure header.
    FILTER_MESSAGE_HEADER MessageHeader;

    // Private scanner-specific fields begin here.
    LOCKER_NOTIFICATION Notification;

    // Overlapped structure: this is not really part of the message
    // However we embed it instead of using a separately allocated overlap structure
    OVERLAPPED Ovlp;

} LOCKER_MESSAGE, *PLOCKER_MESSAGE;

typedef struct _LOCKER_REPLY_MESSAGE {

    // Required structure header.
    FILTER_REPLY_HEADER ReplyHeader;

    // Private scanner-specific fields begin here.
    LOCKER_REPLY Reply;

} LOCKER_REPLY_MESSAGE, *PLOCKER_REPLY_MESSAGE;


//
// FilterThreadProc
//
// This is the thread procedure for the worker thread that sits waiting on the
// driver to send us filenames of the files that are being accessed.  We are
// told whether the file is being accessed (via some process calling
// CreateFile) or if the last reference on a file is being released.  It does
// a waiting block on GetQueuedCompletionStatus, but has a timeout of 1000
// milliseconds where it checks to see if the "Quitting" global has been set.
//
DWORD WINAPI
FilterThreadProc(
    __in LPVOID unused
   )
{
    HRESULT hr = S_OK;
	PLOCKER_MESSAGE message = NULL;

    while (!Globals.Quitting) {
        //  Poll for messages from the filter component to scan.

		LPOVERLAPPED pOvlp;
		DWORD outSize;
		ULONG_PTR key;

		BOOL result = GetQueuedCompletionStatus(
			Completion,
			&outSize,
			&key,
			&pOvlp,
			1000 // milliseconds, e.g. one second polling period to listen to the filter
			); 

		if (Globals.Quitting) {
			break;
		}

		if (!result) {
			if (pOvlp == NULL) {
				// A simple timeout, nothing happened!
				continue;
			} else {
				// An error occured...
				hr = HRESULT_FROM_WIN32(GetLastError());
				DbgPrint(TEXT("FilterThreadProc() encountered error with GetQueuedCompletionStatus(): %u\n"), GetLastError());
	            break;
			}
		}

        // Obtain the message note that the message we sent down via FilterGetMessage() may NOT be
        // the one dequeued off the completion queue: this is solely because there are multiple
        // threads per single port handle. Any of the FilterGetMessage() issued messages can be
        // completed in random order - and we will just dequeue a random one.
		message = CONTAINING_RECORD(pOvlp, LOCKER_MESSAGE, Ovlp);

        DbgPrint(TEXT("Received message, size %d\n"), pOvlp->InternalHigh);

		PLOCKER_NOTIFICATION notification = &message->Notification;

		DbgPrint(TEXT("Filename is %s"), notification->Buffer);

		{
			ACCESS_TRACK_STRUCT* tracker = new ACCESS_TRACK_STRUCT;
			tracker->MessageId = message->MessageHeader.MessageId;
			tracker->FilePath = new WCHAR [notification->Length + 1];
			// Driver guarantees this was null-terminated...
			memcpy(tracker->FilePath, notification->Buffer, notification->Length + sizeof(WCHAR));

			// Probably fold the above code into this...
			cJSON* resultJson = SendAccessRequestToServer(tracker);

			// for the moment, just ignore but it is synchronous
			cJSON_Delete(resultJson);
		}

		// Ask for another record, although it may well not be from the same thread
		// because we are blocking that one until the user mode reply comes!

        memset(&message->Ovlp, 0, sizeof(OVERLAPPED));

        hr = FilterGetMessage(
			Port,
			&message->MessageHeader,
			FIELD_OFFSET(LOCKER_MESSAGE, Ovlp),
			&message->Ovlp
		);

        if (hr != HRESULT_FROM_WIN32(ERROR_IO_PENDING)) {
            break;
        }
    }

    if (!SUCCEEDED(hr)) {
        if (hr == HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE)) {
            // Locker port disconncted.
            DbgPrint(TEXT("Locker: Port is disconnected, probably due to kernel mode component not running.\n"));
        } else {
            DbgPrint(TEXT("Locker: Unknown error occured. Error = 0x%X\n"), hr);
        }
    }

	if (message == NULL) {
		DbgPrint(TEXT("Message is NULL and this shouldn't happen."));
	} else {
	    delete message;
		message = NULL;
	}

	DbgPrint(TEXT("Exiting FilterThreadProc()\n"));
    return hr;
}


//
// InitFilter
//
HRESULT InitFilter() {
    PLOCKER_MESSAGE scanMsg;
    DWORD threadId;

	HRESULT hr = FilterConnectCommunicationPort(
		LockerPortName,
		0,
		NULL,
		0,
		NULL,
		&Port
	);

	if (IS_ERROR(hr)) {
		MessageBox(
			Globals.hwnd,
			TEXT("Could not connect to CloneLockerDriver.sys.\n") \
			TEXT("The service is either not started and/or not installed.\n") \
			TEXT("Try \"SC START CloneLocker\" from an administrator-enabled command line."),
			Globals.szAppName,
			MB_ICONERROR
		);
		return hr;
	}

    //Create a completion port to associate with this handle.
    Completion = CreateIoCompletionPort(
		Port,
		NULL,
		0,
		// two concurrent threads, for WinMain and our listener?  They both
		// might be using the port at the same time, but is the completion
		// part of that?  See documentation:
		// http://msdn.microsoft.com/en-us/library/windows/desktop/aa363862(v=vs.85).aspx
		2
	);

    if (Completion == NULL) {
        DbgPrint(TEXT("ERROR: Creating completion port: %d\n"), GetLastError());
        CloseHandle(Port);
        return 3;
    }

    DbgPrint(TEXT("Locker: Port = 0x%p Completion = 0x%p\n"), Port, Completion);

    // Create the thread that listens for requests from the filter driver
    filterThread = CreateThread(
		NULL,
		0,
		FilterThreadProc,
		(LPVOID) 0, // Thread parameters, none ATM
		0,
		&threadId
	);

    if (filterThread == NULL) {
        //  Couldn't create thread.
        hr = GetLastError();
        DbgPrint(TEXT("ERROR: Couldn't create thread: %d\n"), hr);
        return 4;
    }

#pragma prefast(suppress:__WARNING_MEMORY_LEAK, "scanMsg will not be leaked because it is freed in LockerWorker")

	// Although the kernel can send variable amounts of memory, the user mode cannot receive it
	// variable.  It may be possible to get the required number of bytes, reallocate the message
	// buffer, and try again... look into that later.
	// For now: http://answers.yahoo.com/question/index?qid=20070626110330AAAHC74

	// We added 32768 to this before for no real apparent reason
    scanMsg = new LOCKER_MESSAGE;

    if (scanMsg == NULL) {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

	// Overlapped piggy backs onto our filter message, avoids an additional allocation
	// This is important... why?
    memset(&scanMsg->Ovlp, 0, sizeof(OVERLAPPED));

    //
    //  Request messages from the filter driver.
    //

    hr = FilterGetMessage(
		Port,
		&scanMsg->MessageHeader,
		FIELD_OFFSET(LOCKER_MESSAGE, Ovlp),
		&scanMsg->Ovlp
	);

    if (hr != HRESULT_FROM_WIN32(ERROR_IO_PENDING)) {
        delete scanMsg;
		DbgPrint(TEXT("CloneLocker didn't get ERROR_IO_PENDING from FilterGetMessage.\n"));
        return hr;
    }

	return S_OK;
}


//
// ShutdownFilter
//
// Just waits five seconds then warn user we are using the ugly TerminateProcess...
//
HRESULT ShutdownFilter() {
	HRESULT hr = S_OK;
	
	if (filterThread != (HANDLE) NULL) {
		hr = WaitForSingleObject(filterThread, 5 * 1000);
		if (hr == WAIT_OBJECT_0) {
			CloseHandle(filterThread);
			// we're going to kill the whole shebang with TerminateProcess
			// if we didn't exit that thread.
		}
		filterThread = (HANDLE) NULL;
	}

	if (Port != (HANDLE) NULL) {
		CloseHandle(Port);
		Port = (HANDLE) NULL;
	}
	
	if (Port != (HANDLE) NULL) {
		CloseHandle(Completion);
		Port = (HANDLE) NULL;
	}

	return hr == WAIT_OBJECT_0 ? S_OK : E_FAIL;
}


//
// ReplyToFilterAndFreeTracker
//
HRESULT ReplyToFilterAndFreeTracker(ACCESS_TRACK_STRUCT* tracker, BOOL SafeToOpen) {
	HRESULT hr;

	LOCKER_REPLY_MESSAGE replyMessage;
	replyMessage.ReplyHeader.Status = 0;
	replyMessage.ReplyHeader.MessageId = tracker->MessageId;
	replyMessage.Reply.SafeToOpen = SafeToOpen;

	DbgPrint(TEXT("Replying message, SafeToOpen: %d\n"), SafeToOpen);

	hr = FilterReplyMessage(
		Port,
		(PFILTER_REPLY_HEADER) &replyMessage,
		sizeof(replyMessage)
	);

	delete[] tracker->FilePath;
	delete tracker;

	return hr;
}