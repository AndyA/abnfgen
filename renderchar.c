/*
 * $Id: renderchar.c,v 1.1 2004/10/11 00:38:57 jutta Exp $
 *
 * $Log: renderchar.c,v $
 * Revision 1.1  2004/10/11 00:38:57  jutta
 * Initial revision
 *
 * Revision 1.2  2004/10/10 18:31:26  jutta
 * Add header
 *
 */

#include "abnfgenp.h"

static char const rcsid[] = 
  "$Id: renderchar.c,v 1.1 2004/10/11 00:38:57 jutta Exp $";

char const * renderchar(unsigned char c)
{
	static char	buf[5];
	char		* s = buf;

	for (;;) {
		if (c < 32) {
			*s++ = '^';
			*s++ = '@' + c;
			*s++ = '\0';
			return buf;
		}
		if (c < 0177) {
			*s++ = c;
			*s++ = '\0';
			return buf;
		}
		if (c == 0177) {
			*s++ = '^';
			*s++ = '?';
			*s++ = '\0';
			return buf;
		}
		c -= 128;
		*s++ = 'M';
		*s++ = '-';
	}
}

