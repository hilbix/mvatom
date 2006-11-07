#!/bin/bash
# $Header$
#
# Compare directory with a second and delete files which match.
#
# Copyright (C)2006 by Valentin Hilbig
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA
#
# $Log$
# Revision 1.1  2006-11-07 12:41:20  tino
# cmpanddel.sh added
#

set -e

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
if [ 2 != $# -a 3 != $# ]
then
	echo "Usage: `basename "$0"` directory-to-cleanup directory-to-compare [tmpdirname]"
	exit
fi
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
if [ -e "$pf" ]
then
	echo "internal error: $pf exists"
	exit
fi
echo "$1" > "$pl"
mvatom "$1" "$pf"
unpf="$1"
}

unp()
{
if [ -z "$unpf" ]
then
	echo "internal error: variable not set"
	exit
fi

if [ -e "$unpf" ]
then
	echo "internal error: $unpf exists"
	exit
fi

if [ ".$unpf" != ".`cat "$pl"`" ]
then
	echo "internal error: $unpf does not match $pl"
	exit
fi

mvatom "$pf" "$unpf"
unpf=""
}

show()
{
x="$(( ${#2}<70 ? 0 : ${#2}-70 ))"
echo -n "$1 ${2:$x}$el"
}

getlink()
{
find "$1" -type d -prune -o -type l -printf "%l\n"
}

cmpsoftlink()
{
show "link" "$1"

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

b="`getlink "$pf"`"
c="`getlink "$DST/$1"`"
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
show "file" "$1"

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
	echo "$SRC/$1 mismatch."
	unp
	return
fi

rm -f "$pf"
unpf=""
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
	echo "$DST/$1 not empty"
	return
fi
unpf=""
}

usage "$@"
warn "$1" "$2"
maketmpdir "$3"

find "$SRC" -type f -printf "%P\n" -o -type d -name "$tmpdir" -prune |
while read -r a
do
	cmpfile "$a"
done

find "$SRC" -type l -printf "%P\n" -o -type d -name "$tmpdir" -prune |
while read -r a
do
	cmpsoftlink "$a"
done

find "$SRC" -type d -name "$tmpdir" -prune -o -depth -type d -printf "%P\n" |
while read -r a
do
	[ -z "$a" ] || cmpdir "$a"
done
