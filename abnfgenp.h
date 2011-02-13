#ifndef	ABNFGENP_H
#define	ABNFGENP_H	/* Guard against multiple includes */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <assert.h>

typedef char * ag_symbol;

typedef struct ag_nonterminal 	ag_nonterminal;
typedef union   ag_expression 	ag_expression;

typedef struct hashtable {
	unsigned long	mask;
	unsigned long	m;
	unsigned long 	n;
	unsigned long   l;
	unsigned long   o;
	void 		** h;
} hashtable;

typedef struct {

	int		  errors;

	char const	* input_string;
	FILE		* input_file;
	char const	* input_name;
	int		  input_line;

	int		  depth;
	char const	* output_name;
	FILE		* output_file;

	char const	* progname;
	int		  verbose;

	hashtable	  complained;
	hashtable	  nonterminals;
	hashtable	  symbols;

	long long	  token_number;
	ag_symbol	  token_symbol;
	unsigned long	  token_range_from,
			  token_range_to;

	ag_symbol	  start_symbol;
	unsigned int	  seed;
	char const	* seed_prefix;

	unsigned int	  full_coverage;

	/*  Information about the white space
	 *  in front of the most recently returned
	 *  token.
	 */
	unsigned int	  token_saw_space:   1;
	unsigned int	  underscore_in_identifiers: 1;
	unsigned int	  understand_prose: 1;
	unsigned int	  legal: 1;

} ag_handle;

typedef enum ag_type {

	AG_TOKEN_ERROR	 = -2,
	AG_TOKEN_EOF,
	AG_TOKEN_NONE	 = 0,
	AG_TOKEN_NUMBER,
	AG_TOKEN_NAME,			/*  2 */
	AG_TOKEN_STRING,		/*  3 */
	AG_TOKEN_OCTETS,		/*  4 */
	AG_TOKEN_RANGE,			/*  5 */
	AG_TOKEN_PROSE,			/*  6 */
	AG_TOKEN_ASTERISK,		/*  7 */
	AG_TOKEN_OR,			/*  8 */
	AG_TOKEN_EQUALS,		/*  9 */
	AG_TOKEN_EQUALSTOO,		/* 10 */
	AG_TOKEN_SQUAREOPEN,		/* 11 */
	AG_TOKEN_SQUARECLOSE,		/* 12 */
	AG_TOKEN_OPEN,			/* 13 */
	AG_TOKEN_CLOSE,			/* 14 */
	AG_TOKEN_LHS,			/* 15 */
	AG_TOKEN_CHANCE,		/* 16 */

	AG_EXPRESSION_ALTERNATION,	/* 17 */
	AG_EXPRESSION_CONCATENATION,	/* 18 */
	AG_EXPRESSION_REPETITION	/* 19 */

} ag_type;

#define	AG_EXPRESSION_IS_COMPOUND(x)			\
	(  (  (x)->type == AG_EXPRESSION_ALTERNATION	\
	   || (x)->type == AG_EXPRESSION_CONCATENATION)	\
	&& (x)->compound.child				\
	&& (x)->compound.child->any.next)

struct ag_nonterminal {
	ag_expression		* expression;
	unsigned int		  tentative: 1;
};

union ag_expression {

	ag_type			  type;

	struct {
		ag_type		  type;
		unsigned long  	  chance;
		int		  distance;
		ag_expression	* next;
		ag_symbol	  input_name;
		int		  input_line;
		unsigned int	  coverage_self:  1;
		unsigned int	  coverage_recursive: 1;
	} any;

	struct {
		ag_type		  type;
		unsigned long  	  chance;
		int		  distance;
		ag_expression	* next;
		ag_symbol	  input_name;
		int		  input_line;
		unsigned int	  coverage_self:  1;
		unsigned int	  coverage_recursive: 1;
		ag_expression	* child;

	} compound;

	struct {
		ag_type		  type;
		unsigned long  	  chance;
		int		  distance;
		ag_expression	* next;
		ag_symbol	  input_name;
		int		  input_line;
		unsigned int	  coverage_self:  1;
		unsigned int	  coverage_recursive: 1;

		ag_expression   * body;
		unsigned long	  min;
		unsigned long	  max;
		unsigned long	  left_chance;
		unsigned long	  right_chance;

		unsigned int	  coverage_left: 1; 
		unsigned int	  coverage_right: 1; 
	} repetition;

	struct {
		ag_type		  type;
		unsigned long  	  chance;
		int		  distance;
		ag_expression	* next;
		ag_symbol	  input_name;
		int		  input_line;
		unsigned int	  coverage_self:  1;
		unsigned int	  coverage_recursive: 1;

		unsigned int	  coverage_upper:  1;
		unsigned int	  coverage_lower:  1;
		ag_symbol	  symbol;
	} token;

	struct {
		ag_type		  type;
		unsigned long  	  chance;
		int		  distance;
		ag_expression	* next;
		ag_symbol	  input_name;
		int		  input_line;
		unsigned int	  coverage_self:  1;
		unsigned int	  coverage_recursive: 1;

		unsigned char	  coverage[256 / 8];

		unsigned long	  from;
		unsigned long	  to;
	} range;

};

/* errors */

#define	AG_ERROR_EOF		-1
#define	AG_ERROR_SYNTAX	-2
#define	AG_ERROR_MEMORY	-3

/* argv.c */

int     argvlen(char const * const * argv);
char ** argvarg(char const * const *, char const *);
int 	argvpos(char const * const * argv, char const * arg);
char ** argvadd(char ** argv, char const * arg);
char ** argvdel(char ** argv, char const * arg);
char ** argvdup(char const * const * argv);
char ** argvcat(char ** argv, char const * const * brgv);
void    argvfree(char ** argv);

/* mem */

void  * malcpy(void const *, size_t);

#define	TREALLOC(t,p,n)		realloc((p), sizeof(t) * n)
#define	TMALLOC(t, n)		TREALLOC(t, 0, n)
#define	TFREE(t, p)		TREALLOC(t, p, 0)
#define	VFREE(p)		((void)realloc(p, 0))

#define	STRMALCPY(s) 		((char *)malcpy(s, strlen(s) + 1))
#define TMEMCPY(T,a,b,n) 	((T *)memcpy( (a), (b), (n) * sizeof(T)))

/* hash.c */

hashtable  * hashcreate(size_t, int);
int 	     hashinit(	 hashtable *, size_t, int);
int 	     hashgrow(	 hashtable *);
void 	   * hash(    	 hashtable *, void const *, size_t, int);
void 	   * hashnext(	 hashtable *, void const *);
void 	     hashfinish( hashtable *);
void	     hashdestroy(hashtable *);
void 	     hashdelete( hashtable *, void *);
void const * hashmem(	 hashtable *, void const *);
size_t 	     hashsize(	 hashtable *, void const *);

#define	htype(ob, T)  		      ((void)(0 && hashcreate(0,(ob) == (T*)0)))
#define	htable(T, page)			hashcreate(sizeof(T), page)
#define	hinit(tab, T, page)		hashinit(tab, sizeof(T), page)
#define	hdestroy(tab, T)		hashdestroy(tab)
#define	hexcl(tab, T, mem, s) 		((T *)hash(tab, mem, s, 2))
#define	hnew(tab, T, mem, s)		((T *)hash(tab, mem, s, 1))
#define	haccess(tab, T, mem, s)		((T *)hash(tab, mem, s, 0))
#define hnext(tab, T, ob)	  	(htype(ob, T), (T *)hashnext(tab, ob))
#define	hmem(tab, T, ob) 	        (htype(ob, T), (T *)hashmem(tab, ob))
#define	hsize(tab, T, ob)		(htype(ob, T), hashsize(  tab, ob))
#define	hdelete(tab, T, ob)		(htype(ob, T), hashdelete(tab, ob))

/* rand */

#ifndef	RAND_MAX 	/* sigh */
#define RAND_MAX 32767
#endif
#define PICK(n) 	((int)((double)rand() / ((double)RAND_MAX + 1) * (n)))

/* string.c */

char const * renderchar(unsigned char);

/* output.c */

void ag_dump_expression(FILE * fp, ag_handle * ag, ag_expression * e);
void ag_output(ag_handle *, FILE *, char const *, int, unsigned int);

/* input.c */

void  ag_input(ag_handle *, FILE *, char const *, int);

/* check.c */

void  ag_check(ag_handle *);

/* ag.c */

void   ag_error(ag_handle *, char const *, ...);
void * ag_emalloc(ag_handle *, char const *, size_t n);

/* symbol.c */

char const * ag_symbol_text(
	ag_handle 		* ag,
	ag_symbol		  name);

size_t ag_symbol_size(
	ag_handle 		* ag,
	ag_symbol		  sym);

ag_symbol ag_symbol_make(
	ag_handle 		* ag,
	char const		* name);

ag_symbol ag_symbol_make_binary(
	ag_handle 		* ag,
	char const		* s,
	size_t		 	  n);

ag_symbol ag_symbol_lookup(
	ag_handle		* ag,
	char const		* name);

ag_symbol ag_symbol_make_lowercase(
	ag_handle 		* ag,
	char const		* name);

ag_symbol ag_symbol_lookup_lowercase(
	ag_handle		* ag,
	char const		* name);

/* nonterminal.c */

ag_nonterminal * ag_nonterminal_lookup_symbol(
	ag_handle 		* ag,
	ag_symbol		  name);

ag_nonterminal * ag_nonterminal_make_symbol(
	ag_handle 		* ag,
	ag_symbol		  name);

char const * ag_nonterminal_name(
	ag_handle		* ag,
	ag_nonterminal const 	* nt);

ag_symbol ag_nonterminal_symbol(
	ag_handle		* ag,
	ag_nonterminal const  	* nt);

/* util.c */

char * ag_dup_lowercase(
	ag_handle	* ag,
	char const 	* text);

/* scan.c */

char const * ag_token_string(ag_handle *, ag_type, char *);
int ag_getc(ag_handle *);
ag_type ag_read_token(ag_handle *);

/* expression.c */

ag_expression * ag_expression_create(ag_handle * ag, int type);
void ag_expression_free(ag_handle * ag, ag_expression ** ex);
int ag_compound_add(
	ag_handle      * ag,
	int		  type,
	ag_expression ** loc,
	ag_expression ** alt);

/* toutf8 */

int to_utf8(unsigned long uc, unsigned char * p);

/* coverage.c */

void ag_check_coverage(ag_handle * ag);

#endif	/* ABNFGENP_H */
