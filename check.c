/*
 * $Id: check.c,v 1.5 2004/10/23 01:21:52 jutta Exp $
 *
 * $Log: check.c,v $
 * Revision 1.5  2004/10/23 01:21:52  jutta
 * keep track of which nonterminals we've complained about.
 *
 * Revision 1.4  2004/10/19 04:55:02  jutta
 * Check for loops.
 *
 * Revision 1.3  2004/10/18 07:43:57  jutta
 * Set distance for undefined nonterminals to 0, get more meaningful errors.
 *
 * Revision 1.2  2004/10/18 07:37:30  jutta
 * Test for endless loops in grammars.
 *
 * Revision 1.1  2004/10/11 00:36:50  jutta
 * Initial revision
 *
 *
 */

#include "abnfgenp.h"
#include <limits.h>

static char const rcsid[] = 
  "$Id: check.c,v 1.5 2004/10/23 01:21:52 jutta Exp $";

static int ag_check_expression(ag_handle * ag, ag_expression * e)
{
	ag_expression * child;
	ag_nonterminal * nt;
	int max, min;
	int any = 0;

	if (e == NULL)
		return 0;

	if (e->any.distance >= 0)
		return 0;

	switch (e->type) {
	default:
		ag_error(ag, "ag_check_expression: unexpected "
			"expression type %d\n", e->type);
		return 0;

	case AG_TOKEN_NAME:
		nt = ag_nonterminal_lookup_symbol(ag, e->token.symbol);
		if (!nt) {
			if (ag_complained_lookup_symbol(ag, e->token.symbol))
				;
			else
			{
				ag_error(ag,
					"%s: no definition for "
					"nonterminal <%s>\n",
					ag->progname,
					ag_symbol_text(ag, e->token.symbol));
				ag_complained_make_symbol(ag, e->token.symbol);
			}
			e->any.distance = 0;
			return 1;
		}
		if (nt->expression && nt->expression->any.distance >= 0) {
			e->any.distance = nt->expression->any.distance + 1;
			return 1;
		}
		return 0;

	case AG_TOKEN_STRING:
	case AG_TOKEN_OCTETS:
	case AG_TOKEN_RANGE:
		e->any.distance = 0;
		return 1;

	case AG_TOKEN_PROSE:
		if (ag->understand_prose) {
			e->any.distance = 0;
			return 1;
		}
		ag_error(ag, "%s: can't expand prose <%s> (from %s:%d)\n",
			ag->progname, ag_symbol_text(ag, e->token.symbol),
			ag_symbol_text(ag, e->any.input_name),
			e->any.input_line);
		e->any.distance = 0;
		break;

	case AG_EXPRESSION_ALTERNATION:
		min = INT_MAX;
		for (child = e->compound.child; child; child = child->any.next)
		{
			any |= ag_check_expression(ag, child);
			if (  child->any.distance >= 0
			   && child->any.distance < min)
				min = child->any.distance;
		}
		if (min != INT_MAX) {
			e->any.distance = min + 1;
			return 1;
		}
		break;

	case AG_EXPRESSION_CONCATENATION:
		max = -1;
		for (child = e->compound.child; child; child = child->any.next)
		{
			any |= ag_check_expression(ag, child);
			if (child->any.distance < 0)
				return any;
			if (child->any.distance > max)
				max = child->any.distance;
		}
		e->any.distance = max;
		return 1;

	case AG_EXPRESSION_REPETITION:
		if (e->repetition.min == 0) {
			e->any.distance = 0;
			if (e->repetition.max == 0)
				return 1;
		}
		if (e->repetition.body)
			any |= ag_check_expression(ag, e->repetition.body);

		if (  e->repetition.body
		   && e->repetition.body->any.distance >= 0) {
		   	if (e->any.distance < 0)
				e->any.distance
					= e->repetition.body->any.distance + 1;
			return 1;
		}
		break;
	}
	return any;
}

void ag_check(ag_handle * ag)
{
	ag_nonterminal * nt = 0;
	int any;

	do {
		any = 0;
		nt  = 0;

		while (nt = hnext(&ag->nonterminals, ag_nonterminal, nt)) {
			if (nt->expression)
				any |= ag_check_expression(ag, nt->expression);
		}

	} while (any);

	/*  All that can be learned incrementally has been learned.
	 *  If anyone is without a distance, they do not terminate.
	 */

	while (nt = hnext(&ag->nonterminals, ag_nonterminal, nt))
		if (nt->expression && nt->expression->any.distance < 0)
			ag_error(ag, "%s: expansion for nonterminal"
				" <%s> does not terminate\n",
				ag->progname,
				ag_nonterminal_name(ag, nt));
}
