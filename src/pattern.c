#include<fakeLisp/pattern.h>
#include<fakeLisp/codegen.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/vm.h>
#include<string.h>
#include<ctype.h>

FklStringMatchPattern* fklCreateStringMatchPattern(FklNastNode* parts
		,FklByteCodelnt* bcl
		,FklPrototypePool* ptpool
		,FklStringMatchPattern* next
		,int own)
{
	FklStringMatchPattern* r=(FklStringMatchPattern*)malloc(sizeof(FklStringMatchPattern));
	FKL_ASSERT(r);
	r->parts=parts;
	r->type=FKL_STRING_PATTERN_DEFINED;
	r->u.proc=bcl;
	r->next=next;
	r->ptpool=ptpool;
	r->own=own;
	return r;
}

static const char* BuiltinStringPattern_quote[]={":'","#car",};
static const char* BuiltinStringPattern_qsquote[]={":`","#car",};
static const char* BuiltinStringPattern_unquote[]={":~","#car",};
static const char* BuiltinStringPattern_unqtesp[]={":~@","#car",};
static const char* BuiltinStringPattern_box[]={":#&","#car",};
static const char* BuiltinStringPattern_list_0[]={":(","&car",":)",};
static const char* BuiltinStringPattern_list_1[]={":[","&car",":]",};
static const char* BuiltinStringPattern_pair_0[]={":(","#car","&cdr",":,","#cons",":)",};
static const char* BuiltinStringPattern_pair_1[]={":[","#car","&cdr",":,","#cons",":]",};
static const char* BuiltinStringPattern_vector_0[]={":#(","&car",":)",};
static const char* BuiltinStringPattern_vector_1[]={":#[","&car",":]",};
static const char* BuiltinStringPattern_bytevector_0[]={":#vu8(","&car",":)",};
static const char* BuiltinStringPattern_bytevector_1[]={":#vu8[","&car",":]",};
static const char* BuiltinStringPattern_hash_0[]={":#hash(","&car",":)",};
static const char* BuiltinStringPattern_hash_1[]={":#hash[","&car",":]",};
static const char* BuiltinStringPattern_hasheqv_0[]={":#hasheqv(","&car",":)",};
static const char* BuiltinStringPattern_hasheqv_1[]={":#hasheqv[","&car",":]",};
static const char* BuiltinStringPattern_hashequal_0[]={":#hashequal(","&car",":)",};
static const char* BuiltinStringPattern_hashequal_1[]={":#hashequal[","&car",":]",};

static FklNastNode* create_nast_list(FklNastNode** a,size_t num,uint64_t line)
{
	FklNastNode* r=NULL;
	FklNastNode** cur=&r;
	for(size_t i=0;i<num;i++)
	{
		(*cur)=fklCreateNastNode(FKL_NAST_PAIR,a[i]->curline);
		(*cur)->u.pair=fklCreateNastPair();
		(*cur)->u.pair->car=a[i];
		cur=&(*cur)->u.pair->cdr;
	}
	(*cur)=fklCreateNastNode(FKL_NAST_NIL,line);
	return r;
}

static FklNastNode* nastWithHeaderProcesser(FklSid_t sid,FklNastNode* v,uint64_t line)
{
	FklNastNode* header=fklCreateNastNode(FKL_NAST_SYM,line);
	header->u.sym=sid;
	FklNastNode* a[]={header,v};
	return create_nast_list(a,2,line);
}

enum
{
	BUILTIN_HEAD_QUOTE=0,
	BUILTIN_HEAD_QSQUOTE=1,
	BUILTIN_HEAD_UNQUOTE=2,
	BUILTIN_HEAD_UNQTESP=3,
}BuildInHeadSymbolIndex;

static FklNastNode* quoteProcesser(FklPtrStack* nastStack
		,uint64_t line
		,size_t* errorLine
		,const FklSid_t st[4])
{
	return nastWithHeaderProcesser(st[BUILTIN_HEAD_QUOTE]
			,fklMakeNastNodeRef(fklTopPtrStack(nastStack))
			,line);
}

static FklNastNode* qsquoteProcesser(FklPtrStack* nastStack
		,uint64_t line
		,size_t* errorLine
		,const FklSid_t st[4])
{
	return nastWithHeaderProcesser(st[BUILTIN_HEAD_QSQUOTE]
			,fklMakeNastNodeRef(fklTopPtrStack(nastStack))
			,line);
}

static FklNastNode* unquoteProcesser(FklPtrStack* nastStack
		,uint64_t line
		,size_t* errorLine
		,const FklSid_t st[4])
{
	return nastWithHeaderProcesser(st[BUILTIN_HEAD_UNQUOTE]
			,fklMakeNastNodeRef(fklTopPtrStack(nastStack))
			,line);
}

static FklNastNode* unqtespProcesser(FklPtrStack* nastStack
		,uint64_t line
		,size_t* errorLine
		,const FklSid_t st[4])
{
	return nastWithHeaderProcesser(st[BUILTIN_HEAD_UNQTESP]
			,fklMakeNastNodeRef(fklTopPtrStack(nastStack))
			,line);
}

static FklNastNode* boxProcesser(FklPtrStack* nastStack
		,uint64_t line
		,size_t* errorLine
		,const FklSid_t st[4])
{
	FklNastNode* r=fklCreateNastNode(FKL_NAST_BOX,line);
	r->u.box=fklMakeNastNodeRef(fklTopPtrStack(nastStack));
	return r;
}

static FklNastNode* listProcesser(FklPtrStack* nastStack
		,uint64_t line
		,size_t* errorLine
		,const FklSid_t st[4])
{
	FklNastNode* retval=NULL;
	FklNastNode** cur=&retval;
	int r=1;
	for(size_t i=0;i<nastStack->top;i++)
	{
		FklNastNode* node=nastStack->base[i];
		(*cur)=fklCreateNastNode(FKL_NAST_PAIR,node->curline);
		(*cur)->u.pair=fklCreateNastPair();
		(*cur)->u.pair->car=fklMakeNastNodeRef(node);
		cur=&(*cur)->u.pair->cdr;
	}
	if(r&&!(*cur))
		(*cur)=fklCreateNastNode(FKL_NAST_NIL,line);
	return retval;
}

static FklNastNode* pairProcesser(FklPtrStack* nastStack
		,uint64_t line
		,size_t* errorLine
		,const FklSid_t st[4])
{
	FklNastNode* retval=NULL;
	FklNastNode** cur=&retval;
	int r=1;
	for(size_t i=0;i<nastStack->top-1;i++)
	{
		FklNastNode* node=nastStack->base[i];
		(*cur)=fklCreateNastNode(FKL_NAST_PAIR,node->curline);
		(*cur)->u.pair=fklCreateNastPair();
		(*cur)->u.pair->car=fklMakeNastNodeRef(node);
		cur=&(*cur)->u.pair->cdr;
	}
	if(r&&!(*cur))
		(*cur)=fklMakeNastNodeRef(fklTopPtrStack(nastStack));
	return retval;
}

static FklNastNode* vectorProcesser(FklPtrStack* nastStack
		,uint64_t line
		,size_t* errorLine
		,const FklSid_t st[4])
{
	FklNastNode* retval=fklCreateNastNode(FKL_NAST_VECTOR,line);
	retval->u.vec=fklCreateNastVector(nastStack->top);
	for(size_t i=0;i<nastStack->top;i++)
	{
		FklNastNode* node=nastStack->base[i];
		retval->u.vec->base[i]=fklMakeNastNodeRef(node);
	}
	return retval;
}

static FklNastNode* bytevectorProcesser(FklPtrStack* nastStack
		,uint64_t line
		,size_t* errorLine
		,const FklSid_t st[4])
{
	FklNastNode* retval=fklCreateNastNode(FKL_NAST_BYTEVECTOR,line);
	retval->u.bvec=fklCreateBytevector(nastStack->top,NULL);
	for(size_t i=0;i<nastStack->top;i++)
	{
		FklNastNode* node=nastStack->base[i];
		if(node->type==FKL_NAST_FIX
				||node->type==FKL_NAST_BIG_INT)
		{
			retval->u.bvec->ptr[i]=node->type==FKL_NAST_FIX
				?node->u.fix
				:fklBigIntToI64(node->u.bigInt);
		}
		else
		{
			*errorLine=node->curline;
			fklDestroyNastNode(retval);
			retval=NULL;
			break;
		}
	}
	return retval;
}

static FklNastNode* hashTableProcess(FklVMhashTableEqType type
		,FklPtrStack* nastStack
		,uint64_t line
		,size_t* errorLine)
{
	FklNastNode* retval=fklCreateNastNode(FKL_NAST_HASHTABLE,line);
	retval->u.hash=fklCreateNastHash(type,nastStack->top);
	for(size_t i=0;i<nastStack->top;i++)
	{
		FklNastNode* node=nastStack->base[i];
		if(node->type==FKL_NAST_PAIR)
		{
			retval->u.hash->items[i].car=fklMakeNastNodeRef(node->u.pair->car);
			retval->u.hash->items[i].cdr=fklMakeNastNodeRef(node->u.pair->cdr);
		}
		else
		{
			*errorLine=node->curline;
			fklDestroyNastNode(retval);
			retval=NULL;
			break;
		}
	}
	return retval;
}

static FklNastNode* hashEqProcesser(FklPtrStack* nodeStack
		,uint64_t line
		,size_t* errorLine
		,const FklSid_t st[4])
{
	return hashTableProcess(FKL_VM_HASH_EQ,nodeStack,line,errorLine);
}

static FklNastNode* hashEqvProcesser(FklPtrStack* nodeStack
		,uint64_t line
		,size_t* errorLine
		,const FklSid_t st[4])
{
	return hashTableProcess(FKL_VM_HASH_EQV,nodeStack,line,errorLine);
}

static FklNastNode* hashEqualProcesser(FklPtrStack* nodeStack
		,uint64_t line
		,size_t* errorLine
		,const FklSid_t st[4])
{
	return hashTableProcess(FKL_VM_HASH_EQUAL,nodeStack,line,errorLine);
}

static struct BuiltinStringPattern
{
	size_t num;
	const char** parts;
	FklNastNode* (*func)(FklPtrStack*,uint64_t,size_t*,const FklSid_t st[4]);
}BuiltinStringPatterns[]=
{
	{2,BuiltinStringPattern_quote,quoteProcesser},
	{2,BuiltinStringPattern_qsquote,qsquoteProcesser},
	{2,BuiltinStringPattern_unquote,unquoteProcesser},
	{2,BuiltinStringPattern_unqtesp,unqtespProcesser},
	{2,BuiltinStringPattern_box,boxProcesser},

	{3,BuiltinStringPattern_list_0,listProcesser},
	{3,BuiltinStringPattern_list_1,listProcesser},

	{6,BuiltinStringPattern_pair_0,pairProcesser},
	{6,BuiltinStringPattern_pair_1,pairProcesser},

	{3,BuiltinStringPattern_vector_0,vectorProcesser},
	{3,BuiltinStringPattern_vector_1,vectorProcesser},

	{3,BuiltinStringPattern_bytevector_0,bytevectorProcesser},
	{3,BuiltinStringPattern_bytevector_1,bytevectorProcesser},

	{3,BuiltinStringPattern_hash_0,hashEqProcesser},
	{3,BuiltinStringPattern_hash_1,hashEqProcesser},

	{3,BuiltinStringPattern_hasheqv_0,hashEqvProcesser},
	{3,BuiltinStringPattern_hasheqv_1,hashEqvProcesser},

	{3,BuiltinStringPattern_hashequal_0,hashEqualProcesser},
	{3,BuiltinStringPattern_hashequal_1,hashEqualProcesser},

	{0,NULL,NULL},
};

static FklNastNode* createPatternPartFromCstr(const char* s,FklSymbolTable* publicSymbolTable)
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
			r->u.sym=fklAddSymbolCstr(&s[1],publicSymbolTable)->id;
			break;
		case '&':
			r->type=FKL_NAST_BOX;
			r->u.box=fklCreateNastNode(FKL_NAST_SYM,0);
			r->u.box->u.sym=fklAddSymbolCstr(&s[1],publicSymbolTable)->id;
			break;
		default:
			FKL_ASSERT(0);
	}
	return r;
}

static FklStringMatchPattern* createBuiltinStringPattern(FklNastNode* parts
		,FklNastNode* (*func)(FklPtrStack*,uint64_t,size_t*,const FklSid_t st[4])
		,FklStringMatchPattern* next)
{
	FklStringMatchPattern* r=(FklStringMatchPattern*)malloc(sizeof(FklStringMatchPattern));
	FKL_ASSERT(r);
	r->parts=parts;
	r->type=FKL_STRING_PATTERN_BUILTIN;
	r->u.func=func;
	r->next=next;
	r->ptpool=NULL;
	r->own=1;
	return r;
}

static int isCover(const FklNastVector* p0,const FklNastVector* p1)
{
	size_t i0=0;
	size_t i1=0;
	size_t s0=p0->size;
	size_t s1=p1->size;
	FklNastNode** b0=p0->base;
	FklNastNode** b1=p1->base;
	int r=1;
	while(r&&i0<s0&&i1<s1)
	{
		switch(b0[i0]->type)
		{
			case FKL_NAST_STR:
				if(b1[i1]->type!=FKL_NAST_STR)
					r=0;
				else
				{
					FklString* s0=b0[i0]->u.str;
					FklString* s1=b1[i1]->u.str;
					r=s0->size<s1->size&&!memcmp(s0->str,s1->str,s0->size);
				}
				i0++;
				i1++;
				break;
			case FKL_NAST_SYM:
				r=b1[i1]->type!=FKL_NAST_BOX;
				i0++;
				i1++;
				break;
			case FKL_NAST_BOX:
				i1++;
				break;
			default:
				FKL_ASSERT(0);
				break;
		}
	}
	return r;
}

int fklStringPatternCoverState(const FklNastNode* p0,const FklNastNode* p1)
{
	int r=0;
	FklNastVector* v0=p0->u.vec;
	FklNastVector* v1=p1->u.vec;
	r+=isCover(v0,v1)?FKL_PATTERN_COVER:0;
	r+=isCover(v0,v1)?FKL_PATTERN_BE_COVER:0;
	return r;
}

FklStringMatchPattern* fklInitBuiltInStringPattern(FklSymbolTable* publicSymbolTable)
{
	FklStringMatchPattern* r=NULL;
	for(struct BuiltinStringPattern* cur=&BuiltinStringPatterns[0];cur->parts;cur++)
	{
		size_t num=cur->num;
		const char** curParts=cur->parts;
		FklNastNode* vec=fklCreateNastNode(FKL_NAST_VECTOR,0);
		vec->u.vec=fklCreateNastVector(num);
		for(size_t i=0;i<num;i++)
			vec->u.vec->base[i]=createPatternPartFromCstr(curParts[i],publicSymbolTable);
		r=createBuiltinStringPattern(vec,cur->func,r);
	}
	return r;
}

typedef struct
{
	FklSid_t id;
}SidHashItem;

static SidHashItem* createSidHashItem(FklSid_t key)
{
	SidHashItem* r=(SidHashItem*)malloc(sizeof(SidHashItem));
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
	return &((SidHashItem*)item)->id;
}

static FklHashTableMethodTable SidHashMethodTable=
{
	.__hashFunc=_sid_hashFunc,
	.__destroyItem=_sid_destroyItem,
	.__keyEqual=_sid_keyEqual,
	.__getKey=_sid_getKey,
};

int fklIsValidStringPattern(const FklNastNode* parts,FklHashTable** psymbolTable)
{
	size_t size=parts->u.vec->size;
	FklNastNode** base=parts->u.vec->base;
	if(size<1)
		return 0;
	if(base[0]->type!=FKL_NAST_STR)
		return 0;
	FklHashTable* symbolTable=fklCreateHashTable(8,4,2,0.75,1,&SidHashMethodTable);
	for(size_t i=1;i<size;i++)
	{
		FklNastNode* cur=base[i];
		switch(cur->type)
		{
			case FKL_NAST_SYM:
				if(fklGetHashItem((void*)&cur->u.sym,symbolTable))
				{
					fklDestroyHashTable(symbolTable);
					*psymbolTable=NULL;
					return 0;
				}
				fklPutNoRpHashItem(createSidHashItem(cur->u.sym)
						,symbolTable);
				break;
			case FKL_NAST_BOX:
				if(i==size-1
						||base[i+1]->type!=FKL_NAST_STR
						||cur->u.box->type!=FKL_NAST_SYM)
				{
					fklDestroyHashTable(symbolTable);
					*psymbolTable=NULL;
					return 0;
				}
				else
				{
					FklSid_t id=cur->u.box->u.sym;
					if(fklGetHashItem(&id,symbolTable))
					{
						fklDestroyHashTable(symbolTable);
						*psymbolTable=NULL;
						return 0;
					}
					fklPutNoRpHashItem(createSidHashItem(id)
							,symbolTable);
				}
				break;
			case FKL_NAST_STR:
				break;
			default:
				fklDestroyHashTable(symbolTable);
				*psymbolTable=NULL;
				return 0;
		}
	}
	*psymbolTable=symbolTable;
	return 1;
}

void fklInitCodegenEnvWithPatternParts(const FklNastNode* parts,FklCodegenEnv* env)
{
	size_t size=parts->u.vec->size;
	FklNastNode** base=parts->u.vec->base;
	for(size_t i=0;i<size;i++)
	{
		FklNastNode* cur=base[i];
		if(cur->type==FKL_NAST_SYM)
			fklAddCodegenDefBySid(cur->u.sym,0,env);
		else if(cur->type==FKL_NAST_BOX)
			fklAddCodegenDefBySid(cur->u.box->u.sym,0,env);
	}
}

static inline void uninit_string_match_pattern(FklStringMatchPattern* o)
{
	if(o->own)
	{
		if(o->type==FKL_STRING_PATTERN_DEFINED)
			fklDestroyByteCodelnt(o->u.proc);
		fklDestroyPrototypePool(o->ptpool);
		fklDestroyNastNode(o->parts);
	}
}

void fklAddStringMatchPattern(FklNastNode* parts
		,FklByteCodelnt* proc
		,FklStringMatchPattern** head
		,FklPrototypePool* ptpool
		,int own)
{
	int coverState=0;
	FklStringMatchPattern** phead=head;
	for(;*phead;phead=&(*phead)->next)
	{
		FklStringMatchPattern* cur=*phead;
		coverState=fklStringPatternCoverState(cur->parts,parts);
		if(coverState&&coverState!=FKL_PATTERN_BE_COVER)
			break;
	}
	if(!coverState)
		*head=fklCreateStringMatchPattern(parts,proc,ptpool,*head,own);
	else
	{
		//if(coverState==FKL_PATTERN_COVER)
		//{
		//	FklStringMatchPattern* next=*phead;
		//	*phead=fklCreateStringMatchPattern(parts,proc,ptpool,next,own);
		//}
		//else if(coverState==FKL_PATTERN_BE_COVER)
		//{
		//	FklStringMatchPattern* next=(*phead)->next;
		//	(*phead)->next=fklCreateStringMatchPattern(parts,proc,ptpool,next,own);
		//}
		//else
		if(coverState==FKL_PATTERN_EQUAL)
		{
			FklStringMatchPattern* cur=*phead;
			uninit_string_match_pattern(cur);
			cur->type=FKL_STRING_PATTERN_DEFINED;
			cur->own=own;
			cur->parts=parts;
			cur->u.proc=proc;
			cur->ptpool=ptpool;
		}
		else
		{
			FklStringMatchPattern* next=*phead;
			*phead=fklCreateStringMatchPattern(parts,proc,ptpool,next,own);
		}
	}
}

void fklDestroyStringPattern(FklStringMatchPattern* o)
{
	uninit_string_match_pattern(o);
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

void fklPatternMatchingHashTableSet(FklSid_t sid,FklNastNode* node,FklHashTable* ht)
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
	FklPtrStack s0=FKL_STACK_INIT;
	fklInitPtrStack(&s0,32,16);
	FklPtrStack s1=FKL_STACK_INIT;
	fklInitPtrStack(&s1,32,16);
	fklPushPtrStack(pattern->u.pair->cdr,&s0);
	fklPushPtrStack(exp->u.pair->cdr,&s1);
	while(!fklIsPtrStackEmpty(&s0)&&!fklIsPtrStackEmpty(&s1))
	{
		FklNastNode* n0=fklPopPtrStack(&s0);
		FklNastNode* n1=fklPopPtrStack(&s1);
		if(n0->type==FKL_NAST_SLOT)
		{
			if(ht!=NULL)
				fklPatternMatchingHashTableSet(n0->u.sym,n1,ht);
		}
		else if(n0->type==FKL_NAST_PAIR&&n1->type==FKL_NAST_PAIR)
		{
			fklPushPtrStack(n0->u.pair->cdr,&s0);
			fklPushPtrStack(n0->u.pair->car,&s0);
			fklPushPtrStack(n1->u.pair->cdr,&s1);
			fklPushPtrStack(n1->u.pair->car,&s1);
		}
		else if(!fklNastNodeEqual(n0,n1))
		{
			fklUninitPtrStack(&s0);
			fklUninitPtrStack(&s1);
			return 0;
		}
	}
	fklUninitPtrStack(&s0);
	fklUninitPtrStack(&s1);
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

int fklIsValidSyntaxPattern(const FklNastNode* p,FklHashTable** psymbolTable)
{
	if(p->type!=FKL_NAST_PAIR)
		return 0;
	FklNastNode* head=p->u.pair->car;
	if(head->type!=FKL_NAST_SYM)
		return 0;
	const FklNastNode* body=p->u.pair->cdr;
	FklHashTable* symbolTable=fklCreateHashTable(8,4,2,0.75,1,&SidHashMethodTable);
	FklPtrStack stack=FKL_STACK_INIT;
	fklInitPtrStack(&stack,32,16);
	fklPushPtrStack((void*)body,&stack);
	while(!fklIsPtrStackEmpty(&stack))
	{
		const FklNastNode* c=fklPopPtrStack(&stack);
		switch(c->type)
		{
			case FKL_NAST_PAIR:
				fklPushPtrStack(c->u.pair->cdr,&stack);
				fklPushPtrStack(c->u.pair->car,&stack);
				break;
			case FKL_NAST_SYM:
				if(fklGetHashItem((void*)&c->u.sym,symbolTable))
				{
					fklDestroyHashTable(symbolTable);
					fklUninitPtrStack(&stack);
					*psymbolTable=NULL;
					return 0;
				}
				fklPutNoRpHashItem(createSidHashItem(c->u.sym)
						,symbolTable);
				break;
			default:
				break;
		}
	}
	*psymbolTable=symbolTable;
	fklUninitPtrStack(&stack);
	return 1;
}

inline int fklPatternCoverState(const FklNastNode* p0,const FklNastNode* p1)
{
	int r=0;
	r+=fklPatternMatch(p0,p1,NULL)?FKL_PATTERN_COVER:0;
	r+=fklPatternMatch(p1,p0,NULL)?FKL_PATTERN_BE_COVER:0;
	return r;
}

void fklDestroyStringMatchState(FklStringMatchState* state)
{
	for(FklStringMatchState** pcur=&state;*pcur;)
	{
		FklStringMatchState* cur=*pcur;
		*pcur=cur->next;
		free(cur);
	}

}

void fklDestroyStringMatchSet(FklStringMatchSet* set)
{
	if(set!=NULL&&set!=FKL_STRING_PATTERN_UNIVERSAL_SET)
	{
		for(FklStringMatchSet** pset=&set;*pset;)
		{
			FklStringMatchSet* cur=*pset;
			*pset=cur->prev;
			fklDestroyStringMatchState(cur->str);
			fklDestroyStringMatchState(cur->box);
			fklDestroyStringMatchState(cur->sym);
			free(cur);
		}
	}
}

FklStringMatchRouteNode* fklCreateStringMatchRouteNode(FklStringMatchPattern* p
		,size_t s
		,size_t e
		,FklStringMatchRouteNode* parent
		,FklStringMatchRouteNode* siblings
		,FklStringMatchRouteNode* children)
{
	FklStringMatchRouteNode* r=(FklStringMatchRouteNode*)malloc(sizeof(FklStringMatchRouteNode));
	FKL_ASSERT(r);
	r->pattern=p;
	r->start=s;
	r->end=e;
	r->parent=parent;
	r->siblings=siblings;
	r->children=children;
	return r;
}

void fklInsertMatchRouteNodeAsLastChild(FklStringMatchRouteNode* p,FklStringMatchRouteNode* c)
{
	c->parent=p;
	FklStringMatchRouteNode** pnode=&p->children;
	for(;*pnode;pnode=&(*pnode)->siblings);
	*pnode=c;
}

void fklInsertMatchRouteNodeAsFirstChild(FklStringMatchRouteNode* p,FklStringMatchRouteNode* c)
{
	c->parent=p;
	c->siblings=p->children;
	p->children=c;
}

void fklDestroyStringMatchRoute(FklStringMatchRouteNode* root)
{
	FklPtrStack stack=FKL_STACK_INIT;
	fklInitPtrStack(&stack,32,16);
	fklPushPtrStack(root,&stack);
	while(!fklIsPtrStackEmpty(&stack))
	{
		FklStringMatchRouteNode* cur=fklPopPtrStack(&stack);
		if(cur)
		{
			fklPushPtrStack(cur->siblings,&stack);
			fklPushPtrStack(cur->children,&stack);
			free(cur);
		}
	}
	fklUninitPtrStack(&stack);
}

inline static void printStringMatchRoute(FklStringMatchRouteNode* root,FILE* fp,uint32_t depth)
{
	for(uint32_t i=1;i<depth;i++)
		fprintf(fp,"|   ");
	if(depth)
		fprintf(fp,"+---");
	fprintf(fp,"[%p,%lu,%lu]",root->pattern,root->start,root->end);
	if(root->children)
	{
		//putc('{',fp);
		//putc(':',fp);
		for(FklStringMatchRouteNode* cur=root->children;cur;cur=cur->siblings)
		{
			putc('\n',fp);
			printStringMatchRoute(cur,fp,depth+1);
		}
		for(uint32_t i=0;i<depth;i++)
			fprintf(fp,"    ");
		//putc('}',fp);
	}
}

void fklPrintStringMatchRoute(FklStringMatchRouteNode* root,FILE* fp)
{
//	FklPtrStack stack=FKL_STACK_INIT;
//	fklInitPtrStack(&stack,32,16);
//	while(!fklIsPtrStackEmpty(&stack))
//	{
//		FklStringMatchRouteNode* cur=fklPopPtrStack(&stack);
//	}
//	fklUninitPtrStack(&stack);
	printStringMatchRoute(root,fp,0);
}

static inline int is_valid_pattern_nast(const FklNastNode* p)
{
	if(p->type!=FKL_NAST_PAIR)
		return 0;
	if(p->u.pair->car->type!=FKL_NAST_SYM)
		return 0;
	return 1;
}

static inline int is_pattern_slot(FklSid_t s,const FklNastNode* p)
{
	return p->type==FKL_NAST_PAIR
		&&p->u.pair->cdr->type==FKL_NAST_PAIR
		&&p->u.pair->cdr->u.pair->cdr->type==FKL_NAST_NIL
		&&p->u.pair->car->type==FKL_NAST_SYM
		&&p->u.pair->car->u.sym==s
		&&p->u.pair->cdr->u.pair->car->type==FKL_NAST_SYM;
}

FklNastNode* fklCreatePatternFromNast(FklNastNode* node,FklHashTable** psymbolTable)
{
	FklNastNode* r=NULL;
	if(node->type==FKL_NAST_PAIR
			&&fklIsNastNodeList(node)
			&&node->u.pair->car->type==FKL_NAST_SYM
			&&node->u.pair->cdr->type==FKL_NAST_PAIR
			&&node->u.pair->cdr->u.pair->cdr->type==FKL_NAST_NIL
			&&is_valid_pattern_nast(node->u.pair->cdr->u.pair->car))
	{
		FklHashTable* symbolTable=fklCreateHashTable(8,4,2,0.75,1,&SidHashMethodTable);
		FklNastNode* exp=node->u.pair->cdr->u.pair->car;
		FklSid_t slotId=node->u.pair->car->u.sym;
		FklNastNode* rest=exp->u.pair->cdr;

		FklPtrStack stack=FKL_STACK_INIT;
		fklInitPtrStack(&stack,32,16);
		fklPushPtrStack((void*)rest,&stack);
		while(!fklIsPtrStackEmpty(&stack))
		{
			FklNastNode* c=fklPopPtrStack(&stack);
			if(c->type==FKL_NAST_PAIR)
			{
				if(is_pattern_slot(slotId,c))
				{
					FklSid_t sym=c->u.pair->cdr->u.pair->car->u.sym;
					if(fklGetHashItem((void*)&sym,symbolTable))
					{
						fklDestroyHashTable(symbolTable);
						fklUninitPtrStack(&stack);
						*psymbolTable=NULL;
						return 0;
					}
					fklDestroyNastNode(c->u.pair->car);
					fklDestroyNastNode(c->u.pair->cdr);
					free(c->u.pair);
					c->type=FKL_NAST_SLOT;
					c->u.sym=sym;
					fklPutNoRpHashItem(createSidHashItem(sym),symbolTable);
				}
				else
				{
					fklPushPtrStack(c->u.pair->cdr,&stack);
					fklPushPtrStack(c->u.pair->car,&stack);
				}
			}
		}
		r=exp;
		fklUninitPtrStack(&stack);
		if(psymbolTable)
			*psymbolTable=symbolTable;
		else
			fklDestroyHashTable(symbolTable);
	}
	return r;
}
