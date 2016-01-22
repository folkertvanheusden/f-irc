/* GPLv2 applies
 * SVN revision: $Revision: 882 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#include "buffer.h"
#include "string_array.h"

typedef enum { YES=0, NO=1, ABORT=2 } yna_reply_t;

void keypress_visual_feedback(void);
void cmd_LEAVE(int current_server, int current_server_channel_nr, const char *channel_name);
int user_command(int current_server, int current_server_channel_nr, const char *user_line, BOOL do_command);
int user_menu(int server_index, int channel_index, int name_index);
void refresh_window_with_buffer(NEWWIN *where, const int window_height, buffer *pbuffer, const char *hl, BOOL force_partial_highlight);
void server_menu(int sr);
void popup_notify(BOOL use_getch, const char *format, ...);
void add_server_menu(void);
int user_channel_menu(int sr, const char *user);
BOOL onoff_box(const char *q, BOOL default_value);
yna_reply_t yesno_box(BOOL use_getch, const char *title, const char *q, BOOL allow_abort);
BOOL configure_firc(void);
void close_notice_channels(void);
BOOL edit_string_array(string_array_t *p, const char *title);
void edit_dictionary(void);
const char *edit_box(int width, const char *title, const char *initial);
void edit_scripts(void);
