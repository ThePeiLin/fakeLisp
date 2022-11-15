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

static const char* BuiltinStringPattern_quote[]={":'","#a",};
static const char* BuiltinStringPattern_qsquote[]={":`","#a",};
static const char* BuiltinStringPattern_unquote[]={":~","#a",};
static const char* BuiltinStringPattern_unqtesp[]={":~@","#a",};
static const char* BuiltinStringPattern_box[]={":#&","#a",};
static const char* BuiltinStringPattern_list_0[]={":(","&a",":)",};
static const char* BuiltinStringPattern_list_1[]={":[","&a",":]",};
static const char* BuiltinStringPattern_pair_0[]={":(","#a","&b",":,","#c",":)",};
static const char* BuiltinStringPattern_pair_1[]={":[","#a","&b",":,","#c",":]",};
static const char* BuiltinStringPattern_vector_0[]={":#(","&a",":)",};
static const char* BuiltinStringPattern_vector_1[]={":#[","&a",":]",};
static const char* BuiltinStringPattern_bytevector_0[]={":#vu8(","&a",":)",};
static const char* BuiltinStringPattern_bytevector_1[]={":#vu8[","&a",":]",};
static const char* BuiltinStringPattern_hash_0[]={":#hash(","&a",":)",};
static const char* BuiltinStringPattern_hash_1[]={":#hash[","&a",":]",};
static const char* BuiltinStringPattern_hasheqv_0[]={":#hasheqv(","&a",":)",};
static const char* BuiltinStringPattern_hasheqv_1[]={":#hasheqv[","&a",":]",};
static const char* BuiltinStringPattern_hashequal_0[]={":#hashequal(","&a",":)",};
static const char* BuiltinStringPattern_hashequal_1[]={":#hashequal[","&a",":]",};

static struct BuiltinStringPattern
{
	size_t num;
	const char** parts;
	FklNastNode* (*func)(FklPtrStack*,uint64_t,size_t*);
}BuiltinStringPatterns[]=
{
	{2,BuiltinStringPattern_quote,NULL},
	{2,BuiltinStringPattern_qsquote,NULL},
	{2,BuiltinStringPattern_unquote,NULL},
	{2,BuiltinStringPattern_unqtesp,NULL},
	{2,BuiltinStringPattern_box,NULL},

	{3,BuiltinStringPattern_list_0,NULL},
	{3,BuiltinStringPattern_list_1,NULL},

	{6,BuiltinStringPattern_pair_0,NULL},
	{6,BuiltinStringPattern_pair_1,NULL},

	{3,BuiltinStringPattern_vector_0,NULL},
	{3,BuiltinStringPattern_vector_1,NULL},

	{3,BuiltinStringPattern_bytevector_0,NULL},
	{3,BuiltinStringPattern_bytevector_1,NULL},

	{3,BuiltinStringPattern_hash_0,NULL},
	{3,BuiltinStringPattern_hash_1,NULL},

	{3,BuiltinStringPattern_hasheqv_0,NULL},
	{3,BuiltinStringPattern_hasheqv_1,NULL},

	{3,BuiltinStringPattern_hashequal_0,NULL},
	{3,BuiltinStringPattern_hashequal_1,NULL},

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

void fklDestroyStringPattern(FklStringMatchPattern* o)
{
	if(o->type==FKL_STRING_PATTERN_DEFINED)
		fklDestroyByteCodelnt(o->u.proc);
	fklDestroyNastNode(o->parts);
	free(o);
}

void fklDestroyAllStringPattern(FklStringMatchPattern* h)
{
	FklStringMatchPattern** ph=&h;
	while(*ph)
	{
		FklStringMatchPattern* cur=*ph;
		*ph=cur->next;
		fklDestroyStringPattern(cur);
	}
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
