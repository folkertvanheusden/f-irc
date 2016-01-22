/* GPLv2 applies
 * SVN revision: $Revision: 789 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <regex.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#ifndef AIX
#include <sys/termios.h> /* needed on Solaris 8 */
#endif
#include <sys/types.h>
#include <sys/stat.h>

#include "gen.h"
#include "error.h"
#include "theme.h"
#include "term.h"
#include "utils.h"
#include "config.h"
#include "main.h"
#include "colors.h"

int max_x = 80, max_y = 24;
int default_colorpair = 0, highlight_colorpair = 0, meta_colorpair = 0, error_colorpair = 0, temp_colorpair = 0, markerline_colorpair = 0;
NEWWIN *override_cursor_win = NULL;
int override_cursor_x = 0, override_cursor_y = 0;

void wrong_key(void)
{
	flash();
	beep();
	flushinp();
}

void mywbkgd(NEWWIN *win, int pair)
{
	if (pair < 0)
		error_exit(FALSE, "color pair %d < 0 by color_on", pair);

	if (pair >= get_n_cpairs())
		error_exit(FALSE, "color pair %d >= %d by color_on", pair, get_n_cpairs());

	if (colors_all == TRUE)
		wbkgd(win -> win, COLOR_PAIR(pair));
}

void color_on(NEWWIN *win, int pair)
{
	if (pair < 0)
		error_exit(FALSE, "color pair %d < 0 by color_on", pair);

	if (pair >= get_n_cpairs())
		error_exit(FALSE, "color pair %d>= %d by color_on", pair, get_n_cpairs());

	if (colors_all == TRUE)
		wattron(win -> win, COLOR_PAIR(pair));
}

void color_off(NEWWIN *win, int pair)
{
	if (pair < 0)
		error_exit(FALSE, "color pair %d < 0 by color_off", pair);

	if (pair >= get_n_cpairs())
		error_exit(FALSE, "color pair %d >= %d by color_off", pair, get_n_cpairs());

	if (colors_all == TRUE)
		wattroff(win -> win, COLOR_PAIR(pair));
}

void delete_window(NEWWIN *mywin)
{
	mydelwin(mywin);

	myfree(mywin);
}

void mydelwin(NEWWIN *win)
{
	if (win)
	{
		if (win -> pwin && ERR == del_panel(win -> pwin))
			error_exit(TRUE, "del_panel() failed\n");

		if (win -> win && ERR == delwin(win -> win))
			error_exit(TRUE, "delwin() failed\n");
	}
}

extern NEWWIN *input_window;

void mydoupdate()
{
	update_panels();

	if (override_cursor_win)
	{
		wmove(override_cursor_win -> win, override_cursor_y, override_cursor_x);
		setsyx(override_cursor_win -> y + override_cursor_y, override_cursor_win -> x + override_cursor_x);
	}
	else if (input_window)
	{
		wmove(input_window -> win, 0, ul_x);
		setsyx(input_window -> y + 0, input_window -> x + ul_x);
	}

	doupdate();
}

WINDOW * mynewwin(int nlines, int ncols, int begin_y, int begin_x)
{
        WINDOW *dummy = newwin(nlines, ncols, begin_y, begin_x);
        if (!dummy)
                error_exit(TRUE, "failed to create window (subwin) with dimensions %d-%d at offset %d,%d (terminal size: %d,%d)\n", ncols, nlines, begin_x, begin_y, max_x, max_y);

	keypad(dummy, TRUE);

        return dummy;
}

NEWWIN * create_window(int n_lines, int n_colls)
{
        return create_window_xy((max_y/2) - (n_lines/2), (max_x/2) - (n_colls/2), n_lines, n_colls);
}

NEWWIN * create_window_xy(int y_offset, int x_offset, int n_lines, int n_colls)
{
        NEWWIN *newwin = malloc(sizeof(NEWWIN));

        /* create new window */
        newwin -> win = mynewwin(n_lines, n_colls, y_offset, x_offset);
	newwin -> pwin = new_panel(newwin -> win);
        werase(newwin -> win);

	newwin -> ncols = n_colls;
	newwin -> nlines = n_lines;

	newwin -> x = x_offset;
	newwin -> y = y_offset;

        return newwin;
}

void limit_print(NEWWIN *win, int width, int y, int x, const char *format, ...)
{
        va_list ap;
	int len = 0;
	char *buf = NULL;

        va_start(ap, format);
	len = vasprintf(&buf, format, ap);
        va_end(ap);

	if (len > width)
		buf[width] = 0x00;

	mvwprintw(win -> win, y, x, "%s", buf);

	free(buf);
}

void escape_print_xy(NEWWIN *win, int y, int x, const char *str)
{
	int loop = 0, cursor_x = 0, len = strlen(str);
	BOOL inv = FALSE, underline = FALSE;

	for(loop=0; loop<len; loop++)
	{
		if (str[loop] == '^')
		{
			if (!inv)
				mywattron(win -> win, A_REVERSE);
			else
				mywattroff(win -> win, A_REVERSE);

			inv = 1 - inv;
		}
		else if (str[loop] == '_')
		{
			if (!underline)
				mywattron(win -> win, A_UNDERLINE);
			else
				mywattroff(win -> win, A_UNDERLINE);

			underline = 1 - underline;
		}
		else if (str[loop] == '\n')
		{
			cursor_x = 0;
			y++;
		}
		else
		{
			mvwprintw(win -> win, y, x + cursor_x++, "%c", str[loop]);
		}
	}

	if (inv)
		mywattroff(win -> win, A_REVERSE);

	if (underline)
		mywattroff(win -> win, A_UNDERLINE);
}

void escape_print(NEWWIN *win, const char *str, const char rev, const char un)
{
	int loop, len = strlen(str);
	BOOL inv = FALSE, underline = FALSE;

	for(loop=0; loop<len; loop++)
	{
		if (str[loop] == rev)
		{
			if (!inv)
				mywattron(win -> win, A_REVERSE);
			else
				mywattroff(win -> win, A_REVERSE);

			inv = 1 - inv;
		}
		else if (str[loop] == un)
		{
			if (!underline)
				mywattron(win -> win, A_UNDERLINE);
			else
				mywattroff(win -> win, A_UNDERLINE);

			underline = 1 - underline;
		}
		else
		{
			waddch(win -> win, str[loop]);
		}
	}

	if (inv)
		mywattroff(win -> win, A_REVERSE);

	if (underline)
		mywattroff(win -> win, A_UNDERLINE);
}

void determine_terminal_size(void)
{
	struct winsize size;

	max_x = max_y = 0;

	/* changed from 'STDIN_FILENO' as that is incorrect: we're
	* outputting to stdout!
	*/
	if (ioctl(1, TIOCGWINSZ, &size) == 0)
	{
		max_y = size.ws_row;
		max_x = size.ws_col;
	}

	if (!max_x || !max_y)
	{
		char *dummy = getenv("COLUMNS");
		if (dummy)
			max_x = atoi(dummy);
		else
			max_x = 80;

		dummy = getenv("LINES");
		if (dummy)
			max_x = atoi(dummy);
		else
			max_x = 24;
	}
}

void create_win_border(int width, int height, const char *title, NEWWIN **bwin, NEWWIN **win, BOOL f1)
{
	const char f1_for_help [] = " F1 for help ";
	int x = max_x / 2 - (width + 2) / 2;
	int y = max_y / 2 - (height + 2) / 2;

        *bwin = create_window_xy(y + 0, x + 0, height + 2, width + 2);
        *win  = create_window_xy(y + 1, x + 1, height + 0, width + 0);

        mywattron((*bwin) -> win, A_REVERSE);
        box((*bwin) -> win, 0, 0);
        mywattroff((*bwin) -> win, A_REVERSE);

	mywattron((*bwin) -> win, A_STANDOUT);

	if (inverse_window_heading)
	{
		mvwprintw((*bwin) -> win, 0, 1, "%s", title);

		if (f1)
			mvwprintw((*bwin) -> win, (*bwin) -> nlines - 1, 2, "%s", f1_for_help);
	}
	else
	{
		mvwprintw((*bwin) -> win, 0, 1, "[ %s ]", title);

		if (f1)
			mvwprintw((*bwin) -> win, (*bwin) -> nlines - 1, 2, "[ %s ]", f1_for_help);
	}

	mywattroff((*bwin) -> win, A_STANDOUT);
}

void init_colors(void)
{
	default_colorpair = get_color_ncurses(-1, -1);
	highlight_colorpair = get_color_ncurses(COLOR_GREEN, -1);
	meta_colorpair = get_color_ncurses(COLOR_BLUE, -1);
	error_colorpair = get_color_ncurses(COLOR_YELLOW, -1);
	temp_colorpair = get_color_ncurses(COLOR_CYAN, -1);
	markerline_colorpair = default_colorpair;
}

void apply_mouse_setting(void)
{
	if (ignore_mouse)
		mousemask(0, NULL);
	else
		mousemask(BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED | BUTTON3_CLICKED, NULL);
}

void init_ncurses(BOOL ignore_mouse)
{
        initscr();
	start_color(); /* don't care if this one failes */
	use_default_colors();
        keypad(stdscr, TRUE);
        cbreak();
        intrflush(stdscr, FALSE);
        noecho();
        nonl();
        refresh();
        nodelay(stdscr, FALSE);
        meta(stdscr, TRUE);     /* enable 8-bit input */
        raw();  /* to be able to catch ctrl+c */
	mywattron(stdscr, COLOR_PAIR(default_colorpair));

	apply_mouse_setting();

	init_colors();

        max_y = LINES;
        max_x = COLS;
}

void mywattron(WINDOW *w, int a)
{
	if (a != A_BLINK && a != A_BOLD && a != A_NORMAL && a != A_REVERSE && a != A_STANDOUT && a != A_UNDERLINE)
		error_exit(FALSE, "funny attributes: %d\n", a);

	wattron(w, a);
}

void mywattroff(WINDOW *w, int a)
{
	if (a != A_BLINK && a != A_BOLD && a != A_NORMAL && a != A_REVERSE && a != A_STANDOUT && a != A_UNDERLINE)
		error_exit(FALSE, "funny attributes: %d\n", a);

	wattroff(w, a);
}

void reset_attributes(NEWWIN *win)
{
	wattrset(win -> win, A_NORMAL | COLOR_PAIR(default_colorpair));
}

BOOL is_in_window(NEWWIN *win, int x, int y)
{
	return wenclose(win -> win, y, x);
}

BOOL right_mouse_button_clicked(void)
{
	MEVENT event;

	return getmouse(&event) == OK && (event.bstate & BUTTON3_CLICKED);
}

void display_markerline(NEWWIN *win, const char *msg)
{
	char *line = (char *)calloc(1, win -> ncols + 1), *msg_use = NULL;
	int len = 0, len_msg = 0;

	if (getcurx(win -> win))
		waddch(win -> win, '\n');

	color_on(win, markerline_colorpair);
        mywattron(win -> win, A_REVERSE);

	memset(line, '-', win -> ncols);

	len_msg = asprintf(&msg_use, "%s", msg);
	msg_use[min(len_msg, win -> ncols)] = 0x00;

	len = strlen(msg_use);
	memcpy(&line[win -> ncols / 2 - len / 2], msg_use, len);

	waddstr(win -> win, line);

        mywattroff(win -> win, A_REVERSE);

	free(line);
	free(msg_use);

	color_off(win, markerline_colorpair);
	color_on(win, default_colorpair);
}

void simple_marker(NEWWIN *win)
{
	char *end_marker = (char *)calloc(1, win -> ncols + 1);

	memset(end_marker, '-', win -> ncols);

	if (getcurx(win -> win))
		waddstr(win -> win, "\n");

	waddstr(win -> win, end_marker);

	free(end_marker);
}
