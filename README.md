# mvatom - atomic move operation

## Usage

	git clone https://github.com/hilbix/mvatom.git
	cd mvatom
	make
	sudo make install

Then

	mvatom [options] oldname newname
	mvatom [options] -d directory name...
	mvatom [options] -ab name
	mvatom --help

## Rants

### Why not `mv`

Post-modern `mv` implementations re-invented a wheel which is no more
round.  If they cannot move, they do a `copy`/`unlink()` operation
which is unsatisfying, as when the `unlink()` fails, you stay with
the copy which was insanely done.  Or even worse on silent write
errors, you may accidentally destroy everything as well.

`mv` shall be an atomic operation.  However you cannot switch `mv`
into atomic mode.


### `mvatom` works reliably, no heuristics, no races

`mvatom` does what `mv` is supposed to do:

It renames the target and fails if it cannot do the rename (but beware
the common NFS bug in such a situation).  Also it tries hard to never
overwrite things.

Another thing is that it only guesses something for the last argument
(like `mv` does), if it is instructed to do so.  So the default always
is to work as reliably as possible.  This way you exactly know, what
will happen, if you look at the command line, without knowledge of
what currently is present in the filesystem.  This avoids race
conditions, like something springs in existence right after you looked
at it to make sure it is not present.

`mvatom` never overwrites files (except for weird cases on unsupported
platforms).  Period.  There also will be no option to do so.

There is no "unsafe" mode, it defeats the purpose.  If you want to
override a destination, rename it away into a scratch directory,
then delete the backup directory with it's contents.

Also you can "move away" files with mvatom, too.  This is done with
the "backup" feature, where an existing destination can be renamed.


### Moving data across filesytem boundaries

To move across filesystem boundaries, do it as follows:

- copy the source tree to the destination
  - never overwriting things
- `sync`
- compare the source tree against the destination
  - remove equal things on the source

The first two step can be done with `cp -an`.
Be sure to use `-n`, else, on circular mounts, you may destroy
your source by accidentally overwriting it with itself.

For the last step, there is a script called `cmpanddel.sh`.

This script is designed to never hurt any data, even in the most
weirdest circumstance, like circular mounted filesystems.
If this script cannot protect you against unintended data loss,
only doing nothing can (or even cannot).

Note that `cmpanddel.sh` uses `mvatom` to move the source away
into a newly created subdirectory.  Hence it must run from a
working directory which is on the same filesystem as the source,
else `mvatom` cannot do it's job.


# Bugs

Atomic means:

- There is no point in time where something is in an undefined
  state when the command is interrupted, for example by an
  unexpected power outage.
- If the destination already exists, it stays always available,
  so there is no point in time, where something is missing.

So here are the bugs according to this:

- `mvatom` currently moves away the destination.  So there is
  a time, where an existing destination is missing.
  I want to change this in future on supported platforms,
  but this has not yet been implemented.
  If this goal is archived, `mvatom` will reach version `1.0.0`.

- `mvatom` may leave debris behind if interrupted unconditionally.
  It makes sure, though, that nothing is destroyed, but you may
  see all sorts of artifacts, missing destination, renamed source,
  half moved things etc. or mixed roles.

However `mvatom` is clearly designed to never harm any existing data.

Also `cmpanddel.sh` has following bugs:

- If interrupted, it may leave a subdirectory named `tmpcmp.*`
  which may contain two entries:
  - `tmpcmp.*/file` which is the `file` which was compared
    with the destination before it gets deleted if equal.
  - `tmpcmp.*/location` which denotes the original location
    of the `file`.
- Currently you must cleanup by hand, like this:

      OLD=tmpcmp.*	# enter the name of the directory here
      mvatom "$OLD/file" "`cat "$OLD/location"` &&
      rm -f "$OLD/file" && rmdir "$OLD"


# License

This Works is placed under the terms of the Copyright Less License,
see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.

So `mvatom` is free as in free beer, free speech and free baby.
Copyright on babys means slavery.
