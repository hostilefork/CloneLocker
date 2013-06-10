/*
 * CloneLockerShared.h
 * Copyright (C) 2013 HostileFork.com
 *
 * These definitions are used by both the kernel-mode component
 * of CloneLocker and the user-mode component.  It defines the
 * structures common to both.  It is therefore intended to be
 * built with a C compiler as the kernel mode component is
 * written in C and not C++.  Note that there are some very specific
 * rules differentiating the includes for kernel mode components vs.
 * user mode components, hence there cannot be things in this
 * file like #include <Windows.h> ... different includes must be
 * made by kernel mode vs the user mode.
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

#ifndef __CLONELOCKERSHARED_H__
#define __CLONELOCKERSHARED_H__

/*
 * Name of port used to communicate, we use a #define because it's in 
 * a header file and we can get linker problems if we did otherwise.
 */
#define LockerPortName L"\\CloneLockerPort"

/*
 * Took this command example from MiniSpy DDK example when trying to
 * implement a clean exit from the filter thread that didn't have to resort
 * to polling.  Realized that an IOCTL coming from user mode to send a
 * command like this had no way to call back out and break out of a
 * GetQueuedCompletionStatus.  Leaving the code in (deactivated)
 * because getting the version is going to be needed someday, at least.
 *
 * Refer back to miniSpy when reactivating this code.
 */
typedef enum _LOCKER_COMMAND {
	GetLockerVersion
} LOCKER_COMMAND;

#pragma warning(push)
/* disable warnings for structures with zero length arrays. */
#pragma warning(disable:4200)
typedef struct _COMMAND_MESSAGE {
    LOCKER_COMMAND Command;
	/* There was some issue about alignment on IA64, hence this.  Isn't Itanium dead? */
    ULONG Reserved;
    UCHAR Data[];
} COMMAND_MESSAGE, *PCOMMAND_MESSAGE;
#pragma warning(pop)


/*
 * Layout for the common structure that encodes a notification about a 
 * file access attempt from kernel mode to be read by user mode.
 */
#pragma warning(push)
/* disable warnings for structures with zero length arrays. */
#pragma warning(disable:4200)
typedef struct _LOCKER_NOTIFICATION {

	/*
	 * Was supposed to be simply a boolean indicating whether the request
	 * bubbling up out of the driver is a CreateFile call or a cleanup
	 * when a file is no longer used.  Keeping everything close to the
	 * DDK scanner.c example in the beginning and not messing with
	 * alignment problems, hence these things are kept as ULONG until
	 * I understand more about what's allowed or disallowed.
	 */
	ULONG IsCreate;
	
	/*
	 * Length in BYTES (not characters!) of the string.  This does not count
	 * the null terminator.  Note that this is kind of following the same
	 * pattern as UNICODE_STRING, but there is no need for MaxLength as
	 * this is read-only.
	 */
	 ULONG Length;

	/*
	 * Using a fixed-sized buffer for a filename is a terrible idea.  However,
	 * the example I was following embeds this structure inside of another
	 * with a variable length data array at the end, and as in Highlander
	 * "there can be only one (zero length buffer at the end of your structure
	 * definition as the hack you use to do a single allocation).  The 512
	 * character limit on paths is just something I threw in here for the
	 * moment but this must be fixed.
	 *
	 *    REVIEW THIS!  Screwing this kind of thing up in kernel mode
	 *    is a good way to get blue screens of death or other nastiness.
	 *
	 * Regarding the technical limits of file path lengths in Windows, refer to:
	 *     http://stackoverflow.com/a/295033/211160
	 */
    WCHAR Buffer[512];

} LOCKER_NOTIFICATION, *PLOCKER_NOTIFICATION;
#pragma warning(pop)


/*
 * Layout for the common structure that encodes a reply about a 
 * file access attempt from kernel mode to be read by user mode.
 *
 * In the scanner example I was looking at, the notificiation came
 * AFTER the file had been approved for opening.  That's because it
 * was in the spirit of a virus scanner or similar and was looking for
 * words in the file and refusing to open it if those words were in the
 * contents.  ClientLocker isn't (currently) based on locking any file
 * *contents* at all, it only looks at file *names*.  And if the file
 * is already taken by the network it opens the file itself (and it has
 * a special exclusion to do so based on process ID).  So there
 * isn't any information to reply to the driver with.  But if there
 * were it would be here.
 *
 * To sum all that up: "SafeToOpen" was a boolean from the scanner
 * example, but it's currently ignored.
 */
typedef struct _LOCKER_REPLY {
    BOOL SafeToOpen;
} LOCKER_REPLY, *PLOCKER_REPLY;

#endif /* __CLONELOCKERSHARED_H__ */
