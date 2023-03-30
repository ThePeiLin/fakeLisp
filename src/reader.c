#include<fakeLisp/reader.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/lexer.h>
#include<string.h>
#include<stdlib.h>
#include<ctype.h>

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
			char* rt=(char*)realloc(tmp,sizeof(char)*(size));
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
		tmp=(char*)realloc(tmp,sizeof(char)*(size+nextSize));
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
