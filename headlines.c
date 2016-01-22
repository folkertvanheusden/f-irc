/* GPLv2 applies
 * SVN revision: $Revision: 760 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "headlines.h"
#include "term.h"
#include "config.h"
#include "loop.h"
#include "utils.h"

string_array_t matchers;
static string_array_t headlines;
static time_t last_headline_update = 0;
static const char *current = NULL;

void add_headline_matcher(const char *str)
{
	add_to_string_array(&matchers, str);
}

BOOL dump_headline_matchers(FILE *fh)
{
	return dump_string_array(&matchers, "headline_matcher", fh);
}

void init_headlines(void)
{
	init_string_array(&headlines);
}

void free_headlines(void)
{
	free_string_array(&headlines);

	myfree(current);
	current = NULL;
}

void add_headline(BOOL prio, const char *what)
{
	if (prio && string_array_get_n(&headlines) > 0)
	{
		insert_into_string_array(&headlines, 0, what);

		last_headline_update = 0;
	}
	else
	{
		while(string_array_get_n(&headlines) >= MAX_HEADLINES_QUEUED)
			del_nr_from_string_array(&headlines, 0);

		add_to_string_array(&headlines, what);
	}
}

BOOL update_headline(BOOL force)
{
	time_t now = time(NULL);

	if (headline_window)
	{
		if (now - last_headline_update >= NEXT_HEADLINE_INTERVAL && string_array_get_n(&headlines) > 0 && show_headlines)
		{
			const char *new = strdup(string_array_get(&headlines, 0));

			wclear(headline_window -> win);
			mvwprintw(headline_window -> win, 0, 0, "%s", new);

			del_nr_from_string_array(&headlines, 0);

			myfree(current);
			current = new;

			last_headline_update = now;

			return TRUE;
		}
		else if (force && current)
		{
			wclear(headline_window -> win);

			mvwprintw(headline_window -> win, 0, 0, "%s", current);

			return TRUE;
		}
	}

	return FALSE;
}

void check_headline_matches(const char *haystack, const char *headline)
{
	int index = 0;

	for(index=0; index<string_array_get_n(&matchers); index++)
	{
		if (strcasestr(haystack, string_array_get(&matchers, index)))
		{
			add_headline(TRUE, headline);
			break;
		}
	}
}

void apply_show_headlines(void)
{
	if (!show_headlines)
	{
		delete_window(headline_window);
		headline_window = NULL;
	}
}
