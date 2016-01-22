/* GPLv2 applies
 * SVN revision: $Revision: 864 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#include "gen.h"
#include "buffer.h"
#include "channels.h"
#include "grep_filter.h"

typedef enum { CNF_BOOL, CNF_VALUE, CNF_STRING, CNF_COLOR } cnf_entry_t;

typedef struct
{
	const char *name;
	cnf_entry_t type;
	void *p;
	void (*dofunc)(void);
} cnf_entry;

extern cnf_entry cnf_pars[];

extern BOOL nick_color;
extern BOOL colors_meta, colors_all, use_nonbasic_colors;
extern BOOL inverse_window_heading;
extern BOOL partial_highlight_match;
extern BOOL auto_private_channel;
extern const char *part_message, *server_exit_message;
extern int max_channel_record_lines;
extern int delay_before_reconnect;
extern BOOL highlight, fuzzy_highlight;
extern BOOL topic_scroll;
extern BOOL notice_in_server_channel;
extern BOOL allow_invite;
extern BOOL store_config_on_exit;
extern BOOL show_parts;
extern BOOL show_mode_changes;
extern BOOL show_nick_change;
extern BOOL show_joins;
extern BOOL auto_rejoin;
extern BOOL mark_personal_messages;
extern BOOL update_clock_at_data;
extern BOOL irc_keepalive;
extern BOOL auto_reconnect;
extern BOOL space_after_start_marker;
extern BOOL allow_userinfo;
extern BOOL ignore_mouse;
extern BOOL jumpy_navigation;
extern BOOL user_column;
extern BOOL mark_meta;
extern BOOL full_user;
extern BOOL show_headlines;
extern BOOL auto_markerline;
extern BOOL ignore_unknown_irc_protocol_msgs;
extern BOOL only_one_markerline;
extern BOOL keep_channels_sorted;
extern BOOL create_channel_for_meta_requests;
extern int user_column_width;
extern const char *userinfo;
extern int nick_sleep;
extern favorite *favorite_channels;
extern int n_favorite_channels, favorite_channels_index;
extern char *conf_file;
extern const char *log_dir;
extern const char *notify_nick;
extern const char *dcc_bind_to;
extern const char *xclip;
extern grep_target *gp; /* grep filter */
extern grep_target *hlgp; /* headline grep filter */

extern const char *finger_str; /* ? */
extern int check_for_mail;

int load_config(const char *file);
void add_favorite(const char *serv, const char *chan);
void free_favorites(void);
BOOL save_config(BOOL save_channels, char **err_msg);
int config_color_str_convert(const char *in, int linenr, const char *subj);
int parse_color_spec(const char *par, int linenr, const char *subj);
