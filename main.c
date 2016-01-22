/* GPLv2 applies
 * SVN revision: $Revision: 886 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#ifndef AIX
#include <sys/termios.h> /* needed on Solaris 8 */
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <ncursesw/ncurses.h>
#include <ncursesw/panel.h>

#include "gen.h"
#include "error.h"
#include "theme.h"
#include "term.h"
#include "buffer.h"
#include "channels.h"
#include "utils.h"
#include "servers.h"
#include "loop.h"
#include "user.h"
#include "config.h"
#include "utf8.h"
#include "key_value.h"
#include "grep_filter.h"
#include "wordcloud.h"
#include "dcc.h"
#include "nickcolor.h"
#include "chistory.h"
#include "autocomplete.h"
#include "checkmail.h"
#include "names.h"
#include "ignores.h"
#include "colors.h"
#include "dictionary.h"
#include "headlines.h"
#include "help.h"
#include "scrollback.h"

char *jump_channel = NULL;

/* editline */
unsigned int ul_str_pos = 0, ul_x = 0;
BOOL editline_redraw = FALSE;
/* channel window */
cursor_mode_t cursor_mode = CM_CHANNELS;
time_t cursor_mode_since = 0;
/* redraw terminal when resizing */
volatile BOOL terminal_changed = FALSE;

string_array_t extra_highlights;

utf8_string *input_line_undo = NULL;

time_t started_at = 0;

void set_cursor_mode(cursor_mode_t cm)
{
	cursor_mode = cm;
	cursor_mode_since = time(NULL);
}

cursor_mode_t get_cursor_mode(void)
{
	return cursor_mode;
}

time_t get_cursor_mode_since(void)
{
	return cursor_mode_since;
}

server *cur_server(void)
{
	return &server_list[current_server];
}

channel *cur_channel(void)
{
	return &server_list[current_server].pchannels[current_server_channel_nr];
}

/* move to the end of an editline */
void reposition_editline_cursor(void)
{
	char *dummy = utf8_get_ascii(cur_channel() -> input);

	if (current_server >= 0 && current_server_channel_nr >= 0 && n_servers > 0)
	{
		int str_len = utf8_strlen(cur_channel() -> input);

		ul_x = str_len % input_window_width;
		ul_str_pos = str_len - ul_x;

		/* have the line redrawn */
		editline_redraw = TRUE;
	}
	else
	{
		ul_x = ul_str_pos = 0;
	}

	free(dummy);
}

void inputline_mouse(mmask_t buttons, int x, int y)
{
	if ((buttons & BUTTON1_CLICKED) || (buttons & BUTTON1_DOUBLE_CLICKED))
	{
		if (get_cursor_mode() == CM_EDIT)
		{
			int len = utf8_strlen(cur_channel() -> input);

			if (x + ul_str_pos < len)
				ul_x = x;
			else
				reposition_editline_cursor();
		}
		else
		{
			set_cursor_mode(CM_EDIT);
		}
	}

	if (buttons & BUTTON3_CLICKED)
	{
		select_own_history_line(FALSE);

		reposition_editline_cursor();
	}
}

void update_input_buffer()
{
	if (cur_channel() -> input_buffer_changed)
	{
		const char *ul_asc = utf8_get_utf8(cur_channel() -> input);

		add_to_buffer(cur_channel() -> input_buffer, ul_asc, cur_server() -> nickname, FALSE, current_server, current_server_channel_nr);

		myfree((void *)ul_asc);
	}

	utf8_truncate(cur_channel() -> input, 0);

	if (cur_channel() -> input_buffer_cursor < get_buffer_n_elements(cur_channel() -> input_buffer))
		utf8_strcat_ascii(cur_channel() -> input, get_from_buffer(cur_channel() -> input_buffer, cur_channel() -> input_buffer_cursor) -> msg);

	cur_channel() -> input_buffer_changed = FALSE;

	reposition_editline_cursor();
}

void edit_line_keypress(c)
{
	if (c == KEY_LEFT)
	{
		if (ul_x > 0)
		{
			ul_x--;                        
		}
		else if (ul_str_pos > 0)
		{
			ul_str_pos--;
		}
		else
		{
			wrong_key();
		}
	}
	else if (c == KEY_RIGHT)
	{
		int len = utf8_strlen(cur_channel() -> input);

		if (ul_x + ul_str_pos < len)
		{
			if (ul_x < input_window_width - 1)
				ul_x++;
			else
				ul_str_pos++;
		}
		else
		{
			wrong_key();
		}
	}
	else if (c == KEY_UP)
	{
		if (cur_channel() -> input_buffer_cursor)
		{
			cur_channel() -> input_buffer_cursor--;

			update_input_buffer();

			editline_redraw = TRUE;
		}
		else
		{
			wrong_key();
		}
	}
	else if (c == KEY_HOME)
	{
		if (cur_channel() -> input_buffer_cursor)
		{
			cur_channel() -> input_buffer_cursor = 0;

			update_input_buffer();

			editline_redraw = TRUE;
		}
		else
		{
			wrong_key();
		}
	}
	else if (c == KEY_DOWN)
	{
		if (cur_channel() -> input_buffer_cursor < get_buffer_n_elements(cur_channel() -> input_buffer))
		{
			cur_channel() -> input_buffer_cursor++;

			update_input_buffer();
		}
		else
		{
			wrong_key();
		}

		editline_redraw = TRUE;
	}
	else if (c == KEY_END)
	{
		if (cur_channel() -> input_buffer_cursor != get_buffer_n_elements(cur_channel() -> input_buffer) - 1)
		{
			cur_channel() -> input_buffer_cursor = get_buffer_n_elements(cur_channel() -> input_buffer) - 1;

			update_input_buffer();

			editline_redraw = TRUE;
		}
		else
		{
			wrong_key();
		}
	}
}

void jump_next_favorite(void)
{
	if (n_favorite_channels)
	{
		int attempts = 0;

		for(attempts=0; attempts<n_favorite_channels; attempts++)
		{
			favorite *pf = &favorite_channels[favorite_channels_index];
			int s_i = -1, c_i = -1;

			find_server_channel_index(pf -> server, pf -> channel, &s_i, &c_i);

			if (s_i != -1 && c_i != -1 && (s_i != current_server || c_i != current_server_channel_nr))
			{
				change_channel(s_i, c_i, TRUE, TRUE, TRUE);
				break;
			}

			if (c_i == -1)
			{
				LOG("channel %s %s not found\n", pf -> server, pf -> channel);
				wrong_key();
			}

			favorite_channels_index = (favorite_channels_index + 1) % n_favorite_channels;
		}
	}
	else
	{
		LOG("no favorites defined\n");
		wrong_key();
	}
}

void redraw()
{
	determine_terminal_size();

	if (ERR == resizeterm(max_y, max_x))
		error_exit(TRUE, "problem resizing terminal\n");

	wresize(stdscr, max_y, max_x);

	endwin();
	refresh();

	wclear(stdscr);

	create_windows();

	refresh_window_with_buffer(chat_window, max_y - 1, cur_channel() -> pbuffer, cur_server() -> nickname, FALSE);

	change_channel(current_server, current_server_channel_nr, TRUE, FALSE, TRUE);

	create_visible_channels_list();

	update_headline(TRUE);
}

void redraw_chat_window_only()
{
	werase(chat_window -> win);

	refresh_window_with_buffer(chat_window, max_y - 1, cur_channel() -> pbuffer, cur_server() -> nickname, FALSE);
}

void reset_new_data(void)
{
	int s;

	for(s=0; s<n_servers; s++)
	{
		int c;

		for(c=0; c<server_list[s].n_channels; c++)
			server_list[s].pchannels[c].new_entry = NONE;
	}
}

void do_resize(int s)
{
	terminal_changed = TRUE;
}

void find_next_channel_with_data(BOOL me_only, BOOL forwards)
{
	int prev_index = channel_offset + channel_cursor;
	int found = -1;

	/* start at 1: no need to check the current channel */
	if (forwards == TRUE)
	{
		int loop;
		for(loop=1; loop<vc_list -> n_channels; loop++)
		{
			int check_index = (prev_index + loop) % vc_list -> n_channels;

			int check_server = vc_list -> server_index[check_index];
			int check_channel = vc_list -> channel_index[check_index];

			new_entry_t ne = server_list[check_server].pchannels[check_channel].new_entry;

			if (ne == NONE/* || ne == META*/)
				continue;

			if (me_only == TRUE && ne != YOU)
				continue;

			found = check_index;
			break;
		}
	}
	else
	{
		int loop;

		for(loop=1; loop<vc_list -> n_channels; loop++)
		{
			int check_index = prev_index - loop;
			int check_server = -1, check_channel = -1;
			new_entry_t ne = NONE;

			while(check_index < 0)
				check_index += vc_list -> n_channels;

			check_server = vc_list -> server_index[check_index];
			check_channel = vc_list -> channel_index[check_index];

			ne = server_list[check_server].pchannels[check_channel].new_entry;

			if (ne == NONE /*|| ne == META*/)
				continue;

			if (me_only == TRUE && ne != YOU)
				continue;

			found = check_index;
			break;
		}
	}

	if (found == -1)
		wrong_key();
	else
	{
		int check_server = vc_list -> server_index[found];
		int check_channel = vc_list -> channel_index[found];

		set_cursor_mode(CM_CHANNELS);

		if (jumpy_navigation)
			change_channel(check_server, check_channel, TRUE, TRUE, TRUE);
		else
		{
			int diff = found - prev_index;

			channel_cursor += diff;

			if (channel_cursor < 0)
			{
				channel_offset += channel_cursor;
				channel_cursor = 0;
			}
			else if (channel_cursor >= channel_window -> nlines)
			{
				channel_offset += channel_cursor - (channel_window -> nlines - 1);
				channel_cursor = channel_window -> nlines - 1;
			}

			change_channel(check_server, check_channel, FALSE, TRUE, TRUE);
		}
	}
}

void add_char(int c)
{
	unsigned int len = utf8_strlen(cur_channel() -> input);

	if (len >= LINE_LENGTH)
	{
		wrong_key();
	}
	else
	{
		int cur_combined_pos = ul_str_pos + ul_x;
		BOOL added = TRUE;

		if (cur_combined_pos > len)
			error_exit(FALSE, "position too large %d[%d], cur: %d[%d], %d",
				cur_combined_pos, len,
				current_server, n_servers,
				current_server_channel_nr);

		/* cursor at end of cur_channel() -> input? */
		if (cur_combined_pos == len)
			added = add_stream_to_utf8_string(cur_channel() -> input, c);
		else /* add character to somewhere IN the cur_channel() -> input */
			utf8_insert_pos_ascii(cur_channel() -> input, cur_combined_pos, c);

		cur_channel() -> input_buffer_changed = TRUE;

		editline_redraw = TRUE;

		if (!added) /* UTF8 stream */
		{
		}
		else if (cur_combined_pos < LINE_LENGTH)
		{
			if (ul_x < input_window_width - 1)
				ul_x++;
			else
				ul_str_pos++;
		}
		else
		{
			wrong_key();
		}
	}
}

void add_escape_char()
{
	int c = -1;
	NEWWIN *bwin = NULL, *win = NULL;

	create_win_border(21, 3, "^V", &bwin, &win, FALSE);

	mvwprintw(win -> win, 1, 1, "Press key to escape");
	mydoupdate();

	c = wait_for_keypress(FALSE);

	if (c == 21) /* ^U -> ^_ */
		c = 31;

	delete_window(win);
	delete_window(bwin);

	if (c != 0x00)
		add_char(c);
	else
		wrong_key();
}

void draw_editline()
{
	werase(input_window -> win);

	if (utf8_strlen(cur_channel() -> input) > 0)
	{
		wchar_t *part = utf8_get_wchar_pos(cur_channel() -> input, ul_str_pos);
		int len = min(wcslen(part), input_window_width);
		int index = 0;

		for(index=0; index<len; index++)
		{
			wchar_t temp[2] = { 0 };

			temp[0] = part[index];
			temp[1] = 0x00;

			if (temp[0] < 32)
			{
				mywattron(input_window -> win, A_REVERSE);
				/* FIXME handle > 26 */
				mvwaddch(input_window -> win, 0, index, temp[0] + 'A' - 1);
				mywattroff(input_window -> win, A_REVERSE);
			}
			else
			{
				mvwaddwstr(input_window -> win, 0, index, temp);
			}
		}

		myfree(part);
	}
}

void exit_fi(void)
{
	char *cnf_save_msg = NULL;
	int loop = 0;

	if (input_line_undo)
		free_utf8_string(input_line_undo);

	if (store_config_on_exit)
	{
		char *err_msg = NULL;

		if (save_config(TRUE, &err_msg) == FALSE)
			asprintf(&cnf_save_msg, "\n\nProblem: %s\n\n\n", err_msg);
		else
			asprintf(&cnf_save_msg, "\n\nUpdated configuration file (%s)\n\n\n", err_msg);

		free(err_msg);
	}

	for(loop=0; loop<n_servers; loop++)
		close_server(loop, TRUE);

	myfree(server_list);

	delete_window(input_window);
	delete_window(input_window_border);

	delete_window(chat_window);
	delete_window(chat_window_border);

	delete_window(channel_window);
	delete_window(channel_window_border);

	delete_window(topic_line_window);

	delete_window(headline_window);

	delete_window(wc_window);
	delete_window(wc_window_border);

	endwin();

	free_colors();

	/* wordcloud */
	for(loop=0; loop<wc_list_n; loop++)
		free(wc_list[loop]);

	free(wc_list);
	free(wc_counts);

	uninit_wc();
	/*...*/

	free_visible_channels_list();

	free_dcc();

	myfree(jump_channel);
	myfree(theme_file);
	myfree(part_message);
	myfree(server_exit_message);
	myfree(ignore_file);
	myfree(conf_file);

	free_grep_filters(gp);
	free_grep_filters(hlgp);

	free_ignores();
	free_favorites();

	free_channel_history();

	free_headlines();

	free_string_array(&extra_highlights);

	if (cnf_save_msg)
	{
		printf("%s\n", cnf_save_msg);

		free(cnf_save_msg);
	}

	exit(0);
}

void test_colors(void);

int main(int argc, char *argv[])
{
	int config_loaded = -1;

	started_at = time(NULL);

	memset(undo_channels, 0x00, sizeof(undo_channels));

	/*setlocale(LC_ALL,"C-UTF-8");*/
	setlocale(LC_CTYPE, "");

	memset(&theme, 0x00, sizeof(theme));

	srand(time(NULL));
	srand48(time(NULL));

	theme.channellist_window_width = 15;
	theme.channellist_border = TRUE;
	theme.chat_window_border = TRUE;
	theme.show_clock = TRUE;
	theme.show_time = TRUE;
	theme.start_in_channellist_window = TRUE;
	theme.channellist_newlines_markchar = '*';
	theme.show_date_when_changed = TRUE;

	gp = alloc_grep_target();
	hlgp = alloc_grep_target();

	init_channel_history(10);

	init_dcc();

	init_ncurses(ignore_mouse);

	init_headlines();

	init_string_array(&extra_highlights);

	/*test_colors();*/

	init_nick_coloring(DJB2);

	if (argc == 2)
	{
		config_loaded = load_config(argv[1]);

		if (config_loaded == -1)
		{
			if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)
			{
				commandline_help();

				exit_fi();
			}
			else
			{
				char *msg = NULL;

				asprintf(&msg, "File %s was not found; continue?", argv[1]);

				if (yesno_box(TRUE, "Configuration file not found", msg, FALSE) == NO)
					exit_fi();

				free(msg);
			}
		}
	}
	else
	{
		/* first try to load config from homedirectory */
		char *home_file = NULL;
		const char *dummy = getenv("HOME");

		if (dummy)
		{
			asprintf(&home_file, "%s/.firc", dummy);

			config_loaded = load_config(home_file);
		}

		if (config_loaded == -1) /* if it's not there, load from system's settings dir */
		{
			char *conf_path = NULL;

			asprintf(&conf_path, "%s/firc.conf", SYSCONFDIR);
			config_loaded = load_config(conf_path);

			if (conf_file == NULL)
				conf_file = strdup(conf_path);

			myfree(conf_path);
		}

		if (config_loaded == -1) /* if it's still not there, load from current directory */
			config_loaded = load_config("firc.conf");

		if (config_loaded == -1)
			conf_file = home_file;
		else
			myfree(home_file);
	}

	create_default_server();

	init_wc();

	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);

	if (theme.start_in_channellist_window)
		set_cursor_mode(CM_CHANNELS);
	else
		set_cursor_mode(CM_EDIT);

	create_windows();

        /* initialise mailcheckstuff */
	init_check_mail();

	change_channel(0, 0, TRUE, TRUE, FALSE);

	for(;;)
	{
		unsigned int ul_prev_str_pos = ul_str_pos;
		int prev_channel_pos = channel_cursor + channel_offset;
		int prev_names_pos = names_cursor + names_offset;
		BOOL force_channel_win_redraw = FALSE;
		int c = wait_for_keypress(FALSE);
		BOOL is_cursor_key = c == KEY_LEFT || c == KEY_RIGHT || c == KEY_UP || c == KEY_DOWN || c == KEY_NPAGE || c == KEY_PPAGE || c == KEY_HOME || c == KEY_END;

		if (current_server < 0 || current_server >= n_servers || current_server_channel_nr >= cur_server() -> n_channels || current_server_channel_nr < 0)
			error_exit(FALSE, "p: %d %d\n", current_server, current_server_channel_nr);

		if (c == 8 || c == KEY_F(1))	/* F1: Help */
		{
			main_help();
		}
		else if (c == KEY_MOUSE)
		{
			MEVENT event;
			int x = 0, y = 0;

			if (getmouse(&event) == OK)
			{
				x = event.x;
				y = event.y;

				if ((chat_window_border && is_in_window(chat_window_border, event.x, event.y)) || (chat_window && is_in_window(chat_window, event.x, event.y)))
				{
					show_channel_history();
				}
				else if (input_window && is_in_window(input_window, event.x, event.y))
				{
					wmouse_trafo(input_window -> win, &y, &x, FALSE);

					inputline_mouse(event.bstate, x, y);
				}
				else if (input_window_border && is_in_window(input_window_border, event.x, event.y))
					set_cursor_mode(CM_EDIT);
				else if (channel_window && is_in_window(channel_window, event.x, event.y))
				{
					wmouse_trafo(channel_window -> win, &y, &x, FALSE);

					channelwindow_mouse(event.bstate, x, y);
				}
				else if (channel_window_border && is_in_window(channel_window_border, event.x, event.y))
					set_cursor_mode(CM_CHANNELS);
				else if (wc_window && is_in_window(wc_window, event.x, event.y))
				{
					wmouse_trafo(wc_window -> win, &y, &x, FALSE);

					wordcloud_mouse(event.bstate, x, y);
				}
				else if (wc_window_border && is_in_window(wc_window_border, event.x, event.y))
					set_cursor_mode(CM_WC);
				else
				{
					wrong_key();
				}

				editline_redraw = TRUE;
				force_channel_win_redraw = TRUE;
			}
		}
		else if (c == KEY_F(2))		/* F2: save config */
		{
			char *err_msg = NULL;

			if (!save_ignore_list())
				popup_notify(FALSE, "Problem saving ignore-list file");

			(void)save_config(TRUE, &err_msg);
			popup_notify(FALSE, "%s", err_msg);

			free(err_msg);
		}
		else if (c == KEY_F(3))		/* F3: edit scripts for current channel */
			edit_scripts();
		else if (c == KEY_F(4))				/* F4 */
		{
			set_cursor_mode(CM_EDIT);
		}
		else if (c == -1 || c == KEY_F(5))	/* F5: terminal resize / redraw */
		{
			redraw();

			terminal_changed = FALSE;

			editline_redraw = TRUE;
		}
		else if (c == KEY_F(6))		/* F6: search in all channels */
		{
			search_everywhere();
		}
		else if (c == KEY_F(7))		/* F7: close notice channels */
		{
			close_notice_channels();
		}
		else if (c == KEY_F(8))		/* F8: edit configuration */
		{
			if (configure_firc())
				redraw();
		}
		else if (c == KEY_F(9))		/* F9: undo channel close */
		{
			if (!redo_channel())
				wrong_key();
			else
				force_channel_win_redraw = TRUE;
		}
		else if (c == 14 || c == KEY_F(10)) 		/* ^N / F10 */
		{
			if (get_cursor_mode() == CM_CHANNELS)
				set_cursor_mode(CM_EDIT);
			else if (get_cursor_mode() == CM_NAMES)
				set_cursor_mode(CM_EDIT);
			else if (get_cursor_mode() == CM_EDIT && word_cloud_n > 0)
				set_cursor_mode(CM_WC);
			else
				set_cursor_mode(CM_CHANNELS);

			if (get_cursor_mode() == CM_EDIT)
				cur_channel() -> input_buffer_cursor = get_buffer_n_elements(cur_channel() -> input_buffer);

			force_channel_win_redraw = TRUE;
		}
		else if (c == KEY_F(12))
			add_markerline_to_all();
		else if (c == KEY_RESIZE)	/* handled by signal */
		{
		}
		else if (c == 23)		/* ^W search next channel with data */
			find_next_channel_with_data(FALSE, TRUE);
		else if (c == 7) /* ^G: close channel */
		{
			yna_reply_t rc = ABORT;
			int nr = find_in_autojoin(current_server, server_list[current_server].pchannels[current_server_channel_nr].channel_name);

			if (nr != -1)
			{
				rc = yesno_box(FALSE, "Close channel", "Remove from auto-join list as well?", TRUE);

				if (rc == YES)
					remove_autojoin(current_server, nr);
			}
			else
			{
				rc = YES;
			}

			if (rc != ABORT)
				cmd_LEAVE(current_server, current_server_channel_nr, NULL);

			reposition_editline_cursor();
		}
		else if (c == 3)	/* ^C */
		{
			if (yesno_box(FALSE, "Terminate f-irc", "Are you sure you want to terminate the program?", FALSE) == YES)
				break;
		}
		else if (c == 20)	/* ^T */
		{
			ch_t prev = pop_channel_history();

			if (prev.s != -1 && prev.c != -1)
				change_channel(prev.s, prev.c, TRUE, FALSE, TRUE);
			else
				wrong_key();
		}
		else if (c == 17)	/* ^Q */
			jump_next_favorite();
		else if (is_cursor_key && get_cursor_mode() == CM_WC)
			do_word_cloud_keypress(c);
		else if (is_cursor_key && get_cursor_mode() == CM_NAMES)
		{
			do_names_keypress(c);

			force_channel_win_redraw = TRUE;
		}
		else if (is_cursor_key && get_cursor_mode() == CM_CHANNELS)
		{
			do_channels_keypress(c);

			force_channel_win_redraw = TRUE;
		}
		else if (is_cursor_key && get_cursor_mode() == CM_EDIT)
		{
			edit_line_keypress(c);
		}
		else if (c == 4) /* ^D */
		{
			int str_offset = ul_str_pos + ul_x;

			if (utf8_strlen(cur_channel() -> input) - str_offset > 0)
			{
				utf8_del_pos(cur_channel() -> input, str_offset);

				cur_channel() -> input_buffer_changed = TRUE;
			}
			else
			{
				wrong_key();
			}

			editline_redraw = TRUE;
		}
		else if (c == KEY_BACKSPACE || c == 127) /* BACKSPACE/DEL */
		{
			int str_offset = ul_str_pos + ul_x;

			if (str_offset > 0)
			{
				utf8_del_pos(cur_channel() -> input, str_offset - 1);

				if (ul_x > 0)
					ul_x--;
				else
					ul_str_pos--;

				editline_redraw = TRUE;

				cur_channel() -> input_buffer_changed = TRUE;
			}
			else
			{
				wrong_key();
			}
		}
		else if (c == 9 && utf8_strlen(cur_channel() -> input) > 0)		/* TAB */
		{
			unsigned int x = ul_str_pos + ul_x;

			/* find first non-space before the cursor */
			x = utf8_find_nonblank_reverse(cur_channel() -> input, x);

			if (x != -1)
			{
				int space_offset = -1, word_offset = -1;
				char *temp_str = NULL;
				const char *dummy = NULL;

				/* then find the last space (if any) before the cursor */
				space_offset = utf8_find_blank_reverse(cur_channel() -> input, x);

				if (space_offset == -1)
					word_offset = 0;
				else
					word_offset = space_offset + 1;

				temp_str = utf8_get_ascii_pos(cur_channel() -> input, word_offset);

				if (temp_str[0] == '/')
					dummy = make_complete_command(temp_str);
				else
				{
					dummy = make_complete_nickorchannel(temp_str, word_offset == 0);

					if (!dummy && dictionary_file)
					{
						dummy = lookup_dictionary(temp_str);

						if (dummy)
							dummy = strdup(dummy);
					}
				}

				if (dummy)
				{
					utf8_truncate(cur_channel() -> input, word_offset);

					utf8_strcat_ascii(cur_channel() -> input, dummy);

					reposition_editline_cursor();

					editline_redraw = TRUE;
				}
				else
				{
					wrong_key();
				}

				myfree(dummy);

				myfree(temp_str);
			}
		}
		else if (c == 18)		/* ^R search next channel with data backwares */
			find_next_channel_with_data(FALSE, FALSE);
		else if (c == 26)		/* ^Z personal msg */
			find_next_channel_with_data(TRUE, TRUE);
		else if (c == 24)		/* ^X personal msg, backwards */
			find_next_channel_with_data(TRUE, FALSE);
		else if (c == 16)		/* ^P */
		{
			add_markerline(current_server, current_server_channel_nr);

			if (only_one_markerline)
				redraw_chat_window_only();
		}
		else if (c == 10)		/* ^J */
		{
			if (jump_channel == NULL)
				wrong_key();
			else
			{
				int vc_list_offset = channel_cursor + channel_offset;

				int vc_nr = find_vc_list_entry_by_name(jump_channel, vc_list_offset, FALSE);
				if (vc_nr == -1)
					vc_nr = find_vc_list_entry_by_name(jump_channel, vc_list_offset, TRUE);

				if (vc_nr == -1)
					wrong_key();
				else
				{
					int s = vc_list -> server_index[vc_nr];
					int c = vc_list -> channel_index[vc_nr];

					change_channel(s, c, TRUE, TRUE, TRUE);
				}
			}
		}
		else if (c == 13)		/* RETURN */
		{
			if (utf8_strlen(cur_channel() -> input) > 0)
			{
				BOOL do_line = TRUE, do_commands = TRUE;

				if (utf8_ascii_get_at(cur_channel() -> input, 0) == '@' && utf8_strlen(cur_channel() -> input) >= 2)
				{
					if (utf8_ascii_get_at(cur_channel() -> input, 1) == '/' || utf8_ascii_get_at(cur_channel() -> input, 1) == '@')
					{
						/* strlen without -1 to include the terminating 0x00 */
						utf8_del_pos(cur_channel() -> input, 0);
						do_commands = FALSE;
					}
					else
					{
						const char *c_part = utf8_get_ascii_pos(cur_channel() -> input, 1);
						int vc_nr = find_vc_list_entry_by_name(c_part, channel_cursor + channel_offset, FALSE);
						if (vc_nr == -1)
							vc_nr = find_vc_list_entry_by_name(c_part, channel_cursor + channel_offset, TRUE);

						utf8_truncate(cur_channel() -> input, 0);

						ul_x = ul_str_pos = 0;

						do_line = FALSE;

						if (vc_nr == -1)
							wrong_key();
						else
						{
							int s = vc_list -> server_index[vc_nr];
							int c = vc_list -> channel_index[vc_nr];

							myfree(jump_channel);
							jump_channel = strdup(c_part);

							change_channel(s, c, TRUE, TRUE, TRUE);
						}

						myfree((void *)c_part);
					}
				}

				if (do_line)
				{
					const char *ul_asc = utf8_get_utf8(cur_channel() -> input);

					if (user_command(current_server, current_server_channel_nr, ul_asc, do_commands) == -1)
					{
						cur_server() -> state = STATE_DISCONNECTED;
						close(cur_server() -> fd);
						update_statusline(current_server, current_server_channel_nr, "Connection to %s:%d closed by other end (6)", cur_server() -> server_host, cur_server() -> server_port);
					}
					else
					{
						add_to_buffer(cur_channel() -> input_buffer, ul_asc, cur_server() -> nickname, FALSE, current_server, current_server_channel_nr);
					}

					utf8_truncate(cur_channel() -> input, 0);

					ul_x = ul_str_pos = 0;

					myfree((void *)ul_asc);
				}

				editline_redraw = TRUE;

				werase(input_window -> win);
			}
			else
			{
				wrong_key();
			}

			if (get_cursor_mode() != CM_CHANNELS)
			{
				set_cursor_mode(CM_CHANNELS);

				force_channel_win_redraw = TRUE;
			}
		}
		else if (c == 1 || c == KEY_HOME)	/* ^A / HOME */
			ul_str_pos = ul_x = 0;
		else if (c == 5 || c == KEY_END)	/* ^E / END */
			reposition_editline_cursor();
		else if (c == 21)			/* ^U */
		{
			if (utf8_strlen(cur_channel() -> input) == 0) /* redo */
			{
				if (!input_line_undo)
					wrong_key();
				else
				{
					utf8_strcat_utf8_string(cur_channel() -> input, input_line_undo);

					free_utf8_string(input_line_undo);
					input_line_undo = NULL;

					reposition_editline_cursor();
				}
			}
			else
			{
				if (input_line_undo != NULL)
					free_utf8_string(input_line_undo);

				input_line_undo = utf8_strdup(cur_channel() -> input);

				utf8_truncate(cur_channel() -> input, 0);

				ul_x = ul_str_pos = 0;
			}

			editline_redraw = TRUE;
		}
		else if (c == 15) /* ^O reset new data */
		{
			reset_new_data();

			force_channel_win_redraw = TRUE;
		}
		else if (c == 2 || c == 19) /* ^B scrollback inputlines, ^S in all */
		{
			select_own_history_line(c == 19);

			reposition_editline_cursor();

			editline_redraw = TRUE;
		}
		else if (c == 6) /* ^F scrollback channel */
		{
			show_channel_history();

			/* because of ^B functionality: */
			reposition_editline_cursor();

			editline_redraw = TRUE;
		}
		else if (c == 22)	/* ^V escape char */
			add_escape_char();
		else if (c == 25)	/* ^Y with-data only toggle */
		{
			if (vc_list_data_only == FALSE)
			{
				if (yesno_box(FALSE, "Channels to show", "Show only channels with messages?", FALSE) == YES)
					vc_list_data_only = TRUE;
			}
			else
			{
				vc_list_data_only = FALSE;
			}

			show_channel_names_list();
		}
		/* 255: because ncurses has values > 255 for certain keys */
		else if (c >= 32 && c <= 255)
		{
			add_char(c);
		}
		else
		{
			LOG("invalid key: %d\n", c);
		}

		/* at this point the use may have changed channel/server so re-get the pointers */

		/* redraw channel cursor (if moved) */
		if (channel_cursor + channel_offset != prev_channel_pos || names_cursor + names_offset != prev_names_pos || force_channel_win_redraw)
			show_channel_names_list();

                if (ul_str_pos != ul_prev_str_pos || editline_redraw)
		{
			draw_editline();

			editline_redraw = FALSE;
		}

                mydoupdate();
	}

	exit_fi();

	return 0;
}
