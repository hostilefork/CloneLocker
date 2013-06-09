//
// CloneLockerFilter.h
// Copyright (C) 2013 HostileFork.com
//
// Definitions for the part of the user mode component that communicates
// with the "miniFilter" device driver that provides notifications when
// access requests are made to the filesystem.
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

#ifndef __CLONELOCKERFILTER_H__
#define __CLONELOCKERFILTER_H__

#include "CloneLocker.h"

#include "CloneLockerClient.h"

HRESULT InitFilter();
HRESULT ShutdownFilter();
HRESULT ReplyToFilterAndFreeTracker(ACCESS_TRACK_STRUCT* tracker, BOOL SafeToOpen);

#endif // __CLONELOCKERFILTER_H__