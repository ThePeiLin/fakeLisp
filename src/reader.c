#include"reader.h"
#include"tool.h"
#include<string.h>
#include<stdlib.h>
#include<ctype.h>
#define MAX_STRING_SIZE 64
StringMatchPattern* newStringPattern(const char** parts,int32_t size)
{
	StringMatchPattern* tmp=(StringMatchPattern*)malloc(sizeof(StringMatchPattern));
	if(!tmp)errors("newStringPattern",__FILE__,__LINE__);
	tmp->size=size;
	char** tmParts=(char**)malloc(sizeof(char*)*size);
	if(!tmParts)errors("newStringPattern",__FILE__,__LINE__);
	int i=0;
	for(;i<size;i++)
		tmParts[i]=copyStr(parts[i]);
	tmp->parts=tmParts;
	return tmp;
}

void freeStringPattern(StringMatchPattern* o)
{
	int i=0;
	int32_t size=o->size;
	for(;i<size;i++)
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

char** splitPattern(const char* str,int32_t* size)
{
	int i=0;
	int32_t count=countStringParts(str);
	*size=count;
	char** tmp=(char**)malloc(sizeof(char*)*count);
	if(!tmp)errors("splitPattern",__FILE__,__LINE__);
	count=0;
	for(;str[i]!='\0';i++)
	{
		if(str[i]=='[')
		{
			int j=i;
			for(;str[j]!='\0'&&str[j]!=']';j++);
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
		for(;str[j]!='\0'&&str[j]!='[';j++);
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
		if(str[i]=='[')
		{
			int j=i;
			for(;str[j]!='\0'&&str[j]!=']';j++);
			i=j;
			count++;
			continue;
		}
		int j=i;
		for(;str[j]!='\0'&&str[j]!='[';j++);
		count++;
		i=j-1;
		continue;
	}
	return count;
}

//char* readInPattern(intpr* inter,StringMatchPattern* head)
//{
//	static char* appendix;
//	char* cur=NULL;
//	if(appendix)
//	{
//		char* tmp=appendix;
//		appendix=NULL;
//		return tmp;
//	}
//	cur=readSingle(inter->file);
//	appendix=readSingle(inter->file);
//	if(findStringPattern(cur)&&findStringPattern(appendix))
//	return cur;
//}

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
		if(isspace(ch)||ch==';'||ch==')'||ch=='(')
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
