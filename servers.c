/* GPLv2 applies
 * SVN revision: $Revision: 885 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "error.h"
#include "gen.h"
#include "utils.h"
#include "term.h"
#include "user.h"
#include "channels.h"
#include "servers.h"
#include "irc.h"
#include "loop.h"
#include "config.h"
#include "tcp.h"
#include "utils.h"
#include "string_array.h"

void free_server(int server_index)
{
	server *pc = &server_list[server_index];
	int loop;

	if (pc -> state >= STATE_IRC_CONNECTING)
		close(pc -> fd);

	myfree(pc -> server_host);
	myfree(pc -> server_real);
	myfree(pc -> nickname);
	myfree(pc -> nickname2);
	myfree(pc -> description);
	myfree(pc -> username);
	myfree(pc -> password);
	myfree(pc -> user_complete_name);

	myfree(pc -> prev_cmd);

	free_resolve_info(&pc -> ri);

	free_channel_list(server_index);

	for(loop=0; loop<pc -> n_channels; loop++)
		free_channel(&pc -> pchannels[loop]);

	free_string_array(&pc -> send_after_login);

	free_string_array(&pc -> auto_join);

	myfree(pc -> pchannels);

	free_lf_buffer(&pc -> io_buffer);
}

void close_server(int server_index, BOOL leave_channel)
{
	LOG("close server %d\n", server_list[server_index].fd);

	set_state(server_index, STATE_DISCONNECTED);

	irc_quit(server_list[server_index].fd, server_exit_message);

	if (leave_channel)
	{
		int loop;

		for(loop=server_list[server_index].n_channels - 1; loop>-1; loop--)
			close_channel(server_index, loop, leave_channel);
	}

	close(server_list[server_index].fd);
	server_list[server_index].fd = -1;
}

void toggle_server_minimized(int toggle_index)
{
	server_list[vc_list -> server_index[toggle_index]].minimized = ! server_list[vc_list -> server_index[toggle_index]].minimized;
}

int find_server_index(const char *server_name)
{
	int loop;

	for(loop=0; loop<n_servers; loop++)
	{
		server *p = &server_list[loop];

		if (p -> description && strcasecmp(p -> description, server_name) == 0)
			return loop;

		if (strcasecmp(p -> server_host, server_name) == 0)
			return loop;
	}

	return -1;
}

void find_server_channel_index(const char *server_name, const char *channel_name, int *s_i, int *c_i)
{
	int si = -1;

	*s_i = *c_i = -1;

	for(si=0; si<n_servers; si++)
	{
		BOOL check = server_name == NULL;
		int ci = -1;
		server *p = &server_list[si];

		if (server_name && ((p -> description && strcasecmp(p -> description, server_name) == 0) || strcasecmp(p -> server_host, server_name) == 0))
			check = TRUE;

		if (check)
		{
			ci = find_channel_index(si, channel_name);

			if (ci != -1)
			{
				*s_i = si;
				*c_i = ci;
				break;
			}
		}
	}
}

int add_server(const char *host_and_port_in, const char *username, const char *password, const char *nickname, const char *complete_name, const char *description)
{
	char *host_and_port = strdup(host_and_port_in);
	char *colon = strchr(host_and_port, ':');

	/* grow & initialize list */
	server_list = realloc(server_list, (n_servers + 1)* sizeof(server));
	memset(&server_list[n_servers], 0x00, sizeof(server));

	server_list[n_servers].fd = -1;

	/* host [+ port] */
	if (colon)
	{
		*colon = 0x00;
		server_list[n_servers].server_port = atoi(colon + 1);
	}
	else
	{
		server_list[n_servers].server_port = DEFAULT_IRC_PORT;
	}

	server_list[n_servers].server_host = host_and_port;

	/* general info */
	if (description)
		server_list[n_servers].description = strdup(description);
	server_list[n_servers].username = strdup(username ? username : "");
	if (password)
		server_list[n_servers].password = strdup(password);
	server_list[n_servers].nickname = strdup(nickname);
	server_list[n_servers].user_complete_name = strdup(complete_name ? complete_name : "");

	server_list[n_servers].channel_list_complete = FALSE;
	server_list[n_servers].channel_list = NULL;
	server_list[n_servers].channel_list_n = 0;

	init_string_array(&server_list[n_servers].auto_join);

	init_string_array(&server_list[n_servers].send_after_login);

	server_list[n_servers].reconnect_delay = DEFAULT_RECONNECT_DELAY;

	/* initialize server entry */
	server_list[n_servers].state = STATE_NO_CONNECTION;

	/* allocate and init channel */
	(void)add_channel(n_servers, "server messages");

	n_servers++;

	return n_servers - 1;
}

const char *gen_random_nick()
{
	long int some_value = lrand48();
	char *nick = NULL;

	asprintf(&nick, "firc%lx", some_value);

	return nick;
}

void create_default_server(void)
{
	if (n_servers == 0)
	{
		server *ps = NULL;
		const char *nick = gen_random_nick();

		popup_notify(TRUE, "No server in config file (empty?): creating example");

		add_server("irc.vanheusden.com:6667", nick, "bla123", nick, "unconfigured f-irc user", "example server");

		myfree(nick);

		ps = &server_list[0];

		add_to_string_array(&ps -> auto_join, "#f-irc");

		store_config_on_exit= TRUE;

		popup_notify(TRUE, "Since this probably the first run: press F1 for help");
	}
}

void set_state(int server_index, conn_state_t state)
{
	LOG("set state for server %d to %d\n", server_index, state);

	server_list[server_index].state = state;
	server_list[server_index].state_since = time(NULL);

	update_channel_border(server_index);

	show_channel_names_list();
}

conn_state_t get_state(int server_index)
{
	return server_list[server_index].state;
}

long int get_state_age(int server_index)
{
	long int age = time(NULL) - server_list[server_index].state_since;

	return age;
}

int get_server_color(int server_index)
{
	conn_state_t state = get_state(server_index);
	BOOL error_state = state == STATE_NO_CONNECTION || state == STATE_ERROR || state == STATE_DISCONNECTED;
	BOOL busy_state = state != STATE_RUNNING && error_state == FALSE;

	if (error_state)
		return error_colorpair;

	if (busy_state)
		return temp_colorpair;

	return -1;
}

void restart_server(int sr)
{
	LOG("restart server %d\n", server_list[sr].fd);
	close(server_list[sr].fd);

	server_list[sr].state = STATE_NO_CONNECTION;
	server_list[sr].state_since = 0;

	server_list[sr].channel_list_complete = FALSE;
}

void try_next_server(int sr)
{
	server_list[sr].ri.index = (server_list[sr].ri.index + 1) % server_list[sr].ri.alist_n;

	server_list[sr].ri.attempt = 0;

	server_list[sr].reconnect_delay = DEFAULT_RECONNECT_DELAY;
}

int register_server_events(struct pollfd **pfd, int *n_fd)
{
	int loop = 0, redraw_rc = 0;

	for(loop=0; loop<n_servers; loop++)
	{
		server_list[loop].ifd = -1;

		if (get_state(loop) == STATE_NO_CONNECTION)
		{
			if (get_state_age(loop) >= delay_before_reconnect)
			{
				char *message = NULL;

				update_statusline(loop, 0, "Resolving %s", server_list[loop].server_host);
				mydoupdate();

				if (resolve(server_list[loop].server_host, server_list[loop].server_port, &server_list[loop].ri, &message) && server_list[loop].ri.alist_n > 0)
					set_state(loop, STATE_TCP_CONNECT);
				else
				{
					update_statusline(loop, 0, "Problem resolving %s: %s", server_list[loop].server_host, message);
					set_state(loop, STATE_ERROR);

					free(message);
				}

				redraw_rc = 1;
			}
		}
		else if (get_state(loop) == STATE_ERROR)
		{
		}
		else if (get_state(loop) == STATE_TCP_CONNECT)
		{
			char *message = NULL;
			const char *ip = get_ip(&server_list[loop].ri);

			if ((server_list[loop].fd = connect_to(&server_list[loop].ri, &message)) == -1)
			{
				set_state(loop, STATE_ERROR);
				close(server_list[loop].fd);

				update_statusline(loop, 0, "Cannot connect to %s:%d (%s), %s", server_list[loop].server_host, server_list[loop].server_port, message, ip);
			}
			else
			{
				set_state(loop, STATE_IRC_CONNECTING);
				assert(server_list[loop].fd != -1);

				server_list[loop].ifd = add_poll(pfd, n_fd, server_list[loop].fd, POLLOUT | POLLHUP);

				update_statusline(loop, 0, "Connecting to %s:%d (%s)", server_list[loop].server_host?server_list[loop].server_host : "?", server_list[loop].server_port, ip);
				LOG("server %d has fd %d\n", loop, server_list[loop].fd);
			}

			myfree(ip);
			free(message);

			redraw_rc = 1;
		}
		else if (get_state(loop) == STATE_IRC_CONNECTING)
		{
			assert(server_list[loop].fd != -1);
			server_list[loop].ifd = add_poll(pfd, n_fd, server_list[loop].fd, POLLOUT | POLLHUP);
		}
		else if (get_state(loop) == STATE_CONNECTED1)
		{
			free(server_list[loop].server_real);
			server_list[loop].server_real = get_endpoint_name(server_list[loop].fd);

			if (set_no_delay(server_list[loop].fd) == -1)
			{
				set_state(loop, STATE_DISCONNECTED);
				close(server_list[loop].fd);
				update_statusline(loop, 0, "Connection to %s:%d (%s) closed by other end (1)", server_list[loop].server_host, server_list[loop].server_port, server_list[loop].server_real);

				try_next_server(loop);
			}
			/* login */
			else if (irc_login1(&server_list[loop]) == -1)
			{
				set_state(loop, STATE_DISCONNECTED);
				close(server_list[loop].fd);

				if (++server_list[loop].ri.attempt > 3)
					try_next_server(loop);

				update_statusline(loop, 0, "Connection to %s:%d (%s) closed during login (\"PASS\" command)", server_list[loop].server_host, server_list[loop].server_port, server_list[loop].server_real);
			}
			else
			{
				update_statusline(loop, 0, "Connected to %s", server_list[loop].server_real);
				update_statusline(loop, 0, "%s: send NICK, sleeping for %d seconds", server_list[loop].server_host, nick_sleep);
			}

			set_state(loop, STATE_CONNECTED2);

			assert(server_list[loop].fd != -1);
			server_list[loop].ifd = add_poll(pfd, n_fd, server_list[loop].fd, POLLIN | POLLHUP);

			redraw_rc = 1;
		}
		else if (get_state(loop) == STATE_CONNECTED2)
		{
			if (get_state_age(loop) >= nick_sleep)
			{
				if (irc_login2(&server_list[loop]) == -1)
				{
					set_state(loop, STATE_DISCONNECTED);
					close(server_list[loop].fd);
					update_statusline(loop, 0, "Connection to %s:%d (%s) closed (\"USER\" command)", server_list[loop].server_host, server_list[loop].server_port, server_list[loop].server_real);

					if (++server_list[loop].ri.attempt > 3)
						try_next_server(loop);

					redraw_rc = 1;
				}

				set_state(loop, STATE_LOGGING_IN);
			}
		}
		else if (get_state(loop) == STATE_LOGGING_IN)
		{
			assert(server_list[loop].fd != -1);
			server_list[loop].ifd = add_poll(pfd, n_fd, server_list[loop].fd, POLLIN | POLLHUP);

			server_list[loop].must_send_after_login = 1;
		}
		else if (get_state(loop) == STATE_RUNNING)
		{
			int rc = 0;

			assert(server_list[loop].fd != -1);
			server_list[loop].ifd = add_poll(pfd, n_fd, server_list[loop].fd, POLLIN | POLLHUP);

			if (server_list[loop].must_send_after_login == 1)
			{
				/* re-join any channels (when this run was because of a re-(!)connect) */
				if (server_list[loop].n_channels && rc == 0)
				{
					int ch_index = 0;

					update_statusline(loop, 0, "re-join %d channels", server_list[loop].n_channels);

					for(ch_index=1; ch_index<server_list[loop].n_channels; ch_index++)
					{
						const char *ch = server_list[loop].pchannels[ch_index].channel_name;

						if (is_channel(ch))
						{
							int c_rc = 0;

							update_statusline(loop, 0, "REJOIN> %s", ch);

							c_rc = irc_join(server_list[loop].fd, ch);
							rc |= c_rc;

							if (c_rc)
							{
								update_statusline(loop, 0, "...failed");
								break;
							}
						}
					}
				}

				if ((string_array_get_n(&server_list[loop].send_after_login) || string_array_get_n(&server_list[loop].auto_join)) && rc == 0)
				{
					int sal_index, ch_index;

					update_statusline(loop, 0, "Doing %d auto commands (send_after_login)", string_array_get_n(&server_list[loop].send_after_login));

					/* send list of things to do when connecting */
					for(sal_index=0; sal_index<string_array_get_n(&server_list[loop].send_after_login); sal_index++)
					{
						int c_rc = 0;
						const char *cmd = string_array_get(&server_list[loop].send_after_login, sal_index);

						update_statusline(loop, 0, "SAL> %s", cmd);

						c_rc = do_send(server_list[loop].fd, "%s", cmd);
						rc |= c_rc;

						if (c_rc)
						{
							update_statusline(loop, 0, "...failed");
							break;
						}
					}

					/* re-join any configured channels */
					update_statusline(loop, 0, "join %d channels", string_array_get_n(&server_list[loop].auto_join));

					for(ch_index=0; ch_index<string_array_get_n(&server_list[loop].auto_join) && !rc; ch_index++)
					{
						const char *ch = string_array_get(&server_list[loop].auto_join, ch_index);
						int c_rc = 0;

						update_statusline(loop, 0, "JOIN> %s", ch);

						c_rc = irc_join(server_list[loop].fd, ch);
						rc |= c_rc;

						if (c_rc)
						{
							update_statusline(loop, 0, "...failed");
							break;
						}
					}
				}

				show_channel_names_list();

				/* update_statusline(loop, 0, "requesting list of channels");
				rc |= irc_list(server_list[loop].fd); */

				server_list[loop].must_send_after_login = 0;
			}

			if (rc)
			{
				LOG("failure close server %d\n", server_list[loop].fd);
				close(server_list[loop].fd);

				update_statusline(loop, 0, "Connection to %s:%d (%s) closed (during (re-)join/send-after-connect-commands)", server_list[loop].server_host, server_list[loop].server_port, server_list[loop].server_real);

				set_state(loop, STATE_DISCONNECTED);
				close(server_list[loop].fd);

				if (++server_list[loop].ri.attempt > 3)
					try_next_server(loop);
			}

			redraw_rc = 1;
		}
		else if (get_state(loop) == STATE_DISCONNECTED)
		{
			/* do nothing */
			/* if user writes something or does /reconnect, start reconnect */

			if (server_list[loop].reconnect_delay < DEFAULT_MAX_RECONNECT_DELAY)
			{
				double progress = (double)server_list[loop].reconnect_delay / (double)DEFAULT_MAX_RECONNECT_DELAY;
				double factor = 2.0 - 0.9 * progress;
				int ifactor = (int)(factor * 1000.0);

				server_list[loop].reconnect_delay = (server_list[loop].reconnect_delay * ifactor) / 1000;
			}
		}
		else
		{
			update_statusline(loop, 0, "Server socket %d (%s:%d) in unknown state %d", server_list[loop].fd, server_list[loop].server_host, server_list[loop].server_port, server_list[loop].state);

			redraw_rc = 1;
		}
	}

	return redraw_rc;
}

int process_server(int cur_server)
{
	server *ps = &server_list[cur_server];
	char str_buffer[65536] = { 0 };
	int n_read = read(ps -> fd, str_buffer, sizeof str_buffer);

	if (n_read > 0)
	{
		time_t now = time(NULL);
		int t_diff = now - ps -> ts_bytes;

		if (t_diff > 30)
		{
			ps -> prev_bps = ps -> bytes / t_diff;
			ps -> bytes = 0;
			ps -> ts_bytes = now;
		}

		ps -> bytes += n_read;

		ps -> ts_last_action = now;

		/* move data to buffer */
		add_lf_buffer(&ps -> io_buffer, str_buffer, n_read);

		/* see if there's anything to process */
		for(;;)
		{
			char *line = (char *)get_line_lf_buffer(&ps -> io_buffer); /* FIXME char cast */
			if (!line)
				break;

			LOG("IN: %s\n", line);

			if (process_server_do_line(cur_server, line) == -1)
			{
				LOG("process_server_do_line returned -1\n");
				myfree(line);
				return -1;
			}

			myfree(line);
		}
	}
	else if (n_read == 0)	/* connection closed */
	{
		LOG("read() returned 'connection closed'\n");
		update_statusline(cur_server, 0, "Connection closed by server", ps -> server_host, ps -> server_port, strerror(errno), errno);
		return -1;
	}
	else	/* -1: error */
	{
		if (errno != EINTR && errno != EAGAIN)
		{
			LOG("read returned error %d\n", errno);
			update_statusline(cur_server, 0, "read() for server %s:%d failed, reason: %s (%d)", ps -> server_host, ps -> server_port, strerror(errno), errno);
			return -1;
		}
	}

	return 0;
}

void process_server_events(struct pollfd *pfd, int n_fd)
{
	int si = 0;

	for(si=0; si<n_servers; si++)
	{
		struct pollfd *cpfd = NULL;

		if (server_list[si].fd == -1)
			continue;

		/* this can happen for states that do not setup a r/w poll, e.g. STATE_CONNECTED2 */
		if (server_list[si].ifd == -1)
			continue;

		cpfd = &pfd[server_list[si].ifd];

		if ((cpfd -> revents & POLLHUP) || (cpfd -> revents & POLLNVAL))
		{
			update_statusline(si, 0, "Connection to %s:%d closed by other end (POLLHUP/POLLNVAL)", server_list[si].server_host, server_list[si].server_port);

			set_state(si, STATE_DISCONNECTED);

			close(server_list[si].fd);
			server_list[si].fd = -1;
		}
		else if (cpfd -> revents & POLLOUT) /* connected? */
		{
			cstate_t cstate = check_connection_progress(server_list[si].fd);

			if (cstate == TCS_CONNECTED)
			{
				set_state(si, STATE_CONNECTED1);
				update_statusline(si, 0, "Connected to %s:%d (%s)", server_list[si].server_host, (int)server_list[si].server_port, str_or_nothing(server_list[si].description));
			}
			else if (cstate == TCS_ERROR)
			{
				set_state(si, STATE_ERROR);
				update_statusline(si, 0, "Cannot connect to %s:%d (%s), reason: %s (%d)", server_list[si].server_host, server_list[si].server_port, str_or_nothing(server_list[si].description), strerror(errno), errno);

				close(server_list[si].fd);
				server_list[si].fd = -1;
			}
		}

		/* any traffic? */
		if ((cpfd -> revents & POLLIN) && server_list[si].fd != -1)
		{
			if (process_server(si) == -1)
			{
				char prev_state = server_list[si].state;

				update_statusline(si, 0, "Connection to %s:%d closed (processing incoming traffic)", server_list[si].server_host, server_list[si].server_port);

				if (prev_state == STATE_ERROR)
					set_state(si, STATE_ERROR);
				else
					set_state(si, STATE_DISCONNECTED);

				close(server_list[si].fd);
			}
		}
	}
}

int find_in_autojoin(int sr, const char *channel_name)
{
	return find_str_in_string_array(&server_list[sr].auto_join, channel_name, TRUE);
}

void add_autojoin(int sr, char *channel_name)
{
	add_to_string_array(&server_list[sr].auto_join, channel_name);
}

void remove_autojoin(int sr, int aj_nr)
{
	del_nr_from_string_array(&server_list[sr].auto_join, aj_nr);
}

void server_set_additional_nick(int sr, const char *n2)
{
	myfree(server_list[sr].nickname2);

	server_list[sr].nickname2 = strdup(n2);
}

int compare_channel_list_item(const void *a, const void *b)
{
	channel_topic_t *pa = (channel_topic_t *)a;
	channel_topic_t *pb = (channel_topic_t *)b;

	return strcasecmp(pa -> channel, pb -> channel);
}

void free_channel_list(int sr)
{
	server *ps = &server_list[sr];
	int index = 0;

	for(index=0; index<ps -> channel_list_n; index++)
	{
		myfree(ps -> channel_list[index].channel);
		myfree(ps -> channel_list[index].topic);
	}

	myfree(ps -> channel_list);

	ps -> channel_list_n = 0;
	ps -> channel_list = NULL;

	ps -> channel_list_complete = FALSE;
}

int compare_channel_for_sort(const void *a, const void *b)
{
	channel *pa = (channel *)a;
	channel *pb = (channel *)b;

	return strcasecmp(pa -> channel_name, pb -> channel_name);
}

void sort_channels(int sr)
{
	server *ps = &server_list[sr];

	/* server channel must always be at position 0 */
	if (ps -> n_channels > 1)
		qsort(&ps -> pchannels[1], ps -> n_channels - 1, sizeof(channel), compare_channel_for_sort);
}

server *gsr(int sr)
{
	return &server_list[sr];
}
