#include<fakeLisp/pattern.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/vm.h>
#include<string.h>
#include<ctype.h>
static FklStringMatchPattern* HeadOfStringPattern=NULL;
static uint32_t countStringParts(const FklString*);
//static uint32_t* matchPartOfPattern(const FklString*,FklStringMatchPattern*,uint32_t*);
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


//int fklMaybePatternPrefix(const char* str)
//{
//	if(!HeadOfStringPattern)
//		return 0;
//	FklStringMatchPattern* cur=HeadOfStringPattern;
//	str+=fklSkipSpace(str);
//	while(cur)
//	{
//		FklString* part=cur->parts[0];
//		if(strlen(str)&&!strncmp(str,part,strlen(str)))
//			return 1;
//		cur=cur->next;
//	}
//	return 0;
//}

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
	FKL_ASSERT(tmp,__func__);
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
	FKL_ASSERT(tmp,__func__);
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

//static uint32_t* matchPartOfPattern(const FklString* str,FklStringMatchPattern* pattern,uint32_t* num)
//{
//	*num=fklCountInPattern(str,pattern);
//	uint32_t* fklSplitIndex=(uint32_t*)malloc(sizeof(uint32_t)*(*num));
//	FKL_ASSERT(fklSplitIndex,__func__);
//	uint32_t s=0;
//	uint32_t i=0;
//	for(;i<*num;i++)
//	{
//		const FklString* part=pattern->parts[i];
//		if(!fklIsVar(part))
//		{
//			s+=fklSkipSpace(str,s);
//			if(!memcmp(str->str+s,part,part->size))
//				fklSplitIndex[i]=s;
//			s+=part->size;
//		}
//		else
//		{
//			s+=fklSkipSpace(str,s);
//			fklSplitIndex[i]=s;
//			FklString* next=(i+1<pattern->num)?pattern->parts[i+1]:NULL;
//			s+=fklSkipUntilNextWhenReading(str,s,next);
//		}
//	}
//	return fklSplitIndex;
//}


//FklString** fklSplitStringInPattern(const FklString* str,FklStringMatchPattern* pattern,uint32_t* num)
//{
//	int i=0;
//	uint32_t* s=matchPartOfPattern(str,pattern,num);
//	FklString** tmp=(FklString**)malloc(sizeof(FklString*)*(pattern->num));
//	FKL_ASSERT(tmp,__func__);
//	for(;i<pattern->num;i++)
//		tmp[i]=NULL;
//	for(i=0;i<*num;i++)
//	{
//		size_t strSize=(i+1<*num)?((size_t)(s[i+1]-s[i])):(size_t)fklSkipInPattern(str,0,pattern)-s[i];
//		FklString* tmpStr=fklNewString(strSize,str->str+s[i]);
//		tmp[i]=tmpStr;
//	}
//	free(s);
//	return tmp;
//}

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

//uint32_t fklFindKeyString(const FklString* str)
//{
//	FklStringMatchPattern* cur=HeadOfStringPattern;
//	while(cur)
//	{
//		int i=0;
//		const FklString* keyString=cur->parts[0];
//		i+=fklSkipAtom(str+i,keyString);
//		if(str[i])
//			return i;
//		cur=cur->next;
//	}
//	return fklSkipAtom(str,"");
//}

int fklIsInValidStringPattern(const FklString* str)
{
	FklStringMatchPattern* pattern=HeadOfStringPattern;
	uint32_t num=0;
	FklString** parts=fklSplitPattern(str,&num);
	if(fklIsVar(parts[0])||parts[0]->str[0]=='\"')
	{
		fklFreeStringArray(parts,num);
		return 1;
	}
	if(pattern)
	{
		while(pattern->prev)
			pattern=pattern->prev;
		while(pattern)
		{
			if(!fklStringcmp(parts[0],pattern->parts[0]))
			{
				fklFreeStringArray(parts,num);
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
					fklFreeStringArray(parts,num);
					return 1;
				}
			}
			else
			{
				fklFreeStringArray(parts,num);
				return 1;
			}
		}
		else if(parts[i]->str[0]=='\"'||parts[i]->str[0]==';'||!memcmp(parts[i]->str,"#!",strlen("#!")))
		{
			fklFreeStringArray(parts,num);
			return 1;
		}
	}
	fklFreeStringArray(parts,num);
	return 0;
}

int fklIsReDefStringPattern(const FklString* str)
{
	FklStringMatchPattern* pattern=HeadOfStringPattern;
	uint32_t num=0;
	if(pattern)
	{
		FklString** parts=fklSplitPattern(str,&num);
		while(pattern->prev)
			pattern=pattern->prev;
		while(pattern)
		{
			int r=1;
			if(!fklStringcmp(parts[0],pattern->parts[0]))
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
						else if(fklStringcmp(parts[i],pattern->parts[i]))
						{
							r=0;
							break;
						}
					}
				}
				fklFreeStringArray(parts,num);
				return r;
			}
			pattern=pattern->next;
		}
		fklFreeStringArray(parts,num);
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

//size_t fklSkipInPattern(const FklString* str,size_t index,FklStringMatchPattern* pattern)
//{
//	uint32_t i=0;
//	size_t s=0;
//	if(pattern)
//	{
//		for(;i<pattern->num;i++)
//		{
//			FklString* part=pattern->parts[i];
//			if(!fklIsVar(part))
//			{
//				s+=fklSkipSpace(str,s+index);
//				s+=part->size;
//			}
//			else
//			{
//				s+=fklSkipSpace(str,s+index);
//				FklString* next=(i+1<pattern->num)?pattern->parts[i+1]:NULL;
//				s+=fklSkipUntilNextWhenReading(str,s+index,next);
//			}
//		}
//	}
//	else
//		s+=fklSkipUntilNext(str,s+index,NULL);
//	return s;
//}

//uint32_t fklCountInPattern(const FklString* str,FklStringMatchPattern* pattern)
//{
//	uint32_t i=0;
//	uint32_t s=0;
//	size_t len=str->size;
//	while(i<pattern->num)
//	{
//		if(s>=len)break;
//		FklString* part=pattern->parts[i];
//		if(!fklIsVar(part))
//		{
//			s+=fklSkipSpace(str,s);
//			if(memcmp(str->str+s,part,part->size))
//				break;
//			s+=part->size;
//		}
//		else
//		{
//			s+=fklSkipSpace(str,s);
//			if(str->str[s]==')')break;
//			FklString* next=(i+1<pattern->num)?pattern->parts[i+1]:NULL;
//			s+=fklSkipUntilNextWhenReading(str,s,next);
//		}
//		s+=fklSkipSpace(str,s);
//		i++;
//	}
//	return i;
//}

//size_t fklSkipUntilNext(const FklString* str,size_t index,const FklString* part)
//{
//	size_t s=0;
//	const char* buf=str->str;
//	while(buf[s+index])
//	{
//		if(buf[s+index]=='(')
//		{
//			s+=fklSkipParentheses(str,s+index);
//		}
//		else if(buf[s+index]=='\"')
//		{
//			size_t len=0;
//			char* tmpStr=fklCastEscapeCharater(buf+s+index+1,'\"',&len);
//			s+=len+1;
//			free(tmpStr);
//		}
//		else if(isspace(buf[s+index]))
//		{
//			s+=fklSkipSpace(str,s+index);
//			continue;
//		}
//		else if(buf[s+index]==',')
//			break;
//		else
//		{
//			FklStringMatchPattern* pattern=fklFindStringPattern(str+s);
//			if(pattern)
//			{
//				s+=fklSkipInPattern(str,index+s,pattern);
//				s+=fklSkipSpace(str,index+s);
//				continue;
//			}
//			else if(part&&!fklIsVar(part))
//			{
//				s+=fklSkipAtom(str,index+s,part);
//				s+=fklSkipSpace(str,index+s);
//				if(!memcmp(buf+s+index,part,part->size))
//					break;
//			}
//			s+=fklSkipAtom(str,index+s,part);
//		}
//		if(!part||fklIsVar(part))
//			break;
//	}
//	return s;
//}

//size_t fklSkipParentheses(const FklString* str,size_t index)
//{
//	int parentheses=0;
//	int mark=0;
//	int32_t i=0;
//	while(str[i]!='\0')
//	{
//		if(str[i]=='\"'&&(i<1||str[i-1]!='\\'))
//			mark^=1;
//		if(str[i]=='('&&(i<1||str[i-1]!='\\')&&!mark)
//			parentheses++;
//		else if(str[i]==')'&&(i<1||str[i-1]!='\\')&&!mark)
//			parentheses--;
//		i++;
//		if(parentheses==0)
//			break;
//	}
//	return i;
//}

//size_t fklSkipAtom(const FklString* str,size_t i,const FklString* keyString)
//{
//	int32_t keyLen=strlen(keyString);
//	int32_t i=0;
//	i+=fklSkipSpace(str);
//	if(str[i]=='\"')
//	{
//		size_t len=0;
//		char* tStr=fklCastEscapeCharater(str+i+1,'\"',&len);
//		free(tStr);
//		i+=len+1;
//	}
//	else if(str[i]=='#'&&(str[i+1]=='b'||str[i+1]=='\\'))
//	{
//		i++;
//		if(str[i]=='\\')
//		{
//			i++;
//			if(str[i]!='\\')
//				i++;
//			else
//			{
//				i++;
//				if(isdigit(str[i]))
//				{
//					int32_t j=0;
//					for(;isdigit(str[i+j])&&j<3;j++);
//					i+=j;
//				}
//				else if(toupper(str[i])=='X')
//				{
//					i++;
//					int32_t j=0;
//					for(;isxdigit(str[i+j])&&j<2;j++);
//					i+=j;
//				}
//				else
//					i++;
//			}
//		}
//		else if(str[i]=='b')
//		{
//			i++;
//			for(;isxdigit(str[i]);i++);
//		}
//	}
//	else
//	{
//		for(;str[i]!='\0';i++)
//		{
//			if(isspace(str[i])||str[i]=='('||str[i]==')'||str[i]==','||!strncmp(str+i,"#b",2)||!strncmp(str+i,"#\\",2)||(keyLen&&!strncmp(str+i,keyString,keyLen)))
//				break;
//		}
//	}
//	return i;
//}

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

//void fklPrintInPattern(FklString** strs,FklStringMatchPattern* pattern,FILE* fp,uint32_t count)
//{
//	int i=0;
//	for(;i<pattern->num;i++)
//	{
//		int j=0;
//		for(;j<count;j++)
//			putc('\t',fp);
//		if(!fklIsVar(pattern->parts[i]))
//		{
//			char* part=pattern->parts[i];
//			fprintf(fp,"%s\n",part);
//		}
//		else
//		{
//			FklStringMatchPattern* nextPattern=fklFindStringPattern(strs[i]);
//			if(nextPattern)
//			{
//				int32_t num=0;
//				int32_t j=0;
//				char** ss=fklSplitStringInPattern(strs[i],nextPattern,&num);
//				fklPrintInPattern(ss,nextPattern,fp,count+1);
//				for(;j<num;j++)
//					free(ss[j]);
//				free(ss);
//			}
//			else
//			{
//				putc('\t',fp);
//				fprintf(fp,"%s\n",strs[i]);
//			}
//		}
//	}
//}

//size_t fklSkipUntilNextWhenReading(const FklString* str,size_t index,const FklString* part)
//{
//	int32_t s=0;
//	while(str[s])
//	{
//		if(str[s]=='(')
//		{
//			s+=fklSkipParentheses(str+s);
//		}
//		else if(str[s]=='\"')
//		{
//			size_t len=0;
//			char* tmpStr=fklCastEscapeCharater(str+s+1,'\"',&len);
//			s+=len+1;
//			free(tmpStr);
//		}
//		else if(isspace(str[s]))
//		{
//			s+=fklSkipSpace(str+s);
//			continue;
//		}
//		else if(str[s]==',')
//		{
//			if(part)
//				s++;
//			else
//				break;
//		}
//		else
//		{
//			FklStringMatchPattern* pattern=fklFindStringPattern(str+s);
//			if(pattern)
//			{
//				s+=fklSkipInPattern(str+s,pattern);
//				s+=fklSkipSpace(str+s);
//				continue;
//			}
//			else if(part&&!fklIsVar(part))
//			{
//				s+=fklSkipAtom(str+s,part);
//				s+=fklSkipSpace(str+s);
//				if(!strncmp(str+s,part,strlen(part)))
//					break;
//			}
//			s+=fklSkipAtom(str+s,part?part:"");
//		}
//		if(!part||fklIsVar(part))
//			break;
//	}
//	return s;
//}


