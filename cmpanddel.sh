#!/bin/bash
#
# Compare directory with other directories and delete files which match.
#
# This Works is placed under the terms of the Copyright Less License,
# see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.

set -e

STDOUT() { local e=$?; printf '%q' "$1"; printf ' %q' "${@:2}"; printf '\n'; return $e; }
STDERR() { STDOUT "$@" >&2; }
OOPS() { STDERR OOPS: "$@"; exit 23; }
INTERNAL() { OOPS internal error: "$@"; }
x() { "$@"; }
o() { x "$@" || OOPS fail $?: "$@"; }

maketmpdir()
{
if [ -n "$1" ]
then
	tmpdir="$1"
	[ ! -d "$1" ] && o mkdir "$1"
else
	tmpdir="$(mktemp -d tmpcmp.XXXXXX)"
fi

pf="$tmpdir/file"
pl="$tmpdir/location"
trap '[ -e "$pf" ] || rm -f "$pl"; rmdir "$tmpdir"' 0
}

usage()
{
[ 2 -le $# ] && return
cat <<EOF
Usage: "$(basename "$0")" directory-to-cleanup directory-to-compare..
	Temporary directory may be given in env CMPANDDEL_TMP
	You can give multiple compare-directories to check for files,
	the first one with the given target wins.
EOF
exit 42
}

warn()
{
el="$(tput el || :)$(tput cr || :)"
SRC="$1"
DSTS=("${@:2}")
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
EOF

for x in "${DSTS[@]}"
do
	echo "Compares from \"$x\""
done
echo
echo -n "Press return to continue: "
read
}

# Try to find the given file in the DSTS
: get_dst file
get_dst()
{
local a
dst=/does-not-exist
for a in "${DSTS[@]}"
do
	dst="$a/$1"
	[ -e "$dst" ] && return
done
:
}

# Protect a file by moving it into the TMPDIR
# The idea behind this is, that if you compare source with destination,
# the source vanishes, such that the destination vanishes, too.
# This effectively protects against accidents like: cmp x x && rm x
prot()
{
[ ! -e "$pf" ] || INTERNAL "$pf" exists

echo -n "$1" > "$pl"
o mvatom "$1" "$pf"
unpf="$1"
}

# Move file back from compare-directory to where it was
unp()
{
[ -n "$unpf" ] || INTERNAL variable not set
[ ! -e "$unpf" ] || INTERNAL "$unpf" exists
[ ".$unpf." = ".$(cat "$pl"; echo .)" ] || INTERNAL "$unpf" does not match "$pl"

# Do not bail out on error in case something vanished.
# In that case it might leave debris behind for manual cleanup.
# This is better than to give up early.  (Gives up in prot())
x mvatom "$pf" "$unpf"
unpf=""
}

# Print progress output
show()
{
x="$(( ${#2}<70 ? 0 : ${#2}-70 ))"
printf '%q %q%s' "$1" "${2:$x}" "$el"
}

getlink()
{
find "$1" -type d -prune -o -type l -printf '%l'
echo x	# prevent POSIX bug removing arbitrary number of \n from output
}

cmpsoftlink()
{
show link "$1"
get_dst "$1"

if [ ! -L "$dst" ]
then
	if [ -e "$dst" ]
	then
		echo "$dst is no soflink!"
	else
		echo "$dst does not exist"
	fi
	return
fi

prot "$SRC/$1"

b="$(getlink "$pf")"
c="$(getlink "$dst")"
if [ ".$b" != ".$c" ]
then
	unp
	echo "$dst: softlinks mismatch: $SRC/$1"
	return
fi

rm -f "$pf"
unpf=""
}

cmpfile()
{
show file "$1"
get_dst "$1"

if [ ! -e "$dst" ]
then
	echo "$dst does not exist"
	return
fi

prot "$SRC/$1"

if [ -L "$dst" -o ! -f "$dst" ]
then
	unp
	echo "$dst is no normal file!"
	return
fi

if ! cmp "$pf" "$dst"
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
get_dst "$1"

if [ ! -S "$dst" ]
then
	if [ -e "$dst" ]
	then
		echo "$dst is no socket!"
	else
		echo "$dst does not exist"
	fi
	return
fi

prot "$SRC/$1"

if [ ! -S "$dst" ]
then
	unp
	echo "$dst socket vanished!"
	return
fi

rm -f "$pf"
unpf=""
}

cmpfifo()
{
show fifo "$1"
get_dst "$1"

if [ ! -p "$dst" ]
then
	if [ -e "$dst" ]
	then
		echo "$dst is no fifo!"
	else
		echo "$dst does not exist"
	fi
	return
fi

prot "$SRC/$1"

if [ ! -p "$dst" ]
then
	unp
	echo "$dst fifo vanished!"
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
get_dst "$4"

if [ ! -$2 "$dst" ]
then
	if [ -e "$dst" ]
	then
		echo "$dst is no $3 special!"
	else
		echo "$dst does not exist"
	fi
	return
fi

prot "$SRC/$4"

if [ ! -$2 "$dst" ]
then
	unp
	echo "$dst $3 special vanished!"
	return
fi

b="$(getmajorminor "$pf")"
c="$(getmajorminor "$dst")"
if [ ".$b" != ".$c" ]
then
	unp
	echo "$dst: $3 special mismatch ('$c' vs. '$b'): $SRC/$4"
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
get_dst "$1"

if [ ! -e "$dst" ]
then
	echo "$dst does not exist"
	return
fi

prot "$SRC/$1"

if [ -L "$dst" -o ! -d "$dst" ]
then
	unp
	echo "$dst is no directory!"
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
warn "$@"
maketmpdir "$CMPANDDEL_TMP"

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

