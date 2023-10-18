#include<fakeLisp/pattern.h>
#include<fakeLisp/codegen.h>
#include<fakeLisp/vm.h>
#include<string.h>
#include<ctype.h>

FklNastNode* fklPatternMatchingHashTableRef(FklSid_t sid,FklHashTable* ht)
{
	FklPatternMatchingHashTableItem* item=fklGetHashItem(&sid,ht);
	return item->node;
}

void fklPatternMatchingHashTableSet(FklSid_t sid,FklNastNode* node,FklHashTable* ht)
{
	FklPatternMatchingHashTableItem* item=fklPutHashItem(&sid,ht);
	item->node=node;
}

int fklPatternMatch(const FklNastNode* pattern,const FklNastNode* exp,FklHashTable* ht)
{
	if(exp->type!=FKL_NAST_PAIR)
		return 0;
	if(exp->pair->car->type!=FKL_NAST_SYM
			||pattern->pair->car->sym!=exp->pair->car->sym)
		return 0;
	FklPtrStack s0=FKL_STACK_INIT;
	fklInitPtrStack(&s0,32,16);
	FklPtrStack s1=FKL_STACK_INIT;
	fklInitPtrStack(&s1,32,16);
	fklPushPtrStack(pattern->pair->cdr,&s0);
	fklPushPtrStack(exp->pair->cdr,&s1);
	while(!fklIsPtrStackEmpty(&s0)&&!fklIsPtrStackEmpty(&s1))
	{
		FklNastNode* n0=fklPopPtrStack(&s0);
		FklNastNode* n1=fklPopPtrStack(&s1);
		if(n0->type==FKL_NAST_SLOT)
		{
			if(ht!=NULL)
				fklPatternMatchingHashTableSet(n0->sym,n1,ht);
		}
		else if(n0->type==FKL_NAST_PAIR&&n1->type==FKL_NAST_PAIR)
		{
			fklPushPtrStack(n0->pair->cdr,&s0);
			fklPushPtrStack(n0->pair->car,&s0);
			fklPushPtrStack(n1->pair->cdr,&s1);
			fklPushPtrStack(n1->pair->car,&s1);
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

static uintptr_t _pattern_matching_hash_table_hash_func(const void* key)
{
	FklSid_t sid=*(const FklSid_t*)key;
	return sid;
}

static int _pattern_matching_hash_key_equal(const void* pk0,const void* pk1)
{
	FklSid_t k0=*(const FklSid_t*)pk0;
	FklSid_t k1=*(const FklSid_t*)pk1;
	return k0==k1;
}

static void _pattern_match_hash_set_key(void* k0,const void* k1)
{
	*(FklSid_t*)k0=*(const FklSid_t*)k1;
}

static void _pattern_match_hash_set_val(void* d0,const void* d1)
{
	*(FklPatternMatchingHashTableItem*)d0=*(const FklPatternMatchingHashTableItem*)d1;
}

static FklHashTableMetaTable Codegen_hash_meta_table=
{
	.size=sizeof(FklPatternMatchingHashTableItem),
	.__setKey=_pattern_match_hash_set_key,
	.__setVal=_pattern_match_hash_set_val,
	.__hashFunc=_pattern_matching_hash_table_hash_func,
	.__uninitItem=fklDoNothingUninitHashItem,
	.__keyEqual=_pattern_matching_hash_key_equal,
	.__getKey=fklHashDefaultGetKey,
};

FklHashTable* fklCreatePatternMatchingHashTable(void)
{
	return fklCreateHashTable(&Codegen_hash_meta_table);
}

void fklInitPatternMatchHashTable(FklHashTable* ht)
{
	fklInitHashTable(ht,&Codegen_hash_meta_table);
}

static inline int is_pattern_equal(const FklNastNode* pattern,const FklNastNode* exp)
{
	if(exp->type!=FKL_NAST_PAIR)
		return 0;
	if(exp->pair->car->type!=FKL_NAST_SYM
			||pattern->pair->car->sym!=exp->pair->car->sym)
		return 0;
	FklPtrStack s0=FKL_STACK_INIT;
	fklInitPtrStack(&s0,32,16);
	FklPtrStack s1=FKL_STACK_INIT;
	fklInitPtrStack(&s1,32,16);
	fklPushPtrStack(pattern->pair->cdr,&s0);
	fklPushPtrStack(exp->pair->cdr,&s1);
	int r=1;
	while(r&&!fklIsPtrStackEmpty(&s0)&&!fklIsPtrStackEmpty(&s1))
	{
		FklNastNode* n0=fklPopPtrStack(&s0);
		FklNastNode* n1=fklPopPtrStack(&s1);
		if(n0->type!=n1->type)
			r=0;
		else if(n0->type==FKL_NAST_SLOT)
			continue;
		else if(n0->type==FKL_NAST_PAIR&&n1->type==FKL_NAST_PAIR)
		{
			fklPushPtrStack(n0->pair->cdr,&s0);
			fklPushPtrStack(n0->pair->car,&s0);
			fklPushPtrStack(n1->pair->cdr,&s1);
			fklPushPtrStack(n1->pair->car,&s1);
		}
		else if(!fklNastNodeEqual(n0,n1))
			r=0;
	}
	fklUninitPtrStack(&s0);
	fklUninitPtrStack(&s1);
	return r;
}

static inline int is_partly_covered(const FklNastNode* pattern,const FklNastNode* exp)
{
	int r=0;
	if(exp->type!=FKL_NAST_PAIR)
		return r;
	if(exp->pair->car->type!=FKL_NAST_SYM
			||pattern->pair->car->sym!=exp->pair->car->sym)
		return r;
	FklPtrStack s0=FKL_STACK_INIT;
	fklInitPtrStack(&s0,32,16);
	FklPtrStack s1=FKL_STACK_INIT;
	fklInitPtrStack(&s1,32,16);
	fklPushPtrStack(pattern->pair->cdr,&s0);
	fklPushPtrStack(exp->pair->cdr,&s1);
	while(!fklIsPtrStackEmpty(&s0)&&!fklIsPtrStackEmpty(&s1))
	{
		FklNastNode* n0=fklPopPtrStack(&s0);
		FklNastNode* n1=fklPopPtrStack(&s1);
		if(n0->type==FKL_NAST_SLOT)
		{
			r=1;
			break;
		}
		else if(n0->type==FKL_NAST_PAIR&&n1->type==FKL_NAST_PAIR)
		{
			fklPushPtrStack(n0->pair->cdr,&s0);
			fklPushPtrStack(n0->pair->car,&s0);
			fklPushPtrStack(n1->pair->cdr,&s1);
			fklPushPtrStack(n1->pair->car,&s1);
		}
		else if(!fklNastNodeEqual(n0,n1))
			break;
	}
	fklUninitPtrStack(&s0);
	fklUninitPtrStack(&s1);
	return r;
}

int fklPatternCoverState(const FklNastNode* p0,const FklNastNode* p1)
{
	if(is_pattern_equal(p0,p1))
		return FKL_PATTERN_EQUAL;
	else if(is_partly_covered(p0,p1))
		return FKL_PATTERN_COVER;
	else if(is_partly_covered(p1,p0))
		return FKL_PATTERN_BE_COVER;
	else
		return FKL_PATTERN_NOT_EQUAL;
}

static inline int is_valid_pattern_nast(const FklNastNode* p)
{
	if(p->type!=FKL_NAST_PAIR)
		return 0;
	if(p->pair->car->type!=FKL_NAST_SYM)
		return 0;
	return 1;
}

static inline int is_pattern_slot(FklSid_t s,const FklNastNode* p)
{
	return p->type==FKL_NAST_PAIR
		&&p->pair->cdr->type==FKL_NAST_PAIR
		&&p->pair->cdr->pair->cdr->type==FKL_NAST_NIL
		&&p->pair->car->type==FKL_NAST_SYM
		&&p->pair->car->sym==s
		&&p->pair->cdr->pair->car->type==FKL_NAST_SYM;
}

FklNastNode* fklCreatePatternFromNast(FklNastNode* node,FklHashTable** psymbolTable)
{
	FklNastNode* r=NULL;
	if(node->type==FKL_NAST_PAIR
			&&fklIsNastNodeList(node)
			&&node->pair->car->type==FKL_NAST_SYM
			&&node->pair->cdr->type==FKL_NAST_PAIR
			&&node->pair->cdr->pair->cdr->type==FKL_NAST_NIL
			&&is_valid_pattern_nast(node->pair->cdr->pair->car))
	{
		FklHashTable* symbolTable=fklCreateSidSet();
		FklNastNode* exp=fklCopyNastNode(node->pair->cdr->pair->car);
		FklSid_t slotId=node->pair->car->sym;
		FklNastNode* rest=exp->pair->cdr;

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
					FklSid_t sym=c->pair->cdr->pair->car->sym;
					if(fklGetHashItem(&sym,symbolTable))
					{
						fklDestroyHashTable(symbolTable);
						fklUninitPtrStack(&stack);
						*psymbolTable=NULL;
						fklDestroyNastNode(exp);
						return NULL;
					}
					fklDestroyNastNode(c->pair->car);
					fklDestroyNastNode(c->pair->cdr);
					free(c->pair);
					c->type=FKL_NAST_SLOT;
					c->sym=sym;
					fklPutHashItem(&sym,symbolTable);
				}
				else
				{
					fklPushPtrStack(c->pair->cdr,&stack);
					fklPushPtrStack(c->pair->car,&stack);
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
