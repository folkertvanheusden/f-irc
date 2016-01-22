/* GPLv2 applies
 * SVN revision: $Revision: 806 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "error.h"
#include "gen.h"
#include "tcp.h"
#include "utils.h"

void set_ka(int fd)
{
	int on = 1, interval = 31;

	setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof on);

#ifdef __linux__
	setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &interval, sizeof interval);
	setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &interval, sizeof interval);
#endif
}

int set_no_delay(int fd)
{
	int flag = 1;

	if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int)) < 0)
	{
		LOG("could not set TCP_NODELAY on socket (%s)\n", strerror(errno));

		return -1;
	}

	return 0;
}

int setup_nonblocking_socket(void)
{
	/* create socket */
	int handle = socket(AF_INET, SOCK_STREAM, 0);

	if (handle == -1)
		return -1;

	/* set fd to non-blocking */
	if (fcntl(handle, F_SETFL, O_NONBLOCK) == -1)
	{
		int e = errno;

		close(handle);

		errno = e;

		return -1;
	}

	set_ka(handle);

	set_no_delay(handle);

	return handle;
}

char * get_endpoint_name(int fd)
{
	char *result = NULL;
	struct sockaddr_storage addr;
	socklen_t addr_len = sizeof addr;

	if (getpeername(fd, (struct sockaddr *)&addr, &addr_len) == -1)
		asprintf(&result, "getpeername failed: %s (%d)", strerror(errno), errno);
	else
	{
		char peer_name[4096] = { 0 };
		int port = -1, rc = 0;

		if (addr.ss_family == AF_INET)
		{
			struct sockaddr_in *s = (struct sockaddr_in *)&addr;

			port = ntohs(s->sin_port);

			rc = inet_ntop(AF_INET, &s->sin_addr, peer_name, sizeof peer_name) != NULL;
		}
		else /* ipv6 */
		{
			struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;

			port = ntohs(s->sin6_port);

			rc = inet_ntop(AF_INET6, &s->sin6_addr, peer_name, sizeof peer_name) != NULL;
		}

		if (rc)
			asprintf(&result, "[%s]:%d", peer_name, port);
		else
			result = strdup("inet_ntop failed");
	}

	return result;
}

void free_resolve_info(resolve_info *ri)
{
	free(ri -> alist);

	freeaddrinfo(ri -> result);
}

BOOL resolve(const char *host, int portnr, resolve_info *ri, char **message)
{
	char portnr_str[8] = { 0 };
	struct addrinfo hints, **alist = NULL, *result = NULL, *rp = NULL;
	int alist_n = 0, rc = 0;

	memset(ri, 0x00, sizeof(*ri));

	free(*message);
	*message = NULL;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
	hints.ai_protocol = 0;          /* Any protocol */
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	snprintf(portnr_str, sizeof portnr_str, "%d", portnr);

	rc = getaddrinfo(host, portnr_str, &hints, &result);
	if (rc != 0)
	{
		asprintf(message, "problem resolving %s: %s", host, gai_strerror(rc));
		return FALSE;
	}

	for(rp = result; rp != NULL; rp = rp->ai_next)
	{
		alist = (struct addrinfo **)realloc(alist, (alist_n + 1) * sizeof(struct addrinfo *));

		alist[alist_n++] = rp;
	}

	if (alist_n == 0)
	{
		free(alist);
		freeaddrinfo(result);
		asprintf(message, "resolving %s returned nothing", host);
		return FALSE;
	}

	memset(ri, 0x00, sizeof(*ri));

	ri -> result = result;
	ri -> alist = alist;
	ri -> alist_n = alist_n;

	return TRUE;
}

const char *get_ip(resolve_info *ri)
{
	char peer_name[4096] = { 0 };
	struct addrinfo *rp = ri -> alist[ri -> index];

	if (getnameinfo(rp -> ai_addr, sizeof(*rp -> ai_addr), peer_name, sizeof(peer_name), NULL, 0, NI_NUMERICHOST) == -1)
		return strdup("(cannot resolve)");

	return strdup(peer_name);
}

int connect_to(resolve_info *ri, char **message)
{
	struct addrinfo *rp = ri -> alist[ri -> index];

	int fd = socket(rp -> ai_family, rp -> ai_socktype, rp -> ai_protocol);
	if (fd == -1)
	{
		asprintf(message, "could not create socket: %s (%d)", strerror(errno), errno);
		ri -> index = (ri -> index + 1) % ri -> alist_n;
		return -1;
	}

	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
	{
		asprintf(message, "fcnt(O_NONBLOCK) failed: %s (%d)", strerror(errno), errno);
		close(fd);
		ri -> index = (ri -> index + 1) % ri -> alist_n;
		return -1;
	}

	set_ka(fd);

	set_no_delay(fd);

	/* connect to peer */
	if (connect(fd, rp -> ai_addr, rp -> ai_addrlen) == 0)
		return fd;

	if (errno == EINPROGRESS)
	{
		/* not yet made but might be fine*/
		return fd;
	}

	asprintf(message, "connect failed: %s (%d)", strerror(errno), errno);

	close(fd);

	ri -> index = (ri -> index + 1) % ri -> alist_n;

	return -1;
}

cstate_t check_connection_progress(int fd)
{
	int optval = 0;
	socklen_t optvallen = sizeof optval;

	/* see if the connect succeeded or failed */
	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &optval, &optvallen) == -1)
	{
		LOG("getsockopt failed\n");

		return TCS_ERROR;
	}

	/* no error? */
	if (optval == 0)
		return TCS_CONNECTED;

	if (optval != EINPROGRESS)
	{
		errno = optval;

		return TCS_ERROR;
	}

	return TCS_IN_PROGRESS;
}
