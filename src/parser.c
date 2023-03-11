#include<fakeLisp/parser.h>
#include<fakeLisp/codegen.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/lexer.h>
#include<fakeLisp/pattern.h>
#include<fakeLisp/vm.h>
#include<string.h>
#include<ctype.h>

FklNastNode* fklCreateNastNodeFromCstr(const char* cStr
		,const FklSid_t buildInHeadSymbolTable[4]
		,FklStringMatchPattern* patterns
		,FklSymbolTable* publicSymbolTable)
{
	FklPtrStack tokenStack=FKL_STACK_INIT;
	fklInitPtrStack(&tokenStack,8,16);
	size_t size=strlen(cStr);
	size_t line=0;
	size_t j=0;
	FklStringMatchSet* matchSet=FKL_STRING_PATTERN_UNIVERSAL_SET;
	FklStringMatchRouteNode* route=fklCreateStringMatchRouteNode(NULL,0,0,NULL,NULL,NULL);
	FklStringMatchRouteNode* tmp=route;
	int err=0;
	matchSet=fklSplitStringIntoTokenWithPattern(cStr
			,size
			,line
			,&line
			,j
			,&j
			,&tokenStack
			,matchSet
			,patterns
			,route
			,&tmp
			,&err);
	FklNastNode* retval=NULL;
	if(!matchSet)
	{
		size_t errorLine=0;
		retval=fklCreateNastNodeFromTokenStackAndMatchRoute(&tokenStack
				,route
				,&errorLine
				,buildInHeadSymbolTable
				,NULL
				,publicSymbolTable);
	}
	else
		fklDestroyStringMatchSet(matchSet);
	while(!fklIsPtrStackEmpty(&tokenStack))
		fklDestroyToken(fklPopPtrStack(&tokenStack));
	fklDestroyStringMatchRoute(route);
	fklUninitPtrStack(&tokenStack);
	return retval;
}

FklNastNode* fklCreateNastNode(FklNastType type,uint64_t line)
{
	FklNastNode* r=(FklNastNode*)malloc(sizeof(FklNastNode));
	FKL_ASSERT(r);
	r->curline=line;
	r->type=type;
	r->u.str=NULL;
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

static int fklIsValidCharStr(const char* str,size_t len)
{
	if(len==0)
		return 0;
	if(isalpha(str[0])&&len>1)
		return 0;
	if(str[0]=='\\')
	{
		if(len<2)
			return 1;
		if(toupper(str[1])=='X')
		{
			if(len<3||len>4)
				return 0;
			for(size_t i=2;i<len;i++)
				if(!isxdigit(str[i]))
					return 0;
		}
		else if(str[1]=='0')
		{
			if(len>5)
				return 0;
			if(len>2)
			{
				for(size_t i=2;i<len;i++)
					if(!isdigit(str[i])||str[i]>'7')
						return 0;
			}
		}
		else if(isdigit(str[1]))
		{
			if(len>4)
				return 0;
			for(size_t i=1;i<len;i++)
				if(!isdigit(str[i]))
					return 0;
		}
	}
	return 1;
}

static FklNastNode* createChar(const FklString* oStr,uint64_t line,FklSymbolTable* table)
{
	if(!fklIsValidCharStr(oStr->str+2,oStr->size-2))
		return NULL;
	FklNastNode* r=fklCreateNastNode(FKL_NAST_CHR,line);
	r->u.chr=fklCharBufToChar(oStr->str+2,oStr->size-2);
	return r;
}

static FklNastNode* createNum(const FklString* oStr,uint64_t line,FklSymbolTable* table)
{
	FklNastNode* r=NULL;
	if(fklIsDoubleString(oStr))
	{
		r=fklCreateNastNode(FKL_NAST_F64,line);
		r->u.f64=fklStringToDouble(oStr);
	}
	else
	{
		r=fklCreateNastNode(FKL_NAST_FIX,line);
		FklBigInt bInt=FKL_BIG_INT_INIT;
		fklInitBigIntFromString(&bInt,oStr);
		if(fklIsGtLtFixBigInt(&bInt))
		{
			FklBigInt* bi=(FklBigInt*)malloc(sizeof(FklBigInt));
			FKL_ASSERT(bi);
			*bi=bInt;
			r->u.bigInt=bi;
			r->type=FKL_NAST_BIG_INT;
		}
		else
		{
			int64_t num=fklBigIntToI64(&bInt);
			r->type=FKL_NAST_FIX;
			r->u.fix=num;
			fklUninitBigInt(&bInt);
		}
	}
	return r;
}

static FklNastNode* createString(const FklString* oStr,uint64_t line,FklSymbolTable* table)
{
	FklNastNode* r=fklCreateNastNode(FKL_NAST_STR,line);
	r->u.str=fklCopyString(oStr);
	return r;
}

static FklNastNode* createSymbol(const FklString* oStr,uint64_t line,FklSymbolTable* publicSymbolTable)
{
	FklNastNode* r=fklCreateNastNode(FKL_NAST_SYM,line);
	r->u.sym=fklAddSymbol(oStr,publicSymbolTable)->id;
	return r;
}

static FklNastNode* (*literalNodeCreator[])(const FklString*,uint64_t,FklSymbolTable*)=
{
	createChar,
	createNum,
	createString,
	createSymbol,
};

typedef enum
{
	NAST_CAR,
	NAST_CDR,
	NAST_BOX,
	NAST_HASH_ITEM,
}NastPlace;

typedef struct
{
	NastPlace place;
	FklNastNode* node;
}NastElem;

static NastElem* createNastElem(NastPlace place,FklNastNode* node)
{
	NastElem* r=(NastElem*)malloc(sizeof(NastElem));
	FKL_ASSERT(r);
	r->node=node;
	r->place=place;
	return r;
}

static void destroyNastElem(NastElem* n)
{
	free(n);
}

typedef struct
{
	FklStringMatchPattern* pattern;
	uint32_t line;
	uint32_t cindex;
}MatchState;

FklNastHashTable* fklCreateNastHash(FklVMhashTableEqType type,size_t num)
{
	FklNastHashTable* r=(FklNastHashTable*)malloc(sizeof(FklNastHashTable));
	FKL_ASSERT(r);
	r->items=(FklNastPair*)calloc(sizeof(FklNastPair),num);
	FKL_ASSERT(r->items);
	r->num=num;
	r->type=type;
	return r;
}

void fklPrintNastNode(const FklNastNode* exp
		,FILE* fp
		,FklSymbolTable* table)
{
	FklPtrQueue* queue=fklCreatePtrQueue();
	FklPtrStack queueStack=FKL_STACK_INIT;
	fklInitPtrStack(&queueStack,32,16);
	fklPushPtrQueue(createNastElem(NAST_CAR,(FklNastNode*)exp),queue);
	fklPushPtrStack(queue,&queueStack);
	while(!fklIsPtrStackEmpty(&queueStack))
	{
		FklPtrQueue* cQueue=fklTopPtrStack(&queueStack);
		while(fklLengthPtrQueue(cQueue))
		{
			NastElem* e=fklPopPtrQueue(cQueue);
			if(e->place==NAST_CDR)
				fputc(',',fp);
			else if(e->place==NAST_HASH_ITEM)
			{
				FklNastPair* t=(FklNastPair*)e->node;
				fputc('(',fp);
				FklPtrQueue* iQueue=fklCreatePtrQueue();
				fklPushPtrQueue(createNastElem(NAST_CAR,t->car),iQueue);
				fklPushPtrQueue(createNastElem(NAST_CDR,t->cdr),iQueue);
				fklPushPtrStack(iQueue,&queueStack);
				cQueue=iQueue;
				destroyNastElem(e);
				continue;
			}
			FklNastNode* node=e->node;
			destroyNastElem(e);
			switch(node->type)
			{
				case FKL_NAST_SYM:
					fklPrintRawSymbol(fklGetSymbolWithId(node->u.sym,table)->symbol,fp);
					break;
				case FKL_NAST_BYTEVECTOR:
					fklPrintRawBytevector(node->u.bvec,fp);
					break;
				case FKL_NAST_STR:
					fklPrintRawString(node->u.str,fp);
					break;
				case FKL_NAST_FIX:
					fprintf(fp,"%ld",node->u.fix);
					break;
				case FKL_NAST_F64:
					fprintf(fp,"%lf",node->u.f64);
					break;
				case FKL_NAST_CHR:
					fklPrintRawChar(node->u.chr,fp);
					break;
				case FKL_NAST_VECTOR:
					fputs("#(",fp);
					{
						FklPtrQueue* vQueue=fklCreatePtrQueue();
						for(size_t i=0;i<node->u.vec->size;i++)
							fklPushPtrQueue(createNastElem(NAST_CAR,node->u.vec->base[i]),vQueue);
						fklPushPtrStack(vQueue,&queueStack);
						cQueue=vQueue;
						continue;
					}
					break;
				case FKL_NAST_BIG_INT:
					fklPrintBigInt(node->u.bigInt,fp);
					break;
				case FKL_NAST_BOX:
					fputs("#&",fp);
					{
						FklPtrQueue* bQueue=fklCreatePtrQueue();
						fklPushPtrQueue(createNastElem(NAST_BOX,node->u.box),bQueue);
						fklPushPtrStack(bQueue,&queueStack);
						cQueue=bQueue;
					}
					break;
				case FKL_NAST_HASHTABLE:
					{
						const static char* tmp[]=
						{
							"#hash(",
							"#hasheqv(",
							"#hashequal(",
						};
						fputs(tmp[node->u.hash->type],fp);
						FklPtrQueue* vQueue=fklCreatePtrQueue();
						for(size_t i=0;i<node->u.hash->num;i++)
							fklPushPtrQueue(createNastElem(NAST_HASH_ITEM
										,(FklNastNode*)&node->u.hash->items[i]),vQueue);
						fklPushPtrStack(vQueue,&queueStack);
						cQueue=vQueue;
						continue;
					}
					break;
				case FKL_NAST_NIL:
					fputs("()",fp);
					break;
				case FKL_NAST_PAIR:
					{
						fputc('(',fp);
						FklPtrQueue* lQueue=fklCreatePtrQueue();
						FklNastNode* cur=node;
						for(;cur->type==FKL_NAST_PAIR;cur=cur->u.pair->cdr)
						{
							FklNastNode* c=cur->u.pair->car;
							NastElem* ce=createNastElem(NAST_CAR,c);
							fklPushPtrQueue(ce,lQueue);
						}
						if(cur->type!=FKL_NAST_NIL)
						{
							NastElem* cdre=createNastElem(NAST_CDR,cur);
							fklPushPtrQueue(cdre,lQueue);
						}
						fklPushPtrStack(lQueue,&queueStack);
						cQueue=lQueue;
						continue;
					}
					break;
				default:
					FKL_ASSERT(0);
					break;
			}
			if(fklLengthPtrQueue(cQueue)&&((NastElem*)fklFirstPtrQueue(cQueue))->place!=NAST_CDR&&((NastElem*)fklFirstPtrQueue(cQueue))->place!=NAST_BOX)
				fputc(' ',fp);
		}
		fklPopPtrStack(&queueStack);
		fklDestroyPtrQueue(cQueue);
		if(!fklIsPtrStackEmpty(&queueStack))
		{
			fputc(')',fp);
			cQueue=fklTopPtrStack(&queueStack);
			if(fklLengthPtrQueue(cQueue)&&((NastElem*)fklFirstPtrQueue(cQueue))->place!=NAST_CDR)
				fputc(' ',fp);
		}
	}
	fklUninitPtrStack(&queueStack);
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
						free(cur->u.str);
						break;
					case FKL_NAST_BYTEVECTOR:
						free(cur->u.bvec);
						break;
					case FKL_NAST_BIG_INT:
						fklDestroyBigInt(cur->u.bigInt);
						break;
					case FKL_NAST_PAIR:
						fklPushPtrStack(cur->u.pair->car,&stack);
						fklPushPtrStack(cur->u.pair->cdr,&stack);
						destroyNastPair(cur->u.pair);
						break;
					case FKL_NAST_BOX:
						fklPushPtrStack(cur->u.box,&stack);
						break;
					case FKL_NAST_VECTOR:
						for(size_t i=0;i<cur->u.vec->size;i++)
							fklPushPtrStack(cur->u.vec->base[i],&stack);
						destroyNastVector(cur->u.vec);
						break;
					case FKL_NAST_HASHTABLE:
						for(size_t i=0;i<cur->u.hash->num;i++)
						{
							fklPushPtrStack(cur->u.hash->items[i].car,&stack);
							fklPushPtrStack(cur->u.hash->items[i].cdr,&stack);
						}
						destroyNastHash(cur->u.hash);
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

int fklIsNastNodeList(const FklNastNode* list)
{
	for(;list->type==FKL_NAST_PAIR;list=list->u.pair->cdr);
	return list->type==FKL_NAST_NIL;
}

int fklIsNastNodeListAndHasSameType(const FklNastNode* list,FklNastType type)
{
	for(;list->type==FKL_NAST_PAIR;list=list->u.pair->cdr)
		if(list->u.pair->car->type!=type)
			return 0;
	return list->type==FKL_NAST_NIL;
}

FklNastNode* fklMakeNastNodeRef(FklNastNode* n)
{
	n->refcount+=1;
	return n;
}

int fklNastNodeEqual(const FklNastNode* n0,const FklNastNode* n1)
{
	FklPtrStack s0=FKL_STACK_INIT;
	fklInitPtrStack(&s0,16,16);
	FklPtrStack s1=FKL_STACK_INIT;
	fklInitPtrStack(&s1,16,16);
	fklPushPtrStack((void*)n0,&s0);
	fklPushPtrStack((void*)n1,&s1);
	int r=1;
	while(!fklIsPtrStackEmpty(&s0)&&!fklIsPtrStackEmpty(&s1))
	{
		const FklNastNode* c0=fklPopPtrStack(&s0);
		const FklNastNode* c1=fklPopPtrStack(&s1);
		if(c0->type!=c1->type)
			r=0;
		else
		{
			switch(c0->type)
			{
				case FKL_NAST_SYM:
					r=c0->u.sym==c1->u.sym;
					break;
				case FKL_NAST_FIX:
					r=c0->u.fix==c1->u.fix;
					break;
				case FKL_NAST_F64:
					r=c0->u.f64==c1->u.f64;
					break;
				case FKL_NAST_NIL:
					break;
				case FKL_NAST_STR:
					r=!fklStringcmp(c0->u.str,c1->u.str);
					break;
				case FKL_NAST_BYTEVECTOR:
					r=!fklBytevectorcmp(c0->u.bvec,c1->u.bvec);
					break;
				case FKL_NAST_CHR:
					r=c0->u.chr==c1->u.chr;
					break;
				case FKL_NAST_BIG_INT:
					r=fklCmpBigInt(c0->u.bigInt,c1->u.bigInt);
					break;
				case FKL_NAST_BOX:
					fklPushPtrStack(c0->u.box,&s0);
					fklPushPtrStack(c1->u.box,&s1);
					break;
				case FKL_NAST_VECTOR:
					r=c0->u.vec->size==c1->u.vec->size;
					if(r)
					{
						for(size_t i=0;i<c0->u.vec->size;i++)
							fklPushPtrStack(c0->u.vec->base[i],&s0);
						for(size_t i=0;i<c1->u.vec->size;i++)
							fklPushPtrStack(c1->u.vec->base[i],&s1);
					}
					break;
				case FKL_NAST_HASHTABLE:
					r=c0->u.hash->type==c1->u.hash->type&&c0->u.hash->num==c1->u.hash->num;
					if(r)
					{
						for(size_t i=0;i<c0->u.hash->num;i++)
						{
							fklPushPtrStack(c0->u.hash->items[i].car,&s0);
							fklPushPtrStack(c0->u.hash->items[i].cdr,&s0);
						}
						for(size_t i=0;i<c1->u.hash->num;i++)
						{
							fklPushPtrStack(c1->u.hash->items[i].car,&s1);
							fklPushPtrStack(c1->u.hash->items[i].cdr,&s1);
						}
					}
					break;
				case FKL_NAST_PAIR:
					fklPushPtrStack(c0->u.pair->car,&s0);
					fklPushPtrStack(c0->u.pair->cdr,&s0);
					fklPushPtrStack(c1->u.pair->car,&s1);
					fklPushPtrStack(c1->u.pair->cdr,&s1);
					break;
				case FKL_NAST_SLOT:
					r=1;
					break;
			}
		}
		if(!r)
			break;
	}
	fklUninitPtrStack(&s0);
	fklUninitPtrStack(&s1);
	return r;
}

inline static FklToken* getSingleToken(FklStringMatchRouteNode* routeNode
		,FklPtrStack* tokenStack)
{
	return tokenStack->base[routeNode->start];
}

typedef struct CreateNastNodeQuest
{
	uint64_t line;
	FklStringMatchPattern* pattern;
	FklStringMatchRouteNode* routeNode;
	FklPtrStack* route;
	FklPtrStack* nast;
}CreateNastNodeQuest;

static CreateNastNodeQuest* createNastNodeQuest(uint64_t line
		,FklStringMatchRouteNode* routeNode
		,FklStringMatchPattern* pattern
		,FklPtrStack* route)
{
	CreateNastNodeQuest* r=(CreateNastNodeQuest*)malloc(sizeof(CreateNastNodeQuest));
	FKL_ASSERT(r);
	r->line=line;
	r->pattern=pattern;
	r->routeNode=routeNode;
	r->route=route;
	r->nast=fklCreatePtrStack(8,16);
	return r;
}

static void destroyNastNodeQuest(CreateNastNodeQuest* p)
{
	fklDestroyPtrStack(p->nast);
	fklDestroyPtrStack(p->route);
	free(p);
}

static int isNoSeperatorBetween(const FklStringMatchRouteNode* a
		,const FklStringMatchRouteNode* b
		,FklPtrStack* tokenStack)
{
	size_t end=a->start;
	FklToken** base=(FklToken**)tokenStack->base;
	for(size_t i=b->end+1;i<end;i++)
		if(base[i]->type==FKL_TOKEN_RESERVE_STR)
			return 0;
	return 1;
}

static size_t getNumOfList(size_t index
		,FklStringMatchRouteNode** base
		,FklPtrStack* tokenStack)
{
	size_t i=0;
	for(;index>0
			&&(index<2
				||isNoSeperatorBetween(base[index-2],base[index-1],tokenStack))
			;index--
			,i++);
	return i;
}

static inline FklNastNode* create_nast_list_and_make_ref(FklNastNode** a,size_t num,uint64_t line)
{
	FklNastNode* r=NULL;
	FklNastNode** cur=&r;
	for(size_t i=0;i<num;i++)
	{
		(*cur)=fklCreateNastNode(FKL_NAST_PAIR,a[i]->curline);
		(*cur)->u.pair=fklCreateNastPair();
		(*cur)->u.pair->car=fklMakeNastNodeRef(a[i]);
		cur=&(*cur)->u.pair->cdr;
	}
	(*cur)=fklCreateNastNode(FKL_NAST_NIL,line);
	return r;
}

static FklHashTable* processNastStackWithPatternParts(FklNastNode* parts
		,FklStringMatchRouteNode* route
		,FklPtrStack* nastStack
		,FklPtrStack* tokenStack)
{
	FklHashTable* ht=fklCreatePatternMatchingHashTable();
	size_t partsNum=parts->u.vec->size;
	FklNastNode** partsBase=parts->u.vec->base;
	FklNastNode** nastBase=(FklNastNode**)nastStack->base;
	size_t partIndex=1;
	size_t nastIndex=0;
	FklPtrStack routeStack=FKL_STACK_INIT;
	fklInitPtrStack(&routeStack,8,16);
	for(FklStringMatchRouteNode* child=route->children
			;child
			;child=child->siblings)
		fklPushPtrStack(child,&routeStack);
	size_t routeIndex=routeStack.top;
	while(partIndex<partsNum)
	{
		FklNastNode* curPart=partsBase[partIndex];
		FklNastNode* curNast=nastBase[nastIndex];
		switch(curPart->type)
		{
			case FKL_NAST_SYM:
				fklPatternMatchingHashTableSet(curPart->u.sym,fklMakeNastNodeRef(curNast),ht);
				nastIndex++;
				routeIndex--;
				break;
			case FKL_NAST_BOX:
				{
					size_t num=getNumOfList(routeIndex
							,(FklStringMatchRouteNode**)routeStack.base
							,tokenStack);
					FklNastNode* list=create_nast_list_and_make_ref(&nastBase[nastIndex],num,curNast->curline);
					nastIndex+=num;
					routeIndex-=num;
					fklPatternMatchingHashTableSet(curPart->u.box->u.sym,list,ht);
				}
				break;
			case FKL_NAST_STR:
				break;
			default:
				FKL_ASSERT(0);
				break;
		}
		partIndex++;
	}
	fklUninitPtrStack(&routeStack);
	return ht;
}

static FklNastNode* readerMacroExpand(FklStringMatchPattern* pattern
		,FklStringMatchRouteNode* route
		,FklPtrStack* nastStack
		,FklPtrStack* tokenStack
		,size_t curline
		,size_t* errorLine
		,FklCodegen* codegen)
{
	FklHashTable* ht=processNastStackWithPatternParts(pattern->parts
			,route
			,nastStack
			,tokenStack);
	FklHashTable* lineHash=fklCreateLineNumHashTable();
	FklVM* anotherVM=fklInitMacroExpandVM(pattern->u.proc,pattern->ptpool,ht,lineHash,codegen);
	FklVMgc* gc=anotherVM->gc;
	FklNastNode* r=NULL;
	int e=fklRunVM(anotherVM);
	anotherVM->ptpool=NULL;
	if(e)
	{
		fklDeleteCallChain(anotherVM);
		fklJoinAllThread(anotherVM);
		r=NULL;
		*errorLine=curline;
	}
	else
	{
		fklWaitGC(anotherVM->gc);
		fklJoinAllThread(anotherVM);
		fklDestroyNastNode(r);
		r=fklCreateNastNodeFromVMvalue(fklGetTopValue(anotherVM->stack)
				,curline,lineHash
				,codegen->publicSymbolTable);
	}
	for(FklHashTableNodeList* list=ht->list;list;list=list->next)
	{
		FklPatternMatchingHashTableItem* item=list->node->item;
		fklDestroyNastNode(item->node);
	}
	fklDestroyHashTable(ht);
	fklDestroyHashTable(lineHash);
	fklDestroyVMgc(gc);
	fklDestroyAllVMs(anotherVM);
	if(r)
		r->refcount=0;
	return r;
}

FklNastNode* fklCreateNastNodeFromTokenStackAndMatchRoute(FklPtrStack* tokenStack
		,FklStringMatchRouteNode* route
		,size_t* errorLine
		,const FklSid_t st[4]
		,FklCodegen* codegen
		,FklSymbolTable* publicSymbolTable)
{
	if(fklIsPtrStackEmpty(tokenStack))
		return NULL;
	FklPtrStack questStack=FKL_STACK_INIT;
	FklPtrStack* routeStack=fklCreatePtrStack(1,16);
	FklNastNode* retval=NULL;
	fklPushPtrStack(route,routeStack);
	fklInitPtrStack(&questStack,32,16);
	CreateNastNodeQuest* firstQuest=createNastNodeQuest(getSingleToken(route,tokenStack)->line
			,route
			,NULL
			,routeStack);
	fklPushPtrStack(firstQuest,&questStack);
	while(!fklIsPtrStackEmpty(&questStack))
	{
		CreateNastNodeQuest* curQuest=fklTopPtrStack(&questStack);
		FklPtrStack* curRouteStack=curQuest->route;
		FklPtrStack* curNastStack=curQuest->nast;
		while(!fklIsPtrStackEmpty(curRouteStack))
		{
			FklStringMatchRouteNode* curRoute=fklPopPtrStack(curRouteStack);
			if(curRoute->pattern)
			{
				FklPtrStack* newRouteStack=fklCreatePtrStack(8,16);
				for(FklStringMatchRouteNode* child=curRoute->children
						;child
						;child=child->siblings)
					fklPushPtrStack(child,newRouteStack);
				CreateNastNodeQuest* quest=createNastNodeQuest(getSingleToken(curRoute
							,tokenStack)->line
						,curRoute
						,curRoute->pattern
						,newRouteStack);
				fklPushPtrStack(quest,&questStack);
				break;
			}
			else
			{
				FklToken* token=getSingleToken(curRoute,tokenStack);
				if(token->type==FKL_TOKEN_RESERVE_STR)
				{
					while(!fklIsPtrStackEmpty(&questStack))
					{
						CreateNastNodeQuest* cur=fklPopPtrStack(&questStack);
						while(!fklIsPtrStackEmpty(cur->nast))
						{
							FklNastNode* curNode=fklPopPtrStack(cur->nast);
							fklDestroyNastNode(curNode);
						}
						destroyNastNodeQuest(cur);
					}
					fklUninitPtrStack(&questStack);
					*errorLine=token->line;
					return NULL;
				}
				else
				{
					FklNastNode* node=literalNodeCreator[token->type-FKL_TOKEN_CHAR](token->value
							,token->line
							,publicSymbolTable);
					if(!node)
					{
						while(!fklIsPtrStackEmpty(&questStack))
						{
							CreateNastNodeQuest* cur=fklPopPtrStack(&questStack);
							while(!fklIsPtrStackEmpty(cur->nast))
							{
								FklNastNode* curNode=fklPopPtrStack(cur->nast);
								fklDestroyNastNode(curNode);
							}
							destroyNastNodeQuest(cur);
						}
						fklUninitPtrStack(&questStack);
						*errorLine=token->line;
						return NULL;
					}
					fklPushPtrStack(node,curNastStack);
				}
			}
		}
		CreateNastNodeQuest* otherCodegenQuest=fklTopPtrStack(&questStack);
		if(otherCodegenQuest==curQuest)
		{
			fklPopPtrStack(&questStack);
			CreateNastNodeQuest* prevQuest=fklTopPtrStack(&questStack);
			FklStringMatchPattern* pattern=curQuest->pattern;
			FklNastNode* r=NULL;
			if(!pattern)
				r=fklPopPtrStack(curQuest->nast);
			else if(pattern->type==FKL_STRING_PATTERN_BUILTIN)
				r=pattern->u.func(curQuest->nast
						,curQuest->line
						,errorLine
						,st);
			else
				r=readerMacroExpand(pattern
						,curQuest->routeNode
						,curQuest->nast
						,tokenStack
						,curQuest->line
						,errorLine
						,codegen);
			while(!fklIsPtrStackEmpty(curQuest->nast))
			{
				FklNastNode* curNode=fklPopPtrStack(curQuest->nast);
				fklDestroyNastNode(curNode);
			}
			destroyNastNodeQuest(curQuest);
			if(!r)
			{
				while(!fklIsPtrStackEmpty(&questStack))
				{
					CreateNastNodeQuest* cur=fklPopPtrStack(&questStack);
					while(!fklIsPtrStackEmpty(cur->nast))
					{
						FklNastNode* curNode=fklPopPtrStack(cur->nast);
						fklDestroyNastNode(curNode);
					}
					destroyNastNodeQuest(cur);
				}
				fklUninitPtrStack(&questStack);
				return NULL;
			}
			if(prevQuest)
				fklPushPtrStack(r,prevQuest->nast);
			else
				retval=r;
		}
	}
	fklUninitPtrStack(&questStack);
	return retval;
}

