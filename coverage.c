/*
 * $Id: coverage.c,v 1.1 2004/10/11 00:38:57 jutta Exp $
 *
 * $Log: coverage.c,v $
 * Revision 1.1  2004/10/11 00:38:57  jutta
 * Initial revision
 *
 *
 */

#include "abnfgenp.h"

static char const rcsid[] = 
  "$Id: coverage.c,v 1.1 2004/10/11 00:38:57 jutta Exp $";

/* Return 1 if this expression is recursively covered, given that the
 * pointing-to expressions are recursively covered, 0 if it isn't.
 */
static int ag_is_recursively_covered(ag_handle * ag, ag_expression * e)
{
	ag_expression  * child;
	ag_nonterminal * nt;
	int 		 res = 1;

	if (e->any.coverage_recursive) {
		return 1;
	}
	if (!e->any.coverage_self) {
		return 0;
	}

	e->any.coverage_recursive = 1;

	switch (e->type) {
	default:
		ag_error(ag, "ag_check_expression_coverage: unexpected "
			"expression type %d\n", e->type);
		return 0;

	case AG_TOKEN_NAME:
		if (!(nt = ag_nonterminal_lookup_symbol(ag, e->token.symbol))) {
			ag_error(ag,
				"%s: no definition for nonterminal <%s>\n",
				ag->progname,
				ag_symbol_text(ag, e->token.symbol));

			return 0;
		}
		res = ag_is_recursively_covered(ag, nt->expression);
		break;
	
	case AG_TOKEN_STRING:
	case AG_TOKEN_OCTETS:
	case AG_TOKEN_RANGE:
	case AG_TOKEN_PROSE:
		break;

	case AG_EXPRESSION_ALTERNATION:
	case AG_EXPRESSION_CONCATENATION:
		for (child = e->compound.child; child; child = child->any.next)
			res &= ag_is_recursively_covered(ag, child);
		break;

	case AG_EXPRESSION_REPETITION:
		res = ag_is_recursively_covered(ag, e->repetition.body);
		break;
	}
	e->any.coverage_recursive = res;
	return res;
}


/* Return 1 if anything changed, 0 if not.
 */
static int ag_check_expression_coverage(ag_handle * ag, ag_expression * e)
{
	if (   e->any.coverage_recursive
	   || !e->any.coverage_self)
	   	return 0;

	return ag_is_recursively_covered(ag, e);
}

void ag_check_coverage(ag_handle * ag)
{
	ag_nonterminal * nt = 0;
	int change, all;

	do {
		change = 0;
		all    = 1;
		nt     = 0;

		while (nt = hnext(&ag->nonterminals, ag_nonterminal, nt)) {
			change |= ag_check_expression_coverage(ag,
				nt->expression);
			all &= nt->expression->any.coverage_recursive;
		}

	} while (change);
	
	if (all)
		ag->full_coverage = 0;
}
