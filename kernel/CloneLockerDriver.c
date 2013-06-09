/*
 * CloneLockerDriver.c
 * Copyright (C) 2013 HostileFork.com
 *
 * This code is basically cribbed from the Microsoft DDK sample for
 * a "scanner".  I will point out that the example seems completely
 * broken for what it claims to do because it looked for the word
 * "foul" in a file, but in its search it read in some kind of sector
 * sized boundary so if it saw "fo" at the end of that boundary
 * then it would miss "ul" in the next.  I will also point out that the
 * number of misguided acrobatics regarding saving on a memory
 * allocation or two and not explaining what they were actually 
 * doing has probably caused an infinity of bugs.
 * 
 * But that said, it does seem the world of miniFilters is better than
 * how this kind of thing used to be done.  Read up:
 *
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff538896(v=vs.85).aspx
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

#include "CloneLockerDriver.h"

typedef unsigned char BYTE;

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")

/*
 * Structure that contains all the global data structures
 * used throughout the driver.
 */
GLOBALS_TYPE Globals;


/*
 * This is a static list of file name extensions files we are interested in scanning
 * Reduced for the moment to just .TXT files
 */
const UNICODE_STRING LockerExtensionsToScan[] = {
	RTL_CONSTANT_STRING(L"txt"),
	{0, 0, NULL}
};


/*
 * LockerCheckExtension
 *
 * Check if it matches any one of our static extension list
 */
BOOL
LockerCheckExtension(
    __in PUNICODE_STRING Extension
   )
{
    const UNICODE_STRING *ext = LockerExtensionsToScan;

    if(Extension->Length == 0) {
        return FALSE;
    }

    while(ext->Buffer != NULL) {
        if(RtlCompareUnicodeString(Extension, ext, TRUE) == 0) {
            return TRUE;
        }
        ext++;
    }

    return FALSE;
}


/*
 *  Function prototypes
 */

NTSTATUS
LockerPortConnect(
    __in PFLT_PORT ClientPort,
    __in_opt PVOID ServerPortCookie,
    __in_bcount_opt(SizeOfContext) PVOID ConnectionContext,
    __in ULONG SizeOfContext,
    __deref_out_opt PVOID *ConnectionCookie
   );

VOID
LockerPortDisconnect(
    __in_opt PVOID ConnectionCookie
   );

NTSTATUS
LockerMessage (
    __in PVOID ConnectionCookie,
    __in_bcount_opt(InputBufferSize) PVOID InputBuffer,
    __in ULONG InputBufferSize,
    __out_bcount_part_opt(OutputBufferSize,*ReturnOutputBufferLength) PVOID OutputBuffer,
    __in ULONG OutputBufferSize,
    __out PULONG ReturnOutputBufferLength
    );

NTSTATUS
BlockingCallUserModeRegardingFileOperation(
    __in PFLT_INSTANCE Instance,
    __in PFILE_OBJECT FileObject,
	__in PFLT_FILE_NAME_INFORMATION nameInfo,
    __in BOOL IsCreate
   );

BOOL
LockerCheckExtension(
    __in PUNICODE_STRING Extension
   );

/*
 * Assign text sections for each routine.
 */

#ifdef ALLOC_PRAGMA
    #pragma alloc_text(INIT, DriverEntry)
    #pragma alloc_text(PAGE, LockerInstanceSetup)
    #pragma alloc_text(PAGE, LockerPreCreate)
    #pragma alloc_text(PAGE, LockerPortConnect)
    #pragma alloc_text(PAGE, LockerPortDisconnect)
	#pragma alloc_text(PAGE, LockerMessage)
#endif


/*
 * ContextRegistration[]
 *
 * This required parameter to the FLT_REGISTRATION is not explained in the
 * DDK examples.  (They are not good at explaining things, I've noticed.)
 * But essentially this says that we are a FLT_STREAMHANDLE_CONTEXT and
 * that is good enough to get us callbacks when people are requesting
 * access on files.
 *
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff551964(v=vs.85).aspx
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff544629(v=vs.85).aspx
 */
const FLT_CONTEXT_REGISTRATION ContextRegistration[] = {
	{
		FLT_STREAMHANDLE_CONTEXT, // ContextType
		0, // Flags
		NULL, // ContextCleanupCallback
		sizeof(LOCKER_STREAM_HANDLE_CONTEXT), // Size
		'chBS', // PoolTag
		NULL, // ContextAllocateCallback
		NULL, // ContextFreeCallback
		NULL // Reserved, must be null
	}, {
		FLT_CONTEXT_END
	}
};


/*
 * Constant FLT_OPERATION_REGISTRATION structure for our filter.  This
 * initializes the callback routines our filter wants to register
 * for.  This is only used to register with the filter manager
 */
const FLT_OPERATION_REGISTRATION Callbacks[] = {
	{
		IRP_MJ_CREATE,
		0,
		LockerPreCreate,
		LockerPostCreate
	}, {
		IRP_MJ_CLEANUP,
		0,
		LockerPreCleanup,
		NULL
	}, {
		IRP_MJ_WRITE,
		0,
		LockerPreWrite,
		NULL
	}, {
		IRP_MJ_OPERATION_END
	}
};


/*
 * FilterRegistration
 *
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff544811(v=vs.85).aspx
 */
const FLT_REGISTRATION FilterRegistration = {
    sizeof(FLT_REGISTRATION), //  Size
    FLT_REGISTRATION_VERSION, //  Version
    0, //  Flag
    ContextRegistration, //  Context Registration.
    Callbacks, //  Operation callbacks
    LockerUnload, //  FilterUnload
    LockerInstanceSetup, //  InstanceSetup
    LockerQueryTeardown, //  InstanceQueryTeardown
    NULL, //  InstanceTeardownStart
    NULL, //  InstanceTeardownComplete
    NULL, //  GenerateFileName
    NULL, //  GenerateDestinationFileName
    NULL //  NormalizeNameComponent
};


/*
 * DriverEntry
 *
 * Main device driver entry point (the driver equivalent of "WinMain", 'cept for
 * drivers).  This is the first routine called:
 * 
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff557309(v=vs.85).aspx
 *
 * In this specific initialization, we register with something called the
 * "Filter Manager"...which gives us something like an "Application Framework"
 * where much of the boilerplate work is taken care of for us.  Except this is
 * a "Driver Framework", and it allows us to provide only the parts we are
 * interested in writing and accept most of the defaults.  By stylizing our code
 * in this way we are creating something known as a miniFilter:
 *
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff538896(v=vs.85).aspx
 */
NTSTATUS
DriverEntry(
    __in PDRIVER_OBJECT DriverObject,
    __in PUNICODE_STRING RegistryPath
   )
{
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING uniString;
    PSECURITY_DESCRIPTOR sd;
    NTSTATUS status;

    UNREFERENCED_PARAMETER(RegistryPath);

    /*  Register with filter manager. */
    status = FltRegisterFilter(
		DriverObject,
		&FilterRegistration,
		&Globals.Filter
	);


    if (!NT_SUCCESS(status)) {
        return status;
    }

     
    /*  Create a communication port. */
    RtlInitUnicodeString(&uniString, LockerPortName);

    
    /* We secure the port so only ADMINs & SYSTEM can access it. */
    status = FltBuildDefaultSecurityDescriptor(&sd, FLT_PORT_ALL_ACCESS);

    if (NT_SUCCESS(status)) {
        InitializeObjectAttributes(
			&oa,
			&uniString,
			OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
			NULL,
			sd
		);

        status = FltCreateCommunicationPort(
			Globals.Filter,
			&Globals.ServerPort,
			&oa,
			NULL,
			LockerPortConnect,
			LockerPortDisconnect,
			LockerMessage,
			1 /* MaxConnections: Only accept one connection! */
		);

        /*
         * Free the security descriptor in all cases. It is not needed once
         * the call to FltCreateCommunicationPort() is made.
         */
        FltFreeSecurityDescriptor(sd);

        if (NT_SUCCESS(status)) {

            /*
             * Start filtering I/O.  This is blocking, and the minifilter
			 * will keep running until the service is stopped, e.g. with
			 * "SC STOP CloneLocker" at the command line or using a GUI
			 * service manager.
             */
            status = FltStartFiltering(Globals.Filter);

            if (NT_SUCCESS(status)) {
                return STATUS_SUCCESS;
            }

            FltCloseCommunicationPort(Globals.ServerPort);
        }
    }

    FltUnregisterFilter(Globals.Filter);

    return status;
}




/*
 * FLT_REGISTRATION CALLBACKS
 * Filter registration hooks established by FltRegisterFilter
 */


/*
 * LockerUnload
 *
 * This is the unload routine for the Filter driver, which we hooked in when we
 * called FltRegisterFilter.  See PFLT_FILTER_UNLOAD_CALLBACK:
 *
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff551085(v=vs.85).aspx
 *
 * It unregisters the filter from the filter manager, and frees any allocated
 * global data structures.  This means we can stop and delete this service without
 * rebooting, which is made possible by hooking in underneath the FilterManager
 * (by contrast, that manager is a legacy File System Filter Driver...so it cannot
 * itself be safely unloaded without rebooting).
 *
 * Returns the final status of the deallocation routines.
 */
NTSTATUS
LockerUnload(
    __in FLT_FILTER_UNLOAD_FLAGS Flags
   )
{
    UNREFERENCED_PARAMETER(Flags);

    // Close the server port.
    FltCloseCommunicationPort(Globals.ServerPort);

    // Unregister the filter
    FltUnregisterFilter(Globals.Filter);

    return STATUS_SUCCESS;
}


/*
 * LockerInstanceSetup
 * 
 * This routine is called by the filter manager when a new instance is created.
 * See PFLT_INSTANCE_SETUP_CALLBACK:
 *
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff551096(v=vs.85).aspx
 *
 * There is a concept of being attached to a volume or not.  For instance, if
 * you load the filter driver and it is hooking filesystem requests, what if
 * a USB drive is plugged in or unplugged...should the filter receive
 * notifications for that?  The original scanner sample said:
 *
 *     "We specified in the registry that we only want manual attachments,
 *       so that is all we should receive here."
 *
 * Review what this means and what the consequences are for this driver and
 * handling new volumes.  Add debug messages to sort that out.
 */
NTSTATUS
LockerInstanceSetup(
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_SETUP_FLAGS Flags,
    __in DEVICE_TYPE VolumeDeviceType,
    __in FLT_FILESYSTEM_TYPE VolumeFilesystemType
   )
{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);
    UNREFERENCED_PARAMETER(VolumeFilesystemType);

    PAGED_CODE();

    ASSERT(FltObjects->Filter == Globals.Filter);

    //  Don't attach to network volumes.
    if(VolumeDeviceType == FILE_DEVICE_NETWORK_FILE_SYSTEM) {
		DbgPrint("!!! CloneLockerDriver.sys --- denied network filesystem instance\n");
		return STATUS_FLT_DO_NOT_ATTACH;
    }

	DbgPrint("!!! CloneLockerDriver.sys --- accept filesystem instance\n");
    return STATUS_SUCCESS;
}


/*
 * LockerQueryTeardown
 *
 * This is the instance detach routine for the filter, which is called by 
 * a filter manager when a user initiates a manual instance detach.
 * See: PFLT_INSTANCE_TEARDOWN_CALLBACK
 *
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff551098(v=vs.85).aspx
 *
 * This is a 'query' routine: if the filter does not want to support manual
 * detach, it can return a failure status...else STATUS_SUCCESS to allow
 * instance detach to happen.
 */
NTSTATUS
LockerQueryTeardown(
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
   )
{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);

    return STATUS_SUCCESS;
}



/*
 * FLTCREATECOMMUNICATIONPORT CALLBACKS
 * Filter registration hooks established by FltCreateCommunicationPort
 */


/*
 * LockerPortConnect
 *
 * This is called when user-mode connects to the server port - to establish a
 * connection.  See PFLT_CONNECT_NOTIFY under FltCreateCommunicationPort:
 *
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff541931(v=vs.85).aspx
 *
 * To accept the connection, we return STATUS_CONNECT.  Because
 * in FltCreateCommunicationPort we set MaxConnections to 1, we never have
 * to worry about denying this request...if the request makes it here we can
 * just assert there aren't any more.
 */
NTSTATUS
LockerPortConnect(
    __in PFLT_PORT ClientPort,
    __in_opt PVOID ServerPortCookie,
    __in_bcount_opt(SizeOfContext) PVOID ConnectionContext,
    __in ULONG SizeOfContext,
    __deref_out_opt PVOID *ConnectionCookie
   )
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(ServerPortCookie);
    UNREFERENCED_PARAMETER(ConnectionContext);
    UNREFERENCED_PARAMETER(SizeOfContext);
    UNREFERENCED_PARAMETER(ConnectionCookie);

    ASSERT(Globals.ClientPort == NULL);
    ASSERT(Globals.UserProcess == NULL);

    /* Set the user process and port. */
    Globals.UserProcess = PsGetCurrentProcess();
    Globals.ClientPort = ClientPort;

    DbgPrint("!!! ClientFilterDriver.sys --- connected, port=0x%p\n", ClientPort);
    return STATUS_SUCCESS;
}


/*
 * LockerPortDisconnect
 *
 * This is called when user-mode disconnects the server port - to establish a
 * connection.  See PFLT_CONNECT_NOTIFY under FltCreateCommunicationPort:
 *
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff541931(v=vs.85).aspx
 *
 * There is no return value.  In our case, we maintain only a single connection
 * to a single client process, so if we get a call we know who it's from.
 */
VOID
LockerPortDisconnect(
     __in_opt PVOID ConnectionCookie
    )
{
    UNREFERENCED_PARAMETER(ConnectionCookie);

    PAGED_CODE();

    DbgPrint("!!! CloneLockerDriver.sys --- disconnected, port=0x%p\n", Globals.ClientPort);

    /*
	 * Close our handle to the connection: note, since we limited max connections to 1,
     * another connect will not be allowed until we return from the disconnect routine.
	 * Sets ClientPort to NULL.
	 */
    FltCloseClientPort(Globals.Filter, &Globals.ClientPort);
	ASSERT(Globals.ClientPort == NULL);

    /* Reset the user-process field. */
    Globals.UserProcess = NULL;
}


/*
 * LockerMessage
 * 
 * This is called whenever a user mode application wishes to communicate
 * with this minifilter.  See PFLT_MESSAGE_NOTIFY under FltCreateCommunicationPort
 *
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff541931(v=vs.85).aspx
 *
 * The specific API which is called from user mode that makes a blocking
 * call here is FilterSendMessage:
 *
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff541513(v=vs.85).aspx
 *
 * I thought I might use one of these calls to unblock a blocked call from 
 * usermode in order to exit the client process.  But I didn't figure out
 * a way to re-entrantly call back to user mode from such a callback, and doing
 * so is probably a bad idea.  So I switched the client to poll for an exit
 * signal rather than try to break the INFINITE wait with a callback from
 * kernel mode.  Hence this is unused for the moment, but at the very least
 * a call here would be necessary to check the version match between the
 * driver and the client.
 *
 * Do notice that there are several issues documented in the MSDN regarding
 * the buffer alignments.  So read the documentation carefully when activating
 * this code!
 *
 * Returns the status of processing the message.
 */
NTSTATUS
LockerMessage (
    __in PVOID ConnectionCookie,
    __in_bcount_opt(InputBufferSize) PVOID InputBuffer,
    __in ULONG InputBufferSize,
    __out_bcount_part_opt(OutputBufferSize,*ReturnOutputBufferLength) PVOID OutputBuffer,
    __in ULONG OutputBufferSize,
    __out PULONG ReturnOutputBufferLength
    )

{
    PAGED_CODE();

#ifdef HAVE_MESSAGE_HANDLING
    LOCKER_COMMAND command;
    NTSTATUS status;


    UNREFERENCED_PARAMETER( ConnectionCookie );

    /*
     *                     **** PLEASE READ ****
     *
     *  The INPUT and OUTPUT buffers are raw user mode addresses.  The filter
     *  manager has already done a ProbedForRead (on InputBuffer) and
     *  ProbedForWrite (on OutputBuffer) which guarentees they are valid
     *  addresses based on the access (user mode vs. kernel mode).  The
     *  minifilter does not need to do their own probe.
     *
     *  The filter manager is NOT doing any alignment checking on the pointers.
     *  The minifilter must do this themselves if they care (see below).
     *
     *  The minifilter MUST continue to use a try/except around any access to
     *  these buffers.
     */

    if ((InputBuffer != NULL) &&
        (InputBufferSize >= (FIELD_OFFSET(COMMAND_MESSAGE,Command) +
                             sizeof(LOCKER_COMMAND)))) {

        try  {

            /*
             * Probe and capture input message: the message is raw user mode
             * buffer, so need to protect with exception handler
             */

            command = ((PCOMMAND_MESSAGE) InputBuffer)->Command;

        } except( EXCEPTION_EXECUTE_HANDLER ) {

            return GetExceptionCode();
        }

        switch (command) {

            case SendClientQuitMessage:

                
                /* Return as many log records as can fit into the OutputBuffer */

                if ((OutputBuffer == NULL) || (OutputBufferSize == 0)) {

                    status = STATUS_INVALID_PARAMETER;
                    break;
                }

                /*
                 * We want to validate that the given buffer is POINTER
                 * aligned.  But if this is a 64bit system and we want to
                 * support 32bit applications we need to be careful with how
                 * we do the check.  Note that the way SpyGetLog is written
                 * it actually does not care about alignment but we are
                 * demonstrating how to do this type of check.
                 */

#if defined(_WIN64)
                if (IoIs32bitProcess( NULL )) {

                    /*
                     * Validate alignment for the 32bit process on a 64bit
                     * system
                     */

                    if (!IS_ALIGNED(OutputBuffer,sizeof(ULONG))) {

                        status = STATUS_DATATYPE_MISALIGNMENT;
                        break;
                    }

                } else {
#endif

                    if (!IS_ALIGNED(OutputBuffer,sizeof(PVOID))) {

                        status = STATUS_DATATYPE_MISALIGNMENT;
                        break;
                    }

#if defined(_WIN64)
                }
#endif

				status = FltSendMessage(
					Globals.Filter,
					&Globals.ClientPort,
					notification,
					notificationLength,
					notification,
					&replyLength,
					NULL
				);
                break;


            case GetMiniSpyVersion:

                /*
                 * Return version of the MiniSpy filter driver.  Verify
                 * we have a valid user buffer including valid
                 * alignment
                 */

                if ((OutputBufferSize < sizeof( MINISPYVER )) ||
                    (OutputBuffer == NULL)) {

                    status = STATUS_INVALID_PARAMETER;
                    break;
                }

                /*
                 * Validate Buffer alignment.  If a minifilter cares about
                 * the alignment value of the buffer pointer they must do
                 * this check themselves.  Note that a try/except will not
                 * capture alignment faults.
                 */

                if (!IS_ALIGNED(OutputBuffer,sizeof(ULONG))) {
                    status = STATUS_DATATYPE_MISALIGNMENT;
                    break;
                }

                /*
                 * Protect access to raw user-mode output buffer with an
                 * exception handler
                 */
                try {
                    ((PMINISPYVER)OutputBuffer)->Major = MINISPY_MAJ_VERSION;
                    ((PMINISPYVER)OutputBuffer)->Minor = MINISPY_MIN_VERSION;
                } except( EXCEPTION_EXECUTE_HANDLER ) {
                      return GetExceptionCode();
                }

                *ReturnOutputBufferLength = sizeof( MINISPYVER );
                status = STATUS_SUCCESS;
                break;

            default:
                status = STATUS_INVALID_PARAMETER;
                break;
        }

    } else {

        status = STATUS_INVALID_PARAMETER;
    }

    return status;
#endif
	return STATUS_INVALID_PARAMETER;
}


/*
 * LockerPreCreate
 *
 * This is called a "Pre create callback", and you can read about it in 
 * PFLT_PRE_OPERATION_CALLBACK:
 * 
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff551109(v=vs.85).aspx
 *
 * This is a chance to hook an I/O operation before the underlying
 * filesystem knows it is going to happen.  Hence it makes a good moment
 * to signal the user-mode process to take a filesystem-level lock on
 * a file.
 * 
 * The original comment from scanner.c, which is no longer relevant but
 * perhaps informative, read thusly:
 *
 * "We need to remember whether this file has been opened for write access.
 * If it has, we'll want to rescan it in cleanup.  This scheme results in
 * extra scans in at least two cases:
 *    -- if the create fails(perhaps for access denied)
 *    -- the file is opened for write access but never actually written to
 * The assumption is that writes are more common than creates, and checking
 * or setting the context in the write path would be less efficient than
 * taking a good guess before the create."
 *
 * It did not call through to the user mode process, because it was too
 * early to determine if the file was legit.  I believe in our case we
 * do want to call user mode, so I am investigating that.  Returns:
 *
 * FLT_PREOP_SUCCESS_WITH_CALLBACK - If this is not our user-mode process.
 * FLT_PREOP_SUCCESS_NO_CALLBACK - All other threads.
 */ 
FLT_PREOP_CALLBACK_STATUS
LockerPreCreate(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext
   )
{
	NTSTATUS status;
    PFLT_FILE_NAME_INFORMATION nameInfo;
	BOOL scanFile;

    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

    PAGED_CODE();

    /* See if this create is being done by our user process. */
    if(IoThreadToProcess(Data->Thread) == Globals.UserProcess) {
        DbgPrint("!!! CloneLockerDriver.sys -- allowing create for trusted process \n");
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    /* Check if we are interested in this file. */
    status = FltGetFileNameInformation(
		Data,
		FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
		&nameInfo
	);

    if (!NT_SUCCESS(status)) {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    FltParseFileNameInformation(nameInfo);

    /* Check if the extension matches the list of extensions we are interested in */
    scanFile = LockerCheckExtension(&nameInfo->Extension);

    if (!scanFile) {
        /* Not an extension we are interested in */
	    FltReleaseFileNameInformation(nameInfo);
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

	/*
	 * If UNICODE_STRING was always zero terminated, you could use %ws
	 * and pass the buffer of the string.  But if you want to use the
	 * length field and a possible non-zero-terminated string, %wZ is it.
	 * http://www.winvistatips.com/can-dbgprint-unicode_string-t186503.html
	 */
	DbgPrint("FilE REQuEsT ReCEiVeD, prINtInG nAMe: %wZ\n", &(nameInfo->Name));

   (VOID) BlockingCallUserModeRegardingFileOperation(
	   FltObjects->Instance,
	   FltObjects->FileObject,
	   nameInfo,
	   TRUE
	);

    /* Release file name info, we're done with it */
    FltReleaseFileNameInformation(nameInfo);

    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}


/*
 * LockerPostCreate
 *
 * This is called a "Post create callback", and you can read about it in 
 * PFLT_POST_OPERATION_CALLBACK:
 * 
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff551107(v=vs.85).aspx
 *
 * The comment from scanner.c read:
 *
 * "We can't scan the file until after the create has gone to the filesystem,
 * since otherwise the filesystem wouldn't be ready to read the file for us."
 *
 * In our case, we are looking to find out when the file is released, to
 * send a notification to the user mode process so it can reconcile the
 * locks (and perhaps notify those who tried to take a lock that it has
 * become available, if it has).  This may not be the place to put that hook
 * as it's a "post create".  Perhaps cleanup is the right one?
 */
FLT_POSTOP_CALLBACK_STATUS
LockerPostCreate(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in_opt PVOID CompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
   )
{
	return FLT_POSTOP_FINISHED_PROCESSING;

#ifdef IS_SCAN_FILTER
    PLOCKER_STREAM_HANDLE_CONTEXT scannerContext;
    FLT_POSTOP_CALLBACK_STATUS returnStatus = FLT_POSTOP_FINISHED_PROCESSING;
    PFLT_FILE_NAME_INFORMATION nameInfo;
    NTSTATUS status;
    BOOL safeToOpen, scanFile;

    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);

    /* If this create was failing anyway, don't bother scanning now. */
    if (!NT_SUCCESS(Data->IoStatus.Status) ||
       (STATUS_REPARSE == Data->IoStatus.Status)) {
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    /* Check if we are interested in this file. */
    status = FltGetFileNameInformation(
		Data,
		FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
		&nameInfo
	);

    if (!NT_SUCCESS(status)) {
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    FltParseFileNameInformation(nameInfo);

    /* Check if the extension matches the list of extensions we are interested in */
    scanFile = LockerCheckExtension(&nameInfo->Extension);

    if (!scanFile) {
        /* Not an extension we are interested in */
	    FltReleaseFileNameInformation(nameInfo);
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

   (VOID) LockerpScanFileInUserMode(
	   FltObjects->Instance,
	   FltObjects->FileObject,
	   nameInfo,
	   &safeToOpen
	);

    /*  Release file name info, we're done with it */
    FltReleaseFileNameInformation(nameInfo);

    if (!safeToOpen) {
        /* Ask the filter manager to undo the create. */
        DbgPrint("!!! ClientLockerDriver.sys -- not 'safe to open', undoing create \n");

        FltCancelFileOpen(FltObjects->Instance, FltObjects->FileObject);

        Data->IoStatus.Status = STATUS_ACCESS_DENIED;
        Data->IoStatus.Information = 0;

        returnStatus = FLT_POSTOP_FINISHED_PROCESSING;
    } else if(FltObjects->FileObject->WriteAccess) {
        /*
		 * The create has requested write access, mark to rescan the file.
         * Allocate the context.
		 */
        status = FltAllocateContext(
			Globals.Filter,
			FLT_STREAMHANDLE_CONTEXT,
			sizeof(LOCKER_STREAM_HANDLE_CONTEXT),
			PagedPool,
			&scannerContext
		);

        if (NT_SUCCESS(status)) {
            /* Set the handle context. */
            scannerContext->RescanRequired = TRUE;

           (VOID) FltSetStreamHandleContext(
				FltObjects->Instance,
				FltObjects->FileObject,
				FLT_SET_CONTEXT_REPLACE_IF_EXISTS,
				scannerContext,
				NULL
			);

            /*
			 * Normally we would check the results of FltSetStreamHandleContext
             * for a variety of error cases. However, The only error status 
             * that could be returned, in this case, would tell us that
             * contexts are not supported.  Even if we got this error,
             * we just want to release the context now and that will free
             * this memory if it was not successfully set.
			 */

			/* Release our reference on the context(the set adds a reference) */
            FltReleaseContext(scannerContext);
        }
    }

    return returnStatus;
#endif
}


/*
 * LockerPreCleanup
 *
 * This is another example of a pre-operation callback:
 *
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff551109(v=vs.85).aspx
 *
 * It is called before "cleanup" happens, e.g. a response to an IRP_MJ_CLEANUP.
 * This is when all open handles to a file have been closed, at least in user
 * space (caches etc. don't count):
 *
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff548608(v=vs.85).aspx
 *
 * If our client process chooses to take a handle onto a file and won't release
 * it then, we will not get this notification until it makes the release happen.
 * So this release will come after the network told us we can OR if we were the
 * client that took the lock, this will happen and be how we notify the network
 * that we are no longer holding the file.
 *
 * Original comment in scanner was:
 *
 * "If this file was opened for write access, we want to rescan it now."
 */
FLT_PREOP_CALLBACK_STATUS
LockerPreCleanup(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext
   )
{
#ifdef IS_SCAN_FILTER
    NTSTATUS status;
    PLOCKER_STREAM_HANDLE_CONTEXT context;
    BOOL safe;

    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(CompletionContext);

    status = FltGetStreamHandleContext(
		FltObjects->Instance,
		FltObjects->FileObject,
		&context
	);

    if(NT_SUCCESS(status)) {

        if(context->RescanRequired) {
           (VOID) LockerpScanFileInUserMode(
			   FltObjects->Instance,
			   FltObjects->FileObject,
				&safe
			);
			safe = TRUE;

            if(!safe) {
                DbgPrint("!!! ClientLockerDriver.sys -- foul language detected in precleanup !!!\n");
            }
        }

        FltReleaseContext(context);
    }

#endif
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}


/*
 * LockerPreWrite
 *
 * This is another example of a pre-operation callback:
 *
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff551109(v=vs.85).aspx
 *
 * It is called before a write happens, e.g a response to IRP_MJ_WRITE.  This
 * happens when a call to WriteFile is made:
 *
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff549427(v=vs.85).aspx
 *
 * The original comment read: "We want to scan what's being written now."
 *
 * I do not think there is going to be a purpose in the client locker code for
 * trapping writes.  We just need to know when locks are released, but I will
 * leave the hook here.  Returns FLT_PREOP_SUCCESS_NO_CALLBACK.
 */
FLT_PREOP_CALLBACK_STATUS
LockerPreWrite(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext
   )
{
#ifdef IS_SCAN_FILTER
    FLT_PREOP_CALLBACK_STATUS returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
    NTSTATUS status;
    PLOCKER_NOTIFICATION notification = NULL;
    PLOCKER_STREAM_HANDLE_CONTEXT context = NULL;
    ULONG replyLength;
    BOOL safe = TRUE;
    PUCHAR buffer;

    UNREFERENCED_PARAMETER(CompletionContext);

    /* If not client port just ignore this write. */

    if (Globals.ClientPort == NULL) {
#endif IS_SCAN_FILTER
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
#ifdef IS_SCAN_FILTER
    }

    status = FltGetStreamHandleContext(
		FltObjects->Instance,
		FltObjects->FileObject,
		&context
	);

    if (!NT_SUCCESS(status)) {
        /* We are not interested in this file */
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    /* Use try-finally to cleanup */

    try {
        /* Pass the contents of the buffer to user mode. */

        if (Data->Iopb->Parameters.Write.Length != 0) {

            /*
             * Get the users buffer address.  If there is a MDL defined, use it
             * If not use the given buffer address.
             */

            if(Data->Iopb->Parameters.Write.MdlAddress != NULL) {

                buffer = MmGetSystemAddressForMdlSafe(
					Data->Iopb->Parameters.Write.MdlAddress,
					NormalPagePriority
				);

                /*
                 * If we have a MDL but could not get and address, we ran out
                 * of memory, report the correct error
                 */

                if (buffer == NULL) {

                    Data->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
                    Data->IoStatus.Information = 0;
                    returnStatus = FLT_PREOP_COMPLETE;
                    leave;
                }

            } else {

                /* Use the user's buffer */
                buffer  = Data->Iopb->Parameters.Write.WriteBuffer;
            }

            /*
             * In a production-level filter, we would actually let user mode scan the file directly.
             * Allocating & freeing huge amounts of non-paged pool like this is not very good for system perf.
             * This is just a sample!
             */

            notification = ExAllocatePoolWithTag(NonPagedPool,
                                                  sizeof(LOCKER_NOTIFICATION),
                                                  'nacS');
            if(notification == NULL) {

                Data->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
                Data->IoStatus.Information = 0;
                returnStatus = FLT_PREOP_COMPLETE;
                leave;
            }

            notification->BytesToScan = min(Data->Iopb->Parameters.Write.Length, LOCKER_READ_BUFFER_SIZE);

            /* The buffer can be a raw user buffer. Protect access to it */

            try  {
                RtlCopyMemory(&notification->Contents,
                               buffer,
                               notification->BytesToScan);

            } except(EXCEPTION_EXECUTE_HANDLER) {

                /* Error accessing buffer. Complete i/o with failure */

                Data->IoStatus.Status = GetExceptionCode() ;
                Data->IoStatus.Information = 0;
                returnStatus = FLT_PREOP_COMPLETE;
                leave;
            }

            /*
             * Send message to user mode to indicate it should scan the buffer.
             * We don't have to synchronize between the send and close of the handle
             * as FltSendMessage takes care of that.
             */

            replyLength = sizeof(LOCKER_REPLY);

            status = FltSendMessage(
				Globals.Filter,
				&Globals.ClientPort,
				notification,
				sizeof(LOCKER_NOTIFICATION),
				notification,
				&replyLength,
				NULL
			);

            if (STATUS_SUCCESS == status) {

               safe =((PLOCKER_REPLY) notification)->SafeToOpen;

           } else {

               /*
                *  Couldn't send message. This sample will let the i/o through.
                */
               DbgPrint("!!! ClientLockerDriver.sys --- couldn't send message to user-mode to scan file, status 0x%X\n", status);
           }
        }

        if (!safe) {

            /*
             * Block this write if not paging i/o(as a result of course, this scanner will not prevent memory mapped writes of contaminated
             * strings to the file, but only regular writes). The effect of getting ERROR_ACCESS_DENIED for many apps to delete the file they
             * are trying to write usually.
             * To handle memory mapped writes - we should be scanning at close time(which is when we can really establish that the file object
             * is not going to be used for any more writes)
             */ 

            DbgPrint("!!! ClientLockerDriver.sys -- foul language detected in write !!!\n");

            if (!FlagOn(Data->Iopb->IrpFlags, IRP_PAGING_IO)) {

                DbgPrint("!!! ClientLockerDriver.sys -- blocking the write !!!\n");

                Data->IoStatus.Status = STATUS_ACCESS_DENIED;
                Data->IoStatus.Information = 0;
                returnStatus = FLT_PREOP_COMPLETE;
            }
        }

    } finally {

        if(notification != NULL) {

            ExFreePoolWithTag(notification, 'nacS');
        }

        if(context) {

            FltReleaseContext(context);
        }
    }

    return returnStatus;
#endif
}


/*
 * BlockingCallUserModeRegardingFileOperation
 *
 * The original code did an errant scan for a word, because it scanned
 * each sector for the word "foul" but didn't consider cases where the
 * word started and ended on different sectors.  Do they give DDK
 * example authoring--patterned by all kinds of people who don't know
 * what they're doing--to the interns?  Who knows.
 *
 * But in this case we just call user mode with the file name and the
 * operation.  We're interested in passing up pre-creates and cleanups
 */
NTSTATUS
BlockingCallUserModeRegardingFileOperation(
    __in PFLT_INSTANCE Instance,
    __in PFILE_OBJECT FileObject,
	__in PFLT_FILE_NAME_INFORMATION nameInfo,
    __in BOOL IsCreate
   )
{
    NTSTATUS status = STATUS_SUCCESS;

	PLOCKER_NOTIFICATION notification = NULL;
	ULONG notificationLength;
	ULONG replyLength;

    //  If not client port just return.
    if (Globals.ClientPort == NULL) {
        return STATUS_SUCCESS;
    }

    try {
		/* Null terminator is not included in Length */
		DbgPrint("LOCKER_NOTIFICATION => %u, nameInfo->Name.Length => %u\n", sizeof(LOCKER_NOTIFICATION), nameInfo->Name.Length);

		/*
		 * round up to quad-words, like in the original?  Does that help?
		 * Getting buffer problems and there are some notes in the "remarks" here about padding
		 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff544378(v=vs.85).aspx
		 */
		notificationLength = sizeof(LOCKER_NOTIFICATION); 

		/*
		 * Can't use variable length after all :-/  It is probably technically possible
		 * but there is an odd usage here where the overlapped memory structure is
		 * folded in...and there's no way to really send how big things are except
		 * to have some protocol.  I'll consider it.
		 */
		DbgPrint("notificationLength is %lu\n", notificationLength); 
        notification = ExAllocatePoolWithTag(
			NonPagedPool,
            notificationLength,
            'nacS'
		);

        if (NULL == notification) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            leave;
        }
		DbgPrint("allocation succeeded in the NonPagedPool\n");

        notification->Length = nameInfo->Name.Length;

        /* Copy only as much as the buffer can hold */
        RtlCopyMemory(
			&notification->Buffer,
			/* Documentation claims pointer to WSTR (PWSTR)...but it's a PWCH!!! */
			nameInfo->Name.Buffer,
			nameInfo->Name.Length
		);

		/*
		 * Add null terminator, (not included in the Length count)
		 * Note that since length is in bytes and not WCHAR counts, this needs
		 * to be divided by two.  Gah.
		 */
		notification->Buffer[nameInfo->Name.Length / 2] = 0;

		DbgPrint("memory copied into the notification buffer, representing %ws\n", notification->Buffer);
		DbgPrint("length is %u, and value of buffer at nameInfo->Name.Length - 1 is %u\n", nameInfo->Name.Length, notification->Buffer[nameInfo->Name.Length - 1]);

        replyLength = sizeof(LOCKER_REPLY);

        status = FltSendMessage(
			Globals.Filter,
            &Globals.ClientPort,
            notification,
            notificationLength,
            notification,
            &replyLength,
            NULL
		);

		DbgPrint("synchronous call completed to user mode!\n");

		if (STATUS_SUCCESS == status) {
		BOOL SafeToOpen = ((PLOCKER_REPLY) notification)->SafeToOpen;
			/*
			 * This is leftover from the example, but I'm leaving it here so I can see how
			 * return values might be given to the driver
			 */
        } else {
            /* Couldn't send message */
            DbgPrint("!!! ClientLockerDriver.sys --- couldn't send message to user-mode to scan file, status 0x%X\n", status);
        }
    } finally {

        if(NULL != notification) {
            ExFreePoolWithTag(notification, 'nacS');
        }
    }

    return status;
}