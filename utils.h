/* GPLv2 applies
 * SVN revision: $Revision: 846 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#ifndef __POLL_H__
#define __POLL_H__

#include <poll.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "string_array.h"

void LOG(char *str, ...);
int myrand(int max);
/*void myfree(const void *p); */
#define myfree(p) free((void *)p);
int resize(void **pnt, int n, int *len, int size);
ssize_t READ(int fd, char *whereto, size_t len);
ssize_t WRITE(int fd, const char *whereto, size_t len);
char * read_line_fd(int fd);
const char * str_or_nothing(const char *string);
const char * skip_colon(const char *in);
char * replace_string(const char *in, int pos_start, int pos_end, const char *with_what);
unsigned long int myatoul(const char *str);
double gettime(void);
int count_char(const char *str, char what);
double get_ts(void);
int hextoint(const char *in, int n);
int add_poll(struct pollfd **pfd, int *n_fd, int fd, int events);
int mkpath(const char* file_path_in, mode_t mode);
int strpos(const char *str, char what);
const char *explode_path(const char *in);
void terminate_str(char *str, char what);
void terminate_str_r(char *str, char what);
void run(const char *what, const char *pars[]);
BOOL file_exists(const char *p);

#endif
