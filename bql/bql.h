#ifndef BQL_BQL_H
#define BQL_BQL_H

#include "../bolo.h"

/* global query value, shared between lexer / parser */
struct query *QUERY;

struct range {
	int from;
	int until;
};

typedef union {
	double  number;
	char   *text;
	int     integer;

	struct {
		int from;
		int until;
	} range;

	struct bucket bucket;

	struct qfield *qfield;
	struct qexpr  *qexpr;

	struct qcond *qcond;

	struct query *query;
} YYSTYPE;
#define YYSTYPE_IS_DECLARED 1

int yylex(void);
int yyparse(void);
int yyerror(const char *);

#endif
