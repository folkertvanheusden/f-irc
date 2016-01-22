/* GPLv2 applies
 * SVN revision: $Revision: 671 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#ifndef __NICKCOLOR_H__
#define __NICKCOLOR_H__

typedef enum { LRC, DJB2 } hash_types;

typedef struct
{
	int pair;
	BOOL bold;
} nick_color_settings;

extern int n_nick_pairs;

void init_nick_coloring(hash_types hf);
void find_nick_colorpair(const char *nick, nick_color_settings *pncs);

#endif
