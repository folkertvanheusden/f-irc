/* GPLv2 applies
 * SVN revision: $Revision: 695 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#include <stdlib.h>
#include <string.h>

#include "lf_buffer.h"
#include "utils.h"

void init_lf_buffer(lf_buffer_t *p)
{
	p -> data = NULL;
	p -> size = 0;
}

void free_lf_buffer(lf_buffer_t *p)
{
	free(p -> data);

	p -> data = NULL;
	p -> size = 0;
}

void add_lf_buffer(lf_buffer_t *p, const char *what, int what_size)
{
	p -> data = (char *)realloc(p -> data, p -> size + what_size + 1);

	memcpy(&p -> data[p -> size], what, what_size);

	p -> data[p -> size + what_size] = 0x00;

	p -> size += what_size;
}

const char *get_line_lf_buffer(lf_buffer_t *p)
{
	char *out = NULL, *end_in = NULL;
	int len = 0, left = 0;

	if (p -> data == NULL || p -> size == 0)
		return NULL;

	end_in = strstr(p -> data, "\r\n");
	if (!end_in)
		return NULL;

	out = strdup(p -> data);
	len = (int)(end_in - p -> data) + 2;

	terminate_str(out, '\r');
	terminate_str(out, '\n');

	left = p -> size - len;
	if (left > 0)
		memmove(&p -> data[0], &p -> data[len], left + 1);

	p -> size = left;

	return out;
}
