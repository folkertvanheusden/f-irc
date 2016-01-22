/* GPLv2 applies
 * SVN revision: $Revision: 671 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#include <stdlib.h>
#include <string.h>

#include "chistory.h"

ch_t *channel_history = NULL;
int channel_history_max_n = 2;

void init_channel_history(int max_n)
{
	int loop = 0;

	channel_history = calloc(sizeof(ch_t), max_n);

	channel_history_max_n = max_n;

	for(loop=0; loop<max_n; loop++)
		channel_history[loop].s = channel_history[loop].c = -1;
}

void free_channel_history(void)
{
	free(channel_history);

	channel_history = NULL;
}

ch_t pop_channel_history(void)
{
	int n_min_1 = channel_history_max_n - 1;
	ch_t last = channel_history[n_min_1];

	memmove(&channel_history[1], &channel_history[0], sizeof(ch_t) * n_min_1);

	channel_history[0].s = channel_history[0].c = -1;

	return last;
}

void push_channel_history(const int s, const int c)
{
	int n_min_1 = channel_history_max_n - 1;

	memmove(&channel_history[0], &channel_history[1], sizeof(ch_t) * n_min_1);

	channel_history[channel_history_max_n - 1].s = s;
	channel_history[channel_history_max_n - 1].c = c;
}
