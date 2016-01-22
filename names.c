/* GPLv2 applies
 * SVN revision: $Revision: 819 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#include <stdlib.h>
#include <ncursesw/panel.h>
#include <ncursesw/ncurses.h>
#include <string.h>
#include <unistd.h>

#include "gen.h"
#include "theme.h"
#include "term.h"
#include "buffer.h"
#include "channels.h"
#include "servers.h"
#include "irc.h"
#include "names.h"
#include "utils.h"
#include "loop.h"
#include "config.h"
#include "main.h"
#include "user.h"
#include "ignores.h"

int names_offset = 0, names_cursor = 0;

void free_person(person_t *p)
{
	myfree(p -> nick);
	myfree(p -> complete_name);
	myfree(p -> user_host);
}

void delete_index(int server_index, int channel_index, int name_index)
{
	channel *pc = &server_list[server_index].pchannels[channel_index];
	int move_n = pc -> n_names - (name_index + 1);

	free_person(&pc -> persons[name_index]);

	memmove(&pc -> persons[name_index], &pc -> persons[name_index + 1], move_n * sizeof(person_t));

	pc -> n_names--;
}

void change_nick(int server_index, int channel_index, const char *old_nick, const char *new_nick)
{
	channel *pc = &server_list[server_index].pchannels[channel_index];
	int insert_index = 0, nick_found_at = -1;
	const char *search_nick = has_nick_mode(old_nick) ? old_nick + 1 : old_nick;

	if (strcasecmp(search_nick, new_nick) != 0)
	{
		search_for_nick(pc, search_nick, &nick_found_at, &insert_index);

		if (nick_found_at != -1)
		{
			person_t *p = &pc -> persons[nick_found_at];
			const char *complete_name = p -> complete_name ? strdup(p -> complete_name) : NULL;
			const char *user_host = p -> user_host ? strdup(p -> user_host) : NULL;

			/* this to keep the list sorted */
			delete_index(server_index, channel_index, nick_found_at);

			add_nick(server_index, channel_index, new_nick, complete_name, user_host);

			myfree(user_host);
			myfree(complete_name);
		}
	}
}

void change_name(int server_index, int channel_index, const char *nick, const char *new_name)
{
	channel *pc = &server_list[server_index].pchannels[channel_index];
	int insert_index = 0, nick_found_at = -1;
	const char *search_nick = has_nick_mode(nick) ? nick + 1 : nick;

	search_for_nick(pc, search_nick, &nick_found_at, &insert_index);

	if (nick_found_at != -1)
	{
		person_t *p = &pc -> persons[nick_found_at];

		myfree(p -> complete_name);
		p -> complete_name = strdup(new_name);

		LOG("DO_WHOIS %s %s %s\n", search_nick, nick, new_name);
	}
}

void change_user_host(int server_index, int channel_index, const char *nick, const char *new_user_host)
{
	channel *pc = &server_list[server_index].pchannels[channel_index];
	int insert_index = 0, nick_found_at = -1;
	const char *search_nick = has_nick_mode(nick) ? nick + 1 : nick;

	search_for_nick(pc, search_nick, &nick_found_at, &insert_index);

	if (nick_found_at != -1)
	{
		person_t *p = &pc -> persons[nick_found_at];

		myfree(p -> user_host);
		p -> user_host = strdup(new_user_host);
	}
}

void search_for_nick(channel *cur_channel, const char *nick, int *found_at, int *insert_at)
{
        int imin = 0, imax = cur_channel -> n_names - 1;
	const char *search_nick = has_nick_mode(nick) ? nick + 1 : nick;

        while(imax >= imin)
        {
                int imid = (imin + imax) / 2;
                int cmp = strcasecmp(cur_channel -> persons[imid].nick, search_nick);

                if (cmp < 0)
                        imin = imid + 1;
                else if (cmp > 0)
                        imax = imid - 1;
                else
                {
                        *found_at = imid;
			*insert_at = -1;
                        return;
                }
        }

	*insert_at = imin;
	*found_at = -1;
}

irc_user_mode_t text_to_nick_mode(const char *nick)
{
	if (nick[0] == '@')
		return MODE_OPERATOR;

	if (nick[0] == '+')
		return MODE_CAN_SPEAK;

	return 0;
}

void add_nick(int server_index, int channel_index, const char *nick, const char *complete_name, const char *user_host)
{
	int insert_index = 0, nick_found_at = -1;
	channel *cur_channel = &server_list[server_index].pchannels[channel_index];
	const char *search_nick = has_nick_mode(nick) ? nick + 1 : nick;

	search_for_nick(cur_channel, search_nick, &nick_found_at, &insert_index);

	if (nick_found_at == -1)
	{
		int new_n = cur_channel -> n_names + 1, move_n = cur_channel -> n_names - insert_index;

		cur_channel -> persons = realloc(cur_channel -> persons, new_n * sizeof(person_t));

		if (move_n > 0)
			memmove(&(cur_channel -> persons)[insert_index + 1], &(cur_channel -> persons)[insert_index], sizeof(person_t) * move_n);

		/* set */
		(cur_channel -> persons)[insert_index].nick = strdup(search_nick);
		(cur_channel -> persons)[insert_index].complete_name = complete_name ? strdup(complete_name) : NULL;
		(cur_channel -> persons)[insert_index].user_host = user_host ? strdup(user_host) : NULL;
		(cur_channel -> persons)[insert_index].mode = text_to_nick_mode(nick);
		(cur_channel -> persons)[insert_index].ignored = check_ignore(cur_channel -> channel_name, nick, complete_name);

		cur_channel -> n_names++;
	}
	else
	{
		myfree((cur_channel -> persons)[nick_found_at].complete_name);
		(cur_channel -> persons)[nick_found_at].complete_name = complete_name ? strdup(complete_name) : NULL;

		myfree((cur_channel -> persons)[nick_found_at].user_host);
		(cur_channel -> persons)[nick_found_at].user_host = user_host ? strdup(user_host) : NULL;

		(cur_channel -> persons)[nick_found_at].ignored = check_ignore(cur_channel -> channel_name, nick, complete_name);
	}
}

BOOL has_nick(int sr, int ch, const char *nick)
{
	int nick_found_at = -1, insert_index = -1;
	channel *cur_channel = &server_list[sr].pchannels[ch];
	const char *search_nick = has_nick_mode(nick) ? nick + 1 : nick;

	search_for_nick(cur_channel, search_nick, &nick_found_at, &insert_index);

	return nick_found_at != -1;
}

void delete_from_channel_by_nick(int sr, int ch, const char *nick)
{
	int insert_index = 0, nick_found_at = -1;
	channel *cur_channel = &server_list[sr].pchannels[ch];
	const char *search_nick = has_nick_mode(nick) ? nick + 1 : nick;

	search_for_nick(cur_channel, search_nick, &nick_found_at, &insert_index);

	if (nick_found_at != -1)
	{
		int n_to_move = (cur_channel -> n_names - nick_found_at) - 1;

		free_person(&(cur_channel -> persons)[nick_found_at]);

		if (n_to_move > 0)
			memmove(&(cur_channel -> persons)[nick_found_at], &(cur_channel -> persons)[nick_found_at + 1], n_to_move * sizeof(person_t));

		cur_channel -> n_names--;
	}
}

void delete_by_nick(int server_index, const char *nick)
{
	const char *search_nick = has_nick_mode(nick) ? nick + 1 : nick;
	server *ps = &server_list[server_index];
	int channel_index = 0;

	for(channel_index = 0; channel_index < ps -> n_channels; channel_index++)
		delete_from_channel_by_nick(server_index, channel_index, search_nick);
}

void update_user_host(int sr, const char *prefix)
{
	server *ps = &server_list[sr];
	int channel_index = 0;
	char *nick = strdup(prefix);

	terminate_str(nick, '!');

	for(channel_index = 0; channel_index < ps -> n_channels; channel_index++)
	{
		int insert_index = 0, nick_found_at = -1;
		channel *cur_channel = &server_list[sr].pchannels[channel_index];

		search_for_nick(cur_channel, nick, &nick_found_at, &insert_index);

		if (nick_found_at != -1)
		{
			myfree((cur_channel -> persons)[nick_found_at].user_host);

			(cur_channel -> persons)[nick_found_at].user_host = strdup(prefix);
		}
	}

	free(nick);
}

void replace_nick(int server_index, const char *old_nick, const char *new_nick)
{
	server *ps = &server_list[server_index];
	int channel_index = 0;
	const char *search_nick = has_nick_mode(old_nick) ? old_nick + 1 : old_nick;

	for(channel_index = 0; channel_index < ps -> n_channels; channel_index++)
		change_nick(server_index, channel_index, search_nick, new_nick);
}

irc_user_mode_t get_nick_mode(int server_index, int channel_index, const char *nick)
{
	int insert_index = 0, nick_found_at = -1;
	channel *cur_channel = &server_list[server_index].pchannels[channel_index];
	const char *search_nick = has_nick_mode(nick) ? nick + 1 : nick;

	search_for_nick(cur_channel, search_nick, &nick_found_at, &insert_index);

	if (nick_found_at != -1)
		return (cur_channel -> persons)[nick_found_at].mode;

	return 0;
}

void set_nick_mode(int server_index, int channel_index, const char *nick, irc_user_mode_t mode)
{
	int insert_index = 0, nick_found_at = -1;
	channel *cur_channel = &server_list[server_index].pchannels[channel_index];
	const char *search_nick = has_nick_mode(nick) ? nick + 1 : nick;

	search_for_nick(cur_channel, search_nick, &nick_found_at, &insert_index);

	if (nick_found_at != -1)
		(cur_channel -> persons)[nick_found_at].mode = mode;
}

void free_names_list(channel *pc)
{
	int name_index;

	for(name_index=0; name_index<pc -> n_names; name_index++)
		free_person(&pc -> persons[name_index]);

	myfree(pc -> persons);

	pc -> persons = NULL;
	pc -> n_names = 0;
}

char nick_mode_to_text(char nick_mode)
{
	if (nick_mode & MODE_OPERATOR)
		return '@';

	if (nick_mode & MODE_CAN_SPEAK)
		return '+';

	return ' ';
}

void show_name_list(int server_index, int channel_index, NEWWIN *channel_window)
{
	if (server_index == -1 || channel_index == -1)
		return;

	if (server_index < n_servers && server_list[server_index].n_channels > channel_index)
	{
		int name_index = 0;
		channel *cur_channel = &server_list[server_index].pchannels[channel_index];

		werase(channel_window -> win);

		for(name_index=names_offset; name_index<min(names_offset + channel_window -> nlines, cur_channel -> n_names); name_index++)
		{
			char nick_mode = (cur_channel -> persons)[name_index].mode;
			BOOL ignored   = (cur_channel -> persons)[name_index].ignored;
			char nick_char = ignored ? 'I' : nick_mode_to_text(nick_mode);

			if (name_index - names_offset == names_cursor) mywattron(channel_window -> win, A_REVERSE);

			limit_print(channel_window, channel_window -> ncols, name_index - names_offset, 0, "%c%s", nick_char, (cur_channel -> persons)[name_index].nick);

			if (name_index - names_offset == names_cursor) mywattroff(channel_window -> win, A_REVERSE);
		}
	}
}

BOOL has_nick_mode(const char *nick)
{
	if (nick[0] == '@')
		return TRUE;

	if (nick[0] == '+')
		return TRUE;

	return FALSE;
}

void go_to_last_name(void)
{
	int n_names = cur_channel() -> n_names;

	if (n_names >= channel_window -> nlines)
	{
		names_cursor = channel_window -> nlines - 1;
		names_offset = n_names - channel_window -> nlines;
	}
	else
	{
		names_cursor = n_names - 1;
		names_offset = 0;
	}
}

void do_names_keypress(int c)
{
	if (c == KEY_UP)
	{
		if (names_cursor > 0)
			names_cursor--;
		else if (names_offset > 0)
			names_offset--;
		else
			go_to_last_name();
	}
	else if (c == KEY_END)
	{
		go_to_last_name();
	}
	else if (c == KEY_PPAGE)
	{
		if (names_offset >= channel_window -> nlines)
		{
			names_cursor = 0;
			names_offset -= channel_window -> nlines;
		}
		else
		{
			names_cursor = 0;
			names_offset = 0;
		}
	}
	else if (c == KEY_DOWN)
	{
		if (names_offset + names_cursor < cur_channel() -> n_names - 1)
		{
			if (names_cursor < channel_window -> nlines - 1)
				names_cursor++;
			else
				names_offset++;
		}
		else
		{
			names_cursor = 0;
			names_offset = 0;
		}
	}
	else if (c == KEY_NPAGE)
	{
		if (names_offset + channel_window -> nlines < cur_channel() -> n_names)
		{
			names_cursor = 0;
			names_offset += channel_window -> nlines;
		}
		else
		{
			names_cursor = 0;
			names_offset = cur_channel() -> n_names - 1;
		}
	}
	else if (c == KEY_HOME)
	{
		names_cursor = 0;
		names_offset = 0;
	}
	else if (c == KEY_LEFT)	/* leave nameslist */
	{
		set_cursor_mode(CM_CHANNELS);
	}
	else if (c == KEY_RIGHT && names_offset + names_cursor < cur_channel() -> n_names)	/* enter user menu */
	{
		if (user_menu(current_server, current_server_channel_nr, names_offset + names_cursor) == -1)
		{
			cur_server() -> state = STATE_DISCONNECTED;
			close(cur_server() -> fd);
			update_statusline(current_server, current_server_channel_nr, "Connection to %s:%d closed (7)", cur_server() -> server_host, cur_server() -> server_port);
		}
	}
}

BOOL is_ignored(int sr, int ch, const char *nick)
{
	channel *pc = &server_list[sr].pchannels[ch];
	char *search_nick = strdup(has_nick_mode(nick) ? nick + 1 : nick);
	int insert_index = 0, nick_found_at = -1;

	terminate_str(search_nick, '!');

	search_for_nick(pc, search_nick, &nick_found_at, &insert_index);

	free(search_nick);

	if (nick_found_at == -1)
	{
		LOG("is_ignored: %s not found\n", nick);
		return FALSE;
	}

	return pc -> persons[nick_found_at].ignored;
}

BOOL ignore_nick(int sr, int ch, const char *nick, BOOL *was)
{
	channel *pc = &server_list[sr].pchannels[ch];
	char *search_nick = strdup(has_nick_mode(nick) ? nick + 1 : nick);
	int insert_index = 0, nick_found_at = -1;

	terminate_str(search_nick, '!');

	search_for_nick(pc, search_nick, &nick_found_at, &insert_index);

	free(search_nick);

	if (nick_found_at == -1)
		return FALSE;

	*was = pc -> persons[nick_found_at].ignored;

	pc -> persons[nick_found_at].ignored = TRUE;

	return TRUE;
}

BOOL unignore_nick(int sr, int ch, const char *nick, BOOL *was)
{
	channel *pc = &server_list[sr].pchannels[ch];
	char *search_nick = strdup(has_nick_mode(nick) ? nick + 1 : nick);
	int insert_index = 0, nick_found_at = -1;

	terminate_str(search_nick, '!');

	search_for_nick(pc, search_nick, &nick_found_at, &insert_index);

	free(search_nick);

	if (nick_found_at == -1)
		return FALSE;

	*was = pc -> persons[nick_found_at].ignored;

	pc -> persons[nick_found_at].ignored = FALSE;

	return TRUE;
}
