/*
 * $Id: abnfgen.c,v 1.5 2004/10/23 01:21:52 jutta Exp $
 * $Log: abnfgen.c,v $
 * Revision 1.5  2004/10/23 01:21:52  jutta
 * keep track of which nonterminals we've complained about.
 *
 * Revision 1.4  2004/10/23 00:50:32  jutta
 * add verbose expansions
 *
 * Revision 1.3  2004/10/19 04:45:55  jutta
 * reset the default depth to 100.
 *
 * Revision 1.2  2004/10/18 07:38:10  jutta
 * Bump default depth to a 1000.
 *
 * Revision 1.1  2004/10/11 00:36:50  jutta
 * Initial revision
 *
 *
 */

#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>

#include "abnfgenp.h"

static char const rcsid[]
	= "$Id: abnfgen.c,v 1.5 2004/10/23 01:21:52 jutta Exp $";

static char const RFC_4234_CORE[] =
	"ALPHA	=  %x41-5A / %x61-7A   ; A-Z / a-z\n"
	"BIT   	=  \"0\" / \"1\"\n"
	"CHAR  	=  %x01-7F	; any 7-bit US-ASCII character,\n"
	"			;  excluding NUL\n"
	"CR    	=  %x0D		; carriage return\n"
	"CRLF  	=  CR LF	; Internet standard newline\n"
	"CTL   	=  %x00-1F / %x7F\n"
	"			; controls\n"
	"DIGIT 	=  %x30-39	; 0-9\n"
	"DQUOTE	=  %x22		; \" (Double Quote)\n"
	"HEXDIG	=  DIGIT / \"A\" / \"B\" / \"C\" / \"D\" / \"E\" / \"F\"\n"
	"HTAB  	=  %x09		; horizontal tab\n"
	"LF    	=  %x0A		; linefeed\n"
	"LWSP  	=  *(WSP / CRLF WSP)\n"
	"			; linear white space (past newline)\n"
	"OCTET 	=  %x00-FF	; 8 bits of data\n"
	"SP    	=  %x20\n"
	"VCHAR 	=  %x21-7E 	; visible (printing) characters\n"
	"WSP   	=  SP / HTAB	; white space\n";

static void usage(char const * progname)
{
	fprintf(stderr, "%s version %s; send bugs to <jutta@pobox.com>\n"
		"Usage: %s [options][inputs]\n", progname, VERSION, progname);
	fputs(
	"Output:\n"
	"   -d dir	   -- write output files to directory <dir>\n"
	"   -n n	   -- write <n> output files with #=1..n\n"
	"   -p pat#.suf    -- create filenames \"pat#.suf\"\n",
		stderr);
	fputs(
	"Processing:\n"
	"   -c 	           -- attempt full coverage\n"
	"   -l 	           -- (\"legal\") strict RFC 4234 only\n"
	"   -y n	   -- limit recursion depth to <n>\n"
	"   -r n           -- seed random generator with <n>\n"
	"   -s nonterminal -- start symbol is <nonterminal>\n"
	"   -t file        -- include tentative definitions in <file>\n"
	"   -u             -- reject rules that contain <prose>\n",
	   stderr);
	fputs(
	"Miscellaneous:\n"
	"   -x 	           -- exclude (don't preload) the core syntax\n"
	"   -h 	           -- print this statement\n"
	"   -w prefix      -- write seed to stdout, prefixed with <prefix>\n"
	"   -v 	           -- verbose; write rule trace to stderr\n",
		stderr);
	exit(64);
}

void ag_error(ag_handle * ag, char const * fmt, ...)
{
	va_list ap;

	if (ag->errors++ >= 15) {
		fprintf(stderr, "Too many errors; abort.\n");
		exit(0);
	}
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

void * ag_emalloc(
	ag_handle * ag,
	char const * purpose,
	size_t 	     n)
{
	char * tmp = malloc(n);
	if (!tmp) {
		ag_error(ag,
			"failed to allocate %lu bytes for %s\n",
			(unsigned long)n, purpose);
		return 0;
	}
	return tmp;
}

static int open_output_file(
	ag_handle * ag,
	char const * dir,
	char const * pat,
	int 	     i,
	FILE      ** f_out,
	char      ** name_out)
{
	char const * r;
	char *w, * tmp = ag_emalloc(ag, "output file name",
		strlen(dir) + 1 + strlen(pat) + 42);

	if (!tmp) return -1;

	w = tmp;
	if (dir) {
		while (*w = *dir++)
			w++;
		if (w > tmp && w[-1] != '/')
			*w++ = '/';
	}
	for (r = pat; *r;)
		if (*r != '#')
			*w++ = *r++;
		else {
			int n = 0;
			while (*r == '#')
				r++, n++;
			sprintf(w, "%.*u", n, i);
			w += strlen(w);
		}
	*w = 0;

	if (!(*f_out = fopen(tmp, "w"))) {
		ag_error(ag, "%s: failed to open %s for output: %s\n",
			ag->progname, tmp, strerror(errno));
		(void)free(tmp);
		return -1;
	}
	*name_out = tmp;
	return 0;
}

static int ag_process(
	char const  	   * progname,
	int 	      	     verbose,
	int 	      	     understand_prose,
	int 	      	     legal,
	int 	      	     underscore_in_identifiers,
	int 	      	     full_coverage,
	char const  	   * output_directory,
	char const  	   * pattern,
	unsigned int	     seed,
	char const	   * seed_prefix,
	unsigned int	     n_cases,
	unsigned int	     depth,
	char const	   * start_symbol,
	char const * const * av_tentative,
	char const * const * av)
{
	ag_handle    ag[1];
	unsigned int  i;

	memset(ag, 0, sizeof(*ag));
	ag->verbose          = verbose;
	ag->full_coverage    = full_coverage;
	ag->progname         = progname;
	ag->seed             = seed;
	ag->seed_prefix      = seed_prefix;
	ag->understand_prose = !!understand_prose;
	ag->legal	     = !!legal;
	ag->underscore_in_identifiers = !!underscore_in_identifiers;

	hinit(&ag->symbols, 	 char, 1024 * 8);
	hinit(&ag->nonterminals, ag_nonterminal, 512);
	hinit(&ag->complained,   char, 512);

	if (start_symbol)
		ag->start_symbol = ag_symbol_make_lowercase(ag,
			start_symbol);

	if (av_tentative)
		for (i = 0; av_tentative[i]; i++) {

			if (*av_tentative[i] == '\0')
				ag_input_string(ag, RFC_4234_CORE,
				  "ABNF core grammar included in RFC 4234",
				  1);
			else
			{
				FILE * f = fopen(av_tentative[i], "r");
				if (!f) {
					ag_error(ag,
						"%s: can't open %s for input: "
						"%s\n", ag->progname, 
						av_tentative[i],
						strerror(errno));
				}
				else {
					ag_input(ag, f, av_tentative[i], 1);
					fclose(f);
				}
			}
		}

	if (!*av)
		ag_input(ag, stdin, "*standard input*", 0);
	else {
		for (i = 0; av[i]; i++) {
			FILE * f = fopen(av[i], "r");
			if (!f) {
				ag_error(ag, "%s: can't open %s for input: "
					"%s\n", ag->progname,
					av[i], strerror(errno));
			}
			else {
				ag_input(ag, f, av[i], 0);
				fclose(f);
			}
		}
	}

	ag_check(ag);
	if (n_cases && !ag->errors) {

		if (output_directory) {
			
			if (  mkdir(output_directory, 0777) < 0
			   && errno != EEXIST) {

				ag_error(ag,
				 "%s: can't create directory %s: %s\n",
					ag->progname, output_directory,
					strerror(errno));
				return 1;
			}
			for (i = 0; i < n_cases && !ag->errors; i++) {

				char * out_name;
				FILE * out_file;

				if (open_output_file(ag,
					output_directory, pattern, i + 1,
					&out_file, &out_name))

					break;

				ag_output(ag, out_file, out_name, i + 1, depth);
				fclose(out_file);
				(void)free(out_name);
			}
		}
		else
			for (i = 1; i <= n_cases && !ag->errors; i++)  {
				ag_output(ag, stdout, "*standard output*", i,
					depth);
			}
	}
	return ag->errors ? 1 : 0;
}

int main(int ac, char ** av)
{
	unsigned int	  seed = (unsigned int)time(0) * (unsigned int)getpid(),
			  n_cases = 1, verbose = 0, depth = 100;
	char		* seed_prefix = NULL;
	char		* output_directory = 0;
	char const	* pattern = "####.tst";
	char const	* progname;
	int 		  opt, result;
	int		  full_coverage = 0;
	int		  explicit_seed = 0;
	int		  understand_prose = 1;
	int		  legal = 0;
	int		  exclude_core = 0;
	int		  underscore_in_identifiers = 0;
	extern char 	* optarg;
	extern int	  optind;
	char 	       ** av_tentative = 0;
	char		* start_symbol = 0;

	if (progname = strrchr(av[0], '/')) progname++;
	else progname = av[0];

	av_tentative = argvadd(av_tentative, "");

	while ((opt = getopt(ac, av, "cd:hln:p:r:s:t:uvw:x_y:")) != EOF)
		switch (opt) {
		case '_':
			underscore_in_identifiers++;
			break;

		case 'c':
			full_coverage++;
			break;

		case 'd':
			output_directory = optarg;
			break;

		case 'y':
			if (sscanf(optarg, "%u", &depth) != 1) {
				fprintf(stderr, "%s: expected non-negative "
					"integer with -y, got \"%s\"\n",
					progname, optarg);
				usage(progname);
			}
			break;

		case 'n':
			if (sscanf(optarg, "%u", &n_cases) != 1) {
				fprintf(stderr, "%s: expected non-negative "
					"integer with -n, got \"%s\"\n",
					progname, optarg);
				usage(progname);
			}
			break;

		case 'p':
			pattern = optarg;
			break;

		case 'r':
			explicit_seed = 1;
			if (sscanf(optarg, "%u", &seed) != 1) {
				fprintf(stderr, "%s: expected non-negative "
					"integer with -r, got \"%s\"\n",
					progname, optarg);
				usage(progname);
			}
			break;

		case 's':
			start_symbol = optarg;
			break;

		case 't':
			av_tentative = argvadd(av_tentative, optarg);
			break;

		case 'u':
			understand_prose = 0;
			break;

		case 'v':
			verbose++;
			break;

		case 'w':
			explicit_seed = 1;
			seed_prefix = optarg;
			break;

		case 'x':
			exclude_core = 1;
			break;

		case 'l':
			legal = 1;
			break;

		case 'h':
		default:
			usage(progname);
		}

	if (full_coverage && explicit_seed && n_cases > 1) {
		fprintf(stderr, "%s: warning: seeds for other than the first "
			"run of \"%s -c\" won't yield the original result.\n",
			progname, progname);
	}
	result = ag_process(progname, verbose, understand_prose, legal,
		underscore_in_identifiers, full_coverage,
		output_directory, pattern, seed, seed_prefix, n_cases, depth,
		start_symbol,
		(char const * const *)av_tentative + exclude_core,
		(char const * const *)av + optind);
	argvfree(av_tentative);
	return result;
}
