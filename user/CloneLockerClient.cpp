//
// CloneLockerClient.cpp
// Copyright (C) 2013 HostileFork.com
//
// Network client routines for CloneLocker.  The client currently holds
// a network connection to the server for the duration of the program.
// This connection is probed each time a file access request is made
// from the filter component to make a decision of whether access to
// a file should be left alone -or- if the CloneLocker should use a
// CreateFile request to snatch the access before the normal course of
// events run.
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

#include "CloneLocker.h"
#include "CloneLockerGui.h"
#include "CloneLockerServer.h"
#include "CloneLockerFilter.h"
#include "CloneLockerClient.h"

// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

SOCKET ConnectSocket = INVALID_SOCKET;

//
// InitClient
//
HRESULT InitClient() {
    struct addrinfo* result = NULL;
	struct addrinfo* ptr = NULL;
	struct addrinfo hints;

    int iResult;    
	char* server_name = "localhost";

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo(server_name, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        DbgPrint(TEXT("getaddrinfo failed with error: %d\n"), iResult);
        return E_FAIL;
    }

    // Attempt to connect to an address until one succeeds
    for(ptr = result; ptr != NULL; ptr = ptr->ai_next) {

        // Create a SOCKET for connecting to server
        ConnectSocket = socket(
			ptr->ai_family,
			ptr->ai_socktype, 
            ptr->ai_protocol
		);

        if (ConnectSocket == INVALID_SOCKET) {
            DbgPrint(TEXT("socket failed with error: %ld\n"), WSAGetLastError());
            return E_FAIL;
        }

        // Connect to server.
        iResult = connect(ConnectSocket, ptr->ai_addr, (int) ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET) {
        DbgPrint(TEXT("Unable to connect to server!\n"));
        return E_FAIL;
    }

	return S_OK;
}


HRESULT ShutdownClient() {
    int iResult;

	// shutdown the connection since no more data will be sent
    iResult = shutdown(ConnectSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        DbgPrint(TEXT("shutdown failed with error: %d\n"), WSAGetLastError());
        closesocket(ConnectSocket);
        return E_FAIL;
    }
	return S_OK;
}


//
// SendAccessRequestToServer
//
cJSON* SendAccessRequestToServer(ACCESS_TRACK_STRUCT* tracker) {

	DbgPrint(TEXT("SendAccessRequestToServer() entered.\n"));

	assert(GetCurrentThreadId() != Globals.guiThreadId);

	DbgPrint(TEXT("SendAccessRequestToServer: Well, at least we're not on the GUI thread...\n"));

	// the weak bit here is the need to turn a WCHAR string into UTF-8
	// we also need to indicate what kind of access is requested
	cJSON* requestJson = cJSON_CreateObject();
	cJSON_AddItemToObject(requestJson, "Filename", cJSON_CreateString("tracker->FileName"));

    char *requestString = cJSON_Print(requestJson);
	cJSON* responseJson = NULL;

    char recvbuf[DEFAULT_BUFLEN];
    int iResult;
    int recvbuflen = DEFAULT_BUFLEN;

    // Send an initial buffer
    iResult = send(ConnectSocket, requestString, (int)strlen(requestString), 0 );
    if (iResult == SOCKET_ERROR) {
        DbgPrint(TEXT("Client send failed with error: %d\n"), WSAGetLastError());
        closesocket(ConnectSocket);
        return NULL;
    }

    DbgPrint(TEXT("Client bytes Sent: %ld\n"), iResult);

    // Receive until the peer closes the connection
    do {
        iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
        if (iResult > 0) {
            DbgPrint(TEXT("Client bytes received: %d\n"), iResult);
			
			char* responseString = recvbuf;
			responseJson = cJSON_Parse(responseString);

		} else if (iResult == 0) {
            DbgPrint(TEXT("Client connection closed\n"));
			return NULL;
		} else {
            DbgPrint(TEXT("Client recv failed with error: %d\n"), WSAGetLastError());
			return NULL;
		}
    } while (iResult > 0);

	BOOL SafeToOpen = cJSON_GetObjectItem(responseJson, "SafeToOpen")->type == cJSON_True;

	SetGuiFileAccessList(responseJson);

	ReplyToFilterAndFreeTracker(tracker, SafeToOpen);

	DbgPrint(TEXT("SendAccessRequestToServer() exited.\n"));

	return responseJson;
}