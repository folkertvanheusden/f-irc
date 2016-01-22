/* GPLv2 applies
 * SVN revision: $Revision: 882 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ncursesw/panel.h>
#include <ncursesw/ncurses.h>

#include "gen.h"
#include "term.h"
#include "channels.h"
#include "loop.h"
#include "colors.h"
#include "main.h"
#include "user.h"

time_t prev_tooltip = 0;
const int tooltip_interval = 5;
int tooltip_nr = 0;

const char *tooltips[] = {
	"Press right cursor key to select channel",
	"Press F8 to edit configuration",
	"Press F8 to add/edit a server",
	NULL
};

const char *get_tooltip(void)
{
	time_t now = time(NULL);

	if (now - prev_tooltip >= tooltip_interval)
	{
		int nr = rand() % 100, index = 0;

		while(nr > 0)
		{
			++index;

			if (tooltips[index] == NULL)
				index = 0;

			nr--;
		}

		tooltip_nr = index;

		prev_tooltip = now;
	}

	return tooltips[tooltip_nr];
}

void main_help(void)
{
	const int help_win_width = 77, help_win_height = 17;
	NEWWIN *bwin = NULL, *win = NULL;
	int screen_nr = 0;

	create_win_border(help_win_width, help_win_height, "left cursor key: previous screen / exit help, right key: next screen", &bwin, &win, FALSE);

	for(;;)
	{
		int c = 0;

		werase(win -> win);

		if (screen_nr == 0)
		{
			escape_print(win, "\n", '|', '_');
			escape_print(win, " Press |right cursor key| to:\n", '|', '_');
			escape_print(win, "  - enter a screen\n", '|', '_');
			escape_print(win, "  - unfold a server (=show channels)\n", '|', '_');
			escape_print(win, "  - show a server commands menu\n", '|', '_');
			escape_print(win, "  - when in a channel: show list of people (nicks)\n", '|', '_');
			escape_print(win, "  - when in a list of people: show a user menu\n", '|', '_');
			escape_print(win, "  - in the word cloud: do a search for the selected word\n", '|', '_');
			escape_print(win, " \n", '|', '_');
			escape_print(win, " Press |left cursor key| to:\n", '|', '_');
			escape_print(win, "  - leave a screen\n", '|', '_');
			escape_print(win, "  - fold a server (=hide channels)\n", '|', '_');
			escape_print(win, "  - go back from the people list to the channel list\n", '|', '_');
			escape_print(win, " \n", '|', '_');
			escape_print(win, " Press right cursor key to go to the next help-screen\n", '|', '_');
			escape_print(win, " which lists the shortcut keys.\n", '|', '_');
		}
		else if (screen_nr == 1)
		{
			escape_print(win, "\n", '|', '_');
			escape_print(win, " |^A| moves the cursor to the left |^E| move right |^D| deletes current character\n", '|', '_');
			escape_print(win, " |^U| clear line, enter twice to undo clear\n", '|', '_');
			escape_print(win, " |^W| jump to the next channel with new text - ^R jump backwards\n", '|', '_');
			escape_print(win, " |^Z| jump to the next channel with your nick name - ^X search backwards\n", '|', '_');
			escape_print(win, " |^Q| jump to next favorite channel (see configuration file)\n", '|', '_');
			escape_print(win, " |^O| remove all the *'s (forget what channels have unread text)\n", '|', '_');
			escape_print(win, " |^T| go back to the previous selected server/channel\n", '|', '_');
			escape_print(win, " |^F| scroll-back in what was written by others\n", '|', '_');
			escape_print(win, " |^B| scroll-back in what you wrote, enter to select, |^S| search all\n", '|', '_');
			escape_print(win, " |^V| Enter 1 character: used when entering ascii values < 32\n", '|', '_');
			escape_print(win, " |TAB| auto-completion of commands and nick names\n", '|', '_');
			escape_print(win, " |^G| close a channel (F9: undo close)\n", '|', '_');
			escape_print(win, " |^C| terminate the program\n", '|', '_');
			if (vc_list_data_only)
				escape_print(win, "*|^Y| toggle \"only show channels with new messages\"", '|', '_');
			else
				escape_print(win, " |^Y| toggle \"only show channels with new messages\"", '|', '_');
			escape_print(win, " |^P| add markerline\n", '|', '_');
		}
		else if (screen_nr == 2)
		{
			escape_print(win, "\n", '|', '_');
			escape_print(win, " |F1| this help\n", '|', '_');
			escape_print(win, " |F2| store current configuration on disk\n", '|', '_');
			/* escape_print(win, " |F3| add server\n", '|', '_'); */
			escape_print(win, " |F4| switch to edit-line (shortcut for ^N)\n", '|', '_');
			escape_print(win, " |F5|/|^L| redraw screen\n", '|', '_');
			escape_print(win, " |F6| search in all windows for text\n", '|', '_');
			escape_print(win, " |F7| close all channels with only \"NOTICE\" messages\n", '|', '_');
			if (n_servers <= 1)
				escape_print(win, " |F8| _edit configuration_\n", '|', '_');
			else
				escape_print(win, " |F8| edit configuration\n", '|', '_');
			escape_print(win, " |F9| undo last channel close\n", '|', '_');
			escape_print(win, " |F10|/|^N| toggle between channel-list, edit-line and word-cloud\n", '|', '_');
			escape_print(win, " |@/|... sends /... to other end\n", '|', '_');
			escape_print(win, " |@@|... sends @... to other end\n", '|', '_');
			escape_print(win, " |@|... goes to the first channel with \"...\" in its name |^J| for the next\n", '|', '_');
		}
		else if (screen_nr == 3)
		{
			const char pairs[] = " Defined pairs: ";
			int x = sizeof pairs, index = 0, n_cpairs = get_n_cpairs();
			char *nc_vars = NULL;
			wchar_t block = 0;
			char *pblock = "\xe2\x96\x85";
			char *res = NULL;

			mbsrtowcs(&block, (const char **)&pblock, 1, NULL);

			escape_print(win, "\n If you're on IRCNet, OFTC or FreeNet, say hi to |flok|/|flok99|/|flok42|!\n\n", '|', '_');

			waddstr(win -> win, " Compiled on " __DATE__ " " __TIME__ "\n\n");

			asprintf(&res, " Columns/rows: %dx%d\n\n", max_x, max_y);
			waddstr(win -> win, res);
			free(res);

			asprintf(&nc_vars, " Colors: %d, pairs: %d, pairs defined: %d, can change colors: %s,\n nick color pairs: %d\n", COLORS, COLOR_PAIRS, n_cpairs, can_change_color() ? "yes" : "no", n_nick_pairs);
			waddstr(win -> win, nc_vars);
			waddstr(win -> win, "\n");
			waddstr(win -> win, pairs);

			for(index=0; index<n_cpairs; index++)
			{
				color_on(win, index);

				waddnwstr(win -> win, &block, 1);

				color_off(win, index);

				if (++x == help_win_width - 2)
				{
					waddstr(win -> win, "\n ");

					x = 1;
				}
			}

			free(nc_vars);
		}

		mydoupdate();

		c = wait_for_keypress(FALSE);

		if (c == 'q' || c == 'Q' || c == -1)
			break;
		else if (c == 3)
			exit_fi();
		else if (c == KEY_LEFT || (c == KEY_MOUSE && right_mouse_button_clicked()))
		{
			if (screen_nr == 0)
				break;

			screen_nr--;
		}
		else if (c == KEY_RIGHT)
		{
			if (screen_nr < 3)
				screen_nr++;
			else
				wrong_key();
		}
	}

	delete_window(win);
	delete_window(bwin);

	mydoupdate();
}

void configure_firc_help(void)
{
	const int help_win_width = 77, help_win_height = 17;
	NEWWIN *bwin = NULL, *win = NULL;

	create_win_border(help_win_width, help_win_height, "Configure f-irc help | Press any key to exit this help", &bwin, &win, FALSE);

	werase(win -> win);
	escape_print(win, "\n", '|', '_');
	escape_print(win, " Main configuration screen:\n", '|', '_');
	escape_print(win, "  Navigate through items list using the cursor keys.\n", '|', '_');
	escape_print(win, "  Press |/| to search for a configuration item.\n", '|', '_');
	escape_print(win, "  Press |enter| to change an item.\n", '|', '_');
	escape_print(win, "\n", '|', '_');
	escape_print(win, " Changing an item:\n", '|', '_');
	escape_print(win, "  ON/OFF can be toggled using the |space bar|.\n", '|', '_');
	escape_print(win, "  Select between OK and CANCEL using the |TAB| key.\n", '|', '_');
	escape_print(win, "  Press |enter| to select either OK or CANCEL.\n", '|', '_');
	escape_print(win, "\n", '|', '_');
	escape_print(win, " Editing text/number:\n", '|', '_');
	escape_print(win, "  Navigate with the cursor keys or |^A| and |^E|.\n", '|', '_');
	escape_print(win, "  |^U| to clear the line.\n", '|', '_');
	escape_print(win, "  Select between OK and CANCEL using the |TAB| key.\n", '|', '_');
	escape_print(win, "  Press |enter| to select either OK or CANCEL.\n", '|', '_');

	mydoupdate();

	(void)wait_for_keypress(FALSE);

	delete_window(win);
	delete_window(bwin);

	mydoupdate();
}

void edit_box_help(void)
{
	const int help_win_width = 77, help_win_height = 17;
	NEWWIN *bwin = NULL, *win = NULL;

	create_win_border(help_win_width, help_win_height, "Edit text help | Press any key to exit this help", &bwin, &win, FALSE);

	werase(win -> win);
	escape_print(win, "\n", '|', '_');
	escape_print(win, " |^A| move cursor to start of line\n", '|', '_');
	escape_print(win, " |^E| move cursor to end of line\n", '|', '_');
	escape_print(win, " |^D| delete the character under the cursor\n", '|', '_');
	escape_print(win, " |^U| erase line\n", '|', '_');
	escape_print(win, " |^W| delete the word left from the cursor\n", '|', '_');

	mydoupdate();

	(void)wait_for_keypress(FALSE);

	delete_window(win);
	delete_window(bwin);

	mydoupdate();
}

void commandline_help(void)
{
	popup_notify(TRUE, "The only command line parameter accepted is the file name of the\nconfiguration file. Leave empty to create a default one. Servers\ncan be configured from within f-irc (press F8).");
}

void scrollback_help(void)
{
	const int help_win_width = 77, help_win_height = 17;
	NEWWIN *bwin = NULL, *win = NULL;

	create_win_border(help_win_width, help_win_height, "Scrollback help | Press any key to exit this help", &bwin, &win, FALSE);

	werase(win -> win);
	escape_print(win, "\n", '|', '_');
	escape_print(win, " |w| write to file\n", '|', '_');
	escape_print(win, " |c| copy to X clipboard (requires xclip)\n", '|', '_');
	escape_print(win, " |/| search\n", '|', '_');
	escape_print(win, " |m| jump to marker line\n", '|', '_');
	escape_print(win, " |^B| select a line & copy to edit line\n", '|', '_');

	mydoupdate();

	(void)wait_for_keypress(FALSE);

	delete_window(win);
	delete_window(bwin);

	mydoupdate();
}

void scrollback_and_select_help(void)
{
	const int help_win_width = 77, help_win_height = 17;
	NEWWIN *bwin = NULL, *win = NULL;

	create_win_border(help_win_width, help_win_height, "Scrollback + select help | Press any key to exit this help", &bwin, &win, FALSE);

	werase(win -> win);
	escape_print(win, "\n", '|', '_');
	escape_print(win, " |right cursor key| selection line\n", '|', '_');
	escape_print(win, " |left  cursor key| abort selection\n", '|', '_');
	escape_print(win, " |w| write to file\n", '|', '_');
	escape_print(win, " |/| search\n", '|', '_');
	escape_print(win, " |n| search next\n", '|', '_');

	mydoupdate();

	(void)wait_for_keypress(FALSE);

	delete_window(win);
	delete_window(bwin);

	mydoupdate();
}

void user_channel_menu_help(void)
{
	const int help_win_width = 77, help_win_height = 17;
	NEWWIN *bwin = NULL, *win = NULL;

	create_win_border(help_win_width, help_win_height, "User menu | Press any key to exit this help", &bwin, &win, FALSE);

	werase(win -> win);
	escape_print(win, "\n", '|', '_');
	escape_print(win, " Output of commands like \"version\" etc can be seen\n", '|', '_');
	escape_print(win, " in the server channel.\n", '|', '_');

	mydoupdate();

	(void)wait_for_keypress(FALSE);

	delete_window(win);
	delete_window(bwin);

	mydoupdate();
}

void edit_scripts_help(void)
{
	// FIXME
}
