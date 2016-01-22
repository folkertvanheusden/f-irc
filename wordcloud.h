/* GPLv2 applies
 * SVN revision: $Revision: 744 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#ifndef __WORDCLOUD_H__
#define __WORDCLOUD_H__

#include "key_value.h"

extern NEWWIN *wc_window, *wc_window_border;
extern int word_cloud_n, word_cloud_win_height, word_cloud_refresh, word_cloud_min_word_size;
extern time_t word_cloud_last_refresh;
extern int wc_offset, wc_cursor;
extern char **wc_list;
extern int wc_list_n, *wc_counts;

void init_wc(void);
void uninit_wc(void);
void add_to_wc(const char *in);
void get_top_n_keys_from_wc(key_value *kv, int get_n, char ***list, int *list_n, int **counts);
void put_word_cloud(BOOL win_selected, BOOL force_redraw);
void refresh_wc_window(void);
void search_in_wc(const char *what);
void go_to_last_wc(void);
void do_word_cloud_keypress(int c);
void wordcloud_mouse(mmask_t buttons, int x, int y);
void apply_show_wordcloud(void);

#endif
