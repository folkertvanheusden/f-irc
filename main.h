/* GPLv2 applies
 * SVN revision: $Revision: 746 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#ifndef __MAIN_H__
#define __MAIN_H__

#include "channels.h"
#include "servers.h"

extern unsigned int ul_x;
extern volatile BOOL terminal_changed;
extern time_t started_at;
extern string_array_t extra_highlights;

void reposition_editline_cursor(void);
void exit_fi(void);
void do_resize(int s);
server *cur_server(void);
channel *cur_channel(void);
void set_cursor_mode(cursor_mode_t cm);
cursor_mode_t get_cursor_mode(void);
time_t get_cursor_mode_since(void);

#endif
