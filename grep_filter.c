/* GPLv2 applies
 * SVN revision: $Revision: 709 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gen.h"
#include "grep_filter.h"
#include "utils.h"

grep_target *alloc_grep_target(void)
{
	return (grep_target *)calloc(sizeof(grep_target), 1);
}

BOOL add_grep_filter(grep_target *gp, const char *re, const char *sserver, const char *schannel, const char *file, char **err)
{
	int rc = 0;

	gp -> gf_list = (grep_filter_t *)realloc(gp -> gf_list, (gp -> n_gf_list + 1)* sizeof(grep_filter_t));
	memset(&gp -> gf_list[gp -> n_gf_list], 0x00, sizeof(grep_filter_t));

	rc = regcomp(&gp -> gf_list[gp -> n_gf_list].filter, re, REG_NOSUB);
	if (rc)
	{
		char buffer[4096] = { 0 };

		regerror(rc, &gp -> gf_list[gp -> n_gf_list].filter, buffer, sizeof buffer);
		*err = strdup(buffer);

		return FALSE;
	}

	gp -> gf_list[gp -> n_gf_list].re = strdup(re);
	gp -> gf_list[gp -> n_gf_list].sserver = strlen(sserver) ? strdup(sserver) : NULL;
	gp -> gf_list[gp -> n_gf_list].schannel = strlen(schannel) ? strdup(schannel) : NULL;
	gp -> gf_list[gp -> n_gf_list].file = strlen(file) ? strdup(file) : NULL;
	gp -> n_gf_list++;

	*err = NULL;

	return TRUE;
}

BOOL process_grep_filter(const grep_target *gp, const char *sserver, const char *schannel, const char *line, char **err, BOOL *match)
{
	BOOL ok = TRUE;
	int loop = 0;

	*err = NULL;
	*match = FALSE;

	for(loop=0; loop<gp -> n_gf_list && ok; loop++)
	{
		if ((gp -> gf_list[loop].sserver == NULL || strcasecmp(gp -> gf_list[loop].sserver, sserver) == 0) &&
			(gp -> gf_list[loop].schannel == NULL || strcasecmp(gp -> gf_list[loop].schannel, schannel) == 0))
		{
			if (regexec(&gp -> gf_list[loop].filter, line, 0, NULL, 0) == 0)
			{
				*match = TRUE;

				if (gp -> gf_list[loop].file)
				{
					FILE *fh = fopen(gp -> gf_list[loop].file, "a+");
					if (!fh)
					{
						asprintf(err, "Cannot open %s: %s\n", gp -> gf_list[loop].file, strerror(errno));
						ok = FALSE;
					}
					else
					{
						if (fprintf(fh, "%s\n", line) <= 0)
						{
							asprintf(err, "Cannot write to %s: %s\n", gp -> gf_list[loop].file, strerror(errno));
							ok = FALSE;
						}

						fclose(fh);
					}
				}
			}
		}
	}

	return ok;
}

void free_grep_filters(grep_target *gp)
{
	int loop = 0;

	for(loop=0; loop<gp -> n_gf_list; loop++)
	{
		regfree(&gp -> gf_list[loop].filter);
		myfree(gp -> gf_list[loop].re);
		myfree(gp -> gf_list[loop].sserver);
		myfree(gp -> gf_list[loop].schannel);
		myfree(gp -> gf_list[loop].file);
	}

	free(gp -> gf_list);
	gp -> gf_list = NULL;

	gp -> n_gf_list = 0;
}

BOOL dump_grep_filters(const grep_target *gp, const char *target, FILE *fh)
{
	int loop = 0;

	if (gp -> n_gf_list)
		fprintf(fh, ";\n");

	for(loop=0; loop<gp -> n_gf_list; loop++)
	{
		if (fprintf(fh, "%s=%s,%s,%s,%s\n", target, str_or_nothing(gp -> gf_list[loop].sserver), str_or_nothing(gp -> gf_list[loop].schannel), str_or_nothing(gp -> gf_list[loop].file), gp -> gf_list[loop].re) <= 0)
			return FALSE;
	}

	return TRUE;
}
