/*
 * $Id: input.c,v 1.3 2004/10/23 01:35:16 jutta Exp jutta $
 *
 * $Log: input.c,v $
 * Revision 1.3  2004/10/23 01:35:16  jutta
 * dont dereference NULL nonterminal when adding.
 *
 * Revision 1.2  2004/10/23 00:50:32  jutta
 * add verbose expansions
 *
 * Revision 1.1  2004/10/11 00:38:57  jutta
 * Initial revision
 *
 *
 */

#include "abnfgenp.h"

static char const rcsid[] = 
  "$Id: input.c,v 1.3 2004/10/23 01:35:16 jutta Exp jutta $";

static int ag_input_alternation(
	ag_handle      * ag,
	int 		  tok,
	ag_expression ** out);

/* Returns 0 for success, AG_TOKEN_ERROR for error.
 */
static int ag_input_element(ag_handle *ag, int tok, ag_expression ** out)
{
	ag_expression	* child = 0;
	char		  buf[200];

	*out = 0;

	switch (tok) {
	case AG_TOKEN_OPEN:
		tok = ag_input_alternation(ag, ag_read_token(ag), &child);
		if (tok == AG_TOKEN_ERROR)
			return tok;
		if (tok != AG_TOKEN_CLOSE) {
			ag_error(ag, "%s:%d: expected closing parenthesis, "
				"got %s\n", ag->input_name, ag->input_line,
				ag_token_string(ag, tok, buf));
			ag_expression_free(ag, &child);
			return AG_TOKEN_ERROR;
		}
		*out = child;
		break;
	
	case AG_TOKEN_NAME:
	case AG_TOKEN_STRING:
	case AG_TOKEN_OCTETS:
	case AG_TOKEN_PROSE:
		*out = ag_expression_create(ag, tok);
		(*out)->token.symbol = ag->token_symbol;
		break;

	case AG_TOKEN_SQUAREOPEN:
		tok = ag_input_alternation(ag, ag_read_token(ag), &child);
		if (tok == AG_TOKEN_ERROR)
			return tok;
		if (tok != AG_TOKEN_SQUARECLOSE) {
			ag_error(ag, "%s:%d: expected closing square bracket,"
				" got %s\n", ag->input_name, ag->input_line,
				ag_token_string(ag, tok, buf));
			ag_expression_free(ag, &child);
			return AG_TOKEN_ERROR;
		}
		*out = ag_expression_create(ag, AG_EXPRESSION_REPETITION);
		if (!*out) {
			ag_expression_free(ag, &child);
			return AG_TOKEN_ERROR;
		}
		(*out)->repetition.min = 0;
		(*out)->repetition.max = 1;
		(*out)->repetition.body = child;
		(*out)->repetition.left_chance = 1;
		(*out)->repetition.right_chance = 1;
		break;
	
	case AG_TOKEN_RANGE:
		*out = ag_expression_create(ag, AG_TOKEN_RANGE);
		if (!*out) {
			ag_expression_free(ag, &child);
			return AG_TOKEN_ERROR;
		}
		(*out)->range.from = ag->token_range_from;
		(*out)->range.to   = ag->token_range_to;
		break;

	default:
		ag_error(ag, "%s:%d: expected expression, "
			"got %s\n", ag->input_name, ag->input_line,
			ag_token_string(ag, tok, buf));
		ag_expression_free(ag, &child);
		return AG_TOKEN_ERROR;
	}
	return 0;
}

/* Returns 0 for success, AG_ERROR_SYNTAX on error.
 */
static int ag_input_repetition(ag_handle *ag, int tok, ag_expression ** out)
{
	ag_expression	* child = 0;
	long long	  num1 = -1, num2 = -1;
	int 		  err = 0;
	unsigned long	  left_chance = 1, right_chance = 1;
	char		  buf[200];

	*out = 0;

	if (tok == AG_TOKEN_CHANCE) {
		left_chance = (unsigned long)ag->token_number;
		tok = ag_read_token(ag);
	}
	if (tok == AG_TOKEN_ERROR)
		return AG_ERROR_SYNTAX;

	if (tok == AG_TOKEN_NUMBER) {

		num2 = num1 = ag->token_number;

		tok = ag_read_token(ag);
		if (ag->token_saw_space)
			ag_error(ag, "%s:%d: "
				"ABNF doesn't allow white space after "
				"repeat count, before %s\n",
				ag->input_name,
				ag->input_line,
				ag_token_string(ag, tok, buf));
	}
	if (tok == AG_TOKEN_ASTERISK) {

		/*  If an * is used, an unset first
		 *  number defaults to 0.
		 */
		if (num1 == -1)
			num1 = 0;

		num2 = -1;
		tok  = ag_read_token(ag);

		if (ag->token_saw_space)
			ag_error(ag, "%s:%d: "
				"ABNF doesn't allow white space after *, "
				"before %s\n",
				ag->input_name,
				ag->input_line,
				ag_token_string(ag, tok, buf));

		/* An optional {chance}
		 */
		if (tok == AG_TOKEN_CHANCE) {
			right_chance = (unsigned long)ag->token_number;
			tok = ag_read_token(ag);
		}

		/*  An optional number, defaulting
		 *  to (above) -1 (unlimited).
		 */
		if (tok == AG_TOKEN_NUMBER) {

			num2 = ag->token_number;
			tok  = ag_read_token(ag);

			if (ag->token_saw_space)
				ag_error(ag, "%s:%d: "
					"ABNF doesn't allow white space after "
					"maximum repeat count, before %s\n",
					ag->input_name,
					ag->input_line,
					ag_token_string(ag, tok, buf));
		}
		else if (tok == AG_TOKEN_ERROR)
			return AG_ERROR_SYNTAX;
	}
	else {
		if (num1 == -1)
			num2 = num1 = 1;
	
		else
			num2 = num1;

		left_chance  = 1;
		right_chance = 1;
	}

	err = ag_input_element(ag, tok, &child);
	if (err) {
		ag_expression_free(ag, &child);
		return err;
	}

	if (num1 == num2 && num1 == 1) 
		*out = child;
	else {
		*out = ag_expression_create(ag, AG_EXPRESSION_REPETITION);
		if (!*out) {
			ag_expression_free(ag, &child);
			return AG_ERROR_MEMORY;
		}
		(*out)->repetition.min  = num1;
		(*out)->repetition.max  = num2;
		(*out)->repetition.body = child;
		(*out)->repetition.left_chance = left_chance;
		(*out)->repetition.right_chance = right_chance;
	}
	return 0;
}

/* Returns the first token it couldn't consume.
 */
static int ag_input_concatenation(
	ag_handle      * ag,
	int		  tok,
	ag_expression ** out)
{
	ag_expression  * con = 0, * child = 0;
	int		  err;
	unsigned long	  chance = 1;

	*out = 0;

	if (tok == AG_TOKEN_CHANCE) {
		chance = (unsigned long)ag->token_number;
		tok = ag_read_token(ag);
	}
	do {
		err = ag_input_repetition(ag, tok, &child);
		if (  err
		   || ag_compound_add(ag, AG_EXPRESSION_CONCATENATION,
		   	&con, &child)) {

			ag_expression_free(ag, &con);
			ag_expression_free(ag, &child);
			return AG_TOKEN_ERROR;
		}

		tok = ag_read_token(ag);

	} while (  tok != AG_TOKEN_ERROR
		&& tok != AG_TOKEN_EOF
		&& tok != AG_TOKEN_OR
		&& tok != AG_TOKEN_EQUALS
		&& tok != AG_TOKEN_EQUALSTOO
		&& tok != AG_TOKEN_CLOSE
		&& tok != AG_TOKEN_SQUARECLOSE
		&& tok != AG_TOKEN_LHS);

	if (con) {
		if (  con->type == AG_EXPRESSION_CONCATENATION
		   &&  con->compound.child
		   && !con->compound.child->any.next) {
		   
		   	*out = con->compound.child;
			con->compound.child = 0;
			ag_expression_free(ag, &con);
		}
		else {
			*out = con;
		}
		(*out)->any.chance = chance;
	}
	return tok;
}

/* Returns the first token it couldn't consume.
 */
static int ag_input_alternation(
	ag_handle      * ag,
	int 		  tok,
	ag_expression ** out)
{
	ag_expression  * alt = 0, * child = 0;

	*out = 0;
 	for (;;) {
		tok = ag_input_concatenation(ag, tok, &child);
		if (  (tok == AG_TOKEN_ERROR)
		   || ag_compound_add(ag, AG_EXPRESSION_ALTERNATION,
		   	&alt, &child)) {

			ag_expression_free(ag, &alt);
			ag_expression_free(ag, &child);
			return AG_TOKEN_ERROR;
		}
	 	if (tok != AG_TOKEN_OR)
			break;
		tok = ag_read_token(ag);
	}

	if (alt) assert(alt->type == AG_EXPRESSION_ALTERNATION);

	if (   alt != NULL
	   &&  alt->type == AG_EXPRESSION_ALTERNATION
	   &&  alt->compound.child
	   && !alt->compound.child->any.next) {

	   	*out = alt->compound.child;
		alt->compound.child = 0;
		ag_expression_free(ag, &alt);
	}
	else *out = alt;

	return tok;
}


static int ag_input_rulelist(ag_handle * ag, int tentative)
{
	int 		  err, equals, tok;
	ag_symbol	  rulename;
	ag_nonterminal * nt;
	ag_expression  * alt = 0;
	char		  buf[200];

	tok = ag_read_token(ag);
	while (tok == AG_TOKEN_LHS) {

		rulename = ag->token_symbol;

		tok = ag_read_token(ag);
		if (  tok == AG_TOKEN_EQUALS
		   || tok == AG_TOKEN_EQUALSTOO) {
			equals = tok;
			tok = ag_read_token(ag);
		}
		else {
			if (tok != AG_TOKEN_ERROR)
				ag_error(ag, "%s:%d: "
					"expected = or =/, got %s\n",
					ag->input_name,
					ag->input_line,
					ag_token_string(ag, tok, buf));
			return AG_ERROR_SYNTAX;
		}

		alt = 0;
		tok = ag_input_alternation(ag, tok, &alt);
		if (tok == AG_TOKEN_ERROR) {
			ag_expression_free(ag, &alt);
			break;
		}
		if (alt) {
			if (equals == AG_TOKEN_EQUALS) {

				equals = AG_TOKEN_EQUALSTOO;
				nt = ag_nonterminal_make_symbol(ag, rulename);
				if (!nt)
					return 0;

				if (nt->expression) {
					if (!nt->tentative)
						ag_error(ag,
							"%s:%d: redefinition "
							"of <%s>\n",
							ag->input_name,
							ag->input_line,
							ag_symbol_text(ag,
								rulename));
					ag_expression_free(ag, &nt->expression);
				}
				nt->expression = alt;
				alt = 0;
			}
			else {
				nt = ag_nonterminal_make_symbol(ag, rulename);
				if (  nt->expression
				   && nt->expression->type
				   != AG_EXPRESSION_ALTERNATION)
				{
					ag_expression *e = NULL;
					err = ag_compound_add(ag,
						AG_EXPRESSION_ALTERNATION, 
						&e, &nt->expression);
					if (err) return err;
					nt->expression = e;
				}
				if (alt->type == AG_EXPRESSION_ALTERNATION)
				{
					err = ag_compound_add(ag,
						AG_EXPRESSION_ALTERNATION, 
						&nt->expression,
						&alt->compound.child);
					ag_expression_free(ag, &alt);
				} else
					err = ag_compound_add(ag,
						AG_EXPRESSION_ALTERNATION, 
						&nt->expression, &alt);
				if (err)
					return err;
			}
			if (  !(nt->tentative = tentative)
			   && !ag->start_symbol)
			   	ag->start_symbol = rulename;
		}
	}

	if (tok == AG_TOKEN_EOF)
		return 0;

	if (tok != AG_TOKEN_ERROR)
		if (tok == AG_TOKEN_NAME)
		{
			ag_error(ag, "%s:%d: "
				"expected \"=\" or \"/=\" after "
				"left-hand-side name \"%s\"\n",
				ag->input_name,
				ag->input_line,
				ag_symbol_text(ag, ag->token_symbol));
		}
		else
		{
			ag_error(ag, "%s:%d: "
				"expected left-hand-side nonterminal name, got %s\n",
				ag->input_name,
				ag->input_line,
				ag_token_string(ag, tok, buf));
		}
	return AG_ERROR_SYNTAX;
}

void ag_input(
	ag_handle	* ag,
	FILE 		* fp,
	char const 	* filename,
	int		  tentative)
{
	ag->input_file = fp;
	ag->input_string = NULL;
	ag->input_name = filename;
	ag->input_line = 1;

	(void)ag_input_rulelist(ag, tentative);
}

void ag_input_string(
	ag_handle	* ag,
	char const	* text,
	char const 	* filename,
	int		  tentative)
{
	ag->input_string = text;
	ag->input_file   = NULL;
	ag->input_name   = filename;
	ag->input_line   = 1;

	(void)ag_input_rulelist(ag, tentative);
}
