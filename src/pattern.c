#include<fakeLisp/pattern.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/vm.h>
#include<string.h>
#include<ctype.h>

FklStringMatchPattern* fklCreateStringMatchPattern(FklNastNode* parts
		,FklByteCodelnt* bcl
		,FklStringMatchPattern* next)
{
	FklStringMatchPattern* r=(FklStringMatchPattern*)malloc(sizeof(FklStringMatchPattern));
	FKL_ASSERT(r);
	r->parts=fklMakeNastNodeRef(parts);
	r->type=FKL_STRING_PATTERN_DEFINED;
	r->u.proc=bcl;
	r->next=next;
	return r;
}

static const char* BuiltinStringPattern_0[]={":'","#a",};
static const char* BuiltinStringPattern_1[]={":(","&a",":)",};
static const char* BuiltinStringPattern_2[]={":[","&a",":]",};
static const char* BuiltinStringPattern_3[]={":(","#a","&b",":,","#c",":)",};
static const char* BuiltinStringPattern_4[]={":[","#a","&b",":,","#c",":]",};
static const char* BuiltinStringPattern_5[]={":#(","&a",":)",};
static const char* BuiltinStringPattern_6[]={":#[","&a",":]",};

static struct BuiltinStringPattern
{
	size_t num;
	const char** parts;
	FklNastNode* (*func)(FklPtrStack*,uint64_t,size_t*);
}BuiltinStringPatterns[]=
{
	{2,BuiltinStringPattern_0,NULL},
	{3,BuiltinStringPattern_1,NULL},
	{3,BuiltinStringPattern_2,NULL},
	{6,BuiltinStringPattern_3,NULL},
	{6,BuiltinStringPattern_4,NULL},
	{3,BuiltinStringPattern_5,NULL},
	{3,BuiltinStringPattern_6,NULL},
	{0,NULL,NULL},
};

static FklNastNode* createPatternPartFromCstr(const char* s)
{
	FklNastNode* r=fklCreateNastNode(FKL_NAST_NIL,0);
	switch(s[0])
	{
		case ':':
			r->type=FKL_NAST_STR;
			r->u.str=fklCreateStringFromCstr(&s[1]);
			break;
		case '#':
			r->type=FKL_NAST_SYM;
			r->u.sym=fklAddSymbolToGlobCstr(&s[1])->id;
			break;
		case '&':
			r->type=FKL_NAST_BOX;
			r->u.box=fklMakeNastNodeRef(fklCreateNastNode(FKL_NAST_SYM,0));
			r->u.box->u.sym=fklAddSymbolToGlobCstr(&s[1])->id;
			break;
		default:
			FKL_ASSERT(0);
	}
	return r;
}

static FklStringMatchPattern* createBuiltinStringPattern(FklNastNode* parts
		,FklNastNode* (*func)(FklPtrStack*,uint64_t,size_t*)
		,FklStringMatchPattern* next)
{
	FklStringMatchPattern* r=(FklStringMatchPattern*)malloc(sizeof(FklStringMatchPattern));
	FKL_ASSERT(r);
	r->parts=fklMakeNastNodeRef(parts);
	r->type=FKL_STRING_PATTERN_BUILTIN;
	r->u.func=func;
	r->next=next;
	return r;
}

static int isCover(const FklNastVector* p0,const FklNastVector* p1)
{
	return 0;
}

int fklStringPatternCoverState(const FklNastVector* p0,const FklNastVector* p1)
{
	int r=0;
	r+=isCover(p0,p1)?FKL_PATTERN_COVER:0;
	r+=isCover(p0,p1)?FKL_PATTERN_BE_COVER:0;
	return r;
}

FklStringMatchPattern* fklInitBuiltInStringPattern(void)
{
	FklStringMatchPattern* r=NULL;
	for(struct BuiltinStringPattern* cur=&BuiltinStringPatterns[0];cur->parts;cur++)
	{
		size_t num=cur->num;
		const char** curParts=cur->parts;
		FklNastNode* vec=fklCreateNastNode(FKL_NAST_VECTOR,0);
		vec->u.vec=fklCreateNastVector(num);
		for(size_t i=0;i<num;i++)
			vec->u.vec->base[i]=fklMakeNastNodeRef(createPatternPartFromCstr(curParts[i]));
		r=createBuiltinStringPattern(vec,NULL,r);
	}
	return r;
}

void fklAddStringMatchPattern(FklNastNode* parts
		,FklByteCodelnt* proc
		,FklStringMatchPattern** head)
{
	int coverState=0;
	FklStringMatchPattern** phead=head;
	for(;*phead;phead=&(*phead)->next)
	{
		FklStringMatchPattern* cur=*phead;
		coverState=fklStringPatternCoverState(cur->parts->u.vec,parts->u.vec);
		if(coverState)
			break;
	}
	if(!coverState)
		*head=fklCreateStringMatchPattern(parts,proc,*head);
	else
	{
		if(coverState==FKL_PATTERN_COVER)
		{
			FklStringMatchPattern* next=*phead;
			*phead=fklCreateStringMatchPattern(parts,proc,next);
		}
		else if(coverState==FKL_PATTERN_BE_COVER)
		{
			FklStringMatchPattern* next=(*phead)->next;
			(*phead)->next=fklCreateStringMatchPattern(parts,proc,next);
		}
		else
		{
			FklStringMatchPattern* cur=*phead;
			fklDestroyNastNode(cur->parts);
			if(cur->type==FKL_STRING_PATTERN_DEFINED)
				fklDestroyByteCodelnt(cur->u.proc);
			else
				cur->type=FKL_STRING_PATTERN_DEFINED;
			cur->parts=fklMakeNastNodeRef(parts);
			cur->u.proc=proc;
		}
	}
}

inline static FklNastNode* getPartFromState(FklStringMatchState* state,size_t index)
{
	return state->pattern->parts->u.vec->base[index];
}

inline static FklNastNode* getPartFromPattern(FklStringMatchPattern* pattern,size_t index)
{
	return pattern->parts->u.vec->base[index];
}

inline static size_t getSizeFromPattern(FklStringMatchPattern* p)
{
	return p->parts->u.vec->size;
}


static FklStringMatchState* createStringMatchState(FklStringMatchPattern* pattern
		,size_t index
		,FklStringMatchState* next)
{
	FklStringMatchState* r=(FklStringMatchState*)malloc(sizeof(FklStringMatchState));
	FKL_ASSERT(r);
	r->next=next;
	r->index=index;
	r->pattern=pattern;
	return r;
}

static void addStateIntoSyms(FklStringMatchState** syms,FklStringMatchState* state)
{
	state->next=*syms;
	*syms=state;
}

static void addStateIntoStrs(FklStringMatchState** strs,FklStringMatchState* state)
{
	size_t len=getPartFromState(state,state->index)->u.str->size;
	for(;*strs;strs=&(*strs)->next)
	{
		size_t curlen=getPartFromState((*strs),(*strs)->index)->u.str->size;
		if(len>=curlen)
		{
			state->next=*strs;
			*strs=state;
			break;
		}
	}
	if(!*strs)
	{
		state->next=*strs;
		*strs=state;
	}
}

static void addStateIntoBoxes(FklStringMatchState** boxes,FklStringMatchState* state)
{
	size_t len=getPartFromState(state,state->index+1)->u.str->size;
	for(;*boxes;boxes=&(*boxes)->next)
	{
		size_t curlen=getPartFromState((*boxes),(*boxes)->index+1)->u.str->size;
		if(len>=curlen)
		{
			state->next=*boxes;
			*boxes=state;
			break;
		}
	}
	if(!*boxes)
	{
		state->next=*boxes;
		*boxes=state;
	}
}

static void addStateIntoSet(FklStringMatchState* state
		,FklStringMatchState** strs
		,FklStringMatchState** boxes
		,FklStringMatchState** syms)
{
	FklNastNode* part=getPartFromState(state,state->index);
	switch(part->type)
	{
		case FKL_NAST_STR:
			addStateIntoStrs(strs,state);
			break;
		case FKL_NAST_BOX:
			addStateIntoBoxes(boxes,state);
			break;
		case FKL_NAST_SYM:
			addStateIntoSyms(syms,state);
			break;
		default:
			FKL_ASSERT(0);
	}
}

static FklStringMatchSet* createStringMatchSet(FklStringMatchState* strs
		,FklStringMatchState* boxes
		,FklStringMatchState* syms
		,FklStringMatchSet* prev)
{
	FklStringMatchSet* r=(FklStringMatchSet*)malloc(sizeof(FklStringMatchSet));
	FKL_ASSERT(r);
	r->str=strs;
	r->box=boxes;
	r->sym=syms;
	r->prev=prev;
	return r;
}

inline static int simpleMatch(const char* buf,size_t n,FklString* s)
{
	return n>=s->size&&!memcmp(s->str,buf,s->size);
}

inline static int matchWithStr(size_t pn,const char* buf,size_t n,FklString* s)
{
	return (pn==0||pn==s->size)&&simpleMatch(buf,n,s);
}

static FklStringMatchSet* matchInUniversSet(const char* buf
		,size_t n
		,FklStringMatchPattern* p
		,FklToken** pt
		,size_t line
		,FklStringMatchSet* prev)
{
	FklStringMatchSet* r=NULL;
	size_t pn=0;
	FklStringMatchState* strs=NULL;
	FklStringMatchState* boxes=NULL;
	FklStringMatchState* syms=NULL;
	for(;p;p=p->next)
	{
		FklString* s=p->parts->u.vec->base[0]->u.str;
		if(pn!=0&&pn>s->size)
			break;
		if(matchWithStr(pn,buf,n,s))
		{
			pn=s->size;
			if(!*pt)
				*pt=fklCreateToken(FKL_TOKEN_RESERVE_STR,fklCopyString(s),line);
			FklStringMatchState* state=createStringMatchState(p,1,NULL);
			addStateIntoSet(state,&strs,&boxes,&syms);
		}
	}
	if(strs||boxes||syms)
		r=createStringMatchSet(strs,boxes,syms,prev);
	return r;
}

static int matchStrInBoxes(const char* buf
		,size_t n
		,FklStringMatchState* boxes)
{
	for(;boxes;boxes=boxes->next)
	{
		FklString* s=getPartFromState(boxes,boxes->index+1)->u.str;
		if(simpleMatch(buf,n,s))
			return 1;
	}
	return 0;
}

static int matchStrInPatterns(const char* buf
		,size_t n
		,FklStringMatchPattern* patterns)
{
	for(;patterns;patterns=patterns->next)
	{
		FklString* s=getPartFromPattern(patterns,0)->u.str;
		if(simpleMatch(buf,n,s))
			return 1;
	}
	return 0;
}

static size_t getDefaultSymbolLen(const char* buf
		,size_t n
		,FklStringMatchState* boxes
		,FklStringMatchPattern* patterns)
{
	size_t i=0;
	for(;i<n
			&&!isspace(buf[i])
			&&!matchStrInBoxes(&buf[i],n-i,boxes)
			&&!matchStrInPatterns(&buf[i],n-i,patterns)
			;i++);
	return i;
}

static FklToken* defaultTokenCreator(const char* buf
		,size_t n
		,FklStringMatchState* boxes
		,FklStringMatchPattern* patterns
		,FklStringMatchSet* prev
		,size_t line)
{
	size_t len=getDefaultSymbolLen(buf,n,boxes,patterns);
	if(len)
		return fklCreateToken(FKL_TOKEN_SYMBOL,fklCreateString(len,buf),line);
	else
		return NULL;
}

static int updateStrState(const char* buf
		,size_t n
		,FklStringMatchSet** pset
		,FklStringMatchState* strs
		,FklStringMatchSet* prev
		,FklToken** pt
		,size_t line)
{
	int r=0;
	size_t pn=0;
	for(FklStringMatchState** pstate=&strs;*pstate;)
	{
		FklStringMatchState* cur=*pstate;
		FklString* s=getPartFromState(cur,cur->index)->u.str;
		if(pn!=0&&pn>s->size)
			break;
		if(matchWithStr(pn,buf,n,s))
		{
			pn=s->size;
			if(!*pt)
				*pt=fklCreateToken(FKL_TOKEN_RESERVE_STR,fklCopyString(s),line);
			cur->index++;
			if(cur->index<getSizeFromPattern(cur->pattern))
			{
				*pstate=cur->next;
				addStateIntoSet(cur,&prev->str,&prev->box,&prev->sym);
			}
			else
				pstate=&(*pstate)->next;
			r=1;
		}
		else
			pstate=&(*pstate)->next;
	}
	return r;
}

static int updateBoxState(const char* buf
		,size_t n
		,FklStringMatchSet** pset
		,FklStringMatchState* boxes
		,FklStringMatchSet* prev
		,FklStringMatchPattern* patterns
		,FklToken** pt
		,size_t line)
{
	int r=0;
	size_t pn=0;
	for(FklStringMatchState** pstate=&boxes;*pstate;)
	{
		FklStringMatchState* cur=*pstate;
		FklString* s=getPartFromState(cur,cur->index+1)->u.str;
		if(pn!=0&&pn>s->size)
			break;
		if(matchWithStr(pn,buf,n,s))
		{
			pn=s->size;
			if(!*pt)
				*pt=fklCreateToken(FKL_TOKEN_RESERVE_STR,fklCopyString(s),line);
			cur->index+=2;
			r=1;
		}
		if(cur->index<getSizeFromPattern(cur->pattern))
		{
			*pstate=cur->next;
			addStateIntoSet(cur,&prev->str,&prev->box,&prev->sym);
		}
		else
			pstate=&(*pstate)->next;
	}
	if(!r)
	{
		*pset=matchInUniversSet(buf,n
				,patterns
				,pt
				,line
				,prev);
	}
	return r;
}

static void updateSymState(const char* buf
		,size_t n
		,FklStringMatchSet** pset
		,FklStringMatchState* syms
		,FklStringMatchSet* prev
		,FklStringMatchPattern* patterns
		,FklToken** pt
		,size_t line)
{
	for(FklStringMatchState** pstate=&syms;*pstate;)
	{
		FklStringMatchState* cur=*pstate;
		cur->index++;
		if(cur->index<getSizeFromPattern(cur->pattern))
		{
			*pstate=cur->next;
			addStateIntoSet(cur,&prev->str,&prev->box,&prev->sym);
		}
		else
			pstate=&(*pstate)->next;
	}
	*pset=matchInUniversSet(buf,n
			,patterns
			,pt
			,line
			,prev);
}

static FklStringMatchSet* updatePreviusSet(FklStringMatchSet* set
		,const char* buf
		,size_t n
		,FklStringMatchPattern* patterns
		,FklToken** pt
		,size_t line)
{
	FklStringMatchSet* r=NULL;
	if(set==NULL)
		r=matchInUniversSet(buf,n,patterns,pt,line,NULL);
	else if(set->str==NULL&&set->box==NULL&&set->sym==NULL)
	{
		r=set->prev;
		free(set);
	}
	else
	{
		FklStringMatchState* strs=set->str;
		FklStringMatchState* boxes=set->box;
		FklStringMatchState* syms=set->sym;
		set->sym=NULL;
		set->box=NULL;
		set->str=NULL;
		r=NULL;
		if(!updateStrState(buf,n,&r,strs,set,pt,line))
		{
			if(!updateBoxState(buf,n,&r,boxes,set,patterns,pt,line))
			{
				updateSymState(buf,n,&r,syms,set,patterns,pt,line);
				if(!*pt)
					*pt=defaultTokenCreator(buf,n,set->box,patterns,set,line);
			}
		}
		r=set;
	}
	return r;
}

FklStringMatchSet* fklGetMatchingSet(const char* s,size_t n
		,FklStringMatchSet* curSet
		,FklStringMatchPattern* patterns
		,FklToken** ptoken
		,size_t line)
{
	return updatePreviusSet(curSet,s,n,patterns,ptoken,line);
}

void fklDestroyStringPattern(FklStringMatchPattern* o)
{
	if(o->type==FKL_STRING_PATTERN_DEFINED)
		fklDestroyByteCodelnt(o->u.proc);
	fklDestroyNastNode(o->parts);
	free(o);
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

FklToken* fklCreateTokenCopyStr(FklTokenType type,const FklString* str,size_t line)
{
	FklToken* token=(FklToken*)malloc(sizeof(FklToken));
	FKL_ASSERT(token);
	token->type=type;
	token->line=line;
	token->value=fklCopyString(str);
	return token;
}

FklToken* fklCreateToken(FklTokenType type,FklString* str,size_t line)
{
	FklToken* token=(FklToken*)malloc(sizeof(FklToken));
	FKL_ASSERT(token);
	token->type=type;
	token->line=line;
	token->value=str;
	return token;
}

void fklDestroyToken(FklToken* token)
{
	free(token->value);
	free(token);
}

void fklPrintToken(FklPtrStack* tokenStack,FILE* fp)
{
	static const char* tokenTypeName[]=
	{
		"reserve_str",
		"char",
		"num",
		"string",
		"symbol",
		"comment",
	};
	for(uint32_t i=0;i<tokenStack->top;i++)
	{
		FklToken* token=tokenStack->base[i];
		fprintf(fp,"%lu,%s:",token->line,tokenTypeName[token->type]);
		fklPrintString(token->value,fp);
		fputc('\n',fp);
	}
}

int fklIsAllComment(FklPtrStack* tokenStack)
{
	for(uint32_t i=0;i<tokenStack->top;i++)
	{
		FklToken* token=tokenStack->base[i];
		if(token->type!=FKL_TOKEN_COMMENT)
			return 0;
	}
	return 1;
}
