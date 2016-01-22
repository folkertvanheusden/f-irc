/* GPLv2 applies
 * SVN revision: $Revision: 710 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#ifndef __IRC_H__
#define __IRC_H__

#define IRC_MAX_MSG_LEN	512

#include "servers.h"

BOOL is_channel(const char *name);

int do_send(int fd, const char *format, ...);

int process_server_do_line(int sr, const char *string);
int irc_login1(server *pserver);
int irc_login2(server *pserver);

int create_channel(int server_index, const char *channel_index);

int irc_nick(int fd, const char *nick);
int irc_privmsg(int fd, const char *channel_name, const char *msg);
int irc_kick(int fd, const char *channel_name, const char *nick, const char *comment);
int irc_ban(int fd, const char *channel_name, const char *nick);
int irc_op(int fd, const char *channel_name, const char *nick, BOOL op);
int irc_whois(int fd, const char *nick);
int irc_allowspeak(int fd, const char *channel_name, const char *nick, BOOL speak);
int irc_who(int fd, const char *channel_name);
int irc_topic(int fd, const char *channel_name, const char *topic);
int irc_ping(int fd, const char *nick, int ts);
int irc_join(int fd, const char *channel_name);
int irc_version(int fd);
int irc_list(int fd);
int irc_part(int fd, const char *channel_name, const char *msg);
int irc_quit(int fd, const char *msg);
int irc_v3_cap_start(int fd);
int irc_v3_cap_end(int fd);
int irc_time(int fd);

#endif
