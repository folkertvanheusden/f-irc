/* GPLv2 applies
 * SVN revision: $Revision: 819 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#ifndef __NAMES_H__
#define __NAMES_H__

extern int names_offset, names_cursor;

void free_person(person_t *p);
void delete_index(int server_index, int channel_index, int name_index);
void change_nick(int server_index, int channel_index, const char *nick, const char *new_nick);
void change_name(int server_index, int channel_index, const char *nick, const char *new_name);
void change_user_host(int server_index, int channel_index, const char *nick, const char *new_user_host);
void search_for_nick(channel *cur_channel, const char *nick, int *found_at, int *insert_at);
void add_nick(int server_index, int channel_index, const char *nick, const char *complete_name, const char *user_host);
BOOL has_nick(int sr, int ch, const char *nick);
void delete_from_channel_by_nick(int sr, int ch, const char *nick);
void delete_by_nick(int server_index, const char *nick);
void update_user_host(int sr, const char *prefix);
void replace_nick(int server_index, const char *old_nick, const char *new_nick);
irc_user_mode_t get_nick_mode(int server_index, int channel_index, const char *nick);
void set_nick_mode(int server_index, int channel_index, const char *nick, irc_user_mode_t mode);
irc_user_mode_t text_to_nick_mode(const char *nick);
void free_names_list(channel *pc);
void show_name_list(int server_index, int channel_index, NEWWIN *channel_window);
void go_to_last_name(void);
void do_names_keypress(int c);
BOOL has_nick_mode(const char *nick);
BOOL is_ignored(int sr, int ch, const char *nick);
BOOL unignore_nick(int sr, int ch, const char *nick, BOOL *was);
BOOL ignore_nick(int sr, int ch, const char *nick, BOOL *was);

#endif
