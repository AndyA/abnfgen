/*
 * $Id: symbol.c,v 1.1 2004/10/11 00:38:57 jutta Exp $
 *
 * $Log: symbol.c,v $
 * Revision 1.1  2004/10/11 00:38:57  jutta
 * Initial revision
 *
 *
 */

#include "abnfgenp.h"
#include <ctype.h>

static char const rcsid[] = 
  "$Id: symbol.c,v 1.1 2004/10/11 00:38:57 jutta Exp $";

static char * dup_lowercase(
	ag_handle	* ag,
	char const 	* text)
{
	char 		* tmp;
	size_t		  len;

	if (!text) return 0;

	len = strlen(text);
	tmp = ag_emalloc(ag, "lower-case version of a string", len + 1);
	if (tmp) {
		char const * r = text;
		char 	   * w = tmp;

		while (*r) {
			*w++ = tolower((unsigned char)*r);
			r++;
		}
		*w = 0;
	}
	return tmp;
}

size_t ag_symbol_size(
	ag_handle 		* ag,
	ag_symbol		  sym)
{
	return hsize(&ag->symbols, char, sym);
}

char const * ag_symbol_text(
	ag_handle 		* ag,
	ag_symbol		  sym)
{
	assert(sym);
	return hmem(&ag->symbols, char, sym);
}

ag_symbol ag_symbol_make(
	ag_handle 		* ag,
	char const		* name)
{
	return hnew(&ag->symbols, char, name, strlen(name) + 1);
}

ag_symbol ag_symbol_make_binary(
	ag_handle 		* ag,
	char const		* s,
	size_t		 	  n)
{
	return hnew(&ag->symbols, char, s, n);
}

ag_symbol ag_symbol_lookup(
	ag_handle		* ag,
	char const		* name)
{
	return haccess(&ag->symbols, char, name, strlen(name) + 1);
}

ag_symbol ag_symbol_make_lowercase(
	ag_handle 		* ag,
	char const		* name)
{
	ag_symbol result;
	char     * tmp = dup_lowercase(ag, name);

	if (!tmp) return 0;
	result = hnew(&ag->symbols, char, tmp, strlen(tmp) + 1);
	VFREE(tmp);

	return result;
}

ag_symbol ag_symbol_lookup_lowercase(
	ag_handle		* ag,
	char const		* name)
{
	ag_symbol result;
	char     * tmp = dup_lowercase(ag, name);

	if (!tmp) return 0;
	result = hnew(&ag->symbols, char, tmp, strlen(tmp) + 1);
	VFREE(tmp);

	return result;
}
