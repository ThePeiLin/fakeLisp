#include<fakeLisp/pattern.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/vm.h>
#include<string.h>
#include<ctype.h>
static FklStringMatchPattern* HeadOfStringPattern=NULL;
static int32_t countStringParts(const char*);
static int32_t* matchPartOfPattern(const char*,FklStringMatchPattern*,int32_t*);
static uint32_t countReverseCharNum(uint32_t num,char** parts)
{
	uint32_t retval=0;
	for(uint32_t i=0;i<num;i++)
		if(!fklIsVar(parts[i]))
			retval++;
	return retval;
}


int32_t countStringParts(const char* str)
{
	int i=0;
	int32_t count=0;
	for(;str[i]!='\0';i++)
	{
		if(isspace(str[i]))
			i+=fklSkipSpace(str+i);
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


int fklMaybePatternPrefix(const char* str)
{
	if(!HeadOfStringPattern)
		return 0;
	FklStringMatchPattern* cur=HeadOfStringPattern;
	str+=fklSkipSpace(str);
	while(cur)
	{
		char* part=cur->parts[0];
		if(strlen(str)&&!strncmp(str,part,strlen(str)))
			return 1;
		cur=cur->next;
	}
	return 0;
}

FklStringMatchPattern* fklFindStringPatternBuf(const char* buf,size_t size)
{
	if(!HeadOfStringPattern)
		return NULL;
	FklStringMatchPattern* cur=HeadOfStringPattern;
	while(cur)
	{
		char* part=cur->parts[0];
		if(size>=strlen(part)&&!strncmp(buf,part,strlen(part)))
			break;
		cur=cur->next;
	}
	return cur;
}

FklStringMatchPattern* fklFindStringPattern(const char* str)
{
	if(!HeadOfStringPattern)
		return NULL;
	FklStringMatchPattern* cur=HeadOfStringPattern;
	str+=fklSkipSpace(str);
	while(cur)
	{
		char* part=cur->parts[0];
		if(strlen(str)>=strlen(part)&&!strncmp(str,part,strlen(part)))
			break;
		cur=cur->next;
	}
	return cur;
}

FklStringMatchPattern* fklNewStringMatchPattern(int32_t num,char** parts,FklByteCodelnt* proc)
{
	FklStringMatchPattern* tmp=(FklStringMatchPattern*)malloc(sizeof(FklStringMatchPattern));
	FKL_ASSERT(tmp,__func__);
	tmp->type=FKL_PROC;
	tmp->num=num;
	tmp->reserveCharNum=countReverseCharNum(num,parts);
	tmp->parts=parts;
	tmp->u.bProc=proc;
	tmp->next=NULL;
	tmp->prev=NULL;
	if(!HeadOfStringPattern)
		HeadOfStringPattern=tmp;
	else
	{
		FklStringMatchPattern* cur=HeadOfStringPattern;
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
	return tmp;
}

FklStringMatchPattern* fklNewFStringMatchPattern(int32_t num,char** parts,void(*fproc)(FklVM* exe))
{
	FklStringMatchPattern* tmp=(FklStringMatchPattern*)malloc(sizeof(FklStringMatchPattern));
	FKL_ASSERT(tmp,__func__);
	tmp->type=FKL_DLPROC;
	tmp->num=num;
	tmp->parts=parts;
	tmp->reserveCharNum=countReverseCharNum(num,parts);
	tmp->u.fProc=fproc;
	tmp->next=NULL;
	tmp->prev=NULL;
	if(!HeadOfStringPattern)
		HeadOfStringPattern=tmp;
	else
	{
		FklStringMatchPattern* cur=HeadOfStringPattern;
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
	return tmp;
}

char* fklGetNthReverseCharOfStringMatchPattern(FklStringMatchPattern* pattern,uint32_t nth)
{
	uint32_t i=0;
	uint32_t j=0;
	for(;i<pattern->num&&j<nth+1;i++,j+=!fklIsVar(pattern->parts[i]));
	return (j==nth)?pattern->parts[i]:NULL;
}

char* fklGetNthPartOfStringMatchPattern(FklStringMatchPattern* pattern,uint32_t nth)
{
	if(nth>=pattern->num||nth<0)
		return NULL;
	return pattern->parts[nth];
}

char** fklSplitPattern(const char* str,int32_t* num)
{
	int i=0;
	int32_t count=countStringParts(str);
	*num=count;
	char** tmp=(char**)malloc(sizeof(char*)*count);
	FKL_ASSERT(tmp,__func__);
	count=0;
	for(;str[i]!='\0';i++)
	{
		if(isspace(str[i]))
			i+=fklSkipSpace(str+i);
		if(str[i]=='(')
		{
			int j=i;
			for(;str[j]!='\0'&&str[j]!=')';j++);
			char* tmpStr=(char*)malloc(sizeof(char)*(j-i+2));
			FKL_ASSERT(tmpStr,__func__);
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
		FKL_ASSERT(tmpStr,__func__);
		memcpy(tmpStr,str+i,j-i);
		tmpStr[j-i]='\0';
		tmp[count]=tmpStr;
		count++;
		i=j-1;
		continue;
	}
	return tmp;
}

static int32_t* matchPartOfPattern(const char* str,FklStringMatchPattern* pattern,int32_t* num)
{
	*num=fklCountInPattern(str,pattern);
	int32_t* fklSplitIndex=(int32_t*)malloc(sizeof(int32_t)*(*num));
	FKL_ASSERT(fklSplitIndex,__func__);
	int32_t s=0;
	int32_t i=0;
	for(;i<*num;i++)
	{
		char* part=pattern->parts[i];
		if(!fklIsVar(part))
		{
			s+=fklSkipSpace(str+s);
			if(!strncmp(str+s,part,strlen(part)))
				fklSplitIndex[i]=s;
			s+=strlen(part);
		}
		else
		{
			s+=fklSkipSpace(str+s);
			fklSplitIndex[i]=s;
			char* next=(i+1<pattern->num)?pattern->parts[i+1]:NULL;
			s+=fklSkipUntilNextWhenReading(str+s,next);
		}
	}
	return fklSplitIndex;
}


char** fklSplitStringInPattern(const char* str,FklStringMatchPattern* pattern,int32_t* num)
{
	int i=0;
	int32_t* s=matchPartOfPattern(str,pattern,num);
	char** tmp=(char**)malloc(sizeof(char*)*(pattern->num));
	FKL_ASSERT(tmp,__func__);
	for(;i<pattern->num;i++)
		tmp[i]=NULL;
	for(i=0;i<*num;i++)
	{
		int32_t strSize=(i+1<*num)?((size_t)(s[i+1]-s[i])):(size_t)fklSkipInPattern(str,pattern)-s[i];
		char* tmpStr=(char*)malloc(sizeof(char)*(strSize+1));
		FKL_ASSERT(tmpStr,__func__);
		memcpy(tmpStr,str+s[i],strSize);
		tmpStr[strSize]='\0';
		tmp[i]=tmpStr;
	}
	free(s);
	return tmp;
}

void fklFreeAllStringPattern()
{
	FklStringMatchPattern* cur=HeadOfStringPattern;
	while(cur)
	{
		FklStringMatchPattern* prev=cur;
		cur=cur->next;
		fklFreeStringArry(prev->parts,prev->num);
		if(prev->type==FKL_PROC)
			fklFreeByteCodeAndLnt(prev->u.bProc);
		free(prev);
	}
}

void fklFreeStringPattern(FklStringMatchPattern* o)
{
	int i=0;
	int32_t num=o->num;
	for(;i<num;i++)
		free(o->parts[i]);
	free(o->parts);
	free(o);
}

int32_t fklFindKeyString(const char* str)
{
	FklStringMatchPattern* cur=HeadOfStringPattern;
	while(cur)
	{
		int i=0;
		char* keyString=cur->parts[0];
		i+=fklSkipAtom(str+i,keyString);
		if(str[i])
			return i;
		cur=cur->next;
	}
	return fklSkipAtom(str,"");
}

FklVMvalue* singleArgPattern(FklVM* exe,const char* var,const char* str)
{
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* sym=FKL_MAKE_VM_SYM(fklAddSymbolToGlob(str)->id);
	FklVMvalue* varA=fklFindVMenvNode(fklAddSymbolToGlob(var)->id,runnable->localenv->u.env)->value;
	FklVMvalue* pair=fklNewVMvalue(FKL_PAIR,fklNewVMpair(),exe->heap);
	pair->u.pair->car=sym;
	pair->u.pair->cdr=fklNewVMvalue(FKL_PAIR,fklNewVMpair(),exe->heap);
	pair->u.pair->cdr->u.pair->car=varA;
	return pair;
}

void READER_MACRO_quote(FklVM* exe)
{
	FKL_SET_RETURN("READER_MACRO_quote",singleArgPattern(exe,"a","quote"),exe->stack);
}

void READER_MACRO_qsquote(FklVM* exe)
{
	FKL_SET_RETURN("READER_MACRO_qsquote",singleArgPattern(exe,"a","qsquote"),exe->stack);
}

void READER_MACRO_unquote(FklVM* exe)
{
	FKL_SET_RETURN("READER_MACRO_unquote",singleArgPattern(exe,"a","unquote"),exe->stack);
}

void READER_MACRO_unqtesp(FklVM* exe)
{
	FKL_SET_RETURN("READER_MACRO_unqtesp",singleArgPattern(exe,"a","unqtesp"),exe->stack);
}

FklStringMatchPattern* addBuiltInStringPattern(const char* str,void(*fproc)(FklVM* exe))
{
	int32_t num=0;
	char** parts=fklSplitPattern(str,&num);
	FklStringMatchPattern* tmp=fklNewFStringMatchPattern(num,parts,fproc);
	return tmp;
}

void fklInitBuiltInStringPattern(void)
{
	addBuiltInStringPattern("'(a)",READER_MACRO_quote);
	addBuiltInStringPattern("`(a)",READER_MACRO_qsquote);
	addBuiltInStringPattern("~(a)",READER_MACRO_unquote);
	addBuiltInStringPattern("~@(a)",READER_MACRO_unqtesp);
}

int fklIsInValidStringPattern(const char* str)
{
	FklStringMatchPattern* pattern=HeadOfStringPattern;
	int32_t num=0;
	char** parts=fklSplitPattern(str,&num);
	if(fklIsVar(parts[0])||parts[0][0]=='\"')
	{
		fklFreeStringArry(parts,num);
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
				fklFreeStringArry(parts,num);
				return 1;
			}
			pattern=pattern->next;
		}
	}
	int i=0;
	for(;i<num;i++)
	{
		if(fklIsVar(parts[i])&&fklIsMustList(parts[i]))
		{
			if(i<num-1)
			{
				if(fklIsVar(parts[i+1]))
				{
					fklFreeStringArry(parts,num);
					return 1;
				}
			}
			else
			{
				fklFreeStringArry(parts,num);
				return 1;
			}
		}
		else if(parts[i][0]=='\"'||parts[i][0]==';'||!strncmp(parts[i],"#!",strlen("#!")))
		{
			fklFreeStringArry(parts,num);
			return 1;
		}
	}
	fklFreeStringArry(parts,num);
	return 0;
}

int fklIsReDefStringPattern(const char* str)
{
	FklStringMatchPattern* pattern=HeadOfStringPattern;
	int32_t num=0;
	if(pattern)
	{
		char** parts=fklSplitPattern(str,&num);
		while(pattern->prev)
			pattern=pattern->prev;
		while(pattern)
		{
			int r=1;
			if(!strcmp(parts[0],pattern->parts[0]))
			{
				if(pattern->num!=num)
					r=0;
				else
				{
					int32_t i=0;
					for(;i<num;i++)
					{
						if(fklIsVar(pattern->parts[i])&&fklIsVar(parts[i]))
							continue;
						else if(fklIsVar(pattern->parts[i])||fklIsVar(parts[i]))
						{
							r=0;
							break;
						}
						else if(strcmp(parts[i],pattern->parts[i]))
						{
							r=0;
							break;
						}
					}
				}
				fklFreeStringArry(parts,num);
				return r;
			}
			pattern=pattern->next;
		}
		fklFreeStringArry(parts,num);
		return 0;
	}
	else
		return 0;
}

int fklIsMustList(const char* str)
{
	if(!fklIsVar(str))
		return 0;
	if(str[1]==',')
		return 1;
	return 0;
}

int fklIsVar(const char* part)
{
	if(part[0]=='(')
		return 1;
	return 0;
}

void fklFreeStringArry(char** ss,int32_t num)
{
	int i=0;
	for(;i<num;i++)
		free(ss[i]);
	free(ss);
}

size_t fklSkipInPattern(const char* str,FklStringMatchPattern* pattern)
{
	int32_t i=0;
	int32_t s=0;
	if(pattern)
	{
		for(;i<pattern->num;i++)
		{
			char* part=pattern->parts[i];
			if(!fklIsVar(part))
			{
				s+=fklSkipSpace(str+s);
				s+=strlen(part);
			}
			else
			{
				s+=fklSkipSpace(str+s);
				char* next=(i+1<pattern->num)?pattern->parts[i+1]:NULL;
				s+=fklSkipUntilNextWhenReading(str+s,next);
			}
		}
	}
	else
		s+=fklSkipUntilNext(str+s,NULL);
	return s;
}

size_t fklSkipSpace(const char* str)
{
	size_t i=0;
	for(;str[i]!='\0'&&isspace(str[i]);i++);
	return i;
}

int32_t fklCountInPattern(const char* str,FklStringMatchPattern* pattern)
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

size_t fklSkipUntilNext(const char* str,const char* part)
{
	int32_t s=0;
	while(str[s])
	{
		if(str[s]=='(')
		{
			s+=fklSkipParentheses(str+s);
		}
		else if(str[s]=='\"')
		{
			size_t len=0;
			char* tmpStr=fklCastEscapeCharater(str+s+1,'\"',&len);
			s+=len+1;
			free(tmpStr);
		}
		else if(isspace(str[s]))
		{
			s+=fklSkipSpace(str+s);
			continue;
		}
		else if(str[s]==',')
		{
			break;
		}
		else
		{
			FklStringMatchPattern* pattern=fklFindStringPattern(str+s);
			if(pattern)
			{
				s+=fklSkipInPattern(str+s,pattern);
				s+=fklSkipSpace(str+s);
				continue;
			}
			else if(part&&!fklIsVar(part))
			{
				s+=fklSkipAtom(str+s,part);
				s+=fklSkipSpace(str+s);
				if(!strncmp(str+s,part,strlen(part)))
					break;
			}
			s+=fklSkipAtom(str+s,part?part:"");
		}
		if(!part||fklIsVar(part))
			break;
	}
	return s;
}

size_t fklSkipParentheses(const char* str)
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

size_t fklSkipAtom(const char* str,const char* keyString)
{
	int32_t keyLen=strlen(keyString);
	int32_t i=0;
	i+=fklSkipSpace(str);
	if(str[i]=='\"')
	{
		size_t len=0;
		char* tStr=fklCastEscapeCharater(str+i+1,'\"',&len);
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

char* fklGetVarName(const char* str)
{
	int i=(str[1]==',')?2:1;
	int j=i;
	for(;str[i]!='\0'&&str[i]!=')';i++);
	char* tmp=(char*)malloc(sizeof(char)*(i-j+1));
	FKL_ASSERT(tmp,__func__);
	memcpy(tmp,str+j,i-j);
	tmp[i-j]='\0';
	return tmp;
}

void fklPrintInPattern(char** strs,FklStringMatchPattern* pattern,FILE* fp,int32_t count)
{
	int i=0;
	for(;i<pattern->num;i++)
	{
		int j=0;
		for(;j<count;j++)
			putc('\t',fp);
		if(!fklIsVar(pattern->parts[i]))
		{
			char* part=pattern->parts[i];
			fprintf(fp,"%s\n",part);
		}
		else
		{
			FklStringMatchPattern* nextPattern=fklFindStringPattern(strs[i]);
			if(nextPattern)
			{
				int32_t num=0;
				int32_t j=0;
				char** ss=fklSplitStringInPattern(strs[i],nextPattern,&num);
				fklPrintInPattern(ss,nextPattern,fp,count+1);
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

size_t fklSkipUntilNextWhenReading(const char* str,const char* part)
{
	int32_t s=0;
	while(str[s])
	{
		if(str[s]=='(')
		{
			s+=fklSkipParentheses(str+s);
		}
		else if(str[s]=='\"')
		{
			size_t len=0;
			char* tmpStr=fklCastEscapeCharater(str+s+1,'\"',&len);
			s+=len+1;
			free(tmpStr);
		}
		else if(isspace(str[s]))
		{
			s+=fklSkipSpace(str+s);
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
			FklStringMatchPattern* pattern=fklFindStringPattern(str+s);
			if(pattern)
			{
				s+=fklSkipInPattern(str+s,pattern);
				s+=fklSkipSpace(str+s);
				continue;
			}
			else if(part&&!fklIsVar(part))
			{
				s+=fklSkipAtom(str+s,part);
				s+=fklSkipSpace(str+s);
				if(!strncmp(str+s,part,strlen(part)))
					break;
			}
			s+=fklSkipAtom(str+s,part?part:"");
		}
		if(!part||fklIsVar(part))
			break;
	}
	return s;
}


