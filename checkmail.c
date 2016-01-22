/* GPLv2 applies
 * SVN revision: $Revision: 671 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "gen.h"
#include "config.h"

char *mail_spool_file = NULL;
struct stat msf_info;
off_t msf_prev_size = 0;
time_t msf_last_check = 0;

void init_check_mail(void)
{
	mail_spool_file = getenv("MAIL");

        if (check_for_mail > 0 && mail_spool_file != NULL)
        {
                if (stat(mail_spool_file, &msf_info) == -1)
                        check_for_mail = 0;
                else
                {
                        msf_prev_size = msf_info.st_size;

                        msf_last_check = time(NULL);
                }
        }
}
