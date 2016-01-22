/* GPLv2 applies
 * SVN revision: $Revision: 671 $
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#ifndef __GEN_H_
#define __GEN_H_

#define BOOL	char

#ifndef TRUE
	#define TRUE 1

	#ifndef FALSE
		#define FALSE 0
	#endif
#endif

#define min(x, y) ((x)<(y)?(x):(y))
#define max(x, y) ((x)>(y)?(x):(y))

#define DEFAULT_IRC_PORT	6667

/* length of the edit line */
#define LINE_LENGTH	460

#define USER_COLUMN_WIDTH 8

typedef enum { CM_EDIT, CM_CHANNELS, CM_NAMES, CM_WC } cursor_mode_t;

#endif
