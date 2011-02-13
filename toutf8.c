/*
 * $Id: toutf8.c,v 1.1 2004/10/11 00:38:57 jutta Exp $
 *
 * $Log: toutf8.c,v $
 * Revision 1.1  2004/10/11 00:38:57  jutta
 * Initial revision
 *
 *
 */

#include "abnfgenp.h"
#include <ctype.h>

static char const rcsid[] =
	"$Id: toutf8.c,v 1.1 2004/10/11 00:38:57 jutta Exp $";

int to_utf8(unsigned long uc, unsigned char * p)
{
	unsigned char const * p_orig =p;

	if (uc <= 0x7f)
		*p++ = uc;

	else if (uc < (1ul << (5 + 6))) {

		*p++ = 0xC0 | (uc >> 6);
		goto a;
	}
	else if (uc < (1ul << (4 + 6 + 6))) {
		
		*p++ = 0xE0 | (uc >> (6 + 6));
		goto b;
	}
	else if (uc < (1ul << (3 + 6 + 6 + 6))) {

		*p++ = 0xF0 | (uc >> (6 + 6 + 6));
		goto c;
	}
	else if (uc < (1ul << (2 + 6 + 6 + 6 + 6))) {
		*p++ = 0xF8 | (uc >>  (6 + 6 + 6 + 6));
		goto d;
	}
	else {
		*p++ = 0xFC | (uc >>  (6 + 6 + 6 + 6 + 6));
		*p++ = 0x80 | ((uc >> (6 + 6 + 6 + 6)) & 0x3F);
	d:	*p++ = 0x80 | ((uc >>     (6 + 6 + 6)) & 0x3F);
	c:	*p++ = 0x80 | ((uc >>         (6 + 6)) & 0x3F);
	b:	*p++ = 0x80 | ((uc >>              6)  & 0x3F);
	a:	*p++ = 0x80 | ( uc                     & 0x3F);
	}
	return p - p_orig;
}
