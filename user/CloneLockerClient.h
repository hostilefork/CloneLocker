//
// CloneLockerClient.h
// Copyright (C) 2013 HostileFork.com
//
// Definitions for the network client portion of the user mode
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

#ifndef __CLONELOCKERCLIENT_H__
#define __CLONELOCKERCLIENT_H__

#include "CloneLocker.h"

#include "cJSON.h"

typedef struct {
	// Filename to which access is requested, and we are holding
	// it up...!
	PWCH FilePath;

	// This corresponds to the MessageId in a FILTER_MESSAGE_HEADER
	// Necessary to know which filter message you are replying to
	// when a reply is to be finally given back to the driver!
    ULONGLONG MessageId;
} ACCESS_TRACK_STRUCT;

HRESULT InitClient();
HRESULT ShutdownClient();

// The synchronous send and it returns the JSON.  There is
// probably a cleaner design for this, just trying to make
// forward progress.
cJSON* SendAccessRequestToServer(ACCESS_TRACK_STRUCT* tracker);

#endif // __CLONELOCKERCLIENT_H__