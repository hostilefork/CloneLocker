CloneLocker
===========

*(Disclaimer: THIS DOES NOT WORK (as of 9-Jun-2013).  It is a splattering of exploratory testing that was pushed to GitHub after a proof-of-concept study went haywire.  The present state evokes a word a friend told me: "copypasta".  That's where snippets of sample code are stuck together without general coherence.  I share it in this catastrophic state merely so that it may be looked at, and options explored for how a development direction might be taken.  That is IF there is interest in that advancement rather than abandonment.)*

![CloneLocker](http://metaeducation.com/media/shared/respectech/clonelocker.png)

*(Note: Lock icon borrowed/stolen from [Alexandre Moore](http://sa-ki.deviantart.com/).  It came up as being free for commercial or personal use in an image search for "lock icon", and actually had multiple sizes.  I thought I might need a tray icon size, so I picked it from the available free options in that search engine.)*

CloneLocker is an attempt to attack this scenario:

1. there is a directory of files, being shared by users over some network

2. it is a bad thing if these users step on each others work

3. the files are unwieldy and large (by the standards of your day)

4. your LAN is slow for the file sizes, and/or people are working remotely

Were it not for 3 and 4, you'd just tell someone with this problem *"uh...get a faster network and centralize the files on a share"*.  But not everyone can do that, so there does exist an ecosystem of userspace network synchronization products.  These cost money, and I don't really know of any open source ones; it's a niche market.

But let us say you aren't going to defer to an existing product, and want to roll-your-own solution.  You'd need a monitor process running on each machine that would impose artificial read/write exclusions on the local filesystem, but based on what other users on the network are doing.  So let's say user A takes a read lock on a file on his disk -- then the monitor process would communicate that fact to user B's machine.  Although user B is not intentionally running any program accessing the clone of the file on his disk, his monitor process would grab the appropriate lock on it anyway...as a proxy for user A, for the duration of A's interest in the file.

Okay, but how do you write such a thing for Windows (if you're not Microsoft?)  Well, you have to hook the OS to be notified of file accesses: when files are being opened, closed, read, written, etc.  The existence of third-party antivirus screeners on Windows means these hooks must exist; so it *has* to be technically possible.

It is, and strangely enough the seemingly elusive hooking doesn't turn out to be especially hard:

[miniFilter drivers](http://msdn.microsoft.com/en-us/library/windows/hardware/ff541591)

Really all you need is a kernel mode component that passes up the filenames being looked at prior to allowing access. A timer and polling period may be necessary in the user mode process, in case the client who held the lock crashed or had the power plug pulled.  There are some nuisances about installing with elevated privileges, and having cryptographically signed certificates you pay MS for or tell your users to put their machines into "test mode".  I got through all that bit and got cocky.

Turns out that modifying Windows DDK samples is a hornet's nest.  I had encouraging early success--by following the schematic of using the NT DDK build environment to build both a user mode program and a kernel mode program.  But that was torn down by realizing that you really can't develop in userspace with the DDK if you're writing something of any sophistication:

[Including C++ headers in user mode programs built with NT DDK](http://stackoverflow.com/questions/16975728/including-c-headers-in-user-mode-programs-built-with-nt-ddk)

At first I wanted to keep the user mode code "lean and mean", just a small client/server C program that kept all the lock state stored in a Common Controls ListView.  But then it fell victim to concerns such as how none of the decent raw C JSON libraries for network communication support WCHAR... they're full of deprecated `sprintf` or other such things.  The filenames aren't natural paths like "C:\", it's "\Devices\Harddisk1\blah\blah".  Without C++ and decent libraries, matching the user mode code and the driver code is nigh impossible.

In summary I'll say:

1. *"Kids, don't try this at home."*

2. Do not try and use the DDK to build userspace apps.

3. Buy faster network equipment.

4. [Join the revolution against software complexity](https://github.com/hostilefork/r3-hf/wiki/StackOverflow-Chat-FAQ).  This style of coding has to stop.
