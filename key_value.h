/* GPLv2 applies
 * SVN revision: $Revision: 671 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#ifndef __KEY_VALUE_H__
#define __KEY_VALUE_H__

typedef struct
{
	const char *key;
	const void *value;
} key_value_pair;

typedef struct
{
	key_value_pair *pairs;
	int n;
} key_value;

key_value * allocate_kv(void);
void free_kv(key_value *p);
void add_to_kv(key_value *work, const char *key, const void *value);
int get_n_kv_from_kv(const key_value *in);
const char *get_key_by_index(const key_value *in, int nr);
const void *get_value_by_index(const key_value *in, int nr);
const void * get_from_kv(const key_value *in, const char *key);
BOOL update_kv(key_value *work, const char *key, const void *new_value);
void truncate_kv(key_value *work);
void sort_kv(key_value *work, BOOL asc, BOOL key, int (*value_cmp)(const void *pv1, const void *pv2));

#endif
