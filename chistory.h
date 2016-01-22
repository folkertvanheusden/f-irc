/* GPLv2 applies
 * SVN revision: $Revision: 671 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
typedef struct
{
	int s, c;
} ch_t;

void init_channel_history(int max_n);
void free_channel_history(void);
ch_t pop_channel_history(void);
void push_channel_history(const int s, const int c);
