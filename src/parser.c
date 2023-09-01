#include<fakeLisp/parser.h>
#include<fakeLisp/grammer.h>
#include<fakeLisp/codegen.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/pattern.h>
#include<fakeLisp/vm.h>
#include<string.h>
#include<ctype.h>

FklNastNode* fklCreateNastNodeFromCstr(const char* cStr,FklSymbolTable* publicSymbolTable)
{
	FklPtrStack stateStack;
	FklPtrStack symbolStack;
	fklInitPtrStack(&stateStack,16,16);
	fklInitPtrStack(&symbolStack,16,16);
	fklPushState0ToStack(&stateStack);

	int err=0;
	size_t errLine=0;
	FklGrammerMatchOuterCtx outerCtx=FKL_GRAMMER_MATCH_OUTER_CTX_INIT;
	FklNastNode* node=fklDefaultParseForCstr(cStr
			,&outerCtx
			,publicSymbolTable
			,&err
			,&errLine
			,&symbolStack
			,&stateStack);

	while(!fklIsPtrStackEmpty(&symbolStack))
	{
		FklAnalyzingSymbol* s=fklPopPtrStack(&symbolStack);
		fklDestroyNastNode(s->ast);
		free(s);
	}
	fklUninitPtrStack(&symbolStack);
	fklUninitPtrStack(&stateStack);

	return node;
}

char* fklReadWithBuiltinParser(FILE* fp
		,size_t* psize
		,size_t line
		,size_t* pline
		,FklSymbolTable* st
		,int* unexpectEOF
		,size_t* errLine
		,FklNastNode** output
		,FklPtrStack* symbolStack
		,FklPtrStack* stateStack)
{
	size_t size=0;
	char* nextline=NULL;
	size_t nextlen=0;
	char* tmp=NULL;
	*unexpectEOF=0;
	FklNastNode* ast=NULL;
	FklGrammerMatchOuterCtx outerCtx=FKL_GRAMMER_MATCH_OUTER_CTX_INIT;
	outerCtx.line=line;
	size_t offset=0;
	for(;;)
	{
		size_t restLen=size-offset;
		int err=0;
		ast=fklDefaultParseForCharBuf(tmp+offset
				,restLen
				,&restLen
				,&outerCtx
				,st
				,&err
				,errLine
				,symbolStack
				,stateStack);
		if(err==FKL_PARSE_WAITING_FOR_MORE&&feof(fp))
		{
			*errLine=outerCtx.line;
			*unexpectEOF=FKL_PARSE_TERMINAL_MATCH_FAILED;
			free(tmp);
			tmp=NULL;
			break;
		}
		else if(err==FKL_PARSE_TERMINAL_MATCH_FAILED)
		{
			if(restLen)
			{
				FklAnalyzingSymbol* top=fklTopPtrStack(symbolStack);
				*errLine=top?top->ast->curline:outerCtx.line;
				*unexpectEOF=FKL_PARSE_REDUCE_FAILED;
				free(tmp);
				tmp=NULL;
				break;
			}
			else if(feof(fp))
				break;
		}
		else if(err==FKL_PARSE_REDUCE_FAILED)
		{
			*unexpectEOF=err;
			free(tmp);
			tmp=NULL;
			break;
		}
		else if(ast)
		{
			if(restLen)
			{
				fklRewindStream(fp,tmp+size-restLen,restLen);
				size-=restLen;
			}
			*output=ast;
			break;
		}
		ssize_t nextSize=getline(&nextline,&nextlen,fp);
		offset=size-restLen;
		if(nextSize==-1)
			continue;
		tmp=(char*)fklRealloc(tmp,sizeof(char)*(size+nextSize));
		FKL_ASSERT(tmp);
		memcpy(&tmp[size],nextline,nextSize);
		size+=nextSize;
	}
	*pline=outerCtx.line;
	*psize=size;
	free(nextline);
	return tmp;
}

char* fklReadWithAnalysisTable(const FklGrammer* g
		,FILE* fp
		,size_t* psize
		,size_t line
		,size_t* pline
		,FklSymbolTable* st
		,int* unexpectEOF
		,size_t* errLine
		,FklNastNode** output
		,FklPtrStack* symbolStack
		,FklPtrStack* stateStack)
{
	size_t size=0;
	char* nextline=NULL;
	size_t nextlen=0;
	char* tmp=NULL;
	*unexpectEOF=0;
	FklNastNode* ast=NULL;
	FklGrammerMatchOuterCtx outerCtx=FKL_GRAMMER_MATCH_OUTER_CTX_INIT;
	outerCtx.line=line;
	size_t offset=0;
	for(;;)
	{
		size_t restLen=size-offset;
		int err=0;
		ast=fklParseWithTableForCharBuf(g
				,tmp+offset
				,restLen
				,&restLen
				,&outerCtx
				,st
				,&err
				,errLine
				,symbolStack
				,stateStack);
		if(err==FKL_PARSE_WAITING_FOR_MORE&&feof(fp))
		{
			*errLine=outerCtx.line;
			*unexpectEOF=FKL_PARSE_TERMINAL_MATCH_FAILED;
			free(tmp);
			tmp=NULL;
			break;
		}
		else if(err==FKL_PARSE_TERMINAL_MATCH_FAILED)
		{
			if(restLen)
			{
				FklAnalyzingSymbol* top=fklTopPtrStack(symbolStack);
				*errLine=top?top->ast->curline:outerCtx.line;
				*unexpectEOF=FKL_PARSE_REDUCE_FAILED;
				free(tmp);
				tmp=NULL;
				break;
			}
			else if(feof(fp))
				break;
		}
		else if(err==FKL_PARSE_REDUCE_FAILED)
		{
			*unexpectEOF=err;
			free(tmp);
			tmp=NULL;
			break;
		}
		else if(ast)
		{
			if(restLen)
			{
				fklRewindStream(fp,tmp+size-restLen,restLen);
				size-=restLen;
			}
			*output=ast;
			break;
		}
		ssize_t nextSize=getline(&nextline,&nextlen,fp);
		offset=size-restLen;
		if(nextSize==-1)
			continue;
		tmp=(char*)fklRealloc(tmp,sizeof(char)*(size+nextSize));
		FKL_ASSERT(tmp);
		memcpy(&tmp[size],nextline,nextSize);
		size+=nextSize;
	}
	*pline=outerCtx.line;
	*psize=size;
	free(nextline);
	return tmp;
}
