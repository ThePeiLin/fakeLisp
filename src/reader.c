#include"reader.h"
#include"common.h"
#include<string.h>
#include<stdlib.h>
#include<ctype.h>

#define FREE_ALL_LINE_NUMBER_TABLE(l,s) {int32_t i=0;\
	for(;i<(s);i++)\
	freeLineNumTabNode((l)[i]);\
}

static StringMatchPattern* HeadOfStringPattern=NULL;
static int32_t skipUntilNextWhenReading(const char* str,const char* part);
static int32_t countInPatternWhenReading(const char*,StringMatchPattern*);
static char* readString(FILE*);
static char* strCat(char*,const char*);
static int32_t matchStringPattern(const char*,StringMatchPattern* pattern);
static int maybePatternPrefix(const char*);
static int32_t countStringParts(const char*);
static int32_t* matchPartOfPattern(const char*,StringMatchPattern*,int32_t*);

char** splitPattern(const char* str,int32_t* num)
{
	int i=0;
	int32_t count=countStringParts(str);
	*num=count;
	char** tmp=(char**)malloc(sizeof(char*)*count);
	FAKE_ASSERT(tmp,"splitPattern",__FILE__,__LINE__);
	count=0;
	for(;str[i]!='\0';i++)
	{
		if(isspace(str[i]))
			i+=skipSpace(str+i);
		if(str[i]=='(')
		{
			int j=i;
			for(;str[j]!='\0'&&str[j]!=')';j++);
			char* tmpStr=(char*)malloc(sizeof(char)*(j-i+2));
			FAKE_ASSERT(tmpStr,"splitPattern",__FILE__,__LINE__);
			memcpy(tmpStr,str+i,j-i+1);
			tmpStr[j-i+1]='\0';
			tmp[count]=tmpStr;
			i=j;
			count++;
			continue;
		}
		int j=i;
		for(;str[j]!='\0'&&str[j]!='(';j++);
		char* tmpStr=(char*)malloc(sizeof(char)*(j-i+1));
		FAKE_ASSERT(tmpStr,"splitPattern",__FILE__,__LINE__);
		memcpy(tmpStr,str+i,j-i);
		tmpStr[j-i]='\0';
		tmp[count]=tmpStr;
		count++;
		i=j-1;
		continue;
	}
	return tmp;
}

char* getVarName(const char* str)
{
	int i=(str[1]==',')?2:1;
	int j=i;
	for(;str[i]!='\0'&&str[i]!=')';i++);
	char* tmp=(char*)malloc(sizeof(char)*(i-j+1));
	FAKE_ASSERT(tmp,"getVarName",__FILE__,__LINE__);
	memcpy(tmp,str+j,i-j);
	tmp[i-j]='\0';
	return tmp;
}

int32_t countStringParts(const char* str)
{
	int i=0;
	int32_t count=0;
	for(;str[i]!='\0';i++)
	{
		if(isspace(str[i]))
			i+=skipSpace(str+i);
		if(str[i]=='(')
		{
			int j=i;
			for(;str[j]!='\0'&&str[j]!=')';j++);
			i=j;
			count++;
			continue;
		}
		int j=i;
		for(;str[j]!='\0'&&str[j]!='(';j++);
		count++;
		i=j-1;
		continue;
	}
	return count;
}

char* readInPattern(FILE* fp,char** prev,int* unexpectEOF)
{
	size_t len=0;
	*unexpectEOF=0;
	ComStack* s1=newComStack(32);
	ComStack* s2=newComStack(32);
	char* tmp=NULL;
	if(*prev)
	{
		tmp=(char*)malloc(sizeof(char)*(strlen(*prev)+1));
		FAKE_ASSERT(tmp,"readInPattern",__FILE__,__LINE__);
		strcpy(tmp,*prev);
		free(*prev);
		*prev=NULL;
		len=strlen(tmp);
	}
	else
	{
		tmp=(char*)malloc(sizeof(char)*1);
		FAKE_ASSERT(tmp,"readInPattern",__FILE__,__LINE__);
		tmp[0]='\0';
	}
	size_t i=skipSpace(tmp);
	if((tmp[i]=='(')||(!strncmp(tmp+i,"#b",2)||!strncmp(tmp+i,"#\\",2))||(maybePatternPrefix(tmp+i)))
	{
		StringMatchPattern* pattern=findStringPattern(tmp+i);
		pushComStack(pattern,s1);
		pushComStack((void*)i,s2);
	}
	char chs[2]={'\0'};
	int ch='\0';
	for(;;)
	{
		ch=getc(fp);
		chs[0]=ch;
		if(!isComStackEmpty(s1)&&!topComStack(s1)&&tmp[(size_t)topComStack(s2)]=='#')
		{
			if(ch==EOF)
			{
				*unexpectEOF=1;
				break;
			}
			size_t i=(size_t)topComStack(s2);
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
				popComStack(s2);
				popComStack(s1);
			}
		}
		if(isComStackEmpty(s1)||topComStack(s1)||tmp[(size_t)topComStack(s2)]!='#'||strncmp(tmp+(size_t)topComStack(s2),"#\\",2))
		{
			if(ch=='(')
			{
				if(s1->top==0&&len&&!isspace(tmp[len-1]))
				{
					*prev=copyStr(chs);
					break;
				}
				tmp=strCat(tmp," ");
				len++;
				pushComStack(NULL,s1);
				pushComStack((void*)len,s2);
			}
			else if(ch==')')
			{
				if(!isComStackEmpty(s1)&&topComStack(s1)&&tmp[(size_t)topComStack(s2)]!='(')
				{
					*unexpectEOF=2;
					break;
				}
				else
				{
					popComStack(s1);
					popComStack(s2);
				}
			}
			else if((ch=='b'||ch=='\\')&&(!isComStackEmpty(s1)&&!topComStack(s1)&&tmp[(size_t)topComStack(s2)]=='#'))
			{
				size_t i=(size_t)popComStack(s2);
				char* tmp1=copyStr(tmp+i);
				tmp1=strCat(tmp1,chs);
				tmp[i]='\0';
				if(s1->top==1&&i&&!isspace(tmp[i-1]))
				{
					*prev=tmp1;
					break;
				}
				tmp=strCat(tmp," ");
				len++;
				tmp=strCat(tmp,tmp1);
				len++;
				free(tmp1);
				pushComStack((void*)i+1,s2);
				continue;
			}
			else if(ch==',')
			{
				tmp=strCat(tmp," ");
				tmp=strCat(tmp,chs);
				tmp=strCat(tmp," ");
				len+=3;
				continue;
			}
			else if(ch=='#')
			{
				pushComStack(NULL,s1);
				pushComStack((void*)len,s2);
			}
			else if(ch==';')
			{
				while(getc(fp)!='\n');
				chs[0]='\n';
			}
			else if(ch=='\"')
			{
				char* tmp1=readString(fp);
				tmp=strCat(tmp,chs);
				tmp=strCat(tmp,tmp1);
				len+=strlen(tmp1)+1;
				free(tmp1);
				continue;
			}
			else if((!isComStackEmpty(s1)&&!topComStack(s1)&&tmp[(size_t)topComStack(s2)]=='#'))
			{
				popComStack(s1);
				popComStack(s2);
			}
		}
		if(ch==EOF||(isspace(ch)&&len&&!isspace(tmp[len-1])&&isComStackEmpty(s1)&&isComStackEmpty(s2)))
		{
			if(ch==EOF&&s1->top&&s2->top&&s1->top==s2->top)
				*unexpectEOF=1;
			else if(ch!=EOF)
				*prev=copyStr(chs);
			break;
		}
		tmp=strCat(tmp,chs);
		len++;
		if(!(!isComStackEmpty(s1)&&!topComStack(s1)&&tmp[(size_t)topComStack(s2)]=='#')&&maybePatternPrefix(chs))
			pushComStack((void*)len-1,s2);
		if(s1->top!=s2->top)
		{
			size_t i=(size_t)topComStack(s2);
			StringMatchPattern* pattern=findStringPattern(tmp+i);
			if(pattern)
			{
				size_t i=(size_t)topComStack(s2);
				if(s1->top==0&&i&&!isspace(tmp[i-1]))
				{
					*prev=copyStr(tmp+i);
					tmp[i]='\0';
					break;
				}
				else
					pushComStack(pattern,s1);
			}
			else if(!maybePatternPrefix(tmp+(size_t)topComStack(s2)))
				popComStack(s2);
		}
		else
		{
			while(!isComStackEmpty(s1)&&!isComStackEmpty(s2)&&s1->top==s2->top&&topComStack(s1)&&!matchStringPattern(tmp+(size_t)topComStack(s2),topComStack(s1)))
			{
				size_t i=(size_t)topComStack(s2);
				char* tmp1=copyStr(tmp+i);
				tmp[i]='\0';
				tmp=strCat(tmp," ");
				len++;
				tmp=strCat(tmp,tmp1);
				free(tmp1);
				popComStack(s1);
				popComStack(s2);
			}
		}
	}
	freeComStack(s1);
	freeComStack(s2);
	return tmp;
}

int32_t matchStringPattern(const char* str,StringMatchPattern* pattern)
{
	int32_t num=countInPatternWhenReading(str,pattern);
	return (pattern->num-num);
}

char* readString(FILE* fp)
{
	int32_t memSize=MAX_STRING_SIZE;
	char* tmp=(char*)malloc(sizeof(char)*memSize);
	FAKE_ASSERT(tmp,"readString",__FILE__,__LINE__);
	int32_t strSize=0;
	int ch=getc(fp);
	for(;ch!=EOF;ch=getc(fp))
	{
		strSize++;
		if(strSize>memSize-1)
		{
			tmp=(char*)realloc(tmp,sizeof(char)*(memSize+MAX_STRING_SIZE));
			FAKE_ASSERT(tmp,"readString",__FILE__,__LINE__);
			memSize+=MAX_STRING_SIZE;
		}
		tmp[strSize-1]=ch;
		if(ch=='\\')
		{
			ch=getc(fp);
			strSize++;
			if(strSize>memSize-1)
			{
				tmp=(char*)realloc(tmp,sizeof(char)*(memSize+MAX_STRING_SIZE));
				FAKE_ASSERT(tmp,"readString",__FILE__,__LINE__);
				memSize+=MAX_STRING_SIZE;
			}
			tmp[strSize-1]=ch;
		}
		else if(ch=='\"')
			break;
	}
	tmp[strSize]='\0';
	memSize=strlen(tmp)+1;
	tmp=(char*)realloc(tmp,sizeof(char)*memSize);
	FAKE_ASSERT(tmp,"readString",__FILE__,__LINE__);
	return tmp;
}

int32_t skipSpace(const char* str)
{
	int32_t i=0;
	for(;str[i]!='\0'&&isspace(str[i]);i++);
	return i;
}

StringMatchPattern* findStringPattern(const char* str)
{
	if(!HeadOfStringPattern)
		return NULL;
	StringMatchPattern* cur=HeadOfStringPattern;
	str+=skipSpace(str);
	while(cur)
	{
		char* part=cur->parts[0];
		if(!strncmp(str,part,strlen(part)))
			break;
		cur=cur->next;
	}
	return cur;
}

char** splitStringInPattern(const char* str,StringMatchPattern* pattern,int32_t* num)
{
	int i=0;
	int32_t* s=matchPartOfPattern(str,pattern,num);
	char** tmp=(char**)malloc(sizeof(char*)*(pattern->num));
	FAKE_ASSERT(tmp,"splitStringInPattern",__FILE__,__LINE__);
	for(;i<pattern->num;i++)
		tmp[i]=NULL;
	for(i=0;i<*num;i++)
	{
		int32_t strSize=(i+1<*num)?((size_t)(s[i+1]-s[i])):(size_t)skipInPattern(str,pattern)-s[i];
		char* tmpStr=(char*)malloc(sizeof(char)*(strSize+1));
		FAKE_ASSERT(tmpStr,"splitStringInPattern",__FILE__,__LINE__);
		memcpy(tmpStr,str+s[i],strSize);
		tmpStr[strSize]='\0';
		tmp[i]=tmpStr;
	}
	free(s);
	return tmp;
}

int32_t* matchPartOfPattern(const char* str,StringMatchPattern* pattern,int32_t* num)
{
	*num=countInPattern(str,pattern);
	int32_t* splitIndex=(int32_t*)malloc(sizeof(int32_t)*(*num));
	FAKE_ASSERT(splitIndex,"matchPartOfPattern",__FILE__,__LINE__);
	int32_t s=0;
	int32_t i=0;
	for(;i<*num;i++)
	{
		char* part=pattern->parts[i];
		if(!isVar(part))
		{
			s+=skipSpace(str+s);
			if(!strncmp(str+s,part,strlen(part)))
				splitIndex[i]=s;
			s+=strlen(part);
		}
		else
		{
			s+=skipSpace(str+s);
			splitIndex[i]=s;
			char* next=(i+1<pattern->num)?pattern->parts[i+1]:NULL;
			s+=skipUntilNextWhenReading(str+s,next);
		}
	}
	return splitIndex;
}

int32_t countInPattern(const char* str,StringMatchPattern* pattern)
{
	int32_t i=0;
	int32_t s=0;
	int32_t len=strlen(str);
	while(i<pattern->num)
	{
		if(s>=len)break;
		char* part=pattern->parts[i];
		if(!isVar(part))
		{
			s+=skipSpace(str+s);
			if(strncmp(str+s,part,strlen(part)))
				break;
			s+=strlen(part);
		}
		else
		{
			s+=skipSpace(str+s);
			if(str[s]==')')break;
			char* next=(i+1<pattern->num)?pattern->parts[i+1]:NULL;
			s+=skipUntilNextWhenReading(str+s,next);
		}
		s+=skipSpace(str+s);
		i++;
	}
	return i;
}

int32_t countInPatternWhenReading(const char* str,StringMatchPattern* pattern)
{
	int32_t i=0;
	int32_t s=0;
	int32_t len=strlen(str);
	while(i<pattern->num)
	{
		if(s>=len)break;
		char* part=pattern->parts[i];
		if(!isVar(part))
		{
			s+=skipSpace(str+s);
			if(strncmp(str+s,part,strlen(part)))
				break;
			s+=strlen(part);
		}
		else
		{
			s+=skipSpace(str+s);
			if(str[s]==')')break;
			char* next=(i+1<pattern->num)?pattern->parts[i+1]:NULL;
			s+=skipUntilNextWhenReading(str+s,next);
		}
		s+=skipSpace(str+s);
		i++;
	}
	return i;
}


int isVar(const char* part)
{
	if(part[0]=='(')
		return 1;
	return 0;
}

int32_t skipUntilNextWhenReading(const char* str,const char* part)
{
	int32_t s=0;
	while(str[s])
	{
		if(str[s]=='(')
		{
			s+=skipParentheses(str+s);
		}
		else if(str[s]=='\"')
		{
			size_t len=0;
			char* tmpStr=castEscapeCharater(str+s+1,'\"',&len);
			s+=len+1;
			free(tmpStr);
		}
		else if(isspace(str[s]))
		{
			s+=skipSpace(str+s);
			continue;
		}
		else if(str[s]==',')
		{
			if(part)
				s++;
			else
				break;
		}
		else
		{
			StringMatchPattern* pattern=findStringPattern(str+s);
			if(pattern)
			{
				s+=skipInPattern(str+s,pattern);
				s+=skipSpace(str+s);
				continue;
			}
			else if(part&&!isVar(part))
			{
				s+=skipAtom(str+s,part);
				s+=skipSpace(str+s);
				if(!strncmp(str+s,part,strlen(part)))
					break;
			}
			s+=skipAtom(str+s,part?part:"");
		}
		if(!part||isVar(part))
			break;
	}
	return s;
}

int32_t skipUntilNext(const char* str,const char* part)
{
	int32_t s=0;
	while(str[s])
	{
		if(str[s]=='(')
		{
			s+=skipParentheses(str+s);
		}
		else if(str[s]=='\"')
		{
			size_t len=0;
			char* tmpStr=castEscapeCharater(str+s+1,'\"',&len);
			s+=len+1;
			free(tmpStr);
		}
		else if(isspace(str[s]))
		{
			s+=skipSpace(str+s);
			continue;
		}
		else if(str[s]==',')
		{
			break;
		}
		else
		{
			StringMatchPattern* pattern=findStringPattern(str+s);
			if(pattern)
			{
				s+=skipInPattern(str+s,pattern);
				s+=skipSpace(str+s);
				continue;
			}
			else if(part&&!isVar(part))
			{
				s+=skipAtom(str+s,part);
				s+=skipSpace(str+s);
				if(!strncmp(str+s,part,strlen(part)))
					break;
			}
			s+=skipAtom(str+s,part?part:"");
		}
		if(!part||isVar(part))
			break;
	}
	return s;
}

int32_t skipParentheses(const char* str)
{
	int parentheses=0;
	int mark=0;
	int32_t i=0;
	while(str[i]!='\0')
	{
		if(str[i]=='\"'&&(i<1||str[i-1]!='\\'))
			mark^=1;
		if(str[i]=='('&&(i<1||str[i-1]!='\\')&&!mark)
			parentheses++;
		else if(str[i]==')'&&(i<1||str[i-1]!='\\')&&!mark)
			parentheses--;
		i++;
		if(parentheses==0)
			break;
	}
	return i;
}

int32_t skipAtom(const char* str,const char* keyString)
{
	int32_t keyLen=strlen(keyString);
	int32_t i=0;
	i+=skipSpace(str);
	if(str[i]=='\"')
	{
		size_t len=0;
		char* tStr=castEscapeCharater(str+i+1,'\"',&len);
		free(tStr);
		i+=len+1;
	}
	else if(str[i]=='#'&&(str[i+1]=='b'||str[i+1]=='\\'))
	{
		i++;
		if(str[i]=='\\')
		{
			i++;
			if(str[i]!='\\')
				i++;
			else
			{
				i++;
				if(isdigit(str[i]))
				{
					int32_t j=0;
					for(;isdigit(str[i+j])&&j<3;j++);
					i+=j;
				}
				else if(toupper(str[i])=='X')
				{
					i++;
					int32_t j=0;
					for(;isxdigit(str[i+j])&&j<2;j++);
					i+=j;
				}
				else
					i++;
			}
		}
		else if(str[i]=='b')
		{
			i++;
			for(;isxdigit(str[i]);i++);
		}
	}
	else
	{
		for(;str[i]!='\0';i++)
		{
			if(isspace(str[i])||str[i]=='('||str[i]==')'||str[i]==','||!strncmp(str+i,"#b",2)||!strncmp(str+i,"#\\",2)||(keyLen&&!strncmp(str+i,keyString,keyLen)))
				break;
		}
	}
	return i;
}

int32_t skipInPattern(const char* str,StringMatchPattern* pattern)
{
	int32_t i=0;
	int32_t s=0;
	if(pattern)
	{
		for(;i<pattern->num;i++)
		{
			char* part=pattern->parts[i];
			if(!isVar(part))
			{
				s+=skipSpace(str+s);
				s+=strlen(part);
			}
			else
			{
				s+=skipSpace(str+s);
				char* next=(i+1<pattern->num)?pattern->parts[i+1]:NULL;
				s+=skipUntilNextWhenReading(str+s,next);
			}
		}
	}
	else
		s+=skipUntilNext(str+s,NULL);
	return s;
}

void printInPattern(char** strs,StringMatchPattern* pattern,FILE* fp,int32_t count)
{
	int i=0;
	for(;i<pattern->num;i++)
	{
		int j=0;
		for(;j<count;j++)
			putc('\t',fp);
		if(!isVar(pattern->parts[i]))
		{
			char* part=pattern->parts[i];
			fprintf(fp,"%s\n",part);
		}
		else
		{
			StringMatchPattern* nextPattern=findStringPattern(strs[i]);
			if(nextPattern)
			{
				int32_t num=0;
				int32_t j=0;
				char** ss=splitStringInPattern(strs[i],nextPattern,&num);
				printInPattern(ss,nextPattern,fp,count+1);
				for(;j<num;j++)
					free(ss[j]);
				free(ss);
			}
			else
			{
				putc('\t',fp);
				fprintf(fp,"%s\n",strs[i]);
			}
		}
	}
}

void freeStringArry(char** ss,int32_t num)
{
	int i=0;
	for(;i<num;i++)
		free(ss[i]);
	free(ss);
}

char* strCat(char* s1,const char* s2)
{
	s1=(char*)realloc(s1,sizeof(char)*(strlen(s1)+strlen(s2)+1));
	FAKE_ASSERT(s1,"strCat",__FILE__,__LINE__);
	strcat(s1,s2);
	return s1;
}

int isInValidStringPattern(const char* str)
{
	StringMatchPattern* pattern=HeadOfStringPattern;
	int32_t num=0;
	char** parts=splitPattern(str,&num);
	if(isVar(parts[0])||parts[0][0]=='$'||parts[0][0]=='#')
	{
		freeStringArry(parts,num);
		return 1;
	}
	if(pattern)
	{
		while(pattern->prev)
			pattern=pattern->prev;
		while(pattern)
		{
			if(!strcmp(parts[0],pattern->parts[0]))
			{
				freeStringArry(parts,num);
				return 1;
			}
			pattern=pattern->next;
		}
	}
	int i=0;
	for(;i<num;i++)
	{
		if(isVar(parts[i])&&isMustList(parts[i]))
		{
			if(i<num-1)
			{
				if(isVar(parts[i-1])||isVar(parts[i+1]))
				{
					freeStringArry(parts,num);
					return 1;
				}
			}
			else
			{
				freeStringArry(parts,num);
				return 1;
			}
		}
		else if(parts[i][0]=='$'||parts[i][0]=='#')
		{
			freeStringArry(parts,num);
			return 1;
		}
	}
	freeStringArry(parts,num);
	return 0;
}

int isMustList(const char* str)
{
	if(!isVar(str))
		return 0;
	if(str[1]==',')
		return 1;
	return 0;
}

StringMatchPattern* newStringMatchPattern(int32_t num,char** parts,ByteCodelnt* proc)
{
	StringMatchPattern* tmp=(StringMatchPattern*)malloc(sizeof(StringMatchPattern));
	FAKE_ASSERT(tmp,"newStringMatchPattern",__FILE__,__LINE__);
	tmp->num=num;
	tmp->parts=parts;
	tmp->proc=proc;
	tmp->next=NULL;
	tmp->prev=NULL;
	if(!HeadOfStringPattern)
		HeadOfStringPattern=tmp;
	else
	{
		StringMatchPattern* cur=HeadOfStringPattern;
		while(cur)
		{
			int32_t fir=strlen(tmp->parts[0]);
			int32_t sec=strlen(cur->parts[0]);
			if(fir>sec)
			{
				if(!cur->prev)
				{
					tmp->next=HeadOfStringPattern;
					HeadOfStringPattern->prev=tmp;
					HeadOfStringPattern=tmp;
				}
				else
				{
					cur->prev->next=tmp;
					tmp->prev=cur->prev;
					cur->prev=tmp;
					tmp->next=cur;
				}
				break;
			}
			cur=cur->next;
		}
		if(!cur)
		{
			for(cur=HeadOfStringPattern;cur->next;cur=cur->next);
			cur->next=tmp;
			tmp->prev=cur;
		}
	}
//	StringMatchPattern* cur=HeadOfStringPattern;
//	fprintf(stderr,"===\n");
//	while(cur)
//	{
//		fprintf(stderr,"%s\n",cur->parts[0]);
//		cur=cur->next;
//	}
	return tmp;
}

void freeAllStringPattern()
{
	StringMatchPattern* cur=HeadOfStringPattern;
	while(cur)
	{
		StringMatchPattern* prev=cur;
		cur=cur->next;
		freeStringArry(prev->parts,prev->num);
		FREE_ALL_LINE_NUMBER_TABLE(prev->proc->l,prev->proc->ls);
		freeByteCodelnt(prev->proc);
		free(prev);
	}
}

void freeStringPattern(StringMatchPattern* o)
{
	int i=0;
	int32_t num=o->num;
	for(;i<num;i++)
		free(o->parts[i]);
	free(o->parts);
	free(o);
}

int32_t findKeyString(const char* str)
{
	StringMatchPattern* cur=HeadOfStringPattern;
	while(cur)
	{
		int i=0;
		char* keyString=cur->parts[0];
		i+=skipAtom(str+i,keyString);
		if(str[i])
			return i;
		cur=cur->next;
	}
	return skipAtom(str,"");
}

static int maybePatternPrefix(const char* str)
{
	if(!HeadOfStringPattern)
		return 0;
	StringMatchPattern* cur=HeadOfStringPattern;
	str+=skipSpace(str);
	while(cur)
	{
		char* part=cur->parts[0];
		if(strlen(str)&&!strncmp(str,part,strlen(str)))
			return 1;
		cur=cur->next;
	}
	return 0;
}
