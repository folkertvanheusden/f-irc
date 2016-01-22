/* GPLv2 applies
 * SVN revision: $Revision: 886 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#ifndef __CHANNELS_H__
#define __CHANNELS_H__

#include "utf8.h"
#include "term.h"
#include "buffer.h"

extern int channel_offset, channel_cursor;

typedef struct
{
	const char *server;
	const char *channel;
} favorite;

typedef enum { MODE_NONE = 0, MODE_OPERATOR = 1, MODE_CAN_SPEAK = 2 } irc_user_mode_t;

typedef struct
{
	char *nick, *complete_name, *user_host;
	irc_user_mode_t mode;
	BOOL ignored;
} person_t;

typedef enum { NONE=1, META=2, MISC=3, YOU=4 } new_entry_t;

typedef struct
{
	const char *filename;
	int wfd, rfd;
	pid_t pid;
} script_instances_t;

typedef struct
{
	char *channel_name;
	char *topic;
	char *keeptopic;
	BOOL topic_changed;

	/* buffer with received messages */
	buffer *pbuffer;

	/* messages/commands entered by user */
	buffer *input_buffer;
	utf8_string *input;
	int input_buffer_cursor;
	BOOL input_buffer_changed;

	script_instances_t *scripts;
	int n_scripts;

	BOOL recvd_non_notice;

	new_entry_t new_entry;

	person_t *persons;
	int n_names;
	BOOL adding_names;	/* set to FALSE when a 366 is received.
				 * a (new) 353 will then first free the
				 * current list
				 */

	time_t last_view;
	time_t last_entry;

	double t_event;
	int n_event;
} channel;

typedef struct
{
	int *server_index;
	BOOL *is_server_channel;	/* when set, server_host is shown instead of channelname */
	int *channel_index;
	BOOL *is_1on1_channel;

	int n_channels;
} visible_channels;

#define N_CU 10
typedef struct
{
	channel *data;
	const char *s_name;
} cu_t;

extern cu_t undo_channels[N_CU];

extern BOOL vc_list_data_only;

void free_channel(channel *pc);
int add_channel(int server_index, const char *channel_name);
void close_channel(int server_index, int channel_index, BOOL leave_channel);
void show_channel_from_list(NEWWIN *win, int vc_index, int y);
void show_channel_list(NEWWIN *win);
int find_channel_index(int cur_server, const char *channel_name);
void find_server_channel_index(const char *server_name, const char *channel_name, int *s_i, int *c_i);
void set_new_line_received();
BOOL change_channel(int server_index, int channel_index, BOOL reset_cursor, BOOL push_history, BOOL allow_markerline);
void show_channel_names_list(void);
void go_to_last_channel(void);
void do_channels_keypress(int c);
BOOL redo_channel(void);
void channelwindow_mouse(mmask_t buttons, int x, int y);
channel *gch(int sr, int ch);

void create_visible_channels_list(void);
void free_visible_channels_list(void);
int find_vc_list_entry(int server_index, int channel_index);
int find_vc_list_entry_by_name(const char *name, int search_offset, BOOL match_server_channel);

#endif
