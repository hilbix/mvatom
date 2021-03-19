/*
 * Atomic file move utility.
 *
 * Copyright (C)2006-2011 Valentin Hilbig <webmaster@scylla-charybdis.com>
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
 */

#define TINO_NEED_OLD_ERR_FN
#include "tino/filetool.h"
#include "tino/ex.h"
#include "tino/getopt.h"
#include "tino/buf_line.h"

#include "mvatom_version.h"

#define	WHITEOUT_BY_DEFAULT	0	/* or rather set to 1?	*/

#define	JOIN_(X,Y)	X##Y
#define	JOIN(X,Y)	JOIN_(X,Y)
#define	WHITEOUT_BY_DEFAULT_GET	JOIN(WHITEOUT_BY_DEFAULT_, WHITEOUT_BY_DEFAULT)
#define	WHITEOUT_BY_DEFAULT_0	m_whiteout > 0
#define	WHITEOUT_BY_DEFAULT_1	m_whiteout >= 0

static int		errflag;
static int		m_backup, m_ignore, m_nulls, m_lines, m_relative, m_quiet, m_verbose, m_mkdirs, m_append;
static int		m_enforce, m_whiteout;
static const char	*m_dest, *m_source, *m_backupdir;
#if 0
static int		m_unsafe, m_force;
static const char	*m_dest, *m_source, *m_backupdir, *m_tmpdir;
#endif


/**********************************************************************/

static void
verror_fn(const char *prefix, TINO_VA_LIST list, int err)
{
  if (!m_quiet)
    tino_verror_ext(list, err, "mvatom %s", prefix);
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

/* This actually is a hack.
 *
 * We only have one single operation active at a time.  So we can use
 * a static buffer here which keeps the intermediate string.
 */
static const char *
get_src(const char *name)
{
  static TINO_BUF	buf;

  if (!m_source)
    return name;

  tino_buf_resetO(&buf);
  tino_buf_add_sO(&buf, m_source);
  tino_buf_add_sO(&buf, name);
  return tino_buf_get_sN(&buf);
}

static void
do_mkdirs(const char *path, const char *file)
{
  switch (tino_file_mkdirs_forfileE(path, file))
    {
    case -1:
      /* a single retry in case somebody else had made the directory first	*/
      switch (tino_file_mkdirs_forfileE(path, file))
        {
        default:
          return;

        case -1:
          tino_err("failed: mkdir for %s%s%s", path ? path : "", path ? "/" : "", file);
          return;

        case 1:
          break;
        }
      /* fallthrough	*/
    case 1:
      verbose("mkdir for %s%s%s", path ? path : "", path ? "/" : "", file);
      return;
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
  return tino_buf_line_readE(&buf, 0, (m_nulls ? 0 : '\n'));
}

static int
rename_noclobber(const char *name, const char *to)
{
  /* We need a rename which is
   * a) atomic
   * b) fails if *to exists
   *
   * There is renameat2() which can be used.  But is that portable enough?
   * Anyway, use it for now.
   *
   * Note that rename() has a very good purpose:
   *
   * It renames the source into the destination such, that the destination never is gone missing.
   * So it is even double atomic.
   *
   * We should support that, too, but the major idea is not to harm data,
   * so atomically and unconditionally replacing a destination is not what we want.
   *
   * To really be able to atomically replace a destination without doubt, following is needed:
   * a) create a reference to the destination by hardlinking it to the backup location
   * b) atomically replace the destination, if it still resembles the hardlinked file
   *
   * So what we need is a rename() call with 3 arguments:
   * Source	Path to replace the Dest
   * Dest	Path to be replaced
   * Ref	Handle which must be identical to Dest, else the rename fails
   *
   * Also we need a way to "materialize a link to a file handle", so the way would be:
   *
   * - open dest for read(!), giving handle Dest
   * - hardlink Dest (the handle!) to the backup location, so materialize Dest to the filesystem
   * - rename src to dest with Dest as safeguard (this here is missing)
   * - close Dest
   *
   * Why open for read must suffice?  Due to SeLinux.
   * It may be that I am allowed to hardlink, rename and read the file, but writing is forbiden.
   *
   * Well, this also shows another missing link:  Opening a file for reference only, so neither read nor write.
   *
   * Note for Linux:  You probably can hardlink /proc/fd/HANDLE to the given destination,
   * so materializing a file descriptor is present there (even that this is weird).
   */
  return renameat2(AT_FDCWD, name, AT_FDCWD, to, RENAME_NOREPLACE|(WHITEOUT_BY_DEFAULT_GET ? RENAME_WHITEOUT : 0));
}

static int
do_rename(const char *name, const char *to)
{
  if (!rename_noclobber(name, to))
    {
      verbose("rename: %s -> %s", name, to);
      return 0;
    }
  if (errno==EINVAL && !m_enforce && m_whiteout<=0)
    {
      /* EINVAL tells us, that RENAME_NOREPLACE is not available on this filesystem
       *
       * Fallback to normal rename() in case we are allowed to
       */
      if (!rename(name, to))
        {
          verbose("unsafe rename: %s -> %s", name, to);
          return 0;
        }
    }
  tino_err("cannot rename %s -> %s", name, to);
  return 1;
}

/* rename away *name, that is
 * move *name to a *rename
 * possibly into directory (option -c)
 * possibly with an backup extension appended (option -b or option -a)
 */
static int
do_rename_away(const char *name, const char *rename)
{
  char	*tmp1, *tmp2, ret;

  tmp1	= 0;
  if (m_backupdir)	/* option -c present	*/
    rename	= tmp1	= tino_file_glue_pathOi(NULL, 0, m_backupdir, tino_file_filenameptr_constO(rename));
  tmp2	= 0;
  if (!tino_file_notexistsE(rename))
    {
      if (!m_append && !m_backup)
        {
          tino_err("existing backup destination: %s", rename);
          tino_freeO(tmp1);
          return 1;
        }
      rename	= tmp2	= tino_file_backupnameNi(NULL, 0, rename);
    }
  else if (m_mkdirs)
    do_mkdirs(NULL, rename);
  ret = do_rename(name, rename);
  tino_freeO(tmp1);
  tino_freeO(tmp2);
  return ret;
}

static int
do_rename_backup(const char *old, const char *new)
{
  const char	*src;

  src	= get_src(old);

  /* Try to move, skips a lot of syscalls in the most common situation
   */
  if (!rename_noclobber(src, new))
    {
      verbose("rename: %s -> %s", src, new);
      return 0;
    }

  /* Try to figure out what happened	*/

  if (tino_file_notexistsE(src))
    {
      tino_err("missing old name for rename: %s", src);
      return 1;
    }
  errno	= 0;	/* Do not report errors in case there is no error	*/

  if (!tino_file_notexistsE(new))
    {
      /* destination exists	*/
      if (m_append)
        {
          /* just rename away the source (option -a) to the given destination	*/
          return do_rename_away(src, new);
        }
      if (!m_backup && !m_backupdir)
        {
          /* destination exists and we cannot backup the destination	*/
          tino_err("existing destination: %s", new);
          return 1;
        }
      /* rename away the destination	*/
      if (do_rename_away(new, new))
        return 1;
    }
  else if (m_mkdirs)
    do_mkdirs(NULL, new);
  return do_rename(src, new);
}


/**********************************************************************/

static int
do_mvaway(const char *name)
{
  const char	*src;

  src	= get_src(name);
  if (tino_file_notexistsE(src))
    {
      tino_err("missing file to move away: %s", src);
      return 1;
    }
  return do_rename_away(src, src);
}

static int
mvaway(const char *name)
{
  int	ret = 0;

  if (strcmp(name, "-"))
    ret	= do_mvaway(name);
  else
    while ((name=read_dest())!=0)
      ret |= do_mvaway(name);
  return ret;
}


/**********************************************************************/

/* leaks memory	*/
static const char *
do_relative(const char *old, const char *dest)
{
  if (!m_relative)
    return dest;
  return tino_file_glue_pathOi(NULL, 0, tino_file_dirnameOi(NULL, 0, old), tino_file_skip_root_constN(dest));
}

/* leaks memory	*/
static int
mvrename(const char *old, const char *new)
{
  return do_rename_backup(old, do_relative(old, new));
}


/**********************************************************************/

static int
do_mvdest(const char *name)
{
  const char	*targ;
  char		*dest;
  int		ret;

  if (m_relative)
    {
      targ	= tino_file_skip_root_constN(name);
      do_mkdirs(m_dest, targ);
    }
  else
    targ	= tino_file_filenameptr_constO(name);
  dest	= tino_file_glue_pathOi(NULL, 0, m_dest, targ);
  ret	= do_rename_backup(name, dest);
  tino_freeO(dest);
  return ret;
}

static int
mvdest(const char *name)
{
  int	ret = 0;
  if (m_mkdirs<2 && tino_file_notdirE(m_dest))
    {
      tino_err((tino_file_notexistsE(m_dest) ? "missing destination directory: %s" : "existing destination not a directory: %s"), m_dest);
      return 1;
    }
  if (strcmp(name, "-"))
    ret	= do_mvdest(name);
  else
    while ((name=read_dest())!=0)
      ret	|= do_mvdest(name);
  return ret;
}


/**********************************************************************/

static int
is_directory_target(const char *name)
{
  const char	*tmp;

  tmp	= tino_file_filenameptr_constO(name);
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
                      " name..."
                      "\n	if name is - lines are read from stdin."
                      "\n	rename:		%s -r oldname newname"
                      "\n	move:		%s -d dir name..."
                      "\n	rename away:	%s -ab name"
                      "\n	convenience:	alias mv='mvatom -o'"
                      "\n	This is atomic in the sense that it stays on one FS only."
                      "\n	This is not yet atomic in the sense of rename()."
                      ,

                      TINO_GETOPT_USAGE
                      "h	this help"
                      ,

                      TINO_GETOPT_FLAG
                      "0	(This option is 'number zero', not a big O!)\n"
                      "		read 0 terminated lines from stdin\n"
                      "		example: find . -type f -print0 | mvatom -0ab -"
                      , &m_nulls,

                      TINO_GETOPT_FLAG
                      "a	Append .~#~ to source if destination exists\n"
                      "		This has less race conditions than option -b.\n"
                      "		Use with option -b to 'move away' a file."
                      , &m_append,

                      TINO_GETOPT_FLAG
                      "b	Backup existing destination to .~#~\n"
                      "		Cannot be used with option -a except for 'rename away' feature.\n"
                      "		On errors this might leave you with a renamed destination!"
                      , &m_backup,

                      TINO_GETOPT_STRING
                      "c dir	Create backups in the given directory.\n"
                      "		This moves an existing destination into the given dir,\n"
                      "		possibly renaming it according to options -a and -b"
                      , &m_backupdir,

                      TINO_GETOPT_STRING
                      "d dir	target (Destination) directory to move files into"
                      , &m_dest,

                      TINO_GETOPT_FLAG
                      "e	Enforce safe mode\n"
                      "		Fails on filesystems not supporting renameat2(.., RENAME_NOREPLACE)"
                      , &m_enforce,
#if 0
                      TINO_GETOPT_FLAG
                      "f	Force overwrite of destination, needs unsafe mode (option -u)\n"
                      "		This directly calls rename() per move and therefor atomically\n"
                      "		replaces (overwrites) existing destinations unconditionally.\n"
                      "		Also this needs only 1 syscall per move.\n"
                      , &m_force,
#endif
                      TINO_GETOPT_FLAG
                      "i	Ignore (common) errors"
                      , &m_ignore,

                      TINO_GETOPT_FLAG
                      "l	read Lines from stdin, enables '-' as argument\n"
                      "		example: find . -print | mvatom -lb -"
                      , &m_lines,
#if WHITEOUT_BY_DEFAULT
                      TINO_GETOPT_FLAG
                      TINO_GETOPT_MIN
                      "n	no whiteout, opposite of option -w"
                      , &m_whiteout,
                      -1,
#endif
                      TINO_GETOPT_FLAG
                      "o	Original behavior if directory is the last target\n"
                      "		The last argument must end in a / or must be . or .."
                      , &m_original,

                      TINO_GETOPT_FLAG
                      TINO_GETOPT_MAX
                      "p	create missing Parent directories (for -r)\n"
                      "		Give twice to create for -d, too"
                      , &m_mkdirs,
                      2,

                      TINO_GETOPT_FLAG
                      "q	silently fail"
                      , &m_quiet,

                      TINO_GETOPT_FLAG
                      "r	append Relative source path to dest (needs no -p)\n"
                      "		To rename within a directory without moving use:\n"
                      "		mvatom -r /path/to/file/a b"
                      , &m_relative,

                      TINO_GETOPT_STRING
                      "s src	append the given Src prefix, for an usage like:\n"
                      "		( cd whatever; ls -1; ) | mvatom -l -s whatever/ -d todir -\n"
                      "		The source prefix is not a directory, it's a literal prefix"
                      , &m_source,
#if 0
                      TINO_GETOPT_STRING
                      "t dir	use the given Temporary working directory for safe mode\n"
                      "		The directory must be in the same filesystem as source and dest.\n"
                      "		Without this option such a directory is created.  In unsafe mode\n"
                      "		this directory is not used, but it still must be present."
                      , &m_tmpdir,

                      TINO_GETOPT_FLAG
                      "u	Unsafe mode, needs 2 syscalls instead of 3 by using rename()\n"
                      "		This mode was previously the default.  It first checks the\n"
                      "		destination for existence and then uses rename() to move a file.\n"
                      "		If the destination is created between those two syscalls, this\n"
                      "		cannot be detected and therefor rename() overwrites (destroys) it."
                      , &m_unsafe,
#endif
                      TINO_GETOPT_FLAG
                      "v	verbose"
                      , &m_verbose,

                      TINO_GETOPT_FLAG
                      TINO_GETOPT_MIN
                      "w	use whiteout, see renameat2(.., RENAME_WHITEOUT)\n"
                      "		implies option -e.  (Currently not yet the default.)"
                      , &m_whiteout,
                      1,

                      NULL);

printf("%d\n", m_whiteout);
  if (argn<=0)
    return 1;

  if (m_original && !m_dest && argn+1<argc && is_directory_target(argv[argc-1]))
    m_dest	= argv[--argc];
  if (!m_dest && m_backup && m_append && argc==argn+1)
    {
      mvaway(argv[argn]);
      return errflag;
    }
  if (m_backup && m_append)
    tino_err("Options -a and -b cannot be used together this way");
  if (m_dest)
    {
      while (argn<argc)
        mvdest(argv[argn++]);
    }
  else if (argc==argn+2)
    mvrename(argv[argn], argv[argn+1]);
  else
    tino_err("Option -d not present: Rename needs 2 arguments; For 'move away' now use options -ab");
  return errflag;
}

/* How to implement safe atomic mode: (Future plan)
 *
 * Prerequisites:
 * - source exists, destination exists
 * - neither source nor destination must be overwritten
 * - no race conditions make files disappear
 * Following solution fails:
 * - hardlink source to backup
 * - exchange backup and destination
 * - safely remove source - there is no such thing
 * Following solution tries to fix that but fails, too:
 * - safely rename source to backup
 * - safely exchange backup and destination
 * Reasons:
 * - If exchange fails, how to safely move back source?
 * - In case of a brownout the source is gone (it is on backup, but who knows this!?!)
 *
 * Hence we need following algorithm:
 * - create temp subdirectory at source -- fail if this fails
 * - hardlink destination to backup -- destination now is kept safe, multiple backups are no problem
 * - safely rename source into temp -- this assumes no other uses this subdir
 * - exchange source (in temp) with destination
 * - fail if dest in temp (named after source) is not the same as the hardlinked dest
 * - remove dest in temp -- assumes no other process operates on subdir, so this is safe
 * - rmdir temp
 *
 * If "exchange" fails:
 * - safely rename source back from temp and rmdir temp
 * - If this fails, too:  Leave the directory as-is, if it is properly named we understand what happened
 * - As source in temp can be named the same as source on directory up, everything is selfdocumenting
 * If "safely rename source" fails:
 * - Either we have a FS not supporting renameat2() flags -- see below
 * - Or we have another problem - just bail out
 * Now what to do if renameat2() is not supported?
 * - remove temp directory -- fail if this fails
 * - fail if -e present
 * - rename() source to destination
 *
 * Unsafe mode:
 * - hardlink dest to backup
 * - rename() source to dest
 * Unsafe forced mode:
 * - rename() source to dest
 */

