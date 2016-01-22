/* GPLv2 applies
 * SVN revision: $Revision: 886 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <regex.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <wordexp.h>
#include <ncursesw/ncurses.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include "gen.h"
#include "error.h"
#include "utils.h"

int myrand(int max)
{
	return (int)(((double)max * (double)rand()) / (double)RAND_MAX);
}

#if 0
void myfree(const void *p)
{
	free((void *)p);
}
#endif

int resize(void **pnt, int n, int *len, int size)
{
        if (n == *len)
        {
                *len = (*len) ? (*len) * 2 : 4096;
                *pnt = realloc(*pnt, (*len) * size);
        }
	else if (n > *len || n<0 || *len<0)
		error_exit(TRUE, "resize: fatal memory corruption problem: n > len || n<0 || len<0!\n");

	return 0;
}

ssize_t READ(int fd, char *whereto, size_t len)
{
	ssize_t cnt=0;

	while(len > 0)
	{
		ssize_t rc;

		rc = read(fd, whereto, len);

		if (rc == -1)
		{
			if (errno == EINTR || errno == EAGAIN)
				continue;

			return -1;
		}
		else if (rc == 0)
			break;
		else
		{
			whereto += rc;
			len -= rc;
			cnt += rc;
		}
	}

	return cnt;
}

ssize_t WRITE(int fd, const char *whereto, size_t len)
{
	ssize_t cnt=0;

	while(len > 0)
	{
		ssize_t rc;

		rc = write(fd, whereto, len);

		if (rc == -1)
		{
			if (errno == EAGAIN || errno == EINTR)
				continue;

			return -1;
		}
		else if (rc == 0)
			return 0;
		else
		{
			whereto += rc;
			len -= rc;
			cnt += rc;
		}
	}

	return cnt;
}

char * read_line_fd(int fd)
{
	char *str = malloc(80);
	int n_in=0, size=80;

	for(;;)
	{
		char c;

		/* read one character */
		if (READ(fd, &c, 1) != 1)
		{
			if (n_in == 0)
			{
				myfree(str);
				str = NULL;
			}

			return str;
		}

		/* resize input-buffer */
		resize((void **)&str, n_in, &size, sizeof(char));

		/* EOF or \n == end of line */
		if (c == 10 || c == EOF)
			break;

		/* ignore CR */
		if (c == 13)
			continue;

		/* add to string */
		str[n_in++] = c;
	}

	/* terminate string */
	resize((void **)&str, n_in, &size, sizeof(char));
	str[n_in] = 0x00;

	return str;
}

const char * str_or_nothing(const char *string)
{
	if (string)
		return string;

	return "";
}

const char * skip_colon(const char *in)
{
	if (in[0] == ':')
		return in + 1;

	return in;
}

char * replace_string(const char *in, int pos_start, int pos_end, const char *with_what)
{
	int str_len_in = strlen(in);
	int str_len_ww = strlen(with_what);
	int n_remove = (pos_end - pos_start) + 1; /* +1 => including pos_end! */
	int new_len = str_len_in + str_len_ww - n_remove;
	char *out = malloc(new_len + 1); /* +1 => 0x00 */

	memmove(out, in, pos_start);
	memmove(&out[pos_start], with_what, str_len_ww);
	memmove(&out[pos_start + str_len_ww], &in[pos_end + 1], str_len_in - (pos_end + 1));
	out[new_len] = 0x00;

	return out;
}

unsigned long int myatoul(const char *str)
{
	unsigned long int dummy = 0;

	while(isspace(*str)) str++;

	while(*str >= '0' && *str <= '9')
	{
		dummy *= 10;
		dummy += (*str) - '0';
		str++;
	}

	return dummy;
}

int count_char(const char *str, char what)
{
	int len = strlen(str);
	int loop = 0, cnt = 0;

	for(loop=0; loop<len; loop++)
		cnt += str[loop] == what ? 1 :0;

	return cnt;
}

double get_ts(void)
{
        struct timeval ts;

        if (gettimeofday(&ts, NULL) == -1)
	{
		/* I would be surprised if gettimeofday fails */
                return time(NULL);
	}

        return (((double)ts.tv_sec) + ((double)ts.tv_usec)/1000000.0);
}

//#ifdef _DEBUG
#if 0
#define LETTER "a"
#define LF "log" LETTER ".txt"
void LOG(char *s, ...)
{
	int e = errno;
	struct stat buf;
	time_t	now = time(NULL);
	char *tstr = ctime(&now);
        va_list ap;
        FILE *fh = fopen(LF, "a+");
        if (!fh)
        {
                endwin();
                printf("error\n");
        }
	else
	{
		terminate_str(tstr, '\n');

		fprintf(fh, "%s ", tstr);

		va_start(ap, s);
		vfprintf(fh, s, ap);
		va_end(ap);

		fclose(fh);

		if (stat(LF, &buf) != -1 && buf.st_size > (4 * 1024 * 1024))	/* > 4MB? */
		{
			unlink(LF ".9");
			rename(LF ".8", LF ".9");
			rename(LF ".7", LF ".8");
			rename(LF ".6", LF ".7");
			rename(LF ".5", LF ".6");
			rename(LF ".4", LF ".5");
			rename(LF ".3", LF ".4");
			rename(LF ".2", LF ".3");
			rename(LF ".1", LF ".2");
			rename(LF ".0", LF ".1");
			rename(LF, LF ".0");
		}
	}

	errno = e;
}
#else
void LOG(char *s, ...) { }
#endif

int hextoint(const char *in, int n)
{
	int value = 0, index = 0;

	for(index=0; index<n; index++)
	{
		int c = toupper(in[index]);

		value <<= 4;

		if (c >= '0' && c <= '9')
			value += c - '0';
		else
			value += c - 'A' + 10;
	}

	return value;
}

int add_poll(struct pollfd **pfd, int *n_fd, int fd, int events)
{
	int out = -1;

	*pfd = (struct pollfd *)realloc(*pfd, (*n_fd + 1) * sizeof(struct pollfd));

	memset(&(*pfd)[*n_fd], 0x00, sizeof(struct pollfd));
	(*pfd)[*n_fd].fd = fd;

	out = *n_fd;

	(*pfd)[*n_fd].events = events;

	(*n_fd)++;

	return out;
}

int mkpath(const char* file_path_in, mode_t mode)
{
	char *file_path = strdup(file_path_in);
	char *p = NULL;

	for(p=strchr(file_path + 1, '/'); p; p=strchr(p + 1, '/'))
	{
		*p = '\0';

		if (mkdir(file_path, mode) == -1)
		{
			if (errno != EEXIST)
			{
				free(file_path);
				return -1;
			}
		}

		*p = '/';
	}

	free(file_path);

	return 0;
}

int strpos(const char *str, char what)
{
	const char *p = strchr(str, what);

	if (!p)
		return -1;

	return (int)(p - str);
}

const char *explode_path(const char *in)
{
	const char *result = NULL;
	wordexp_t p;

	if (wordexp(in, &p, 0) != 0)
		return NULL;

	if (p.we_wordv[0])
		result = strdup(p.we_wordv[0]);

	wordfree(&p);

	return result;
}

void terminate_str(char *str, char what)
{
	char *dummy = strchr(str, what);

	if (dummy)
		*dummy = 0x00;
}

void terminate_str_r(char *str, char what)
{
	char *dummy = strrchr(str, what);

	if (dummy)
		*dummy = 0x00;
}

void run(const char *what, const char *pars[])
{
	pid_t pid = fork();

	if (pid == 0)
	{
		int fd = open("/dev/null", O_RDWR);

		close(0);
		dup(fd);

		close(1);
		dup(fd);

		close(2);
		dup(fd);

		if (pars)
			pars[0] = what;

		execv(what, (char * const *)pars);

		exit(0);
	}
}

BOOL file_exists(const char *p)
{
	struct stat st;

	if (stat(p, &st) == 0)
		return TRUE;

	return FALSE;
}
