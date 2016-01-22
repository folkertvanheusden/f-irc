/* GPLv2 applies
 * SVN revision: $Revision: 671 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#include <stdlib.h>
#include <string.h>
#include <ncursesw/ncurses.h>
#include <ncursesw/panel.h>

#include "gen.h"
#include "nickcolor.h"
#include "term.h"

int *nick_pairs = NULL, n_nick_pairs = 0;
hash_types hash_function = DJB2;

int hash_nick(const char *nick)
{
	int len = strlen(nick), loop = 0;
	int hash = 0;

	if (hash_function == DJB2)
	{
		/* DJB2 (http://stackoverflow.com/questions/14409466/simple-hash-functions): */
		for(loop=0; loop<len; loop++)
			hash = (hash << 5) + hash + (unsigned char)nick[loop];
	}
	else /* if (hash_function == LRC) */
	{
		/* LRC: */
		for(loop=0; loop<len; loop++)
			hash ^= (unsigned char)nick[loop];
	}

	hash ^= (hash >> 16); /* FvH */
	hash &= 0xffff;

	return hash;
}

void find_nick_colorpair(const char *nick, nick_color_settings *pncs)
{
	if (nick)
	{
		int hash = hash_nick(nick);

		pncs -> bold = hash & 1;
		pncs -> pair = nick_pairs[(hash >> 1) % n_nick_pairs];
	}
	else
	{
		pncs -> bold = FALSE;
		pncs -> pair = default_colorpair;
	}
}

void init_nick_colorpairs(void)
{
	int loop = 0;
	short fg = 0, bg = 0;
	short fg_r = 0, fg_g = 0, fg_b = 0;
	short bg_r = 0, bg_g = 0, bg_b = 0;

	nick_pairs = (int *)calloc(1, COLOR_PAIRS * sizeof(int));
	n_nick_pairs = 0;

	/* they're 0 zo hardcoded */
	pair_content(0, &fg, &bg);
	color_content(fg, &fg_r, &fg_g, &fg_b);
	color_content(bg, &bg_r, &bg_g, &bg_b);
	fg_r = 255;
	fg_b = 255;
	fg_g = 255;
	bg_r = 0;
	bg_g = 0;
	bg_b = 0;

	for(loop=0; loop<COLORS; loop++)
	{
		short cr = 0, cg = 0, cb = 0;

		color_content(loop, &cr, &cg, &cb);

		if ((cr != fg_r || cg != fg_g || cb != fg_b) && (cr != bg_r || cg != bg_g || cb != bg_b))
		{
			int pair = 0;

			for(pair=0; pair<COLOR_PAIRS; pair++)
			{
				pair_content(pair, &fg, &bg);

				if (fg == loop)
				{
					nick_pairs[n_nick_pairs++] = pair;
					break;
				}
			}
		}
	}
}

void init_nick_coloring(hash_types hf)
{
	hash_function = hf;

	init_nick_colorpairs();
}
