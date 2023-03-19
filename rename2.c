/*
 * Atomic file move utility:
 * renameat2(.., RENAME_EXCHANGE) syscall
 *
 * This Works is placed under the terms of the Copyright Less License,
 * see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.
 *
 * Read: Free as free beer, free speech and free baby.
 * Ever saw a Copyright on a baby?
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include "mvatom_version.h"	/* Makfile sadly does not create version file for all (yet)	*/

void
OOPS(int c, ...)
{
  const char	*s;
  va_list	list;

  va_start(list, c);
  while ((s = va_arg(list, const char *))!=0)
    fputs(s, stderr);
  va_end(list);
  exit(c);
}

int
main(int argc, char **argv)
{
  int		mode;
  const char	*modestr;

  switch (argc==4 && argv[1][0] && argv[1][1] && !argv[1][2] ? argv[1][1] : 0)
    {
    default:
      OOPS(42, "Usage: ", argv[0], " src dest\n\tVERSION " MVATOM_VERSION " This uses renameat2(.., MODE) with\n"
	"\t-f\tforce: unconditionally overwrite destination\n"
	"\t-n\tRENAME_NOREPLACE: does not override destination\n"
	"\t-r\tremove source on union-FS: -f with RENAME_WHITEOUT\n"
	"\t-w\tRENAME_NOREPLACE+RENAME_WHITEOUT\n"
	"\t-x\tRENAME_EXCHANGE: exchange source and dest\n"
	"\texit codes: 0=ok 1=fail 2=EINVAL(=unsupported mode) 42=help\n"
	, NULL);

    case 'f':	mode=0;			modestr="0";			break;
    case 'n':	mode=RENAME_NOREPLACE;	modestr="RENAME_NOREPLACE";	break;
    case 'r':	mode=RENAME_WHITEOUT;	modestr="RENAME_WHITEOUT";	break;
    case 'w':	mode=RENAME_NOREPLACE|RENAME_WHITEOUT;	modestr="RENAME_NOREPLACE+RENAME_WHITEOUT";	break;
    case 'x':	mode=RENAME_EXCHANGE;	modestr="RENAME_EXCHANGE";	break;
    }
  if (renameat2(AT_FDCWD, argv[2], AT_FDCWD, argv[3], mode))
    OOPS(errno==EINVAL ? 2 : 1, "failed: renameat2 ", argv[2], " ", argv[3], " ", modestr, ": ", strerror(errno), "\n", NULL);
  return 0;
}

