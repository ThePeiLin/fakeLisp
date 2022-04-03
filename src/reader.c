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
		FKL_ASSERT(tmp,"fklReadInPattern",__FILE__,__LINE__);
		strcpy(tmp,*prev);
		free(*prev);
		*prev=NULL;
		len=strlen(tmp);
	}
	else
	{
		tmp=(char*)malloc(sizeof(char)*1);
		FKL_ASSERT(tmp,"fklReadInPattern",__FILE__,__LINE__);
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
	FKL_ASSERT(tmp,"readString",__FILE__,__LINE__);
	int32_t strSize=0;
	int ch=getc(fp);
	for(;ch!=EOF;ch=getc(fp))
	{
		strSize++;
		if(strSize>memSize-1)
		{
			tmp=(char*)realloc(tmp,sizeof(char)*(memSize+FKL_MAX_STRING_SIZE));
			FKL_ASSERT(tmp,"readString",__FILE__,__LINE__);
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
				FKL_ASSERT(tmp,"readString",__FILE__,__LINE__);
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
	FKL_ASSERT(tmp,"readString",__FILE__,__LINE__);
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

char* fklReadLine(FILE* fp,int* eof)
{
	int32_t memSize=FKL_MAX_STRING_SIZE;
	char* tmp=(char*)malloc(sizeof(char)*memSize);
	FKL_ASSERT(tmp,"readString",__FILE__,__LINE__);
	int32_t strSize=0;
	int ch=getc(fp);
	if(ch==EOF)
		*eof=1;
	for(;;)
	{
		if(ch==EOF)
			break;
		strSize++;
		if(strSize>memSize-1)
		{
			tmp=(char*)realloc(tmp,sizeof(char)*(memSize+FKL_MAX_STRING_SIZE));
			FKL_ASSERT(tmp,"readString",__FILE__,__LINE__);
			memSize+=FKL_MAX_STRING_SIZE;
		}
		tmp[strSize-1]=ch;
		if(ch=='\n')
			break;
		ch=getc(fp);
	}
	tmp[strSize]='\0';
	memSize=strlen(tmp)+1;
	tmp=(char*)realloc(tmp,sizeof(char)*memSize);
	FKL_ASSERT(tmp,"readString",__FILE__,__LINE__);
	return tmp;
}

char* fklReadInStringPattern(FILE* fp,char** prev,int* unexpectEOF)
{
	char* tmp=NULL;
	size_t len=0;
	*unexpectEOF=0;
	if(*prev)
	{
		tmp=(char*)malloc(sizeof(char)*(strlen(*prev)+1));
		FKL_ASSERT(tmp,"fklReadInStringPattern",__FILE__,__LINE__);
		strcpy(tmp,*prev);
		free(*prev);
		*prev=NULL;
		len=strlen(tmp);
	}
	else
	{
		tmp=(char*)malloc(sizeof(char)*1);
		FKL_ASSERT(tmp,"fklReadInStringPattern",__FILE__,__LINE__);
		tmp[0]='\0';
	}
	for(;;)
	{
		int eof=0;
		char* next=fklReadLine(fp,&eof);
		len=strlen(tmp);
		tmp=fklStrCat(tmp,next);
		free(next);
		char* strs[]={tmp};
		uint32_t i=0,j=0;
		int r=fklSplitStringPartsIntoToken(strs,1,0,NULL,&i,&j);
		if(r==0)
		{
			size_t nextLen=strlen(tmp+j);
			if(nextLen)
			{
				tmp[j+nextLen-1]='\0';
				*prev=fklCopyStr(tmp+j);
			}
			tmp[j]='\0';
			len=strlen(tmp);
			char* rt=(char*)realloc(tmp,sizeof(char)*(len+1));
			FKL_ASSERT(rt,"fklReadInStringPattern",__FILE__,__LINE__);
			tmp=rt;
			if(!fklIsAllSpace(tmp)||eof)
				break;
		}
		else if(r==1&&eof)
		{
			*unexpectEOF=1;
			free(tmp);
			tmp=NULL;
			break;
		}
		else if(r==2)
		{
			*unexpectEOF=2;
			free(tmp);
			tmp=NULL;
			break;
		}
	}
	return tmp;
}
