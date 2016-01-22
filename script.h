void start_script(const char *filename, int *rfd, int *wfd, pid_t *pid);
int end_script(pid_t pid);
int send_to_script(script_instances_t *si, server *ps, channel *pc, const char *user, const char *line, BOOL me, char **replacement, char **command);

void add_channel_script(channel *pc, const char *scr);
void process_scripts(server *ps, channel *pc, const char *user, const char *line, BOOL me, char **new_line, char **command);
