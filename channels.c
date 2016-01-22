/* GPLv2 applies
 * SVN revision: $Revision: 886 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#define _GNU_SOURCE
#include <stdlib.h>
#include <time.h>
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
#include "utils.h"
#include "irc.h"
#include "loop.h"
#include "names.h"
#include "main.h"
#include "user.h"
#include "wordcloud.h"
#include "channels.h"
#include "config.h"
#include "chistory.h"

int channel_offset = 0, channel_cursor = 0;
BOOL vc_list_data_only = FALSE;

cu_t undo_channels[N_CU];

void free_channel(channel *pc)
{
	myfree(pc -> channel_name);

	myfree(pc -> topic);

	free_buffer(pc -> pbuffer);
	free_buffer(pc -> input_buffer);

	free_names_list(pc);

	free_utf8_string(pc -> input);
}

void free_visible_channels_list(void)
{
	if (vc_list)
	{
		myfree(vc_list -> server_index);
		myfree(vc_list -> is_server_channel);
		myfree(vc_list -> channel_index);
		myfree(vc_list -> is_1on1_channel);

		myfree(vc_list);

		vc_list = NULL;
	}
}

int add_channel(int server_index, const char *channel_name)
{
	channel *pc = NULL;
	int channel_index = server_list[server_index].n_channels;

	server_list[server_index].n_channels++;
	server_list[server_index].pchannels = realloc(server_list[server_index].pchannels, server_list[server_index].n_channels * sizeof(channel));

	pc = &server_list[server_index].pchannels[channel_index];

	memset(pc, 0x00, sizeof(channel));

	pc -> channel_name = strdup(channel_name);
	pc -> pbuffer      = create_buffer(max_channel_record_lines);

	pc -> input_buffer = create_buffer(max_channel_record_lines);
	pc -> input_buffer_cursor = 0;
	pc -> input_buffer_changed = FALSE;

	pc -> adding_names = TRUE;

	pc -> input = alloc_utf8_string();

	return channel_index;
}

BOOL redo_channel(void)
{
	channel *pc = NULL;
	int si = -1;

	if (undo_channels[N_CU - 1].data == NULL)
		return FALSE;

	si = find_server_index(undo_channels[N_CU - 1].s_name);
	if (si == -1)
		return FALSE;

	if (find_channel_index(si, undo_channels[N_CU - 1].data -> channel_name) == -1)
	{
		server_list[si].n_channels++;
		server_list[si].pchannels = realloc(server_list[si].pchannels, server_list[si].n_channels * sizeof(channel));

		pc = &server_list[si].pchannels[server_list[si].n_channels - 1];

		memcpy(pc, undo_channels[N_CU - 1].data, sizeof(channel));
	}

	free(undo_channels[N_CU - 1].data);
	myfree(undo_channels[N_CU - 1].s_name);

	memmove(&undo_channels[1], &undo_channels[0], (N_CU - 1) * sizeof(cu_t));
	memset(&undo_channels[0], 0x00, sizeof(cu_t));

	if (pc != NULL && is_channel(pc -> channel_name))
		return irc_join(server_list[si].fd, pc -> channel_name) != -1;

	return TRUE;
}

void close_channel(int server_index, int channel_index, BOOL leave_channel)
{
	channel *pc = NULL;

	/* if already membering an other channel, forget that one first */
	if (undo_channels[0].data)
	{
		free_channel(undo_channels[0].data);
		free(undo_channels[0].data);

		myfree(undo_channels[0].s_name);
	}

	pc = &server_list[server_index].pchannels[channel_index];

	/* rember this channel */
	memmove(&undo_channels[0], &undo_channels[1], (N_CU - 1) * sizeof(cu_t));

	undo_channels[N_CU - 1].data = (channel *)malloc(sizeof(channel));
	memcpy(undo_channels[N_CU - 1].data, pc, sizeof(channel));

	if (server_list[server_index].description)
		undo_channels[N_CU - 1].s_name = strdup(server_list[server_index].description);
	else
		undo_channels[N_CU - 1].s_name = strdup(server_list[server_index].server_host);

	/* remove channel from current channel-list */
	if (is_channel(pc -> channel_name) && leave_channel == TRUE)
		irc_part(server_list[server_index].fd, pc -> channel_name, part_message);

	if (channel_index < server_list[server_index].n_channels - 1)
	{
		memmove(&server_list[server_index].pchannels[channel_index],
			&server_list[server_index].pchannels[channel_index + 1],
			sizeof(channel) * (server_list[server_index].n_channels - (channel_index + 1)));
	}

	server_list[server_index].n_channels--;

	if (server_list[server_index].n_channels == 0)
	{
		free(server_list[server_index].pchannels);

		server_list[server_index].pchannels = NULL;
	}
}

void create_visible_channels_list(void)
{
	int sloop = 0;
	int old_server = -1, old_channel = -1;

	if (vc_list)
	{
		int offset = channel_offset + channel_cursor;

		if (offset >= vc_list -> n_channels && vc_list -> n_channels > 0)
			offset = vc_list -> n_channels - 1;

		old_server = vc_list -> server_index[offset];
		old_channel = vc_list -> channel_index[offset];
	}

	free_visible_channels_list();
	vc_list = malloc(sizeof(visible_channels));

	memset(vc_list, 0x00, sizeof(visible_channels));

	for(sloop=0; sloop<n_servers; sloop++)
	{
		server *ps = &server_list[sloop];
		int cloop;
		int n_to_show;

		/* only show main channel or all of them? */
		if (server_list[sloop].minimized)
			n_to_show = 1;	/* show at least the server channel */
		else
			n_to_show = ps -> n_channels;

		/* add this server to the list */
		for(cloop=0; cloop<n_to_show; cloop++)
		{
			channel *pc = &ps -> pchannels[cloop];

			if ((vc_list_data_only && pc -> new_entry != NONE) || !vc_list_data_only || cloop == 0 || (cloop == old_channel && sloop == old_server) || (cloop == current_server_channel_nr && sloop == current_server))
			{
				/* grow lists */
				vc_list -> server_index = (int *)realloc(vc_list -> server_index, sizeof(int) * (vc_list -> n_channels + 1));
				vc_list -> is_server_channel = (BOOL *)realloc(vc_list -> is_server_channel, sizeof(BOOL) * (vc_list -> n_channels + 1));
				vc_list -> channel_index = (int *)realloc(vc_list -> channel_index, sizeof(int) * (vc_list -> n_channels + 1));
				vc_list -> is_1on1_channel = (BOOL *)realloc(vc_list -> is_1on1_channel, sizeof(BOOL) * (vc_list -> n_channels + 1));

				vc_list -> is_server_channel[vc_list -> n_channels] = cloop == 0;

				vc_list -> is_1on1_channel[vc_list -> n_channels] = !is_channel(pc -> channel_name);

				vc_list -> server_index[vc_list -> n_channels] = sloop;
				vc_list -> channel_index[vc_list -> n_channels] = cloop;
				vc_list -> n_channels++;
			}
		}
	}
}

void show_channel_from_list(NEWWIN *win, int vc_index, int y)
{
	char mark = ' ';
	int loop;
	int textcolor = 0;
	int sc = -1;

	int server_nr = vc_list -> server_index[vc_index];
	int channel_nr = vc_list -> channel_index[vc_index];

	server *ps = &server_list[server_nr];
	channel *pc = &ps -> pchannels[channel_nr];

	if (theme.channellist_newlines_markchar != ' ')
	{
		mark = pc -> new_entry == NONE ? ' ' : theme.channellist_newlines_markchar;

		if (pc -> new_entry == YOU)
			textcolor = highlight_colorpair;
		else if (pc -> new_entry == META)
			textcolor = meta_colorpair;
		else
			textcolor = 0;
	}

	if (channel_nr == 0)
	{
		sc = get_server_color(server_nr);

		if (sc != -1)
			color_on(win, sc);
	}

	if (sc == -1 && textcolor && colors_meta)
		color_on(win, textcolor);

	for(loop=0; loop<win -> ncols; loop++)
		mvwprintw(win -> win, y, loop, " ");

	if (vc_list -> is_server_channel[vc_index])
	{
		/*if (server_list[server_nr].minimized)
			color_on(win, theme.color_channellist_minimized_server); */

		if (ps -> description)
			limit_print(win, win -> ncols - 0, y, 0, "%c%s", mark, ps -> description);
		else
			limit_print(win, win -> ncols - 0, y, 0, "%c%s", mark, ps -> server_host);

		/*if (server_list[server_nr].minimized)
			color_off(win, theme.color_channellist_minimized_server);*/
	}
	else
	{
		limit_print(win, win -> ncols - 1, y, 0, "%c %s", mark, pc -> channel_name);
	}

	if (sc != -1)
		color_off(win, sc);

	if (sc == -1 && textcolor && colors_meta)
		color_off(win, textcolor);
}

void show_channel_list(NEWWIN *channel_window)
{
	int channel_index = -1;

	create_visible_channels_list();

	werase(channel_window -> win);

	for(channel_index=channel_offset; channel_index<min(channel_offset + channel_window -> nlines, vc_list -> n_channels); channel_index++)
	{
		int y = channel_index - channel_offset;

		if (y == channel_cursor)
			mywattron(channel_window -> win, A_REVERSE);

		show_channel_from_list(channel_window, channel_index, y);

		if (y == channel_cursor)
			mywattroff(channel_window -> win, A_REVERSE);
	}
}

int find_channel_index(int cur_server, const char *channel_name)
{
	server *ps = &server_list[cur_server];
	int loop = 0;

	if (strcmp(channel_name, "AUTH") == 0)
		return 0;

	for(loop=0; loop<ps -> n_channels; loop++)
	{
		if (strcasecmp(ps -> pchannels[loop].channel_name, channel_name) == 0)
			return loop;
	}

	return -1;	/* new channel */
}

void set_new_line_received()
{
	if (get_cursor_mode() == CM_CHANNELS || get_cursor_mode() == CM_EDIT || get_cursor_mode() == CM_WC)
		show_channel_names_list();
}

int find_vc_list_entry(int server_index, int channel_index)
{
	int loop, channel_cursor = -1;

	for(loop=0; loop<vc_list -> n_channels; loop++)
	{
		if (vc_list -> server_index[loop] == server_index &&
		    vc_list -> channel_index[loop] == channel_index)
		{
			channel_cursor = loop;
			break;
		}
	}

	return channel_cursor;
}

int find_vc_list_entry_by_name(const char *name, int search_offset, BOOL match_server_channel)
{
	int loop;

	for(loop=0; loop<vc_list -> n_channels; loop++)
	{
		int cur = (search_offset + loop + 1) % vc_list -> n_channels;
		int si = vc_list -> server_index[cur];
		int ci = vc_list -> channel_index[cur];
		server *ps = &server_list[si];

		if (ci == 0 && match_server_channel == TRUE)
		{
			if (strcasestr(ps -> description, name))
				return cur;

			if (strcasestr(ps -> server_host, name))
				return cur;
		}
		else if (strcasestr(ps -> pchannels[ci].channel_name, name))
		{
			return cur;
		}
	}

	return -1;
}

BOOL change_channel(int server_index, int channel_index, BOOL change_cursor, BOOL push_history, BOOL allow_marker)
{
	server *ps = &server_list[server_index];
	channel *pc = &ps -> pchannels[channel_index];
	BOOL changed = FALSE;

	if (auto_markerline && allow_marker)
		add_markerline(current_server, current_server_channel_nr);

	/* remember last view timestamp */
	if (current_server != -1)
	{
		time_t now = time(NULL);

		server_list[server_index].last_view = now;

		if (current_server_channel_nr != -1 && current_server_channel_nr < ps -> n_channels)
			 pc -> last_view = now;
	}

	refresh_window_with_buffer(chat_window, max_y - 1, pc -> pbuffer, ps -> nickname, FALSE);

	werase(topic_line_window -> win);
	output_to_window(topic_line_window, str_or_nothing(pc -> topic), ps -> nickname, FALSE, NULL, TRUE, TRUE);

	pc -> topic_changed = FALSE;
	pc -> new_entry = NONE;

	names_offset = 0;
	names_cursor = 0;

	show_channel_names_list();

	if (current_server != server_index || current_server_channel_nr != channel_index)
		changed = TRUE;

	if (push_history)
		push_channel_history(current_server, current_server_channel_nr);

	current_server = server_index;
	current_server_channel_nr = channel_index;

	reset_topic_scroll_offset();

	update_channel_border(server_index);

	show_channel_names_list();

	/* find new channel cursor */
	if (change_cursor)
	{
		int dummy = find_vc_list_entry(server_index, channel_index);
		if (dummy != -1)
		{
			channel_cursor = dummy % channel_window -> nlines;
			channel_offset = dummy - channel_cursor;
		}
	}

	reposition_editline_cursor();

	return changed;
}

void show_channel_names_list(void)
{
	BOOL cm_duration = (time(NULL) - get_cursor_mode_since()) < 4;

	if (channel_window_border)
	{
		color_on(input_window_border, default_colorpair);

		if (get_cursor_mode() == CM_EDIT)
			mywattron(input_window_border -> win, A_REVERSE);
		else if (get_cursor_mode() == CM_NAMES || get_cursor_mode() == CM_CHANNELS)
			mywattron(channel_window_border -> win, A_REVERSE);

		if (cm_duration && get_cursor_mode() == CM_EDIT)
		{
			color_on(input_window_border, highlight_colorpair);
			mywattron(input_window_border -> win, A_BLINK);
		}

		mvwprintw(input_window_border -> win, 0, 0, ">");
		mvwprintw(input_window_border -> win, 0, input_window_border -> ncols - 1, "<");

		if (cm_duration && get_cursor_mode() == CM_EDIT)
		{
			color_off(input_window_border, highlight_colorpair);
			color_on(input_window_border, default_colorpair);
			mywattroff(input_window_border -> win, A_BLINK);
		}

		box(channel_window_border -> win, 0, 0);
#if 0
		color_on(channel_window_border, theme.channellist_border_color);
		wborder(channel_window_border -> win,
				theme.channellist_border_left_side, 
				theme.channellist_border_right_side, 
				theme.channellist_border_top_side, 
				theme.channellist_border_bottom_side, 
				theme.channellist_border_top_left_hand_corner, 
				theme.channellist_border_top_right_hand_corner, 
				theme.channellist_border_bottom_left_hand_corner, 
				theme.channellist_border_bottom_right_hand_corner
			);
		color_off(channel_window_border, theme.channellist_border_color);
#endif
		if (get_cursor_mode() == CM_EDIT)
			mywattroff(input_window_border -> win, A_REVERSE);
		else if (get_cursor_mode() == CM_NAMES || get_cursor_mode() == CM_CHANNELS)
			mywattroff(channel_window_border -> win, A_REVERSE);

		if (inverse_window_heading)
		{
			mywattron(channel_window_border -> win, A_STANDOUT);
			mvwprintw(channel_window_border -> win, 0, 1, get_cursor_mode() == CM_NAMES ? "Names" : "Channels");
			mywattroff(channel_window_border -> win, A_STANDOUT);
		}
		else
		{
			BOOL selected = get_cursor_mode() == CM_NAMES || get_cursor_mode() == CM_CHANNELS;

			if (selected)
				mywattron(channel_window_border -> win, A_STANDOUT);

			mvwprintw(channel_window_border -> win, 0, 1, get_cursor_mode() == CM_NAMES ? "[ Names ]" : "[Channels]");

			if (selected)
				mywattroff(channel_window_border -> win, A_STANDOUT);
		}
	}

	if (word_cloud_n > 0 && word_cloud_win_height > 0 && wc_window_border)
	{
		if (get_cursor_mode() == CM_WC)
			mywattron(wc_window_border -> win, A_REVERSE);
		else
			mywattroff(wc_window_border -> win, A_REVERSE);

		box(wc_window_border -> win, 0, 0);

		if (get_cursor_mode() == CM_WC)
			mywattroff(wc_window_border -> win, A_REVERSE);
	}

	if (get_cursor_mode() == CM_NAMES)
		show_name_list(current_server, current_server_channel_nr, channel_window);
	else
		show_channel_list(channel_window);

	put_word_cloud(get_cursor_mode() == CM_WC, TRUE);

	mydoupdate();
}

void go_to_last_channel(void)
{
	if (vc_list -> n_channels >= channel_window -> nlines)
	{
		channel_cursor = channel_window -> nlines - 1;
		channel_offset = vc_list -> n_channels - channel_window -> nlines;
	}
	else
	{
		channel_cursor = vc_list -> n_channels - 1;
		channel_offset = 0;
	}
}

void do_channels_keypress(int c)
{
	if (c == KEY_UP)
	{
		if (channel_cursor > 0)
			channel_cursor--;
		else if (channel_offset > 0)
			channel_offset--;
		else
			go_to_last_channel();
	}
	else if (c == KEY_END)
	{
		int diff = vc_list -> n_channels - (channel_offset + channel_cursor);

		if (diff >= channel_window -> nlines)
			go_to_last_channel();
		else
			channel_offset += diff;
	}
	else if (c == KEY_PPAGE)
	{
		if (channel_offset >= channel_window -> nlines)
			channel_offset -= channel_window -> nlines;
		else
		{
			channel_cursor = 0;
			channel_offset = 0;
		}
	}
	else if (c == KEY_DOWN)
	{
		if (channel_offset + channel_cursor < vc_list -> n_channels - 1)
		{
			if (channel_cursor < channel_window -> nlines - 1)
				channel_cursor++;
			else
				channel_offset++;
		}
		else
		{
			channel_cursor = 0;
			channel_offset = 0;
		}
	}
	else if (c == KEY_NPAGE)
	{
		if (channel_offset + channel_window -> nlines < vc_list -> n_channels - 1)
			channel_offset += channel_window -> nlines;
		else
		{
			channel_cursor = 0;
			channel_offset = vc_list -> n_channels - 1;
		}
	}
	else if (c == KEY_HOME)
	{
		channel_cursor = 0;
		channel_offset = 0;
	}
	else if (c == KEY_LEFT)
	{
		/* if channel_cursor is server, then minimized toggle else wrong_key(); */
		if (vc_list -> is_server_channel[channel_cursor + channel_offset] == TRUE)
		{
			if (server_list[vc_list -> server_index[channel_cursor + channel_offset]].minimized == FALSE)
			{
				server_list[vc_list -> server_index[channel_cursor + channel_offset]].minimized = TRUE;

				if (channel_cursor + channel_offset > vc_list -> n_channels)
					channel_offset = channel_cursor = 0;
			}
			else
			{
				wrong_key();
			}
		}
		else
		{
			wrong_key();
		}
	}
	else if (c == KEY_RIGHT)
	{
		int vc_list_offset = channel_cursor + channel_offset;
		BOOL is_server = vc_list -> is_server_channel[vc_list_offset];

		cur_channel() -> new_entry = NONE;

		if (current_server == vc_list -> server_index[vc_list_offset] &&
		    current_server_channel_nr == vc_list -> channel_index[vc_list_offset])
		{

			if (is_server)
			{
				if (cur_server() -> minimized == TRUE)
				{
					cur_server() -> minimized = FALSE;
					show_channel_names_list();
				}
				else
				{
					server_menu(current_server);
					create_visible_channels_list();
				}
			}
			else if (vc_list -> is_1on1_channel[vc_list_offset])
				user_channel_menu(current_server, server_list[current_server].pchannels[current_server_channel_nr].channel_name);
			else
				set_cursor_mode(CM_NAMES);
		}
		else
		{
			(void)change_channel(vc_list -> server_index[vc_list_offset], vc_list -> channel_index[vc_list_offset], FALSE, TRUE, TRUE);
		}
	}
}

void channelwindow_mouse(mmask_t buttons, int x, int y)
{
	static int prev_y_channels = -1;
	static int prev_y_names = -1;

	if (buttons & (BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED))
	{
		if (get_cursor_mode() == CM_CHANNELS)
		{
			if (channel_offset + y < vc_list -> n_channels)
			{
				if (y < channel_window -> nlines)
				{
					int vc_list_offset = y + channel_offset;

					(void)change_channel(vc_list -> server_index[vc_list_offset], vc_list -> channel_index[vc_list_offset], FALSE, TRUE, TRUE);

					channel_cursor = y;
				}
				else
				{
					go_to_last_channel();
				}

				if (prev_y_channels == y || (buttons & BUTTON1_DOUBLE_CLICKED))
				{
					prev_y_names = -1;

					set_cursor_mode(CM_NAMES);
				}
			}

			prev_y_channels = y;
		}
		else if (get_cursor_mode() == CM_NAMES)
		{
			if (names_offset + y < cur_channel() -> n_names)
			{
				if (y < channel_window -> nlines)
					names_cursor = y;
				else
					go_to_last_name();

				if (prev_y_names == y || (buttons & BUTTON1_DOUBLE_CLICKED))
				{
					if (user_menu(current_server, current_server_channel_nr, names_offset + y) == -1)
					{
						cur_server() -> state = STATE_DISCONNECTED;
						close(cur_server() -> fd);
						update_statusline(current_server, current_server_channel_nr, "Connection to %s:%d closed by other end (10)", cur_server() -> server_host, cur_server() -> server_port);
					}
				}
			}

			prev_y_names = y;
		}
		else
		{
			set_cursor_mode(CM_CHANNELS);

			prev_y_channels = -1;
			prev_y_names = -1;
		}
	}
	else if (buttons & BUTTON3_CLICKED)
	{
		set_cursor_mode(CM_CHANNELS);
	}
}

channel *gch(int sr, int ch)
{
	server *ps = &server_list[sr];

	return &ps -> pchannels[ch];
}
