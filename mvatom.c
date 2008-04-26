/* $Header$
 *
 * Atomic file move utility.
 *
 * Copyright (C)2006-2007 Valentin Hilbig <webmaster@scylla-charybdis.com>
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
 * Revision 1.8  2008-04-26 14:08:31  tino
 * Cosmetic change
 *
 * Revision 1.7  2007-12-13 08:18:31  tino
 * For dist
 *
 * Revision 1.6  2006-10-21 01:59:00  tino
 * Ubuntu fixes (new va_* functions)
 *
 * Revision 1.5  2006/09/27 20:36:40  tino
 * see ANNOUNCE and ChangeLog
 *
 * Revision 1.4  2006/08/12 02:03:35  tino
 * option -o and some minor changes
 *
 * Revision 1.3  2006/07/31 23:01:45  tino
 * Option -p added (mkdir parents for dest)
 *
 * Revision 1.2  2006/07/23 00:06:19  tino
 * Bugs removed, working now
 *
 * Revision 1.1  2006/07/22 23:47:58  tino
 * First version for testing
 */

#define TINO_NEED_OLD_ERR_FN
#include "tino/filetool.h"
#include "tino/ex.h"
#include "tino/getopt.h"
#include "tino/buf_line.h"

#include "mvatom_version.h"

static int		errflag;
static int		m_backup, m_ignore, m_nulls, m_lines, m_relative, m_quiet, m_verbose, m_mkdirs;
static const char	*m_dest;

/**********************************************************************/

static void
verror_fn(const char *prefix, TINO_VA_LIST list, int err)
{
  if (!m_quiet)
    tino_verror_std(prefix, list, err);
  if (!m_ignore)
    exit(1);
  errflag	= 1;
}

static void
verbose(const char *s, ...)
{
  tino_va_list	list;

  if (!m_verbose)
    return;

  tino_va_start(list, s);
  vprintf(s, tino_va_get(list));
  tino_va_end(list);
  printf("\n");
}

/**********************************************************************/

static void
do_mkdirs(const char *path, const char *file)
{
  switch (tino_file_mkdirs_forfile(path, file))
    {
    case -1:
      tino_err("failed: mkdir for %s%s%s", path ? path : "", path ? "/" : "", file);
      break;
      
    case 1:
      verbose("mkdir for %s%s%s", path ? path : "", path ? "/" : "", file);
      break;
    }
}

static const char *
read_dest(void)
{
  static TINO_BUF buf;

  if (!m_nulls && !m_lines)
    {
      tino_err("missing option -l or -0 to read stdin");
      return 0;
    }
  return tino_buf_line_read(&buf, 0, (m_nulls ? 0 : '\n'));
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
  if (tino_file_notexistsE(old))
    {
      tino_err("missing old name for rename: %s", old);
      return;
    }
  errno	= 0;	/* Do not report errors in case there is no error	*/
  if (!tino_file_notexistsE(new))
    {
      char	*tmp;

      if (!m_backup)
	{
	  /* Meaning corrected
	   */
	  tino_err("existing destination: %s", new);
	  return;
	}
      tmp	= tino_file_backupname(NULL, 0, new);
      do_rename(new, tmp);
      free(tmp);
    }
  else if (m_mkdirs)
    do_mkdirs(NULL, new);
  do_rename(old, new);
}

/**********************************************************************/

static void
do_mvaway(const char *name)
{
  char	*buf;

  if (tino_file_notexistsE(name))	
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
      do_mkdirs(m_dest, targ);
    }
  else
    targ	= tino_file_filenameptr_const(name);
  dest	= tino_file_glue_path(NULL, 0, m_dest, targ);
  do_rename_backup(name, dest);
  free(dest);
}

static void
mvdest(const char *name)
{
  if (m_mkdirs<2 && tino_file_notdirE(m_dest))
    {
      tino_err((tino_file_notexistsE(m_dest) ? "missing destination directory: %s" : "existing destination not a directory: %s"), m_dest);
      return;
    }
  if (strcmp(name, "-"))
    do_mvdest(name);
  else
    while ((name=read_dest())!=0)
      do_mvdest(name);
}

/**********************************************************************/

static int
is_directory_target(const char *name)
{
  const char	*tmp;

  tmp	= tino_file_filenameptr_const(name);
  if (*tmp && strcmp(tmp, ".") && strcmp(tmp, ".."))
    return 0;
  return !tino_file_notdirE(name);
}

int
main(int argc, char **argv)
{
  int	argn, m_original;

  tino_verror_fn	= verror_fn;

  argn	= tino_getopt(argc, argv, 1, 0,
		      TINO_GETOPT_VERSION(MVATOM_VERSION)
		      " name...\n"
		      "	if name is - lines are read from stdin.\n"
		      "	rename:		%s -r oldname newname\n"
		      "	move:		%s -d dir name...\n"
		      "	rename away:	%s -b name\n"
		      "	convenience:	alias mv='mvatom -o'",

		      TINO_GETOPT_USAGE
		      "h	this help"
		      ,

		      TINO_GETOPT_FLAG
		      "0	(This option is 'number zero', not a big O!)\n"
		      "		read 0 terminated lines from stdin\n"
		      "		example: find . -print0 | mvatom -0b -"
		      , &m_nulls,

		      TINO_GETOPT_FLAG
		      "b	backup existing destination to .~#~"
		      , &m_backup,

		      TINO_GETOPT_STRING
		      "d dir	target directory"
		      , &m_dest,

		      TINO_GETOPT_FLAG
		      "i	ignore (common) errors"
		      , &m_ignore,

		      TINO_GETOPT_FLAG
		      "l	read lines from stdin, enables - as argument\n"
		      "		example: find . -print | mvatom -lb -"
		      , &m_lines,

		      TINO_GETOPT_FLAG
		      "o	original behavior if directory is the last target.\n"
		      "		The last argument must end in a / or must be . or .."
		      , &m_original,

		      TINO_GETOPT_FLAG
		      TINO_GETOPT_MAX
		      "p	create missing parent directories (for -r)\n"
		      "		Give twice to create for -d, too"
		      , &m_mkdirs,
		      2,

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

  if (m_original && !m_dest && argn+1<argc && is_directory_target(argv[argc-1]))
    m_dest	= argv[--argc];
  if (m_dest)
    {
      while (argn<argc)
	mvdest(argv[argn++]);
    }
  else if (m_backup && argc==argn+1)
    mvaway(argv[argn]);
  else if (argc==argn+2)
    mvrename(argv[argn], argv[argn+1]);
  else
    tino_err("rename needs 2 arguments");
  return errflag;
}
