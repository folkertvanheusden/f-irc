/* GPLv2 applies
 * SVN revision: $Revision: 887 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#ifndef __LOOP_H__
#define __LOOP_H__

#include "nickcolor.h"
#include "servers.h"
#include "term.h"

extern NEWWIN *channel_window_border, *chat_window_border, *input_window_border;
extern NEWWIN *channel_window, *chat_window, *input_window, *topic_line_window, *headline_window;
extern int current_server, current_server_channel_nr;
extern server *server_list;
extern int n_servers;
extern visible_channels *vc_list;
extern int input_window_width;
extern char *time_str_fmt;

void output_to_window(NEWWIN *win, const char *string, const char *match, be_type_t line_type, nick_color_settings *pncs, BOOL force_partial_highlight, BOOL fit);
int log_channel(int server_nr, int channel_nr, const char *user, const char *string, BOOL gen_hl);
void update_statusline(int server_nr, int channel_nr, const char *format, ...);
void create_windows();
int wait_for_keypress(BOOL one_event);
void update_channel_border(int server_nr);
void reset_topic_scroll_offset(void);
int log_to_file(int sr, int ch, const char *nick, const char *msg);
void gen_display_markerline(NEWWIN *win, time_t ts);
void add_markerline(int sr, int ch);
void add_markerline_to_all(void);

#endif
