/* GPLv2 applies
 * SVN revision: $Revision: 671 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#ifndef __IGNORES_H__
#define __IGNORES_H__

typedef struct
{
	const char *nick, *name_complete;
	const char *channel;
} ignore;

extern ignore *ignores;
extern char *ignore_file;
extern int n_ignores;

#define IGNORE_NOT_SET	"NOT sEt bla invalid123"

void add_ignore(const char *channel, const char *nick, const char *name_complete);
void del_ignore(const char *channel, const char *nick);
BOOL load_ignore_list(const char *file);
void free_ignores(void);
BOOL save_ignore_list(void);
BOOL check_ignore(const char *channel, const char *nick, const char *name_complete);

#endif
