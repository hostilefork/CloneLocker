//
// CloneLockerGui.h
// Copyright (C) 2013 HostileFork.com
//
// Definitions for the graphical user interface of CloneLocker.  Currently
// the GUI keeps track of the state of the files being monitored by the
// system and this information is not stored anywhere but inside of the
// ListView widget, however a cleaner division would put that information
// into the CloneLockerClient portion.
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

#ifndef __CLONELOCKERGUI_H__
#define __CLONELOCKERGUI_H__

#include "CloneLocker.h"

#include "cJSON.h"

// Custom Windows Messages
typedef UINT WM;

HRESULT InitGui(int iCmdShow);
HRESULT ShutdownGui();

int RunMainGuiLoop();
cJSON* GetFileAccessListFromGui();
void SetGuiFileAccessList(cJSON* json);

#endif // __CLONELOCKERGUI_H__