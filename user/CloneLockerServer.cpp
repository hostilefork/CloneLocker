//
// CloneLockerServer.cpp
// Copyright (C) 2013 HostileFork.com
//
// Server mode component of the CloneLocker executable.  A CloneLocker
// user-mode instance can be either client, or client and server; hence a
// client/server instance talks to itself.  The server's job is to inform the
// client regarding whether it may open a file without interruption or if
// the client instance should intervene by taking a lock on its own before
// the request has been fulfilled.
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

#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#include "cJSON.h"

#include "CloneLocker.h"
#include "CloneLockerGui.h"
#include "CloneLockerServer.h"

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

HANDLE serverMutex = (HANDLE) NULL;
HANDLE serverAcceptThread = (HANDLE) NULL;

HANDLE serverReadyEvent = (HANDLE) NULL;


//
// ProcessAndFreeClientAccessRequest
//
char* ProcessAndFreeClientAccessRequest(cJSON* requestJson) {
	DbgPrint(TEXT("ProcessAndFreeClientAccessRequest() entered.\n"));

	cJSON* replyJson = NULL;
	char* replyString = NULL;

	// The mutex is necessary because we may be answering here granting
	// privileges... and we don't want to accidentally get a race condition
	// where we let more than one client to get that privilege.
	WaitForSingleObject(serverMutex, INFINITE);

	replyJson = GetFileAccessListFromGui();

	// This decision actually needs to be made based on looking at the above
	// list and the request.  For now we will just be blind, but it has to be
	// made AND the Gui needs to get the new access permissions before we
	// release the mutex!
	cJSON_Delete(requestJson);
	SetGuiFileAccessList(replyJson);
	cJSON_AddBoolToObject(replyJson, "SafeToOpen", FALSE);
	replyString = cJSON_Print(replyJson);

	// Here we would be doing the send of out!  It will be parsed on the
	// receiving end.
	DbgPrint(TEXT("Responding with JSON, %s\n"), replyString);
	OutputDebugStringA(replyString);
	cJSON_Delete(replyJson);
	replyJson = NULL;

	ReleaseMutex(serverMutex);

	DbgPrint(TEXT("ProcessAndFreeClientAccessRequest() exited.\n"));

	return replyString;
}


//
// ServerListenThreadProc
//
DWORD WINAPI
ServerListenerThreadProc(
    __in LPVOID lpVoidClientSocket
   )
{
	DWORD result = 0;
	DWORD BadThingHappened = 1;

	SOCKET ClientSocket = (SOCKET) lpVoidClientSocket;

	int iResult;
	char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;

	// Receive until the peer shuts down the connection
    do {
        iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
        if (iResult > 0) {
            DbgPrint(TEXT("Server bytes received: %d\n"), iResult);

			// We received a JSON query asking about the server state
			// This state is encoded in the ListView widget
			recvbuf[iResult] = 0; // Necessary?
			cJSON* requestJson = cJSON_Parse(recvbuf);
			char* responseString = ProcessAndFreeClientAccessRequest(requestJson);
			iResult = send(ClientSocket, responseString, strlen(responseString), 0);
			free(responseString);

            if (iResult == SOCKET_ERROR) {
                printf("Server send failed with error: %d\n", WSAGetLastError());
                result = BadThingHappened;
				goto cleanup_serverlistener;
            }

            DbgPrint(TEXT("Server bytes sent: %d\n"), iResult);
        } else if (iResult == 0) {
            DbgPrint(TEXT("Server connection closing...\n"));
		} else  {
            DbgPrint(TEXT("Server recv failed with error: %d\n"), WSAGetLastError());
			result = BadThingHappened;
            goto cleanup_serverlistener;
        }

    } while (iResult > 0);

cleanup_serverlistener:

	return result;
}



//
// ServerAcceptThreadProc
//
DWORD WINAPI
ServerAcceptThreadProc(
    __in LPVOID unused
   )
{
	DbgPrint(TEXT("ServerAcceptThreadProc() entered...\n"));

	const DWORD BadThingHappened = 1;

	DWORD result = 0;

    SOCKET ListenSocket = INVALID_SOCKET;

    addrinfo *addrinfoResult = NULL;

    // Resolve the server address and port
	{
	    addrinfo hints;

		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		hints.ai_flags = AI_PASSIVE;

		int iGetAddrInfoResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &addrinfoResult);
		if (iGetAddrInfoResult != 0) {
			DbgPrint(TEXT("Server getaddrinfo failed with error: %d\n"), iGetAddrInfoResult);
			result = BadThingHappened;
			goto cleanup_serveraccept;
		}
	}

    // Create a SOCKET for connecting to server
    ListenSocket = socket(addrinfoResult->ai_family, addrinfoResult->ai_socktype, addrinfoResult->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        DbgPrint(TEXT("Server socket failed with error: %ld\n"), WSAGetLastError());
		result = BadThingHappened;
        goto cleanup_serveraccept;
    }

    // Setup the TCP listening socket
	{
		int iBindResult = bind(ListenSocket, addrinfoResult->ai_addr, (int) addrinfoResult->ai_addrlen);
		if (iBindResult == SOCKET_ERROR) {
			DbgPrint(TEXT("Server bind failed with error: %d\n"), WSAGetLastError());
			result = BadThingHappened;
			goto cleanup_serveraccept;
		}
	}

	if (!SetEvent(serverReadyEvent)) {
		DbgPrint(TEXT("Server SetEvent failed (%d)\n"), GetLastError());
        result = BadThingHappened;
		goto cleanup_serveraccept;
	}

	// Does this belong in the loop, or outside of it?  Guess I'll find out when
	// there is more than one client.
	{
		int iListenResult = listen(ListenSocket, SOMAXCONN);
		if (iListenResult == SOCKET_ERROR) {
			DbgPrint(TEXT("Server listen failed with error: %d\n"), WSAGetLastError());
			result = BadThingHappened;
			goto cleanup_serveraccept;
		}
	}
	
	DbgPrint(TEXT("Entering the loop for the server.  Why not!\n"));

	// At the moment, this loop just opens a thread each time a connection is requested
	// The thread dies if the connection dies but there is no other mechanics
	// FIX!
	while (!Globals.Quitting) {
		// Accept a client socket
	    SOCKET ClientSocket = accept(ListenSocket, NULL, NULL);
		if (ClientSocket == INVALID_SOCKET) {
			DbgPrint(TEXT("Server accept failed with error: %d\n"), WSAGetLastError());
			result = BadThingHappened;
			goto cleanup_serveraccept;
		}

		DWORD listenerThreadId;
		//  Create the thread that listens for requests from the driver
		HANDLE listenerThread = CreateThread(
			NULL,
			0,
			ServerListenerThreadProc,
			(LPVOID) ClientSocket, // Thread parameters, none ATM
			0,
			&listenerThreadId
		);

		if (listenerThread == NULL) {
			//  Couldn't create thread.
			HRESULT hr = GetLastError();
			DbgPrint(TEXT("ERROR: Couldn't create thread: %d\n"), hr);
			result = BadThingHappened;
			goto cleanup_serveraccept;
		}
	}

cleanup_serveraccept:

	if (addrinfoResult != NULL) {
		freeaddrinfo(addrinfoResult);
		addrinfoResult = NULL;
	}

	if (ListenSocket != INVALID_SOCKET) {
		closesocket(ListenSocket);
		ListenSocket = INVALID_SOCKET;
	}

#ifdef HAVE_HANDLING_FOR_CLIENT_SOCKET_SHUTDOWN
	// This would have to be a loop of some kind over all the socket threads
	// that would kill them cleanly
	if (ClientSocket != INVALID_SOCKET) {
		iResult = shutdown(ClientSocket, SD_SEND);
		if (iResult == SOCKET_ERROR) {
			DbgPrint(TEXT("Server shutdown failed with error: %d\n"), WSAGetLastError());
		}

		closesocket(ClientSocket);
		ClientSocket = INVALID_SOCKET;
	}
#endif

	DbgPrint(TEXT("ServerAcceptThreadProc() exited...\n"));

    return 0;
}


//
// InitServer
//
HRESULT InitServer() {

	DbgPrint(TEXT("Entering InitServer()...!\n"));

	HRESULT hr = S_OK;

	serverMutex = CreateMutex(NULL, FALSE, NULL);

    // Create a manual-reset event object. The write thread sets this
    // object to the signaled state when it finishes writing to a 
    // shared buffer. 

    serverReadyEvent = CreateEvent( 
		NULL, // default security attributes
		TRUE, // manual-reset event
		FALSE, // initial state is nonsignaled
		TEXT("serverReadyEvent")  // object name
	); 

	DWORD serverAcceptThreadId;
    //  Create the thread that listens for requests from the driver
    serverAcceptThread = CreateThread(
		NULL,
		0,
		ServerAcceptThreadProc,
		(LPVOID) 0, // Thread parameters, none ATM
		0,
		&serverAcceptThreadId
	);

    if (serverAcceptThread == NULL) {
        //  Couldn't create thread.
        hr = GetLastError();
        DbgPrint(TEXT("ERROR: Couldn't create thread: %d\n"), hr);
        goto initserver_cleanup;
    }

	// Currently we make sure the server is running before we start any clients,
	// including this client if we are configured as a client/server.  It has
	// to accept the connection
	if (WaitForSingleObject(serverReadyEvent, INFINITE) != WAIT_OBJECT_0) {
		hr = E_FAIL;
		DbgPrint(TEXT("ERROR: server thread didn't signal ready event.\n"));
		goto initserver_cleanup;
	}

initserver_cleanup:

	DbgPrint(TEXT("Exiting InitServer()...!\n"));
	return hr;
}


HRESULT ShutdownServer() {
	if (serverMutex != (HANDLE) NULL) {
		CloseHandle(serverMutex);
		serverMutex = (HANDLE) NULL;
	}
	if (serverAcceptThread != (HANDLE) NULL) {
		CloseHandle(serverAcceptThread);
		serverAcceptThread = (HANDLE) NULL;
	}
	if (serverReadyEvent != (HANDLE) NULL) {
		CloseHandle(serverReadyEvent);
		serverReadyEvent = (HANDLE) NULL;
	}
	return S_OK;
}