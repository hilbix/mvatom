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
 * Revision 1.1  2008-05-07 15:12:17  tino
 * dirlist added
 *
 */

#include "tino/main_getini.h"
#include "tino/err.h"

#include <dirent.h>

#include "mvatom_version.h"

static void
dirlist(const char *dir, void *user)
{
  DIR           *dp;
  struct dirent *d;

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
	    continue;
	}
      printf("%s\n", d->d_name);
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
		      "\tjust a primitive directory lister\n"
		      "\tIt does lists the names in the directory regardless of type\n"
		      "\tand skips only . and .., that's all."
		      ,
		      NULL);
}
