#include"reader.h"
#include"tool.h"
#include<string.h>
#include<stdlib.h>
#include<ctype.h>
#define MAX_STRING_SIZE 64
StringMatchPattern* newStringPattern(const char** parts,int32_t num)
{
	StringMatchPattern* tmp=(StringMatchPattern*)malloc(sizeof(StringMatchPattern));
	if(!tmp)errors("newStringPattern",__FILE__,__LINE__);
	tmp->num=num;
	char** tmParts=(char**)malloc(sizeof(char*)*num);
	if(!tmParts)errors("newStringPattern",__FILE__,__LINE__);
	int i=0;
	for(;i<num;i++)
		tmParts[i]=copyStr(parts[i]);
	tmp->parts=tmParts;
	return tmp;
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

ReaderMacro* newReaderMacro(StringMatchPattern* pattern,AST_cptr* expression)
{
	ReaderMacro* tmp=(ReaderMacro*)malloc(sizeof(ReaderMacro));
	if(!tmp)errors("newReaderMacro",__FILE__,__LINE__);
	tmp->pattern=pattern;
	tmp->expression=expression;
	tmp->next=NULL;
	return tmp;
}

void freeReaderMacro(ReaderMacro* tmp)
{
	freeStringPattern(tmp->pattern);
	deleteCptr(tmp->expression);
	free(tmp);
}

char** splitPattern(const char* str,int32_t* num)
{
	int i=0;
	int32_t count=countStringParts(str);
	*num=count;
	char** tmp=(char**)malloc(sizeof(char*)*count);
	if(!tmp)errors("splitPattern",__FILE__,__LINE__);
	count=0;
	for(;str[i]!='\0';i++)
	{
		if(isspace(str[i]))
			i+=skipSpace(str+i);
		if((i-1<1||str[i-1]!='\\')&&str[i]=='[')
		{
			int j=i;
			for(;str[j]!='\0'&&!((j-1<1||str[j-1]!='\\')&&str[j]==']');j++);
			char* tmpStr=(char*)malloc(sizeof(char)*(j-i+2));
			if(!tmpStr)errors("splitPattern",__FILE__,__LINE__);
			memcpy(tmpStr,str+i,j-i+1);
			tmpStr[j-i+1]='\0';
			tmp[count]=tmpStr;
			i=j;
			count++;
			continue;
		}
		int j=i;
		for(;str[j]!='\0'&&(((j-1<1||str[j-1]!='\\')&&str[j]!='[')&&!isspace(str[j]));j++);
		char* tmpStr=(char*)malloc(sizeof(char)*(j-i+1));
		if(!tmpStr)errors("splitPattern",__FILE__,__LINE__);
		memcpy(tmpStr,str+i,j-i);
		tmpStr[j-i]='\0';
		tmp[count]=tmpStr;
		count++;
		i=j-1;
		continue;
	}
	return tmp;
}

char* getKeyString(const char* str)
{
	int i=0;
	for(;str[i]!='\0'&&str[i]!='[';i++);
	char* tmp=(char*)malloc(sizeof(char)*(1+i));
	if(!tmp)errors("getKeyString",__FILE__,__LINE__);
	memcpy(tmp,str,i);
	tmp[i]='\0';
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
		if((i-1<1||str[i-1]!='\\')&&str[i]=='[')
		{
			int j=i;
			for(;str[j]!='\0'&&!((j-1<1||str[j-1]!='\\')&&str[j]==']');j++);
			i=j;
			count++;
			continue;
		}
		int j=i;
		for(;str[j]!='\0'&&(((j-1<1||str[j-1]!='\\')&&str[j]!='[')&&!isspace(str[j]));j++);
		count++;
		i=j-1;
		continue;
	}
	return count;
}
#ifdef success
char* readInPattern(intpr* inter,StringMatchPattern* head)
{
	static char* appendix;
	char* cur=NULL;
	if(appendix)
	{
		char* tmp=appendix;
		appendix=NULL;
		return tmp;
	}
	cur=readSingle(inter->file);
	appendix=readSingle(inter->file);
	StringMatchPattern* curPattern=NULL;
	cur=findStringPattern(cur,head);
	if(!curPattern)curPattern=findStringPattern(appendix,head);
	if(curPattern);
	{
		if(fullMatchStringPattern(cur,curPattern))return cur;
		if(fullMatchStringPattern(appendix,curPattern))
		{
			cur=(char*)realloc(cur,sizeof(char)*(strlen(cur)+strlen(appendix)+1));
			if(!cur)errors("readInPattern",__FILE__,__LINE__);
			strcat(cur,appendix);
			free(appendix);
			appendix=NULL;
		}
	}
	return cur;
}
#endif
char* readSingle(FILE* fp)
{
	char* tmp=NULL;
	char* subStr=NULL;
	int32_t memSize=0;
	int32_t strSize=0;
	int ch=getc(fp);
	while(ch!=EOF)
	{
		if(isspace(ch))
		{
			ungetc(ch,fp);
			if(!tmp)tmp=readSpace(fp);
			else
			{
				char* tmpStr=readSpace(fp);
				tmp=(char*)realloc(tmp,sizeof(char)*(strlen(tmp)+strlen(tmpStr)+1));
				strcat(tmp,tmpStr);
				free(tmpStr);
			}
			strSize=strlen(tmp);
			memSize=strSize+1;
		}
		else if(ch==';')
			skipComment(fp);
		else
		{
			strSize++;
			if(strSize>memSize-1)
			{
				tmp=(char*)realloc(tmp,sizeof(char)*(memSize+MAX_STRING_SIZE));
				if(!tmp)errors("readSingle",__FILE__,__LINE__);
				memSize+=MAX_STRING_SIZE;
			}
			if(ch==')')
			{
				strSize--;
				ungetc(ch,fp);
				break;
			}
			tmp[strSize-1]=ch;
			break;
		}
		ch=getc(fp);
	}
	tmp[strSize]='\0';
	switch(ch)
	{
		case '(':
			subStr=readList(fp);
			break;
		case '\"':
			subStr=readString(fp);
			break;
		case ')':
			memSize=strlen(tmp)+1;
			tmp=(char*)realloc(tmp,sizeof(char)*memSize);
			if(!tmp)errors("readSingle",__FILE__,__LINE__);
			return tmp;
			break;
		default:
			subStr=readAtom(fp);
			break;
	}
	strSize+=strlen(subStr);
	if(strSize>memSize-1)
	{
		tmp=(char*)realloc(tmp,sizeof(char)*(strSize+1));
		if(!tmp)errors("readSingle",__FILE__,__LINE__);
	}
	strcat(tmp,subStr);
	memSize=strlen(tmp)+1;
	tmp=(char*)realloc(tmp,sizeof(char)*memSize);
	if(!tmp)errors("readSingle",__FILE__,__LINE__);
	free(subStr);
	return tmp;
}

void skipComment(FILE* fp)
{
	while(getc(fp)!='\n');
	ungetc('\n',fp);
}

char* readSpace(FILE* fp)
{
	int32_t memSize=MAX_STRING_SIZE;
	char* tmp=(char*)malloc(sizeof(char)*memSize);
	if(!tmp)errors("readSpace",__FILE__,__LINE__);
	int32_t strSize=0;
	int ch=getc(fp);
	for(;ch!=EOF&&isspace(ch);ch=getc(fp))
	{
		strSize++;
		if(strSize>memSize-1)
		{
			tmp=(char*)realloc(tmp,sizeof(char)*(memSize+MAX_STRING_SIZE));
			if(!tmp)errors("readSpace",__FILE__,__LINE__);
			memSize+=MAX_STRING_SIZE;
		}
		tmp[strSize-1]=ch;
	}
	if(ch!=EOF)
		ungetc(ch,fp);
	tmp[strSize]='\0';
	memSize=strlen(tmp)+1;
	tmp=(char*)realloc(tmp,sizeof(char)*memSize);
	if(!tmp)errors("readSpace",__FILE__,__LINE__);
	return tmp;
}

char* readString(FILE* fp)
{
	int32_t memSize=MAX_STRING_SIZE;
	char* tmp=(char*)malloc(sizeof(char)*memSize);
	if(!tmp)errors("readString",__FILE__,__LINE__);
	int32_t strSize=0;
	int ch=getc(fp);
	for(;ch!=EOF;ch=getc(fp))
	{
		strSize++;
		if(strSize>memSize-1)
		{
			tmp=(char*)realloc(tmp,sizeof(char)*(memSize+MAX_STRING_SIZE));
			if(!tmp)errors("readString",__FILE__,__LINE__);
			memSize+=MAX_STRING_SIZE;
		}
		tmp[strSize-1]=ch;
		if(strSize>1&&tmp[strSize-2]!='\\'&&ch=='\"')
			break;
	}
	tmp[strSize]='\0';
	memSize=strlen(tmp)+1;
	tmp=(char*)realloc(tmp,sizeof(char)*memSize);
	if(!tmp)errors("readString",__FILE__,__LINE__);
	return tmp;
}

char* readAtom(FILE* fp)
{
	int32_t memSize=MAX_STRING_SIZE;
	char* tmp=(char*)malloc(sizeof(char)*memSize);
	if(!tmp)errors("readAtom",__FILE__,__LINE__);
	int32_t strSize=0;
	int ch=getc(fp);
	for(;ch!=EOF;ch=getc(fp))
	{
		if(isspace(ch)||((strSize-1<0||tmp[strSize-1]!='\\')&&(ch==';'||ch==')'||ch=='('||ch=='\"')))
		{
			ungetc(ch,fp);
			break;
		}
		strSize++;
		if(strSize>memSize-1)
		{
			tmp=(char*)realloc(tmp,sizeof(char)*(memSize+MAX_STRING_SIZE));
			if(!tmp)errors("readAtom",__FILE__,__LINE__);
			memSize+=MAX_STRING_SIZE;
		}
		tmp[strSize-1]=ch;
	}
	tmp[strSize]='\0';
	memSize=strlen(tmp)+1;
	tmp=(char*)realloc(tmp,sizeof(char)*memSize);
	if(!tmp)errors("readAtom",__FILE__,__LINE__);
	return tmp;
}

char* readList(FILE* fp)
{
	char* tmp=NULL;
	int ch=getc(fp);
	while(ch!=EOF)
	{
		if(ch==')')
		{
			if(!tmp)
			{
				tmp=(char*)malloc(sizeof(char)*2);
				if(!tmp)errors("readList",__FILE__,__LINE__);
				strcpy(tmp,")");
			}
			else
			{
				tmp=(char*)realloc(tmp,sizeof(char)*(strlen(tmp)+2));
				if(!tmp)errors("readList",__FILE__,__LINE__);
				strcat(tmp,")");
			}
			break;
		}
		else
		{
			ungetc(ch,fp);
			if(!tmp)
				tmp=readSingle(fp);
			else
			{
				char* tmpStr=readSingle(fp);
				tmp=(char*)realloc(tmp,sizeof(char)*(strlen(tmp)+strlen(tmpStr)+1));
				if(!tmp)errors("readList",__FILE__,__LINE__);
				strcat(tmp,tmpStr);
				free(tmpStr);
			}
		}
		ch=getc(fp);
	}
	return tmp;
}

int32_t skipSpace(const char* str)
{
	int32_t i=0;
	for(;str[i]!='\0'&&isspace(str[i]);i++);
	return i;
}
#ifdef success
StringMatchPattern* findStringPattern(const char* str,StringMatchPattern* head)
{
	while(head!=NULL)
	{
		char* part=head->parts[0];
		char* keyString=castKeyStringToNormalString(part);
		if(!strncmp(str,keyString,strlen(keyString)));
			break;
		head=head->next;
		free(KeyString);
	}
	return head;
}

int fullMatchStringPattern(StringMatchPattern** patterns,int32_t num)
{
}

int matchStringPattern(const char* str,StringMatchPattern* pattern)
{
}

char** splitStringInPattern(const char* str,StringMatchPattern* pattern,int32_t* num)
{
	int i=0;
	*num=pattern->num;
	char** tmp=(char*)malloc(sizeof(char*)*pattern->num);
	if(!tmp)errors("splitStringInPattern",__FILE__,__LINE__);
	int32_t* s=matchPartOfPattern(str,pattern);
}

int32_t* matchPartOfPattern(const char* str,StringMatchPattern* pattern)
{
	int32_t* splitIndex=(int32_t*)malloc(sizeof(int32_t)*(pattern->num));
	if(!splitIndex)errors("matchPartOfPattern",__FILE__,__LINE__);
	int32_t s=0;
	int32_t i=0;
	for(;i<pattern->num;i++)
	{
		char* part=pattern->parts[i];
		if(isKeyString(part))
		{
			char* keyString=castKeyStringToNormalString(part);
			s=skipSpace(str+s);
			if(!strncmp(str+s,keyString,strlen(keyString)))
				splitIndex[i]=s;
			s+=strlen(part);
		}
		else
		{
			s=skipSpace(str+s);
			splitIndex[i]=s;
			s+=skipSingleUntilNext(str+s,pattern->parts[i+1]);
		}
	}
	return splitIndex;
}

int isKeyString(StringMatchPattern* pattern,int32_t count)
{
	char* part=pattern->parts[count];
	if(part[0]=='[')
		return 1;
	return 0;
}

int32_t skipSingleUntilNext(const char* str,const char* part)
{
	if(str[0]=='(')
		return skipParentheses(str);
	else if(str[0]=='\"')
		return skipString(str);
	else
		return skipAtom(str,part[0]);
}

int32_t skipParentheses(const char* str)
{
	int parentheses=0;
	int mark=0;
	int32_t i=0;
	for(;str[i]!='\0';i++)
	{
		if(str[i]=='\"'&&(i<1||str[i-1]!='\\'))
			mark^=1;
		if(str[i]=='('&&(i<1||str[i-1]!='\\')&&!mark)
			parentheses++;
		else if(str[i]==')'&&(i<1||str[i-1]!='\\')&&!mark)
			parentheses--;
		if(parentheses==0)
			break;
	}
	return i;
}

int32_t skipAtom(const char* str,char ch)
{
	int32_t i=0;
	for(;str[i]!=0;i++)
	{
		if(str[i]==ch&&(i<1||str[i-1]!='\\'))
			break;
	}
	return i;
}
#endif

char* castKeyStringToNormalString(const char* str)
{
	int32_t strSize=0;
	int32_t memSize=MAX_STRING_SIZE;
	int32_t i=1;
	char* tmp=(char*)malloc(sizeof(char)*memSize);
	while(str[i]!=']')
	{
		int ch=0;
		if(str[i]=='\\')
		{
			if(isdigit(str[i+1]))
			{
				if(str[i+1]=='0')
				{
					if(isdigit(str[i+2]))
					{
						int len=0;
						while((isdigit(str[i+2+len])&&(str[i+2+len]<'8')&&len<4))len++;
						sscanf(str+i+1,"%4o",&ch);
						i+=len+2;
					}

					else if(toupper(str[i+2])=='X')
					{
						int len=0;
						while(isxdigit(str[i+3+len])&&len<2)len++;
						sscanf(str+i+1,"%4x",&ch);
						i+=len+3;
					}
				}
				else if(isdigit(str[i+1]))
				{
					int len=0;
					while(isdigit(str[i+1+len])&&len<4)len++;
					sscanf(str+i+1,"%4d",&ch);
					i+=len+1;
				}
			}
			else if(str[i+1]=='\n')
			{
				i+=2;
				continue;
			}
			else
			{
				switch(toupper(str[i+1]))
				{
					case 'A':ch=0x07;break;
					case 'B':ch=0x08;break;
					case 'T':ch=0x09;break;
					case 'N':ch=0x0a;break;
					case 'V':ch=0x0b;break;
					case 'F':ch=0x0c;break;
					case 'R':ch=0x0d;break;
					case '\\':ch=0x5c;break;
					default:ch=str[i+1];break;
				}
				i+=2;
			}
		}
		else ch=str[i++];
		strSize++;
		if(strSize>memSize-1)
		{
			
			tmp=(char*)realloc(tmp,sizeof(char)*(memSize+MAX_STRING_SIZE));
			if(!tmp)errors("castKeyStringToNormalString",__FILE__,__LINE__);
			memSize+=MAX_STRING_SIZE;

		}
		tmp[strSize-1]=ch;
	}
	if(tmp)tmp[strSize]='\0';
	memSize=strlen(tmp)+1;
	tmp=(char*)realloc(tmp,memSize*sizeof(char));
	if(!tmp)errors("castKeyStringToNormalString",__FILE__,__LINE__);
	return tmp;
}
