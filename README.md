# move files atomically

Move files by atomic rename, so never move across filesystems.

It also tries to never overwrite or harm existing data.

> This here does not call `rename()` to atomically replace the destination
> yet.  This might be added in future.


## Usage

	git clone https://github.com/hilbix/mvatom.git
	cd mvatom
	make
	sudo make install


## Rationale

> This is an old text which probably needs a rewrite.

Post-modern mv implementations re-invented a wheel which is no more
round.  If they cannot move they do a copy/unlink() operation which is
unsatisfying, as when the unlink() fails you stay with the copy which
was insanely done.

mv shall be an atomic operation.  However you cannot switch mv into
atomic mode.  This version here does what mv is supposed to do:

It renames the target and fails if it cannot do the rename (but beware
the common NFS bug in such a situation).  In unsafe mode it does this
using the rename() operation (note that rename() has a possible race
condition which may overwrite a destination unconditionally, if it is
created after mvatom has checked the presence of the destination), in
safe mode (will be introduced in 0.5.0) it will use hardlink/unlink.
The move is "atomically" in respect to the destination either is there
completely or missing, it is not "atomically" in the sense of "man 2
rename".  If you need this, you need option -uf.

Another thing is that it only guesses something for the last argument
like mv does if it is instructed to do so.  So it always works
reliably.  You know what it does from looking at the command line, and
if no heuristics are given this does not depend from the last arg
beeing a directory or not.

If the destination is a directory this must be given as an option to
mvatom.  Also all options are simple and clear and there is no way to
overwrite files, as mvatom never overwrites files (but beware the race
condition in unsafe mode which was used before 0.5.0).

Also you can "move away" files with mvatom, too.  This is done with
the "backup" feature, where an existing destination can be renamed.

There now is a helper script: `cmpanddel.sh` deletes
everything from a first directory which has an indentical match in a
second directory.  It shall be safe even against the weirdest
situations where you simply cannot detect that the first directory in
fact is the second one, thus accidentally deleting a whole directory
tree.  The protection is done by renaming the source first with
mvatom.  It is not race condition free, so it is thought for all those
lazy experienced sysadmins out there who exactly know what they are
doing.

The helper [`dirlist`](https://github.com/hilbix/dirlist/) now is in a separate package.
Example use:

	dirlist -0p DIRa | mvatom -0dDIRb -


## FAQ

License?

- The Licence currently is GPLv2.
- I want to change that into [CLL](COPYRIGHT.CLL),
  but did not yet came around to do the neccessary review.

