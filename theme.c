/* GPLv2 applies
 * SVN revision: $Revision: 671 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ncursesw/ncurses.h>
#include <ctype.h>

#include "gen.h"
#include "error.h"
#include "theme.h"
#include "utils.h"

layout_theme theme;
const char *theme_file = NULL;

theme_parsing theme_parser[] =
{
	/* channel list */
	{
		"color_channellist_default",
		"color for channellist box",
		THEME_VAR_TYPE_COLOR,
		THEME_STRUCT_OFFSET(color_channellist_default)
	},

	{
		"color_channellist_minimized_server",
		"color for a minimized server",
		THEME_VAR_TYPE_COLOR,
		THEME_STRUCT_OFFSET(color_channellist_minimized_server)
	},

	{
		"color_channellist_newlines",
		"color for a channel with new messages",
		THEME_VAR_TYPE_COLOR,
		THEME_STRUCT_OFFSET(color_channellist_newlines)
	},

	{
		"channellist_newlines_markchar",
		"character to place before channels with new messages",
		THEME_VAR_TYPE_CHAR,
		THEME_STRUCT_OFFSET(channellist_newlines_markchar)
	},

	{
		"channellist_window_width",
		"width of the channellist window",
		THEME_VAR_TYPE_INT,
		THEME_STRUCT_OFFSET(channellist_window_width)
	},

	{
		"channellist_border",
		"wether to draw a border around the channellist or not",
		THEME_VAR_TYPE_BOOL,
		THEME_STRUCT_OFFSET(channellist_border)
	},

	{
		"channellist_border_color",
		"color of the channellist border",
		THEME_VAR_TYPE_COLOR,
		THEME_STRUCT_OFFSET(channellist_border_color)
	},

	{ "channellist_border_left_side", "channellist window layout", THEME_VAR_TYPE_CHAR, THEME_STRUCT_OFFSET(channellist_border_left_side) },
	{ "channellist_border_right_side", "channellist window layout", THEME_VAR_TYPE_CHAR, THEME_STRUCT_OFFSET(channellist_border_right_side) },
	{ "channellist_border_top_side", "channellist window layout", THEME_VAR_TYPE_CHAR, THEME_STRUCT_OFFSET(channellist_border_top_side) },
	{ "channellist_border_bottom_side", "channellist window layout", THEME_VAR_TYPE_CHAR, THEME_STRUCT_OFFSET(channellist_border_bottom_side) },
	{ "channellist_border_top_left_hand_corner", "channellist window layout", THEME_VAR_TYPE_CHAR, THEME_STRUCT_OFFSET(channellist_border_top_left_hand_corner) },
	{ "channellist_border_top_right_hand_corner", "channellist window layout", THEME_VAR_TYPE_CHAR, THEME_STRUCT_OFFSET(channellist_border_top_right_hand_corner) },
	{ "channellist_border_bottom_left_hand_corner", "channellist window layout", THEME_VAR_TYPE_CHAR, THEME_STRUCT_OFFSET(channellist_border_bottom_left_hand_corner) },
	{ "channellist_border_bottom_right_hand_corner", "channellist window layout", THEME_VAR_TYPE_CHAR, THEME_STRUCT_OFFSET(channellist_border_bottom_right_hand_corner) },

	{
		"start_in_channellist_window",
		"wether to start (with the cursor) in the channel window",
		THEME_VAR_TYPE_BOOL,
		THEME_STRUCT_OFFSET(start_in_channellist_window)
	},

	/* chat window */
	{
		"show_time",
		"show time when a message was received?",
		THEME_VAR_TYPE_BOOL,
		THEME_STRUCT_OFFSET(show_time)
	},

	{
		"show_date_when_changed",
		"show date when it has changed since the last message when a message was received?",
		THEME_VAR_TYPE_BOOL,
		THEME_STRUCT_OFFSET(show_date_when_changed)
	},

	{
		"chat_window_border",
		"wether to draw a border around the chatwindow or not",
		THEME_VAR_TYPE_BOOL,
		THEME_STRUCT_OFFSET(chat_window_border)
	},

	{
		"chat_window_border_color",
		"color of the chatwindow border",
		THEME_VAR_TYPE_COLOR,
		THEME_STRUCT_OFFSET(chat_window_border_color)
	},

	{ "chat_window_border_left_side", "chat window layout", THEME_VAR_TYPE_CHAR, THEME_STRUCT_OFFSET(chat_window_border_left_side) },
	{ "chat_window_border_right_side", "chat window layout", THEME_VAR_TYPE_CHAR, THEME_STRUCT_OFFSET(chat_window_border_right_side) },
	{ "chat_window_border_top_side", "chat window layout", THEME_VAR_TYPE_CHAR, THEME_STRUCT_OFFSET(chat_window_border_top_side) },
	{ "chat_window_border_bottom_side", "chat window layout", THEME_VAR_TYPE_CHAR, THEME_STRUCT_OFFSET(chat_window_border_bottom_side) },
	{ "chat_window_border_top_left_hand_corner", "chat window layout", THEME_VAR_TYPE_CHAR, THEME_STRUCT_OFFSET(chat_window_border_top_left_hand_corner) },
	{ "chat_window_border_top_right_hand_corner", "chat window layout", THEME_VAR_TYPE_CHAR, THEME_STRUCT_OFFSET(chat_window_border_top_right_hand_corner) },
	{ "chat_window_border_bottom_left_hand_corner", "chat window layout", THEME_VAR_TYPE_CHAR, THEME_STRUCT_OFFSET(chat_window_border_bottom_left_hand_corner) },
	{ "chat_window_border_bottom_right_hand_corner", "chat window layout", THEME_VAR_TYPE_CHAR, THEME_STRUCT_OFFSET(chat_window_border_bottom_right_hand_corner) },

	/* show clock? */
	{
		"show_clock",
		"wether to show a clock in the topicline",
		THEME_VAR_TYPE_BOOL,
		THEME_STRUCT_OFFSET(show_clock)
	},

	{
		NULL,
		NULL,
		0,
		-1
	}
};

void load_theme(const char *file)
{
	int self_defined = 0;
	int loop;
	color *clist[THEME_N_COLOR_STRUCTS];
	int n_clist = 0;
	int linenr = 0;
	int fd = open(file, O_RDONLY);
	if (fd == -1)
		error_exit(TRUE, "Cannot open themefile %s", file);

	theme_file = strdup(file);

	for(;;)
	{
		int theme_index = 0;
		char *line = read_line_fd(fd);
		char *cmd = NULL, *par = NULL;
		char *is = NULL;

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
			error_exit(FALSE, "themeconfig: line %d is missing either command or parameter! (%s)", linenr, line);

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

		while(theme_parser[theme_index].keyword)
		{
			if (strcasecmp(theme_parser[theme_index].keyword, cmd) == 0)
				break;

			theme_index++;
		}

		if (theme_parser[theme_index].keyword)
		{
			void *dummy_theme_pnt = (void *)&(((char *)&theme)[theme_parser[theme_index].offset]);
			BOOL *boolpnt;
			color *colorpnt;
			int *intpnt;
			char *charpnt;
			char *komma;

			switch(theme_parser[theme_index].type)
			{
			case THEME_VAR_TYPE_BOOL:
				boolpnt = (BOOL *)dummy_theme_pnt;

				if (parse_false_true(par, cmd, linenr))
					*boolpnt = TRUE;
				else
					*boolpnt = FALSE;
				break;

			case THEME_VAR_TYPE_COLOR:
				colorpnt = (color *)dummy_theme_pnt;
				memset(colorpnt, 0x00, sizeof(color));

				komma = strchr(par, ',');
				if (!komma && isdigit(par[0]))
				{
					colorpnt -> mode = COLOR_MODE_INDEX;
					colorpnt -> index = atoi(par);
				}
				else if (!isdigit(par[0]))
				{
					colorpnt -> mode = COLOR_MODE_INDEX;

					if (strcasecmp(par, "BLACK") == 0)
						colorpnt -> index = COLOR_BLACK;
					else if (strcasecmp(par, "RED") == 0)
						colorpnt -> index = COLOR_RED;
					else if (strcasecmp(par, "GREEN") == 0)
						colorpnt -> index = COLOR_GREEN;
					else if (strcasecmp(par, "YELLOW") == 0)
						colorpnt -> index = COLOR_YELLOW;
					else if (strcasecmp(par, "BLUE") == 0)
						colorpnt -> index = COLOR_BLUE;
					else if (strcasecmp(par, "MAGENTA") == 0)
						colorpnt -> index = COLOR_MAGENTA;
					else if (strcasecmp(par, "CYAN") == 0)
						colorpnt -> index = COLOR_CYAN;
					else if (strcasecmp(par, "WHITE") == 0)
						colorpnt -> index = COLOR_WHITE;
					else
						error_exit(FALSE, "line %d: %s is an unknown color\n", linenr, par);
				}
				else
				{
					colorpnt -> mode = COLOR_MODE_RGB;
					colorpnt -> r = atoi(par);
					colorpnt -> g = atoi(komma + 1);
					komma = strchr(komma + 1, ',');
					if (!komma)
						error_exit(FALSE, "line %d: '%s' is an invalid color specification", linenr, theme_parser[theme_index].keyword);
					colorpnt -> b = atoi(komma + 1);

					clist[n_clist++] = colorpnt;
				}
				break;

			case THEME_VAR_TYPE_INT	:
				intpnt = (int *)dummy_theme_pnt;
				*intpnt = atoi(par);
				break;

			case THEME_VAR_TYPE_CHAR:
				charpnt = (char *)dummy_theme_pnt;
				if (*par == 0x00)
					*charpnt = ' ';
				else
					*charpnt = *par;
				break;
			}
		}
		else
		{
			error_exit(FALSE, "line %d: '%s=%s' is not understood\n", linenr, cmd, par);
		}

		myfree(line);
	}

	close(fd);

	/* convert rgb values to indexes (if any) */
	for(loop=0; loop<n_clist; loop++)
	{
		int cloop, cindex = -1;
		int mindiff = (256 * 2) * 3;

		clist[loop] -> mode = COLOR_MODE_INDEX;

		/* fist see if this color is one of the 8 predefined colors or the
		 * ones just created
		 */
		for(cloop=0; cloop<(8 + self_defined); cloop++)
		{
			short r, g, b;
			int diff;

			color_content(cloop, &r, &g, &b);

			/* let's not use black */
			if (r == 0 && g == 0 && b == 0)
				continue;

			diff = abs(clist[loop] -> r - r) +
				abs(clist[loop] -> g - g) +
				abs(clist[loop] -> b - b);

			if (diff < mindiff)
			{
				cindex = cloop;
				mindiff = diff;
				if (diff == 0)
					break;
			}
		}

		if (cindex == -1)
			error_exit(FALSE, "internal error: no colors found at all");

		/* found? or no more available? or cannot define any colors at all? */
		if (mindiff == 0 || (8 + self_defined) >= COLORS || can_change_color() == FALSE)
		{
			clist[loop] -> index = cindex;
		}
		else
		{
			init_color(8 + self_defined, clist[loop] -> r, clist[loop] -> g, clist[loop] -> b);
			clist[loop] -> index = 8 + self_defined;
			self_defined++;
		}
	}
}

BOOL parse_false_true(const char *value_in, const char *subject, int line)
{
	if (strcasecmp(value_in, "YES") == 0 ||
	    strcasecmp(value_in, "JA") == 0 ||
	    strcasecmp(value_in, "TRUE") == 0 ||
	    strcasecmp(value_in, "ON") == 0 ||
	    strcasecmp(value_in, "1") == 0)
	{
		return TRUE;
	}
	else if (strcasecmp(value_in, "NO") == 0 ||
	    strcasecmp(value_in, "NEE") == 0 ||
	    strcasecmp(value_in, "FALSE") == 0 ||
	    strcasecmp(value_in, "OFF") == 0 ||
	    strcasecmp(value_in, "0") == 0)
	{
		return FALSE;
	}

	error_exit(FALSE, "%s requires either 'true' or 'false', not '%s' (line %d)", subject, value_in, line);

	return FALSE;
}
