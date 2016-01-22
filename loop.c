/* GPLv2 applies
 * SVN revision: $Revision: 887 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#define _GNU_SOURCE
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <ncursesw/ncurses.h>
#include <ncursesw/panel.h>

#include "gen.h"
#include "error.h"
#include "theme.h"
#include "term.h"
#include "buffer.h"
#include "channels.h"
#include "servers.h"
#include "loop.h"
#include "utils.h"
#include "tcp.h"
#include "irc.h"
#include "dcc.h"
#include "main.h"
#include "names.h"
#include "key_value.h"
#include "wordcloud.h"
#include "config.h"
#include "colors.h"
#include "ansi.h"
#include "soundex.h"
#include "user.h"
#include "ctcp.h"
#include "headlines.h"
#include "help.h"

NEWWIN *chat_window_border = NULL, *channel_window_border = NULL, *input_window_border = NULL;
NEWWIN *chat_window = NULL, *input_window = NULL, *channel_window = NULL, *topic_line_window = NULL, *headline_window = NULL;
visible_channels *vc_list = NULL;
server *server_list = NULL;
int n_servers = 0;
layout_theme theme;
int input_window_width = 40;
char *time_str_fmt = "%T";

BOOL notified_logging_error = FALSE;

int topic_scroll_offset = 0;

int current_server = 0, current_server_channel_nr = 0;

double last_topic_show_ts = 0.0;

void gen_display_markerline(NEWWIN *win, time_t ts)
{
	char *ts_str = ctime(&ts);

	terminate_str(ts_str, '\n');

	display_markerline(win, ts_str);
}

void add_markerline(int sr, int ch)
{
	server *ps = &server_list[sr];
	channel *pc = &ps -> pchannels[ch];

	if (!latest_is_markerline(pc -> pbuffer))
	{
		if (only_one_markerline)
			delete_type(pc -> pbuffer, BET_MARKERLINE);

		add_to_buffer(pc -> pbuffer, NULL, NULL, TRUE, sr, ch);

		if (sr == current_server && ch == current_server_channel_nr)
		{
			time_t now = time(NULL);

			gen_display_markerline(chat_window, now);
		}
	}
}

void add_markerline_to_all(void)
{
	int sr = 0;

	for(sr=0; sr<n_servers; sr++)
	{
		int ch = 0;
		server *ps = &server_list[sr];

		for(ch=0; ch<ps -> n_channels; ch++)
			add_markerline(sr, ch);
	}
}

int myisdigit(int c)
{
	return c >= '0' && c <= '9';
}

int get_color(const char *string, int *num)
{
	char c1 = string[0];

	if (myisdigit(c1))
	{
		char c2 = string[1];

		if (myisdigit(c2))
		{
			*num = (c1 - '0') * 10 + (c2 - '0');
			return 2;
		}

		*num = c1 - '0';

		return 1;
	}

	return 0;
}

BOOL nick_hit(const char *nick, const char *str)
{
	if (fuzzy_highlight && fuzzy_match(str, nick, NULL))
		return TRUE;

	if (strcasestr(str, nick))
		return TRUE;

	return FALSE;
}

BOOL find_matches(const char *haystack, const char *needle, char *bitmap)
{
	int nlen = strlen(needle);
	const char *index = haystack;
	BOOL match = FALSE;

	for(;;)
	{
		int pos = -1;

		index = strcasestr(index, needle);
		if (!index)
			break;

		match = TRUE;

		pos = (int)(index - haystack);
		memset(&bitmap[pos], '1', nlen);

		index += nlen;
	}

	return match;
}

BOOL find_extra_highlights(const char *haystack, char *bitmap)
{
	BOOL match = FALSE;
	int index = 0;

	for(index=0; index<string_array_get_n(&extra_highlights); index++)
		match |= find_matches(haystack, string_array_get(&extra_highlights, index), bitmap);

	return match;
}

void output_to_window(NEWWIN *win, const char *string_in, const char *match, be_type_t line_type, nick_color_settings *pncs, BOOL force_partial_highlight, BOOL fit)
{
	BOOL bold = FALSE, bright = FALSE, underline = FALSE, inverse = FALSE;
	int last_pair = -1;
	BOOL hl = FALSE;
	char *pm_bitmap = NULL;
	BOOL pm_set = FALSE;
	const char *string = filter_ansi(string_in);
	int loop = 0, len = strlen(string), outer_loop = 0;

	if (highlight)
	{
		pm_bitmap = (char *)malloc(len + 1);
		memset(pm_bitmap, '0', len);
		pm_bitmap[len] = 0x00;

		if (fuzzy_highlight)
			hl = fuzzy_match(string, match, pm_bitmap);
		else
			hl = find_matches(string, match, pm_bitmap);

		hl |= find_extra_highlights(string, pm_bitmap);
	}

	if (pncs)
	{
		bold = pncs -> bold;

		if (bold)
			mywattron(win -> win, A_BOLD);
		else
			mywattroff(win -> win, A_BOLD);

		last_pair = pncs -> pair;
		color_on(win, last_pair);
	}
	else if (hl == TRUE && colors_meta)
	{
		if (!partial_highlight_match && !force_partial_highlight)
		{
			last_pair = highlight_colorpair;

			color_on(win, last_pair);
		}
	}
	else if (line_type == BET_META && colors_meta)
	{
		last_pair = meta_colorpair;
		color_on(win, last_pair);
	}

	for(outer_loop=0; outer_loop<len && string[outer_loop] && string[outer_loop] != '\n';)
	{
		int x = getcurx(win -> win);
		int start_word = 0, wordlen = 0;

		if (outer_loop == 0 && x > 0)
			wprintw(win -> win, "\n");

		while(string[outer_loop] == ' ')
		{
			wprintw(win -> win, " ");
			outer_loop++;
		}

		x = getcurx(win -> win);

		start_word = outer_loop;
		while(string[outer_loop] != ' ' && string[outer_loop] != 0x00 && string[outer_loop] != '\n')
			outer_loop++;

		/* count size of word, might be utf8! */
		/* FIXME do not count escapes (^B, ^C) */
		for(loop=start_word; loop<outer_loop;)
		{
			loop += count_utf_bytes(string[loop]);

			wordlen++;
		}

		if (x + wordlen > win -> ncols && wordlen < win -> ncols / 3)
			wprintw(win -> win, "\n");

		for(loop=start_word; loop<outer_loop;)
		{
			if (hl && partial_highlight_match && pm_bitmap)
			{
				if (pm_bitmap[loop] == '1')
				{
					last_pair = highlight_colorpair;

					color_on(win, last_pair);

					pm_set = TRUE;
				}
				else if (pm_set)
				{
					if (last_pair == highlight_colorpair)
					{
						color_off(win, last_pair);

						last_pair = -1;
					}

					pm_set = FALSE;
				}
			}

			if (string[loop] == 15) /* ^O -> reset attributes */
			{
				inverse = FALSE;
				underline = FALSE;
				bold = FALSE;
				bright = FALSE;

				reset_attributes(win);

				if (last_pair != -1)
				{
					color_off(win, last_pair);

					last_pair = -1;
				}

				loop++;
			}
			else if (string[loop] == 22) /* ^V -> inverse */
			{
				inverse = !inverse;

				if (inverse)
					mywattron(win -> win, A_REVERSE);
				else
					mywattroff(win -> win, A_REVERSE);

				loop++;
			}
			else if (string[loop] == 31)	/* ^_ -> underline */
			{
				underline = !underline;

				if (underline)
					mywattron(win -> win, A_UNDERLINE);
				else
					mywattroff(win -> win, A_UNDERLINE);

				loop++;
			}
			else if (string[loop] == 2)	/* ^B -> bold */
			{
				bold = !bold;

				if (bold)
					mywattron(win -> win, A_BOLD);
				else
					mywattroff(win -> win, A_BOLD);

				loop++;
			}
			else if (string[loop] == 3) /* ^c */
			{
				int fg = -1, bg = -1;

				if (last_pair != -1)
				{
					color_off(win, last_pair);
					last_pair = -1;

					if (bright)
					{
						mywattroff(win -> win, A_STANDOUT);
						bright = FALSE;
					}
				}

				loop++; /* skip ^c */
				loop += get_color(&string[loop], &fg); /* skip over this color */

				if (string[loop] == ',')
				{
					loop++; /* skip , */
					loop += get_color(&string[loop], &bg);
				}

				last_pair = get_color_mirc(fg, bg);

				color_on(win, last_pair);

				hl = FALSE;
			}
			else
			{
				wchar_t dest = 0;
				const char *dummy = &string[loop], *dummy_start = dummy;
				int skip_n = 0;

				mbsrtowcs(&dest, &dummy, 1, NULL);

				waddnwstr(win -> win, &dest, 1);

				skip_n = (int)(dummy - dummy_start);
				if (skip_n <= 0)
					skip_n = 1;

				loop += skip_n;
			}
		}
	}

	if (last_pair != -1)
		color_off(win, last_pair);

	reset_attributes(win);

	color_on(win, default_colorpair);

	if (!fit)
	{
		mywattron(win -> win, A_REVERSE);
		waddch(win -> win, '>');
		mywattroff(win -> win, A_REVERSE);
	}

	myfree(pm_bitmap);

	myfree(string);
}

int log_channel(int iserver, int channel_nr, const char *user, const char *string, BOOL meta_hl)
{
	int rc = 0;
	char *str_buffer = NULL;
	time_t rnow = time(NULL);
	struct tm *ptm = localtime(&rnow);
	char *ts = NULL;
	server *ps = &server_list[iserver];
	channel *pc = &ps -> pchannels[channel_nr];
	BOOL output_to_terminal = iserver == current_server && channel_nr == current_server_channel_nr;
	char *headline = NULL;

	asprintf(&headline, "%s(%s) %s: %s\n", pc -> channel_name, ps -> description, str_or_nothing(user), string);

	if (log_to_file(iserver, channel_nr, user, string) == -1)
	{
		if (notified_logging_error == FALSE)
		{
			popup_notify(FALSE, "Problem logging to file:\n%s\n(this message is shown once)", strerror(errno));

			notified_logging_error = TRUE;
		}
	}

	if (user != NULL && nick_hit(ps -> nickname, string) && strcasecmp(user, ps -> nickname) != 0)
	{
		if (notify_nick)
		{
			const char *pars[4] = { NULL };

			pars[1] = user;
			pars[2] = string;
			pars[3] = NULL;

			run(notify_nick, pars);
		}

		add_headline(TRUE, headline);
	}
	else
	{
		check_headline_matches(string, headline);
	}

	if (pc -> last_entry)
	{
		pc -> t_event += rnow - pc -> last_entry;
		pc -> n_event++;

		if (pc -> n_event > 100)
		{
			pc -> t_event /= (double)pc -> n_event;

			pc -> t_event *= 5;
			pc -> n_event = 5;
		}
	}

	if (theme.show_time)
	{
		char buffer[1024] = { 0 };

		strftime(buffer, sizeof buffer, time_str_fmt, ptm);

		ts = strdup(buffer);
	}

	if (user)
	{
		BOOL me = FALSE;
		const char *temp = exec_and_strip_ctcp(iserver, channel_nr, user, string, &me);

		if (me)
			asprintf(&str_buffer, "%s %s", ts, temp);
		else if (full_user)
			asprintf(&str_buffer, "%s %s: %s", ts, user, temp);
		else
		{
			char *nick_only = strdup(user);
			terminate_str(nick_only, '!');

			asprintf(&str_buffer, "%s %s: %s", ts, nick_only, temp);

			free(nick_only);
		}

		myfree(temp);
	}
	else
	{
		asprintf(&str_buffer, "%s > %s", ts, string);
	}

	if (user == NULL || is_ignored(iserver, channel_nr, user) == FALSE)
	{
		time_t last_entry = pc -> last_entry;

		if (rnow - last_entry >= 86400 && last_entry > 0)
		{
			char *meta_str_buffer = NULL;

			/* output date/time last msg */
			char *time_str1 = strdup(ctime(&last_entry)), *time_str2 = NULL;

			if (time_str1 == NULL)
				time_str1 = strdup("?");
			else
			{
				char *dummy = strchr(time_str1, '\n');

				if (dummy)
					*dummy = 0x00;
			}

			/* display current date/time */
			time_str2 = strdup(ctime(&rnow));

			if (!time_str2)
				time_str2 = strdup("?");
			else
				terminate_str(time_str2, '\n');

			asprintf(&meta_str_buffer, "last message: %s (%ld), current time: %s (%ld)", time_str1, last_entry, time_str2, rnow);

			add_to_buffer(pc -> pbuffer, meta_str_buffer, NULL, TRUE, iserver, channel_nr);

			if (output_to_terminal)
				output_to_window(chat_window, meta_str_buffer, ps -> nickname, FALSE, NULL, FALSE, TRUE);

			myfree(meta_str_buffer);
			myfree(time_str2);
			myfree(time_str1);
		}

		add_to_buffer(pc -> pbuffer, str_buffer, user, meta_hl, iserver, channel_nr);

		pc -> last_entry = rnow;

		if (output_to_terminal)
		{
			nick_color_settings ncs;

			find_nick_colorpair(user, &ncs);

			output_to_window(chat_window, str_buffer, ps -> nickname, meta_hl, nick_color ? &ncs : NULL, FALSE, TRUE);
		}
		else
		{
			/* set mark on window */
			if (nick_hit(ps -> nickname, string) || (is_channel(pc -> channel_name) == FALSE && channel_nr != 0 && mark_personal_messages))
				pc -> new_entry = YOU;
			else
			{
				if (meta_hl == TRUE)
				{
					if (pc -> new_entry == NONE && mark_meta)
						pc -> new_entry = META;
				}
				else
				{
					if (pc -> new_entry == NONE || pc -> new_entry == META)
						pc -> new_entry = MISC;
				}
			}

			set_new_line_received();
		}
	}

	free(headline);

	myfree(str_buffer);

	myfree(ts);

	return rc;
}

void update_statusline(int serv, int chan, const char *fmt, ...)
{
	char *str_buffer = NULL;
	va_list ap;

	va_start(ap, fmt);
	(void)vasprintf(&str_buffer, fmt, ap);
	va_end(ap);

	if (serv == -1)
		serv = 0;

	log_channel(serv, chan, NULL, str_buffer, TRUE);
	LOG("%s\n", str_buffer);

	free(str_buffer);
}

void create_windows()
{
	int ch_win_height = max_y - 2, ch_win_offset = 0;
	int all_win_offset = 1, lines_min = 2;

	bkgd(default_colorpair);

	/* while building the display, ignore SIGWINCH */
	if (signal(SIGWINCH, SIG_IGN) == SIG_ERR)
		error_exit(TRUE, "signal (SIGWINCH/SIG_IGN) failed");

	if (topic_line_window)
		delete_window(topic_line_window);
	topic_line_window = create_window_xy(0, 0, 1, max_x);

	if (headline_window)
		delete_window(headline_window);
	if (show_headlines)
	{
		headline_window = create_window_xy(1, 0, 1, max_x);
		all_win_offset++;
		lines_min++;
		ch_win_height--;
	}

	scrollok(topic_line_window -> win, FALSE);

	color_on(topic_line_window, default_colorpair);
	mywbkgd(topic_line_window, default_colorpair);

	if (chat_window_border)
		delete_window(chat_window_border);
	if (chat_window)
		delete_window(chat_window);

	if (theme.chat_window_border == TRUE && max_y > 2)
	{
		chat_window_border = create_window_xy(all_win_offset, 0, max_y - lines_min, max_x - theme.channellist_window_width);
		color_on(chat_window_border, default_colorpair);
		box(chat_window_border -> win, 0, 0);
		mywbkgd(chat_window_border, default_colorpair);

		chat_window = create_window_xy(all_win_offset + 1, 1, max_y - (lines_min + 2), max_x - (theme.channellist_window_width + 2));
	}
	else
	{
		chat_window = create_window_xy(all_win_offset, 0, max_y - lines_min, max_x - theme.channellist_window_width);
	}

	mywbkgd(chat_window, default_colorpair);

	color_on(chat_window, default_colorpair);
	scrollok(chat_window -> win, TRUE);

	if (input_window_border)
		delete_window(input_window_border);
	if (input_window)
		delete_window(input_window);

	if (space_after_start_marker)
		input_window_width = max_x - 4;
	else
		input_window_width = max_x - 2;

	input_window_border = create_window_xy(max_y - 1, 0, 1, input_window_width + (space_after_start_marker ? 4 : 2));

	input_window = create_window_xy(max_y - 1, (space_after_start_marker ? 2 : 1), 1, input_window_width);

	color_on(input_window, default_colorpair);
	mywbkgd(input_window, default_colorpair);

	if (channel_window_border)
		delete_window(channel_window_border);
	if (channel_window)
		delete_window(channel_window);

	if (word_cloud_n > 0 && word_cloud_win_height > 0)
	{
		ch_win_height -= word_cloud_win_height + 2/* =border size */;
		ch_win_offset = word_cloud_win_height + 2 + all_win_offset;

		wc_window_border = create_window_xy(all_win_offset, max_x - theme.channellist_window_width, word_cloud_win_height + 2, theme.channellist_window_width);
		wc_window = create_window_xy(all_win_offset + 1, max_x - (theme.channellist_window_width - 1), word_cloud_win_height, theme.channellist_window_width - 2);

		mywbkgd(wc_window_border, default_colorpair);
		mywbkgd(wc_window, default_colorpair);

		color_on(wc_window, default_colorpair);
		color_on(wc_window_border, default_colorpair);
	}
	else
	{
		ch_win_offset = all_win_offset;
	}

	if (theme.channellist_border == TRUE)
	{
		channel_window_border = create_window_xy(ch_win_offset, max_x - theme.channellist_window_width, ch_win_height, theme.channellist_window_width);
		color_on(channel_window_border, default_colorpair);
		channel_window = create_window_xy(1 + ch_win_offset, max_x - (theme.channellist_window_width - 1), ch_win_height - 2, theme.channellist_window_width - 2);

		mywbkgd(channel_window_border, default_colorpair);
	}
	else
	{
		channel_window = create_window_xy(ch_win_offset, max_x - theme.channellist_window_width, ch_win_height, theme.channellist_window_width);
	}

	color_on(channel_window, default_colorpair);
	mywbkgd(channel_window, default_colorpair);

	show_channel_names_list();

	/* set signalhandler for terminal resize */
	if (signal(SIGWINCH, do_resize) == SIG_ERR)
		error_exit(TRUE, "signal(SIGWINCH/SIG_IGN) failed");
}

BOOL show_clock(time_t prev_ts, time_t now_ts, BOOL new_data)
{
	if (theme.show_clock == TRUE)
	{
		BOOL update_clock = FALSE;
		int time_diff = now_ts - prev_ts;

		if (update_clock_at_data)
			update_clock = time_diff > 0 && new_data;
		else
			update_clock = time_diff > 0;

		if (update_clock)
		{
			struct tm *ptm = localtime(&now_ts);
			prev_ts = now_ts;

			mvwprintw(topic_line_window -> win, 0, topic_line_window -> ncols - 9, " %02d:%02d:%02d", ptm -> tm_hour, ptm -> tm_min, ptm -> tm_sec);

			return TRUE;
		}
	}

	return FALSE;
}

BOOL show_topic(int sr, int ch, double now_tsd)
{
	channel *pch = NULL;
	BOOL rc = FALSE;

	if (sr != -1)
		pch = &server_list[sr].pchannels[ch];

	if (pch == NULL)
	{
	}
	else if (topic_scroll && now_tsd - last_topic_show_ts >= 0.1)
	{
		char *cur_topic = strdup(str_or_nothing(pch -> topic));
		int ct_len = strlen(cur_topic);
		int n_shown = 0, cur_offset = 0;

		if (topic_scroll_offset >= ct_len)
			topic_scroll_offset = 0;

		werase(topic_line_window -> win);

		cur_offset = topic_scroll_offset;
		while(n_shown < topic_line_window -> ncols)
		{
			mvwprintw(topic_line_window -> win, 0, n_shown, "%s ", &cur_topic[cur_offset]);

			n_shown += ct_len - cur_offset + 1/*space!*/;
			cur_offset = 0;
		}

		topic_scroll_offset++;

		myfree(cur_topic);

		rc = TRUE;

		last_topic_show_ts = now_tsd;
	}
	else if (pch -> topic_changed == TRUE)
	{
		werase(topic_line_window -> win);

		output_to_window(topic_line_window, str_or_nothing(pch -> topic), server_list[sr].nickname, FALSE, NULL, TRUE, TRUE);

		pch -> topic_changed = FALSE;

		rc = TRUE;
	}

	return rc;
}

void check_server_connections_alive(time_t now_ts, BOOL force)
{
	if (irc_keepalive || force)
	{
		int ka_loop = 0;

		for(ka_loop=0; ka_loop<n_servers; ka_loop++)
		{
			server *ps = &server_list[ka_loop];
			time_t last_data = ps -> ts_bytes;
			int d = difftime(now_ts, last_data);

			if ((d >= 30 || force) && ps -> state == STATE_RUNNING)
			{
				if (ps -> sent_time_req_ts < 1)
				{
					ps -> hide_time_req = TRUE;

					ps -> sent_time_req_ts = get_ts();
				}

				if (irc_time(ps -> fd) == -1)
				{
					close(ps -> fd);
					set_state(ka_loop, STATE_NO_CONNECTION);

					update_statusline(ka_loop, 0, "Connection to %s:%d closed by other end (5)", ps -> server_host, ps -> server_port);
				}

				ps -> ts_bytes = now_ts;
			}
		}
	}
}

void auto_reconnect_servers(time_t now_ts)
{
	if (auto_reconnect)
	{
		int ar_loop = 0;

		for(ar_loop=0; ar_loop<n_servers; ar_loop++)
		{
			if (get_state(ar_loop) == STATE_DISCONNECTED)
			{
				server *ps = &server_list[ar_loop];
				time_t disc_since = ps -> state_since;
				int d = difftime(now_ts, disc_since);

				if (d < ps -> reconnect_delay)
					continue;

				restart_server(ar_loop);

				update_statusline(ar_loop, 0, "Retry connecting to %s:%d", ps -> server_host, ps -> server_port);
			}
		}
	}
}

int wait_for_keypress(BOOL one_event)
{
	int c = 0;
	time_t prev_ts = time(NULL);
	struct pollfd *pfd = NULL;
	int n_fd = 0;

	do
	{
		int pollrc = 0, do_refresh = 0, stdin_index = -1;

		free(pfd);
		pfd = NULL;
		n_fd = 0;

		/* wait for chars from stdin */
		/* things will break if fd0 is not first in this array */
		stdin_index = add_poll(&pfd, &n_fd, 0, POLLIN | POLLHUP);

		do_refresh |= register_dcc_events(&pfd, &n_fd);

		do_refresh |= register_server_events(&pfd, &n_fd);

		if (do_refresh)
		{
			mydoupdate();

			do_refresh = 0;
		}

		for(;!terminal_changed;)
		{
			double now_tsd = get_ts();
			double t_left = 0.1 - (now_tsd - last_topic_show_ts);
			int sleep_value = t_left * 1000.0;
			time_t now_ts = 0;

			if (sleep_value < 0)
				sleep_value = 0;

			if (!topic_scroll && sleep_value < 100)
				sleep_value = 100;

			pollrc = poll(pfd, n_fd, sleep_value);

			if (pollrc == -1)
			{
				if (errno == EINTR)
					continue;

				error_exit(TRUE, "poll() failed in main loop\n");
			}

			now_tsd = get_ts();
			now_ts = (time_t)now_tsd;

			if (update_headline(FALSE))
				do_refresh = 1;

			/* update word cloud */
			put_word_cloud(get_cursor_mode() == CM_WC, FALSE);

			if (show_clock(prev_ts, now_ts, pollrc > 0))
				do_refresh = 1;

			if (show_topic(current_server, current_server_channel_nr, now_tsd))
				do_refresh = 1;

			check_server_connections_alive(now_ts, FALSE);

			auto_reconnect_servers(now_ts);

			/* anything to redraw? */
			if (pollrc > 0 && do_refresh == 0)
				do_refresh = 1;

			break;
		}

		process_dcc_events(pfd, n_fd);

		process_server_events(pfd, n_fd);

		if (do_refresh)
			mydoupdate();

		/* terminal resize? */
		if (terminal_changed)
		{
			c = -1;
			break;
		}

		/* key pressed? then break out of loop & process */
		if (pfd[stdin_index].revents & POLLIN)
		{
			c = getch();

			break;
		}
	}
	while(one_event == FALSE);

	free(pfd);

	return c;
}

void update_channel_border(int server_index)
{
	if (chat_window_border && server_index == current_server)
	{
		const char f1_help[] = "[ Press F1 for help ]";
		const char *tooltip = get_tooltip();
		int f1_x = 0, f1_y = 0;
		int kr_x = 0, kr_y = -1;
		int f1_help_len = sizeof f1_help;
		int tooltip_len = strlen(tooltip);
		int server_color = get_server_color(server_index);

		if (server_color != -1)
			color_on(chat_window_border, server_color);

		/*color_on(chat_window_border, theme.chat_window_border_color);*/
		wborder(chat_window_border -> win, 
					theme.chat_window_border_left_side, 
					theme.chat_window_border_right_side, 
					theme.chat_window_border_top_side, 
					theme.chat_window_border_bottom_side, 
					theme.chat_window_border_top_left_hand_corner,
					theme.chat_window_border_top_right_hand_corner, 
					theme.chat_window_border_bottom_left_hand_corner, 
					theme.chat_window_border_bottom_right_hand_corner
			);

		if (inverse_window_heading)
		{
			mywattron(chat_window_border -> win, A_STANDOUT);
			mvwprintw(chat_window_border -> win, 0, 2, "%s (%s)", str_or_nothing(cur_channel() -> channel_name), str_or_nothing(cur_server() -> description ? cur_server() -> description : cur_server() -> server_host));
			mywattroff(chat_window_border -> win, A_STANDOUT);
		}
		else
		{
			mvwprintw(chat_window_border -> win, 0, 1, "[ %s (%s) ]", str_or_nothing(cur_channel() -> channel_name), str_or_nothing(cur_server() -> description ? cur_server() -> description : cur_server() -> server_host));
		}

		if (time(NULL) - started_at > 30)
		{
			f1_y = chat_window_border -> nlines - 1;
			kr_y = -1;
		}
		else
		{
			f1_y = 0;
			kr_y = chat_window_border -> nlines - 1;
		}

		f1_x = chat_window_border -> ncols - (f1_help_len + 2);
		if (f1_x < 0)
		{
			f1_x = 0;
			f1_y = -1;
		}

		kr_x = chat_window_border -> ncols - (tooltip_len + 2);
		if (kr_x < 0)
			kr_y = -1;

		if (f1_y >= 0)
			mvwprintw(chat_window_border -> win, f1_y, f1_x, f1_help);

		if (kr_y >= 0)
			mvwprintw(chat_window_border -> win, kr_y, kr_x, tooltip);

		if (server_color != -1)
			color_off(chat_window_border, server_color);
	}
}

void reset_topic_scroll_offset(void)
{
	topic_scroll_offset = 0;
}

int log_to_file(int sr, int ch, const char *nick, const char *msg)
{
	int rc = 0;

	if (log_dir && strlen(log_dir) > 0)
	{
		server *ps = &server_list[sr];
		channel *pc = &ps -> pchannels[ch];
		char *path = NULL, *file = NULL;
		FILE *fh = NULL;

		asprintf(&path, "%s/%s/", log_dir, ps -> server_host);
		asprintf(&file, "%s/%s/%s.log", log_dir, ps -> server_host, pc -> channel_name);

		if (mkpath(path, 0755) == -1)
			rc = -1;
		else
		{
			fh = fopen(file, "a+");

			if (!fh)
				rc = -1;
			else
			{
				time_t now = time(NULL);
				char *tstr = ctime(&now), *dummy = strchr(tstr, '\n');
				int crc = -1;

				if (dummy)
					*dummy = 0x00;

				if (nick)
					crc = fprintf(fh, "%s <%s> %s\n", tstr, nick, msg);
				else
					crc = fprintf(fh, "%s + %s\n", tstr, msg);

				fclose(fh);

				if (crc == -1)
					rc = -1;
			}
		}

		free(file);
		free(path);
	}

	return rc;
}
