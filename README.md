CloneLocker
===========

![CloneLocker](http://metaeducation.com/media/shared/respectech/clonelocker.png)

*(Note: Lock icon is temporary and borrowed from the InterWeb!  Attribution is to [Alexandre Moore](http://sa-ki.deviantart.com/) )*

App with kernel mode and user mode that attempts to mimic behavior of network share locking on files that are disconnected clones of each other, but would like the filesystem-level locking behavior as if they were on a share.  At time of writing (9-Jun-2013) it does not yet work.  Published so that it may perhaps get the help it needs to actually work.

The idea did not seem complicated to begin with.  Basically, use a driver-level hook to notice when file lock accesses (read/write) were requested.  Anti-virus programs exist...Q.E.D. this is technically possible.  This should be easier than AV.  Instead of an anti-virus user-level processs, you just have a driver that passes up the filenames being looked at... and a user-level process that hits a network database to see if someone else has a claim on that file.  If the lock is not taken, allow the access and record the lock.  If an existing lock is in the DB then the user-mode monitor process takes a lock on behalf of the network peer until further notice.

Turns out that adapting Windows DDK samples is a bit tricker than you might think!
