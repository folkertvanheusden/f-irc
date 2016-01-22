/* GPLv2 applies
 * SVN revision: $Revision: 696 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#ifndef __TCP_H__
#define __TCP_H__

typedef enum { TCS_CONNECTED, TCS_IN_PROGRESS, TCS_ERROR } cstate_t;

typedef struct
{
	struct addrinfo *result;
	struct addrinfo **alist;
	int alist_n, index, attempt;
} resolve_info;

void free_resolve_info(resolve_info *ri);
BOOL resolve(const char *host, int portnr, resolve_info *ri, char **message);
int connect_to(resolve_info *ri, char **message);
const char *get_ip(resolve_info *ri);

int setup_nonblocking_socket(void);
int set_no_delay(int fd);
cstate_t check_connection_progress(const int fd);
char * get_endpoint_name(int fd);

#endif
