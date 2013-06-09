/*
 * CloneLockerDriver.h
 * Copyright (C) 2013 HostileFork.com
 *
 * These definitions are the ones used by the kernel mode component
 * of CloneLocker, which is currently much more basic than the user mode
 * component.
 *
 * 	http://social.msdn.microsoft.com/Forums/en-US/wfp/thread/84558578-b8cb-4619-983a-ee72c3c9fea7/
 *
 * This file is part of CloneLocker
 *
 * CloneLocker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CloneLocker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with CloneLocker.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __CLONELOCKERDRIVER_H__
#define __CLONELOCKERDRIVER_H__


/*
 * DDK-specific headers, you have to have the 7.1.0 Driver Kit for these
 * http://www.microsoft.com/en-us/download/confirmation.aspx?id=11800
 */
#include <ntifs.h>
#include <fltKernel.h>
#include <ntstrsafe.h>

/*
 * The DDK does not define BOOL but we need it for our shared file.  You
 * really have to wonder about people living in a world where something
 * so basic couldn't be nailed down correctly.  I swear, things like this
 * infuriate me:
 *
 * http://blogs.msdn.com/b/oldnewthing/archive/2004/12/22/329884.aspx
 */
typedef int BOOL;

#include "CloneLockerShared.h"



typedef struct _GLOBALS_TYPE {

    /* The object that identifies this driver. */
    PDRIVER_OBJECT DriverObject;

    /* The filter handle that results from a call to FltRegisterFilter. */
    PFLT_FILTER Filter;

    /* Listens for incoming connection. */
    PFLT_PORT ServerPort;

    /* User process that connected to the port */
    PEPROCESS UserProcess;

    /* Client port for a connection to user-mode */
    PFLT_PORT ClientPort;

} GLOBALS_TYPE, *PGLOBALS_TYPE;

extern GLOBALS_TYPE Globals;


/*
 * This component seems unused and is based on a need that 
 * the scanner example had which this driver does not have.
 */
typedef struct _LOCKER_STREAM_HANDLE_CONTEXT {
    BOOL RescanRequired;
} LOCKER_STREAM_HANDLE_CONTEXT, *PLOCKER_STREAM_HANDLE_CONTEXT;


/*
 * This is another example of something that doesn't really seem
 * to apply after adapting the scanner to the new purposes.
 */
#pragma warning(push)
/* disable warnings for structures with zero length arrays. */
#pragma warning(disable:4200) 
typedef struct _LOCKER_CREATE_PARAMS {
    WCHAR String[0];
} LOCKER_CREATE_PARAMS, *PLOCKER_CREATE_PARAMS;
#pragma warning(pop)


/*
 * Prototypes for the startup and unload routines used for 
 * this filter, so that they may be defined in any order.  As
 * with most things textual-programming-based, I laugh at
 * this needless manual repetition, but just checked my watch
 * and this is the dark ages still.
 */

DRIVER_INITIALIZE DriverEntry;
NTSTATUS
DriverEntry (
    __in PDRIVER_OBJECT DriverObject,
    __in PUNICODE_STRING RegistryPath
    );

NTSTATUS
LockerUnload (
    __in FLT_FILTER_UNLOAD_FLAGS Flags
    );

NTSTATUS
LockerQueryTeardown (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
    );

FLT_PREOP_CALLBACK_STATUS
LockerPreCreate (
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext
    );

FLT_POSTOP_CALLBACK_STATUS
LockerPostCreate (
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in_opt PVOID CompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    );

FLT_PREOP_CALLBACK_STATUS
LockerPreCleanup (
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext
    );

FLT_PREOP_CALLBACK_STATUS
LockerPreWrite (
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext
    );

NTSTATUS
LockerInstanceSetup (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_SETUP_FLAGS Flags,
    __in DEVICE_TYPE VolumeDeviceType,
    __in FLT_FILESYSTEM_TYPE VolumeFilesystemType
    );

#endif /* __CLONELOCKERDRIVER_H__ */