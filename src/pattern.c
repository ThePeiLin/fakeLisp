#include<fakeLisp/pattern.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/vm.h>
#include<string.h>
#include<ctype.h>
static FklStringMatchPattern* HeadOfStringPattern=NULL;
static uint32_t countStringParts(const FklString*);
static uint32_t countReverseCharNum(uint32_t num,FklString* const* parts)
{
	uint32_t retval=0;
	for(uint32_t i=0;i<num;i++)
		if(!fklIsVar(parts[i]))
			retval++;
	return retval;
}

static size_t fklSkipSpace(const FklString* str,size_t index)
{
	size_t i=0;
	size_t size=str->size;
	const char* buf=str->str;
	for(;i+index<size&&isspace(buf[i]);i++);
	return i;
}


uint32_t countStringParts(const FklString* str)
{
	uint32_t count=0;
	size_t size=str->size;
	const char* buf=str->str;
	for(size_t i=0;i<size;i++)
	{
		if(isspace(buf[i]))
			i+=fklSkipSpace(str,i);
		if(buf[i]=='(')
		{
			size_t j=i;
			for(;j<size&&buf[j]!=')';j++);
			i=j;
			count++;
			continue;
		}
		size_t j=i;
		for(;j<size&&buf[j]!='(';j++);
		count++;
		i=j-1;
		continue;
	}
	return count;
}


FklStringMatchPattern* fklFindStringPatternBuf(const char* buf,size_t size)
{
	if(!HeadOfStringPattern)
		return NULL;
	FklStringMatchPattern* cur=HeadOfStringPattern;
	while(cur)
	{
		const FklString* part=cur->parts[0];
		if(size>=part->size&&!memcmp(buf,part->str,part->size))
			break;
		cur=cur->next;
	}
	return cur;
}

FklStringMatchPattern* fklFindStringPattern(const FklString* str)
{
	if(!HeadOfStringPattern)
		return NULL;
	FklStringMatchPattern* cur=HeadOfStringPattern;
	str+=fklSkipSpace(str,0);
	while(cur)
	{
		const FklString* part=cur->parts[0];
		if(str->size>=part->size&&!memcmp(str->str,part->str,part->size))
			break;
		cur=cur->next;
	}
	return cur;
}

FklStringMatchPattern* fklNewStringMatchPattern(uint32_t num,FklString** parts,FklByteCodelnt* proc)
{
	FklStringMatchPattern* tmp=(FklStringMatchPattern*)malloc(sizeof(FklStringMatchPattern));
	FKL_ASSERT(tmp);
	tmp->num=num;
	tmp->reserveCharNum=countReverseCharNum(num,parts);
	tmp->parts=parts;
	tmp->proc=proc;
	tmp->next=NULL;
	tmp->prev=NULL;
	if(!HeadOfStringPattern)
		HeadOfStringPattern=tmp;
	else
	{
		FklStringMatchPattern* cur=HeadOfStringPattern;
		while(cur)
		{
			int32_t fir=tmp->parts[0]->size;
			int32_t sec=cur->parts[0]->size;
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

const FklString* fklGetNthReverseCharOfStringMatchPattern(FklStringMatchPattern* pattern,uint32_t nth)
{
	uint32_t i=0;
	uint32_t j=0;
	for(;i<pattern->num&&j<nth+1;i++,j+=!fklIsVar(pattern->parts[i]));
	return (j==nth)?pattern->parts[i]:NULL;
}

const FklString* fklGetNthPartOfStringMatchPattern(FklStringMatchPattern* pattern,uint32_t nth)
{
	if(nth>=pattern->num||nth<0)
		return NULL;
	return pattern->parts[nth];
}

FklString** fklSplitPattern(const FklString* str,uint32_t* num)
{
	uint32_t count=countStringParts(str);
	*num=count;
	FklString** tmp=(FklString**)malloc(sizeof(FklString*)*count);
	FKL_ASSERT(tmp);
	count=0;
	const char* buf=str->str;
	size_t size=str->size;
	for(size_t i=0;i<size;i++)
	{
		if(isspace(buf[i]))
			i+=fklSkipSpace(str,i);
		if(buf[i]=='(')
		{
			size_t j=i;
			for(;j<size&&buf[j]!=')';j++);
			FklString* tmpStr=fklNewString(j-i+1,buf+i);
			tmp[count]=tmpStr;
			i=j;
			count++;
			continue;
		}
		size_t j=i;
		for(;j<size&&buf[j]!='(';j++);
		FklString* tmpStr=fklNewString(j-i,buf+i);
		tmp[count]=tmpStr;
		count++;
		i=j-1;
		continue;
	}
	return tmp;
}

void fklFreeAllStringPattern()
{
	FklStringMatchPattern* cur=HeadOfStringPattern;
	while(cur)
	{
		FklStringMatchPattern* prev=cur;
		cur=cur->next;
		fklFreeStringArray(prev->parts,prev->num);
		fklFreeByteCodeAndLnt(prev->proc);
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

int fklIsInValidStringPattern(FklString* const* parts,uint32_t num)
{
	FklStringMatchPattern* pattern=HeadOfStringPattern;
	if(fklIsVar(parts[0])||parts[0]->str[0]=='\"')
		return 1;
	if(pattern)
	{
		while(pattern->prev)
			pattern=pattern->prev;
		while(pattern)
		{
			if(!fklStringcmp(parts[0],pattern->parts[0]))
				return 1;
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
					return 1;
			}
			else
				return 1;
		}
		else if(parts[i]->str[0]=='\"'||parts[i]->str[0]==';'||!memcmp(parts[i]->str,"#!",strlen("#!")))
			return 1;
	}
	return 0;
}

int fklIsReDefStringPattern(FklString* const* parts,uint32_t num)
{
	FklStringMatchPattern* pattern=HeadOfStringPattern;
	if(pattern)
	{
		while(pattern->prev)
			pattern=pattern->prev;
		while(pattern)
		{
			if(!fklStringcmp(parts[0],pattern->parts[0]))
			{
				if(pattern->num!=num)
					return 0;
				else
				{
					int32_t i=0;
					for(;i<num;i++)
					{
						if(fklIsVar(pattern->parts[i])&&fklIsVar(parts[i]))
							continue;
						else if(fklIsVar(pattern->parts[i])||fklIsVar(parts[i]))
							return 0;
						else if(fklStringcmp(parts[i],pattern->parts[i]))
							return 0;
					}
				}
				return 1;
			}
			pattern=pattern->next;
		}
		return 0;
	}
	else
		return 0;
}

int fklIsMustList(const FklString* str)
{
	if(!fklIsVar(str))
		return 0;
	if(str->str[1]==',')
		return 1;
	return 0;
}

int fklIsVar(const FklString* part)
{
	const char* buf=part->str;
	if(buf[0]=='(')
		return 1;
	return 0;
}

void fklFreeCstrArray(char** ss,uint32_t num)
{
	for(uint32_t i=0;i<num;i++)
		free(ss[i]);
	free(ss);
}

FklString* fklGetVarName(const FklString* str)
{
	size_t size=str->size;
	const char* buf=str->str;
	size_t i=(buf[1]==',')?2:1;
	size_t j=i;
	for(;i<size&&buf[i]!=')';i++);
	FklString* tmp=fklNewString(i-j,buf+j);
	return tmp;
}

