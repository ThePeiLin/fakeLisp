#include<fakeLisp/grammer.h>
#include<fakeLisp/base.h>
#include<fakeLisp/parser.h>
#include<fakeLisp/utils.h>
#include<stdio.h>
#include<stdlib.h>
#include<ctype.h>

int main()
{
	FklSymbolTable* st=fklCreateSymbolTable();

	fputc('\n',stdout);
	
	const char* exps[]=
	{
		"#\\\\11",
		"#\\\\z",
		"#\\\\n",
		"#\\\\",
		"#\\;",
		"#\\|",
		"#\\\"",
		"#\\(",
		"#\\\\s",
		"(abcd)abcd",
		";comments\nabcd",
		"foobar|foo|foobar|bar|",
		"(\"foo\" \"bar\" \"foobar\",;abcd\n\"i\")",
		"[(foobar;comments\nfoo bar),abcd]",
		"(foo bar abcd|foo \\|bar|efgh foo \"foo\\\"\",bar)",
		"#hash((a,1) (b,2))",
		"#vu8(114 514 114514)",
		"114514",
		"#\\ ",
		"'#&#(foo 0x114514 \"foobar\" .1 0x1p1 114514|foo|bar #\\a #\\\\0 #\\\\x11 #\\\\0123 #\\\\0177 #\\\\0777)",
		"\"foobar\"",
		"114514",
		"\"foobar\"",
		NULL,
	};

	int retval=0;
	for(const char** exp=&exps[0];*exp;exp++)
	{
		FklGrammerMatchOuterCtx outerCtx=FKL_GRAMMER_MATCH_OUTER_CTX_INIT;

		FklPtrStack stateStack;
		FklPtrStack symbolStack;

		fklInitPtrStack(&stateStack,8,16);
		fklPushState0ToStack(&stateStack);
		fklInitPtrStack(&symbolStack,8,16);
		FklNastNode* ast=fklReaderParserForCstr(*exp,&outerCtx,st,&retval,&symbolStack,&stateStack);

		while(!fklIsPtrStackEmpty(&symbolStack))
			free(fklPopPtrStack(&symbolStack));
		fklUninitPtrStack(&symbolStack);
		fklUninitPtrStack(&stateStack);
		if(retval)
			break;

		fklPrintNastNode(ast,stdout,st);
		fklDestroyNastNode(ast);

		fputc('\n',stdout);
	}
	fklDestroySymbolTable(st);
	return retval;
}
