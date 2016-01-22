/* GPLv2 applies
 * SVN revision: $Revision: 709 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#ifndef __UTF8__
#define __UTF8__

#include <wchar.h>

typedef struct
{
	char ascii;

	char utf8[4];
} utf8_byte;

typedef struct
{
	utf8_byte *string;
	int len;

	BOOL working;
	char work_buffer[4];
	int work_buffer_in;
} utf8_string;

utf8_string *alloc_utf8_string();
utf8_string *utf8_strdup(const utf8_string *in);
void free_utf8_string(const utf8_string *in);
void truncate_utf8_string(utf8_string *work);
BOOL add_stream_to_utf8_string(utf8_string *out, const char cur_byte);
void add_utf8_to_utf8_string(utf8_string *out, const utf8_byte in);
void utf8_strcat_ascii(utf8_string *work, const char *in);
void utf8_strcat_utf8_string(utf8_string *work, const utf8_string *in);
int utf8_strlen(const utf8_string *in);
char *utf8_get_ascii(const utf8_string *in);
char *utf8_get_ascii_pos(const utf8_string *in, const int pos);
char *utf8_get_utf8(const utf8_string *in);
char *utf8_get_utf8_pos(const utf8_string *in, const int pos);
wchar_t *utf8_get_wchar(const utf8_string *in);
wchar_t *utf8_get_wchar_pos(const utf8_string *in, const int pos);
void utf8_del_pos(utf8_string *in, const int pos);
void utf8_insert_pos_ascii(utf8_string *work, const int pos, const int c);
void utf8_insert_pos_utf8(utf8_string *work, const int pos, const utf8_byte in);
int utf8_find_nonblank_reverse(const utf8_string *work, const int start);
int utf8_find_nonblank(const utf8_string *work, const int start);
int utf8_find_blank_reverse(const utf8_string *work, const int start);
int utf8_ascii_get_at(const utf8_string *in, const int pos);
void utf8_ascii_set_at(utf8_string *work, const int pos, const char what);
void utf8_truncate(utf8_string *work, const int new_size);

int count_utf_bytes(int c);

#endif
