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

FklStringMatchPattern* fklCreateStringMatchPattern(uint32_t num,FklString** parts,FklByteCodelnt* proc)
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
			FklString* tmpStr=fklCreateString(j-i+1,buf+i);
			tmp[count]=tmpStr;
			i=j;
			count++;
			continue;
		}
		size_t j=i;
		for(;j<size&&buf[j]!='(';j++);
		FklString* tmpStr=fklCreateString(j-i,buf+i);
		tmp[count]=tmpStr;
		count++;
		i=j-1;
		continue;
	}
	return tmp;
}

void fklDestroyAllStringPattern()
{
	FklStringMatchPattern* cur=HeadOfStringPattern;
	while(cur)
	{
		FklStringMatchPattern* prev=cur;
		cur=cur->next;
		fklDestroyStringArray(prev->parts,prev->num);
		fklDestroyByteCodelnt(prev->proc);
		free(prev);
	}
}

void fklDestroyStringPattern(FklStringMatchPattern* o)
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

void fklDestroyCstrArray(char** ss,uint32_t num)
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
	FklString* tmp=fklCreateString(i-j,buf+j);
	return tmp;
}

FklNastNode* fklPatternMatchingHashTableRef(FklSid_t sid,FklHashTable* ht)
{
	FklPatternMatchingHashTableItem* item=fklGetHashItem(&sid,ht);
	return item->node;
}

inline static FklPatternMatchingHashTableItem* createPatternMatchingHashTableItem(FklSid_t id,FklNastNode* node)
{
	FklPatternMatchingHashTableItem* r=(FklPatternMatchingHashTableItem*)malloc(sizeof(FklPatternMatchingHashTableItem));
	FKL_ASSERT(r);
	r->id=id;
	r->node=node;
	return r;
}

static void patternMatchingHashTableSet(FklSid_t sid,FklNastNode* node,FklHashTable* ht)
{
	fklPutReplHashItem(createPatternMatchingHashTableItem(sid,node),ht);
}

int fklPatternMatch(const FklNastNode* pattern,const FklNastNode* exp,FklHashTable* ht)
{
	if(exp->type!=FKL_NAST_PAIR)
		return 0;
	if(exp->u.pair->car->type!=FKL_NAST_SYM
			||pattern->u.pair->car->u.sym!=exp->u.pair->car->u.sym)
		return 0;
	FklPtrStack* s0=fklCreatePtrStack(32,16);
	FklPtrStack* s1=fklCreatePtrStack(32,16);
	fklPushPtrStack(pattern->u.pair->cdr,s0);
	fklPushPtrStack(exp->u.pair->cdr,s1);
	while(!fklIsPtrStackEmpty(s0)&&!fklIsPtrStackEmpty(s1))
	{
		FklNastNode* n0=fklPopPtrStack(s0);
		FklNastNode* n1=fklPopPtrStack(s1);
		if(n0->type==FKL_NAST_SYM)
		{
			if(ht!=NULL)
				patternMatchingHashTableSet(n0->u.sym,n1,ht);
		}
		else if(n0->type==FKL_NAST_PAIR&&n1->type==FKL_NAST_PAIR)
		{
			fklPushPtrStack(n0->u.pair->cdr,s0);
			fklPushPtrStack(n0->u.pair->car,s0);
			fklPushPtrStack(n1->u.pair->cdr,s1);
			fklPushPtrStack(n1->u.pair->car,s1);
		}
		else if(!fklNastNodeEqual(n0,n1))
		{
			fklDestroyPtrStack(s0);
			fklDestroyPtrStack(s1);
			return 0;
		}
	}
	fklDestroyPtrStack(s0);
	fklDestroyPtrStack(s1);
	return 1;
}

static size_t _pattern_matching_hash_table_hash_func(void* key)
{
	FklSid_t sid=*(FklSid_t*)key;
	return sid;
}

static void _pattern_matching_hash_free_item(void* item)
{
	free(item);
}

static void* _pattern_matching_hash_get_key(void* item)
{
	return &((FklPatternMatchingHashTableItem*)item)->id;
}

static int _pattern_matching_hash_key_equal(void* pk0,void* pk1)
{
	FklSid_t k0=*(FklSid_t*)pk0;
	FklSid_t k1=*(FklSid_t*)pk1;
	return k0==k1;
}

static FklHashTableMethodTable Codegen_hash_method_table=
{
	.__hashFunc=_pattern_matching_hash_table_hash_func,
	.__destroyItem=_pattern_matching_hash_free_item,
	.__keyEqual=_pattern_matching_hash_key_equal,
	.__getKey=_pattern_matching_hash_get_key,
};

FklHashTable* fklCreatePatternMatchingHashTable(void)
{
	return fklCreateHashTable(8,8,2,0.75,1,&Codegen_hash_method_table);
}

typedef struct
{
	FklSid_t id;
}FklSidHashItem;

static FklSidHashItem* createSidHashItem(FklSid_t key)
{
	FklSidHashItem* r=(FklSidHashItem*)malloc(sizeof(FklSidHashItem));
	FKL_ASSERT(r);
	r->id=key;
	return r;
}

static size_t _sid_hashFunc(void* key)
{
	FklSid_t sid=*(FklSid_t*)key;
	return sid;
}

static void _sid_destroyItem(void* item)
{
	free(item);
}

static int _sid_keyEqual(void* pkey0,void* pkey1)
{
	FklSid_t k0=*(FklSid_t*)pkey0;
	FklSid_t k1=*(FklSid_t*)pkey1;
	return k0==k1;
}

static void* _sid_getKey(void* item)
{
	return &((FklSidHashItem*)item)->id;
}

static FklHashTableMethodTable SidHashMethodTable=
{
	.__hashFunc=_sid_hashFunc,
	.__destroyItem=_sid_destroyItem,
	.__keyEqual=_sid_keyEqual,
	.__getKey=_sid_getKey,
};

int fklIsValidSyntaxPattern(const FklNastNode* p)
{
	if(p->type!=FKL_NAST_PAIR)
		return 0;
	FklNastNode* head=p->u.pair->car;
	if(head->type!=FKL_NAST_SYM)
		return 0;
	const FklNastNode* body=p->u.pair->cdr;
	FklHashTable* symbolTable=fklCreateHashTable(8,4,2,0.75,1,&SidHashMethodTable);
	FklPtrStack* stack=fklCreatePtrStack(32,16);
	fklPushPtrStack((void*)body,stack);
	while(!fklIsPtrStackEmpty(stack))
	{
		const FklNastNode* c=fklPopPtrStack(stack);
		switch(c->type)
		{
			case FKL_NAST_PAIR:
				fklPushPtrStack(c->u.pair->car,stack);
				fklPushPtrStack(c->u.pair->cdr,stack);
				break;
			case FKL_TYPE_SYM:
				if(fklGetHashItem((void*)&c->u.sym,symbolTable))
				{
					fklDestroyHashTable(symbolTable);
					fklDestroyPtrStack(stack);
					return 0;
				}
				fklPutNoRpHashItem(createSidHashItem(c->u.sym)
						,symbolTable);
				break;
			default:
				break;
		}
	}
	fklDestroyHashTable(symbolTable);
	fklDestroyPtrStack(stack);
	return 1;
}

inline int fklPatternCoverState(const FklNastNode* p0,const FklNastNode* p1)
{
	int r=0;
	r+=fklPatternMatch(p0,p1,NULL)?FKL_PATTERN_COVER:0;
	r+=fklPatternMatch(p1,p0,NULL)?FKL_PATTERN_BE_COVER:0;
	return r;
}
