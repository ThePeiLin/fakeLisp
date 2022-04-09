#include<fakeLisp/reader.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/parser.h>
#include<string.h>
#include<stdlib.h>
#include<ctype.h>

static int32_t fklCountInPatternWhenReading(const char*,FklStringMatchPattern*);
static char* readString(FILE*);
static int32_t matchStringPattern(const char*,FklStringMatchPattern* pattern);

char* fklReadInPattern(FILE* fp,char** prev,int* unexpectEOF)
{
	size_t len=0;
	*unexpectEOF=0;
	FklPtrStack* s1=fklNewPtrStack(32,16);
	FklPtrStack* s2=fklNewPtrStack(32,16);
	char* tmp=NULL;
	if(*prev)
	{
		tmp=(char*)malloc(sizeof(char)*(strlen(*prev)+1));
		FKL_ASSERT(tmp,"fklReadInPattern");
		strcpy(tmp,*prev);
		free(*prev);
		*prev=NULL;
		len=strlen(tmp);
	}
	else
	{
		tmp=(char*)malloc(sizeof(char)*1);
		FKL_ASSERT(tmp,"fklReadInPattern");
		tmp[0]='\0';
	}
	size_t i=fklSkipSpace(tmp);
	if((tmp[i]=='(')||(!strncmp(tmp+i,"#b",2)||!strncmp(tmp+i,"#\\",2))||(fklMaybePatternPrefix(tmp+i)))
	{
		FklStringMatchPattern* pattern=fklFindStringPattern(tmp+i);
		fklPushPtrStack(pattern,s1);
		fklPushPtrStack((void*)i,s2);
	}
	char chs[2]={'\0'};
	int ch='\0';
	for(;;)
	{
		ch=getc(fp);
		chs[0]=ch;
		if(!fklIsPtrStackEmpty(s1)&&!fklTopPtrStack(s1)&&tmp[(size_t)fklTopPtrStack(s2)]=='#')
		{
			if(ch==EOF)
			{
				*unexpectEOF=1;
				break;
			}
			size_t i=(size_t)fklTopPtrStack(s2);
			if((!strncmp(tmp+i,"#b",2)&&!isxdigit(ch))
					||(!strncmp(tmp+i,"#\\",2)
						&&(isspace(ch)
							||(len-i>2&&ch!='\\'&&tmp[i+2]!='\\')
							||(len-i>3&&!strncmp(tmp+i,"#\\\\",3))
							||(len-i>4
								&&len-i<(6+tmp[i+3]=='0')
								&&((isdigit(tmp[i+3])&&!isdigit(ch))
									||(toupper(tmp[i+3])=='X'&&!isxdigit(ch)))))))
			{
				fklPopPtrStack(s2);
				fklPopPtrStack(s1);
			}
		}
		if(fklIsPtrStackEmpty(s1)||fklTopPtrStack(s1)||tmp[(size_t)fklTopPtrStack(s2)]!='#'||strncmp(tmp+(size_t)fklTopPtrStack(s2),"#\\",2))
		{
			if(ch=='(')
			{
				if(s1->top==0&&len&&!isspace(tmp[len-1]))
				{
					*prev=fklCopyStr(chs);
					break;
				}
				tmp=fklStrCat(tmp," ");
				len++;
				fklPushPtrStack(NULL,s1);
				fklPushPtrStack((void*)len,s2);
			}
			else if(ch==')')
			{
				if(!fklIsPtrStackEmpty(s1)&&fklTopPtrStack(s1)&&tmp[(size_t)fklTopPtrStack(s2)]!='(')
				{
					*unexpectEOF=2;
					break;
				}
				else
				{
					fklPopPtrStack(s1);
					fklPopPtrStack(s2);
				}
			}
			else if((ch=='b'||ch=='\\')&&(!fklIsPtrStackEmpty(s1)&&!fklTopPtrStack(s1)&&tmp[(size_t)fklTopPtrStack(s2)]=='#'))
			{
				size_t i=(size_t)fklPopPtrStack(s2);
				char* tmp1=fklCopyStr(tmp+i);
				tmp1=fklStrCat(tmp1,chs);
				tmp[i]='\0';
				if(s1->top==1&&i&&!isspace(tmp[i-1]))
				{
					*prev=tmp1;
					break;
				}
				tmp=fklStrCat(tmp," ");
				len++;
				tmp=fklStrCat(tmp,tmp1);
				len++;
				free(tmp1);
				fklPushPtrStack((void*)i+1,s2);
				continue;
			}
			else if(ch==',')
			{
				tmp=fklStrCat(tmp," ");
				tmp=fklStrCat(tmp,chs);
				tmp=fklStrCat(tmp," ");
				len+=3;
				continue;
			}
			else if(ch=='#')
			{
				fklPushPtrStack(NULL,s1);
				fklPushPtrStack((void*)len,s2);
			}
			else if(ch==';')
			{
				while(getc(fp)!='\n');
				chs[0]='\n';
			}
			else if(ch=='\"')
			{
				char* tmp1=readString(fp);
				tmp=fklStrCat(tmp,chs);
				tmp=fklStrCat(tmp,tmp1);
				len+=strlen(tmp1)+1;
				free(tmp1);
				continue;
			}
			else if((!fklIsPtrStackEmpty(s1)&&!fklTopPtrStack(s1)&&tmp[(size_t)fklTopPtrStack(s2)]=='#'))
			{
				fklPopPtrStack(s1);
				fklPopPtrStack(s2);
			}
		}
		if(ch==EOF||(isspace(ch)&&len&&!isspace(tmp[len-1])&&fklIsPtrStackEmpty(s1)&&fklIsPtrStackEmpty(s2)))
		{
			if(ch==EOF&&s1->top&&s2->top&&s1->top==s2->top)
				*unexpectEOF=1;
			else if(ch!=EOF)
				*prev=fklCopyStr(chs);
			break;
		}
		tmp=fklStrCat(tmp,chs);
		len++;
		if(!(!fklIsPtrStackEmpty(s1)&&!fklTopPtrStack(s1)&&tmp[(size_t)fklTopPtrStack(s2)]=='#')&&fklMaybePatternPrefix(chs))
			fklPushPtrStack((void*)len-1,s2);
		if(s1->top!=s2->top)
		{
			size_t i=(size_t)fklTopPtrStack(s2);
			FklStringMatchPattern* pattern=fklFindStringPattern(tmp+i);
			if(pattern)
			{
				size_t i=(size_t)fklTopPtrStack(s2);
				if(s1->top==0&&i&&!isspace(tmp[i-1]))
				{
					*prev=fklCopyStr(tmp+i);
					tmp[i]='\0';
					break;
				}
				else
					fklPushPtrStack(pattern,s1);
			}
			else if(!fklMaybePatternPrefix(tmp+(size_t)fklTopPtrStack(s2)))
				fklPopPtrStack(s2);
		}
		else
		{
			while(!fklIsPtrStackEmpty(s1)&&!fklIsPtrStackEmpty(s2)&&s1->top==s2->top&&fklTopPtrStack(s1)&&!matchStringPattern(tmp+(size_t)fklTopPtrStack(s2),fklTopPtrStack(s1)))
			{
				size_t i=(size_t)fklTopPtrStack(s2);
				char* tmp1=fklCopyStr(tmp+i);
				tmp[i]='\0';
				tmp=fklStrCat(tmp," ");
				len++;
				tmp=fklStrCat(tmp,tmp1);
				free(tmp1);
				fklPopPtrStack(s1);
				fklPopPtrStack(s2);
			}
		}
	}
	fklFreePtrStack(s1);
	fklFreePtrStack(s2);
	return tmp;
}

int32_t matchStringPattern(const char* str,FklStringMatchPattern* pattern)
{
	int32_t num=fklCountInPatternWhenReading(str,pattern);
	return (pattern->num-num);
}

char* readString(FILE* fp)
{
	int32_t memSize=FKL_MAX_STRING_SIZE;
	char* tmp=(char*)malloc(sizeof(char)*memSize);
	FKL_ASSERT(tmp,"readString");
	int32_t strSize=0;
	int ch=getc(fp);
	for(;ch!=EOF;ch=getc(fp))
	{
		strSize++;
		if(strSize>memSize-1)
		{
			tmp=(char*)realloc(tmp,sizeof(char)*(memSize+FKL_MAX_STRING_SIZE));
			FKL_ASSERT(tmp,"readString");
			memSize+=FKL_MAX_STRING_SIZE;
		}
		tmp[strSize-1]=ch;
		if(ch=='\\')
		{
			ch=getc(fp);
			strSize++;
			if(strSize>memSize-1)
			{
				tmp=(char*)realloc(tmp,sizeof(char)*(memSize+FKL_MAX_STRING_SIZE));
				FKL_ASSERT(tmp,"readString");
				memSize+=FKL_MAX_STRING_SIZE;
			}
			tmp[strSize-1]=ch;
		}
		else if(ch=='\"')
			break;
	}
	tmp[strSize]='\0';
	memSize=strlen(tmp)+1;
	tmp=(char*)realloc(tmp,sizeof(char)*memSize);
	FKL_ASSERT(tmp,"readString");
	return tmp;
}

int32_t fklCountInPatternWhenReading(const char* str,FklStringMatchPattern* pattern)
{
	int32_t i=0;
	int32_t s=0;
	int32_t len=strlen(str);
	while(i<pattern->num)
	{
		if(s>=len)break;
		char* part=pattern->parts[i];
		if(!fklIsVar(part))
		{
			s+=fklSkipSpace(str+s);
			if(strncmp(str+s,part,strlen(part)))
				break;
			s+=strlen(part);
		}
		else
		{
			s+=fklSkipSpace(str+s);
			if(str[s]==')')break;
			char* next=(i+1<pattern->num)?pattern->parts[i+1]:NULL;
			s+=fklSkipUntilNextWhenReading(str+s,next);
		}
		s+=fklSkipSpace(str+s);
		i++;
	}
	return i;
}

char* fklReadLine(FILE* fp,size_t* size,int* eof)
{
	int32_t memSize=FKL_MAX_STRING_SIZE;
	char* tmp=(char*)malloc(sizeof(char)*memSize);
	FKL_ASSERT(tmp,"readString");
	int ch=getc(fp);
	if(ch==EOF)
		*eof=1;
	for(;;)
	{
		if(ch==EOF)
			break;
		(*size)++;
		if((*size)>memSize)
		{
			tmp=(char*)realloc(tmp,sizeof(char)*(memSize+FKL_MAX_STRING_SIZE));
			FKL_ASSERT(tmp,"readString");
			memSize+=FKL_MAX_STRING_SIZE;
		}
		tmp[*size-1]=ch;
		if(ch=='\n')
			break;
		ch=getc(fp);
	}
	tmp=(char*)realloc(tmp,sizeof(char)*(*size));
	FKL_ASSERT(!*size||tmp,"fklReadLine");
	return tmp;
}

char* fklReadInStringPattern(FILE* fp,char** prev,size_t* size,size_t* prevSize,uint32_t curline,int* unexpectEOF,FklPtrStack* retval,char* (*read)(FILE*,size_t*,int*))
{
	char* tmp=NULL;
	*unexpectEOF=0;
	FklPtrStack* matchStateStack=fklNewPtrStack(32,16);
	if(!read)
		read=fklReadLine;
	if(*prev)
	{
		tmp=(char*)malloc(sizeof(char)*(*prevSize));
		FKL_ASSERT(tmp,"fklReadInStringPattern");
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
	int eof=0;
	for(;;)
	{
		char* strs[]={tmp+cpost};
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
			FKL_ASSERT(!*size||rt,"fklReadInStringPattern");
			tmp=rt;
			if((!fklIsAllSpaceBufSize(tmp,*size)&&!fklIsAllComment(retval))||eof)
			{
				curline=line;
				break;
			}
		}
		else if(r==1&&eof)
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
		char* next=read(fp,&nextSize,&eof);
		tmp=(char*)realloc(tmp,sizeof(char)+(*size+nextSize));
		FKL_ASSERT(tmp,"fklReadInStringPattern");
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
