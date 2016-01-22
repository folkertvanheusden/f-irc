/* GPLv2 applies
 * SVN revision: $Revision: 671 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gen.h"
#include "utils.h"

int ansi_color_to_ncurses(int nr)
{
	switch(nr)
	{
		case 0:	/* black */
			return 1;
		case 1:	/* red */
			return 4;
		case 2:	/* green */
			return 3;
		case 3:	/* yellow */
			return 8;
		case 4:	/* blue */
			return 2;
		case 5:	/* magenta */
			return 12;
		case 6:	/* cyan */
			return 11;
		case 7:	/* white */
			return 0;
	}

	return -1;
}

void convert_ansi(const char *ansi_in, char *out, int *out_index, int out_len, BOOL *has_color)
{
	char *ansi = strdup(ansi_in);
	int len = strlen(ansi);
	int cmd = 0, index = 0;
	string_array_t parts;
	BOOL bold = FALSE, underline = FALSE, reverse = FALSE;
	int fgc = 0, bgc = 1; /* white on black is default */
	const char *values = ansi + 1; /* skip ^[ */
	BOOL new_has_color = FALSE;

	init_string_array(&parts);

	/* remove terminating 'm' */
	if (len > 0)
	{
		cmd = ansi[len - 1];
		ansi[len - 1] = 0x00;
	}

	if (cmd != 'm')
		LOG("unexpected ansi command %c!\n", cmd);

	if (values[0] == '[')
		values++;

	split_string(values, ";", TRUE, &parts);

	for(index=0; index<string_array_get_n(&parts); index++)
	{
		int val = atoi(string_array_get(&parts, index));

		if (val == 0)	/* reset attributes */
		{
			bold = underline = reverse = FALSE;
			fgc = 0;
			bgc = 1;
			new_has_color = FALSE;
		}
		else if (val == 1)	/* bold */
			new_has_color = bold = TRUE;
		else if (val == 4)	/* underline */
			new_has_color = underline = TRUE;
		else if (val == 6)	/* blink, use reverse */
			new_has_color = reverse = TRUE;
		else if (val == 7)	/* reverse */
			new_has_color = reverse = TRUE;
		else if (val >= 30 && val <= 37)	/* foreground color */
		{
			fgc = ansi_color_to_ncurses(val - 30);
			new_has_color = TRUE;
		}
		else if (val >= 40 && val <= 47)	/* background color */
		{
			bgc = ansi_color_to_ncurses(val - 40);
			new_has_color = TRUE;
		}
	}

	if (*has_color)
		*out_index += snprintf(&out[*out_index], out_len - *out_index, "\x03"); /* ^c terminate previous sequence */

	if (bold)
		*out_index += snprintf(&out[*out_index], out_len - *out_index, "\x02"); /* ^B */
	if (underline)
		*out_index += snprintf(&out[*out_index], out_len - *out_index, "\x1f"); /* ^U */
	if (reverse)
		*out_index += snprintf(&out[*out_index], out_len - *out_index, "\x16"); /* ^V */

	*out_index += snprintf(&out[*out_index], out_len - *out_index, "\x03%d,%d", fgc, bgc);

	*has_color = new_has_color;

	free_splitted_string(&parts);

	free(ansi);
}

const char *filter_ansi(const char *string_in)
{
	int len = strlen(string_in);
	int out_len = len * 8  + 1;
	char *out = (char *)calloc(1, out_len);
	char *ansi = (char *)calloc(1, len + 1);
	int index = 0, out_index = 0, ansi_index = 0;
	BOOL is_ansi = FALSE, has_color = FALSE;

	for(index=0; index<len; index++)
	{
		int c = string_in[index];

		if (is_ansi)
		{
			ansi[ansi_index++] = c;

			/* 16 is an arbitrary limit on the length of ansi escape sequences */
			if (isalpha(c) || ansi_index > 16)
			{
				if (c == 'm') /* color */
					convert_ansi(ansi, out, &out_index, out_len - 1, &has_color);

				is_ansi = FALSE;
				ansi_index = 0;
			}
		}
		else if (c == 27)
		{
			is_ansi = TRUE;

			ansi[ansi_index++] = c;
		}
		else
			out[out_index++] = c;
	}

	if (has_color)
		out[out_index++] = 0x03;

	out[out_index] = 0x00;

	free(ansi);

	return out;
}
