#include"reader.h"
#include"tool.h"
#include<string.h>
#include<stdlib.h>
#include<ctype.h>
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

StringMatchPattern* findStringPattern(const char* str,StringMatchPattern* head)
{
	while(head!=NULL)
	{
		char* part=head->parts[0];
		char* keyString=castEscapeCharater(part+1,']');
		if(!strncmp(str,keyString,strlen(keyString)))
			break;
		head=head->next;
		free(keyString);
	}
	return head;
}

#ifdef success
int fullMatchStringPattern(StringMatchPattern** patterns,int32_t num)
{
}

int matchStringPattern(const char* str,StringMatchPattern* pattern)
{
}

#endif
char** splitStringInPattern(const char* str,StringMatchPattern* pattern,int32_t* num)
{
	int i=0;
	int32_t* s=matchPartOfPattern(str,pattern,num);
	char** tmp=(char**)malloc(sizeof(char*)*(*num));
	if(!tmp)errors("splitStringInPattern",__FILE__,__LINE__);
	for(;i<*num;i++)
	{
		int32_t strSize=(i+1<*num)?((size_t)(s[i+1]-s[i])):strlen(str)-s[i];
		char* tmpStr=(char*)malloc(sizeof(char)*(strSize+1));
		if(!tmpStr)errors("splitStringInPattern",__FILE__,__LINE__);
		memcpy(tmpStr,str+s[i],strSize);
		tmpStr[strSize]='\0';
		tmp[i]=tmpStr;
	}
	return tmp;
}

int32_t* matchPartOfPattern(const char* str,StringMatchPattern* pattern,int32_t* num)
{
	int32_t* splitIndex=(int32_t*)malloc(sizeof(int32_t)*(pattern->num));
	if(!splitIndex)errors("matchPartOfPattern",__FILE__,__LINE__);
	int32_t s=0;
	int32_t i=0;
	*num=countInPattern(str,pattern);
	for(;i<*num;i++)
	{
		char* part=pattern->parts[i];
		if(isKeyString(part))
		{
			char* keyString=castEscapeCharater(part+1,']');
			s+=skipSpace(str+s);
			if(!strncmp(str+s,keyString,strlen(keyString)))
				splitIndex[i]=s;
			s+=strlen(keyString);
		}
		else
		{
			s+=skipSpace(str+s);
			splitIndex[i]=s;
			char* next=(i+1<pattern->num)?pattern->parts[i+1]:NULL;
			s+=skipUntilNext(str+s,next);
		}
	}
	return splitIndex;
}

int32_t countInPattern(const char* str,StringMatchPattern* pattern)
{
	int32_t i=0;
	int32_t s=0;
	while(i<pattern->num)
	{
		if(str[s]=='\0')break;
		char* part=pattern->parts[i];
		if(isKeyString(part))
		{
			char* keyString=castEscapeCharater(part+1,']');
			s+=skipSpace(str+s);
			if(strncmp(str+s,keyString,strlen(keyString)))
				break;
			s+=strlen(keyString);
		}
		else
		{
			s+=skipSpace(str+s);
			char* next=(i+1<pattern->num)?pattern->parts[i+1]:NULL;
			s+=skipUntilNext(str+s,next);
		}
		i++;
	}
	return i;
}

int isKeyString(const char* part)
{
	if(part[0]=='[')
		return 1;
	return 0;
}

int32_t skipUntilNext(const char* str,const char* part)
{
	int32_t s=0;
	while(str[s])
	{
		if(str[s]=='(')
			s+=skipParentheses(str+s);
		else if(str[s]=='\"')
			s+=skipString(str+s);
		else if(isspace(str[s]))
			s+=skipSpace(str+s);
		else
		{
			if(part&&isKeyString(part))
			{
				char* keyString=castEscapeCharater(part+1,']');
				s+=skipAtom(str+s,keyString);
				if(!strncmp(str+s,keyString,strlen(keyString)))
					break;
			}
			s+=skipAtom(str+s,"");
		}
		if(!part||!isKeyString(part))break;
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
	for(;str[i]!='\0';i++)
	{
		if(isspace(str[i])||(keyLen&&!strncmp(str+i,keyString,keyLen))&&(i<1||str[i-1]!='\\'))
			break;
	}
	return i;
}

int32_t skipString(const char* str)
{
	int32_t i=0;
	for(;str[i]!=0&&(!((i-1<1||str[i-1]!='\\')&&str[i]=='\"'));i++);
	return i;
}
