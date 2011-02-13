/*
 * $Id: mem.c,v 1.1 2004/10/11 00:38:57 jutta Exp $
 *
 * $Log: mem.c,v $
 * Revision 1.1  2004/10/11 00:38:57  jutta
 * Initial revision
 *
 *
 */

#include "abnfgenp.h"

static char const rcsid[] = 
  "$Id: mem.c,v 1.1 2004/10/11 00:38:57 jutta Exp $";

void * malcpy(void const * old_p, size_t size)
{
	void * new_p = malloc(size);
	if (new_p) 
		memcpy(new_p, old_p, size);
	return new_p;
}
