/* GPLv2 applies
 * SVN revision: $Revision: 810 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#define _GNU_SOURCE
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

#include "utils.h"
#include "irc.h"
#include "loop.h"
#include "config.h"
#include "dcc.h"

char *exec_and_strip_ctcp(int sr, int ch, const char *nick_complete, const char *in, BOOL *me)
{
	server *ps = &server_list[sr];
	int len = strlen(in), rc = 0;
	char *out = (char *)calloc(len * 2 + 1, 1), *p = out;
	char *nick = strdup(nick_complete);

	memcpy(out, in, len + 1);

	terminate_str(nick, '!');

	LOG("exec_and_strip_ctcp(%s, %s)\n", nick, out);

	*me = FALSE;

	for(;;)
	{
		BOOL skip = FALSE;
		int n_to_move = 0;
		char *marker = strchr(p, '\001'), *end_marker = NULL;

		if (!marker)
			break;

		end_marker = strchr(marker + 1, '\001');

		if (!end_marker)
			LOG("no end-marker? '%s'\n", in);

		if (strncmp(marker, "\001ACTION ", 8) == 0)
		{
			char *work = strdup(&marker[8]);
			char *temp = NULL;

			terminate_str(work, '\001');

			asprintf(&temp, "* \002%s\002 %s", nick, work);

			strcpy(out, temp);

			*me = TRUE;

			free(temp);
			free(work);
			break;
		}
		else if (strncmp(marker, "\001VERSION\001", 9) == 0)
		{
			const char *reply = "f-irc v" VERSION " by mail@vanheusden.com - http://www.vanheusden.com/f-irc/ (" __DATE__ " " __TIME__ ")";

			update_statusline(sr, ch, "CTCP VERSION %s: %s", nick, reply);

			if (do_send(ps -> fd, "NOTICE %s :\001VERSION %s\001", nick, reply) == -1)
			{
				rc = -1;
				break;
			}
		}
		else if (strncmp(marker, "\001TIME\001", 6) == 0)
		{
			time_t now = time(NULL);
			char *ts = ctime(&now);

			terminate_str(ts, '\n');

			update_statusline(sr, ch, "CTCP TIME %s: %s", nick, ts);

			if (do_send(ps -> fd, "NOTICE %s :\001TIME %s\001", nick, ts) == -1)
			{
				rc = -1;
				break;
			}
		}
		else if (strncmp(marker, "\001FINGER\001", 8) == 0)
		{
			const char *reply = finger_str ? finger_str : "http://vanheusden.com/f-irc/";

			update_statusline(sr, ch, "CTCP FINGER %s: %s", nick, reply);

			if (do_send(ps -> fd, "NOTICE %s :\001FINGER %s\001", nick, reply) == -1)
			{
				rc = -1;
				break;
			}
		}
		else if (strncmp(marker, "\001PING", 5) == 0)
		{
			/* make copy and end at the ^A (which ends the ctcp
			 * command)
			 */
			BOOL is_reply = FALSE;
			char *ping = strdup(marker + 1);
			char *space = strchr(ping, ' ');

			terminate_str(ping, '\001');

			if (space && ps -> user_ping != NULL)
			{
				while(*space == ' ')
					space++;

				if (atoi(space) == ps -> user_ping_id)	/* is it a reply? */
				{
					char *str_buffer = NULL;

					asprintf(&str_buffer, "Ping to %s took aproximately %f seconds", ps -> user_ping, get_ts() - ps -> t_user_ping);

					if (log_channel(sr, ch, NULL, str_buffer, TRUE) == -1)
						rc = -1;

					free(str_buffer);

					myfree(ps -> user_ping);
					ps -> user_ping = NULL;

					is_reply = TRUE;
				}
			}

			if (is_reply == FALSE)
			{
				update_statusline(sr, ch, "CTCP PING %s: %s", nick, ping);

				if (do_send(ps -> fd, "NOTICE %s :\001%s\001", nick, ping) == -1)
					rc = -1;
			}

			myfree(ping);

			if (rc == -1)
				break;
		}
		else if (strncmp(marker, "\001CLIENTINFO\001", 12) == 0)
		{
			const char *reply = "PING DCC FINGER TIME ACTION USERINFO CLIENTINFO";

			update_statusline(sr, ch, "CTCP CLIENTINFO %s: %s", nick, reply);

			if (do_send(ps -> fd, "NOTICE %s :\001CLIENTINFO %s\001", nick, reply) == -1)
			{
				rc = -1;
				break;
			}
		}
		else if (strncmp(marker, "\001USERINFO\001", 10) == 0 && allow_userinfo)
		{
			char *reply = NULL;

			if (userinfo && strlen(userinfo) > 0)
				reply = strdup(userinfo);
			else
			{
				struct passwd *p = NULL;

				setpwent();

				while((p = getpwent()) != NULL)
				{
					if (p -> pw_uid == getuid())
						break;
				}

				if (p)
					asprintf(&reply, "%s (%s)", p -> pw_gecos, p -> pw_name);
			}

			if (reply)
			{
				update_statusline(sr, ch, "CTCP USERINFO %s: %s", nick, reply);

				if (do_send(ps -> fd, "NOTICE %s :\001USERINFO %s\001", nick, reply) == -1)
					rc = -1;

				free(reply);
			}

			if (rc == -1)
				break;
		}
		else if (strncmp(marker, "\001DCC SEND ", 10) == 0)
		{
			char *fname = NULL, *pars = NULL;
			char *dcc = strdup(marker + 10);
			string_array_t parts;

			terminate_str(dcc, '\001');

			pars = strchr(dcc, ' ');

			if (dcc[0] == '"')
			{
				char *end_fname = strchr(dcc + 1, '"');

				if (!end_fname)
					end_fname = pars;
				else
					pars = end_fname;

				if (end_fname)
				{
					*end_fname = 0x00;

					fname = strdup(dcc + 1);
				}
			}
			else
			{
				if (pars)
					*pars = 0x00;

				fname = strdup(dcc);
			}

			/* skip over ' ' and '"' */
			if (pars)
				pars++;
			/* seek to end of file name */
			while(pars && *pars == ' ')
				pars++;

			init_string_array(&parts);
			split_string(pars, " ", TRUE, &parts);

			/* dcc: file ip port [size] */
			/* ^ADCC SEND flok.txt 2a02:898:62:f6::ae 38113 4^A */
			if (string_array_get_n(&parts) >= 3)
			{
				const char *ip = string_array_get(&parts, 0);
				int port = atoi(string_array_get(&parts, 1));
				int size = atoi(string_array_get(&parts, 2));

				update_statusline(sr, ch, "DCC: receive %s (%d bytes) from %s ([%s]:%d)", fname, size, nick, ip, port);

				init_recv_dcc(fname, ip, port, sr, ch);
			}

			free_splitted_string(&parts);

			myfree(fname);

			myfree(dcc);
		}
		else
		{
			LOG("CTCP request '%s' not known\n", marker);

			skip = TRUE;
		}

		if (skip)
			p = end_marker + 1;
		else
		{
			n_to_move = strlen(end_marker + 1) + 1;

			if (n_to_move)
				memmove(marker, end_marker + 1, n_to_move);

			p = marker;
		}
	}

	if (strlen(out) == 0)
	{
		free(out);
		out = NULL;

		asprintf(&out, "(incoming CTCP request)");
	}

	free(nick);

	if (rc == -1)
	{
		free(out);
		return NULL;
	}

	return out;
}
