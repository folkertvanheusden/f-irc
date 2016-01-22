/* GPLv2 applies
 * SVN revision: $Revision: 671 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gen.h"
#include "utils.h"
#include "key_value.h"

key_value * allocate_kv(void)
{
	return (key_value *)calloc(1, sizeof(key_value));
}

void free_kv(key_value *p)
{
	truncate_kv(p);

	free(p);
}

void bin_search(const key_value *work, const char *key, int *found_at, int *insert_at)
{
	int imin = 0, imax = work -> n - 1, found = -1;

	while(imax >= imin)
	{
		int imid = (imin + imax) / 2;
		int cmp = strcmp(work -> pairs[imid].key, key);

		if (cmp < 0)
			imin = imid + 1;
		else if (cmp > 0)
			imax = imid - 1;
		else
		{
			found = imid;
			break;
		}
	}

	if (found == -1)
	{
		*insert_at = imin;
		*found_at = -1;
	}
	else
	{
		*insert_at = -1;
		*found_at = found;
	}
}

int get_n_kv_from_kv(const key_value *in)
{
	return in -> n;
}

const char *get_key_by_index(const key_value *in, int nr)
{
	return in -> pairs[nr].key;
}

const void *get_value_by_index(const key_value *in, int nr)
{
	return in -> pairs[nr].value;
}

void add_to_kv(key_value *work, const char *key, const void *value)
{
	int found_at = -1, insert_at = -1;

	bin_search(work, key, &found_at, &insert_at);

	if (found_at != -1)
	{
		myfree(work -> pairs[found_at].value);

		work -> pairs[found_at].value = value;
	}
	else
	{
		int n_to_move = work -> n - insert_at;

		work -> pairs = realloc(work -> pairs, (work -> n + 1) * sizeof(key_value_pair));

		if (n_to_move)
			memmove(&work -> pairs[insert_at + 1], &work -> pairs[insert_at], n_to_move * sizeof(key_value_pair));

		work -> pairs[insert_at].key = key;
		work -> pairs[insert_at].value = value;

		work -> n++;
	}
}

const void * get_from_kv(const key_value *in, const char *key)
{
	int found_at = -1, insert_at = -1;

	bin_search(in, key, &found_at, &insert_at);

	if (found_at == -1)
		return NULL;

	return in -> pairs[found_at].value;
}

BOOL update_kv(key_value *work, const char *key, const void *new_value)
{
	int found_at = -1, insert_at = -1;

	bin_search(work, key, &found_at, &insert_at);

	if (found_at == -1)
		return FALSE;

	myfree(work -> pairs[found_at].value);

	work -> pairs[found_at].value = new_value;

	return TRUE;
}

void truncate_kv(key_value *work)
{
	if (work)
	{
		int loop;

		for(loop=0; loop<work -> n; loop++)
		{
			myfree(work -> pairs[loop].key);
			myfree(work -> pairs[loop].value);
		}

		myfree(work -> pairs);

		work -> pairs = NULL;
		work -> n = 0;
	}
}

typedef struct
{
	BOOL asc, key;
	int (*value_cmp)(const void *pv1, const void *pv2);
} sort_pars;

sort_pars sp;

int qsort_cmp(const void *pv1, const void *pv2)
{
	key_value_pair *v1 = (key_value_pair *)pv1;
	key_value_pair *v2 = (key_value_pair *)pv2;

	if (sp.key)
	{
		if (sp.asc)
			return strcmp(v1 -> key, v2 -> key);
		else
			return strcmp(v2 -> key, v1 -> key);
	}

	if (sp.asc)
		return sp.value_cmp(v1 -> value, v2 -> value);
	else
		return sp.value_cmp(v2 -> value, v1 -> value);
}

void sort_kv(key_value *work, const BOOL asc, const BOOL key, int (*value_cmp)(const void *pv1, const void *pv2))
{
	sp.asc = asc;
	sp.key = key;
	sp.value_cmp = value_cmp;

	qsort(work -> pairs, work -> n, sizeof(key_value_pair), qsort_cmp);
}
