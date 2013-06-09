//
// CloneLockerServer.h
// Copyright (C) 2013 HostileFork.com
//
// Definitions for the server component of CloneLocker.
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

#ifndef __CLONELOCKERSERVER_H__
#define __CLONELOCKERSERVER_H__

#include "CloneLocker.h"

HRESULT InitServer();
HRESULT ShutdownServer();

// Actually an internal routine, just testing
char* ProcessAndFreeClientAccessRequest(cJSON* request);

#endif // __CLONELOCKERSERVER_H__