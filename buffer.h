/* GPLv2 applies
 * SVN revision: $Revision: 844 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#ifndef __BUFFER_H__
#define __BUFFER_H__

#include <time.h>

typedef enum { BET_REGULAR, BET_META, BET_MARKERLINE } be_type_t;

typedef struct
{
	const char *msg, *msg_from;
	be_type_t line_type;
	time_t when;
	int sr, ch; /* server/channel */
} buffer_element_t;

typedef struct
{
	int n_records;
	buffer_element_t *records;

	int max_n_elements;

	int last_shown;
} buffer;

buffer * create_buffer(const int max_channel_record_lines);
void truncate_buffer(buffer *pbuffer);
void free_buffer(buffer *pbuffer);
int get_buffer_n_elements(const buffer *pb);
buffer_element_t *get_from_buffer(const buffer *pb, int pos);
void add_buffer_to_buffer(buffer *target, const buffer *source);
void add_to_buffer(buffer *pbuffer, const char *what, const char *what_from, const BOOL is_meta, const int sr, const int ch);
void search_in_buffer(const buffer *in, buffer *result, const char *search_what, BOOL fuzzy);
buffer * search_in_buffer_new(const buffer *in, const char *search_what, BOOL fuzzy);
void sort_buffer(buffer *work, BOOL direction);
BOOL is_markerline(buffer *pbuffer, int pos);
BOOL latest_is_markerline(buffer *pb);
void delete_type(buffer *pb, be_type_t type);
int search_in_buffer_index(const buffer *in, const char *what, int search_offset);

#endif
