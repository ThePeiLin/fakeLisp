#include<fakeLisp/parser.h>
#include<fakeLisp/utils.h>
#include<ctype.h>

FklNastNode* fklCreateNastNode(FklNastType type,uint64_t line)
{
	FklNastNode* r=(FklNastNode*)malloc(sizeof(FklNastNode));
	FKL_ASSERT(r);
	r->curline=line;
	r->type=type;
	r->str=NULL;
	r->refcount=0;
	return r;
}

FklNastPair* fklCreateNastPair(void)
{
	FklNastPair* pair=(FklNastPair*)malloc(sizeof(FklNastPair));
	FKL_ASSERT(pair);
	pair->car=NULL;
	pair->cdr=NULL;
	return pair;
}

FklNastVector* fklCreateNastVector(size_t size)
{
	FklNastVector* vec=(FklNastVector*)malloc(sizeof(FklNastVector));
	FKL_ASSERT(vec);
	vec->size=size;
	vec->base=(FklNastNode**)calloc(size,sizeof(FklNastNode*));
	FKL_ASSERT(vec->base);
	return vec;
}

FklNastHashTable* fklCreateNastHash(FklHashTableEqType type,size_t num)
{
	FklNastHashTable* r=(FklNastHashTable*)malloc(sizeof(FklNastHashTable));
	FKL_ASSERT(r);
	r->items=(FklNastPair*)calloc(sizeof(FklNastPair),num);
	FKL_ASSERT(r->items);
	r->num=num;
	r->type=type;
	return r;
}

size_t fklNastListLength(const FklNastNode* list)
{
	size_t i=0;
	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr,i++);
	return i;
}

FklNastNode* fklMakeNastNodeRef(FklNastNode* n)
{
	n->refcount+=1;
	return n;
}

static void destroyNastPair(FklNastPair* pair)
{
	free(pair);
}

static void destroyNastVector(FklNastVector* vec)
{
	free(vec->base);
	free(vec);
}

static void destroyNastHash(FklNastHashTable* hash)
{
	free(hash->items);
	free(hash);
}

void fklDestroyNastNode(FklNastNode* node)
{
	FklPtrStack stack=FKL_STACK_INIT;
	fklInitPtrStack(&stack,32,16);
	fklPushPtrStack(node,&stack);
	while(!fklIsPtrStackEmpty(&stack))
	{
		FklNastNode* cur=fklPopPtrStack(&stack);
		if(cur)
		{
			if(!cur->refcount)
			{
				switch(cur->type)
				{
					case FKL_NAST_FIX:
					case FKL_NAST_SYM:
					case FKL_NAST_SLOT:
					case FKL_NAST_NIL:
					case FKL_NAST_CHR:
					case FKL_NAST_F64:
						break;
					case FKL_NAST_STR:
						free(cur->str);
						break;
					case FKL_NAST_BYTEVECTOR:
						free(cur->bvec);
						break;
					case FKL_NAST_BIG_INT:
						fklDestroyBigInt(cur->bigInt);
						break;
					case FKL_NAST_PAIR:
						fklPushPtrStack(cur->pair->car,&stack);
						fklPushPtrStack(cur->pair->cdr,&stack);
						destroyNastPair(cur->pair);
						break;
					case FKL_NAST_BOX:
						fklPushPtrStack(cur->box,&stack);
						break;
					case FKL_NAST_VECTOR:
						for(size_t i=0;i<cur->vec->size;i++)
							fklPushPtrStack(cur->vec->base[i],&stack);
						destroyNastVector(cur->vec);
						break;
					case FKL_NAST_HASHTABLE:
						for(size_t i=0;i<cur->hash->num;i++)
						{
							fklPushPtrStack(cur->hash->items[i].car,&stack);
							fklPushPtrStack(cur->hash->items[i].cdr,&stack);
						}
						destroyNastHash(cur->hash);
						break;
					default:
						FKL_ASSERT(0);
						break;
				}
				free(cur);
			}
			else
				cur->refcount--;
		}
	}
	fklUninitPtrStack(&stack);
}

