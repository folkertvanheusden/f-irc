/* GPLv2 applies
 * SVN revision: $Revision: 886 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ncursesw/panel.h>
#include <ncursesw/ncurses.h>

#include "gen.h"
#include "error.h"
#include "theme.h"
#include "term.h"
#include "buffer.h"
#include "channels.h"
#include "servers.h"
#include "utils.h"
#include "irc.h"
#include "loop.h"
#include "main.h"
#include "dcc.h"
#include "names.h"
#include "config.h"
#include "user.h"
#include "colors.h"
#include "ignores.h"
#include "dictionary.h"
#include "headlines.h"
#include "help.h"
#include "scrollback.h"
#include "script.h"

void keypress_visual_feedback(void)
{
	NEWWIN *popup = create_window(5, 25);

	box(popup -> win, 0, 0);
	mywattron(popup -> win, A_BOLD);
	mvwprintw(popup -> win, 0, 2, "Executing command");
	mywattroff(popup -> win, A_BOLD);

	escape_print_xy(popup, 2, 2, "Please wait...");

	mydoupdate();

	usleep(100000);

	delete_window(popup);

	mydoupdate();
}

void popup_notify(BOOL use_getch, const char *format, ...)
{
	va_list ap;
	char *msg = NULL;
	const char any_key[] = "Press any key...";
	int width = -1, loop = 0;
	string_array_t lines;
	NEWWIN *popup = NULL;

	init_string_array(&lines);

	va_start(ap, format);
	vasprintf(&msg, format, ap);
	va_end(ap);

	split_string(msg, "\n", FALSE, &lines);

	for(loop=0; loop<string_array_get_n(&lines); loop++)
	{
		int cur_width = strlen(string_array_get(&lines, loop));

		if (cur_width > width)
			width = cur_width;
	}

	if (sizeof(any_key) > width)
		width = sizeof(any_key);

	popup = create_window(5 + string_array_get_n(&lines) - 1, width + 4);

	box(popup -> win, 0, 0);
	mywattron(popup -> win, A_BOLD);
	mvwprintw(popup -> win, 0, 2, any_key);
	mywattroff(popup -> win, A_BOLD);

	escape_print_xy(popup, 2, 2, msg);

	mydoupdate();

	myfree(msg);

	if (use_getch)
		getch();
	else
		(void)wait_for_keypress(FALSE);

	delete_window(popup);

	free_string_array(&lines);

	mydoupdate();
}

BOOL onoff_box(const char *q, BOOL default_value)
{
	NEWWIN *bwin = NULL, *win = NULL;
	const char *title = " Space to toggle, tab key select OK/CANCEL, enter exit ";
	int q_len = max(strlen(title), strlen(q));
	BOOL cur_value = default_value;
	BOOL ret_ok = TRUE;

	create_win_border(q_len + 5, 5, title, &bwin, &win, FALSE);

	for(;;)
	{
		int c = -1;

		werase(win -> win);

		escape_print_xy(win, 1, 1, q);

		if (cur_value)
			escape_print_xy(win, 2, q_len / 2, "^ ON ^");
		else
			escape_print_xy(win, 2, q_len / 2, " OFF");

		if (ret_ok == TRUE)
			escape_print_xy(win, 3, 1, "^[ OK ]^ [ CANCEL ]");
		else
			escape_print_xy(win, 3, 1, "[ OK ] ^[ CANCEL ]^");

		mydoupdate();

		c = wait_for_keypress(FALSE);

		if (c == ' ')
			cur_value = !cur_value;
		else if (c == 7)
		{
			ret_ok = false;
			cur_value = default_value;
			break;
		}
		else if (c == 9)
			ret_ok = !ret_ok;
		else if (c == 13)
		{
			if (!ret_ok)
				cur_value = default_value;

			break;
		}
		else if (c == 3)
			exit_fi();
		else if (toupper(c) == 'q')
			break;
		else
			wrong_key();
	}

	delete_window(win);
	delete_window(bwin);

	mydoupdate();

	return cur_value;
}

void save_config_with_popup(void)
{
	char *err_msg = NULL;

	(void)save_config(TRUE, &err_msg);
	popup_notify(FALSE, "%s", err_msg);

	free(err_msg);
}

const char *edit_box(int width, const char *title, const char *initial)
{
	char *line = NULL;
	NEWWIN *bwin = NULL, *win = NULL;
	BOOL ret_ok = TRUE;
	int offset = 0, pos = 0;
	BOOL reposition = TRUE;
	char *undo_clear = NULL;

	width = min(max(width, 38), max_x);

	if (initial)
		line = strdup(initial);
	else
	{
		line = (char *)malloc(1);
		line[0] = 0x00;
	}

	create_win_border(width, 3, " TAB select OK/CANCEL, enter to exit ", &bwin, &win, TRUE);

	for(;;)
	{
		char *dummy = NULL;
		int c = -1, l = strlen(line);

		werase(win -> win);

		escape_print_xy(win, 0, 1, title);

		if (ret_ok == TRUE)
			escape_print_xy(win, 2, 1, "^[ OK ]^ [ CANCEL ]");
		else
			escape_print_xy(win, 2, 1, "[ OK ] ^[ CANCEL ]^");

		if (reposition)
		{
			if (l < width)
				pos = l;
			else
				pos = (width * 3) / 4;

			offset = l - pos;

			reposition = FALSE;
		}

		dummy = strdup(&line[offset]);
		if (strlen(dummy) >= width - 2)
			dummy[width - 2] = 0x00;
		mvwprintw(win -> win, 1, 1, "%s", dummy);
		free(dummy);

		override_cursor_win = win;
		override_cursor_x = pos + 1;
		override_cursor_y = 1;

		mydoupdate();

		c = wait_for_keypress(FALSE);

		if (c == KEY_BACKSPACE || c == 127)	/* backspace/del */
		{
			if (pos > 0)
				pos--;
			else if (offset > 0)
				offset--;
			else
				wrong_key();

			line[pos + offset] = 0x00;
		}
		else if (c == KEY_F(1))	/* F1 for help */
			edit_box_help();
		else if (c == 1)	/* ^A */
			pos = offset = 0;
		else if (c == 4)	/* ^D */
		{
			int x = pos + offset;

			if (x < l)
				memmove(&line[x], &line[x + 1], l - x);
			else
				wrong_key();
		}
		else if (c == 5)	/* ^E */
			reposition = TRUE;
		else if (c == 7)
		{
			ret_ok = false;
			break;
		}
		else if (c == 9)
			ret_ok = !ret_ok;
		else if (c == 3)
			exit_fi();
		else if (c == 13)	/* enter */
			break;
		else if (c == 21)	/* ^U */
		{
			if (l == 0)
			{
				free(line);
				line = undo_clear;
				undo_clear = NULL;
			}
			else
			{
				free(undo_clear);
				undo_clear = strdup(line);

				line[0] = 0x00;
			}

			reposition = TRUE;
		}
		else if (c == KEY_LEFT)
		{
			if (pos > 0)
				pos--;
			else if (offset > 0)
				offset--;
			else
				wrong_key();
		}
		else if (c == KEY_RIGHT)
		{
			int l = strlen(line);

			if (pos + offset < l)
			{
				if (pos < width - 1)
					pos++;
				else
					offset++;
			}
			else
			{
				wrong_key();
			}
		}
		else if (c == 23)	/* ^W */
		{
			int x = pos + offset;
			while(x > 0 && isalnum(line[x - 1]))
			{
				line[x - 1] = 0x00;
				x--;
			}
		}
		else if (c >= 32 && c < 127)
		{
			int l = strlen(line);
			int x = pos + offset;

			line = realloc(line, l + 1 + 1);

			if (x < l)
				memmove(&line[x + 1], &line[x], (l - x) + 1);

			line[x] = c;
			line[l + 1] = 0x00;

			if (pos < width - 1)
				pos++;
			else
				offset++;
		}
	}

	delete_window(win);
	delete_window(bwin);

	override_cursor_win = NULL;
	override_cursor_y = override_cursor_x = 0;

	mydoupdate();

	if (!ret_ok)
	{
		myfree(line);
		line = NULL;
	}

	return line;
}

int ping_user(int sr, const char *nick)
{
	popup_notify(FALSE, "Ping sent");

	server_list[sr].user_ping = strdup(nick);
	server_list[sr].t_user_ping = get_ts();
	server_list[sr].user_ping_id = rand();

	if (irc_ping(server_list[sr].fd, nick, server_list[sr].user_ping_id) != 0)
		return -1;

	return log_channel(sr, 0, nick, "send ping", TRUE);
}

int dcc_send_user(int sr, int ch, const char *nick)
{
	int rc = 0;
	const char *filename_in = edit_box(60, "^DCC send file^", NULL);

	if (filename_in)
	{
		char *log_msg = NULL;
		const char *filename = explode_path(filename_in);

		rc = init_send_dcc(filename, sr, ch, nick);

		asprintf(&log_msg, "DCC send %s", filename);

		if (rc == 0 && log_channel(sr, ch, nick, log_msg, TRUE) == -1)
			rc = -1;

		myfree(filename);

		myfree(log_msg);

		myfree(filename_in);
	}

	return rc;
}

int ctcp_user(int sr, const char *nick)
{
	int rc = 0;
	const char *ctcp_command = edit_box(60, "^CTCP^", NULL);

	if (ctcp_command)
	{
		char *msg = NULL, *log_msg = NULL;

		asprintf(&msg, "\001%s\001", ctcp_command);

		rc = irc_privmsg(server_list[sr].fd, nick, msg);

		myfree(msg);

		asprintf(&log_msg, "CTCP: %s", ctcp_command);

		myfree(ctcp_command);

		if (rc == 0 && log_channel(sr, 0, nick, log_msg, TRUE) == -1)
			rc = -1;

		myfree(log_msg);
	}

	return rc;
}

void cmd_LEAVE(int server_nr, int server_channel_nr, const char *channel_name)
{
	int ch = -1;

	/* see if there's a channel given */
	if (channel_name && strlen(channel_name) > 0)
	{
		int loop;

		for(loop=0; loop<server_list[server_nr].n_channels; loop++)
		{
			if (strcasecmp(server_list[server_nr].pchannels[loop].channel_name, channel_name) == 0)
			{
				ch = loop;
				break;
			}
		}

		if (ch == -1)
		{
			update_statusline(server_nr, server_channel_nr, "Not in channel %s", channel_name);
			wrong_key();
		}
	}
	else
	{
		/* server channel? */
		if (server_channel_nr == 0)
		{
			update_statusline(server_nr, server_channel_nr, "Cannot leave the server channel: use /QUIT to leave a server");
			wrong_key();
		}
		else
		{
			ch = server_channel_nr;
		}
	}

	if (ch != -1)
	{
		int cur_channel = server_channel_nr;

		update_statusline(server_nr, 0, "Channel %s closed", server_list[server_nr].pchannels[ch].channel_name);

		close_channel(server_nr, ch, TRUE);

		if (server_nr == current_server)
		{
			int dummy = 0;

			if (cur_channel >= server_list[server_nr].n_channels)
				cur_channel = server_list[server_nr].n_channels - 1;

			if (cur_channel < 0)
				cur_channel = 0;

			change_channel(server_nr, cur_channel, TRUE, FALSE, FALSE);

			dummy = find_vc_list_entry(server_nr, cur_channel);
			if (dummy == -1)
				dummy = 0;

                        channel_cursor = dummy % channel_window -> nlines;
                        channel_offset = dummy - channel_cursor;

			if (get_cursor_mode() == CM_NAMES)
				set_cursor_mode(CM_CHANNELS);
		}
	}

	show_channel_names_list();
}

int user_command(int current_server, int current_server_channel_nr, const char *user_line, BOOL do_command)
{
	int rc = 0;
	char me = strncasecmp(user_line, "/ME ", 4) == 0;

	if (user_line[0] == '/' && !me && do_command == TRUE) /* command for server? */
	{
		char *command = strdup(user_line + 1);
		char *pars = strchr(command, ' ');
		char *parsc = NULL;

		/* find start of parameters */
		if (pars)
		{
			*pars = 0x00;
			pars++;

			while(*pars == ' ')
				pars++;

			parsc = strdup(pars);
		}

		/* remove spaces at the end of the parameters */
		if (pars)
		{
			int pars_len = strlen(pars);

			while(pars_len)
			{
				pars_len--;

				if (isspace(pars[pars_len]))
					pars[pars_len] = 0x00;
				else
					break;
			}

			if (strlen(pars) == 0)
				pars = NULL;
		}

		/* first determine if it is a command for the irc-client or for the server */
		if (strcasecmp(command, "ADDSERVER") == 0)
		{
			if (!pars)
				popup_notify(FALSE, "/ADDSERVER: missing host(-name) to connect to");
			else
			{
				int sr = add_server(pars,			/* host[:port] */
						server_list[current_server].username,	/* username */
						NULL,					/* password */
						server_list[current_server].nickname,	/* nickname */
						server_list[current_server].user_complete_name,	/* complete name */
						NULL					/* description */
					  );

				update_statusline(current_server, current_server_channel_nr, "Added server %s:%d", server_list[sr].server_host, server_list[sr].server_port);

				change_channel(sr, 0, TRUE, TRUE, TRUE);
			}
		}
		else if (strcasecmp(command, "TIME") == 0)
		{
			server *ps = &server_list[current_server];

			ps -> hide_time_req = FALSE;

			/* only set when not already set: a ping may already have been pending */
			if (ps -> sent_time_req_ts < 1)
			{
				ps -> sent_time_req_ts = get_ts();

				if (irc_time(ps -> fd) == -1)
					rc = -1;
			}
		}
		else if (strcasecmp(command, "PING") == 0)
		{
			char do_it = 1;

			if (server_list[current_server].user_ping)
			{
				if (get_ts() - server_list[current_server].t_user_ping > 10.0)
				{
					update_statusline(current_server, current_server_channel_nr, "PING to user %s timed out", server_list[current_server].user_ping);
					myfree(server_list[current_server].user_ping);
					server_list[current_server].user_ping = NULL;
				}
				else
				{
					update_statusline(current_server, current_server_channel_nr, "A PING to user %s is still in progress", server_list[current_server].user_ping);
					do_it = 0;
				}
			}

			if (!pars)
				popup_notify(FALSE, "/PING: you forgot to tell me what user to ping");
			else if (do_it)
			{
				if (ping_user(current_server, pars) == -1)
					rc = -1;
			}
		}
		else if (strcasecmp(command, "IGNORE") == 0)
		{
			if (!pars)
				popup_notify(FALSE, "Ignore: nick name missing");
			else
			{
				channel *pch = &server_list[current_server].pchannels[current_server_channel_nr];
				BOOL prev_status = FALSE, ok = ignore_nick(current_server, current_server_channel_nr, pars, &prev_status);

				add_ignore(pch -> channel_name, pars, IGNORE_NOT_SET);

				if (!ok)
					popup_notify(FALSE, "IGNORE: user %s is not known", pars);
				else if (prev_status)
					popup_notify(FALSE, "IGNORE: already ignoring user %s!", pars);
				else
				{
					update_statusline(current_server, current_server_channel_nr, "ignoring user %s", pars);

					if (!save_ignore_list())
						popup_notify(FALSE, "Problem saving ignore-list file");
				}
			}
		}
		else if (strcasecmp(command, "UNIGNORE") == 0)
		{
			if (!pars)
				popup_notify(FALSE, "Unignore: nick name missing");
			else
			{
				channel *pch = &server_list[current_server].pchannels[current_server_channel_nr];
				BOOL prev_status = FALSE, ok = unignore_nick(current_server, current_server_channel_nr, pars, &prev_status);

				del_ignore(pch -> channel_name, pars);

				if (!ok)
					popup_notify(FALSE, "IGNORE: user %s is not known", pars);
				else if (!prev_status)
					popup_notify(FALSE, "IGNORE: was not ignoring user %s!", pars);
				else
				{
					update_statusline(current_server, current_server_channel_nr, "no longer ignoring user %s", pars);

					if (!save_ignore_list())
						popup_notify(FALSE, "Problem saving ignore-list file");
				}
			}
		}
		else if (strcasecmp(command, "DCCSEND") == 0)
		{
			char *end_of_nick = pars ? strchr(pars, ' ') : NULL;
			if (!end_of_nick)
				popup_notify(FALSE, "DCCSEND: either the nickname or the filename are missing");
			else
			{
				*end_of_nick = 0x00;

				/* verify nick */
				if (has_nick(current_server, current_server_channel_nr, pars) == FALSE)
					popup_notify(FALSE, "DCCSEND: nick '%s' is not known for this channel", pars);
				else
				{
					if (init_send_dcc(end_of_nick + 1, current_server, current_server_channel_nr, pars) != 0)
						popup_notify(FALSE, "DCCSEND: failed to start transmitting file");
					else
						update_statusline(current_server, current_server_channel_nr, "DCCSEND: waiting for user '%s' to acknowledge file '%s'", pars, end_of_nick + 1);
				}
			}
		}
		else if (strcasecmp(command, "BAN") == 0)
		{
			if (!pars)
			{
				rc = do_send(server_list[current_server].fd, "MODE %s +b", server_list[current_server].pchannels[current_server_channel_nr].channel_name);
			}
			else
			{
				int loop = 0;
				server *ps = &server_list[current_server];
				channel *pc = &ps -> pchannels[current_server_channel_nr];
				person_t *p = NULL;

				for(loop=0; loop<pc -> n_names; loop++)
				{
					if (strcasecmp((pc -> persons)[loop].nick, pars) == 0)
					{
						p = & (pc -> persons)[loop];
						break;
					}
				}

				if (p)
				{
					if (irc_ban(ps -> fd, pc -> channel_name, p -> user_host) == -1)
						rc = -1;
					else
						update_statusline(current_server, current_server_channel_nr, "user %s (%s / %s) banned", pars, p -> complete_name, p -> user_host);
				}
				else
				{
					popup_notify(FALSE, "BAN: user %s is not known", pars);
				}
			}
		}
		else if (strcasecmp(command, "RAW") == 0 || strcasecmp(command, "QUOTE") == 0)
		{
			if (do_send(server_list[current_server].fd, "%s", pars) == -1)
				rc = -1;
		}
		else if (strcasecmp(command, "LEAVE") == 0 || strcasecmp(command, "PART") == 0)
		{
			cmd_LEAVE(current_server, current_server_channel_nr, pars);
		}
		else if (strcasecmp(command, "TOPIC") == 0)
		{
			channel *pc = &server_list[current_server].pchannels[current_server_channel_nr];
			if (parsc == NULL || strlen(parsc) == 0)
			{
				char *topic = pc -> topic;

				update_statusline(current_server, current_server_channel_nr, "Topic for channel %s is: %s", pc -> channel_name, topic?topic: "not yet known.");
			}
			else if (irc_topic(server_list[current_server].fd, pc -> channel_name, parsc) == -1)
			{
				rc = -1;
			}
		}
		else if (strcasecmp(command, "KEEPTOPIC") == 0)
		{
			if (parsc == NULL || strlen(parsc) == 0)
			{
				myfree(server_list[current_server].pchannels[current_server_channel_nr].keeptopic);
				server_list[current_server].pchannels[current_server_channel_nr].keeptopic = NULL;

				update_statusline(current_server, current_server_channel_nr, "No longer maintaining topic on channel %s", server_list[current_server].pchannels[current_server_channel_nr].channel_name);
			}
			else
			{
				myfree(server_list[current_server].pchannels[current_server_channel_nr].keeptopic);
				server_list[current_server].pchannels[current_server_channel_nr].keeptopic = strdup(parsc);

				if (irc_topic(server_list[current_server].fd, server_list[current_server].pchannels[current_server_channel_nr].channel_name, parsc) == -1)
				{
					rc = -1;
				}
			}
		}
		else if (strcasecmp(command, "EXIT") == 0)	/* leave program */
		{
			exit_fi();
		}
		else if (strcasecmp(command, "QUIT") == 0)	/* leave server */
		{
			int sr = -1;

			/* see if there's a channel given */
			if (pars && strlen(pars) > 0)
			{
				int loop;

				/* find server into sr */
				for(loop=0; loop<n_servers; loop++)
				{
					if (strcasecmp(server_list[loop].server_host, pars) == 0)
					{
						sr = loop;
						break;
					}
				}
			}

			if (sr != -1)
			{
				if (n_servers == 1)
				{
					if (yesno_box(FALSE, "Terminate f-irc?", "Deleting the last/only server stops the program!", FALSE) == YES)
					{
						endwin();
						exit(0);
					}
				}
				else
				{
					close_server(sr, TRUE);
					free_server(sr);

					if (sr <= (n_servers - 1))
						memmove(&server_list[sr], &server_list[sr + 1], sizeof(server) * (n_servers - (sr + 1)));

					change_channel(0, 0, TRUE, FALSE, FALSE);

					n_servers--;
				}
			}
			else
			{
				if (pars == NULL || strlen(pars) == 0)
					popup_notify(FALSE, "/QUIT is to leave a server (/EXIT to terminate the program)\nso it requires a parameter: the server(-name) to leave.");
				else
					popup_notify(FALSE, "Cannot leave server '%s': is not known", pars);
			}
		}
		else if (strcasecmp(command, "SPAM") == 0)	/* pm spam */
		{
			server *ps = &server_list[current_server];
			channel *pc = &ps -> pchannels[current_server_channel_nr];
			int loop = 0;
			for(loop=0; loop<pc -> n_names; loop++)
			{
				if (irc_privmsg(ps -> fd, pc -> persons[loop].nick, pars) == -1)
					rc = -1;
			}
		}
		else if (strcasecmp(command, "VERSIONSPAM") == 0)	/* ask all CTCP version */
		{
			int loop = 0;
			server *ps = &server_list[current_server];
			channel *pc = &ps -> pchannels[current_server_channel_nr];

			for(loop=0; loop<pc -> n_names; loop++)
			{
				if (irc_privmsg(ps -> fd, pc -> persons[loop].nick, "\001VERSION\001") == -1)
					rc = -1;
			}
		}
		else if (strcasecmp(command, "MSG") == 0 || strcasecmp(command, "QUERY") == 0)	/* private message */
		{
			if (!pars)
				popup_notify(FALSE, "%s: missing nick/channel and message", command);
			else
			{
				char *end_of_nick = strchr(pars, ' ');
				if (!end_of_nick)
					popup_notify(FALSE, "%s: missing nick/channel or message", command);
				else
				{
					*end_of_nick = 0x00;

					if (auto_private_channel)
					{
						int channel_nr = create_channel(current_server, pars);

						if (channel_nr == -1)
							rc = -1;
						else if (log_channel(current_server, channel_nr, server_list[current_server].nickname, end_of_nick + 1, FALSE) == -1)
							rc = -1;
					}

					if (irc_privmsg(server_list[current_server].fd, pars, end_of_nick + 1) == -1)
					{
						rc = -1;
					}
				}
			}
		}
		else if (strcasecmp(command, "CTCP") == 0)	/* CTCP message */
		{
			char *end_of_nick = pars ? strchr(pars, ' ') : NULL;
			if (!end_of_nick)
				popup_notify(FALSE, "CTCP: either the nickname or the message is missing");
			else
			{
				*end_of_nick = 0x00;

				/* verify nick */
				if (has_nick(current_server, current_server_channel_nr, pars) == FALSE)
					popup_notify(FALSE, "CTCP: nick '%s' is not known for this channel", pars);
				else
				{
					char *msg = NULL;
					asprintf(&msg, "\001%s\001", end_of_nick + 1);

					if (irc_privmsg(server_list[current_server].fd, pars, msg) == -1)
						rc = -1;

					myfree(msg);
				}
			}
		}
		else if (strcasecmp(command, "SAVECONFIG") == 0)
		{
			save_config_with_popup();
		}
		else if (strcasecmp(command, "SEARCHALL") == 0)
			global_search(pars, pars);
		else
		{
			if (do_send(server_list[current_server].fd, "%s", user_line + 1) == -1)
				rc = -1;
		}

		myfree(command);
		myfree(parsc);
	}
	else			/* message to a certain channel */
	{
		server *ps = &server_list[current_server];
		channel *pc = &ps -> pchannels[current_server_channel_nr];
		char *line = NULL;
		char *new_line = NULL, *command = NULL;

		if (me)
			asprintf(&line, "\001ACTION %s\001", &user_line[4]);
		else
			line = strdup(user_line);

		process_scripts(ps, pc, ps -> nickname, line, FALSE, &new_line, &command);

		if (new_line != NULL && command != NULL)
		{
			if (strcasecmp(command, "REPLACE") == 0)
			{
				myfree(line);
				line = new_line;
				new_line = NULL;
			}
			else
			{
				myfree(new_line);
			}
		}

		myfree(command);
		myfree(new_line);

		if (irc_privmsg(ps -> fd, ps -> pchannels[current_server_channel_nr].channel_name, line) == -1)
			rc = -1;

		if (log_channel(current_server, current_server_channel_nr, ps -> nickname, line, FALSE) == -1)
			rc = -1;

		myfree(line);
	}

	return rc;
}

int user_menu(int sr, int ch, int name_index)
{
	int rc = 0;
	NEWWIN *bwin = NULL, *win = NULL;
	char *channel_name = gch(sr, ch) -> channel_name;
	const char *nick = NULL;
	char *title = NULL;

	if (name_index < 0 || name_index > gch(sr, ch) -> n_names)
	{
		delete_window(win);
		delete_window(bwin);
		mydoupdate();

		return 0;
	}

	nick = gch(sr, ch) -> persons[name_index].nick;

	if (gch(sr, ch) -> persons[name_index].complete_name == NULL)
		(void)irc_whois(server_list[sr].fd, nick);

	asprintf(&title, "%s [%s] (%s)", nick, channel_name, server_list[sr].server_host);

	create_win_border(60, 18, title, &bwin, &win, FALSE);

	free(title);

	escape_print_xy(win, 3, 2, "^k^ kick user, ^K^ kick with message");
	escape_print_xy(win, 4, 2, "^b^ ban nick,  ^B^ ban+kick nick");
	escape_print_xy(win, 5, 2, "^g^ ban user,  ^G^ ban+kick user");
	escape_print_xy(win, 6, 2, "^o^ op,        ^O^ de-op");
	escape_print_xy(win, 7, 2, "^w^ whois");
	escape_print_xy(win, 8, 2, "^v^ version");
	escape_print_xy(win, 9, 2, "^a^ allow speak");
	escape_print_xy(win, 10, 2, "^A^ disallow speak");
	escape_print_xy(win, 11, 2, "^p^ ping!");
	escape_print_xy(win, 12, 2, "^i^ ignore,   ^I^ un-ignore");
	escape_print_xy(win, 13, 2, "^D^ send a file using DCC");
	escape_print_xy(win, 14, 2, "^c^ CTCP");
	escape_print_xy(win, 15, 2, "^q^/^LEFT cursor key^ exit this menu");

	if (gch(sr, ch) -> persons[name_index].ignored)
		escape_print_xy(win, 16, 2, "currently ignoring user");

	for(;;)
	{
		int c = -1;

		const char *name_complete = gch(sr, ch) -> persons[name_index].complete_name;
		const char *user_host = gch(sr, ch) -> persons[name_index].user_host;

		mywattron(win -> win, A_BOLD);
		mvwprintw(win -> win, 1, 2, "%-44s", name_complete ? name_complete : "(name not yet known)");
		mvwprintw(win -> win, 2, 2, "%-44s", user_host ? user_host : "(user & host not yet known)");
		mywattroff(win -> win, A_BOLD);

		mydoupdate();

		c = wait_for_keypress(TRUE);

		if (c == KEY_LEFT || (c == KEY_MOUSE && right_mouse_button_clicked()))
			break;

		if (c <= 0)
			continue;

		if (c == 'q')
			break;
		else if (c == 7)
			break;
		else if (c == 3)
			exit_fi();
		else if (c == 'i')
		{
			gch(sr, ch) -> persons[name_index].ignored = TRUE;

			add_ignore(gch(sr, ch) -> channel_name, gch(sr, ch) -> persons[name_index].nick, gch(sr, ch) -> persons[name_index].user_host);

			if (!save_ignore_list())
				popup_notify(FALSE, "Problem saving ignore-list file");

			if (rc == 0 && log_channel(sr, ch, nick, "ignore", TRUE) == -1)
				rc = -1;
		}
		else if (c == 'I')
		{
			gch(sr, ch) -> persons[name_index].ignored = FALSE;

			del_ignore(gch(sr, ch) -> channel_name, gch(sr, ch) -> persons[name_index].nick);

			if (!save_ignore_list())
				popup_notify(FALSE, "Problem saving ignore-list file");

			if (rc == 0 && log_channel(sr, ch, nick, "unignore", TRUE) == -1)
				rc = -1;
		}
		else if (c == 'k' || c == 'K')
		{
			const char *msg = NULL;
			BOOL dokick = TRUE;

			if (c == 'K')
			{
				msg = edit_box(60, "^Kick message^", NULL);

				if (!msg)
					dokick = FALSE;
			}

			if (dokick)
			{
				rc = irc_kick(server_list[sr].fd, channel_name, nick, msg);

				if (rc == 0 && log_channel(sr, ch, nick, "kick", TRUE) == -1)
					rc = -1;
			}

			myfree(msg);

			break;
		}
		else if (c == 'b' || c == 'g')
		{
			const char *what = c == 'b' ? nick : user_host;

			rc = irc_ban(server_list[sr].fd, channel_name, what);

			if (rc == 0 && log_channel(sr, ch, what, "BAN", TRUE) == -1)
				rc = -1;
			break;
		}
		else if (c == 'B' || c == 'G')
		{
			const char *msg = edit_box(60, "^Kick+ban message^", NULL);

			if (msg)
			{
				const char *what = c == 'B' ? nick : user_host;

				rc = irc_ban(server_list[sr].fd, channel_name, what);

				if (rc == -1)
					update_statusline(sr, ch, "BAN failed: also not kicked\n");
				else
				{
					rc = irc_kick(server_list[sr].fd, channel_name, what, msg);

					if (rc == 0 && log_channel(sr, ch, what, "BAN + KICK", TRUE) == -1)
						rc = -1;
				}

				myfree(msg);
			}

			break;
		}
		else if (c == 'o')
		{
			rc = irc_op(server_list[sr].fd, channel_name, nick, TRUE);

			if (rc == 0 && log_channel(sr, ch, nick, "set OP", TRUE) == -1)
				rc = -1;
		}
		else if (c == 'O')
		{
			rc = irc_op(server_list[sr].fd, channel_name, nick, FALSE);

			if (rc == 0 && log_channel(sr, ch, nick, "remove OP", TRUE) == -1)
				rc = -1;
		}
		else if (c == 'w')
		{
			rc = irc_whois(server_list[sr].fd, nick);

			if (rc == 0 && log_channel(sr, 0, nick, "WHOIS", TRUE) == -1)
				rc = -1;
		}
		else if (c == 'v')
		{
			rc = irc_privmsg(server_list[sr].fd, nick, "\001VERSION\001");

			if (rc == 0 && log_channel(sr, 0, nick, "CTCP VERSION", TRUE) == -1)
				rc = -1;
		}
		else if (c == 'a')
		{
			popup_notify(FALSE, "Speak allowed");

			rc = irc_allowspeak(server_list[sr].fd, channel_name, nick, TRUE);

			if (rc == 0 && log_channel(sr, ch, nick, "allow speak", TRUE) == -1)
				rc = -1;
		}
		else if (c == 'A')
		{
			popup_notify(FALSE, "Speak disabled");

			rc = irc_allowspeak(server_list[sr].fd, channel_name, nick, FALSE);

			if (rc == 0 && log_channel(sr, ch, nick, "disallow speak", TRUE) == -1)
				rc = -1;
		}
		else if (c == 'p')
		{
			if (ping_user(sr, nick) == -1)
				rc = -1;
			else
				popup_notify(FALSE, "Ping sent");
		}
		else if (c == 'D')
		{
			if (dcc_send_user(sr, ch, nick) == -1)
				rc = -1;
		}
		else if (c == 'c')
		{
			if (ctcp_user(sr, nick) == -1)
				rc = -1;
		}
		else
		{
			wrong_key();
		}

		keypress_visual_feedback();
	}

	delete_window(win);
	delete_window(bwin);
	mydoupdate();

	return rc;
}

void refresh_window_with_buffer(NEWWIN *where, const int window_height, buffer *pbuffer, const char *hl, BOOL force_partial_highlight)
{
	int loop=0, n_elements = get_buffer_n_elements(pbuffer);

	werase(where -> win);

	for(loop=max(n_elements - window_height, 0); loop<n_elements; loop++)
	{
		buffer_element_t *pel = get_from_buffer(pbuffer, loop);
		nick_color_settings ncs;

		if (pel -> line_type == BET_MARKERLINE)
			gen_display_markerline(where, pel -> when);
		else
		{
			find_nick_colorpair(pel -> msg_from, &ncs);

			output_to_window(where, pel -> msg, hl, pel -> line_type, nick_color ? &ncs : NULL, force_partial_highlight, TRUE);
		}
	}

	pbuffer -> last_shown = n_elements;

	mydoupdate();
}

int select_server(void)
{
	NEWWIN *bwin = NULL, *win = NULL;
	char *title = NULL;
	int lines = 12, cols = 59;
	int offset = 0, woffset = 0, selection = -1;

	asprintf(&title, "Select server");

	create_win_border(63, 16, title, &bwin, &win, FALSE);

	free(title);

	for(;;)
	{
		int c = -1, index = 0;

		werase(win -> win);

		for(index=0; index<lines && index + offset < n_servers; index++)
		{
			char *buffer = (char *)malloc(cols + 1);
			char *temp = NULL;
			int si = index + offset;
			int clen = asprintf(&temp, "%s (%s:%d)", server_list[si].description, server_list[si].server_host, server_list[si].server_port);

			memset(buffer, ' ', cols);
			buffer[cols] = 0x00;

			memcpy(buffer, temp, clen);

			free(temp);

			if (index == woffset)
				mywattron(win -> win, A_REVERSE);

			mvwprintw(win -> win, 1 + index, 2, buffer);

			if (index == woffset)
				mywattroff(win -> win, A_REVERSE);

			free(buffer);
		}

		mydoupdate();

		c = wait_for_keypress(TRUE);

		if (c == 0)
			continue;

		if (c == 'q' || c == 'Q' || c == -1)
			break;
		else if (c == KEY_LEFT || (c == KEY_MOUSE && right_mouse_button_clicked()))
			break;
		else if (c == 7)
			break;

		if (c == KEY_UP)
		{
			if (woffset > 0)
				woffset--;
			else if (offset > 0)
				offset--;
			else
				wrong_key();
		}
		else if (c == KEY_DOWN)
		{
			if (woffset < lines - 1 && woffset + offset < n_servers - 1)
				woffset++;
			else if (offset + woffset < n_servers - 1)
				offset++;
			else
				wrong_key();
		}
		else if (c == 3)
		{
			if (yesno_box(FALSE, "Terminate f-irc", "Are you sure you want to terminate the program?", FALSE) == YES)
				exit_fi();
		}
		else if (c == ' ' || c == KEY_RIGHT || c == 13)
		{
			selection = offset + woffset;
			break;
		}
		else
		{
			wrong_key();
		}
	}

	delete_window(win);
	delete_window(bwin);

	mydoupdate();

	return selection;
}

void edit_highlight_matchers()
{
	if (edit_string_array(&extra_highlights, "EDIT \"highlight matchers\", left cursor to exit"))
		save_config_with_popup();
}

void edit_headline_matchers()
{
	if (edit_string_array(&matchers, "EDIT \"headline matchers\", left cursor to exit"))
		save_config_with_popup();
}

void edit_send_after_connect(int sr)
{
	char *title = NULL;

	asprintf(&title, "EDIT %s \"send after connect\", left cursor to exit", gsr(sr) -> server_host);

	edit_string_array(&gsr(sr) -> send_after_login, title);

	free(title);
}

void edit_auto_join(int sr)
{
	char *title = NULL;

	asprintf(&title, "EDIT %s \"auto-join\", left cursor to exit", gsr(sr) -> server_host);

	edit_string_array(&gsr(sr) -> auto_join, title);

	free(title);
}

void edit_favorites(void)
{
	int i = 0;
	string_array_t f;

	init_string_array(&f);

	for(i=0; i<n_favorite_channels; i++)
	{
		char *line = NULL;

		if (favorite_channels[i].server)
			asprintf(&line, "%s %s", favorite_channels[i].server, favorite_channels[i].channel);
		else
			asprintf(&line, "%s", favorite_channels[i].channel);

		add_to_string_array(&f, line);

		free(line);
	}

	edit_string_array(&f, "EDIT favorites");

	free_favorites();

	for(i=0; i<string_array_get_n(&f); i++)
	{
		int n = -1;
		string_array_t parts;
		init_string_array(&parts);

		split_string(string_array_get(&f, i), " ", TRUE, &parts);

		n = string_array_get_n(&parts);

		if (n == 2)
			add_favorite(string_array_get(&parts, 0), string_array_get(&parts, 1));
		else if (n == 1)
			add_favorite(NULL, string_array_get(&parts, 0));

		free_string_array(&parts);
	}

	favorite_channels_index = 0;

	free_string_array(&f);
}

BOOL edit_string_array(string_array_t *p, const char *title)
{
	BOOL changes = FALSE;
	NEWWIN *bwin = NULL, *win = NULL;
	int lines = 9, cols = 59;
	int offset = 0, woffset = 0;

	create_win_border(63, 16, title, &bwin, &win, FALSE);

	for(;;)
	{
		int c = -1, index = 0;

		werase(win -> win);

		escape_print_xy(win, 1, 2, "^a^ add  ^e^/^right cursor key^ edit  ^i^ insert");
		escape_print_xy(win, 2, 2, "^d^ del (the one under the cursor)  ^/^ search");

		for(index=0; index<lines && offset + index < string_array_get_n(p); index++)
		{
			char *buffer = (char *)malloc(cols + 1);
			int use_index = index + offset;
			int clen = strlen(string_array_get(p, use_index));

			memset(buffer, ' ', cols);
			buffer[cols] = 0x00;

			memcpy(buffer, string_array_get(p, use_index), clen);

			if (index == woffset)
				mywattron(win -> win, A_REVERSE);

			mvwprintw(win -> win, 4 + index, 2, buffer);

			if (index == woffset)
				mywattroff(win -> win, A_REVERSE);

			free(buffer);
		}

		mydoupdate();

		c = wait_for_keypress(TRUE);

		if (c == 0)
			continue;

		if (c == 'q' || c == 'Q' || c == -1)
			break;
		else if (c == KEY_LEFT || (c == KEY_MOUSE && right_mouse_button_clicked()))
			break;
		else if (c == 7)
			break;

		if (c == KEY_UP)
		{
			if (woffset > 0)
				woffset--;
			else if (offset > 0)
				offset--;
			else
				wrong_key();
		}
		else if (c == KEY_DOWN)
		{
			if (woffset < lines - 1 && woffset + offset < string_array_get_n(p) - 1)
				woffset++;
			else if (woffset + offset < string_array_get_n(p) - 1)
				offset++;
			else
				wrong_key();
		}
		else if (c == KEY_PPAGE)
		{
			if (offset >= lines)
				offset -= lines;
			else if (woffset)
				woffset = 0;
			else
				wrong_key();
		}
		else if (c == KEY_NPAGE)
		{
			if (woffset + offset + lines < string_array_get_n(p) - 1)
				offset += lines;
			else if (woffset + offset < string_array_get_n(p) - 1)
			{
				woffset = 0;
				offset = string_array_get_n(p) - 1;
			}
			else
			{
				wrong_key();
			}
		}
		else if (c == 3)
		{
			if (yesno_box(FALSE, "Terminate f-irc", "Are you sure you want to terminate the program?", FALSE) == YES)
				exit_fi();
		}
		else if (c == 'd')
		{
			if (yesno_box(FALSE, "Delete", "Are you sure you want to delete this entry?", FALSE) == YES)
			{
				del_nr_from_string_array(p, offset + woffset);

				changes = TRUE;

				woffset = offset = 0;
			}
		}
		else if (c == '/')
		{
			const char *what = edit_box(60, "^Search...^", NULL);

			if (what != NULL && strlen(what) != 0)
			{
				int n = string_array_get_n(p), idx = -1, found_at = -1;

				for(idx=0; idx<n; idx++)
				{
					if (strstr(string_array_get(p, idx), what) == 0)
					{
						found_at = idx;
						break;
					}
				}

				if (found_at != -1)
				{
					woffset = found_at % lines;
					offset = found_at - woffset;
				}
			}

			myfree(what);
		}
		else if (c == 'a')
		{
			char *n = (char *)edit_box(60, "^Add^", NULL);

			if (n != NULL)
			{
				add_to_string_array(p, n);

				free(n);

				changes = TRUE;
			}
		}
		else if (c == 'i')
		{
			char *n = (char *)edit_box(60, "^Insert^", NULL);

			if (n != NULL)
			{
				insert_into_string_array(p, offset + woffset, n);

				free(n);

				changes = TRUE;
			}
		}
		else if (c == 'e' || c == KEY_RIGHT || c == 13)
		{
			char *n = (char *)edit_box(60, "^Edit^", string_array_get(p, offset + woffset));

			if (n != NULL)
			{
				replace_in_string_array(p, offset + woffset, n);

				free(n);

				changes = TRUE;
			}
		}
		else
		{
			wrong_key();
		}
	}

	delete_window(win);
	delete_window(bwin);

	mydoupdate();

	return changes;
}

void edit_server(int sr)
{
	NEWWIN *bwin = NULL, *win = NULL;
	char *title = NULL;
	BOOL restart_required = FALSE;

	asprintf(&title, "EDIT %s:%d, left cursor key to exit", gsr(sr) -> server_host, gsr(sr) -> server_port);

	create_win_border(63, 16, title, &bwin, &win, FALSE);

	free(title);

	for(;;)
	{
		int c = -1;

		werase(win -> win);

		escape_print_xy(win, 1, 2, "^d^ description");
		escape_print_xy(win, 2, 2, "^h^ host");
		escape_print_xy(win, 3, 2, "^p^ port");
		escape_print_xy(win, 4, 2, "^U^ complete name");
		escape_print_xy(win, 5, 2, "^u^ username");
		escape_print_xy(win, 6, 2, "^P^ password");
		escape_print_xy(win, 7, 2, "^n^ nickname");
		escape_print_xy(win, 8, 2, "^S^ edit \"send after connect\"");
		escape_print_xy(win, 9, 2, "^A^ edit \"auto-join\"");

		mydoupdate();

		c = wait_for_keypress(TRUE);

		if (c == 0)
			continue;

		if (c == 'q' || c == 'Q' || c == -1)
			break;
		if (c == KEY_LEFT || (c == KEY_MOUSE && right_mouse_button_clicked()))
			break;
		else if (c == 7)
			break;

		if (c == 3)
		{
			if (yesno_box(FALSE, "Terminate f-irc", "Are you sure you want to terminate the program?", FALSE) == YES)
				exit_fi();
		}
		else if (c == 'd')
		{
			const char *n = edit_box(60, "^Edit server description^", gsr(sr) -> description);

			if (n != NULL)
			{
				myfree(gsr(sr) -> description);
				gsr(sr) -> description = n;
			}
		}
		else if (c == 'h')
		{
			const char *n = edit_box(60, "^Edit server host^", gsr(sr) -> server_host);

			if (n != NULL)
			{
				myfree(gsr(sr) -> server_host);
				gsr(sr) -> server_host = n;
			}

			restart_required = TRUE;
		}
		else if (c == 'p')
		{
			char *port_str = NULL;
			const char *n = NULL;

			asprintf(&port_str, "%d", gsr(sr) -> server_port);
			n = edit_box(60, "^Edit server port^", port_str);
			free(port_str);

			if (n != NULL)
				gsr(sr) -> server_port = atoi(n);

			restart_required = TRUE;
		}
		else if (c == 'U')
		{
			char *n = (char *)edit_box(60, "^Edit \"complete name\"^", gsr(sr) -> user_complete_name);

			if (n != NULL)
			{
				free(gsr(sr) -> user_complete_name);
				gsr(sr) -> user_complete_name = n;
			}

			restart_required = TRUE;
		}
		else if (c == 'u')
		{
			char *n = (char *)edit_box(60, "^Edit user name^", gsr(sr) -> username);

			if (n != NULL)
			{
				free(gsr(sr) -> username);
				gsr(sr) -> username = n;
			}

			restart_required = TRUE;
		}
		else if (c == 'P')
		{
			char *n = (char *)edit_box(60, "^Edit password^", gsr(sr) -> password);

			if (n != NULL)
			{
				free(gsr(sr) -> password);
				gsr(sr) -> password = n;
			}

			restart_required = TRUE;
		}
		else if (c == 'n')
		{
			char *n = (char *)edit_box(60, "^Edit nick^", gsr(sr) -> nickname);

			if (n != NULL)
			{
				free(gsr(sr) -> nickname);
				gsr(sr) -> nickname = n;
			}

			restart_required = TRUE;
		}
		else if (c == 'S')
			edit_send_after_connect(sr);
		else if (c == 'A')
			edit_auto_join(sr);
		else
		{
			wrong_key();
		}
	}

	if (restart_required)
		popup_notify(FALSE, "The changes you made require you to\n\"restart the irc-connection\".\nTo do so, press 'r' in the server menu.");

	delete_window(win);
	delete_window(bwin);

	mydoupdate();
}

void server_menu(int sr)
{
	NEWWIN *bwin = NULL, *win = NULL;
	char *title = NULL;

	asprintf(&title, "%s [%s]", gsr(sr) -> description, gsr(sr) -> server_host);

	create_win_border(63, 16, title, &bwin, &win, FALSE);

	free(title);

	for(;;)
	{
		char *ss = NULL;
		int c = -1;
		char *ch_list_compl = NULL;
		char *addr = NULL, *addr_real = NULL, *bps_str = NULL;
		time_t now = time(NULL);
		double bps = 0.0;
		int t_diff = now - gsr(sr) -> ts_bytes;

		werase(win -> win);

		asprintf(&addr, "address: ^[%s]:%d^", gsr(sr) -> server_host, gsr(sr) -> server_port);
		escape_print_xy(win, 3, 2, addr);
		free(addr);

		asprintf(&addr_real, "connected to: ^%s^", gsr(sr) -> server_real);
		escape_print_xy(win, 4, 2, addr_real);
		free(addr_real);

		bps = gsr(sr) -> prev_bps;

		if (t_diff)
			bps = (bps + gsr(sr) -> bytes / (double)t_diff) / 2;

		asprintf(&bps_str, "BPS: %.2f  latency: %fs", bps, gsr(sr) -> server_latency);

		escape_print_xy(win, 5, 2, bps_str);
		free(bps_str);

		escape_print_xy(win, 7, 2, "^r^ reconnect");
		escape_print_xy(win, 8, 2, "^p^ disconnect & remove");
		escape_print_xy(win, 9, 2, "^g^ (re-)retrieve channel list (/LIST)");
		escape_print_xy(win, 10, 2, "^j^ join a channel");
		escape_print_xy(win, 11, 2, "^e^ edit server settings");
		escape_print_xy(win, 14, 2, "^q^/^LEFT cursor key^ exit this menu");

		if (gsr(sr) -> channel_list_complete == TRUE)
			asprintf(&ch_list_compl, "List of channels available (%d known)", gsr(sr) -> channel_list_n);
		else
			asprintf(&ch_list_compl, "List of channels NOT YET available (received: %d)", gsr(sr) -> channel_list_n);

		escape_print_xy(win, 1, 2, ch_list_compl);

		free(ch_list_compl);

		switch(gsr(sr) -> state)
		{
			case STATE_NO_CONNECTION:
				ss = "Server state: no connection, will connect";
				break;
			case STATE_ERROR:
				ss = "Server state: connect error";
				break;
			case STATE_TCP_CONNECT:
				ss = "Server state: connecting (TCP)";
				break;
			case STATE_IRC_CONNECTING:
				ss = "Server state: connecting (IRC)";
				break;
			case STATE_CONNECTED1:
				ss = "Server state: connecting (IRC handshake phase 1)";
				break;
			case STATE_CONNECTED2:
				ss = "Server state: connecting (IRC handshake phase 2)";
				break;
			case STATE_LOGGING_IN:
				ss = "Server state: connecting (logging in)";
				break;
			case STATE_RUNNING:
				ss = "Server state: connected!";
				break;
			case STATE_DISCONNECTED:
				ss = "Server state: disconnected";
				break;
			default:
				ss = "Server state: UNKNOWN!";
				break;
		}

		escape_print_xy(win, 2, 2, ss);

		mydoupdate();

		c = wait_for_keypress(TRUE);

		if (c == 0)
			continue;

		if (c == 'q' || c == 'Q' || c == -1)
			break;
		else if (c == KEY_LEFT || (c == KEY_MOUSE && right_mouse_button_clicked()))
			break;
		else if (c == 7)
			break;

		if (c == 3)
			exit_fi();
		else if (c ==  'p')
		{
			if (n_servers > 1)
			{
				if (yesno_box(FALSE, "Remove server", "Are you sure?", FALSE) == YES)
				{
					close_server(sr, TRUE);
					free_server(sr);

					if (sr <= n_servers - 1)
						memmove(&server_list[sr], &server_list[sr + 1], sizeof(server) * (n_servers - (sr + 1)));

					n_servers--;

					change_channel(0, 0, TRUE, FALSE, FALSE);
					show_channel_names_list();

					popup_notify(FALSE, "Server removed");
				}
			}
			else
			{
				popup_notify(FALSE, "Cannot remove server: need at least 1.");
			}

			break;
		}
		else if (c == 'r')
		{
			server_list[sr].reconnect_delay = DEFAULT_RECONNECT_DELAY;

			restart_server(sr);

			popup_notify(FALSE, "Restarting connection");
		}
		else if (c == 'g')
		{
			if (irc_list(gsr(sr) -> fd) == -1)
			{
				popup_notify(FALSE, "Disconnected from server");
				break;
			}
			else
			{
				popup_notify(FALSE, "Requested channel list");
			}
		}
		else if (c == 'e')
		{
			edit_server(sr);
		}
		else if (c == 'j')
		{
			if (gsr(sr) -> channel_list_complete == FALSE)
				popup_notify(FALSE, "Please wait for the channel list to load");
			else
			{
				buffer *temp = create_buffer(server_list[sr].channel_list_n);
				int loop, csr = -1, cch = -1;
				char *dummy = NULL;
				BOOL err = FALSE;
				NEWWIN *bwin = NULL, *win = NULL;

				create_win_border(max_x - 6, max_y - 4, "right cursor key: select, left: cancel", &bwin, &win, FALSE);

				for(loop=0; loop<server_list[sr].channel_list_n; loop++)
				{
					char *line = NULL;

					asprintf(&line, "%-12s %s", gsr(sr) -> channel_list[loop].channel, gsr(sr) -> channel_list[loop].topic);

					add_to_buffer(temp, line, NULL, FALSE, sr, loop);

					free(line);
				}

				if (scrollback_and_select(win, temp, &dummy, &csr, &cch, server_list[sr].nickname, FALSE, FALSE))
				{
					if (irc_join(server_list[sr].fd, gsr(sr) -> channel_list[cch].channel))
						err = TRUE;
				}

				delete_window(win);
				delete_window(bwin);

				mydoupdate();

				myfree(dummy);

				free_buffer(temp);

				if (err)
					break;
			}
		}
	}

	delete_window(win);
	delete_window(bwin);

	mydoupdate();
}

void add_server_menu(void)
{
	const char *nick = NULL, *s = NULL;

	s = edit_box(60, "^Add server: enter hostname:port^", NULL);

	if (s)
		nick = edit_box(60, "^Add server: enter nick^", NULL);

	if (nick)
	{
		add_server(s, nick, "notset", nick, nick, s);

		popup_notify(FALSE, "Server %s was added", s);
	}

	myfree(s);
	myfree(nick);
}

int user_channel_menu(int sr, const char *nick)
{
	int rc = 0;
	NEWWIN *bwin = NULL, *win = NULL;
	char *title = NULL;

	asprintf(&title, "%s (%s)", nick, server_list[sr].server_host);

	create_win_border(40, 9, title, &bwin, &win, TRUE);

	free(title);

	escape_print_xy(win, 1, 2, "^w^ whois");
	escape_print_xy(win, 2, 2, "^v^ version");
	escape_print_xy(win, 3, 2, "^p^ ping!");
	escape_print_xy(win, 4, 2, "^D^ send a file using DCC");
	escape_print_xy(win, 5, 2, "^c^ CTCP");
	escape_print_xy(win, 6, 2, "^q^/^LEFT cursor key^ exit this menu");

	mydoupdate();

	for(;;)
	{
		int c = wait_for_keypress(FALSE);

		if (c == KEY_LEFT || (c == KEY_MOUSE && right_mouse_button_clicked()))
			break;

		if (c == 'q')
			break;
		else if (c == 7)
			break;
		else if (c == 3)
			exit_fi();
		else if (c == KEY_F(1))
			user_channel_menu_help();
		else if (c == 'w')
		{
			rc = irc_whois(server_list[sr].fd, nick);

			if (rc == 0 && log_channel(sr, 0, nick, "CTCP WHOIS", TRUE) == -1)
				rc = -1;
			break;
		}
		else if (c == 'v')
		{
			rc = irc_privmsg(server_list[sr].fd, nick, "\001VERSION\001");

			if (rc == 0 && log_channel(sr, 0, nick, "CTCP VERSION", TRUE) == -1)
				rc = -1;
			break;
		}
		else if (c == 'p')
		{
			if (ping_user(sr, nick) == -1)
				rc = -1;
			else
				popup_notify(FALSE, "Ping sent");
		}
		else if (c == 'D')
		{
			if (dcc_send_user(sr, 0, nick) == -1)
				rc = -1;
		}
		else if (c == 'c')
		{
			if (ctcp_user(sr, nick) == -1)
				rc = -1;
		}
		else
			wrong_key();
	}

	delete_window(win);
	delete_window(bwin);
	mydoupdate();

	return rc;
}

/* use_getch: used when no valid configuration was loaded */
yna_reply_t yesno_box(BOOL use_getch, const char *title, const char *q, BOOL allow_abort)
{
	NEWWIN *bwin = NULL, *win = NULL;
	int q_len = max(strlen(title), strlen(q));
	yna_reply_t rc = NO;

	create_win_border(q_len + 5, 4, title, &bwin, &win, FALSE);

	for(;;)
	{
		int c = -1;

		werase(win -> win);

		escape_print_xy(win, 1, 1, q);

		if (allow_abort)
			escape_print_xy(win, 2, 1, "^y^es / ^n^o / ^a^bort");
		else
			escape_print_xy(win, 2, 1, "^y^es / ^n^o");

		mydoupdate();

		if (use_getch)
			c = getch();
		else
			c = wait_for_keypress(FALSE);

		if (c == 3 || c == 'n' || c == 'N')
		{
			rc = NO;
			break;
		}

		if (c == 'y' || c == 'Y')
		{
			rc = YES;
			break;
		}

		if ((c == 7 || c == 'a' || c == 'A') && allow_abort)
		{
			rc = ABORT;
			break;
		}

		wrong_key();
	}

	delete_window(win);
	delete_window(bwin);

	mydoupdate();

	return rc;
}

void put_field(NEWWIN *win, const char *what, int y, int x, short fg, short bg)
{
	short dummy = get_color_ncurses(fg, bg);

	color_on(win, dummy);

	mvwprintw(win -> win, y, x, what);

	color_off(win, dummy);
}

void put_colors(NEWWIN *win, int y, int x)
{
	put_field(win, "BLACK  ", y + 0, x, COLOR_WHITE, COLOR_BLACK);
	put_field(win, "RED    ", y + 1, x, COLOR_BLACK, COLOR_RED);
	put_field(win, "GREEN  ", y + 2, x, COLOR_BLACK, COLOR_GREEN);
	put_field(win, "YELLOW ", y + 3, x, COLOR_BLACK, COLOR_YELLOW);
	put_field(win, "BLUE   ", y + 4, x, COLOR_BLACK, COLOR_BLUE);
	put_field(win, "MAGENTA", y + 5, x, COLOR_BLACK, COLOR_MAGENTA);
	put_field(win, "CYAN   ", y + 6, x, COLOR_BLACK, COLOR_CYAN);
	put_field(win, "WHITE  ", y + 7, x, COLOR_BLACK, COLOR_WHITE);
	put_field(win, "default", y + 8, x, -1, -1);
}

void put_cursor(NEWWIN *win, int y, int x, BOOL selected)
{
	mywattron(win -> win, A_REVERSE);

	if (selected)
		mvwprintw(win -> win, y, x, ">");
	else
		mvwprintw(win -> win, y, x, " ");

	mywattroff(win -> win, A_REVERSE);
}

int choose_colorpair(const int default_pair)
{
	int color = default_pair;
	NEWWIN *bwin = NULL, *win = NULL;
	BOOL ret_ok = TRUE, update_buttons = TRUE;
	short fg = 0, bg = 0;
	int column = 0;

	pair_content(default_pair, &fg, &bg);

	if (fg == -1)
		fg = 8;
	if (bg == -1)
		bg = 8;

	create_win_border(50, 15, " Navigate with cursor keys, tab+enter to exit ", &bwin, &win, FALSE);

	werase(win -> win);

	mywattron(win -> win, A_UNDERLINE);
	mvwprintw(win -> win, 1, 10, "foreground");
	mvwprintw(win -> win, 1, 26, "background");
	mywattroff(win -> win, A_UNDERLINE);

	put_colors(win, 2, 10);
	put_colors(win, 2, 26);

	for(;;)
	{
		int c = -1, old_fg = fg, old_bg = bg;

		put_cursor(win, 2 + fg, 8, column == 0);
		put_cursor(win, 2 + bg, 24, column == 1);

		if (update_buttons)
		{
			if (ret_ok == TRUE)
				escape_print_xy(win, 13, 1, "^[ OK ]^ [ CANCEL ]");
			else
				escape_print_xy(win, 13, 1, "[ OK ] ^[ CANCEL ]^");

			update_buttons = FALSE;
		}

		c = wait_for_keypress(FALSE);

		if (c == 9)
		{
			ret_ok = !ret_ok;
			update_buttons = TRUE;
		}
		else if (c == 3 || c == 7)
		{
			color = default_pair;
			break;
		}
		else if (c == 13)
		{
			if (!ret_ok)
			{
				color = default_pair;
				break;
			}

			if (fg == 8)
				fg = -1;
			if (bg == 8)
				bg = -1;

			color = get_color_ncurses(fg, bg);

			if (fg == bg && fg != -1)
			{
				if (yesno_box(FALSE, "Use selected colors?", "Foreground and background colors are the same: are you sure?", FALSE) == YES)
					break;
			}
			else
			{
				break;
			}
		}
		else if (c == KEY_LEFT)
		{
			if (column > 0)
				column--;
			else
				wrong_key();
		}
		else if (c == KEY_RIGHT)
		{
			if (column < 1)
				column++;
			else
				wrong_key();
		}
		else if (c == KEY_UP)
		{
			if (column == 0)
			{
				if (fg)
					fg--;
				else
					wrong_key();
			}
			else
			{
				if (bg)
					bg--;
				else
					wrong_key();
			}
		}
		else if (c == KEY_DOWN)
		{
			if (column == 0)
			{
				if (fg < 8)
					fg++;
				else
					wrong_key();
			}
			else
			{
				if (bg < 8)
					bg++;
				else
					wrong_key();
			}
		}
		else
		{
			wrong_key();
		}

		if (fg != old_fg)
			mvwprintw(win -> win, 2 + old_fg, 8, " ");

		if (bg != old_bg)
			mvwprintw(win -> win, 2 + old_bg, 24, " ");
	}

	delete_window(win);
	delete_window(bwin);

	mydoupdate();

	return color;
}

BOOL configure_firc(void)
{
	BOOL force_redraw = TRUE;
	NEWWIN *bwin = NULL, *win = NULL;
	int cursor = 0, offset = 0;
	BOOL redraw = TRUE;
	const int win_height = 21, win_width = 68, content_offset = 6, content_height = win_height - (content_offset + 1);
	int cnt = 0, cnf_len = 0, val_pos = 50;
	const char title[] = " Configure f-irc, key right to select, key left to exit ";
	const char *find = NULL;

	for(cnt=0; cnf_pars[cnt].p != NULL; cnt++)
	{
		int l = strlen(cnf_pars[cnt].name);

		if (l > cnf_len)
			cnf_len = l;
	}

	if (cnf_len + 2 < val_pos)
		val_pos = cnf_len + 2;

	create_win_border(win_width, win_height, title, &bwin, &win, TRUE);

	for(;;)
	{
		int prev_pos = offset + cursor;
		int c = -1;

		if (redraw)
		{
			int loop = 0, max_rows = cnt - offset;

			werase(win -> win);

			escape_print_xy(win, 1, 1, "^F2^: store configuration on disk");
			escape_print_xy(win, 2, 1, "^a^: add server  ^s^: edit server(s)");
			escape_print_xy(win, 3, 1, "^l^ edit favorites list  ^d^ edit dictionary");
			escape_print_xy(win, 4, 1, "^m^: edit highlight matchers  ^h^ edit headline matchers");

			for(loop=0; loop<min(max_rows, content_height); loop++)
			{
				int pos = offset + loop, sp = 0;
				char *str_out = (char *)calloc(1, win_width - 2 + 1);
				BOOL *pb = NULL;
				int *pi = NULL;
				char *pc = NULL;
				short cfg = -1, cbg = -1;

				snprintf(&str_out[0], val_pos - 2, "%s", cnf_pars[pos].name);

				switch(cnf_pars[pos].type)
				{
					case CNF_BOOL:
						pb = (BOOL *)cnf_pars[pos].p;

						if (*pb)
							sprintf(&str_out[val_pos], "ON");
						else
							sprintf(&str_out[val_pos], "OFF");
						break;

					case CNF_VALUE:
						pi = (int *)cnf_pars[pos].p;

						sprintf(&str_out[val_pos], "%d", *pi);
						break;

					case CNF_STRING:
						pc = *(char **)cnf_pars[pos].p;

						snprintf(&str_out[val_pos], (win_width - 2) - val_pos, "%s", str_or_nothing(pc));
						break;

					case CNF_COLOR:
						pi = (int *)cnf_pars[pos].p;

						pair_content(*pi, &cfg, &cbg);

						sprintf(&str_out[val_pos], "%s,%s", color_to_str(cfg), color_to_str(cbg));
						break;
				}

				str_out[win_width - 2] = 0x00;

				for(sp=0; sp<win_width - 2; sp++)
				{
					if (str_out[sp] == 0x00)
						str_out[sp] = ' ';
				}

				if (loop == cursor)
					mywattron(win -> win, A_REVERSE);

				mvwprintw(win -> win, content_offset + loop, 1, "%s", str_out);

				if (loop == cursor)
					mywattroff(win -> win, A_REVERSE);

				free(str_out);
			}

			mydoupdate();

			redraw = FALSE;
		}

		c = wait_for_keypress(FALSE);

		if (c == KEY_LEFT || c == 3 || toupper(c) == 'Q')
			break;
		else if (c == KEY_LEFT || (c == KEY_MOUSE && right_mouse_button_clicked()))
			break;
		else if (c == 7)
			break;

		if (c == KEY_F(1))
			configure_firc_help();
		else if (c == KEY_F(2))
			save_config_with_popup();
		else if (c == 'a')
			add_server_menu();
		else if (c == 's')
		{
			for(;;)
			{
				int sr = select_server();

				if (sr == -1)
					break;

				edit_server(sr);
			}
		}
		else if (c == 'l')
		{
                        char *err_msg = NULL;

			edit_favorites();

                        if (save_config(TRUE, &err_msg) == FALSE)
			{
				popup_notify(FALSE, "%s", err_msg);

				free(err_msg);
			}
		}
		else if (c == 'd')
			edit_dictionary();
		else if (c == 'h')
			edit_headline_matchers();
		else if (c == 'm')
			edit_highlight_matchers();
		else if (c == KEY_RIGHT || c == 13 || c == ' ')
		{
			int pos = offset + cursor;
			BOOL *pb = NULL;
			int *pi = NULL;
			char *pi_str = NULL;
			char *pc = NULL;
			const char *new_str = NULL;
			char *q = NULL;

			asprintf(&q, "%s:", cnf_pars[pos].name);

			q[0] = toupper(q[0]);

			switch(cnf_pars[pos].type)
			{
				case CNF_BOOL:
					pb = (BOOL *)cnf_pars[pos].p;
					*pb = onoff_box(q, *pb);
					break;

				case CNF_VALUE:
					pi = (int *)cnf_pars[pos].p;
					asprintf(&pi_str, "%d", *pi);

					new_str = edit_box(val_pos, q, pi_str);

					if (new_str)
					{
						*pi= atoi(new_str);
						myfree(new_str);
					}

					free(pi_str);
					break;

				case CNF_STRING:
					pc = *(char **)cnf_pars[pos].p;

					new_str = edit_box(val_pos, q, pc);

					if (new_str)
					{
						free(pc);

						*(char **)cnf_pars[pos].p = (char *)new_str;
					}

					break;

				case CNF_COLOR:
					pi = (int *)cnf_pars[pos].p;
					*pi = choose_colorpair(*pi);
					break;
			}

			free(q);

			redraw = TRUE;

			if (cnf_pars[pos].dofunc)
				(cnf_pars[pos].dofunc)();
		}
		else if (c == KEY_PPAGE)
		{
			if (offset >= content_height)
				offset -= content_height;
			else if (cursor)
				cursor = 0;
			else
				wrong_key();
		}
		else if (c == KEY_NPAGE)
		{
			if (cursor + offset + content_height < cnt - 1)
				offset += content_height;
			else if (cursor + offset < cnt - 1)
			{
				cursor = 0;
				offset = cnt - 1;
			}
			else
			{
				wrong_key();
			}
		}
		else if (c == KEY_UP)
		{
			if (cursor > 0)
				cursor--;
			else if (offset > 0)
				offset--;
			else
				wrong_key();
		}
		else if (c == KEY_DOWN)
		{
			if (cursor < content_height - 1 && cursor + offset < cnt - 1)
				cursor++;
			else if (cursor + offset < cnt - 1)
				offset++;
			else
				wrong_key();
		}
		else if (c == KEY_HOME)
		{
			cursor = offset = 0;
		}
		else if (c == KEY_END)
		{
			cursor = content_height - 1;
			offset = cnt - cursor - 1;
		}
		else if (c == '/' || c == 'n')
		{
			int pos = offset + cursor + 1, loop = 0, found = -1;

			if (c == '/')
			{
				const char *new_find = edit_box(40, "Search for...", find);

				if (new_find)
				{
					myfree(find);

					find = new_find;
				}
			}

			if (find)
			{
				for(loop=0; loop<cnt; loop++)
				{
					while(pos >= cnt)
						pos -= cnt;

					if (strcasestr(cnf_pars[pos].name, find))
					{
						found = pos;
						break;
					}

					pos++;
				}
			}

			if (found == -1)
				wrong_key();
			else
			{
				cursor = pos % content_height;
				offset = pos - cursor;
			}
		}
		else
		{
			wrong_key();
		}

		if (cursor + offset != prev_pos)
			redraw = TRUE;
	}

	delete_window(win);
	delete_window(bwin);

	mydoupdate();

	return force_redraw;
}

void close_notice_channels(void)
{
	if (yesno_box(FALSE, "Close all \"NOTICE\" channels", "Are you sure?", FALSE) == YES)
	{
		int sr = 0;
		NEWWIN *win = NULL, *bwin = NULL;

		create_win_border(30, 3, "Please wait", &bwin, &win, FALSE);
		mvwprintw(win -> win, 1, 1, "Closing \"NOTICE\" channels...");
		mydoupdate();

		for(sr = 0; sr < n_servers; sr++)
		{
			int ch = 0;
			server *ps = &server_list[sr];
			BOOL left = FALSE;

			do
			{
				left = FALSE;

				for(ch=1; ch<ps -> n_channels; ch++)
				{
					channel *pc = &ps -> pchannels[ch];

					if (is_channel(pc -> channel_name))
						continue;

					if (pc -> recvd_non_notice == FALSE)
					{
						cmd_LEAVE(sr, ch, NULL);
						left = TRUE;
						break;
					}
				}
			}
			while(left);
		}

		delete_window(win);
		delete_window(bwin);

		mydoupdate();
	}
}

void edit_dictionary(void)
{
	BOOL changes_pending = edit_string_array(&dictionary, "Edit dictionary");

	if (changes_pending)
	{
		sort_string_array(&dictionary);

		if (!dictionary_file && string_array_get_n(&dictionary) > 0)
			dictionary_file = explode_path("~/.firc.dictionary");

		if (dictionary_file)
			save_dictionary();
	}
}

void edit_scripts(void)
{
#if 0
	NEWWIN *bwin = NULL, *win = NULL;
	int cursor = 0, offset = 0;
	BOOL redraw = TRUE;
	const int win_height = 21, win_width = 68, content_offset = 5, content_height = win_height - (content_offset + 1);
	const char title[] = " Select scripts ";

	create_win_border(win_width, win_height, title, &bwin, &win, TRUE);

	for(;;)
	{
		int prev_pos = offset + cursor;
		int c = -1;

		if (redraw)
		{
			int loop = 0, max_rows = pc -> n_scripts - offset;
			channel *pc = cur_channel():

			werase(win -> win);

			escape_print_xy(win, 1, 1, "^F2^: store configuration on disk");
			escape_print_xy(win, 2, 1, "^a^: add script  ^s^: edit script");
			escape_print_xy(win, 3, 1, "^d^: delete script");

			for(loop=0; loop<min(max_rows, content_height); loop++)
			{
				int pos = offset + loop, sp = 0;

				if (loop == cursor)
					mywattron(win -> win, A_REVERSE);

				mvwprintw(win -> win, content_offset + loop, 1, "%s", pc -> scripts[pos].filename);

				if (loop == cursor)
					mywattroff(win -> win, A_REVERSE);

				free(str_out);
			}

			mydoupdate();

			redraw = FALSE;
		}

		c = wait_for_keypress(FALSE);

		if (c == KEY_LEFT || c == 3 || toupper(c) == 'Q')
			break;
		else if (c == 7)
			break;
		else if (c == KEY_F(1))
			edit_scripts_help();
		else if (c == KEY_F(2))
			save_config_with_popup();
// FIXME
		else
			wrong_key();
	}
#endif
}
