/* GPLv2 applies
 * SVN revision: $Revision: 810 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
typedef enum { DCC_RECEIVE_FILE, DCC_SEND_FILE, DCC_CHAT } dcc_type_t;

typedef enum { DSTATE_NO_CONNECTION = 0, DSTATE_ERROR, DSTATE_TCP_CONNECT, DSTATE_DCC_CONNECTING, DSTATE_CONNECTED1, DSTATE_RUNNING, DSTATE_DISCONNECTED } dcc_conn_state_t;

typedef struct
{
	dcc_type_t mode;
	dcc_conn_state_t state;
	int fd_conn, ifd;
	int fd_file;
	char *filename;
	int server_nr, channel_nr;
	time_t last_update;
	resolve_info ri;
} DCC_t;

extern DCC_t *dcc_list;
extern int n_dcc;
extern char *dcc_path;

void init_dcc(void);
void free_dcc(void);
void init_recv_dcc(const char *filename, const char *addr, int port, int server_index, int channel_index);
int init_send_dcc(const char *filename, int server_index, int channel_index, const char *nick);
void set_dcc_state(int index, dcc_conn_state_t state);
dcc_conn_state_t get_dcc_state(int index);
int dcc_send(DCC_t *pnt);
int dcc_receive(DCC_t *pnt);
void free_dcc(void);
int register_dcc_events(struct pollfd **pfd, int *n_fd);
void process_dcc_events(struct pollfd *pfd, int n_fd);
