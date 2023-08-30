#include<fakeLisp/reader.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/lexer.h>
#include<fakeLisp/grammer.h>
#include<string.h>
#include<stdlib.h>
#include<ctype.h>

char* fklReadWithBuiltinParser(FILE* fp
		,size_t* psize
		,size_t line
		,size_t* pline
		,FklSymbolTable* st
		,int* unexpectEOF
		,size_t* errLine
		,FklNastNode** output)
{
	size_t size=0;
	char* nextline=NULL;
	size_t nextlen=0;
	FklPtrStack stateStack;
	FklPtrStack symbolStack;
	fklInitPtrStack(&stateStack,16,16);
	fklInitPtrStack(&symbolStack,16,16);
	fklPushState0ToStack(&stateStack);
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
				,&symbolStack
				,&stateStack);
		if(err==FKL_PARSE_TERMINAL_MATCH_FAILED&&feof(fp))
		{
			if(stateStack.top>1)
			{
				*errLine=line;
				*unexpectEOF=!restLen?err:FKL_PARSE_REDUCE_FAILED;
				free(tmp);
				tmp=NULL;
			}
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
	fklUninitPtrStack(&stateStack);
	while(!fklIsPtrStackEmpty(&symbolStack))
	{
		FklAnalyzingSymbol* s=fklPopPtrStack(&symbolStack);
		fklDestroyNastNode(s->ast);
		free(s);
	}
	fklUninitPtrStack(&symbolStack);
	*pline=outerCtx.line;
	*psize=size;
	free(nextline);
	return tmp;
}

char* fklReadInStringPattern(FILE* fp
		,size_t* psize
		,size_t line
		,size_t* pline
		,int* unexpectEOF
		,FklPtrStack* retval
		,FklStringMatchPattern* patterns
		,FklStringMatchRouteNode** proute)
{
	size_t size=0;
	char* nextline=NULL;
	size_t nextlen=0;
	char* tmp=NULL;
	*unexpectEOF=0;
	FklStringMatchSet* matchSet=FKL_STRING_PATTERN_UNIVERSAL_SET;
	FklStringMatchRouteNode* route=fklCreateStringMatchRouteNode(NULL
			,0,0
			,NULL
			,NULL
			,NULL);
	*proute=route;
	size_t j=0;
	int err=0;
	for(;;)
	{
		matchSet=fklSplitStringIntoTokenWithPattern(tmp
				,size
				,line
				,&line
				,j
				,&j
				,retval
				,matchSet
				,patterns
				,route
				,&route
				,&err);
		if(matchSet==NULL)
		{
			if(size-j)
			{
				size_t restSize=size-j;
				fklRewindStream(fp,tmp+j,restSize);
				size-=restSize;
			}
			char* rt=(char*)fklRealloc(tmp,sizeof(char)*(size));
			FKL_ASSERT(rt||!size);
			tmp=rt;
			break;
		}
		else if(feof(fp))
		{
			while(!fklIsPtrStackEmpty(retval))
				fklDestroyToken(fklPopPtrStack(retval));
			fklDestroyStringMatchSet(matchSet);
			if(matchSet!=FKL_STRING_PATTERN_UNIVERSAL_SET)
				*unexpectEOF=1;
			free(tmp);
			tmp=NULL;
			break;
		}
		else if(err)
		{
			while(!fklIsPtrStackEmpty(retval))
				fklDestroyToken(fklPopPtrStack(retval));
			fklDestroyStringMatchSet(matchSet);
			*unexpectEOF=2;
			free(tmp);
			tmp=NULL;
			break;
		}
		ssize_t nextSize=getline(&nextline,&nextlen,fp);
		if(nextSize==-1)
			continue;
		tmp=(char*)fklRealloc(tmp,sizeof(char)*(size+nextSize));
		FKL_ASSERT(tmp);
		memcpy(&tmp[size],nextline,nextSize);
		size+=nextSize;
	}
	*pline=line;
	*psize=size;
	free(nextline);
	return tmp;
}

int fklIsAllSpaceBufSize(const char* buf,size_t size)
{
	for(size_t i=0;i<size;i++)
		if(!isspace(buf[i]))
			return 0;
	return 1;
}
