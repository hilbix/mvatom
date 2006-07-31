/* $Header$
 *
 * Atomic file move utility.
 *
 * Copyright (C)2006 by Valentin Hilbig
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 * $Log$
 * Revision 1.3  2006-07-31 23:01:45  tino
 * Option -p added (mkdir parents for dest)
 *
 * Revision 1.2  2006/07/23 00:06:19  tino
 * Bugs removed, working now
 *
 * Revision 1.1  2006/07/22 23:47:58  tino
 * First version for testing
 */

#include "tino/filetool.h"
#include "tino/ex.h"
#include "tino/getopt.h"

#include "mvatom_version.h"

static int		errflag;
static int		m_backup, m_ignore, m_nulls, m_relative, m_quiet, m_verbose, m_mkdirs;
static const char	*m_dest;

/**********************************************************************/

static void
verror_fn(const char *prefix, const char *s, va_list list, int err)
{
  if (!m_quiet)
    tino_verror_std(prefix, s, list, err);
  if (!m_ignore)
    exit(1);
  errflag	= 1;
}

static void
verbose(const char *s, ...)
{
  va_list	list;

  if (!m_verbose)
    return;

  va_start(list, s);
  vprintf(s, list);
  va_end(list);
  printf("\n");
}

/**********************************************************************/

static const char *
read_dest(void)
{
  tino_fatal("read from stdin not yet implemented");
  return 0;
}

static void
do_rename(const char *name, const char *to)
{
  if (rename(name, to))
    {
      tino_err("cannot rename %s -> %s", name, to);
      return;
    }
  verbose("rename: %s -> %s", name, to);	
}

static void
do_rename_backup(const char *old, const char *new)
{
  if (tino_file_notexists(old))
    {
      tino_err("missing old name for rename: %s", old);
      return;
    }
  if (!tino_file_notexists(new))
    {
      char	*tmp;

      if (!m_backup)
	{
	  tino_err("existing destination for rename: %s", new);
	  return;
	}
      tmp	= tino_file_backupname(NULL, 0, new);
      do_rename(new, tmp);
      free(tmp);
    }
  else if (m_mkdirs)
    tino_file_mkdirs_forfile(NULL, new);
  do_rename(old, new);
}

/**********************************************************************/

static void
do_mvaway(const char *name)
{
  char	*buf;

  if (tino_file_notexists(name))	
    {
      tino_err("missing file to move away: %s", name);
      return;
    }
  buf	= tino_file_backupname(NULL, 0, name);
  do_rename(name, buf);
}

static void
mvaway(const char *name)
{
  if (strcmp(name, "-"))
    do_mvaway(name);
  else
    while ((name=read_dest())!=0)
      do_mvaway(name);
}

/**********************************************************************/

/* leaks memory	*/
static const char *
do_relative(const char *old, const char *dest)
{
  if (!m_relative)
    return dest;
  return tino_file_glue_path(NULL, 0, tino_file_dirname(NULL, 0, old), tino_file_skip_root_const(dest));
}

/* leaks memory	*/
static void
mvrename(const char *old, const char *new)
{
  do_rename_backup(old, do_relative(old, new));
}

/**********************************************************************/

static void
do_mvdest(const char *name)
{
  const char	*targ;
  char		*dest;
 
  if (m_relative)
    {
      targ	= tino_file_skip_root_const(name);
      tino_file_mkdirs_forfile(m_dest, targ);
    }
  else
    targ	= tino_file_filenameptr(name);
  dest	= tino_file_glue_path(NULL, 0, m_dest, targ);
  do_rename_backup(name, dest);
  free(dest);
}

static void
mvdest(const char *name)
{
  if (tino_file_notdir(m_dest))
    {
      tino_err((tino_file_notexists(m_dest) ? "missing destination directory: %s" : "existing destination not a directory: %s"), m_dest);
      return;
    }
  if (strcmp(name, "-"))
    do_mvdest(name);
  else
    while ((name=read_dest())!=0)
      do_mvdest(name);
}

/**********************************************************************/

int
main(int argc, char **argv)
{
  int		argn;

  tino_verror_fn	= verror_fn;

  argn	= tino_getopt(argc, argv, 1, 0,
		      TINO_GETOPT_VERSION(MVATOM_VERSION)
		      " name...\n"
		      "	rename:		%s -r oldname newname\n"
		      "	move:		%s -d dir name...\n"
		      "	rename away:	%s -b name",

		      TINO_GETOPT_USAGE
		      "h	this help"
		      ,

		      TINO_GETOPT_FLAG
		      "0	read 0 terminated lines from stdin\n"
		      "		example: find . -print 0 | mvatom -0b -"
		      , &m_nulls,

		      TINO_GETOPT_FLAG
		      "i	ignore (common) errors"
		      , &m_ignore,

		      TINO_GETOPT_FLAG
		      "b	backup existing destination to .~#~"
		      , &m_backup,

		      TINO_GETOPT_STRING
		      "d dir	target directory"
		      , &m_dest,

		      TINO_GETOPT_FLAG
		      "p	create missing parent directories (for dest)"
		      , &m_mkdirs,

		      TINO_GETOPT_FLAG
		      "q	silently fail"
		      , &m_quiet,

		      TINO_GETOPT_FLAG
		      "r	append relative source path to dest (needs no -p)\n"
		      "		To rename within a directory without moving use:\n"
		      "		mvatom -r /path/to/file/a b"
		      , &m_relative,

		      TINO_GETOPT_FLAG
		      "v	verbose"
		      , &m_verbose,

		      NULL);

  if (argn<=0)
    return 1;

  if (m_dest)
    {
      while (argn<argc)
	mvdest(argv[argn++]);
    }
  else if (m_backup && argc==argn+1)
    mvaway(argv[argn]);
  else if (argc!=argn+2)
    tino_err("rename needs 2 arguments");
  else
    mvrename(argv[argn], argv[argn+1]);
  return errflag;
}
