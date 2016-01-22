/* GPLv2 applies
 * SVN revision: $Revision: 671 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#define _GNU_SOURCE
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncursesw/ncurses.h>

#include "gen.h"
#include "error.h"
#include "utils.h"

typedef struct {
	int r, g, b;
	const char *name;
} rgb_t;

rgb_t mirc_col[16] = {
	{ 999, 999, 999, "white" },	/* 0 white */
	{   0,   0,   0, "black" },	/* 1 black */
	{   0,   0, 500, "blue"  },	/* 2 navy blue */
	{   0, 999,   0, "green" },	/* 3 green */
	{ 999,   0,   0, "red"   },	/* 4 red */
	{ 482,  66,  74, "maroon" },	/* 5 maroon */
	{ 666,   0, 999, "purple" },	/* 6 purple ("paars") */
	{ 999, 647,   0, "orange" },	/* 7 orange */
	{ 999, 999,   0, "yellow" },	/* 8 yellow */
	{ 196, 803, 196, "limegreen" },	/* 9 lime green */
	{   0, 999, 999, "cyan" },	/* 10 cyan */
	{ 878, 999, 999, "lightcyan" },	/* 11 light cyan */
	{ 678, 847, 901, "lightblue" },	/* 12 light blue */
	{ 999, 752, 796, "pink" },	/* 13 pink */
	{ 500, 500, 500, "grey" },	/* 14 grey */
	{ 750, 750, 750, "silvergrey" }	/* 15 silver grey */
		};

/* ncurses color (COLOR_...) to mirc color index */
int nc_to_mirc[8] = { 1, 4, 3, 8, 2, 12, 10, 0 };

int mirc_color_cache[16] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

typedef struct
{
	BOOL fg, bg;
} is_default_color;

is_default_color *pidc = NULL;
int n_pidc = 0;

int cpairs = 1;

void free_colors(void)
{
	free(pidc);
}

void set_idc(int pair, BOOL fg, BOOL bg)
{
	int new_n = pair + 1, index = 0;

	if (new_n > n_pidc)
		pidc = (is_default_color *)realloc(pidc, new_n * sizeof(is_default_color));

	for(index=n_pidc; index<new_n; index++)
		pidc[index].fg = pidc[index].bg = FALSE;

	pidc[pair].fg = fg;
	pidc[pair].bg = bg;

	if (new_n > n_pidc)
		n_pidc = new_n;
}

BOOL get_idc_fg(int pair)
{
	if (pair < n_pidc)
		return pidc[pair].fg;

	return FALSE;
}

BOOL get_idc_bg(int pair)
{
	if (pair < n_pidc)
		return pidc[pair].bg;

	return FALSE;
}

/* taken & adapted from http://www.compuphase.com/cmetric.htm */
double colour_distance(rgb_t *e1, rgb_t *e2)
{
	double r_mean = ((double)e1 -> r + (double)e2 -> r) / 2.0;
	double rd = pow(e1 -> r - e2 -> r, 2.0);
	double gd = pow(e1 -> g - e2 -> g, 2.0);
	double bd = pow(e1 -> b - e2 -> b, 2.0);

	return sqrt((((512.0 + r_mean) * rd) / 256.0) + 4.0 * gd + (((767.0 - r_mean) * bd) / 256.0));
}

/* find an RGB value in the COLORS RGB triplets as available by ncurses */
int find_rgb(rgb_t *col)
{
	int index = 0, chosen = -1;
	double max = 2000000000.0;

	for(index=0; index<COLORS; index++)
	{
		short cr = 0, cg = 0, cb = 0;
		double cd = 0;
		rgb_t crgb;

		color_content(index, &cr, &cg, &cb);

		crgb.r = cr;
		crgb.g = cg;
		crgb.b = cb;

		cd = colour_distance(&crgb, col);

		if (cd < max)
		{
			max = cd;
			chosen = index;
		}
	}

	return chosen;
}

int get_n_cpairs(void)
{
	return cpairs;
}

int find_alloc_colorpair(int fg, int bg)
{
	int index = 0, curmaxpairs = cpairs;

	for(index = 0; index<cpairs; index++)
	{
		short cfg = 0, cbg = 0;

		pair_content(index, &cfg, &cbg);

		if ((cfg == fg || (fg == -1 && get_idc_fg(index))) && (cbg == bg || (bg == -1 && get_idc_bg(index))))
			return index;
	}

	if (cpairs >= COLOR_PAIRS - 1)
		return 0;

	set_idc(cpairs, fg == -1, bg == -1);

	if (init_pair(cpairs++, fg, bg) != OK)
		error_exit(FALSE, "init_pair(%d: %d,%d) failed", curmaxpairs, fg, bg);

	return curmaxpairs;
}

void test_colors(void)
{
	int id0 = find_alloc_colorpair(COLOR_RED, -1);
	int id1 = find_alloc_colorpair(COLOR_RED, -1);
	int id2 = find_alloc_colorpair(COLOR_GREEN, -1);
	int id3 = find_alloc_colorpair(COLOR_RED, -1);

	LOG("%d %d,%d\n", id0, get_idc_fg(id0), get_idc_bg(id0));
	LOG("%d %d,%d\n", id1, get_idc_fg(id1), get_idc_bg(id1));
	LOG("%d %d,%d\n", id2, get_idc_fg(id2), get_idc_bg(id2));
	LOG("%d %d,%d\n", id3, get_idc_fg(id3), get_idc_bg(id3));

	error_exit(FALSE, "");
}

int get_color_mirc(int mfg, int mbg)
{
	int pair = 0;
	int fg = -1, bg = -1;

	if (mfg == -1 && mbg == -1)
		set_idc(0, TRUE, TRUE);
	else if (mfg == mbg)
	{
		mbg++;
		mbg &= 0x0f;
	}

	if (mfg != -1 && mfg < 16)
	{
		if (mirc_color_cache[mfg] != -1)
			fg = mirc_color_cache[mfg];
		else
			mirc_color_cache[mfg] = fg = find_rgb(&mirc_col[mfg]);
	}

	if (mbg != -1 && mbg < 16)
	{
		if (mirc_color_cache[mbg] != -1)
			bg = mirc_color_cache[mbg];
		else
			mirc_color_cache[mbg] = bg = find_rgb(&mirc_col[mbg]);
	}

	pair = find_alloc_colorpair(fg, bg);

	return pair;
}

int get_color_ncurses(int ncol_fg, int ncol_bg)
{
	int pair = 0;

	if (ncol_fg == -1 && ncol_bg == -1)
		set_idc(0, TRUE, TRUE);
	else if (ncol_fg == ncol_bg)
	{
		ncol_bg++;
		ncol_bg %= COLORS;
	}

	pair = find_alloc_colorpair(ncol_fg, ncol_bg);

	return pair;
}

/* -1: default color,
 * >= 0: color x
 * -2: error
 */
int color_str_convert(const char *in)
{
	int index = 0;

	if (in[0] == 0x00)
		return -1;

	if (in[0] == '#')
	{
		rgb_t triplet;

		if (strlen(&in[1]) != 9)
			return -2;

		triplet.r = hextoint(&in[1], 3);
		triplet.g = hextoint(&in[4], 3);
		triplet.b = hextoint(&in[7], 3);

		return find_rgb(&triplet);
	}

	if (strcasecmp(in, "default") == 0)
		return -1;

	for(index=0; index<16; index++)
	{
		if (strcasecmp(mirc_col[index].name, in) == 0)
                	return find_rgb(&mirc_col[index]);
	}

	return -2;
}

char *color_to_str(int color_nr)
{
	int index = 0;
	short cr = 0, cg = 0, cb = 0;
	char *name = NULL;

	if (color_nr == -1)
		return strdup("default");

	color_content(color_nr, &cr, &cg, &cb);

	for(index=0; index<16; index++)
	{
		if (mirc_col[index].r == cr && mirc_col[index].g == cg && mirc_col[index].b == cb)
			return strdup(mirc_col[index].name);
	}

	asprintf(&name, "#%03x%03x%03x", cr, cg, cb);

	return name;
}

void emit_colorpair(FILE *fh, short pair)
{
	char *fg_str = NULL, *bg_str = NULL;
	short cfg = 0, cbg = 0;

	pair_content(pair, &cfg, &cbg);

	if (cfg == cbg || pair == 0)
	{
		fprintf(fh, "default,default");
		return;
	}

	fg_str = get_idc_fg(pair) ? color_to_str(-1) : color_to_str(cfg);
	bg_str = get_idc_bg(pair) ? color_to_str(-1) : color_to_str(cbg);

	fprintf(fh, "%s,%s", fg_str, bg_str);

	free(bg_str);
	free(fg_str);
}
