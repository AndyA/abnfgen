/*
 * $Id: argv.c,v 1.1 2004/10/11 00:36:50 jutta Exp $
 *
 * $Log: argv.c,v $
 * Revision 1.1  2004/10/11 00:36:50  jutta
 * Initial revision
 *
 *
 */

#include "abnfgenp.h"

static char const rcsid[] = 
  "$Id: argv.c,v 1.1 2004/10/11 00:36:50 jutta Exp $";

/*
 *  argv.c -- manipulating a malloc'ed argv vector.
 */

int argvlen(char const * const * argv)
{
	int i = 0;
	while (*argv++) i++;
	return i;
}

char ** argvarg(char const * const * argv, char const * arg) /* like strchr */
{
	if (!argv) return 0;
	if (!arg) {
		while (*argv) argv++;
		return (char **)argv;
	}
	while (*argv) {
		if (**argv == *arg && !strcmp(*argv, arg))
			return (char **)argv;
		else argv++;
	}
	return (char **)0;
}

int argvpos(char const * const * argv, char const * arg)
{
	char ** a = argvarg( argv, arg );
	if (a) return a - (char **)argv;
	else return -1;
}

char ** argvadd(char ** argv, char const * arg)
{
	if (!argv) {
		argv = TMALLOC(char *, 1 + !!arg);
		if (arg) {
			argv[1] = (char *)0;
			argv[0] = STRMALCPY(arg);
		}  else argv[0] = (char *)0;
	}
	else if (argvarg((char const * const *)argv, arg)) return argv;
	else {
		size_t len  = argvlen((char const * const *)argv );
		argv = TREALLOC( char *, argv, len + 2 );

		argv[ len     ] = STRMALCPY( arg );
		argv[ len + 1 ] = (char *)0;
	}
	return argv;
}

char ** argvdel(char ** argv, char const * arg)
{
	char ** a = argvarg((char const * const *)argv, arg);
	if (!a || !arg) return argv;

	VFREE( *a );

	while (a[0] = a[1]) a++;
	return argv;
}

void argvfree(argv) char ** argv;
{
	char ** a;

	if (argv) {
		for (a = argv; *a; a++) VFREE(*a);
		VFREE(argv);
	}
}

char ** argvdup(char const * const * argv)
{
	if (!argv) return 0;
	else {
		int     n = argvlen(argv);
		char ** a = TMALLOC( char *, n + 1 );

		a[n] = (char *)0;
		while (n--) a[n] = STRMALCPY(argv[n]);
		return a;
	}
}

char ** argvcat(char ** argv, char const * const * brgv)
{
	size_t alen, blen;
	char       ** a;
	char const * const * b;

	alen = argv ? argvlen((char const * const *)argv) : 0;
	blen = brgv ? argvlen((char const * const *)brgv) : 0;

	if (!blen) return argv;
	if (!alen) {
		if (argv) VFREE( argv );
		return argvdup( brgv );
	}

	argv = TREALLOC( char *, argv, alen + blen + 1);
	for (a = argv + alen, b = brgv; *b; b++)
		if (!argvarg((char const * const *)argv, *b))
			*a++ = STRMALCPY( *b );
		
	*a = (char *)0;
	return argv;
}
