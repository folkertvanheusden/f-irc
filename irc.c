/* GPLv2 applies
 * SVN revision: $Revision: 858 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
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
#include "tcp.h"
#include "loop.h"
#include "irc.h"
#include "names.h"
#include "user.h"
#include "key_value.h"
#include "wordcloud.h"
#include "grep_filter.h"
#include "config.h"
#include "headlines.h"
#include "script.h"

BOOL notified_grep_filter_problem = FALSE;

BOOL is_channel(const char *name)
{
	return name[0] == '#' || name[0] == '&';
}

int do_send(int fd, const char *format, ...)
{
	/* not to way to do it but I can't find a way to "predict" how large
	 * a string will be
	 */
	char *str_buffer = NULL;
	int len = 0, rc = -1;
        va_list ap;

        va_start(ap, format);
	len = vasprintf(&str_buffer, format, ap);
        va_end(ap);

	str_buffer = realloc(str_buffer, len + 3);
	str_buffer[len + 0] = '\r';
	str_buffer[len + 1] = '\n';
	str_buffer[len + 2] = 0x00;

	LOG("OUT %d: %s\n", fd, str_buffer);

	len += 2; /* CR/LF */

	rc = WRITE(fd, str_buffer, len);
	if (rc != len)
	{
		LOG("send: %d, rc: %d\n", len, rc);
		myfree(str_buffer);
		return -1;
	}

	myfree(str_buffer);

	return 0;
}

int irc_nick(int fd, const char *nick)
{
	if (do_send(fd, "NICK %s", nick))
		return -1;

	return 0;
}

int irc_login1(server *pserver)
{
	/* password required? */
	if (pserver -> password && do_send(pserver -> fd, "PASS %s", pserver -> password) == -1)
		return -1;

	/* send nick */
	if (irc_nick(pserver -> fd, pserver -> nickname) == -1)
		return -1;

	return 0;
}

int irc_login2(server *pserver)
{
	/* logon */
	if (do_send(pserver -> fd, "USER %s %s %s :%s", pserver -> username, "-", pserver -> server_host, pserver -> user_complete_name) == -1)
		return -1;

	return 0;
}

int create_channel(int server_index, const char *channel)
{
	/* find channel */
	int channel_index = find_channel_index(server_index, channel);

	if (channel_index == -1)
		channel_index = add_channel(server_index, channel);

	return channel_index;
}

void headline_channel_msg(server *ps, channel *pc, const char *msg)
{
	char *headline = NULL;

	asprintf(&headline, "%s (%s): %s", pc -> channel_name, ps -> description ? ps -> description : ps -> server_host, msg);
	add_headline(FALSE, headline);

	free(headline);
}

void log_to_user_window(int sr, const char *nick, const char *what)
{
	int ch = create_channel(sr, nick);

	log_channel(sr, ch, nick, what, FALSE);
}

int process_server_do_line(int sr, const char *string)
{
	int rc = 0;
	time_t now = time(NULL);

	server *ps = &server_list[sr];

	const char *prefix = NULL;
	const char *cmd = NULL;
	int cmd_nr = -1;
	const char *pars = NULL;
	char *pars_before_colon = NULL;
	const char *pars_after_colon = NULL;
	string_array_t before_parts;

	const char *parse_offset = string;

	init_string_array(&before_parts);

	/* prefix */
	if (parse_offset[0] == ':')
	{
		char *space = NULL;

		prefix = strdup(parse_offset + 1);

		space = strchr(prefix, ' ');
		if (!space)
		{
			LOG("Line consists of only a prefix?\n");
			return -1;
		}

		*space = 0x00;

		if (strlen(prefix) == 0)
		{
			myfree(prefix);
			prefix = NULL;
		}

		parse_offset = strchr(parse_offset, ' ');
		while(parse_offset && isspace(*parse_offset))
			parse_offset++;
	}

	/* command */
	if (parse_offset && strlen(parse_offset))
	{
		cmd = strdup(parse_offset);

		cmd_nr = atoi(cmd);

		terminate_str((char *)cmd, ' ');

		parse_offset = strchr(parse_offset, ' ');
		while(parse_offset && isspace(*parse_offset))
			parse_offset++;
	}

	/* get parameter(s) */
	if (parse_offset && strlen(parse_offset))
	{
		if (parse_offset[0] == ':')
		{
			pars_after_colon = strdup(parse_offset + 1);
		}
		else
		{
			char *colon_search_before = NULL;

			pars_before_colon = strdup(parse_offset);

			colon_search_before = strstr(pars_before_colon, " :");

			if (colon_search_before)
			{
				const char *colon_search_after = strstr(parse_offset, " :");

				*colon_search_before = 0x00;

				if (colon_search_after)
					pars_after_colon = strdup(colon_search_after + 2);
			}

			split_string(pars_before_colon, " ", TRUE, &before_parts);

			/* also keep track of nick used in reply-messages: it may be different
			 * from what we think is our nick in case you use e.g. znc
			 */
			if (cmd_nr > 1 && string_array_get_n(&before_parts) >= 1)
				server_set_additional_nick(sr, string_array_get(&before_parts, 0));
		}
	}

	LOG("%s %d> %s|%s|%s :%s\n", ps -> description ? ps -> description : ps -> server_host, ps -> fd, str_or_nothing(prefix), str_or_nothing(cmd), str_or_nothing(pars_before_colon), str_or_nothing(pars_after_colon));

	/* do commands */
	if (!cmd)
	{
		LOG("line without command from irc server\n");
	}
	else if (strcmp(cmd, "PING") == 0)			/* PING/PONG */
	{
		if (do_send(ps -> fd, "PONG %s", pars_after_colon) == -1)
			rc = -1;
	}
	else if (strcmp(cmd, "TOPIC") == 0)
	{
		if (string_array_get_n(&before_parts) && pars_after_colon)
		{
			const char *topic = pars_after_colon;
			int channel_index = create_channel(sr, string_array_get(&before_parts, 0));
			channel *pc = &ps -> pchannels[channel_index];

			myfree(pc -> topic);
			pc -> topic = strdup(topic);

			if (pc -> keeptopic != NULL && strstr(topic, pc -> keeptopic) == NULL)
			{
				if (irc_topic(ps -> fd, pc -> channel_name, pc -> keeptopic) == -1)
					rc = -1;
			}

			pc -> topic_changed = TRUE;

			headline_channel_msg(ps, pc, topic);

			update_statusline(sr, channel_index, "topic changed on channel %s by %s to %s", pc -> channel_name, prefix, topic);
		}
	}
	else if (strcmp(cmd, "KICK") == 0)
	{
		if (string_array_get_n(&before_parts) == 2)
	 	{
			const char *channel_name = string_array_get(&before_parts, 0);
			int channel_index = create_channel(sr, channel_name);

			const char *victim = string_array_get(&before_parts, 1);
			const char *kicker = prefix ? prefix : "?";
			const char *reason = pars_after_colon ? pars_after_colon : "?";

			if (channel_index != -1)
			{
				char redraw = 0;

				if (sr == current_server && current_server_channel_nr == channel_index)
					redraw = 1;

				/* me? */
				if (strcasecmp(victim, ps -> nickname) == 0)
				{
					update_statusline(sr, 0, "you were kicked from channel '%s' by %s: %s\n", victim, kicker, reason);
					cmd_LEAVE(sr, channel_index, channel_name);

					if (auto_rejoin)
						irc_join(ps -> fd, channel_name);
					else
						popup_notify(FALSE, "You were kicked from channel %s by\n%s, reason:\n%s", channel_name, kicker, reason);
				}
				else
				{
					delete_from_channel_by_nick(sr, channel_index, victim);

					update_statusline(sr, channel_index, "user '%s' kicked from channel '%s' by %s\n", victim, channel_name, kicker);
				}

				if (redraw)
				{
					show_channel_names_list();
				}
			}
			else if (channel_index == -1)
			{
				update_statusline(sr, 0, "user '%s' kicked from unknown channel '%s'\n", victim, channel_name);
			}
		}

		if (prefix)
			update_user_host(sr, prefix);
	}
	else if (strcmp(cmd, "PART") == 0 && prefix)	/* PART/QUIT (leave channel) */
	{
		int idx = 0, n = string_array_get_n(&before_parts);
		char *nick = strdup(prefix);

		for(idx=0; idx<n; idx++)
		{
			const char *channel_name = string_array_get(&before_parts, idx);
			int channel_index = find_channel_index(sr, channel_name);

			LOG("PART %d %d |%s|\n", idx, channel_index, channel_name);

			if (channel_index != -1)
			{
				delete_from_channel_by_nick(sr, channel_index, nick);

				if (show_parts)
					update_statusline(sr, channel_index, "user %s left (%s)", nick, str_or_nothing(pars_after_colon));
			}
		}

		if (prefix)
			update_user_host(sr, prefix);

		free(nick);
	}
	else if (strcmp(cmd, "QUIT") == 0 && prefix)	/* PART/QUIT (leave channel) */
	{
		char *nick = strdup(prefix);

		terminate_str(nick, '!');

		if (show_parts)
		{
			int channel_index = 0;

			for(channel_index = 0; channel_index < ps -> n_channels; channel_index++)
			{
				if (has_nick(sr, channel_index, nick))
					update_statusline(sr, channel_index, "user %s quits (%s)", nick, str_or_nothing(pars_after_colon));
			}
		}

		delete_by_nick(sr, nick);

		free(nick);
	}
	else if (strcmp(cmd, "MODE") == 0 && string_array_get_n(&before_parts) >= 2)		/* MODE */
	{
		const char *channel_str = string_array_get(&before_parts, 0);
		int channel_index = create_channel(sr, channel_str);
		const char *mode = string_array_get(&before_parts, 1);
		const char *flag_pars = string_array_get_n(&before_parts) >= 3 ? string_array_get(&before_parts, 2) : NULL;

		if (is_channel(channel_str) && flag_pars != NULL)
		{
			char *nick = strdup(flag_pars);
			irc_user_mode_t u_mode = MODE_NONE;

			terminate_str(nick, ' ');

			if (mode[1] == 'o')
				u_mode = MODE_OPERATOR;
			else if (mode[1] == '+') /* FIXME */
				u_mode = MODE_CAN_SPEAK;

			if (u_mode == MODE_OPERATOR)
			{
				channel *pc = &ps -> pchannels[channel_index];
				char *headline = NULL;

				if (mode[0] == '+')
					asprintf(&headline, "%s became OP", nick);
				else
					asprintf(&headline, "%s is no longer OP", nick);

				headline_channel_msg(ps, pc, headline);

				free(headline);
			}

			if (mode[0] == '+')
				set_nick_mode(sr, channel_index, nick, text_to_nick_mode(nick) | u_mode);
			else if (mode[0] == '-')
				set_nick_mode(sr, channel_index, nick, text_to_nick_mode(nick) & (~u_mode));
			else
				LOG("!! unknown mode thing '%s' in channel '%s'\n", mode, channel_str);

			show_channel_names_list();

			if (show_mode_changes)
				update_statusline(sr, channel_index, "new mode %s by %s for %s in channel %s", mode, prefix, nick, channel_str);

			free(nick);
		}
		else if (show_mode_changes)
		{
			update_statusline(sr, channel_index, "new mode %s for %s", str_or_nothing(mode), str_or_nothing(channel_str));
		}

		if (prefix)
			update_user_host(sr, prefix);
	}
	else if (strcmp(cmd, "NICK") == 0 && (pars_after_colon != NULL || pars_before_colon != NULL) && prefix)		/* NICK */
	{
		int channel_index = 0;
		const char *new_nick = (pars_after_colon && strlen(pars_after_colon)) ? pars_after_colon : pars_before_colon;
		char *old_nick = strdup(prefix);

		terminate_str(old_nick, '!');

		for(channel_index = 0; channel_index < ps -> n_channels; channel_index++)
		{
			channel *cur_channel = &ps -> pchannels[channel_index];

			/* change nick in each channel */
			if (show_nick_change)
			{
				if (has_nick(sr, channel_index, old_nick))
					update_statusline(sr, channel_index, "%s is now known as %s", old_nick, new_nick);
			}

			/* change channelnames */
			if (strcasecmp(cur_channel -> channel_name, old_nick) == 0)
			{
				myfree(cur_channel -> channel_name);

				cur_channel -> channel_name = strdup(new_nick);
			}
		}

		replace_nick(sr, old_nick, new_nick);

		free(old_nick);
	}
	else if (strcmp(cmd, "JOIN") == 0)		/* JOIN */
	{
		int idx = 0;
		char *channels_str_before = pars_before_colon ? strdup(pars_before_colon) : NULL;
		char *channels_str_after = pars_after_colon ? strdup(pars_after_colon) : NULL;
		char *nick = strdup(prefix);
		BOOL me = strcasecmp(ps -> nickname, nick) == 0;
		string_array_t channels;

		init_string_array(&channels);

		terminate_str(nick, '!');

		if (channels_str_before && strlen(channels_str_before))
		{
			terminate_str(channels_str_before, ' ');
			split_string(channels_str_before, ",", TRUE, &channels);
		}

		if (channels_str_after && strlen(channels_str_after))
		{
			terminate_str(channels_str_after, ' ');
			split_string(channels_str_after, ",", TRUE, &channels);
		}

		for(idx=0; idx<string_array_get_n(&channels); idx++)
		{
			const char *cur_channel = string_array_get(&channels, idx);
			int channel_index = create_channel(sr, cur_channel);

			add_nick(sr, channel_index, nick, NULL, prefix);

			if (show_joins)
				update_statusline(sr, channel_index, "user %s joined %s", prefix, cur_channel);

			/* myself? then get a list of hostnames (using the WHO command) */
			if (me && irc_who(ps -> fd, cur_channel) == -1)
				rc = -1;
		}

		show_channel_names_list();

		free_string_array(&channels);

		free(channels_str_before);
		free(channels_str_after);

		if (keep_channels_sorted)
			sort_channels(sr);
	}
	else if (strcmp(cmd, "INVITE") == 0)	/* INVITE */
	{
		const char *channel_name = pars_after_colon;

		if (!channel_name)
			update_statusline(sr, 0, "Received garbled INVITE message (channel missing)");
		else if (!allow_invite)
			update_statusline(sr, 0, "Ignoring invite for channel %s", channel_name);
		else
			rc = irc_join(ps -> fd, channel_name);
	}
	else if ((strcmp(cmd, "PRIVMSG") == 0 || strcmp(cmd, "NOTICE") == 0) && pars_before_colon) /* PRIVMSG / NOTICE */
	{
		int idx = 0;
		const char *text = pars_after_colon;
		const char *sender = prefix ? prefix : "?";
		char *channel_list = strdup(pars_before_colon);
		char *new_line = NULL, *command = NULL;
		string_array_t targets;

		terminate_str(channel_list, ' ');

		init_string_array(&targets);
		split_string(channel_list, ",", TRUE, &targets);

		if (ps -> ts_last_msg)
		{
			ps -> t_event += now - ps -> ts_last_msg;
			ps -> n_event++;

			if (ps -> n_event > 100)
			{
				ps -> t_event /= (double)ps -> n_event;

				ps -> t_event *= 5;
				ps -> n_event = 5;
			}
		}

		ps -> ts_last_msg = now;

		if (text)
			add_to_wc(text);

		for(idx=0; idx<string_array_get_n(&targets); idx++)
		{
			channel *pc = NULL;
			int channel_index = -1;

			if (notice_in_server_channel == TRUE && strcmp(cmd, "NOTICE") == 0)
				channel_index = 0;
			else
			{
				char *channel = strdup(string_array_get(&targets, idx)), *err = NULL;

				/* is this a private msg? (dest is my nick: use * nick of sender) */
				if (strcasecmp(ps -> nickname, channel) == 0 || (ps -> nickname2 && strcasecmp(ps -> nickname2, channel) == 0))
				{
					free(channel);

					channel = strdup(sender);

					terminate_str_r(channel, '!');
				}

				if (text)
				{
					BOOL dummy = FALSE, match = FALSE;

					if (process_grep_filter(gp, ps -> description, channel, text, &err, &dummy) == FALSE && notified_grep_filter_problem == FALSE)
					{
						popup_notify(FALSE, "%s", err);
						free(err);

						notified_grep_filter_problem = TRUE;
					}

					if (process_grep_filter(hlgp, ps -> description, channel, text, &err, &match) && show_headlines && match)
					{
						char *line = NULL;
						char *sender_short = strdup(sender);

						terminate_str(sender_short, '!');

						asprintf(&line, "%s@%s (%s): %s", sender_short, channel, ps -> description, text);
						add_headline(TRUE, line);
						free(line);

						free(sender_short);
					}
				}

				channel_index = create_channel(sr, channel);

				myfree(channel);
			}

			if (text && log_channel(sr, channel_index, sender, text, FALSE) == -1)
				rc = -1;

			pc = &ps -> pchannels[channel_index];

			if (strcmp(cmd, "PRIVMSG") == 0)
				pc -> recvd_non_notice = TRUE;

			process_scripts(ps, pc, sender, text, FALSE, &new_line, &command);

			if (new_line != NULL && command != NULL)
			{
				if (strcasecmp(command, "REPLY") == 0)
				{
					if (irc_privmsg(ps -> fd, pc -> channel_name, new_line) == -1)
						rc = -1;

					if (log_channel(sr, channel_index, ps -> nickname, new_line, FALSE) == -1)
						rc = -1;
				}
			}

			myfree(new_line);
			myfree(command);
		}

		free_string_array(&targets);
		free(channel_list);

		if (prefix)
			update_user_host(sr, prefix);
	}
	else if (cmd_nr == 001)
	{
		update_statusline(sr, 0, "Logged in to %s:%d (%s)", ps -> server_host, (int)ps -> server_port, str_or_nothing(ps -> description));

		set_state(sr, STATE_RUNNING);
	}
	else if (cmd_nr == 317 && pars_before_colon)		/* idle time and such */
	{
		time_t signon = 0;

		/* flok Uhmmm 111 1110956321 :seconds idle, signon time */
		char *space = strchr(pars_before_colon, ' ');
		if (space)
		{
			char *nick = NULL;

			while(*space == ' ')
				space++;

			/* space now points behind 'flok' */
			nick = space;
			space = strchr(space, ' ');
			if (space)
			{
				int idle = 0;

				*space = 0x00;
				space++;

				while(*space == ' ')
					space++;

				/* space now points behind 'uhmmm' */
				idle = atoi(space);

				space = strchr(space, ' ');
				if (space)
				{
					int lc_rc = -1;
					char *str_buffer = NULL;

					while(*space == ' ')
						space++;

					/* space now points behind 111 */
					signon = (time_t)atol(space);

					asprintf(&str_buffer, "%s is %d seconds idle and on-line since %s", nick, idle, ctime(&signon));

					lc_rc = log_channel(sr, 0, NULL, str_buffer, TRUE);
					myfree(str_buffer);

					if (lc_rc == -1)
						rc = -1;
				}
			}
		}
	}
	else if (cmd_nr == 332)		/* set TOPIC */
	{
		const char *channel_str = string_array_get(&before_parts, 1);
		const char *topic = pars_after_colon;

		int channel_index = create_channel(sr, channel_str);

		channel *pc = &ps -> pchannels[channel_index];
		myfree(pc -> topic);

		pc -> topic = topic ? strdup(topic) : NULL;

		headline_channel_msg(ps, pc, topic);

		pc -> topic_changed = TRUE;
	}
	else if (cmd_nr == 409)		/* ":No origin specified": PING/PONG fails! */
	{
		update_statusline(sr, 0, "internal error while replying to PING");

		rc = -1;
	}
	else if (cmd_nr >= 431 && cmd_nr <= 436)/* problem setting NICK */
	{
		if (ps -> state != STATE_RUNNING)
		{
			char *msg = NULL;
			const char *nick = gen_random_nick();

			rc = irc_nick(ps -> fd, nick);

			asprintf(&msg, "Your default nick was refused, using a temporary random one: %s", nick);
			update_statusline(sr, 0, msg);

			myfree(nick);

			myfree(msg);
		}
		else
		{
			const char msg [] = "Your new nick was refused";

			update_statusline(sr, 0, msg);

			popup_notify(FALSE, msg);
		}
	}
	else if ((cmd_nr == 353 || cmd_nr == 366) && pars_before_colon)	/* channels nick list */
	{
		char *channel_start = strchr(pars_before_colon, '#');

		if (channel_start == NULL)
			channel_start = strchr(pars_before_colon, '&');

		if (channel_start && pars_after_colon)
		{
			int channel_index = 0;
			const char *pnames = pars_after_colon;
			channel *pc = NULL;

			terminate_str(channel_start, ' ');

			/* find channel index */
			channel_index = create_channel(sr, channel_start);

			pc = &ps -> pchannels[channel_index];

			if (cmd_nr == 353)
			{
				if (pc -> adding_names == FALSE)
				{
					free_names_list(pc);

					pc -> adding_names = TRUE;
				}

				for(; strlen(pnames) > 0;)
				{
					char *space = strchr(pnames, ' ');
					if (space)
						*space = 0x00;

					add_nick(sr, channel_index, pnames, NULL, NULL);

					if (!space)
						break;

					pnames = space + 1;
				}
			}
			else
			{
				pc -> adding_names = FALSE;
			}

			show_channel_names_list();
		}
	}
	else if (cmd_nr >= 251 && cmd_nr <= 259) /* stats */
		update_statusline(sr, 0, "%s %03d: %s :%s", str_or_nothing(prefix), cmd_nr, str_or_nothing(pars_before_colon), str_or_nothing(pars_after_colon));
	else if (cmd_nr == 311)	/* reply to whois */
	{
		/* 311     RPL_WHOISUSER
		   "<nick> <user> <host> * :<real name>"
		   :irc.xs4all.nl 311 mynick hisnick hisuser hishost * :hisname */
		if (string_array_get_n(&before_parts) >= 2 && pars_after_colon != NULL)
		{
			char *temp = NULL;
			const char *nick = string_array_get(&before_parts, 1);
			const char *full_name = pars_after_colon;
			int index = 0;

			for(index=0; index<ps -> n_channels; index++)
				change_name(sr, index, nick, full_name);

			if (string_array_get_n(&before_parts) >= 4)
			{
				char *user_host = NULL;

				asprintf(&user_host, "%s@%s", string_array_get(&before_parts, 3), string_array_get(&before_parts, 4));
				update_user_host(sr, user_host);

				free(user_host);
			}

			asprintf(&temp, "%s is %s", nick, full_name);
			update_statusline(sr, 0, "%s\n", temp);

			if (string_array_get_n(&before_parts) >= 2)
				log_to_user_window(sr, string_array_get(&before_parts, 1), temp);

			free(temp);
		}
	}
	else if ((cmd_nr >= 312 && cmd_nr <= 319) || cmd_nr == 330) /* more WHOIS info */
	{
		if (string_array_get_n(&before_parts) >= 2)
		{
			char *temp = NULL;
			asprintf(&temp, "%s %s", str_or_nothing(pars_before_colon), str_or_nothing(pars_after_colon));
			log_to_user_window(sr, string_array_get(&before_parts, 1), temp);
			free(temp);
		}
	}
	else if (cmd_nr == 321)		/* LIST start */
	{
		update_statusline(sr, 0, "Receiving list of channels/topics...");

		free_channel_list(sr);
	}
	else if (cmd_nr == 322 && string_array_get_n(&before_parts) >= 2)	/* LIST part */
	{
		const char *channel_str = string_array_get(&before_parts, 1);
		const char *topic = str_or_nothing(pars_after_colon);

		ps -> channel_list = (channel_topic_t *)realloc(ps -> channel_list, sizeof(channel_topic_t) * (ps -> channel_list_n + 1));
		ps -> channel_list[ps -> channel_list_n].channel = strdup(channel_str);
		ps -> channel_list[ps -> channel_list_n].topic = strdup(topic);

		ps -> channel_list_n++;
	}
	else if (cmd_nr == 323)		/* LIST end */
	{
		ps -> channel_list_complete = TRUE;

		qsort(ps -> channel_list, ps -> channel_list_n, sizeof(channel_topic_t), compare_channel_list_item);

		update_statusline(sr, 0, "List of channels/topics successfully received");
	}
	else if (cmd_nr == 352)		/* reply to 'WHO' */
	{
		const char *channel_str = string_array_get(&before_parts, 1);
		const char *nick = string_array_get(&before_parts, 5);
		int channel_index = create_channel(sr, channel_str);
		const char *user = string_array_get(&before_parts, 2);
		const char *host = string_array_get(&before_parts, 3);
		char *user_host = NULL;

		/* irc.0x20.nl|352|flox3 #eenzaam folkert7 2001:888:13b3:64:349c:ff08:8d88:114f irc.0x20.nl flok H :0 Folkert van Heusden */

		asprintf(&user_host, "%s@%s", user, host);

		if (!has_nick(sr, channel_index, nick))
			add_nick(sr, channel_index, nick, NULL, user_host);
		else
			change_user_host(sr, channel_index, nick, user_host);

		free(user_host);
	}
	else if (cmd_nr == 372 || cmd_nr == 375 || cmd_nr == 376) /* MOTD */
		update_statusline(sr, 0, "%s %03d: %s :%s", str_or_nothing(prefix), cmd_nr, str_or_nothing(pars_before_colon), str_or_nothing(pars_after_colon));
	else if (cmd_nr == 391)		/* TIME */
	{
		double took = get_ts() - ps -> sent_time_req_ts;

		if (!ps -> hide_time_req && pars_after_colon && (ps -> prev_cmd == NULL || strcmp(ps -> prev_cmd, cmd) != 0))
			log_channel(sr, 0, NULL, pars_after_colon, TRUE);

		ps -> sent_time_req_ts = 0;
		ps -> hide_time_req = FALSE;

		if (ps -> server_latency <= 0.0)
			ps -> server_latency = took;
		else
			ps -> server_latency = (took * 2 + ps -> server_latency) / 3;
	}
	else if (!ignore_unknown_irc_protocol_msgs)
	{
		update_statusline(sr, 0, "unknown command from irc-server: ':%s %s %s :%s'", str_or_nothing(prefix), str_or_nothing(cmd), str_or_nothing(pars_before_colon), str_or_nothing(pars_after_colon));
	}

	if (strcmp(cmd, "PRIVMSG") != 0 && strcmp(cmd, "NOTICE") != 0 && strcmp(cmd, "PING") != 0)
	{
		free(server_list[sr].prev_cmd);
		ps -> prev_cmd = strdup(cmd);
	}

	free_string_array(&before_parts);

	myfree(pars_after_colon);
	myfree(pars_before_colon);
	myfree(pars);
	myfree(cmd);
	myfree(prefix);

	return rc;
}

int irc_privmsg(int fd, const char *channel, const char *msg)
{
	return do_send(fd, "PRIVMSG %s :%s", channel, msg);
}

int irc_kick(int fd, const char *channel, const char *nick, const char *comment)
{
	if (comment)
		return do_send(fd, "KICK %s %s :%s", channel, nick, comment);
	else
		return do_send(fd, "KICK %s %s", channel, nick);
}

int irc_ban(int fd, const char *channel, const char *nick)
{
	return do_send(fd, "MODE %s +b %s", channel, nick);
}

int irc_list(int fd)
{
	return do_send(fd, "LIST");
}

int irc_op(int fd, const char *channel, const char *nick, BOOL op)
{
	if (op)
		return do_send(fd, "MODE %s +o %s", channel, nick);
	else
		return do_send(fd, "MODE %s -o %s", channel, nick);
}

int irc_whois(int fd, const char *nick)
{
	return do_send(fd, "WHOIS %s", nick);
}

int irc_allowspeak(int fd, const char *channel, const char *nick, BOOL speak)
{
	if (speak)
		return do_send(fd, "MODE %s +v %s", channel, nick);
	else
		return do_send(fd, "MODE %s -v %s", channel, nick);
}

int irc_v3_cap_start(int fd)
{
	return do_send(fd, "CAP REQ");
}

int irc_v3_cap_end(int fd)
{
	return do_send(fd, "CAP END");
}

int irc_who(int fd, const char *channel)
{
	return do_send(fd, "WHO %s", channel);
}

int irc_topic(int fd, const char *channel, const char *topic)
{
	return do_send(fd, "TOPIC %s :%s", channel, topic);
}

int irc_ping(int fd, const char *nick, int ts)
{
	char str_buffer[IRC_MAX_MSG_LEN] = { 0 };

	snprintf(str_buffer, sizeof str_buffer, "\001PING %d\001", ts);

	return irc_privmsg(fd, nick, str_buffer);
}

int irc_version(int fd)
{
	return do_send(fd, "VERSION");
}

int irc_time(int fd)
{
	return do_send(fd, "TIME");
}

int irc_join(int fd, const char *channel)
{
	return do_send(fd, "JOIN %s", channel);
}

int irc_part(int fd, const char *channel, const char *msg)
{
	if (msg)
		return do_send(fd, "PART %s :%s", channel, msg);

	return do_send(fd, "PART %s", channel);
}

int irc_quit(int fd, const char *msg)
{
	if (msg)
		return do_send(fd, "QUIT :%s", msg);

	return do_send(fd, "QUIT");
}
