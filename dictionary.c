/* GPLv2 applies
 * SVN revision: $Revision: 696 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "string_array.h"
#include "utils.h"

const char *dictionary_file = NULL;
string_array_t dictionary;

BOOL load_dictionary(void)
{
	char buffer[4096] = { 0 };
	FILE *fh = fopen(dictionary_file, "r");
	if (!fh)
		return FALSE;

	free_string_array(&dictionary);

	while(!feof(fh))
	{
		if (fgets(buffer, sizeof buffer, fh) == NULL)
			break;

		if (strlen(buffer) == 0)
			continue;

		terminate_str(buffer, '\r');
		terminate_str(buffer, '\n');

		add_to_string_array(&dictionary, buffer);
	}

	fclose(fh);

	sort_string_array(&dictionary);

	return TRUE;
}

BOOL save_dictionary(void)
{
	int n = string_array_get_n(&dictionary), idx = 0, rc = 0;
	FILE *fh = fopen(dictionary_file, "w");
	if (!fh)
		return FALSE;

	for(idx=0; idx<n; idx++)
	{
		if (fprintf(fh, "%s\n", string_array_get(&dictionary, idx)) < 0)
			rc = -1;
	}

	fclose(fh);

	return rc == 0;
}

const char *lookup_dictionary(const char *what)
{
	const char *match = NULL, *field2 = NULL;
	int i = partial_match_search_string_array(&dictionary, what);

	if (i == -1)
		return NULL;

	match = string_array_get(&dictionary, i);

	field2 = strchr(match, ' ');

	if (field2)
	{
		while(isspace(*field2))
			field2++;

		return field2;
	}

	return match;
}
