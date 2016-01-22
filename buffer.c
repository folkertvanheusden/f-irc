/* GPLv2 applies
 * SVN revision: $Revision: 798 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#define _GNU_SOURCE
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ncursesw/ncurses.h>
#include <ncursesw/panel.h>

#include "gen.h"
#include "term.h"
#include "buffer.h"
#include "channels.h"
#include "servers.h"
#include "loop.h"
#include "utils.h"
#include "main.h"

void truncate_buffer(buffer *pbuffer)
{
	int loop;

	for(loop=0; loop<pbuffer -> n_records; loop++)
	{
		myfree((void *)pbuffer -> records[loop].msg);
		myfree((void *)pbuffer -> records[loop].msg_from);
	}

	myfree(pbuffer -> records);

	pbuffer -> records = NULL;
	pbuffer -> n_records = 0;
}

void free_buffer(buffer *pbuffer)
{
	truncate_buffer(pbuffer);
	free(pbuffer);
}

buffer * create_buffer(const int max_channel_record_lines)
{
	buffer *pbuffer = malloc(sizeof(buffer));

	assert(max_channel_record_lines >= 0);

	memset(pbuffer, 0x00, sizeof(buffer));

	pbuffer -> max_n_elements = max_channel_record_lines;

	return pbuffer;
}

int get_buffer_n_elements(const buffer *pb)
{
	assert(pb -> n_records >= 0);

	return pb -> n_records;
}

buffer_element_t *get_from_buffer(const buffer *pb, int pos)
{
	assert(pos < pb -> n_records);
	assert(pos >= 0);

	return &pb -> records[pos];
}

BOOL is_markerline(buffer *pb, int pos)
{
	assert(pos < pb -> n_records);
	assert(pos >= 0);

	return pb -> records[pos].line_type == BET_MARKERLINE;
}

BOOL latest_is_markerline(buffer *pb)
{
	if (pb -> n_records == 0)
		return FALSE;

	return pb -> records[pb -> n_records - 1].line_type == BET_MARKERLINE;
}

void add_to_buffer(buffer *pbuffer, const char *what, const char *what_from, const BOOL is_meta, const int sr, const int ch)
{
	be_type_t type = BET_REGULAR;

	if (is_meta)
		type = BET_META;

	if (!what)
	{
		what = "";
		type = BET_MARKERLINE;
	}

	if (pbuffer -> n_records < pbuffer -> max_n_elements || pbuffer -> max_n_elements == 0)
	{
		pbuffer -> records = (buffer_element_t *)realloc(pbuffer -> records, sizeof(buffer_element_t) * (pbuffer -> n_records + 1));
		pbuffer -> records[pbuffer -> n_records].msg = strdup(what);
		pbuffer -> records[pbuffer -> n_records].msg_from = what_from ? strdup(what_from) : NULL;
		pbuffer -> records[pbuffer -> n_records].line_type = type;
		pbuffer -> records[pbuffer -> n_records].when = time(NULL);
		pbuffer -> records[pbuffer -> n_records].sr = sr;
		pbuffer -> records[pbuffer -> n_records].ch = ch;
		pbuffer -> n_records++;
	}
	else
	{
		myfree(pbuffer -> records[0].msg);
		myfree(pbuffer -> records[0].msg_from);

		memmove(&pbuffer -> records[0], &pbuffer -> records[1], (pbuffer -> n_records - 1) * sizeof(buffer_element_t));
		pbuffer -> records[pbuffer -> n_records - 1].msg = strdup(what);
		pbuffer -> records[pbuffer -> n_records - 1].msg_from = what_from ? strdup(what_from) : NULL;
		pbuffer -> records[pbuffer -> n_records - 1].line_type = type;
		pbuffer -> records[pbuffer -> n_records - 1].when = time(NULL);
		pbuffer -> records[pbuffer -> n_records - 1].sr = sr;
		pbuffer -> records[pbuffer -> n_records - 1].ch = ch;
	}

	if (pbuffer -> last_shown > 0)
		pbuffer -> last_shown--;
}

void search_in_buffer(const buffer *in, buffer *result, const char *search_what, BOOL fuzzy)
{
	int index;

	for(index=0; index<in -> n_records; index++)
	{
		const char *msg = in -> records[index].msg;

		if (msg && strcasestr(msg, search_what))
		{
			buffer_element_t *pbe = &in -> records[index];

			add_to_buffer(result, msg, pbe -> msg_from, pbe -> line_type, pbe -> sr, pbe -> ch);
		}
	}
}

buffer * search_in_buffer_new(const buffer *in, const char *search_what, BOOL fuzzy)
{
	buffer *result = create_buffer(1000);

	search_in_buffer(in, result, search_what, fuzzy);

	return result;
}

int buffer_sorter_asc(const void *el1_in, const void *el2_in)
{
	buffer_element_t *el1 = (buffer_element_t *)el1_in;
	buffer_element_t *el2 = (buffer_element_t *)el2_in;

	return el1 -> when - el2 -> when;
}

int buffer_sorter_desc(const void *el1_in, const void *el2_in)
{
	buffer_element_t *el1 = (buffer_element_t *)el1_in;
	buffer_element_t *el2 = (buffer_element_t *)el2_in;

	return el2 -> when - el1 -> when;
}

void sort_buffer(buffer *work, BOOL direction)
{
	if (direction)
		qsort(work -> records, work -> n_records, sizeof(buffer_element_t), buffer_sorter_asc);
	else
		qsort(work -> records, work -> n_records, sizeof(buffer_element_t), buffer_sorter_desc);
}

void add_buffer_to_buffer(buffer *target, const buffer *source)
{
	int index=0;

	for(index=0; index<source -> n_records; index++)
	{
		buffer_element_t *pbe = &source -> records[index];

		add_to_buffer(target, pbe -> msg, pbe -> msg_from, pbe -> line_type == BET_META, pbe -> sr, pbe -> ch);
	}
}

void delete_type(buffer *pb, be_type_t type)
{
	int index=0;

	for(index=0; index<pb -> n_records;)
	{
		if (pb -> records[index].line_type == type)
		{
			int n_to_move = pb -> n_records - (index + 1);

			myfree(pb -> records[index].msg);
			myfree(pb -> records[index].msg_from);

			if (n_to_move > 0)
				memmove(&pb -> records[index], &pb -> records[index + 1], n_to_move * sizeof(buffer_element_t));

			pb -> n_records--;
		}
		else
		{
			index++;
		}
	}
}

int search_in_buffer_index(const buffer *in, const char *what, int search_offset)
{
	int index = 0;

	for(index=0; index<in -> n_records; index++)
	{
		int offset = (search_offset + index) % in -> n_records;

		if (strcasestr(in -> records[offset].msg, what))
			return offset;
	}

	return -1;
}
