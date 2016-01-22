/* GPLv2 applies
 * SVN revision: $Revision: 671 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <regex.h>
#if defined(__GLIBC__)
#include <execinfo.h>
#endif
#include <ncursesw/ncurses.h>

#include "error.h"

void error_exit(BOOL sys_err, char *format, ...)
{
	int e = errno;
	va_list ap;

#if defined(__GLIBC__)
	int index;
        void *trace[128];
        int trace_size = backtrace(trace, 128);
        char **messages = backtrace_symbols(trace, trace_size);
#endif

	(void)endwin();

	va_start(ap, format);
	(void)vfprintf(stderr, format, ap);
	va_end(ap);

	if (sys_err == TRUE)
		fprintf(stderr, "error: %s (%d)\n", strerror(e), e);

	fflush(NULL);

#if defined(__GLIBC__)
        fprintf(stderr, "Execution path:\n");
        for(index=0; index<trace_size; ++index)
                fprintf(stderr, "\t%d %s\n", index, messages[index]);
#endif

	printf("Press enter to continue...\n");
	getchar();

#ifdef _DEBUG
{ char *dummy = NULL; *dummy = 123; }
#else
	exit(EXIT_FAILURE);
#endif
}

