/* GPLv2 applies
 * SVN revision: $Revision: 671 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#define COLOR_MODE_NONE		0
#define COLOR_MODE_RGB		1
#define COLOR_MODE_INDEX	2

typedef struct
{
	char mode;
	unsigned char r, g, b;
	int index;
} color;

typedef struct
{
	/* channel list */
	color	color_channellist_default;
	color	color_channellist_minimized_server;
	color	color_channellist_newlines;
	char	channellist_newlines_markchar;
	int	channellist_window_width;
	BOOL	channellist_border;
	color	channellist_border_color;
	char    channellist_border_left_side;
	char    channellist_border_right_side;
	char    channellist_border_top_side;
	char    channellist_border_bottom_side;
	char    channellist_border_top_left_hand_corner;
	char    channellist_border_top_right_hand_corner;
	char    channellist_border_bottom_left_hand_corner;
	char    channellist_border_bottom_right_hand_corner;
	BOOL	start_in_channellist_window;

	/* chat window */
	BOOL	show_time;
	BOOL	show_date_when_changed;
	BOOL	chat_window_border;
	color	chat_window_border_color;
	char    chat_window_border_left_side;
	char    chat_window_border_right_side;
	char    chat_window_border_top_side;
	char    chat_window_border_bottom_side;
	char    chat_window_border_top_left_hand_corner;
	char    chat_window_border_top_right_hand_corner;
	char    chat_window_border_bottom_left_hand_corner;
	char    chat_window_border_bottom_right_hand_corner;

	BOOL	show_clock;
} layout_theme;

#define THEME_N_COLOR_STRUCTS	7

#define THEME_STRUCT_OFFSET(x) ( (long int)&((layout_theme *)NULL) -> x )

#define THEME_VAR_TYPE_BOOL	1
#define THEME_VAR_TYPE_COLOR	2
#define THEME_VAR_TYPE_INT	3
#define THEME_VAR_TYPE_CHAR	4

typedef struct
{
	char *keyword;
	char *description;
	int type;		/* THEME_VAR_TYPE_... */
	long int offset;
} theme_parsing;

extern layout_theme theme;
extern const char *theme_file;

void load_theme(const char *file);
BOOL parse_false_true(const char *value_in, const char *subject, int line);
