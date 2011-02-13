/*
 * $Id: output.c,v 1.2 2004/10/23 00:50:32 jutta Exp $
 *
 * $Log: output.c,v $
 * Revision 1.2  2004/10/23 00:50:32  jutta
 * add verbose expansions
 *
 * Revision 1.1  2004/10/11 00:38:57  jutta
 * Initial revision
 *
 *
 */

#include "abnfgenp.h"

#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>

static char const rcsid[] = 
  "$Id: output.c,v 1.2 2004/10/23 00:50:32 jutta Exp $";

static void ag_output_expression(ag_handle *, ag_expression *, int, int);

void ag_dump_expression(
	FILE 		* fp,
	ag_handle 	* ag,
	ag_expression 	* e)
{
	char const 	* s;
	size_t 		  n;

	if (  e->any.chance
	   && e->any.chance != 1)
	   	printf("{%lu} ", e->any.chance);

	switch (e->type) {
	default:
		fprintf(stderr,
			"*** unexpected expression type %d\n", e->type);
		break;

	case AG_TOKEN_NAME:
		fputs(ag_symbol_text(ag, e->token.symbol), fp);
		break;

	case AG_TOKEN_STRING:
		s = ag_symbol_text(ag, e->token.symbol);
		n = ag_symbol_size(ag, e->token.symbol);
		n--;

		putc('"', fp);
		while (n--) fputs(renderchar(*s++), fp);
		putc('"', fp);
		break;

	case AG_TOKEN_PROSE:
		fprintf(fp, "<%s>", ag_symbol_text(ag, e->token.symbol));
		break;

	case AG_TOKEN_OCTETS:
		s = ag_symbol_text(ag, e->token.symbol);
		n = ag_symbol_size(ag, e->token.symbol);

		putc('\'', fp);
		while (n--) fputs(renderchar(*s++), fp);
		putc('\'', fp);
		break;

	case AG_TOKEN_RANGE:
		n = e->range.from;
		fputc('[', fp);
		while (n <= e->range.to) {
			fputs(renderchar(n), fp);
			n++;
		}
		fputc(']', fp);
		break;

	case AG_EXPRESSION_ALTERNATION:
		if (e->compound.child && !e->compound.child->any.next)
			ag_dump_expression(fp, ag, e->compound.child);
		else
		{
			for (e = e->compound.child; e; e = e->any.next) {
				
				if (AG_EXPRESSION_IS_COMPOUND(e))
					fputc('(', fp);
				ag_dump_expression(fp, ag, e);
				if (AG_EXPRESSION_IS_COMPOUND(e))
					fputc(')', fp);
				if (e->any.next) 
					fputs(" / ", fp);
			}
		}
		break;

	case AG_EXPRESSION_CONCATENATION:
		if (e->compound.child && !e->compound.child->any.next)
			ag_dump_expression(fp, ag, e->compound.child);
		else
		{
			for (e = e->compound.child; e; e = e->any.next) {

				if (e->type == AG_EXPRESSION_ALTERNATION)
					fputc('(', fp);
				ag_dump_expression(fp, ag, e);
				if (e->type == AG_EXPRESSION_ALTERNATION)
					fputc(')', fp);

				if (e->any.next) putc(' ', fp);
			}
		}
		break;

	case AG_EXPRESSION_REPETITION:
		if (e->repetition.min == e->repetition.max)
			fprintf(fp, "%lu", e->repetition.min);
		else
		{
			if (e->repetition.min != 0)
				fprintf(fp, "%lu", e->repetition.min);
			putc('*', fp);
			if (e->repetition.max != (unsigned long)-1)
				fprintf(fp, "%lu", e->repetition.max);
		}
		if (e->repetition.body)
		{
			if (AG_EXPRESSION_IS_COMPOUND(e->repetition.body))
				fputc('(', fp);
			ag_dump_expression(fp, ag, e->repetition.body);
			if (AG_EXPRESSION_IS_COMPOUND(e->repetition.body))
				fputc(')', fp);
		}
		break;
	}
}

static void ag_output_alternation(
	ag_handle * ag, ag_expression * e, int indent, int depth)
{
	ag_expression * c;
	ag_expression * pick = 0;
	unsigned int 	n;

	e->any.coverage_self = 1;

	if (depth <= 0) {

		/*  Find a branch with the shortest distance to termination.
 		 */
		n = UINT_MAX;
		for (c = e->compound.child; c; c = c->any.next)
			if (  c->any.distance >= 0 
			   && c->any.distance < n) {
			   	n    = c->any.distance;
				pick = c;
			}
	}
	else {
		if (ag->full_coverage && !e->any.coverage_recursive) {

			/* If you can, pick something that doesn't have
			 * coverage_self.
			 */
			n = 0;
			pick = 0;
			for (c = e->compound.child; c; c = c->any.next) {
				if (c->any.coverage_self)
					continue;
				n += c->any.chance;
				if (!pick || PICK(n) < c->any.chance) {

					pick = c;
				}
			}

			if (!pick) {

				/* If everybody has coverage_self,
				 * pick something that doesn't have
				 * recursive coverage yet.
				 */
				n    = 0;
				for (c = e->compound.child;
				     c; c = c->any.next) {
					if (c->any.coverage_recursive)
						continue;
					n += c->any.chance;
					if (!pick || PICK(n) < c->any.chance)
						pick = c;
				}
			}
		}
	}
	if (!pick) {
		n = 0;
		for (c = e->compound.child; c; c = c->any.next) {
			n += c->any.chance;

			if (!pick || PICK(n) < c->any.chance)
				pick = c;
		}
	}
	if (pick)
	{
		if (ag->verbose)
		{
			fprintf(stderr, "%*.*s", indent, indent, "");
			ag_dump_expression(stderr, ag, e);
			fputs(" -> ", stderr);
			ag_dump_expression(stderr, ag, pick);
			putc('\n', stderr);
		}
		ag_output_expression(ag, pick, indent + 1, depth - 1);
	}
}

static void ag_output_repetition(
	ag_handle     * ag,
	ag_expression * e,
	int		indent,
	int 		depth)
{
	unsigned long 	l;
	int		entry_depth = depth;

	if (ag->full_coverage && !e->any.coverage_self) {

		unsigned long n;

		if (  !e->repetition.coverage_right
		   && !e->repetition.coverage_left)
			n = PICK(2) ? e->repetition.max : e->repetition.min;

		else if (e->repetition.coverage_right)
			n = e->repetition.min;

		else 	n = e->repetition.max;
	
		if (n == e->repetition.min)
			e->repetition.coverage_left = 1;

		if (n == e->repetition.max)
			e->repetition.coverage_right = 1;
		
		if (  e->repetition.coverage_left
		   && e->repetition.coverage_right) {

			e->repetition.coverage_self = 1;
		}
		if (  n == e->repetition.max
		   && ag->full_coverage < 2
		   && e->repetition.max - e->repetition.min > 100)

			n = e->repetition.min + 1 + PICK(20);

		for (l = 0;
		     l < n && (l < e->repetition.min || depth >= 0);
		     l++) {
			depth--;

			if (ag->verbose)
			{
				fprintf(stderr, "%*.*s", indent, indent, "");
				ag_dump_expression(stderr, ag, e);
				fputs(" -> ", stderr);
				ag_dump_expression(stderr, ag,
					e->repetition.body);
				fprintf(stderr, " ; #%lu\n", l + 1);
			}

			ag_output_expression(ag, e->repetition.body,
				indent + 1, depth);
		}
	}
	else {
		for (l = 0; l < e->repetition.max && !ag->errors; l++)
		{
			if (  l >= e->repetition.min
			   && (  depth <= 0
			      || PICK( e->repetition.left_chance 
				     + e->repetition.right_chance)
				< e->repetition.left_chance))

				break;

			if (ag->verbose)
			{
				fprintf(stderr, "%*.*s", indent, indent, "");
				ag_dump_expression(stderr, ag, e);
				fputs(" -> ", stderr);
				ag_dump_expression(stderr, ag,
					e->repetition.body);
				fprintf(stderr, " ; #%lu\n", l + 1);
			}

			depth--;
			ag_output_expression(ag,
				e->repetition.body, indent + 1, depth);
		}
	}

	if (l == 0 && ag->verbose)
	{
		fprintf(stderr, "%*.*s", indent, indent, "");
		ag_dump_expression(stderr, ag, e);
		fputs(" -> # zero repetitions\n", stderr);
	}
}

static unsigned long ag_pick_range_coverage(ag_handle * ag, ag_expression * e)
{
	unsigned long n, mult, c, pick_c;
	int	      nopt = 0;

	n 	= 0;
	pick_c  = e->range.from;

	for (pick_c = c = e->range.from; c <= e->range.to;) {

		if (! (  e->range.coverage[(c % 0xFF) >> 3]
		      &  (1 << (c & 7)))) {

			mult = 1 + (e->range.to - c) / 256;
			n   += mult;

		      	if (!nopt++ || PICK(n) < mult) {
				pick_c = c;
			}
		}
		c++;
		if (c % 0xFF == e->range.from % 0xFF)
			break;
	}
	e->range.coverage[(pick_c % 0xFF) >> 3] |= 1 << (pick_c & 7);
	if (nopt <= 1) {
		e->range.coverage_self 	    = 1;
		e->range.coverage_recursive = 1;
	}
	return pick_c;
}

static void ag_output_expression(
	ag_handle     * ag,
	ag_expression * e,
	int		indent,
	int		depth)
{
	int 	 	i, c;
	char const    * s;
	size_t       	n;
	char 		buf[20];
	ag_nonterminal const * nt;

	assert(e);
	switch (e->type) {
	default:
		fprintf(stderr, "ag_output_expression: unexpected "
			"expression type %d\n", e->type);
		return;

	case AG_TOKEN_NAME:

		e->any.coverage_self = 1;
		nt = ag_nonterminal_lookup_symbol(ag, e->token.symbol);
		if (!nt) {
			ag_error(ag, "%s: no definition for nonterminal "
				"<%s> used in %s:%d\n",
				ag->progname,
				ag_symbol_text(ag, e->token.symbol),
				ag_symbol_text(ag, e->any.input_name),
				e->any.input_line);
			return;
		}

		if (ag->verbose)
		{
			fprintf(stderr, "%*.*s%s -> ", 
				indent, indent, "",
				ag_symbol_text(ag, e->token.symbol));
			ag_dump_expression(stderr, ag, nt->expression);
			fprintf(stderr, "  ; %s:%d\n", 
				ag_symbol_text(ag,
					nt->expression->any.input_name),
				nt->expression->any.input_line);
		}
		ag_output_expression(ag, nt->expression, indent + 1, depth - 1);
		break;

	case AG_TOKEN_STRING:
		s = ag_symbol_text(ag, e->token.symbol);
		n = ag_symbol_size(ag, e->token.symbol);

		if (ag->full_coverage && !e->any.coverage_self) {

			int choices =  !e->token.coverage_lower
				     + !e->token.coverage_upper;

			choices = PICK(choices);
			if (!choices && !e->token.coverage_lower) {
				e->token.coverage_lower = 1;
				for (i = 0; i < n - 1; i++) {
					if (putc(tolower((unsigned char)s[i]),
						ag->output_file) == EOF) {
						ag_error(ag, "%s: error "
							"writing to %s: %s\n", 
							ag->progname,
							ag->output_name,
							strerror(errno));
						break;
					}
				}
			} else {
				e->token.coverage_upper = 1;
				for (i = 0; i < n - 1; i++) {
					if (putc(toupper((unsigned char)s[i]),
						ag->output_file) == EOF) {
						ag_error(ag, "%s: error "
							"writing to %s: %s\n", 
							ag->progname,
							ag->output_name,
							strerror(errno));
						break;
					}
				}
			}
			for (i = 0; i < n - 1; i++)
				if (isalpha((unsigned char)s[i]))
					break;

			/* If there were no alpha characters in the
			 * printed string, consider both lower and
			 * upper case covered by printing it, whatever
			 * side we chose up there.
			 */
			if (i >= n - 1)
				e->token.coverage_lower =
				e->token.coverage_upper = 1;

			if (  e->token.coverage_upper
			   && e->token.coverage_lower)
				e->token.coverage_self = 1;

			if (e->token.coverage_self)
				e->token.coverage_recursive = 1;
		}
		else {
			for (i = 0; i < n - 1; i++) {
				if (putc((PICK(2)
					? tolower((unsigned char)s[i])
				 	: toupper((unsigned char)s[i])),
					ag->output_file) == EOF) {

					ag_error(ag, "%s: error "
						"writing to %s: %s\n", 
						ag->progname,
						ag->output_name,
						strerror(errno));
					break;
				}
			}
		}
		break;

	case AG_TOKEN_OCTETS:
		if (ag->full_coverage) {
			e->any.coverage_self = 1;
		}
		s = ag_symbol_text(ag, e->token.symbol);
		n = ag_symbol_size(ag, e->token.symbol);
		for (i = 0; i < n; i++)
			if (putc(s[i], ag->output_file) == EOF) {
				ag_error(ag, "%s: error writing %s: %s\n", 
					ag->progname, 
					ag->output_name, strerror(errno));
				break;
			}
		break;

	case AG_TOKEN_RANGE:
		if (ag->full_coverage && !e->any.coverage_self) {
			c = ag_pick_range_coverage(ag, e);
		}
		else {
			c = e->range.from
				+ PICK((e->range.to - e->range.from) + 1);
		}

		if (ag->legal)
		{
			n = 1;
			*buf = c;
		}
		else
			n = to_utf8(c, (unsigned char *)buf);

		for (i = 0; i < n; i++)
			if (putc(buf[i], ag->output_file) == EOF) {
				ag_error(ag, "%s: error writing %s: %s\n", 
					ag->progname,
					ag->output_name, strerror(errno));
				break;
			}
		break;

	case AG_TOKEN_PROSE:
		if (ag->understand_prose)
			fprintf(ag->output_file, "<%s>",
				ag_symbol_text(ag, e->token.symbol));
		else
			ag_error(ag,
				"%s: can't expand prose <%s> (from %s:%d)\n",
				ag->progname,
				ag_symbol_text(ag, e->token.symbol),
				ag_symbol_text(ag, e->any.input_name),
				e->any.input_line);
		break;

	case AG_EXPRESSION_ALTERNATION:
		ag_output_alternation(ag, e, indent, depth);
		break;

	case AG_EXPRESSION_CONCATENATION:
		e->any.coverage_self = 1;
		for (e = e->compound.child; e; e = e->any.next)
			ag_output_expression(ag, e, indent, depth);
		break;

	case AG_EXPRESSION_REPETITION:
		ag_output_repetition(ag, e, indent, depth);
		break;
	}
}

static void ag_output_rulelist(ag_handle * ag, int depth)
{
	ag_nonterminal const * nt = 0;

	if (!ag->start_symbol) {
		ag_error(ag, "%s: nothing to do!\n", ag->progname);
		return;
	}

	nt = ag_nonterminal_lookup_symbol(ag, ag->start_symbol);
	if (!nt) {
		ag_error(ag, "%s: no definition for start-symbol <%s>\n",
			ag->progname,
			ag_symbol_text(ag, ag->start_symbol));
		return;
	}
	if (!nt->expression) {
		ag_error(ag, "%s: no expansion for start-symbol <%s>\n",
			ag->progname, ag_symbol_text(ag, ag->start_symbol));
		return;
	}

	if (ag->verbose)
	{
		fprintf(stderr, "%s -> ", ag_symbol_text(ag, ag->start_symbol));
		ag_dump_expression(stderr, ag, nt->expression);
		putc('\n', stderr);
	}
	ag_output_expression(ag, nt->expression, 1, depth - 1);
}

void ag_output(
	ag_handle	* ag,
	FILE 		* fp,
	char const 	* filename,
	int		  i,
	unsigned int	  depth)
{
	unsigned int	  seed;

	ag->output_file = fp;
	ag->output_name = filename;
	ag->depth	= depth;

	seed = ag->seed;
	seed %= RAND_MAX;
	srand(seed);

	if (ag->seed_prefix) printf("%s%u\n", ag->seed_prefix, seed);

	ag_output_rulelist(ag, depth);
	ag->seed = rand();

	if (ag->full_coverage)
		ag_check_coverage(ag);
	
}
