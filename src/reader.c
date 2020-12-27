#include"reader.h"
#include"tool.h"
#include"compiler.h"
#include<string.h>
#include<stdlib.h>
#include<ctype.h>

static StringMatchPattern* HeadOfStringPattern=NULL;

StringMatchPattern* addStringPattern(char** parts,int32_t num,AST_cptr* express,intpr* inter)
{
	StringMatchPattern* tmp=NULL;
	ErrorStatus status={0,NULL};
	CompEnv* tmpGlobCompEnv=newCompEnv(NULL);
	initCompEnv(tmpGlobCompEnv);
	intpr* tmpInter=newTmpIntpr(NULL,NULL);
	tmpInter->filename=inter->filename;
	tmpInter->curline=inter->curline;
	tmpInter->glob=tmpGlobCompEnv;
	tmpInter->head=inter->head;
	tmpInter->tail=inter->tail;
	tmpInter->modules=inter->modules;
	tmpInter->curDir=inter->curDir;
	tmpInter->procs=NULL;
	tmpInter->prev=NULL;
	CompEnv* tmpCompEnv=createPatternCompEnv(parts,num,tmpGlobCompEnv);
	int32_t bound=(tmpCompEnv->symbols==NULL)?-1:tmpCompEnv->symbols->count;
	ByteCode* tmpByteCode=compile(express,tmpCompEnv,tmpInter,&status);
	if(!status.status)
	{
		tmp=(StringMatchPattern*)malloc(sizeof(StringMatchPattern));
		if(!tmp)errors("newStringPattern",__FILE__,__LINE__);
		tmp->num=num;
		char** tmParts=(char**)malloc(sizeof(char*)*num);
		if(!tmParts)errors("newStringPattern",__FILE__,__LINE__);
		int i=0;
		for(;i<num;i++)
			tmParts[i]=copyStr(parts[i]);
		tmp->parts=tmParts;
		tmp->prev=NULL;
		tmp->next=HeadOfStringPattern;
		if(HeadOfStringPattern)
			HeadOfStringPattern->prev=tmp;
		HeadOfStringPattern=tmp;
		tmp->procs=tmpInter->procs;
		tmp->proc=tmpByteCode;
		tmp->bound=bound;
	}
	else
	{
		if(tmpByteCode)
			freeByteCode(tmpByteCode);
		exError(status.place,status.status,inter);
		status.place=NULL;
		status.status=0;
	}
	destroyCompEnv(tmpCompEnv);
	destroyCompEnv(tmpGlobCompEnv);
	free(tmpInter);
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

char* getVarName(const char* str)
{
	int i=1;
	for(;str[i]!='\0'&&str[i]!=']';i++);
	char* tmp=(char*)malloc(sizeof(char)*(1+i));
	if(!tmp)errors("getVarName",__FILE__,__LINE__);
	memcpy(tmp,str+1,i);
	tmp[i-1]='\0';
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

char* readInPattern(FILE* fp,StringMatchPattern** retval)
{
	char* tmp=readSingle(fp);
	if(!tmp)
		return NULL;
	StringMatchPattern* pattern=findStringPattern(tmp+skipSpace(tmp),HeadOfStringPattern);
	if(retval!=NULL)
		*retval=pattern;
	if(!pattern)
		return tmp;
	while(matchStringPattern(tmp,pattern))
	{
		int32_t num=0;
		int32_t backIndex=strlen(tmp);
		int32_t* splitIndex=matchPartOfPattern(tmp,pattern,&num);
		if(isVar(pattern->parts[num-1]))
		{
			backIndex=splitIndex[num-1];
			ungetString(tmp+backIndex,fp);
		}
		free(splitIndex);
		char* tmpNext=readInPattern(fp,NULL);
		tmp=exStrCat(tmp,tmpNext,backIndex);
		free(tmpNext);
		if(!matchStringPattern(tmp,pattern)&&tmp[strlen(tmp)-1]==')')
			break;
		tmpNext=readSingle(fp);
		tmp=exStrCat(tmp,tmpNext,strlen(tmp));
		free(tmpNext);
	}
	if(!isVar(pattern->parts[pattern->num-1]))
	{
		int32_t num=0;
		int32_t* index=matchPartOfPattern(tmp,pattern,&num);
		char* part=pattern->parts[num-1];
		int32_t finaleIndex=index[num-1]+strlen(part);
		ungetString(tmp+finaleIndex,fp);
		tmp[finaleIndex]='\0';
		int32_t memsize=strlen(tmp)+1;
		tmp=(char*)realloc(tmp,memsize*sizeof(char));
		if(!tmp)errors("readInPattern",__FILE__,__LINE__);
		free(index);
	}
	return tmp;
}

int32_t matchStringPattern(const char* str,StringMatchPattern* pattern)
{
	int32_t num=countInPattern(str,pattern);
	return (pattern->num-num);
}

char* readSingle(FILE* fp)
{
	char* tmp=NULL;
	char* subStr=NULL;
	int32_t memSize=0;
	int32_t strSize=0;
	int ch=getc(fp);
	if(ch==EOF)
		return NULL;
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
				if(!tmp)errors("readSingle",__FILE__,__LINE__);
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
	if(tmp)
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
		if(ch=='\\')
		{
			ch=getc(fp);
			strSize++;
			if(strSize>memSize-1)
			{
				tmp=(char*)realloc(tmp,sizeof(char)*(memSize+MAX_STRING_SIZE));
				if(!tmp)errors("readString",__FILE__,__LINE__);
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

StringMatchPattern* findStringPattern(const char* str,StringMatchPattern* cur)
{
	StringMatchPattern* tmp=(cur==NULL)?HeadOfStringPattern:cur;
	cur=tmp;
	while(cur)
	{
		char* part=cur->parts[0];
		if(!strncmp(str,part,strlen(part)))
			break;
		cur=cur->next;
	}
	if(cur==NULL&&tmp!=NULL)
	{
		cur=tmp->prev;
		while(cur)
		{
			char* part=cur->parts[0];
			if(!strncmp(str,part,strlen(part)))
				break;
			cur=cur->prev;
		}
	}
	return cur;
}

char** splitStringInPattern(const char* str,StringMatchPattern* pattern,int32_t* num)
{
	int i=0;
	int32_t* s=matchPartOfPattern(str,pattern,num);
	char** tmp=(char**)malloc(sizeof(char*)*(pattern->num));
	for(;i<pattern->num;i++)
		tmp[i]=NULL;
	if(!tmp)errors("splitStringInPattern",__FILE__,__LINE__);
	for(i=0;i<*num;i++)
	{
		int32_t strSize=(i+1<*num)?((size_t)(s[i+1]-s[i])):strlen(str)-s[i];
		char* tmpStr=(char*)malloc(sizeof(char)*(strSize+1));
		if(!tmpStr)errors("splitStringInPattern",__FILE__,__LINE__);
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
	if(!splitIndex)errors("matchPartOfPattern",__FILE__,__LINE__);
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
			StringMatchPattern* nextPattern=findStringPattern(str+s,pattern);
			if(nextPattern)
				s+=skipInPattern(str+s,nextPattern);
			else
			{
				char* next=(i+1<pattern->num)?pattern->parts[i+1]:NULL;
				s+=skipUntilNext(str+s,next);
			}
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
			StringMatchPattern* nextPattern=findStringPattern(str+s,pattern);
			if(nextPattern)
				s+=skipInPattern(str+s,nextPattern);
			else
			{
				char* next=(i+1<pattern->num)?pattern->parts[i+1]:NULL;
				s+=skipUntilNext(str+s,next);
			}
		}
		i++;
	}
	return i;
}

int isVar(const char* part)
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
			if(part&&!isVar(part))
			{
				s+=skipAtom(str+s,part);
				if(!strncmp(str+s,part,strlen(part)))
					break;
			}
			s+=skipAtom(str+s,"");
		}
		if(!part||isVar(part))break;
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
		if(isspace(str[i])||(keyLen&&!strncmp(str+i,keyString,keyLen))&&(i<1||str[i-1]!='\\')) break;
	}
	return i;
}

int32_t skipString(const char* str)
{
	int32_t i=0;
	for(;str[i]!=0&&(!((i-1<1||str[i-1]!='\\')&&str[i]=='\"'));i++);
	return i;
}

int32_t skipInPattern(const char* str,StringMatchPattern* pattern)
{
	int32_t i=0;
	int32_t s=0;
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
			StringMatchPattern* nextPattern=findStringPattern(str+s,pattern);
			if(nextPattern)
				s+=skipInPattern(str+s,nextPattern);
			else
			{
				char* next=(i+1<pattern->num)?pattern->parts[i+1]:NULL;
				s+=skipUntilNext(str+s,next);
			}
		}
	}
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
			StringMatchPattern* nextPattern=findStringPattern(strs[i],pattern);
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

char* exStrCat(char* s1,const char* s2,int32_t i)
{
	int32_t len=strlen(s2)+i;
	s1=(char*)realloc(s1,sizeof(char)*(len+1));
	if(!s1)errors("exStrCat",__FILE__,__LINE__);
	strcpy(s1+i,s2);
	return s1;
}

void ungetString(const char* str,FILE* fp)
{
	int32_t i=strlen(str)-1;
	for(;i>-1;i--)
		ungetc(str[i],fp);
}

int isInValidStringPattern(const char* str)
{
	StringMatchPattern* pattern=HeadOfStringPattern;
	int32_t num=0;
	char** parts=splitPattern(str,&num);
	if(isVar(parts[0]))
	{
		freeStringArry(parts,num);
		return 1;
	}
	if(pattern)
	{
		int32_t len1=strlen(parts[0]);
		while(pattern->prev)
			pattern=pattern->prev;
		while(pattern)
		{
			int32_t len2=strlen(pattern->parts[0]);
			int32_t len=(len2>len1)?len1:len2;
			if(!strncmp(parts[0],pattern->parts[0],len))
			{
				freeStringArry(parts,num);
				return 1;
			}
			pattern=pattern->next;
		}
	}
	freeStringArry(parts,num);
	return 0;
}

CompEnv* createPatternCompEnv(char** parts,int32_t num,CompEnv* prev)
{
	if(parts==NULL)return NULL;
	CompEnv* tmpEnv=newCompEnv(prev);
	int32_t i=0;
	for(;i<num;i++)
		if(isVar(parts[i]))
		{
			char* varName=getVarName(parts[i]);
			addCompDef(varName,tmpEnv);
			free(varName);
		}
	return tmpEnv;
}

void freeAllStringPattern()
{
	StringMatchPattern* cur=HeadOfStringPattern;
	while(cur)
	{
		StringMatchPattern* prev=cur;
		cur=cur->next;
		freeStringArry(prev->parts,prev->num);
		freeByteCode(prev->proc);
		RawProc* tmp=prev->procs;
		while(tmp!=NULL)
		{
			RawProc* prev=tmp;
			tmp=tmp->next;
			freeByteCode(prev->proc);
			free(prev);
		}
		free(prev);
	}
}
