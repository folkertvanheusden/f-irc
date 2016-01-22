/* GPLv2 applies
 * SVN revision: $Revision: 727 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "error.h"
#include "ignores.h"
#include "config.h"
#include "utils.h"
#include "string_array.h"

char *ignore_file = NULL;
ignore *ignores = NULL;
int n_ignores = 0;

int find_ignore(const char *channel, const char *nick, const char *name_complete)
{
	int loop;

	for(loop=0; loop<n_ignores; loop++)
	{
		if (strcasecmp(ignores[loop].channel, channel) == 0 &&
			(strcasecmp(ignores[loop].nick, nick) == 0 ||
				(name_complete != NULL &&
				strcasecmp(ignores[loop].name_complete, name_complete) == 0 &&
				strcasecmp(name_complete, IGNORE_NOT_SET) != 0)
			)
		)
		{
			return loop;
		}
	}

	return -1;
}

void add_ignore(const char *channel, const char *nick, const char *name_complete)
{
	if (find_ignore(channel, nick, name_complete) == -1)
	{
		ignores = realloc(ignores, sizeof(ignore) * (n_ignores + 1));

		ignores[n_ignores].channel = strdup(channel);
		ignores[n_ignores].nick = strdup(nick);
		ignores[n_ignores].name_complete = strdup(name_complete);

		n_ignores++;
	}
}

void del_ignore(const char *channel, const char *nick)
{
	int index = 0, found = -1;

	for(index=0; index<n_ignores; index++)
	{
		if (strcasecmp(ignores[index].channel, channel) == 0 && strcasecmp(ignores[index].nick, nick) == 0)
		{
			found = index;
			break;
		}
	}

	if (found != -1)
	{
		int n_move = (n_ignores - found) - 1;

		myfree(ignores[index].channel);
		myfree(ignores[index].nick);
		myfree(ignores[index].name_complete);

		if (n_move > 0)
			memmove(&ignores[found], &ignores[found + 1], sizeof(ignore) * n_move);

		n_ignores--;
	}
}

BOOL load_ignore_list(const char *file)
{
	int fd = open(file, O_RDONLY);
	if (fd == -1)
		return FALSE;

	ignore_file = strdup(file);

	for(;;)
	{
		char *line = read_line_fd(fd);
		string_array_t parts;

		init_string_array(&parts);

		if (!line)
			break;

		if (strlen(line) == 0)
		{
			myfree(line);
			continue;
		}

		split_string(line, " ", TRUE, &parts);

		if (string_array_get_n(&parts) == 2)
			add_ignore(string_array_get(&parts, 0), string_array_get(&parts, 1), IGNORE_NOT_SET);
		else if (string_array_get_n(&parts) == 3)
			add_ignore(string_array_get(&parts, 0), string_array_get(&parts, 1), string_array_get(&parts, 2));
		else
			error_exit(FALSE, "'ignore' (%s) missing parameter: must be 'channel nick [name_complete]'", line);

		free_splitted_string(&parts);

		myfree(line);
	}

	close(fd);

	return TRUE;
}

void free_ignores(void)
{
	int loop = 0;

	for(loop=0; loop<n_ignores; loop++)
	{
		myfree(ignores[loop].channel);
		myfree(ignores[loop].nick);
		myfree(ignores[loop].name_complete);
	}

	myfree(ignores);
	ignores = NULL;

	n_ignores = 0;
}

BOOL save_ignore_list(void)
{
	int loop = 0;
	BOOL ok = TRUE;
	FILE *fh = NULL;

	if (!ignore_file)
		asprintf(&ignore_file, "%s/%s", dirname(conf_file), ".firc.ignore");

	fh = fopen(ignore_file, "w");
	if (!fh)
		return FALSE;

	for(loop=0; loop<n_ignores; loop++)
	{
		int rc = -1;

		if (strcmp(ignores[loop].name_complete, IGNORE_NOT_SET) == 0)
			rc = fprintf(fh, "%s %s\n", ignores[loop].channel, ignores[loop].nick);
		else
			rc = fprintf(fh, "%s %s %s\n", ignores[loop].channel, ignores[loop].nick, ignores[loop].name_complete);

		if (rc < 0)
			ok = FALSE;
	}

	fclose(fh);

	return ok;
}

BOOL check_ignore(const char *channel, const char *nick, const char *name_complete)
{
	return find_ignore(channel, nick, name_complete) != -1;
}
