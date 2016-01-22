/* GPLv2 applies
 * SVN revision: $Revision: 671 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
void free_colors(void);
int get_color_mirc(int mfg, int mbg);
int find_alloc_colorpair(int fg, int bg);
int get_color_ncurses(int ncol_fg, int ncol_bg);
int color_str_convert(const char *in);
char *color_to_str(int nr);
void emit_colorpair(FILE *fh, short pair);
int get_n_cpairs(void);
