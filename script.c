/* GPLv2 applies
 * SVN revision: $Revision: 876 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#define _GNU_SOURCE
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef _POSIX_PRIORITY_SCHEDULING
#include <sched.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>

#include "gen.h"
#include "error.h"
#include "servers.h"
#include "channels.h"
#include "user.h"
#include "utils.h"

void start_script(const char *filename, int *rfd, int *wfd, pid_t *pid)
{
	int fds[2] = { 0 };

	if (pipe(fds) == -1)
		error_exit(TRUE, "error creating pipe\n");

	*pid = fork();
	if (*pid == -1)
		error_exit(TRUE, "error forking\n");

	if (*pid == 0)
	{
		char *no_arguments[] = { NULL };

		close(0);
		dup(fds[0]);

		close(1);
		dup(fds[1]);

		close(2);
		open("/dev/null", O_RDWR);

		if (execve(filename, no_arguments, no_arguments) == -1)
			error_exit(TRUE, "execve of %s failed\n", filename);

		exit(1);
	}

	*rfd = fds[0];
	*wfd = fds[1];
}

int end_script(pid_t pid)
{
	if (kill(pid, SIGTERM) == -1)
		return -1;

#ifdef _POSIX_PRIORITY_SCHEDULING
	sched_yield();
#endif
	usleep(100000);

	kill(pid, SIGKILL);

	return 0;
}

/*
	// line to script:
	// ME/THEM server channel nick :text
	// ME: line entered by local user
	// THEM: line coming in from irc server
	//
	// processing by f-irc:
	// read line
	// if direction = ME
		// if first word = 'REPLACE'
			// send replace string instead of my line to irc server
		// else (first word == 'KEEP')
			// send org line to irc server
	// else (direction == THEM)
		// print current line &

		// if first word = 'REPLY'
			// send replacement to irc server
	// endif
*/
int send_to_script(script_instances_t *si, server *ps, channel *pc, const char *user, const char *line, BOOL me, char **replacement, char **command)
{
	char *temp = NULL, *space = NULL;
	const char *direction = me ? "ME" : "THEM";
	char *str = NULL, *p = NULL;

	int len = asprintf(&str, "%s %s %s %s :%s\n", direction, ps -> server_host, pc -> channel_name, user, line);
	p = str;

	while(len > 0)
	{
		int rc = write(si -> wfd, p, len);

		if (rc <= 0)
		{
			free(str);

			return -1;
		}

		len -= rc;
		p += rc;
	}

	free(str);

	temp = calloc(1, 1);

	/* FIXME timeout */
	for(;;)
	{
		int len = strlen(temp);
		char buffer[4096] = { 0 };
		int rc = read(si -> rfd, buffer, sizeof buffer);

		if (rc <= 0)
		{
			free(temp);

			return -1;
		}

		temp = realloc(temp, len + rc + 1);
		memcpy(&temp[len], buffer, rc);
		temp[len + rc] = 0x00;

		if (strchr(temp, '\n'))
			break;
	}

	space = strchr(temp, ' ');
	if (!space)
	{
		free(temp);

		return -1;
	}

	*replacement = strdup(space + 1);

	*space = 0x00;
	*command = temp;

	return 0;
}

void add_channel_script(channel *pc, const char *scr)
{
	script_instances_t *cur = NULL;

	pc -> scripts = (script_instances_t *)realloc(pc -> scripts, sizeof(script_instances_t) * (pc -> n_scripts + 1));

	cur = &pc -> scripts[pc -> n_scripts];

	start_script(scr, &cur -> rfd, &cur -> wfd, &cur -> pid);

	cur -> filename = strdup(scr);

	pc -> n_scripts++;
}

void process_scripts(server *ps, channel *pc, const char *user, const char *line, BOOL me, char **new_line, char **command)
{
	int index = -1;

	*new_line = *command = NULL;

	for(index=0; index<pc -> n_scripts; index++)
	{
		script_instances_t *cur = &pc -> scripts[index];

		if (send_to_script(cur, ps, pc, user, line, me, new_line, command) == -1)
		{
			int n_to_move = (pc -> n_scripts - index) - 1;

			popup_notify(FALSE, "Script\n%s\nterminated", cur -> filename);

			end_script(cur -> pid);

			myfree(cur -> filename);

			if (n_to_move > 0)
				memmove(&pc -> scripts[index], &pc -> scripts[index + 1], n_to_move * sizeof(script_instances_t));

			index--;
		}
		else if (new_line != NULL && command != NULL)
		{
			break;
		}
	}
}
