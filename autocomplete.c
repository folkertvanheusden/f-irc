/* GPLv2 applies
 * SVN revision: $Revision: 716 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gen.h"
#include "autocomplete.h"
#include "utils.h"
#include "term.h"
#include "buffer.h"
#include "channels.h"
#include "servers.h"
#include "loop.h"
#include "main.h"

tab_completion_t tab_completion[] = 
{
	{ "/ADDSERVER" },
	{ "/ADMIN" },
	{ "/AWAY" },
	{ "/BAN" },
	{ "/CTCP" },
	{ "/CONNECT" },
	{ "/DCCSEND" },
	{ "/EXIT" },
	{ "/IGNORE" },
	{ "/INFO" },
	{ "/INVITE" },
	{ "/ISON" },
	{ "/JOIN" },
	{ "/KEEPTOPIC" },
	{ "/KICK" },
	{ "/KILL" },
	{ "/LEAVE" },
	{ "/LINKS" },
	{ "/LIST" },
	{ "/MODE" },
	{ "/MSG" },
	{ "/NAMES" },
	{ "/NICK" },
	{ "/NOTICE" },
	{ "/OPER" },
	{ "/PART" },
	{ "/PASS" },
	{ "/PING" },
	{ "/PRIVMSG" },
	{ "/QUIT" },
	{ "/QUOTE" },
	{ "/RAW" },
	{ "/REHASH" },
	{ "/RESTART" },
	{ "/SAVECONFIG" },
	{ "/SEARCHALL" },
	{ "/SPAM" },
	{ "/STATS" },
	{ "/SUMMON" },
	{ "/TIME" },
	{ "/TOPIC" },
	{ "/TRACE" },
	{ "/UNIGNORE" },
	{ "/USER" },
	{ "/USERHOST" },
	{ "/USERS" },
	{ "/VERSION" },
	{ "/WALLOPS" },
	{ "/WHO" },
	{ "/WHOIS" },
	{ "/WHOWAS" },
	{ NULL }
};

char *make_complete_command(const char *in)
{
	int n_elements_equal = 0;
	char *matching_str = NULL, *dummy = NULL;
	int matching_str_len = 32767;
	int index = 0;
	int in_len = strlen(in);
	int matching_index = -1;
	char str_buffer[4096] = { 0 };
	unsigned int nb = 0;

	while(tab_completion[index].what)
	{
		if (strncasecmp(tab_completion[index].what, in, in_len) == 0)
		{
			if (matching_str)
			{
				int loop;
				int cur_len = strlen(tab_completion[index].what);

				for(loop=in_len; loop<min(matching_str_len, cur_len); loop++)
				{
					if (tab_completion[index].what[loop] != matching_str[loop])
						break;
				}

				matching_str[loop] = 0x00;
				matching_str_len = loop;
				matching_index = -1;	/* no longer completely matching */
			}
			else
			{
				matching_index = index;
				matching_str = strdup(tab_completion[index].what);
				matching_str_len = strlen(matching_str);
			}

			nb += snprintf(&str_buffer[nb], sizeof str_buffer - nb, "%s ", tab_completion[index].what);
			n_elements_equal++;

			if (nb >= sizeof str_buffer - 1)
				break;
		}

		index++;
	}

	if (n_elements_equal > 1)
		update_statusline(current_server, current_server_channel_nr, "Matching: %s", str_buffer);

	if (matching_index != -1)
	{
		asprintf(&dummy, "%s ", matching_str);

		myfree(matching_str);
		matching_str = dummy;
	}

	return matching_str;
}

char *make_complete_nickorchannel(char *in, int start_of_line)
{
	int n_elements_equal = 0;
	char *matching_str = NULL, *dummy = NULL;
	int matching_str_len = 32767;
	int index = 0;
	int in_len = strlen(in);
	int matching_index = -1;
	int cur_n_channels = cur_server() -> n_channels;
	int cur_n_names = cur_channel() -> n_names;
	int matching_type = -1;
	char str_buffer[4096] = { 0 };
	unsigned int nb = 0;

	for(index=0; index<(cur_n_channels + cur_n_names); index++)
	{
		char *compare_str;
		int type;

		if (index >= cur_n_channels)
		{
			char *dummy = cur_channel() -> persons[index - cur_n_channels].nick;
			if (dummy[0] == '@')
				compare_str = &dummy[1];
			else
				compare_str = dummy;
			type = 1; /* nick */
		}
		else
		{
			compare_str = cur_server() -> pchannels[index].channel_name;
			type = 0; /* channel */
		}

		if (strncasecmp(compare_str, in, in_len) == 0)
		{
			if (matching_str)
			{
				int loop;
				int cur_len = strlen(compare_str);

				for(loop=in_len; loop<min(matching_str_len, cur_len); loop++)
				{
					if (compare_str[loop] != matching_str[loop])
						break;
				}
				matching_str[loop] = 0x00;
				matching_str_len = loop;
				matching_index = -1;	/* no longer completely matching */
			}
			else
			{
				matching_index = index;
				matching_type = type;
				matching_str = strdup(compare_str);
				matching_str_len = strlen(matching_str);
			}

			nb += snprintf(&str_buffer[nb], sizeof str_buffer - nb, "%s ", compare_str);
			n_elements_equal++;

			if (nb >= sizeof str_buffer - 1)
				break;
		}
	}

	if (n_elements_equal > 1)
		update_statusline(current_server, current_server_channel_nr, "Matching: %s", str_buffer);

	if (matching_index != -1)
	{
		if (matching_type == 1 && start_of_line)
			asprintf(&dummy, "%s: ", matching_str);
		else
			asprintf(&dummy, "%s ", matching_str);

		myfree(matching_str);
		matching_str = dummy;
	}

	return matching_str;
}
