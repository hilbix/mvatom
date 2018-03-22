#!/bin/bash
#
# Compare directory with a second and delete files which match.
#
# This Works is placed under the terms of the Copyright Less License,
# see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.

set -e

STDOUT() { local e=$?; printf '%q' "$1"; printf ' %q' "${@:2}"; printf '\n'; return $e; }
STDERR() { STDOUT "$@" >&2; }
OOPS() { STDERR OOPS: "$@"; exit 23; }
x() { "$@"; }
o() { x "$@" || OOPS fail $?: "$@"; }

maketmpdir()
{
if [ -n "$1" ]
then
	tmpdir="$1"
	[ ! -d "$1" ] && mkdir "$1"
else
	tmpdir="`mktemp -d tmpcmp.XXXXXX`"
fi

pf="$tmpdir/file"
pl="$tmpdir/location"
trap '[ -e "$pf" ] || rm -f "$pl"; rmdir "$tmpdir"' 0
}

usage()
{
[ 2 = $# -o 3 = $# ] ||
OOPS "Usage: `basename "$0"` directory-to-cleanup directory-to-compare [tmpdirname]"
}

warn()
{
el="`tput el || :``tput cr || :`"
SRC="$1"
DST="$2"
cat <<EOF

This script is for lazy experienced sysadmins and comes with mvatom:
http://www.scylla-charybdis.com/tool.php?tool=mvatom

This script removes files from the first directory which match files
in the second directory.  It checks softlinks, too.  Be sure that the
directory to delete is quiescent, this is, nobody else modifies files
or changes anything.  Also never run two of this scripts in parallel
on the same directory, you have been warned!

If this script fails have a look in the tempdir created, there are two
entries, "file" and "location".  Location tells you where file came
from.  Move it back manually.  (No warranty.  Use at own risk.  etc.)

Deletes  from "$SRC"
Compares from "$DST"

EOF
echo -n "Press return to continue: "
read
}

prot()
{
[ ! -e "$pf" ] || OOPS "internal error: $pf exists"

echo "$1" > "$pl"
o mvatom "$1" "$pf"
unpf="$1"
}

unp()
{
[ -n "$unpf" ] || OOPS "internal error: variable not set"
[ ! -e "$unpf" ] || OOPS "internal error: $unpf exists"
[ ".$unpf" = ".`cat "$pl"`" ] || OOPS "internal error: $unpf does not match $pl"

mvatom "$pf" "$unpf"
unpf=""
}

show()
{
x="$(( ${#2}<70 ? 0 : ${#2}-70 ))"
printf '%q %q%s' "$1" "${2:$x}" "$el"
}

getlink()
{
find "$1" -type d -prune -o -type l -printf '%l'
}

cmpsoftlink()
{
show link "$1"

if [ ! -L "$DST/$1" ]
then
	if [ -e "$DST/$1" ]
	then
		echo "$DST/$1 is no soflink!"
	else
		echo "$DST/$1 does not exist"
	fi
	return
fi

prot "$SRC/$1"

b="$(getlink "$pf")x"
c="$(getlink "$DST/$1")x"
if [ ".$b" != ".$c" ]
then
	unp
	echo "$DST/$1: softlinks mismatch: $SRC/$1"
	return
fi

rm -f "$pf"
unpf=""
}

cmpfile()
{
show file "$1"

if [ ! -e "$DST/$1" ]
then
	echo "$DST/$1 does not exist"
	return
fi

prot "$SRC/$1"

if [ -L "$DST/$1" -o ! -f "$DST/$1" ]
then
	unp
	echo "$DST/$1 is no normal file!"
	return
fi

if ! cmp "$pf" "$DST/$1"
then
	unp
	echo "$SRC/$1 mismatch."
	return
fi

rm -f "$pf"
unpf=""
}

cmpsock()
{
show sock "$1"

if [ ! -S "$DST/$1" ]
then
	if [ -e "$DST/$1" ]
	then
		echo "$DST/$1 is no socket!"
	else
		echo "$DST/$1 does not exist"
	fi
	return
fi

prot "$SRC/$1"

if [ ! -S "$DST/$1" ]
then
	unp
	echo "$DST/$1 socket vanished!"
	return
fi

rm -f "$pf"
unpf=""
}

cmpfifo()
{
show fifo "$1"

if [ ! -p "$DST/$1" ]
then
	if [ -e "$DST/$1" ]
	then
		echo "$DST/$1 is no fifo!"
	else
		echo "$DST/$1 does not exist"
	fi
	return
fi

prot "$SRC/$1"

if [ ! -p "$DST/$1" ]
then
	unp
	echo "$DST/$1 fifo vanished!"
	return
fi

rm -f "$pf"
unpf=""
}

getmajorminor()
{
stat -c %t,%T "$1"
}

cmpspecial()
{
show "$1" "$4"

if [ ! -$2 "$DST/$4" ]
then
	if [ -e "$DST/$4" ]
	then
		echo "$DST/$4 is no $3 special!"
	else
		echo "$DST/$4 does not exist"
	fi
	return
fi

prot "$SRC/$4"

if [ ! -$2 "$DST/$4" ]
then
	unp
	echo "$DST/$4 $3 special vanished!"
	return
fi

b="$(getmajorminor "$pf")"
c="$(getmajorminor "$DST/$4")"
if [ ".$b" != ".$c" ]
then
	unp
	echo "$DST/$4: $3 special mismatch ('$c' vs. '$b'): $SRC/$4"
	return
fi

rm -f "$pf"
unpf=""
}

cmpblock()
{
cmpspecial "blk " b block "$1"
}

cmpchar()
{
cmpspecial char c char "$1"
}

cmpdir()
{
show "dir " "$1"

if [ ! -e "$DST/$1" ]
then
	echo "$DST/$1 does not exist"
	return
fi

prot "$SRC/$1"

if [ -L "$DST/$1" -o ! -d "$DST/$1" ]
then
	unp
	echo "$DST/$1 is no directory!"
	return
fi

if ! rmdir "$pf"
then
	unp
	echo "$pf not empty"
	return
fi
unpf=""
}

usage "$@"
warn "$1" "$2"
maketmpdir "$3"

find "$SRC" -type f -printf '%P\0' -o -type d -name "$tmpdir" -prune |
while IFS='' read -rd '' a
do
	cmpfile "$a"
done

find "$SRC" -type l -printf '%P\0' -o -type d -name "$tmpdir" -prune |
while IFS='' read -rd '' a
do
	cmpsoftlink "$a"
done

find "$SRC" -type s -printf '%P\0' -o -type d -name "$tmpdir" -prune |
while IFS='' read -rd '' a
do
	cmpsock "$a"
done

find "$SRC" -type p -printf '%P\0' -o -type d -name "$tmpdir" -prune |
while IFS='' read -rd '' a
do
	cmpfifo "$a"
done

find "$SRC" -type b -printf '%P\0' -o -type d -name "$tmpdir" -prune |
while IFS='' read -rd '' a
do
	cmpblock "$a"
done

find "$SRC" -type c -printf '%P\0' -o -type d -name "$tmpdir" -prune |
while IFS='' read -rd '' a
do
	cmpchar "$a"
done

find "$SRC" -depth -type d -name "$tmpdir" -prune -o -type d -printf '%P\0' |
while IFS='' read -rd '' a
do
	[ -z "$a" ] || cmpdir "$a"
done

