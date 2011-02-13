/*
 * $Id: scan.c,v 1.1 2004/10/11 00:38:57 jutta Exp $
 *
 * $Log: scan.c,v $
 * Revision 1.1  2004/10/11 00:38:57  jutta
 * Initial revision
 *
 *
 */

#include "abnfgenp.h"
#include <ctype.h>

static char const rcsid[] = 
  "$Id: scan.c,v 1.1 2004/10/11 00:38:57 jutta Exp $";

#define	IS_IDENTIFIER_CHAR(ag, c)		\
	(  (c) == '-' 				\
	|| (isascii(c) && isalnum(c))		\
	|| ((ag)->underscore_in_identifiers && c == '_'))

static void ag_ungetc(ag_handle * ag, int c)
{
	if (c == '\n')
		ag->input_line--;

	if (ag->input_file != NULL)
		ungetc(c, ag->input_file);
	else if (ag->input_string != NULL)
		ag->input_string--;
}

int ag_getc(ag_handle * ag)
{
	int c;

	if (ag->input_file == NULL)
	{
		if (ag->input_string == NULL)
			return EOF;

		c = *ag->input_string++;
		if (c == '\0')
			ag->input_string = NULL;
	}
	else
	{
		c = getc(ag->input_file);
		if (c == EOF) {
			ag->input_file = 0;
			return EOF;
		}
	}
	if (c == '\n')
		ag->input_line++;
	return c;
}

static int ag_getc_skipspace(ag_handle * ag)
{
	int c;

	while ((c = ag_getc(ag)) != EOF) {
		if (isspace((unsigned char)c))
		{
			ag->token_saw_space = 1;
			continue;
		}
		if (c == ';' || c == '#')
		{
			ag->token_saw_space   = 1;
			while ((c = ag_getc(ag)) != EOF && c != '\n')
				;
		}
		else
			break;
	}
	return c;
}


static ag_type ag_read_token_chance(ag_handle * ag)
{
	long long num = 0;
	int	  c;

	/* Read a non-white input character.
	 */
	while ((c = ag_getc(ag)) != EOF && isdigit(c)) {

		num *= 10;
		num += c - '0';
	}
	ag->token_number = num;
	if (c != '}') {
		ag_error(ag, "%s:%d: expected closing brace, got %s\n",
			ag->input_name, ag->input_line, renderchar(c));
		return AG_TOKEN_ERROR;
	}
	return AG_TOKEN_CHANCE;
}

static ag_type ag_read_token_number(ag_handle * ag)
{
	long long num = 0;
	int	  c;

	/* Read a non-white input character.
	 */
	while ((c = ag_getc(ag)) != EOF && isdigit(c)) {

		num *= 10;
		num += c - '0';
	}
	ag->token_number = num;
	ag_ungetc(ag, c);

	return AG_TOKEN_NUMBER;
}

static ag_type ag_read_token_name(ag_handle * ag)
{
	static char	* s = 0;
	static size_t	  m = 0;
	size_t		  n = 0;

	for (;;) {
		int c;
		
		if (n + 1 >= m) {
			
			char * tmp;

			m += 64;
			if (!(tmp = realloc(s, m))) {
				ag_error(ag, "%s:%d: failed to reallocate "
					"%lu bytes for name token buffer\n",
					ag->input_name, ag->input_line, m);
				return AG_TOKEN_ERROR;
			}
			s = tmp;
		}
		c = ag_getc(ag);

		if (! IS_IDENTIFIER_CHAR(ag, c))
		{
			ag_ungetc(ag, c);
			break;
		}
		s[n++] = tolower(c);
	}

	s[n] = 0;
	ag->token_symbol = ag_symbol_make(ag, s);
	return ag->token_symbol ? AG_TOKEN_NAME : AG_TOKEN_ERROR;
}

static ag_type ag_read_token_literal(ag_handle * ag)
{
	static char	* s = 0;
	static size_t	  m = 0;
	size_t		  n = 0;

	for (;;) {
		int c;
		
		if (n + 1 >= m) {
			
			char * tmp;

			m += 64;
			if (!(tmp = realloc(s, m))) {
				ag_error(ag, "%s:%d: failed to reallocate "
					"%lu bytes for name token buffer\n",
					ag->input_name, ag->input_line, m);
				return AG_TOKEN_ERROR;
			}
			s = tmp;
		}
		c = ag_getc(ag);
		if (c == EOF)
		{
			ag_error(ag, "%s:%d: EOF in literal '%.*s%s'\n",
				ag->input_name,
				ag->input_line,
				(int)(n > 80 ? 80 : n), s, n > 80 ? "..." : "");
			return AG_TOKEN_ERROR;
		}
		if (c == '\'')
			break;
		s[n++] = c;
	}
	s[n] = 0;
	ag->token_symbol = ag_symbol_make_binary(ag, s, n);
	return ag->token_symbol ? AG_TOKEN_OCTETS : AG_TOKEN_ERROR;
}

static ag_type ag_read_token_string(ag_handle * ag)
{
	static char	* s = 0;
	static size_t	  m = 0;
	size_t		  n = 0;

	for (;;) {
		int c;
		
		if (n + 1 >= m) {
			
			char * tmp;

			m += 64;
			if (!(tmp = realloc(s, m))) {
				ag_error(ag, "%s:%d: failed to reallocate "
					"%lu bytes for name token buffer\n",
					ag->input_name, ag->input_line, m);
				return AG_TOKEN_ERROR;
			}
			s = tmp;
		}
		c = ag_getc(ag);
		if (c == EOF)
		{
			ag_error(ag, "%s:%d: EOF in string \"%.*s%s\"\n",
				ag->input_name,
				ag->input_line,
				(int)(n > 80 ? 80 : n), s, n > 80 ? "..." : "");
			return AG_TOKEN_ERROR;
		}
		if (c == '"')
			break;
		s[n++] = tolower(c);
	}
	s[n] = 0;
	ag->token_symbol = ag_symbol_make(ag, s);
	return ag->token_symbol ? AG_TOKEN_STRING : AG_TOKEN_ERROR;
}

static ag_type ag_read_token_prose(ag_handle * ag)
{
	static char	* s = 0;
	static size_t	  m = 0;
	size_t		  n = 0;

	for (;;) {
		int c;
		if (n + 1 >= m) {
			
			char * tmp;

			m += 64;
			if (!(tmp = realloc(s, m))) {
				ag_error(ag, "%s:%d: failed to reallocate "
					"%lu bytes for name token buffer\n",
					ag->input_name, ag->input_line, m);
				return AG_TOKEN_ERROR;
			}
			s = tmp;
		}
		c = ag_getc(ag);
		if (c == EOF)
		{
			ag_error(ag, "%s:%d: EOF in prose <%.*s%s>\n",
				ag->input_name,
				ag->input_line,
				(int)(n > 80 ? 80 : n), s, n > 80 ? "..." : "");
			return AG_TOKEN_ERROR;
		}
		if (c == '>')
			break;
		s[n++] = c;
	}
	s[n] = 0;
	if (n == 1) {
		s[0] = tolower(s[0]);
		ag->token_symbol = ag_symbol_make(ag, s);
		return ag->token_symbol ? AG_TOKEN_STRING : AG_TOKEN_ERROR;
	}
	else {
		ag->token_symbol = ag_symbol_make(ag, s);
		return ag->token_symbol ? AG_TOKEN_PROSE : AG_TOKEN_ERROR;
	}
}

static int ag_read_token_code(ag_handle * ag, int base, unsigned long * out)
{
	int c;
	int num = 0;
	int first = 1;

	for (;;) {
		int n;

		c = ag_getc(ag);
		if (isdigit(c)) n = c - '0';
		else if (islower(c)) n = c - 'a' + 10;
		else if (isupper(c)) n = (c - 'A') + 10;
		else
		{
			ag_ungetc(ag, c);
			if (first)
			{
				ag_error(ag,
					"%s:%d: expected base-%d value, got '%c'\n",
					ag->input_name, ag->input_line,
						base, c);
				return -1;
			}
			break;
		}
		if (n >= base)
		{
			ag_error(ag,
				"%s:%d: expected base-%d value, got '%c'\n",
				ag->input_name, ag->input_line, base, c);
			return -1;
		}
		first = 0;
		num = num * base + n;
	}
	*out = num;
	return 0;
}

static ag_type ag_read_token_chars(ag_handle * ag)
{
	int c, base;
	unsigned long num = 0;
	static char	* s = 0;
	static size_t	  m = 0;
	size_t		  n = 0;

	c = ag_getc(ag);

	switch (tolower(c)) {
	default:  ag_ungetc(ag, c);
	case 'x': base = 16;		break;
	case 'b': base = 2;		break;
	case 'o': base = 8;		break;
	case 'd': base = 10;		break;
	}

	if (ag_read_token_code(ag, base, &num))
		return AG_TOKEN_ERROR;

	c = ag_getc(ag);

	if (c == '-') {
		ag->token_range_from = num;
		if (ag_read_token_code(ag, base, &ag->token_range_to)) {
			return AG_TOKEN_ERROR;
		}
		return AG_TOKEN_RANGE;
	}
	else for (;;) {
		if (n + 10 >= m) {
			char * tmp;

			m += 64;
			if (!(tmp = realloc(s, m))) {
				ag_error(ag, "%s:%d: "
					"failed to reallocate %lu "
					"bytes for name token buffer\n",
					ag->input_name, ag->input_line, m);
				return AG_TOKEN_ERROR;
			}
			s = tmp;
		}
		n += to_utf8(num, (unsigned char *)(s + n));
		if (c != '.') {
			break;
		}
		if (ag_read_token_code(ag, base, &num)) {
			return AG_TOKEN_ERROR;
		}
		c = ag_getc(ag);
	}
	ag_ungetc(ag, c);
	ag->token_symbol = ag_symbol_make_binary(ag, s, n);

	return ag->token_symbol ? AG_TOKEN_OCTETS : AG_TOKEN_ERROR;
}

ag_type ag_read_token(ag_handle * ag)
{
	int c;
	int result;

	ag->token_saw_space   = 0;

	/* Read a non-white input character.
	 */
	for (;;) {
		c = ag_getc_skipspace(ag);
		if (c == EOF) return AG_TOKEN_EOF;
		if (isdigit((unsigned char)c)) {
			ag_ungetc(ag, c);
			return ag_read_token_number(ag);
		}

		if (isalpha((unsigned char)c)) {

			int  token_saw_space = ag->token_saw_space;

			ag_ungetc(ag, c);

			result = ag_read_token_name(ag);
			if (result == AG_TOKEN_ERROR)
				return result;

			c = ag_getc_skipspace(ag);
			ag_ungetc(ag, c);
			ag->token_saw_space = token_saw_space;

			if (c == '=')
				return AG_TOKEN_LHS;
			return AG_TOKEN_NAME;
		}

		switch (c)
		{
		  case '{': 
		  	    if (!ag->legal)
			    	return ag_read_token_chance(ag);

			    ag_error(ag, 
				"%s:%d unexpected input '{' (omit command-line "
				"'-l' to enable branch weighting)\n",
				ag->input_name,
				ag->input_line);
			    break;
   
		  case '\'':
		  	    if (!ag->legal)
			    	return ag_read_token_literal(ag);

			    ag_error(ag, 
				"%s:%d unexpected input <'> (omit command-line "
				"'-l' to enable case-sensitive text literals)\n",
				ag->input_name,
				ag->input_line);
			    break;

		  case '"': return ag_read_token_string(ag);
		  case '<': return ag_read_token_prose(ag);
		  case '%': return ag_read_token_chars(ag);
		  case '*': return AG_TOKEN_ASTERISK;
		  case '(': return AG_TOKEN_OPEN;
		  case ')': return AG_TOKEN_CLOSE;
		  case '[': return AG_TOKEN_SQUAREOPEN;
		  case ']': return AG_TOKEN_SQUARECLOSE;
		  case '/': return AG_TOKEN_OR;
		  case '=':
			c = ag_getc(ag);
			if (c == '/') return AG_TOKEN_EQUALSTOO;

			ag_ungetc(ag, c);
			return AG_TOKEN_EQUALS;

		  default:
			ag_error(ag, 
				"%s:%d unexpected input '%s'\n",
				ag->input_name,
				ag->input_line,
				renderchar((unsigned char)c));
		}
	}
}

char const * ag_token_string(ag_handle * ag, ag_type tok, char * buf)
{
	size_t	n;

	switch (tok) {
	case AG_TOKEN_ERROR: 		return "*error*"; 
	case AG_TOKEN_EOF:		return "*EOF*";
	case AG_TOKEN_NONE:		return "*none*";
	case AG_TOKEN_ASTERISK:		return "*";
	case AG_TOKEN_OR:		return "/";
	case AG_TOKEN_EQUALS:		return "=";
	case AG_TOKEN_EQUALSTOO:	return "=/";
	case AG_TOKEN_SQUAREOPEN:	return "[";
	case AG_TOKEN_SQUARECLOSE:	return "]";
	case AG_TOKEN_OPEN:		return "(";
	case AG_TOKEN_CLOSE:		return ")";

	case AG_TOKEN_CHANCE:
		sprintf(buf, "<chance %lu>",
			(unsigned long)ag->token_number);
		break;

	case AG_TOKEN_NUMBER:
		sprintf(buf, "<number %lu>",
			(unsigned long)ag->token_number);
		break;

	case AG_TOKEN_NAME:
		sprintf(buf, "<name %.100s>",
			ag_symbol_text(ag, ag->token_symbol));
		break;

	case AG_TOKEN_STRING:
		sprintf(buf, "<string \"%.100s\">",
			ag_symbol_text(ag, ag->token_symbol));
		break;

	case AG_TOKEN_PROSE:
		sprintf(buf, "<prose \"%.100s\">",
			ag_symbol_text(ag, ag->token_symbol));
		break;

	case AG_TOKEN_LHS:
		sprintf(buf, "<lhs \"%.100s\">",
			ag_symbol_text(ag, ag->token_symbol));
		break;

	case AG_TOKEN_OCTETS:
		n = ag_symbol_size(ag, ag->token_symbol);
		sprintf(buf, "<octets \"%.*s\">",
			n > 100 ? 100 : n, 
			ag_symbol_text(ag, ag->token_symbol));
		break;

	case AG_TOKEN_RANGE:
		sprintf(buf, "<range %lu .. %lu>", 
			ag->token_range_from,
			ag->token_range_to);
		break;

	default:
		sprintf(buf, "<unknown token %d>", tok);
		break;
	}
	return buf;
}
