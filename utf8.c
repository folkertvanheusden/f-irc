/* GPLv2 applies
 * SVN revision: $Revision: 671 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "gen.h"
#include "utf8.h"
#include "utils.h"

utf8_string *alloc_utf8_string()
{
	return (utf8_string *)calloc(1, sizeof(utf8_string));
}

utf8_string *utf8_strdup(const utf8_string *in)
{
	utf8_string *out = alloc_utf8_string();

	int n_bytes = in -> len * sizeof(utf8_byte);
	out -> string = (utf8_byte *)malloc(n_bytes);
	memcpy(out -> string, in -> string, n_bytes);

	out -> len = in -> len;

	return out;
}

void free_utf8_string(const utf8_string *in)
{
	myfree(in -> string);
	myfree((void *)in);
}

void truncate_utf8_string(utf8_string *work)
{
	free(work -> string);
	work -> string = NULL;
	work -> len = 0;

	work -> working = FALSE;
	work -> work_buffer_in = 0;
	memset(work -> work_buffer, 0x00, sizeof work -> work_buffer);
}

void char_to_utf8_byte(const int c, utf8_byte *out)
{
	out -> ascii = c;

	memset(out -> utf8, 0x00, sizeof(out -> utf8));
}

BOOL add_stream_to_utf8_string(utf8_string *out, const char cur_byte)
{
	if (out -> working)
	{
		out -> work_buffer[out -> work_buffer_in++] = cur_byte;

		if (out -> work_buffer_in == count_utf_bytes(out -> work_buffer[0]))
		{
			utf8_byte ub;

			ub.ascii = (out -> work_buffer[0] & 0x3f) | ((out -> work_buffer[1] & 3) << 6);
			memcpy(ub.utf8, out -> work_buffer, sizeof ub.utf8);

			add_utf8_to_utf8_string(out, ub);

			out -> working = FALSE;
			out -> work_buffer_in = 0;
			memset(out -> work_buffer, 0x00, sizeof out -> work_buffer);

			return TRUE;
		}

		return FALSE;
	}
	else if (count_utf_bytes(cur_byte) > 1)
	{
		out -> working = TRUE;
		out -> work_buffer[out -> work_buffer_in++] = cur_byte;

		return FALSE;
	}
	else
	{
		utf8_byte ub;

		ub.ascii = cur_byte;
		ub.utf8[0] = 0x00;

		add_utf8_to_utf8_string(out, ub);

		return TRUE;
	}

	return FALSE;
}

void utf8_strcat_ascii(utf8_string *work, const char *in)
{
	const int len = strlen(in);
	int loop;

	for(loop=0; loop<len; loop++)
		add_stream_to_utf8_string(work, in[loop]);
}

void utf8_strcat_utf8_string(utf8_string *work, const utf8_string *in)
{
	const int len = utf8_strlen(in);
	int loop;

	for(loop=0; loop<len; loop++)
		add_utf8_to_utf8_string(work, in -> string[loop]);
}

void add_utf8_to_utf8_string(utf8_string *out, const utf8_byte in)
{
	assert(out -> len >= 0);
	out -> string = realloc(out -> string, (out -> len + 1) * sizeof(utf8_byte));

	memcpy(&out -> string[out -> len], &in, sizeof in);

	out -> len++;
}

int utf8_strlen(const utf8_string *in)
{
	assert(in -> len >= 0);
	return in -> len;
}

char *utf8_get_ascii(const utf8_string *in)
{
	return utf8_get_ascii_pos(in, 0);
}

char *utf8_get_ascii_pos(const utf8_string *in, const int pos)
{
	char *out = (char *)malloc(in -> len + 1);
	int loop;

	assert(pos >= 0);
	assert(pos < in -> len || (in -> len == 0 && pos == 0));
	assert(in -> len >= 0);

	for(loop=pos; loop<in -> len; loop++)
		out[loop - pos] = in -> string[loop].ascii;

	out[loop - pos] = 0x00;

	return out;
}

char *utf8_get_utf8(const utf8_string *in)
{
	return utf8_get_utf8_pos(in, 0);
}

char *utf8_get_utf8_pos(const utf8_string *in, const int pos)
{
	int out_index = 0, loop;
	char *out = (char *)malloc(in -> len * 4 + 1);

	assert(pos >= 0);
	assert(pos < in -> len || (in -> len == 0 && pos == 0));
	assert(in -> len >= 0);

	for(loop=pos; loop<in -> len; loop++)
	{
		if (in -> string[loop].utf8[0])
		{
			int n = count_utf_bytes(in -> string[loop].utf8[0]);

			memcpy(&out[out_index], in -> string[loop].utf8, n);

			out_index += n;
		}
		else
		{
			out[out_index++] = in -> string[loop].ascii;
		}
	}

	out[out_index] = 0x00;

	return out;
}

wchar_t *utf8_get_wchar(const utf8_string *in)
{
	return utf8_get_wchar_pos(in, 0);
}

wchar_t *utf8_get_wchar_pos(const utf8_string *in, int pos)
{
	wchar_t *out = (wchar_t *)malloc((in -> len + 1) * sizeof(wchar_t));
	char *temp = utf8_get_utf8_pos(in, pos);
	const char *dummy = temp;
	int n = 0;

	assert(pos >= 0);
	assert(pos < in -> len || (in -> len == 0 && pos == 0));
	assert(in -> len >= 0);

	n = mbsrtowcs(out, &dummy, in -> len, NULL);
	if (n >= 0)
		out[n] = 0x00;

	myfree(temp);

	return out;
}

void utf8_del_pos(utf8_string *in, const int pos)
{
	int n_to_move = (in -> len - pos) - 1;

	assert(pos >= 0);
	assert(pos < in -> len || (in -> len == 0 && pos == 0));
	assert(in -> len >= 0);
	assert(n_to_move >= 0);

	if (n_to_move > 0)
		memmove(&in -> string[pos], &in -> string[pos + 1], n_to_move * sizeof(utf8_byte));

	in -> len--;
}

void utf8_insert_pos_utf8(utf8_string *work, const int pos, const utf8_byte in)
{
	assert(pos >= 0);
	assert(pos < work -> len || (work -> len == 0 && pos == 0));
	assert(work -> len >= 0);

	work -> string = realloc(work -> string, (work -> len + 1) * sizeof(utf8_byte));

	memmove(&work -> string[pos + 1], &work -> string[pos], (work -> len - pos) * sizeof(utf8_byte));
	memcpy(&work -> string[pos], &in, sizeof in);

	work -> len++;
}

void utf8_insert_pos_ascii(utf8_string *work, const int pos, const int c)
{
	utf8_byte ub;

	char_to_utf8_byte(c, &ub);

	utf8_insert_pos_utf8(work, pos, ub);
}

int utf8_find_nonblank_reverse(const utf8_string *work, const int start)
{
	int cur_pos = start;

	assert(start >= 0);

	if (cur_pos == work -> len)
		cur_pos--;

	assert(cur_pos < work -> len);

	while(cur_pos > 0 && work -> string[cur_pos].ascii == ' ')
		cur_pos--;

	if (cur_pos == 0 && work -> string[cur_pos].ascii == ' ')
		return -1;

	return cur_pos;
}

int utf8_find_nonblank(const utf8_string *work, const int start)
{
	int cur_pos = start;

	assert(start >= 0);
	assert(start < work -> len || (work -> len == 0 && start == 0));

	while(cur_pos < work -> len && work -> string[cur_pos].ascii == ' ')
		cur_pos++;

	if (cur_pos == work -> len)
		return -1;

	return cur_pos;
}

int utf8_find_blank_reverse(const utf8_string *work, const int start)
{
	int loop;

	assert(start >= 0);

	for(loop=start; loop > -1; loop--)
	{
		if (work -> string[loop].ascii == ' ')
			return loop;
	}

	return -1;
}

int utf8_ascii_get_at(const utf8_string *in, const int pos)
{
	assert(pos >= 0);
	assert(pos < in -> len || (in -> len == 0 && pos == 0));

	return in -> string[pos].ascii;
}

void utf8_ascii_set_at(utf8_string *work, const int pos, const char what)
{
	assert(pos >= 0);
	assert(pos < work -> len || (work -> len == 0 && pos == 0));

	work -> string[pos].ascii = what;

	memset(work -> string[pos].utf8, 0x00, sizeof(work -> string[pos].utf8));
	work -> string[pos].utf8[0] = what;
}

void utf8_truncate(utf8_string *work, const int new_size)
{
	assert(new_size >= 0 && new_size <= work -> len);

	work -> len = new_size;
}

int count_utf_bytes(int c)
{
	if ((c & 0xe0) == 0xc0)
		return 2;
	else if ((c & 0xf0) == 0xe0)
		return 3;
	else if ((c & 0xf8) == 0xf0)
		return 4;

	return 1;
}
