/* GPLv2 applies
 * SVN revision: $Revision: 687 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
typedef struct
{
	char *data;
	int size;
} lf_buffer_t;

void init_lf_buffer(lf_buffer_t *p);
void free_lf_buffer(lf_buffer_t *p);
void add_lf_buffer(lf_buffer_t *p, const char *what, int what_size);
const char *get_line_lf_buffer(lf_buffer_t *p);
