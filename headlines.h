/* GPLv2 applies
 * SVN revision: $Revision: 760 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#define MAX_HEADLINES_QUEUED 10
#define NEXT_HEADLINE_INTERVAL 5

#include "gen.h"
#include "string_array.h"

extern string_array_t matchers;

void init_headlines(void);
void free_headlines(void);
void add_headline(BOOL prio, const char *what);
BOOL update_headline(BOOL force);
void add_headline_matcher(const char *str);
BOOL dump_headline_matchers(FILE *fh);
void check_headline_matches(const char *haystack, const char *headline);
void apply_show_headlines(void);
