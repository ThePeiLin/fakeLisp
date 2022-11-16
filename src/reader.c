#include<fakeLisp/reader.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/lexer.h>
#include<string.h>
#include<stdlib.h>
#include<ctype.h>

char* fklReadLine(FILE* fp,size_t* size)
{
	int32_t memSize=FKL_MAX_STRING_SIZE;
	char* tmp=(char*)malloc(sizeof(char)*memSize);
	FKL_ASSERT(tmp);
	int ch=getc(fp);
	for(;;)
	{
		if(ch==EOF)
			break;
		(*size)++;
		if((*size)>memSize)
		{
			char* ttmp=(char*)realloc(tmp,sizeof(char)*(memSize+FKL_MAX_STRING_SIZE));
			FKL_ASSERT(ttmp);
			tmp=ttmp;
			memSize+=FKL_MAX_STRING_SIZE;
		}
		tmp[*size-1]=ch;
		if(ch=='\n')
			break;
		ch=getc(fp);
	}
	char* ttmp=(char*)realloc(tmp,sizeof(char)*(*size));
	FKL_ASSERT(!*size||ttmp);
	tmp=ttmp;
	return tmp;
}

char* fklReadInStringPattern(FILE* fp
		,char** prev
		,size_t* psize
		,size_t* prevSize
		,size_t curline
		,int* unexpectEOF
		,FklPtrStack* retval
		,char* (*read)(FILE*,size_t*)
		,FklStringMatchPattern* patterns)
{
	size_t size=0;
	char* tmp=NULL;
	*unexpectEOF=0;
	if(!read)
		read=fklReadLine;
	if(*prev)
	{
		tmp=(char*)malloc(sizeof(char)*(*prevSize));
		FKL_ASSERT(tmp);
		memcpy(tmp,*prev,*prevSize);
		free(*prev);
		*prev=NULL;
		size=*prevSize;
		*prevSize=0;
	}
	else
		size=0;
	size_t line=curline;
	FklStringMatchSet* matchSet=FKL_STRING_PATTERN_UNIVERSAL_SET;
	for(;;)
	{
		size_t j=0;
		matchSet=fklSplitStringIntoTokenWithPattern(tmp
				,size
				,line
				,&j
				,&line
				,retval
				,FKL_STRING_PATTERN_UNIVERSAL_SET,patterns);
		if(matchSet==NULL)
		{
			if(size-j)
			{
				*prevSize=size-j;
				*prev=fklCopyMemory(tmp+j,*prevSize);
				size-=*prevSize;
			}
			char* rt=(char*)realloc(tmp,sizeof(char)*(size));
			FKL_ASSERT(!size||rt);
			tmp=rt;
			if((!fklIsAllSpaceBufSize(tmp,size)&&!fklIsAllComment(retval))||feof(fp))
			{
				curline=line;
				break;
			}
		}
		else if(feof(fp))
		{
			while(!fklIsPtrStackEmpty(retval))
				fklDestroyToken(fklPopPtrStack(retval));
			fklDestroyStringMatchSet(matchSet);
			*unexpectEOF=1;
			free(tmp);
			tmp=NULL;
			break;
		}
		size_t nextSize=0;
		char* next=read(fp,&nextSize);
		tmp=(char*)realloc(tmp,sizeof(char)+(size+nextSize));
		FKL_ASSERT(tmp);
		memcpy(tmp+size,next,nextSize);
		size+=nextSize;
		free(next);
	}
	*psize=size;
	return tmp;
}

int fklIsAllSpaceBufSize(const char* buf,size_t size)
{
	for(size_t i=0;i<size;i++)
		if(!isspace(buf[i]))
			return 0;
	return 1;
}
