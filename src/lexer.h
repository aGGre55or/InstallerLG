#ifndef LEXER_H_
#define LEXER_H_

#define LINE yyget_lineno(scanner)
#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
typedef void* yyscan_t;
#endif

typedef union
{
    int n; 
    char *s; 
    entry_p e;
} YYSTYPE;

int yyget_lineno(yyscan_t yyscanner);
int yylex(YYSTYPE * yylval_param ,yyscan_t yyscanner);
int yyerror(yyscan_t scanner, char *err);

#endif