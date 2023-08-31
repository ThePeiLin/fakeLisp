#include<fakeLisp/parser.h>
#include<fakeLisp/grammer.h>
#include<fakeLisp/codegen.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/lexer.h>
#include<fakeLisp/pattern.h>
#include<fakeLisp/vm.h>
#include<string.h>
#include<ctype.h>

FklNastNode* fklCreateNastNodeFromCstr(const char* cStr,FklSymbolTable* publicSymbolTable)
{
	FklPtrStack stateStack;
	FklPtrStack symbolStack;
	fklInitPtrStack(&stateStack,16,16);
	fklInitPtrStack(&symbolStack,16,16);
	fklPushState0ToStack(&stateStack);

	int err=0;
	size_t errLine=0;
	FklGrammerMatchOuterCtx outerCtx=FKL_GRAMMER_MATCH_OUTER_CTX_INIT;
	FklNastNode* node=fklDefaultParseForCstr(cStr
			,&outerCtx
			,publicSymbolTable
			,&err
			,&errLine
			,&symbolStack
			,&stateStack);

	while(!fklIsPtrStackEmpty(&symbolStack))
	{
		FklAnalyzingSymbol* s=fklPopPtrStack(&symbolStack);
		fklDestroyNastNode(s->ast);
		free(s);
	}
	fklUninitPtrStack(&symbolStack);
	fklUninitPtrStack(&stateStack);

	return node;
}

static FklNastNode* createChar(const FklString* oStr,uint64_t line,FklSymbolTable* table)
{
	if(!fklIsValidCharBuf(oStr->str+2,oStr->size-2))
		return NULL;
	FklNastNode* r=fklCreateNastNode(FKL_NAST_CHR,line);
	r->chr=fklCharBufToChar(oStr->str+2,oStr->size-2);
	return r;
}

static FklNastNode* createNum(const FklString* oStr,uint64_t line,FklSymbolTable* table)
{
	FklNastNode* r=NULL;
	if(fklIsDoubleString(oStr))
	{
		r=fklCreateNastNode(FKL_NAST_F64,line);
		r->f64=fklStringToDouble(oStr);
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
			r->bigInt=bi;
			r->type=FKL_NAST_BIG_INT;
		}
		else
		{
			int64_t num=fklBigIntToI64(&bInt);
			r->type=FKL_NAST_FIX;
			r->fix=num;
			fklUninitBigInt(&bInt);
		}
	}
	return r;
}

static FklNastNode* createString(const FklString* oStr,uint64_t line,FklSymbolTable* table)
{
	FklNastNode* r=fklCreateNastNode(FKL_NAST_STR,line);
	r->str=fklCopyString(oStr);
	return r;
}

static FklNastNode* createSymbol(const FklString* oStr,uint64_t line,FklSymbolTable* publicSymbolTable)
{
	FklNastNode* r=fklCreateNastNode(FKL_NAST_SYM,line);
	r->sym=fklAddSymbol(oStr,publicSymbolTable)->id;
	return r;
}

static FklNastNode* (*literalNodeCreator[])(const FklString*,uint64_t,FklSymbolTable*)=
{
	createChar,
	createNum,
	createString,
	createSymbol,
};

typedef struct
{
	FklStringMatchPattern* pattern;
	uint32_t line;
	uint32_t cindex;
}MatchState;

static inline FklToken* getSingleToken(FklStringMatchRouteNode* routeNode
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
		(*cur)->pair=fklCreateNastPair();
		(*cur)->pair->car=fklMakeNastNodeRef(a[i]);
		cur=&(*cur)->pair->cdr;
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
	size_t partsNum=parts->vec->size;
	FklNastNode** partsBase=parts->vec->base;
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
				fklPatternMatchingHashTableSet(curPart->sym,fklMakeNastNodeRef(curNast),ht);
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
					fklPatternMatchingHashTableSet(curPart->box->sym,list,ht);
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
	FklNastNode* retval=NULL;
	FklVM* anotherVM=fklInitMacroExpandVM(pattern->proc
			,pattern->pts
			,ht
			,lineHash
			,codegen
			,&retval
			,curline);
	FklVMgc* gc=anotherVM->gc;
	FklNastNode* r=NULL;
	int e=fklRunVM(anotherVM);
	anotherVM->pts=NULL;
	if(e)
	{
		fklDeleteCallChain(anotherVM);
		r=NULL;
		*errorLine=curline;
	}
	else if(retval)
	{
		fklDestroyNastNode(r);
		r=retval;
	}
	for(FklHashTableItem* list=ht->first;list;list=list->next)
	{
		FklPatternMatchingHashTableItem* item=(FklPatternMatchingHashTableItem*)list->data;
		fklDestroyNastNode(item->node);
	}
	fklDestroyHashTable(ht);
	fklDestroyHashTable(lineHash);
	fklDestroyAllVMs(anotherVM);
	fklDestroyVMgc(gc);
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
				r=pattern->func(curQuest->nast
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

FklNastNode* fklNastConsWithSym(FklSid_t sym
		,FklNastNode* cdr
		,uint64_t l1
		,uint64_t l2)
{
	FklNastNode* r=fklCreateNastNode(FKL_NAST_PAIR,l1);
	FklNastNode* h=fklCreateNastNode(FKL_NAST_SYM,l2);
	h->sym=sym;
	r->pair=fklCreateNastPair();
	r->pair->car=h;
	r->pair->cdr=cdr;
	return r;
}

FklNastNode* fklNastCons(FklNastNode* car
		,FklNastNode* cdr
		,uint64_t l1)
{
	FklNastNode* r=fklCreateNastNode(FKL_NAST_PAIR,l1);
	r->pair=fklCreateNastPair();
	r->pair->car=car;
	r->pair->cdr=cdr;
	return r;
}
