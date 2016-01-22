/* GPLv2 applies
 * SVN revision: $Revision: 762 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#include "gen.h"
#include "term.h"

void search_everywhere(void);
void write_buffer_to_file(const buffer *pb, BOOL with_channel);
void global_search(const char *highlight, const char *title_in);
void scrollback_new(const buffer *pbuffer, const char *highlight, const char *title_in, BOOL with_channel, BOOL force_partial_highlight);
void scrollback(NEWWIN *win, const buffer *pbuffer, const char *highlight, BOOL with_channel, BOOL force_partial_highlight);
BOOL scrollback_and_select(NEWWIN *win, const buffer *pbuffer, char **value, int *sr, int *ch, const char *needle, BOOL with_channel, BOOL force_partial_highlight);
void show_channel_history(void);
void select_own_history_line(BOOL search_in_all);
