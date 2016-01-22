/* GPLv2 applies
 * SVN revision: $Revision: 749 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#ifndef __STRING_ARRAY_H__
#define __STRING_ARRAY_H__

#include <stdio.h>

#include "gen.h"

typedef struct
{
	const char **data;
	int n;
} string_array_t;

void init_string_array(string_array_t *p);
void free_string_array(string_array_t *p);
void add_to_string_array(string_array_t *p, const char *what);
int find_str_in_string_array(const string_array_t *p, const char *what, BOOL ignore_case);
void del_str_from_string_array(string_array_t *p, const char *what);
void del_nr_from_string_array(string_array_t *p, int nr);
int string_array_get_n(const string_array_t *p);
const char* string_array_get(const string_array_t *p, int idx);
void insert_into_string_array(string_array_t *p, int idx, const char *what);
void replace_in_string_array(string_array_t *p, int idx, const char *what);
void sort_string_array(string_array_t *p);
int partial_match_search_string_array(const string_array_t *p, const char *what);
void split_string(const char *in, const char *split, BOOL clean, string_array_t *parts);
void free_splitted_string(string_array_t *prats);
BOOL dump_string_array(const string_array_t *p, const char *name, FILE *fh);

#endif
