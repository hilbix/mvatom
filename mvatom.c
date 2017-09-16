/* $Header$
 *
 * Atomic file move utility.
 *
 * On supported platforms this operation is atomic in the sense, that:
 * - destination path never does not exist
 * - it is free of race conditions, even on power outage
 *
 * Supported: Linux
 *
 * This Works is placed under the terms of the Copyright Less License,
 * see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.
 */

#define TINO_NEED_OLD_ERR_FN
#include "tino/filetool.h"
#include "tino/ex.h"
#include "tino/getopt.h"
#include "tino/buf_line.h"

#include "mvatom_version.h"

static int		errflag;
static int		m_backup, m_ignore, m_nulls, m_lines, m_relative, m_quiet, m_verbose, m_mkdirs, m_append;
static const char	*m_dest, *m_source, *m_backupdir, *m_tmpdir;


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
  if (tino_file_renameE(name, to))
    {
      tino_err("cannot rename %s -> %s", name, to);
      return;
    }
  verbose("rename: %s -> %s", name, to);
}

static void
do_rename_away(const char *name, const char *rename)
{
  char	*tmp, *o;

  o	= 0;
  if (!m_backupdir || !tino_file_notexistsE(rename=tmp=o=tino_file_glue_pathOi(NULL, 0, m_backupdir, tino_file_filenameptr_constO(rename))))
    tmp	= tino_file_backupnameNi(NULL, 0, rename);
  do_rename(name, tmp);
  tino_freeO(tmp);
  if (o!=tmp)
    tino_freeO(o);
}

static void
do_rename_backup(const char *old, const char *new)
{
  const char	*src;

  src	= get_src(old);

#if 0
  XXX DOES NOT WORK XXX;
  /* Try to move, skips 2 syscalls in the most common situation
   */
  if (!rename(src, new))
    {
      verbose("rename: %s -> %s", src, new);
      return;
    }
#endif

  /* Try to figure out what happened	*/

  if (tino_file_notexistsE(src))
    {
      tino_err("missing old name for rename: %s", src);
      return;
    }
  errno	= 0;	/* Do not report errors in case there is no error	*/

  if (!tino_file_notexistsE(new))
    {
      if (m_append)
        {
	  do_rename_away(src, new);
	  return;
	}
      if (!m_backup)
	{
	  /* Meaning corrected
	   */
	  tino_err("existing destination: %s", new);
	  return;
	}
      do_rename_away(new, new);
    }
  else if (m_mkdirs)
    do_mkdirs(NULL, new);
  do_rename(src, new);
}


/**********************************************************************/

static void
do_mvaway(const char *name)
{
  const char	*src;

  src	= get_src(name);
  if (tino_file_notexistsE(src))
    {
      tino_err("missing file to move away: %s", src);
      return;
    }
  do_rename_away(src, src);
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
  return tino_file_glue_pathOi(NULL, 0, tino_file_dirnameOi(NULL, 0, old), tino_file_skip_root_constN(dest));
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
      targ	= tino_file_skip_root_constN(name);
      do_mkdirs(m_dest, targ);
    }
  else
    targ	= tino_file_filenameptr_constO(name);
  dest	= tino_file_glue_pathOi(NULL, 0, m_dest, targ);
  do_rename_backup(name, dest);
  tino_freeO(dest);
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
		      " name...\n"
		      "	if name is - lines are read from stdin.\n"
		      "	rename:		%s -r oldname newname\n"
		      "	move:		%s -d dir name...\n"
		      "	rename away:	%s -ab name\n"
		      "	convenience:	alias mv='mvatom -o'",

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
		      "i	Ignore (common) errors"
		      , &m_ignore,

		      TINO_GETOPT_FLAG
		      "l	read Lines from stdin, enables '-' as argument\n"
		      "		example: find . -print | mvatom -lb -"
		      , &m_lines,

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
#endif
		      TINO_GETOPT_FLAG
		      "v	verbose"
		      , &m_verbose,

		      NULL);

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
