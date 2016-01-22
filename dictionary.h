/* GPLv2 applies
 * SVN revision: $Revision: 673 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
extern const char *dictionary_file;
extern string_array_t dictionary;

BOOL load_dictionary(void);
BOOL save_dictionary(void);
const char *lookup_dictionary(const char *what);
void edit_dictionary(void);
