/* $Header$
 *
 * Primitive directory lister
 *
 * Copyright (C)2008 Valentin Hilbig <webmaster@scylla-charybdis.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 *
 * $Log$
 * Revision 1.2  2008-05-21 17:56:53  tino
 * Options
 *
 * Revision 1.1  2008-05-07 15:12:17  tino
 * dirlist added
 *
 */

#include "tino/main_getini.h"
#include "tino/err.h"

#include <dirent.h>

#include "mvatom_version.h"

static int f_nulls, f_buffered, f_parent, f_nodot, f_source;

static void
dirlist(const char *dir, void *user)
{
  DIR           *dp;
  struct dirent *d;

  if (f_buffered)
    setvbuf(stdout, NULL, _IOFBF, BUFSIZ);
  if (!dir)
    dir=".";
  if ((dp=opendir(dir))==0)
    {
      tino_err(TINO_ERR(ETTDI100B,%s), dir);
      return;
    }
  while ((d=readdir(dp))!=0)
    {
      if (d->d_name[0]=='.')
        {
	  if (d->d_name[1]==0 || (d->d_name[1]=='.' && d->d_name[2]==0))
	    {
	      if (!f_parent)
		continue;
	    }
	  else if (f_nodot)
	    continue;
	}
      if (f_source)
	{
	  fputs(dir, stdout);
	  putchar('/');
	}
      fputs(d->d_name, stdout);
      putchar(f_nulls ? 0 : '\n');
      if (!f_buffered)
        fflush(stdout);
    }
  if (closedir(dp))
    {
      tino_err(TINO_ERR(ETTDI101B,%s), dir);
      return;
    }
}

int
main(int argc, char **argv)
{
  return tino_main_if(dirlist,
		      NULL,	/* user	*/
		      NULL,
		      argc, argv,
		      0, 1,
		      TINO_GETOPT_VERSION(MVATOM_VERSION)
		      " [directory...]\n"
		      "\tA primitive directory lister, lists all names in a dir,\n"
		      "\tskips . and .. by default."
		      ,

		      TINO_GETOPT_FLAG
		      "0	write NUL terminated lines (instead of LF)"
		      , &f_nulls,

		      TINO_GETOPT_FLAG
		      "f	full buffered IO (no flushs after each line)"
		      , &f_buffered,

		      TINO_GETOPT_FLAG
		      "n	no dot-files, hides files starting with a ."
		      , &f_nodot,

		      TINO_GETOPT_FLAG
		      "p	list . and .."
		      , &f_parent,

		      TINO_GETOPT_FLAG
		      "s	add source path to output"
		      , &f_source,

		      NULL);
}
