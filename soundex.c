/* GPLv2 applies
 * SVN revision: $Revision: 671 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "gen.h"
#include "utils.h"
#include "string_array.h"

/* Returns the soundex equivalent to in 
 * adapted from http://physics.nist.gov/cuu/Reference/soundex.html
 */
char *soundex(const char *in)
{
	int index_in = 1, index_out = 1;
	char prev_dig, *out = (char *)malloc(5), cur_char;

	memset(out, '0', 4);

	out[0] = tolower(in[0]);
	prev_dig = out[0];

	while(in[index_in] && index_out < 4) 
	{
		switch(tolower(in[index_in++]))
		{
			case 'b' : cur_char = '1'; break;
			case 'p' : cur_char = '1'; break;
			case 'f' : cur_char = '1'; break;
			case 'v' : cur_char = '1'; break;
			case 'c' : cur_char = '2'; break;
			case 's' : cur_char = '2'; break;
			case 'k' : cur_char = '2'; break;
			case 'g' : cur_char = '2'; break;
			case 'j' : cur_char = '2'; break;
			case 'q' : cur_char = '2'; break;
			case 'x' : cur_char = '2'; break;
			case 'z' : cur_char = '2'; break;
			case 'd' : cur_char = '3'; break;
			case 't' : cur_char = '3'; break;
			case 'l' : cur_char = '4'; break;
			case 'm' : cur_char = '5'; break;
			case 'n' : cur_char = '5'; break;
			case 'r' : cur_char = '6'; break;
			default : cur_char = '*';
		}

		if (cur_char != prev_dig && cur_char != '*')
			out[index_out++] = prev_dig = cur_char;
	}

	out[4] = 0x00;
 
	return out;
}

BOOL fuzzy_match(const char *haystackIn, const char *needle, char *bitmap)
{
	const char *needleS = soundex(needle);
	char *haystack = strdup(haystackIn), *search_start = haystack;
	int loop = 0, len = strlen(haystackIn);
	BOOL match = FALSE;
	string_array_t hsS;

	init_string_array(&hsS);

	for(loop=0; loop<len; loop++)
	{
		if (!isalpha(haystack[loop]))
			haystack[loop] = ' ';
	}

	split_string(haystack, " ", TRUE, &hsS);

	for(loop=0; loop<string_array_get_n(&hsS); loop++)
	{
		const char *word_in = string_array_get(&hsS, loop);
		const char *cur = soundex(word_in);

		if (strcmp(cur, needleS) == 0)
		{
			char *found_at = strstr(search_start, word_in);
			int pos = (int)(found_at - haystack), word_len = strlen(word_in);

			match = TRUE;

			if (bitmap != NULL && pos >= 0 && pos <= len - word_len)
				memset(&bitmap[pos], '1', word_len);

			if (!bitmap)
			{
				myfree(cur);
				break;
			}

			search_start = found_at + word_len;
		}

		myfree(cur);
	}

	free_splitted_string(&hsS);
	myfree(needleS);

	free(haystack);

	return match;
}
