/* GPLv2 applies
 * SVN revision: $Revision: 671 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
typedef struct {
	char *what;
} tab_completion_t;

char *make_complete_command(const char *in);
char *make_complete_nickorchannel(char *in, int start_of_line);
