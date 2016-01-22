/* GPLv2 applies
 * SVN revision: $Revision: 709 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#ifndef __GREP_FILTER_H__
#define __GREP_FILTER_H__

#include <regex.h>
#include <sys/types.h>

typedef struct
{
	const char *sserver, *schannel;
	const char *file;
	const char *re;
	regex_t filter;
} grep_filter_t;

typedef struct
{
	grep_filter_t *gf_list;
	int n_gf_list;
} grep_target;

grep_target *alloc_grep_target(void);

BOOL add_grep_filter(grep_target *gp, const char *re, const char *sserver, const char *schannel, const char *file, char **err);
BOOL process_grep_filter(const grep_target *gp, const char *sserver, const char *schannel, const char *line, char **err, BOOL *match);
void free_grep_filters(grep_target *gp);
BOOL dump_grep_filters(const grep_target *gp, const char *target, FILE *fh);

#endif
