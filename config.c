/* GPLv2 applies
 * SVN revision: $Revision: 887 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ncursesw/ncurses.h>
#include <ncursesw/panel.h>

#include "gen.h"
#include "error.h"
#include "term.h"
#include "buffer.h"
#include "channels.h"
#include "servers.h"
#include "utils.h"
#include "irc.h"
#include "loop.h"
#include "grep_filter.h"
#include "main.h"
#include "wordcloud.h"
#include "dcc.h"
#include "theme.h"
#include "config.h"
#include "colors.h"
#include "ignores.h"
#include "string_array.h"
#include "dictionary.h"
#include "headlines.h"
#include "script.h"

BOOL topic_scroll = FALSE;
BOOL partial_highlight_match = FALSE;
const char *part_message = NULL, *server_exit_message = NULL;
int check_for_mail = 0;
int delay_before_reconnect = 31;
int max_channel_record_lines;
BOOL auto_private_channel;
BOOL highlight = FALSE, fuzzy_highlight = FALSE;
BOOL notice_in_server_channel = FALSE;
BOOL colors_meta = TRUE, colors_all = TRUE;
BOOL allow_invite = TRUE;
BOOL store_config_on_exit = FALSE;
BOOL show_parts = TRUE;
BOOL show_mode_changes = TRUE;
BOOL show_nick_change = TRUE;
BOOL show_joins = TRUE;
BOOL auto_rejoin = FALSE;
BOOL nick_color = FALSE;
BOOL use_nonbasic_colors = FALSE;
char *conf_file = NULL;
const char *finger_str = NULL;
BOOL inverse_window_heading = TRUE;
BOOL remember_channels = TRUE;
BOOL mark_personal_messages = FALSE;
BOOL update_clock_at_data = FALSE;
BOOL irc_keepalive = TRUE;
BOOL auto_reconnect = TRUE;
BOOL space_after_start_marker = FALSE;
BOOL allow_userinfo = TRUE;
BOOL ignore_mouse = TRUE;
BOOL jumpy_navigation = FALSE;
BOOL user_column = FALSE;
BOOL mark_meta = TRUE;
BOOL full_user = FALSE;
BOOL show_headlines = FALSE;
BOOL auto_markerline = FALSE;
BOOL ignore_unknown_irc_protocol_msgs = TRUE;
BOOL only_one_markerline = TRUE;
BOOL keep_channels_sorted = FALSE;
BOOL create_channel_for_meta_requests = FALSE;
int user_column_width = 8;
int nick_sleep = 4;
const char *userinfo = NULL;
const char *log_dir = NULL;
const char *notify_nick = NULL;
const char *dcc_bind_to = NULL;
const char *xclip = "/usr/bin/xclip";
grep_target *gp = NULL;
grep_target *hlgp = NULL;

favorite *favorite_channels = NULL;
int n_favorite_channels = 0, favorite_channels_index = 0;

cnf_entry cnf_pars[] = {
	{ "enable colors", CNF_BOOL, &colors_all, NULL },
	{ "use non basic colors", CNF_BOOL, &use_nonbasic_colors, NULL },
	{ "nick color", CNF_BOOL, &nick_color, NULL },
	{ "meta-texts colors", CNF_BOOL, &colors_meta, NULL },
	{ "default colors", CNF_COLOR, &default_colorpair, NULL },
	{ "highlight colors", CNF_COLOR, &highlight_colorpair, NULL },
	{ "highlight", CNF_BOOL, &highlight, NULL },
	{ "fuzzy highlight match", CNF_BOOL, &fuzzy_highlight, NULL },
	{ "meta colors", CNF_COLOR, &meta_colorpair, NULL },
	{ "error colors", CNF_COLOR, &error_colorpair, NULL },
	{ "temp colors", CNF_COLOR, &temp_colorpair, NULL },
	{ "inverse window heading", CNF_BOOL, &inverse_window_heading, NULL },
	{ "partial highlight match", CNF_BOOL, &partial_highlight_match, NULL },
	{ "markerline colors", CNF_COLOR, &markerline_colorpair, NULL },
	{ "marker line when changing channel", CNF_BOOL, &auto_markerline, NULL },
	{ "show only the last marker line", CNF_BOOL, &only_one_markerline, NULL },
	{ "scroll topic", CNF_BOOL, &topic_scroll, NULL },
	{ "only update clock when data comes in", CNF_BOOL, &update_clock_at_data, NULL },
	{ "space between \">\" and input line", CNF_BOOL, &space_after_start_marker, NULL },
	{ "channel buffering", CNF_VALUE, &max_channel_record_lines, NULL },
	{ "keep channels sorted", CNF_BOOL, &keep_channels_sorted, NULL },
	{ "mark personal messages-channels", CNF_BOOL, &mark_personal_messages, NULL },
	{ "star meta messages", CNF_BOOL, &mark_meta, NULL },
	{ "align nicks in a column", CNF_BOOL, &user_column, NULL },
	{ "nick column width", CNF_VALUE, &user_column_width, NULL },
	{ "full user: include hostname", CNF_BOOL, &full_user, NULL },
	{ "show headlines", CNF_BOOL, &show_headlines, apply_show_headlines },
	{ "word cloud window height (=enable)", CNF_VALUE, &word_cloud_n, apply_show_wordcloud },
	{ "DCC path", CNF_STRING, &dcc_path, NULL },
	{ "DCC network interface to bind to", CNF_STRING, &dcc_bind_to, NULL },
	{ "ignores file", CNF_STRING, &ignore_file, NULL },
	{ "logging directory", CNF_STRING, &log_dir, NULL },
	{ "program to invoke when nick matches", CNF_STRING, &notify_nick, NULL },
	{ "store configuration on exit", CNF_BOOL, &store_config_on_exit, NULL },
	{ "IRC ping (detect off-line sooner)", CNF_BOOL, &irc_keepalive, NULL },
	{ "suppress unknown irc protocol msgs", CNF_BOOL, &ignore_unknown_irc_protocol_msgs, NULL },
	{ "delay before connect", CNF_VALUE, &delay_before_reconnect, NULL },
	{ "auto reconnect when connection lost", CNF_BOOL, &auto_reconnect, NULL },
	{ "allow USERINFO CTCP request", CNF_BOOL, &allow_userinfo, NULL },
	{ "USERINFO string, empty for default", CNF_STRING, &userinfo, NULL },
	{ "FINGER string, empty for default", CNF_STRING, &finger_str, NULL },
	{ "part message", CNF_STRING, &part_message, NULL },
	{ "leave a server message", CNF_STRING, &server_exit_message, NULL },
	{ "automatically create private channel", CNF_BOOL, &auto_private_channel, NULL },
	{ "notices in server window", CNF_BOOL, &notice_in_server_channel, NULL },
	{ "create channel for meta requests", CNF_BOOL, &create_channel_for_meta_requests, NULL },
	{ "show PARTs", CNF_BOOL, &show_parts, NULL },
	{ "show JOINs", CNF_BOOL, &show_joins, NULL },
	{ "show mode changes", CNF_BOOL, &show_mode_changes, NULL },
	{ "show nick changes", CNF_BOOL, &show_nick_change, NULL },
	{ "allow invites", CNF_BOOL, &allow_invite, NULL },
	{ "automatically re-join after kick", CNF_BOOL, &auto_rejoin, NULL },
	{ "^W/R/Z/X: jumpy navigation", CNF_BOOL, &jumpy_navigation, NULL },
	{ "ignore mouse clicks", CNF_BOOL, &ignore_mouse, apply_mouse_setting },
	{ "path to xclip program", CNF_STRING, &xclip, NULL },
	{ "set time string (man strftime)", CNF_STRING, &time_str_fmt, NULL },
	{ NULL, 0, NULL }
	};

void add_favorite(const char *serv, const char *chan)
{
	favorite_channels = realloc(favorite_channels, (n_favorite_channels + 1) * sizeof(favorite));
	favorite_channels[n_favorite_channels].server = serv ? strdup(serv) : NULL;

	favorite_channels[n_favorite_channels].channel = strdup(chan);

	n_favorite_channels++;
}

void free_favorites(void)
{
	int loop = 0;

	for(loop=0; loop<n_favorite_channels; loop++)
	{
		myfree(favorite_channels[loop].server);
		myfree(favorite_channels[loop].channel);
	}

	myfree(favorite_channels);
	favorite_channels = NULL;

	n_favorite_channels = 0;
}

BOOL is_auto_join(server *ps, const char *channel)
{
	int achl = 0;

	for(achl=0; achl<string_array_get_n(&ps -> auto_join); achl++)
	{
		if (strcasecmp(string_array_get(&ps -> auto_join, achl), channel) == 0)
			return TRUE;
	}

	return FALSE;
}

void add_filter(grep_target *gpt, const char *par, int linenr)
{
	char *err = NULL;
	string_array_t parts;

	init_string_array(&parts);

	split_string(par, ",", FALSE, &parts);

	if (string_array_get_n(&parts) != 4)
		error_exit(FALSE, "Parameter(s) missing for filter: %s (line %d)\n", err, linenr);

	if (add_grep_filter(gpt, string_array_get(&parts, 3), string_array_get(&parts, 0), string_array_get(&parts, 1), string_array_get(&parts, 2), &err) == FALSE)
		error_exit(FALSE, "Failed processing filter: %s (line %d)\n", err, linenr);

	free_splitted_string(&parts);
}

const char *true_false_str(BOOL val)
{
	if (val)
		return "true";

	return "false";
}

BOOL save_config(BOOL save_channels, char **err_msg)
{
	BOOL failed = FALSE;
	int loop = 0;
	FILE *fh = NULL;

	*err_msg = NULL;

	if (!conf_file)	
	{
		*err_msg = strdup("No configuration file selected");
		return FALSE;
	}

	fh = fopen(conf_file, "wb");

	if (!fh)
	{
		asprintf(err_msg, "Cannot open %s for write access", conf_file);
		return FALSE;
	}

	failed |= fprintf(fh, "keep_channels_sorted=%s\n", true_false_str(keep_channels_sorted)) == -1;

	failed |= fprintf(fh, "default_colorpair=") == -1;
	emit_colorpair(fh, default_colorpair);
	failed |= fprintf(fh, "\n") == -1;
	failed |= fprintf(fh, "highlight_colorpair=") == -1;
	emit_colorpair(fh, highlight_colorpair);
	failed |= fprintf(fh, "\n") == -1;
	failed |= fprintf(fh, "meta_colorpair=") == -1;
	emit_colorpair(fh, meta_colorpair);
	failed |= fprintf(fh, "\n") == -1;
	failed |= fprintf(fh, "error_colorpair=") == -1;
	emit_colorpair(fh, error_colorpair);
	failed |= fprintf(fh, "\n") == -1;
	failed |= fprintf(fh, "temp_colorpair=") == -1;
	emit_colorpair(fh, temp_colorpair);
	failed |= fprintf(fh, "\n") == -1;
	failed |= fprintf(fh, "markerline_colorpair=") == -1;
	emit_colorpair(fh, markerline_colorpair);
	failed |= fprintf(fh, "\n") == -1;

	failed |= fprintf(fh, "\n") == -1;

	for(loop=0; loop<n_servers && !failed; loop++)
	{
		int sal = 0, achl = 0, ch = 0;
		server *ps = &server_list[loop];

		failed |= fprintf(fh, "server=%s:%d\n", ps -> server_host, ps -> server_port) == -1;
		failed |= fprintf(fh, "description=%s\n", ps -> description ? ps -> description : ps -> server_host) == -1;
		failed |= fprintf(fh, "name=%s\n", ps -> user_complete_name) == -1;
		failed |= fprintf(fh, "username=%s\n", ps -> username) == -1;
		failed |= fprintf(fh, "password=%s\n", ps -> password) == -1;
		failed |= fprintf(fh, "nickname=%s\n", ps -> nickname) == -1;

		for(achl=0; achl<string_array_get_n(&ps -> auto_join) && !failed; achl++)
			failed |= fprintf(fh, "auto_join=%s\n", string_array_get(&ps -> auto_join, achl)) == -1;

		for(sal=0; sal<string_array_get_n(&ps -> send_after_login) && !failed; sal++)
			failed |= fprintf(fh, "send_after_login=%s\n", string_array_get(&ps -> send_after_login, sal)) == -1;

		if (remember_channels)
		{
			int chl = 0;

			for(chl=0; chl<ps -> n_channels && !failed; chl++)
			{
				const char *cur_channel = ps -> pchannels[chl].channel_name;

				if (!is_auto_join(ps, cur_channel))
					failed |= fprintf(fh, "rejoin=%s\n", cur_channel) == -1;
			}
		}

		for(ch=0; ch<ps -> n_channels && !failed; ch++)
		{
			channel *pc = &ps -> pchannels[ch];

			if (remember_channels || is_auto_join(ps, pc -> channel_name))
			{
				int si = 0;

				for(si=0; si<pc -> n_scripts && !failed; si++)
					failed |= fprintf(fh, "script=%s %s\n", pc -> channel_name, pc -> scripts[si].filename);
			}
		}

		failed |= fprintf(fh, ";\n") == -1;
	}

	for(loop=0; loop<n_favorite_channels && !failed; loop++)
	{
		favorite *pf = &favorite_channels[loop];

		if (pf -> server)
			failed |= fprintf(fh, "favorite=%s %s\n", pf -> server, pf -> channel) == 0;
		else
			failed |= fprintf(fh, "favorite=%s\n", pf -> channel) == 0;

		if (failed)
			break;
	}

	if (n_favorite_channels)
		failed |= fprintf(fh, ";\n") == -1;

	failed |= fprintf(fh, "time_str_fmt=%s\n", time_str_fmt) == -1;
	failed |= fprintf(fh, "all-colors=%s\n", true_false_str(colors_all)) == -1;
	failed |= fprintf(fh, "meta-colors=%s\n", true_false_str(colors_meta)) == -1;
	failed |= fprintf(fh, "nick-color=%s\n", true_false_str(nick_color)) == -1;
	failed |= fprintf(fh, "partial_highlight_match=%s\n", true_false_str(partial_highlight_match)) == -1;
	failed |= fprintf(fh, "notice_in_serverchannel=%s\n", true_false_str(notice_in_server_channel)) == -1;
	failed |= fprintf(fh, "highlight=%s\n", true_false_str(highlight)) == -1;
	failed |= fprintf(fh, "auto_private_channel=%s\n", true_false_str(auto_private_channel)) == -1;
	failed |= fprintf(fh, "update_clock_at_data=%s\n", true_false_str(update_clock_at_data)) == -1;
	failed |= fprintf(fh, "max_channel_record_lines=%d\n", max_channel_record_lines) == -1;
	failed |= fprintf(fh, "word_cloud_n=%d\n", word_cloud_n) == -1;
	failed |= fprintf(fh, "delay_before_reconnect=%d\n", delay_before_reconnect) == -1;
	failed |= fprintf(fh, "word_cloud_refresh=%d\n", word_cloud_refresh) == -1;
	failed |= fprintf(fh, "word_cloud_win_height=%d\n", word_cloud_win_height) == -1;
	failed |= fprintf(fh, "word_cloud_min_word_size=%d\n", word_cloud_min_word_size) == -1;
	failed |= fprintf(fh, "store_config_on_exit=%s\n", true_false_str(store_config_on_exit)) == -1;
	failed |= fprintf(fh, "topic_scroll=%s\n", true_false_str(topic_scroll)) == -1;
	failed |= fprintf(fh, "allow_invite=%s\n", true_false_str(allow_invite)) == -1;
	failed |= fprintf(fh, "show_parts=%s\n", true_false_str(show_parts)) == -1;
	failed |= fprintf(fh, "show_mode_changes=%s\n", true_false_str(show_mode_changes)) == -1;
	failed |= fprintf(fh, "show_nick_change=%s\n", true_false_str(show_nick_change)) == -1;
	failed |= fprintf(fh, "show_joins=%s\n", true_false_str(show_joins)) == -1;
	failed |= fprintf(fh, "auto_rejoin=%s\n", true_false_str(auto_rejoin)) == -1;
	failed |= fprintf(fh, "use_nonbasic_colors=%s\n", true_false_str(use_nonbasic_colors)) == -1;
	failed |= fprintf(fh, "inverse_window_heading=%s\n", true_false_str(inverse_window_heading)) == -1;
	failed |= fprintf(fh, "remember_channels=%s\n", true_false_str(remember_channels)) == -1;
	failed |= fprintf(fh, "mark_personal_messages=%s\n", true_false_str(mark_personal_messages)) == -1;
	failed |= fprintf(fh, "space_after_start_marker=%s\n", true_false_str(space_after_start_marker)) == -1;
	failed |= fprintf(fh, "fuzzy_highlight=%s\n", true_false_str(fuzzy_highlight)) == -1;
	failed |= fprintf(fh, "allow_userinfo=%s\n", true_false_str(allow_userinfo)) == -1;
	failed |= fprintf(fh, "ignore_mouse=%s\n", true_false_str(ignore_mouse)) == -1;
	failed |= fprintf(fh, "jumpy_navigation=%s\n", true_false_str(jumpy_navigation)) == -1;
	failed |= fprintf(fh, "user_column=%s\n", true_false_str(user_column)) == -1;
	failed |= fprintf(fh, "user_column_width=%d\n", user_column_width) == -1;
	failed |= fprintf(fh, "mark_meta=%s\n", true_false_str(mark_meta)) == -1;
	failed |= fprintf(fh, "full_user=%s\n", true_false_str(full_user)) == -1;
	failed |= fprintf(fh, "show_headlines=%s\n", true_false_str(show_headlines)) == -1;
	failed |= fprintf(fh, "irc_keepalive=%s\n", true_false_str(irc_keepalive)) == -1;
	failed |= fprintf(fh, "auto_markerline=%s\n", true_false_str(auto_markerline)) == -1;
	failed |= fprintf(fh, "ignore_unknown_irc_protocol_msgs=%s\n", true_false_str(ignore_unknown_irc_protocol_msgs)) == -1;
	failed |= fprintf(fh, "only_one_markerline=%s\n", true_false_str(only_one_markerline)) == -1;

	if (xclip)
		failed |= fprintf(fh, "xclip=%s\n", xclip) == -1;

	if (dcc_bind_to)
		failed |= fprintf(fh, "dcc_bind_to=%s\n", dcc_bind_to) == -1;

	if (log_dir)
		failed |= fprintf(fh, "log_dir=%s\n", log_dir) == -1;

	if (userinfo)
		failed |= fprintf(fh, "userinfo=%s\n", userinfo) == -1;

	if (notify_nick)
		failed |= fprintf(fh, "notify_nick=%s\n", notify_nick) == -1;

	if (dictionary_file)
		failed |= fprintf(fh, "dictionary_file=%s\n", dictionary_file) == -1;

	if (finger_str)
		failed |= fprintf(fh, "finger_str=%s\n", finger_str) == -1;

	if (dcc_path)
		failed |= fprintf(fh, "dcc_path=%s\n", dcc_path) == -1;

	if (ignore_file)
		failed |= fprintf(fh, "ignore_file=%s\n", ignore_file) == -1;

	if (theme_file)
		failed |= fprintf(fh, "theme=%s\n", theme_file) == -1;

	if (part_message)
		failed |= fprintf(fh, "part_message=%s\n", part_message) == -1;

	if (server_exit_message)
		failed |= fprintf(fh, "server_exit_message=%s\n", server_exit_message) == -1;

	failed |= dump_grep_filters(gp, "grep_filter", fh) == FALSE;

	failed |= dump_grep_filters(hlgp, "headline_filter", fh) == FALSE;

	failed |= dump_headline_matchers(fh) == FALSE;

	failed |= dump_string_array(&extra_highlights, "extra_highlights", fh) == FALSE;

	if (failed)
	{
		asprintf(err_msg, "Failed writing to %s: disk full?", conf_file);
		fclose(fh);
		return FALSE;
	}

	asprintf(err_msg, "All fine, written %ld bytes to %s", ftell(fh), conf_file);

	if (fclose(fh))
	{
		asprintf(err_msg, "Problem finalizing %s", conf_file);
		return FALSE;
	}

	return TRUE;
}

int load_config(const char *file)
{
	char *description = NULL, *server_host = NULL, *username = NULL, *password = NULL, *nickname = NULL, *user_complete_name = NULL;
	int server_index = -1;
	int linenr = 0;
	int fd = open(file, O_RDONLY);

	if (fd == -1)
	{
		if (errno == ENOENT)
			return -1;

		error_exit(TRUE, "Cannot open config file %s\n", file);
	}

	conf_file = strdup(file);

	for(;;)
	{
		char *line = read_line_fd(fd);
		char *cmd, *par;
		char *is;

		if (!line)
			break;

		linenr++;

		if (strlen(line) == 0)
		{
			myfree(line);
			continue;
		}

		if (line[0] == '#' || line[0] == ';')
		{
			myfree(line);
			continue;
		}

		is = strchr(line, '=');
		if (!is)
			error_exit(FALSE, "config: line %d is missing either command or parameter! (%s)", linenr, line);

		/* find parameter */
		par = is + 1;
		while(*par == ' ')
			par++;

		/* remove spaces around command */
		/* spaces at the start */
		cmd = line;
		while(*cmd == ' ')
			cmd++;
		/* spaces at the end */
		*is = 0x00;
		is--;
		while(*is == ' ')
		{
			*is = 0x00;
			is--;
		}

		if (strcmp(cmd, "server") == 0 || strcmp(cmd, "send_after_login") == 0 || strcmp(cmd, "auto_join") == 0 || strcmp(cmd, "channel") == 0 || strcmp(cmd, "rejoin") == 0 || strcmp(cmd, "script") == 0)
		{
			/* all stuff already known? */
			if (server_host)
			{
				if (nickname == NULL)
					error_exit(FALSE, "nickname must be set for %s", server_host);

				server_index = add_server(server_host, username, password, nickname, user_complete_name, description ? description : server_host);
				myfree(server_host);
				server_host = NULL;
				myfree(username);
				myfree(password);
				myfree(nickname);
				myfree(user_complete_name);
				myfree(description);

				username = password = nickname = user_complete_name = description = NULL;
			}
		}

		if (strcmp(cmd, "server") == 0)
		{
			/* new server */
			server_host = strdup(par);
		}
		else if (strcmp(cmd, "favorite") == 0)
		{
			int n = -1;
			string_array_t parts;

			init_string_array(&parts);

			split_string(par, " ", TRUE, &parts);
			n = string_array_get_n(&parts);

			if (n != 1 && n != 2)
				error_exit(FALSE, "favorite needs either be in format \"server channel\" or \"channel\"");

			if (n == 2)
				add_favorite(string_array_get(&parts, 0), string_array_get(&parts, 1));
			else
				add_favorite(NULL, string_array_get(&parts, 0));

			free_splitted_string(&parts);
		}
		else if (strcmp(cmd, "username") == 0)
			username = strdup(par);
		else if (strcmp(cmd, "password") == 0)
			password = strdup(par);
		else if (strcmp(cmd, "nick") == 0 || strcmp(cmd, "nickname") == 0)
			nickname = strdup(par);
		else if (strcmp(cmd, "name") == 0)
			user_complete_name = strdup(par);
		else if (strcmp(cmd, "dictionary_file") == 0)
		{
			const char *filename = explode_path(par);

			if (!filename)
				error_exit(TRUE, "Path '%s' is not understood\n", par);

			dictionary_file = filename;

			if (load_dictionary() == FALSE)
				error_exit(TRUE, "Failure loading dictionary file %s (%s)", filename, par);
		}
		else if (strcmp(cmd, "description") == 0)
			description = strdup(par);
		else if (strcmp(cmd, "time_str_fmt") == 0)
			time_str_fmt = strdup(par);
		else if (strcmp(cmd, "server_exit_message") == 0)
			server_exit_message = strdup(par);
		else if (strcmp(cmd, "log_dir") == 0)
			log_dir = strdup(par);
		else if (strcmp(cmd, "part_message") == 0)
			part_message = strdup(par);
		else if (strcmp(cmd, "notify_nick") == 0)
			notify_nick = strdup(par);
		else if (strcmp(cmd, "userinfo") == 0)
			userinfo = strdup(par);
		else if (strcmp(cmd, "finger_str") == 0)
			finger_str = strdup(par);
		else if (strcmp(cmd, "mark_personal_messages") == 0)
			mark_personal_messages = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "meta-colors") == 0)
			colors_meta = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "headline_matcher") == 0)
			add_headline_matcher(par);
		else if (strcmp(cmd, "all-colors") == 0)
			colors_all = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "dcc_bind_to") == 0)
			dcc_bind_to = strdup(par);
		else if (strcmp(cmd, "xclip") == 0)
			xclip = strdup(par);
		else if (strcmp(cmd, "update_clock_at_data") == 0)
			update_clock_at_data = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "nick-color") == 0)
			nick_color = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "use_nonbasic_colors") == 0)
			use_nonbasic_colors = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "ignore_unknown_irc_protocol_msgs") == 0)
			ignore_unknown_irc_protocol_msgs = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "auto_markerline") == 0)
			auto_markerline = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "inverse_window_heading") == 0)
			inverse_window_heading = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "keep_channels_sorted") == 0)
			keep_channels_sorted = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "allow_invite") == 0)
			allow_invite = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "show_headlines") == 0)
			show_headlines = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "remember_channels") == 0)
			remember_channels = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "allow_userinfo") == 0)
			allow_userinfo = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "extra_highlights") == 0)
			add_to_string_array(&extra_highlights, par);
		else if (strcmp(cmd, "only_one_markerline") == 0)
			only_one_markerline = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "auto_rejoin") == 0)
			auto_rejoin = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "ignore_mouse") == 0)
			ignore_mouse = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "irc_keepalive") == 0)
			irc_keepalive = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "space_after_start_marker") == 0)
			space_after_start_marker = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "jumpy_navigation") == 0)
			jumpy_navigation = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "mark_meta") == 0)
			mark_meta = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "user_column") == 0)
			user_column = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "full_user") == 0)
			full_user = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "grep_filter") == 0)
			add_filter(gp, par, linenr);
		else if (strcmp(cmd, "headline_filter") == 0)
			add_filter(hlgp, par, linenr);
		else if (strcmp(cmd, "show_parts") == 0)
			show_parts = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "show_mode_changes") == 0)
			show_mode_changes = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "show_nick_change") == 0)
			show_nick_change = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "show_joins") == 0)
			show_joins = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "store_config_on_exit") == 0)
			store_config_on_exit = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "partial_highlight_match") == 0)
			partial_highlight_match = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "topic_scroll") == 0)
			topic_scroll = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "notice_in_serverchannel") == 0)
			notice_in_server_channel = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "highlight") == 0)
			highlight = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "fuzzy_highlight") == 0)
			fuzzy_highlight = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "theme") == 0)
		{
			struct stat status;
			const char *filename = explode_path(par);

			if (!filename)
				error_exit(TRUE, "Path '%s' is not understood\n", par);

			if (stat(filename, &status) == -1) 	/* file doesn't exist, look for it under SYSCONFDIR */
			{
				int len = strlen(SYSCONFDIR) + strlen(par) + 2;
				char *theme_path = malloc(len * sizeof(char));

				snprintf(theme_path, len, "%s/%s", SYSCONFDIR, par);
				load_theme(theme_path);

				theme_file = theme_path;
			} 
			else
			{
				load_theme(filename);

				theme_file = strdup(par);
			}

			myfree(filename);
		}
		else if (strcmp(cmd, "ignore_file") == 0)
		{
			struct stat status;
			const char *filename = explode_path(par);

			if (!filename)
				error_exit(TRUE, "Path '%s' is not understood\n", par);

			if (load_ignore_list(par) == TRUE)
			{
			}
			else if (load_ignore_list(filename) == TRUE)
			{
			}
			else if (stat(filename, &status) == -1) 	/* file doesn't exist, look elsewhere */
			{
				int len = strlen(SYSCONFDIR) + strlen(par) + 2;
				char *ignore_file = malloc(len * sizeof(char));

				/* look for it under SYSCONFDIR */
				snprintf(ignore_file, len, "%s/%s", SYSCONFDIR, par);

				/* look for it under ~/.firc location */
				if (stat(ignore_file, &status) == -1)
					snprintf(ignore_file, len, "%s/%s", dirname(conf_file), par);

				load_ignore_list(ignore_file);

				myfree(ignore_file);
			} 

			myfree(filename);
		}
		else if (strcmp(cmd, "send_after_login") == 0)
		{
			server *ps = &server_list[server_index];

			if (server_index == -1)
				error_exit(FALSE, "send_after_login: you need to define a server first\n");

			add_to_string_array(&ps -> send_after_login, par);
		}
		else if (strcmp(cmd, "auto_join") == 0 || strcmp(cmd, "channel") == 0)
		{
			if (server_index == -1)
				error_exit(FALSE, "auto_join: you need to define a server first\n");

			add_autojoin(server_index, par);
		}
		else if (strcmp(cmd, "rejoin") == 0)
		{
			add_channel(server_index, par);

			if (keep_channels_sorted)
				sort_channels(server_index);
		}
		else if (strcmp(cmd, "script") == 0)
		{
			int ch = -1;
			channel *pc = NULL;
			string_array_t parts;

			init_string_array(&parts);

			split_string(par, " ", TRUE, &parts);

			if (string_array_get_n(&parts) != 2)
				error_exit(FALSE, "script: requires a channel-name and a script filename (script filename cannot have spaces in it)\n");

			ch = add_channel(server_index, string_array_get(&parts, 0));

			pc = &server_list[server_index].pchannels[ch];

			add_channel_script(pc, string_array_get(&parts, 1));
		}
		else if (strcmp(cmd, "auto_private_channel") == 0)
			auto_private_channel = parse_false_true(par, cmd, linenr);
		else if (strcmp(cmd, "dcc_path") == 0)
			dcc_path = strdup(par);
		else if (strcmp(cmd, "default_colorpair") == 0)
			default_colorpair = parse_color_spec(par, linenr, cmd);
		else if (strcmp(cmd, "markerline_colorpair") == 0)
			markerline_colorpair = parse_color_spec(par, linenr, cmd);
		else if (strcmp(cmd, "highlight_colorpair") == 0)
			highlight_colorpair = parse_color_spec(par, linenr, cmd);
		else if (strcmp(cmd, "meta_colorpair") == 0)
			meta_colorpair = parse_color_spec(par, linenr, cmd);
		else if (strcmp(cmd, "error_colorpair") == 0)
			error_colorpair = parse_color_spec(par, linenr, cmd);
		else if (strcmp(cmd, "temp_colorpair") == 0)
			temp_colorpair = parse_color_spec(par, linenr, cmd);
		else if (strcmp(cmd, "check_for_mail") == 0)
			check_for_mail = atoi(par);
		else if (strcmp(cmd, "user_column_width") == 0)
			user_column_width = atoi(par);
		else if (strcmp(cmd, "delay_before_reconnect") == 0)
			delay_before_reconnect = atoi(par);
		else if (strcmp(cmd, "word_cloud_n") == 0)
			word_cloud_n = atoi(par);
		else if (strcmp(cmd, "word_cloud_refresh") == 0)
		{
			word_cloud_refresh = atoi(par);
			word_cloud_last_refresh = time(NULL);
		}
		else if (strcmp(cmd, "word_cloud_win_height") == 0)
			word_cloud_win_height = atoi(par);
		else if (strcmp(cmd, "max_channel_record_lines") == 0)
			max_channel_record_lines = atoi(par);
		else if (strcmp(cmd, "word_cloud_min_word_size") == 0)
			word_cloud_min_word_size = atoi(par);
		else
		{
			error_exit(FALSE, "'%s=%s' is not understood\n", cmd, par);
		}

		myfree(line);
	}

	close(fd);

	if (server_host)
	{
		if (nickname == NULL)
			error_exit(FALSE, "nickname must be set for %s", server_host);
		add_server(server_host, username, password, nickname, user_complete_name, description);
		myfree(server_host);
		myfree(username);
		myfree(password);
		myfree(nickname);
		myfree(user_complete_name);
		myfree(description);
	}

	return 0;
}

int config_color_str_convert(const char *in, int linenr, const char *subj)
{
	int nr = color_str_convert(in);

	if (nr >= -1)
		return nr;

	error_exit(FALSE, "Color %s for %s at line %d not recognized", in, subj, linenr);

	return -2;
}

int parse_color_spec(const char *par, int linenr, const char *subj)
{
	char *temp = strdup(par);
	char *komma = strchr(temp, ',');
	const char *fgstr = temp, *bgstr = "default";
	int fg = -1, bg = -1;
	int col = -1;

	if (komma)
	{
		*komma = 0x00;

		bgstr = komma + 1;
	}

	fg = config_color_str_convert(fgstr, linenr, subj);
	bg = config_color_str_convert(bgstr, linenr, subj);

	free(temp);

	if (fg == bg && fg != -1)
		fg = bg = -1;

	col = get_color_ncurses(fg, bg);

	return col;
}
