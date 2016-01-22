/* GPLv2 applies
 * SVN revision: $Revision: 864 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#ifndef __SERVER_H__
#define __SERVER_H__

#include <poll.h>

#include "tcp.h"
#include "string_array.h"
#include "lf_buffer.h"
#include "channels.h"

#define DEFAULT_RECONNECT_DELAY 44
#define DEFAULT_MAX_RECONNECT_DELAY 512

typedef enum { STATE_NO_CONNECTION = 0, STATE_ERROR, STATE_TCP_CONNECT, STATE_IRC_CONNECTING, STATE_CONNECTED1, STATE_CONNECTED2, STATE_LOGGING_IN, STATE_RUNNING, STATE_DISCONNECTED } conn_state_t;

typedef struct
{
	const char *channel, *topic;
} channel_topic_t;

int compare_channel_list_item(const void *a, const void *b);

typedef struct
{
	const char *server_host;
	char *server_real;
	int server_port;
	const char *description;

	resolve_info ri;
	int reconnect_delay;

	channel_topic_t *channel_list;
	int channel_list_n;
	BOOL channel_list_complete;

	string_array_t send_after_login;
	char must_send_after_login;

	string_array_t auto_join;

	char *user_complete_name;
	char *username;
	char *password;
	char *nickname, *nickname2;

	int fd;
	int ifd;

	lf_buffer_t io_buffer;

	char *user_ping;
	double t_user_ping;
	int user_ping_id;

	conn_state_t state;
	time_t state_since;

	double sent_time_req_ts, server_latency;
	BOOL hide_time_req;

	/* channel[0] = server channel */
	channel *pchannels;
	int n_channels;
	/* wether the complete list of channels is shown or not */
	BOOL minimized;

	time_t ts_last_action;
	time_t ts_last_msg;
	time_t last_view;

	double t_event;
	int n_event;

	int prev_bps;
	time_t ts_bytes;
	long int bytes;

	char *prev_cmd;
} server;

void free_server(int server_index);
void toggle_server_minimized(int toggle_index);
int find_server_index(const char *server_name);
int add_server(const char *host_and_port, const char *username, const char *password, const char *nickname, const char *complete_name, const char *description);
void close_server(int server_index, BOOL leave_channels);
void create_default_server(void);
void set_state(int server_index, conn_state_t state);
conn_state_t get_state(int server_index);
int get_server_color(int server_index);
const char *gen_random_nick();
void restart_server(int sr);
int register_server_events(struct pollfd **pfd, int *n_fd);
void process_server_events(struct pollfd *pfd, int n_fd);
int find_in_autojoin(int sr, const char *channel_name);
void remove_autojoin(int sr, int aj_nr);
void add_autojoin(int server_index, char *channel_name);
void server_set_additional_nick(int sr, const char *n2);
void free_channel_list(int sr);
void sort_channels(int sr);
server *gsr(int sr);

#endif
