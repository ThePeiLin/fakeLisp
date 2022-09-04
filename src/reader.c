#include<fakeLisp/reader.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/parser.h>
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

char* fklReadInStringPattern(FILE* fp,char** prev,size_t* size,size_t* prevSize,uint32_t curline,int* unexpectEOF,FklPtrStack* retval,char* (*read)(FILE*,size_t*))
{
	char* tmp=NULL;
	*unexpectEOF=0;
	FklPtrStack* matchStateStack=fklNewPtrStack(32,16);
	if(!read)
		read=fklReadLine;
	if(*prev)
	{
		tmp=(char*)malloc(sizeof(char)*(*prevSize));
		FKL_ASSERT(tmp);
		memcpy(tmp,*prev,*prevSize);
		free(*prev);
		*prev=NULL;
		*size=*prevSize;
		*prevSize=0;
	}
	else
		*size=0;
	uint32_t cpost=0;
	uint32_t line=curline;
	for(;;)
	{
		const char* strs[]={tmp+cpost};
		size_t sizes[]={*size-cpost};
		uint32_t i=0,j=0;
		int r=fklSplitStringPartsIntoToken(strs,sizes,1,&line,retval,matchStateStack,&i,&j);
		if(r==0)
		{
			if(*size-j-cpost)
			{
				*prevSize=*size-j-cpost;
				*prev=fklCopyMemory(tmp+cpost+j,*prevSize);
				*size-=*prevSize;
			}
			char* rt=(char*)realloc(tmp,sizeof(char)*(*size));
			FKL_ASSERT(!*size||rt);
			tmp=rt;
			if((!fklIsAllSpaceBufSize(tmp,*size)&&!fklIsAllComment(retval))||feof(fp))
			{
				curline=line;
				break;
			}
		}
		else if(r==1&&feof(fp))
		{
			while(!fklIsPtrStackEmpty(retval))
				fklFreeToken(fklPopPtrStack(retval));
			while(!fklIsPtrStackEmpty(matchStateStack))
				free(fklPopPtrStack(matchStateStack));
			*unexpectEOF=1;
			free(tmp);
			tmp=NULL;
			break;
		}
		else if(r==2)
		{
			while(!fklIsPtrStackEmpty(retval))
				fklFreeToken(fklPopPtrStack(retval));
			while(!fklIsPtrStackEmpty(matchStateStack))
				free(fklPopPtrStack(matchStateStack));
			*unexpectEOF=2;
			free(tmp);
			tmp=NULL;
			break;
		}
		cpost+=j;
		size_t nextSize=0;
		char* next=read(fp,&nextSize);
		tmp=(char*)realloc(tmp,sizeof(char)+(*size+nextSize));
		FKL_ASSERT(tmp);
		memcpy(tmp+*size,next,nextSize);
		*size+=nextSize;
		free(next);
	}
	fklFreePtrStack(matchStateStack);
	return tmp;
}

int fklIsAllSpaceBufSize(const char* buf,size_t size)
{
	for(size_t i=0;i<size;i++)
		if(!isspace(buf[i]))
			return 0;
	return 1;
}
