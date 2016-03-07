/* GPLv2 applies
 * SVN revision: $Revision: 856 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "gen.h"
#include "buffer.h"
#include "term.h"
#include "user.h"
#include "utils.h"
#include "scrollback.h"
#include "loop.h"
#include "config.h"
#include "main.h"
#include "theme.h"
#include "xclip.h"
#include "help.h"

void search_in_buffer_popup(const buffer *pb, const char *highlight)
{
	NEWWIN *bwin = NULL, *win = NULL;
	const char *what = edit_box(60, "^Search...^", NULL);
	char *title = NULL;
	buffer *search_results = NULL;

	if (!what)
		return; /* user selected cancel */

	if (strlen(what) == 0)
	{
		myfree(what);
		return; /* user pressed enter without nothing entered */
	}

	search_results = search_in_buffer_new(pb, what, FALSE);

	asprintf(&title, "Press left cursor key to exit | Searched %s", what);

	create_win_border(max_x - 4, max_y - 4, title, &bwin, &win, TRUE);

	scrollback(win, search_results, what, FALSE, TRUE);

	delete_window(win);
	delete_window(bwin);

	mydoupdate();

	myfree(title);

	free_buffer(search_results);

	myfree(what);
}

void search_everywhere(void)
{
	NEWWIN *bwin = NULL, *win = NULL;
	const char *what = edit_box(60, "^Search all ...^", NULL);
	char *title = NULL;
	int sr = 0;
	buffer *search_results = NULL;

	if (!what || strlen(what) == 0)
	{
		myfree(what);
		return; /* user selected cancel or enter with nothing entered */
	}

	search_results = create_buffer(0);

	for(sr=0; sr<n_servers; sr++)
	{
		server *ps = &server_list[sr];
		int ch = 0;

		for(ch=0; ch<ps -> n_channels; ch++)
		{
			channel *pc = &ps -> pchannels[ch];

			search_in_buffer(pc -> pbuffer, search_results, what, TRUE);
		}
	}

	asprintf(&title, "Press left cursor key to exit | Searched for %s", what);

	create_win_border(max_x - 4, max_y - 4, title, &bwin, &win, TRUE);

	scrollback(win, search_results, what, FALSE, TRUE);

	delete_window(win);
	delete_window(bwin);

	mydoupdate();

	myfree(title);

	free_buffer(search_results);

	myfree(what);
}

void generate_line(buffer_element_t *line, BOOL with_server, BOOL with_channel, int max_width, char **out, BOOL *did_fit)
{
	if (with_channel)
	{
		const char *c_name = server_list[line -> sr].pchannels[line -> ch].channel_name;

		if (with_server)
		{
			const char *s_name = server_list[line -> sr].description;
			if (s_name == NULL)
				s_name = server_list[line -> sr].server_host;

			asprintf(out,  "%s|%s] %s", s_name, c_name, line -> msg);
		}
		else
		{
			asprintf(out,  "%s] %s", c_name, line -> msg);
		}
	}
	else
	{
		*out = strdup(line -> msg);
	}

	*did_fit = TRUE;

	if (max_width > 0 && strlen(*out) > max_width)
	{
		*did_fit = FALSE;
		(*out)[max_width - 1] = 0x00;
	}
}

void copy_to_clipboard(const buffer *pb, BOOL with_channel)
{
	char *data = NULL;
	int loop = 0, len_out = 0;

	for(loop=0; loop<get_buffer_n_elements(pb); loop++)
	{
		buffer_element_t *line = get_from_buffer(pb, loop);
		char *msg = NULL;
		BOOL fit = FALSE;
		int len = 0;

		generate_line(line, n_servers > 1, with_channel, -1, &msg, &fit);

		len = strlen(msg);
		data = (char *)realloc(data, len_out + len + 1);
		memcpy(&data[len_out], msg, len + 1);

		len_out += len;

		free(msg);
	}

	if (data)
	{
		if (!file_exists(xclip))
			popup_notify(FALSE, "Cannot copy to clipboard\nRequired binary\n%s\ndoes not exist. See firc.conf\n", xclip);
		else if (getenv("DISPLAY") == NULL)
			popup_notify(FALSE, "Cannot copy to clipboard\n\"DISPLAY\" environment variable is not set.\nThis variable is normally set by the X server.\n");
		else
			send_to_xclip(data);

		free(data);
	}
}

void write_buffer_to_file(const buffer *pb, BOOL with_channel)
{
	const char *file = edit_box(60, "^Select file to write to...^", NULL);

	if (file)
	{
		FILE *fh = fopen(file, "wb");
		if (!fh)
			popup_notify(FALSE, "Failed to create file: %s", strerror(errno));
		else
		{
			BOOL err = FALSE;
			int loop = 0;

			for(loop=0; loop<get_buffer_n_elements(pb); loop++)
			{
				buffer_element_t *line = get_from_buffer(pb, loop);
				char *msg = NULL;
				BOOL fit = FALSE;

				generate_line(line, n_servers > 1, with_channel, -1, &msg, &fit);

				err |= fprintf(fh, "%s\n", msg) < 1;

				free(msg);
			}

			fclose(fh);

			if (err)
				popup_notify(FALSE, "An error occured writing to file %s", file);
		}

		myfree(file);
	}
}

void scrollback_displayline(NEWWIN *win, buffer_element_t *line, const int terminal_offset, const char *highlight, BOOL with_channel, int max_width, BOOL force_partial_highlight)
{
	nick_color_settings ncs;
	char *out = NULL;
	BOOL fit = FALSE;

	generate_line(line, n_servers > 1, with_channel, max_width, &out, &fit);

	wmove(win -> win, terminal_offset, 0);

	if (line -> line_type == BET_MARKERLINE)
		gen_display_markerline(win, line -> when);
	else
	{
		find_nick_colorpair(line -> msg_from, &ncs);

		output_to_window(win, out, highlight, line -> line_type, nick_color ? &ncs : NULL, force_partial_highlight, fit);
	}

	myfree(out);
}

void global_search(const char *highlight, const char *search_what)
{
	NEWWIN *bwin = NULL, *win = NULL;
	const char *what = search_what ? strdup(search_what) : edit_box(60, "^Global search...^", NULL);
	char *title = NULL;
	buffer *search_results = create_buffer(2500);
	int s = 0, c = 0;

	if (!what)
	{
		free_buffer(search_results);

		return; /* user selected cancel */
	}

	if (strlen(what) == 0)
	{
		myfree(what);

		free_buffer(search_results);

		return; /* user pressed enter without nothing entered */
	}

	asprintf(&title, "Press left cursor key to exit | Global search for %s", what);

	create_win_border(max_x - 4, max_y - 4, title, &bwin, &win, TRUE);

	for(s=0; s<n_servers; s++)
	{
		for(c=0; c<server_list[s].n_channels; c++)
			search_in_buffer(server_list[s].pchannels[c].pbuffer, search_results, what, TRUE);
	}

	sort_buffer(search_results, TRUE);

	scrollback(win, search_results, highlight, TRUE, TRUE);

	delete_window(win);
	delete_window(bwin);
	mydoupdate();

	myfree(title);

	free_buffer(search_results);

	myfree(what);
}

void scrollback_new(const buffer *pbuffer, const char *highlight, const char *title_in, BOOL with_channel, BOOL force_partial_highlight)
{
	char *title = NULL;
	NEWWIN *bwin = NULL, *win = NULL;

	asprintf(&title, "Press left cursor key to exit | %s", title_in);

	create_win_border(max_x - 4, max_y - 4, title, &bwin, &win, TRUE);

	scrollback(win, pbuffer, highlight, with_channel, force_partial_highlight);

	delete_window(win);
	delete_window(bwin);

	mydoupdate();

	myfree(title);
}

int calc_line_height(buffer_element_t *pbe, int width)
{
	if (pbe -> line_type == BET_MARKERLINE)
		return 1;

	return (strlen(pbe -> msg) + width) / width;
}

void replace_input_buffer(const char *selected_line)
{
	if (utf8_strlen(cur_channel() -> input) > 0)
	{
		char *temp = utf8_get_utf8(cur_channel() -> input);

		add_to_buffer(cur_channel() -> input_buffer, temp, cur_server() -> nickname, FALSE, current_server, current_server_channel_nr);

		myfree(temp);
	}

	utf8_truncate(cur_channel() -> input, 0);
	utf8_strcat_ascii(cur_channel() -> input, selected_line);
}

void scrollback(NEWWIN *win, const buffer *pbuffer, const char *highlight, BOOL with_channel, BOOL force_partial_highlight)
{
	int buffer_size = get_buffer_n_elements(pbuffer);
	int offset = buffer_size - 1, n_lines = 0;
	if (offset < 0)
		offset = 0;

	for(;;)
	{
		int cur_n_lines = -1;

		if (offset == 0)
			break;

		cur_n_lines = calc_line_height(get_from_buffer(pbuffer, offset - 1), win -> ncols);

		if (n_lines + cur_n_lines >= win -> nlines)
			break;

		n_lines += cur_n_lines;
		offset--;
	}

	for(;;)
	{
		int c = 0;
		int index = 0, lines_used = 0;

		werase(win -> win);

		for(;offset + index < get_buffer_n_elements(pbuffer);)
		{
			int prev_lines_used = lines_used, lines_left = win -> nlines - lines_used;
			buffer_element_t *pbe = get_from_buffer(pbuffer, offset + index);

			if (lines_used >= win -> nlines)
				break;

			lines_used += calc_line_height(pbe, win -> ncols);

			scrollback_displayline(win, pbe, prev_lines_used, highlight, with_channel, win -> ncols * lines_left, force_partial_highlight);

			index++;
		}

		if (lines_used < win -> nlines)
			simple_marker(win);

		mydoupdate();

		c = wait_for_keypress(FALSE);

		if (toupper(c) == 'Q' || toupper(c) == 'X')
			break;
		else if (c == KEY_LEFT || (c == KEY_MOUSE && right_mouse_button_clicked()))
			break;
		else if (c == KEY_F(1))
			scrollback_help();
		else if (c == 2)	/* ^B scrollback & select */
		{
			char *selected_line = NULL;
			int sr = -1, ch = -1;
			NEWWIN *bwin = NULL, *win = NULL;

			create_win_border(max_x - 6, max_y - 4, "right cursor key: select, left: cancel", &bwin, &win, TRUE);

			if (scrollback_and_select(win, pbuffer, &selected_line, &sr, &ch, cur_server() -> nickname, with_channel, force_partial_highlight))
			{
				replace_input_buffer(selected_line);

				free(selected_line);
			}

			delete_window(win);
			delete_window(bwin);

			break;
		}
		else if (c == 3)
			exit_fi();
		else if (c == 'w') /* write to file */
			write_buffer_to_file(pbuffer, with_channel);
		else if (c == 'c') /* copy to clipboard */
		{
			keypress_visual_feedback();
			copy_to_clipboard(pbuffer, with_channel);
			keypress_visual_feedback();
		}
		else if (c == '/') /* search */
			search_in_buffer_popup(pbuffer, highlight);
		else if (c == KEY_RIGHT)
			wrong_key();
		else if (c == KEY_UP && offset > 0)
			offset--;
		else if (c == KEY_DOWN && offset < get_buffer_n_elements(pbuffer) - 1)
			offset++;
		else if (c == KEY_NPAGE && offset < get_buffer_n_elements(pbuffer) - 1)
		{
			int dummy = 0;

			while(offset < get_buffer_n_elements(pbuffer) - 1)
			{
				offset++;

				dummy += calc_line_height(get_from_buffer(pbuffer, offset), win -> ncols);

				if (dummy > win -> nlines)
				{
					offset--;

					break;
				}
			}
		}
		else if (c == KEY_PPAGE && offset > 0)
		{
			int dummy = 0;
			while(offset > 0)
			{
				offset--;

				dummy += calc_line_height(get_from_buffer(pbuffer, offset), win -> ncols);

				if (dummy > win -> nlines)
				{
					offset++;

					break;
				}
			}
		}
		else if (c == KEY_HOME && offset > 0)
			offset = 0;
		else if (c == KEY_END && offset < get_buffer_n_elements(pbuffer) - 1)
			offset = get_buffer_n_elements(pbuffer) - 1;
		else if (c == 'm' && offset > 0)
		{
			BOOL found = false;
			int temp_offset = offset - 1;

			for(;;)
			{
				if (get_from_buffer(pbuffer, temp_offset) -> line_type == BET_MARKERLINE)
				{
					offset = temp_offset;
					found = TRUE;
					break;
				}

				if (temp_offset <= 0)
					break;

				temp_offset--;
			}

			if (!found)
				wrong_key();
		}
		else
		{
			wrong_key();
		}
	}
}

BOOL scrollback_and_select(NEWWIN *win, const buffer *pbuffer, char **sel_str, int *sr, int *ch, const char *needle, BOOL with_channel, BOOL force_partial_highlight)
{
	int ppos = -1;
	int offset = 0, cursor = 0;
	const char *search_for = NULL;

	for(;;)
	{
		int c = 0;

		/* redraw */
		if (offset + cursor != ppos)
		{
			int loop = 0, end_at = min(get_buffer_n_elements(pbuffer) - offset, win -> nlines);

			werase(win -> win);

			for(loop=0; loop < min(get_buffer_n_elements(pbuffer) - offset, win -> nlines); loop++)
			{
				int index = offset + loop;

				if (loop == cursor)
					mywattron(win -> win, A_REVERSE);

				if (index & 1)
					mywattron(win -> win, A_BOLD);

				scrollback_displayline(win, get_from_buffer(pbuffer, index), loop, needle, with_channel, win -> ncols, force_partial_highlight);

				if (index & 1)
					mywattroff(win -> win, A_BOLD);

				if (loop == cursor)
					mywattroff(win -> win, A_REVERSE);
			}

			if (end_at < win -> nlines)
				simple_marker(win);

			mydoupdate();

			ppos = offset + cursor;
		}

		c = wait_for_keypress(FALSE);

		if (tolower(c) == 'q')
			return FALSE;
		else if (c == KEY_LEFT || (c == KEY_MOUSE && right_mouse_button_clicked()))
			return FALSE;
		else if (c == KEY_RIGHT || c == 13)
		{
			if (offset + cursor < get_buffer_n_elements(pbuffer))
			{
				buffer_element_t *pbe = get_from_buffer(pbuffer, offset + cursor);

				*sel_str = strdup(pbe -> msg);
				*sr = pbe -> sr;
				*ch = pbe -> ch;

				return TRUE;
			}

			wrong_key();
		}
		else if (c == KEY_F(1))
			scrollback_and_select_help();
		else if (c == 'w') /* write to file */
			write_buffer_to_file(pbuffer, TRUE);
		else if (c == '/') /* search */
		{
			const char *what = edit_box(60, "^Search...^", NULL);

			myfree(search_for);
			search_for = NULL;

			if (what)
			{
				int new_index = search_in_buffer_index(pbuffer, what, offset + cursor + 1);

				if (new_index != -1)
				{
					offset = new_index;
					cursor = 0;

					search_for = what;
				}
				else
				{
					wrong_key();
				}
			}
		}
		else if (c == 'n')
		{
			if (search_for)
			{
				int new_index = search_in_buffer_index(pbuffer, search_for, offset + cursor + 1);

				if (new_index != -1)
				{
					offset = new_index;
					cursor = 0;
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
		else if (c == KEY_HOME)
		{
			offset = cursor = 0;
		}
		else if (c == KEY_END)
		{
			int n_lines = get_buffer_n_elements(pbuffer);

			if (n_lines >= win -> nlines)
			{
				cursor = win -> nlines - 1;
				offset = n_lines - win -> nlines;
			}
			else
			{
				cursor = n_lines - 1;
				offset = 0;
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
			if (cursor + offset < get_buffer_n_elements(pbuffer) - 1)
			{
				if (cursor < win -> nlines - 1)
					cursor++;
				else
					offset++;
			}
			else
			{
				wrong_key();
			}
		}
		else if (c == KEY_PPAGE)
		{
			if (cursor)
				cursor = 0;
			else if (offset >= win -> nlines -1)
				offset -= win -> nlines - 1;
			else
				wrong_key();
		}
		else if (c == KEY_NPAGE)
		{
			if (cursor + offset < get_buffer_n_elements(pbuffer) - (win -> nlines - 1))
				offset += win -> nlines - 1;
			else
				wrong_key();
		}
		else if (c == 3)
		{
			exit_fi();
		}
		else
		{
			wrong_key();
		}
	}

	myfree(search_for);

	return FALSE;
}

void show_channel_history(void)
{
	NEWWIN *bwin = NULL, *win = NULL;
	create_win_border(max_x - (theme.channellist_window_width + 2), max_y - 4, "Press left cursor key to exit", &bwin, &win, TRUE);

	scrollback(win, cur_channel() -> pbuffer, cur_server() -> nickname, FALSE, FALSE);

	delete_window(win);
	delete_window(bwin);
}

void select_own_history_line(BOOL search_in_all)
{
	char *selected_line = NULL;
	int sr = -1, ch = -1;
	NEWWIN *bwin = NULL, *win = NULL;
	BOOL rc = FALSE;

	create_win_border(max_x - 6, max_y - 4, "right cursor key: select, left: cancel", &bwin, &win, TRUE);

	if (search_in_all)
	{
		buffer *temp = create_buffer(0);
		int sr=0;

		for(sr=0; sr<n_servers; sr++)
		{
			server *ps = &server_list[sr];
			int ch=0;

			for(ch=0; ch<ps -> n_channels; ch++)
				add_buffer_to_buffer(temp, ps -> pchannels[ch].input_buffer);
		}

		sort_buffer(temp, TRUE);

		rc = scrollback_and_select(win, temp, &selected_line, &sr, &ch, cur_server() -> nickname, TRUE, FALSE);

		free_buffer(temp);
	}
	else
	{
		rc = scrollback_and_select(win, cur_channel() -> input_buffer, &selected_line, &sr, &ch, cur_server() -> nickname, FALSE, FALSE);
	}

	if (rc)
	{
		replace_input_buffer(selected_line);

		myfree(selected_line);
	}

	delete_window(win);
	delete_window(bwin);
}
