/* GPLv2 applies
 * SVN revision: $Revision: 774 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <ncursesw/panel.h>
#include <ncursesw/ncurses.h>

#include "gen.h"
#include "utils.h"
#include "key_value.h"
#include "term.h"
#include "user.h"
#include "config.h"
#include "main.h"
#include "scrollback.h"

/* size of word cloud window and number of entries to show in it */
int word_cloud_n = 0, word_cloud_win_height = 5, word_cloud_min_word_size = 4;
/* interval for refreshing the window, in s */
int word_cloud_refresh = 5;
time_t word_cloud_last_refresh = 0, word_cloud_prev_time_update = 0, word_cloud_5s_update = 0;

NEWWIN *wc_window = NULL, *wc_window_border = NULL;
int wc_offset = 0, wc_cursor = 0;

key_value *global_wordcloud1 = NULL;
key_value *global_wordcloud2 = NULL;

char **wc_list = NULL;
int wc_list_n = 0, *wc_counts = NULL;

void init_wc(void)
{
	global_wordcloud1 = allocate_kv();
	global_wordcloud2 = allocate_kv();
}

void uninit_wc(void)
{
	free_kv(global_wordcloud1);
	free_kv(global_wordcloud2);
}

void add_to_wc_do(key_value *kv, const char *in)
{
	int loop = 0;
	string_array_t words;

	init_string_array(&words);

	split_string(in, " ", TRUE, &words);

	for(loop=0; loop<string_array_get_n(&words); loop++)
	{
		const char *word = string_array_get(&words, loop);
		int len = strlen(word), clean_loop = 0, out_index = 0;
		char *temp = (char *)calloc(1, len + 1);

		for(clean_loop=0; clean_loop<len; clean_loop++)
		{
			if (isalpha(word[clean_loop]))
				temp[out_index++] = word[clean_loop];
		}

		temp[out_index] = 0x00;

		if (out_index && out_index >= word_cloud_min_word_size)
		{
			const int *count = get_from_kv(kv, temp);
			int *new_count = calloc(1, sizeof(int));

			if (count)
				*new_count = *count + 1;
			else
				*new_count = 1;

			add_to_kv(kv, strdup(temp), new_count);
		}

		free(temp);
	}

	free_splitted_string(&words);
}

void add_to_wc(const char *in)
{
	add_to_wc_do(global_wordcloud1, in);
	add_to_wc_do(global_wordcloud2, in);
}

int value_cmp_int(const void *pv1, const void *pv2)
{
	int v1 = *(int *)pv1;
	int v2 = *(int *)pv2;

	return v1 - v2;
}

void get_top_n_keys_from_wc(key_value *kv, int get_n, char ***list, int *list_n, int **counts)
{
	int kv_size = get_n_kv_from_kv(kv);
	int out_n = min(get_n, kv_size), loop = 0;

	if (out_n == 0)
	{
		*list = NULL;
		*list_n = 0;
		return;
	}

	sort_kv(kv, FALSE, FALSE, value_cmp_int);

	*list = (char **)malloc(out_n * sizeof(char *));
	*counts = (int *)malloc(out_n * sizeof(int));

	for(loop=0; loop<out_n; loop++)
	{
		const char *k = get_key_by_index(kv, loop);
		int v = *(int *)get_value_by_index(kv, loop);

		(*list)[loop] = strdup(k);
		(*counts)[loop] = v;
	}

	*list_n = out_n;

	sort_kv(kv, TRUE, TRUE, NULL);
}

void refresh_wc_window(void)
{
	int wc_index = 0;

	werase(wc_window -> win);

	for(wc_index=wc_offset; wc_index<min(wc_offset + wc_window -> nlines, wc_list_n); wc_index++)
	{
		int y = wc_index - wc_offset;

		if (y == wc_cursor)
			mywattron(wc_window -> win, A_REVERSE);

		limit_print(wc_window, wc_window -> ncols, y, 0, "%s (%d)", wc_list[wc_index], wc_counts[wc_index]);

		if (y == wc_cursor)
			mywattroff(wc_window -> win, A_REVERSE);
	}

	mydoupdate();
}

void put_word_cloud(BOOL selected, BOOL force_redraw)
{
	if (word_cloud_n > 0 && wc_window_border)
	{
		time_t now = time(NULL);
		int time_passed = now - word_cloud_last_refresh;
		int time_left = word_cloud_refresh - time_passed;

		if (now - word_cloud_prev_time_update >= 1 || force_redraw)
		{
			if (inverse_window_heading)
			{
				mywattron(wc_window_border -> win, A_STANDOUT);
				mvwprintw(wc_window_border -> win, 0, 1, "Word cloud");
				mvwprintw(wc_window_border -> win, wc_window_border -> nlines - 1, 1, "%03ds left", time_left);
				mywattroff(wc_window_border -> win, A_STANDOUT);
			}
			else
			{
				if (selected)
					mywattron(wc_window_border -> win, A_STANDOUT);

				mvwprintw(wc_window_border -> win, 0, 1, "[Word cloud]");
				mvwprintw(wc_window_border -> win, wc_window_border -> nlines - 1, 1, "[ %03ds left ]", time_left);

				if (selected)
					mywattroff(wc_window_border -> win, A_STANDOUT);
			}

			word_cloud_prev_time_update = now;
		}

		if (time_left <= 0 || now - word_cloud_5s_update >= 5 || force_redraw)
		{
			char **new_list = NULL;
			int *new_counts = NULL;
			int new_list_n = 0;

			if (time_left <= 0)
			{
				get_top_n_keys_from_wc(global_wordcloud2, word_cloud_n, &new_list, &new_list_n, &new_counts);
				truncate_kv(global_wordcloud1);
			}
			else
			{
				get_top_n_keys_from_wc(global_wordcloud1, word_cloud_n, &new_list, &new_list_n, &new_counts);
			}

			if (new_list_n)
			{
				int loop = 0;

				for(loop=0; loop<wc_list_n; loop++)
					free(wc_list[loop]);

				free(wc_list);
				free(wc_counts);

				wc_list = new_list;
				wc_counts = new_counts;
				wc_list_n = new_list_n;

				refresh_wc_window();
			}

			if (time_left <= 0)
				word_cloud_last_refresh = now;

			word_cloud_5s_update = now;

			if (time_left <= 0 || wc_offset + wc_cursor >= wc_list_n)
			{
				wc_offset = 0;
				wc_cursor = 0;
			}
		}

		if (time_left <= 0)
		{
			key_value *temp = global_wordcloud1;

			global_wordcloud1 = global_wordcloud2;

			global_wordcloud2 = temp;
		}
	}
}

void search_in_wc(const char *what)
{
	global_search(what, what);
}

void go_to_last_wc(void)
{
	if (wc_list_n >= wc_window -> nlines)
	{
		wc_cursor = wc_window -> nlines - 1;
		wc_offset = wc_list_n - wc_window -> nlines;
	}
	else
	{
		wc_cursor = wc_list_n - 1;
		wc_offset = 0;
	}
}

void do_word_cloud_keypress(int c)
{
	/* extern char **wc_list = NULL; */
	/* extern int wc_list_n = 0, *wc_counts = NULL; */
	if (c == KEY_UP)
	{
		if (wc_cursor > 0)
			wc_cursor--;
		else if (wc_offset > 0)
			wc_offset--;
		else
			go_to_last_wc();
	}
	else if (c == KEY_END)
	{
		go_to_last_wc();
	}
	else if (c == KEY_PPAGE)
	{
		if (wc_offset >= wc_window -> nlines)
		{
			wc_cursor = 0;
			wc_offset -= wc_window -> nlines;
		}
		else
		{
			wc_cursor = 0;
			wc_offset = 0;
		}
	}
	else if (c == KEY_DOWN)
	{
		if (wc_offset + wc_cursor < wc_list_n - 1)
		{
			if (wc_cursor < wc_window -> nlines - 1)
				wc_cursor++;
			else
				wc_offset++;
		}
		else
		{
			wc_cursor = 0;
			wc_offset = 0;
		}
	}
	else if (c == KEY_NPAGE)
	{
		if (wc_offset + wc_window -> nlines < wc_list_n)
		{
			wc_cursor = 0;
			wc_offset += wc_window -> nlines;
		}
		else
		{
			wc_cursor = 0;
			wc_offset = wc_list_n - 1;
		}
	}
	else if (c == KEY_HOME)
	{
		wc_cursor = 0;
		wc_offset = 0;
	}
	else if (c == KEY_RIGHT && wc_offset + wc_cursor < wc_list_n)
	{
		int ar_index = wc_offset + wc_cursor;
		char *temp = strdup(wc_list[ar_index]);

		search_in_wc(temp);

		free(temp);
	}

	refresh_wc_window();
}

void wordcloud_mouse(mmask_t buttons, int x, int y)
{
	if (get_cursor_mode() != CM_WC)
		set_cursor_mode(CM_WC);
	else if (buttons & BUTTON1_CLICKED)
	{
		if (wc_offset + y < wc_list_n)
		{
			int ar_index = wc_offset + y;
			char *temp = strdup(wc_list[ar_index]);

			search_in_wc(temp);

			free(temp);
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

void apply_show_wordcloud(void)
{
	if (word_cloud_n == 0)
	{
		delete_window(wc_window);
		wc_window = NULL;

		delete_window(wc_window_border);
		wc_window_border = NULL;
	}
}
