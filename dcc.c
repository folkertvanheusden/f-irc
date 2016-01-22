/* GPLv2 applies
 * SVN revision: $Revision: 817 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ncursesw/panel.h>
#include <ncursesw/ncurses.h>

#include "error.h"
#include "gen.h"
#include "term.h"
#include "buffer.h"
#include "channels.h"
#include "servers.h"
#include "loop.h"
#include "utils.h"
#include "tcp.h"
#include "irc.h"
#include "dcc.h"
#include "config.h"

DCC_t *dcc_list = NULL;
int n_dcc = 0;
char *dcc_path = NULL;

#define BUFFER_SIZE	1024	/* for read/write: I've read that 1024 is standard for most(?) irc clients */

void init_dcc(void)
{
	dcc_path = strdup("");
}

void set_dcc_state(int index, dcc_conn_state_t state)
{
	LOG("DCC %d state is now %d\n", index, state);

	dcc_list[index].state = state;
}

void free_dcc(void)
{
	int loop = 0;

	for(loop=0; loop<n_dcc; loop++)
	{
		close(dcc_list[loop].fd_file);
		close(dcc_list[loop].fd_conn);
	}

	free(dcc_list);
	dcc_list = NULL;

	n_dcc = 0;

	free(dcc_path);
}

int init_send_dcc(const char *filename, int server_index, int channel_index, const char *nick)
{
	int di = n_dcc;
	char *msg_buffer = NULL, *dummy = NULL;
	int new_conn_fd = -1;
	struct sockaddr sock_name, nc_sa;
	struct sockaddr_in *sock_name_in = (struct sockaddr_in *)&sock_name;
	socklen_t sock_name_len = sizeof(sock_name), nc_sa_len = sizeof(nc_sa);

	/* determine the address of the socket used to connect to the irc-server,
	 * hopefully that is also the shortest route to the guy who will receive
	 * a file via DCC
	 */
	if (dcc_bind_to && strlen(dcc_bind_to))
	{
		if (inet_pton(AF_INET6, dcc_bind_to, &((struct sockaddr_in6 *)&sock_name) -> sin6_addr) == 1)
			((struct sockaddr_in6 *)&sock_name) -> sin6_family = AF_INET6;
		else if (inet_pton(AF_INET, dcc_bind_to, &((struct sockaddr_in *)&sock_name) -> sin_addr) == 1)
			((struct sockaddr_in *)&sock_name) -> sin_family = AF_INET;
		else
			update_statusline(server_index, channel_index, "DCC (send): cannot convert %s (not a valid IPv4 or IPv6 address): %s (%d)", dcc_bind_to, strerror(errno), errno);

		return 0;
	}
	else if (getsockname(server_list[server_index].fd, &sock_name, &sock_name_len))
	{
		update_statusline(server_index, channel_index, "DCC (send): cannot retrieve local address: %s (%d)", strerror(errno), errno);
		return 0; /* not a irc-server socket problem */
	}

	/* setup entry in DCC array */
	n_dcc++;
	dcc_list = (DCC_t *)realloc(dcc_list, n_dcc * sizeof(DCC_t));

	memset(&dcc_list[di], 0x00, sizeof(DCC_t));
	dcc_list[di].mode = DCC_SEND_FILE;
	set_dcc_state(di, DSTATE_DCC_CONNECTING);

	dcc_list[di].server_nr = server_index;
	dcc_list[di].channel_nr = channel_index;

	dcc_list[di].last_update = 0;

	dcc_list[di].filename = strdup(filename);

	/* try to open the file */
	dcc_list[di].fd_file = open(filename, O_RDONLY);
	if (dcc_list[di].fd_file == -1)
	{
		set_dcc_state(di, DSTATE_ERROR);
		update_statusline(server_index, channel_index, "DCC (send): failed to open the file %s: %s (%d)", filename, strerror(errno), errno);
		return 0; /* not a irc-server socket problem */
	}

	/* create socket on which we will listen for a connection from the receiving end */
	dcc_list[di].fd_conn = setup_nonblocking_socket();
	if (dcc_list[di].fd_conn == -1)
	{
		set_dcc_state(di, DSTATE_ERROR);
		update_statusline(server_index, channel_index, "DCC: cannot create socket, reason: %s (%d)", strerror(errno), errno);
		return 0; /* not a irc-server socket problem */
	}
	/* bind socket to port 0: let the OS pick a portnumber */
	sock_name_in -> sin_port = 0;
	if (bind(dcc_list[di].fd_conn, &sock_name, sock_name_len))
	{
		set_dcc_state(di, DSTATE_ERROR);
		update_statusline(server_index, channel_index, "DCC: cannot bind the socket to the adapter used by the outgoing IRC connection, reason: %s (%d)", strerror(errno), errno);
		return 0; /* not a irc-server socket problem */
	}

	/* now find out what port was given to us */
	if (getsockname(dcc_list[di].fd_conn, &sock_name, &sock_name_len))
	{
		update_statusline(server_index, channel_index, "DCC (send): failed finding out local port: %s (%d)", strerror(errno), errno);
		return 0; /* not a irc-server socket problem */
	}

	/* setup listen queue */
	if (listen(dcc_list[di].fd_conn, 5) == -1)
	{
		set_dcc_state(di, DSTATE_ERROR);
		update_statusline(server_index, channel_index, "DCC: cannot setup socket (listen), reason: %s (%d)", strerror(errno), errno);
		return 0; /* not a irc-server socket problem */
	}

	/* create irc command */
	dummy = strrchr(filename, '/');
	/* FIXME dotter ipv4/ipv6 */
	asprintf(&msg_buffer, "\001DCC SEND %s %u %u\001", dummy?dummy+1:filename, ntohl(sock_name_in -> sin_addr.s_addr), ntohs(sock_name_in -> sin_port));

	/* send irc command */
	if (irc_privmsg(server_list[server_index].fd, nick, msg_buffer) != 0)
	{
		free(msg_buffer);
		return -1;
	}

	free(msg_buffer);

	/* now sit back and wait for the other end to connect; then start
	 * start streaming
	 */

	/* start the listening & do a first probe: */
	/* FIXME if other end does not connect then that is a DDOS */
	new_conn_fd = accept(dcc_list[di].fd_conn, &nc_sa, &nc_sa_len);
	if (new_conn_fd != -1)
	{
		/* close listen socket */
		close(dcc_list[di].fd_conn);
		/* and use the new socket (with the new client on it) as the xfer socket */
		dcc_list[di].fd_conn = new_conn_fd;

		set_dcc_state(di, DSTATE_CONNECTED1);
		update_statusline(server_index, channel_index, "DCC: connected");
	}

	return 0;
}

void init_recv_dcc(const char *filename, const char *addr_in, int port, int server_nr, int channel_nr)
{
	int di = n_dcc;
	char *addr = NULL;
	char *real_name = NULL;
	char *local_filename = NULL;
	char *dummy = NULL;
	char *message = NULL;
	struct stat st;

	if (strcmp(addr_in, "0") == 0) /* irc bouncer most of the times */
		addr = strdup(server_list[server_nr].server_host);
	else if (strchr(addr_in, '.')) /* dotted ipv4 */
		addr = strdup(addr_in);
	else if (strchr(addr_in, ':')) /* dotted ipv6 */
		addr = strdup(addr_in);
	else
	{
		int ip = atoi(addr_in);

		asprintf(&addr, "%d.%d.%d.%d", (ip >> 24) & 255, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255);
	}

	update_statusline(server_nr, channel_nr, "DCC: IPv4 address is [%s]:%d", addr, port);

	n_dcc++;
	dcc_list = (DCC_t *)realloc(dcc_list, n_dcc * sizeof(DCC_t));
	memset(&dcc_list[di], 0x00, sizeof(DCC_t));

	dcc_list[di].server_nr = server_nr;
	dcc_list[di].channel_nr = channel_nr;

	dcc_list[di].last_update = 0;

	dcc_list[di].filename = strdup(filename);

	if (resolve(addr, port, &dcc_list[di].ri, &message) == FALSE)
	{
		update_statusline(server_nr, channel_nr, "Failed converting network address: %s", message);
		free(message);

		set_dcc_state(di, DSTATE_ERROR);
	}
	else
	{
		BOOL first_attempt = TRUE;
		const char *dcc_path_exploded = (dcc_path && strlen(dcc_path)) ? (char *)explode_path(dcc_path) : NULL;

		/* strip slashes from filename */
		dummy = strrchr(filename, '/');
		if (dummy)
			filename = dummy + 1;

		dummy = strrchr(filename, '\\');
		if (dummy)
			filename = dummy + 1;

		for(;;)
		{
			local_filename = NULL;

			if (first_attempt)
			{
				if (dcc_path_exploded)
					asprintf(&local_filename, "%s/%s", dcc_path_exploded, filename);
				else
					local_filename = strdup(filename);

				first_attempt = FALSE;
			}
			else
			{
				if (dcc_path_exploded)
					asprintf(&local_filename, "%s/%s.%d", dcc_path_exploded, filename, rand());
				else
					asprintf(&local_filename, "%s.%d", filename, rand());
			}

			if (stat(local_filename, &st) == -1)
				break;

			LOG("DCC create file, tested %s: %s\n", local_filename, strerror(errno));

			free(local_filename);
		}

		myfree(dcc_path_exploded);

		dcc_list[di].fd_file = open(local_filename, O_WRONLY | O_CREAT | O_EXCL, S_IRWXU | S_IRGRP | S_IROTH);
		if (dcc_list[di].fd_file == -1)
		{
			update_statusline(server_nr, channel_nr, "DCC: failed to create local file %s, reason: %s (%d)", local_filename, strerror(errno), errno);

			set_dcc_state(di, DSTATE_ERROR);
		}
		else
		{
			dcc_list[di].mode = DCC_RECEIVE_FILE;

			set_dcc_state(di, DSTATE_TCP_CONNECT);
		}

		myfree(local_filename);

		free(real_name);

		free(addr);
	}
}

int dcc_receive(DCC_t *pnt)
{
	char io_buffer[BUFFER_SIZE] = { 0 };
	int rc = -1;

	for(;;)
	{
		rc = read(pnt -> fd_conn, io_buffer, BUFFER_SIZE);

		if (rc > 0)
			break;

		if (rc == 0)
		{
			update_statusline(pnt -> server_nr, pnt -> channel_nr, "DCC: end of file? (%ld bytes received)", lseek(pnt -> fd_file, 0, SEEK_CUR));
			pnt -> state = DSTATE_NO_CONNECTION;
			close(pnt -> fd_file);
			close(pnt -> fd_conn);
			break;
		}

		if (errno != EINTR && errno != EAGAIN)
		{
			update_statusline(pnt -> server_nr, pnt -> channel_nr, "DCC: read error from socket (%s)", strerror(errno));
			pnt -> state = DSTATE_ERROR;
			close(pnt -> fd_file);
			close(pnt -> fd_conn);
			break;
		}
	}

	if (rc > 0)
	{
		if (write(pnt -> fd_file, io_buffer, rc) != rc)
		{
			update_statusline(pnt -> server_nr, pnt -> channel_nr, "DCC: error writing to disk (%s)", strerror(errno));
			pnt -> state = DSTATE_NO_CONNECTION;
			close(pnt -> fd_file);
			close(pnt -> fd_conn);
			return 1;
		}
		else
		{
			time_t now = time(NULL);

			off_t f_offs = lseek(pnt -> fd_file, 0, SEEK_CUR);
			/* success reading a block and writing it to disk
			 * transmit an offset
			 */
			uint32_t offset = htonl(f_offs);

			if (WRITE(pnt -> fd_conn, (char *)&offset, sizeof(offset)) != sizeof(offset))
			{
				update_statusline(pnt -> server_nr, pnt -> channel_nr, "DCC: failed to ack file");
				pnt -> state = DSTATE_ERROR;
				close(pnt -> fd_file);
				close(pnt -> fd_conn);
				return 1;
			}

			if (now - pnt -> last_update >= 5)
			{
				update_statusline(pnt -> server_nr, pnt -> channel_nr, "DCC: %d bytes received", f_offs);

				pnt -> last_update = now;
			}
		}

		return 0;	/* connection still in progress, 1=eof */
	}

	return 1;
}

int dcc_send(DCC_t *pnt)
{
	char io_buffer[BUFFER_SIZE] = { 0 };
	int rc;

	rc = read(pnt -> fd_file, io_buffer, BUFFER_SIZE);
	if (rc == -1)
	{
		update_statusline(pnt -> server_nr, pnt -> channel_nr, "DCC: error reading from file: %s (%d)", strerror(errno), errno);
		pnt -> state = DSTATE_ERROR;
		close(pnt -> fd_file);
		close(pnt -> fd_conn);
		return 1;
	}
	else if (rc == 0)
	{
		update_statusline(pnt -> server_nr, pnt -> channel_nr, "DCC: EOF");
		close(pnt -> fd_file);
		close(pnt -> fd_conn);
		pnt -> state = DSTATE_NO_CONNECTION;
		return 1;
	}

	if (WRITE(pnt -> fd_conn, io_buffer, rc) != rc)
	{
		update_statusline(pnt -> server_nr, pnt -> channel_nr, "DCC: error sending to host: %s (%d)", strerror(errno), errno);
		pnt -> state = DSTATE_ERROR;
		close(pnt -> fd_file);
		close(pnt -> fd_conn);
		return 1;
	}

	return 0;
}

dcc_conn_state_t get_dcc_state(int index)
{
	return dcc_list[index].state;
}

int register_dcc_events(struct pollfd **pfd, int *n_fd)
{
	char *message = NULL;
	int loop = 0, redraw_rc = 0;

	for(loop=0; loop<n_dcc; loop++)
	{
		dcc_list[loop].ifd = -1;

		switch(get_dcc_state(loop)) {
			case DSTATE_TCP_CONNECT:
				if ((dcc_list[loop].fd_conn = connect_to(&dcc_list[loop].ri, &message)) == -1)
				{
					set_dcc_state(loop, DSTATE_ERROR);
					update_statusline(dcc_list[loop].server_nr, dcc_list[loop].channel_nr, "DCC: cannot connect (file %s), reason: %s (%d) / %s", dcc_list[loop].filename, strerror(errno), errno, message);
					free(message);
				}
				else
				{
					set_dcc_state(loop, DSTATE_DCC_CONNECTING);
					update_statusline(dcc_list[loop].server_nr, dcc_list[loop].channel_nr, "DCC: connecting (file %s)", dcc_list[loop].filename);
				}

				redraw_rc = 1;

				break;

			case DSTATE_DCC_CONNECTING:
				assert(dcc_list[loop].fd_conn != -1);

				if (dcc_list[loop].mode == DCC_RECEIVE_FILE)
					dcc_list[loop].ifd = add_poll(pfd, n_fd, dcc_list[loop].fd_conn, POLLOUT | POLLHUP);
				else if (dcc_list[loop].mode == DCC_SEND_FILE)
					dcc_list[loop].ifd = add_poll(pfd, n_fd, dcc_list[loop].fd_conn, POLLIN | POLLHUP);
				else
					LOG("state_connecting: invalid internal DCC mode: %d\n", dcc_list[loop].mode);
				break;

			case DSTATE_CONNECTED1:
				assert(dcc_list[loop].fd_conn != -1);

				if (dcc_list[loop].mode == DCC_RECEIVE_FILE)
					dcc_list[loop].ifd = add_poll(pfd, n_fd, dcc_list[loop].fd_conn, POLLIN | POLLHUP);
				else if (dcc_list[loop].mode == DCC_SEND_FILE)
					dcc_list[loop].ifd = add_poll(pfd, n_fd, dcc_list[loop].fd_conn, POLLOUT | POLLHUP);
				else
					LOG("state_connected: invalid internal DCC mode: %d\n", dcc_list[loop].mode);
				break;

			case DSTATE_NO_CONNECTION:
			case DSTATE_ERROR:
			case DSTATE_RUNNING:
			case DSTATE_DISCONNECTED:
				break;
		}
	}

	return redraw_rc;
}

void process_dcc_events(struct pollfd *pfd, int n_fd)
{
	int loop;

	for(loop=0; loop<n_dcc; loop++)
	{
		int pfd_idx = dcc_list[loop].ifd;
		int sr = dcc_list[loop].server_nr;
		int ch = dcc_list[loop].channel_nr;

		/* happens when state != DSTATE_DCC_CONNECTING && state != DSTATE_DCC_CONNECTED1 */
		if (pfd_idx == -1)
			continue;

                if ((pfd[pfd_idx].revents & POLLHUP) || (pfd[pfd_idx].revents & POLLNVAL))
                {
                        update_statusline(sr, ch, "DCC: connection closed for %s", dcc_list[loop].filename);

			set_dcc_state(loop, DSTATE_NO_CONNECTION);
                        close(dcc_list[loop].fd_conn);

			continue;
                }

		switch(dcc_list[loop].state) {
			case DSTATE_DCC_CONNECTING:
				if ((dcc_list[loop].mode == DCC_RECEIVE_FILE || dcc_list[loop].mode == DCC_CHAT) &&
					(pfd[pfd_idx].revents & POLLOUT))
				{
					int cstate = check_connection_progress(dcc_list[loop].fd_conn);

					if (cstate == TCS_CONNECTED)
					{
						set_dcc_state(loop, DSTATE_CONNECTED1);
						update_statusline(sr, ch, "DCC: connected");
					}
					else if (cstate == TCS_ERROR)
					{
						set_dcc_state(loop, DSTATE_ERROR);
						update_statusline(sr, ch, "DCC: cannot connect, reason: %s (%d)", strerror(errno), errno);
					}
				}
				else if (dcc_list[loop].mode == DCC_SEND_FILE && 
					(pfd[pfd_idx].revents & POLLIN))
				{
					/* connection waiting: accept connection */
					struct sockaddr addr;
					socklen_t addr_len = sizeof(addr);
					int new_fd = accept(dcc_list[loop].fd_conn, &addr, &addr_len);

					if (new_fd == -1)
						update_statusline(sr, ch, "DCC: failed connecting to peer, reason: %s (%d)", strerror(errno), errno);
					else
					{
						/* close listen socket */
						close(dcc_list[loop].fd_conn);
						/* and use the new socket (with the new client on it) as the xfer socket */
						dcc_list[loop].fd_conn = new_fd;

						set_dcc_state(loop, DSTATE_CONNECTED1);
						update_statusline(sr, ch, "DCC: connected");
					}
				}
				break;

			case DSTATE_CONNECTED1:
				if (dcc_list[loop].mode == DCC_RECEIVE_FILE && (pfd[pfd_idx].revents & POLLIN))
				{
					if (dcc_receive(&dcc_list[loop]))
						set_dcc_state(loop, DSTATE_NO_CONNECTION);
				}
				else if (dcc_list[loop].mode == DCC_SEND_FILE && (pfd[pfd_idx].revents & POLLOUT))
				{
					if (dcc_send(&dcc_list[loop]))
						set_dcc_state(loop, DSTATE_NO_CONNECTION);
				}
				break;

			case DSTATE_NO_CONNECTION:
			case DSTATE_ERROR:
			case DSTATE_TCP_CONNECT:
			case DSTATE_RUNNING:
			case DSTATE_DISCONNECTED:
				break;
		}
	}
}
