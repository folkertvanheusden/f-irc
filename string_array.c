/* GPLv2 applies
 * SVN revision: $Revision: 749 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#include <stdlib.h>
#include <string.h>

#include "string_array.h"
#include "utils.h"

void init_string_array(string_array_t *p)
{
	p -> data = NULL;
	p -> n = 0;
}

void free_string_array(string_array_t *p)
{
	int i = 0;

	for(i=0; i < p -> n; i++)
		myfree(p -> data[i]);

	myfree(p -> data);
	p -> data = NULL;

	p -> n = 0;
}

void add_to_string_array(string_array_t *p, const char *what)
{
	p -> data = (const char **)realloc(p -> data, (p -> n + 1) * sizeof(char *));

	p -> data[p -> n] = strdup(what);
	p -> n++;
}

int find_str_in_string_array(const string_array_t *p, const char *what, BOOL ignore_case)
{
	int i = 0;

	for(i=0; i<p -> n; i++)
	{
		if (ignore_case && strcasecmp(p -> data[i], what) == 0)
			return i;

		if (!ignore_case && strcmp(p -> data[i], what) == 0)
			return i;
	}

	return -1;
}

void del_nr_from_string_array(string_array_t *p, int nr)
{
	int n_to_move = (p -> n - nr) - 1;

	myfree(p -> data[nr]);

	if (n_to_move > 0)
		memmove(&p -> data[nr], &p -> data[nr + 1], n_to_move * sizeof(char *));

	p -> n--;
}

void del_str_from_string_array(string_array_t *p, const char *what)
{
	int idx = find_str_in_string_array(p, what, FALSE);

	if (idx != -1)
		del_nr_from_string_array(p, idx);
}

int string_array_get_n(const string_array_t *p)
{
	return p -> n;
}

const char* string_array_get(const string_array_t *p, int idx)
{
	return p -> data[idx];
}

void insert_into_string_array(string_array_t *p, int idx, const char *what)
{
	int n_to_move = p -> n - idx;

	p -> data = (const char **)realloc(p -> data, (p -> n + 1) * sizeof(char *));

	if (n_to_move > 0)
		memmove(&p -> data[idx + 1], &p -> data[idx], sizeof(char *) * n_to_move);

	p -> data[idx] = strdup(what);

	p -> n++;
}

void replace_in_string_array(string_array_t *p, int idx, const char *what)
{
	myfree(p -> data[idx]);

	p -> data[idx] = strdup(what);
}

void split_string(const char *in, const char *split, BOOL clean, string_array_t *parts)
{
	char *copy = strdup(in), *pc = copy;
	int split_len = strlen(split);

	for(;;)
	{
		char *term = strstr(pc, split);

		if (term)
			*term = 0x00;

		add_to_string_array(parts, pc);

		if (!term)
			break;

		pc = term + split_len;

		if (clean)
		{
			while(strncmp(pc, split, split_len) == 0)
				pc += split_len;
		}
	}

	free(copy);
}

void free_splitted_string(string_array_t *parts)
{
	free_string_array(parts);
}

static int cmp(const void *a, const void *b) 
{ 
	const char **ia = (const char **)a;
	const char **ib = (const char **)b;

	return strcmp(*ia, *ib);
} 

void sort_string_array(string_array_t *p)
{
	qsort(p -> data, p -> n, sizeof(char *), cmp);
}

int partial_match_search_string_array(const string_array_t *p, const char *what)
{
        int imin = 0, imax = p -> n - 1, what_len = strlen(what);

        while(imax >= imin)
        {
                int imid = (imin + imax) / 2, cmp = 0;

		if (memcmp(p -> data[imid], what, what_len) == 0)
			return imid;

                cmp = strcasecmp(p -> data[imid], what);

                if (cmp < 0)
                        imin = imid + 1;
                else /* if (cmp > 0) */
                        imax = imid - 1;
        }

	return -1;
}

BOOL dump_string_array(const string_array_t *p, const char *name, FILE *fh)
{
	int index = 0;

	for(index=0; index<p -> n; index++)
	{
		if (fprintf(fh, "%s=%s\n", name, p -> data[index]) == -1)
			return FALSE;
	}

	return TRUE;
}
