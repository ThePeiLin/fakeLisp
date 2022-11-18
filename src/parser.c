#include<fakeLisp/parser.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/lexer.h>
#include<fakeLisp/pattern.h>
#include<fakeLisp/vm.h>
#include<string.h>
#include<ctype.h>

enum
{
	BUILTIN_HEAD_QUOTE=0,
	BUILTIN_HEAD_QSQUOTE=1,
	BUILTIN_HEAD_UNQUOTE=2,
	BUILTIN_HEAD_UNQTESP=3,
}BuildInHeadSymbolIndex;

FklNastNode* fklCreateNastNodeFromCstr(const char* cStr
		,const FklSid_t buildInHeadSymbolTable[4]
		,FklStringMatchPattern* patterns)
{
	FklPtrStack* tokenStack=fklCreatePtrStack(8,16);
	size_t size=strlen(cStr);
	size_t line=0;
	size_t j=0;
	FklStringMatchSet* matchSet=FKL_STRING_PATTERN_UNIVERSAL_SET;
	FklPtrStack* matchStateStack=fklCreatePtrStack(8,16);
	FklStringMatchRouteNode* route=fklCreateStringMatchRouteNode(NULL,0,0,NULL,NULL,NULL);
	fklSplitStringIntoTokenWithPattern(cStr
			,size
			,line
			,&line
			,j
			,&j
			,tokenStack
			,matchSet
			,patterns
			,route);
	size_t errorLine=0;
	FklNastNode* retval=fklCreateNastNodeFromTokenStackAndMatchRoute(tokenStack
			,route
			,&errorLine
			,buildInHeadSymbolTable);
	while(!fklIsPtrStackEmpty(tokenStack))
		fklDestroyToken(fklPopPtrStack(tokenStack));
	while(!fklIsPtrStackEmpty(matchStateStack))
		free(fklPopPtrStack(matchStateStack));
	fklDestroyPtrStack(tokenStack);
	fklDestroyPtrStack(matchStateStack);
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

static FklNastNode* createNastList(FklNastNode** a,size_t num,uint64_t line)
{
	FklNastNode* r=NULL;
	FklNastNode** cur=&r;
	for(size_t i=0;i<num;i++)
	{
		(*cur)=fklMakeNastNodeRef(fklCreateNastNode(FKL_NAST_PAIR,line));
		(*cur)->u.pair=fklCreateNastPair();
		(*cur)->u.pair->car=fklMakeNastNodeRef(a[i]);
		cur=&(*cur)->u.pair->cdr;
	}
	(*cur)=fklMakeNastNodeRef(fklCreateNastNode(FKL_NAST_NIL,line));
	r->refcount=0;
	return r;
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
			return 0;
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

static FklNastNode* createChar(const FklString* oStr,uint64_t line)
{
	if(!fklIsValidCharStr(oStr->str+2,oStr->size-2))
		return NULL;
	FklNastNode* r=fklCreateNastNode(FKL_NAST_CHR,line);
	r->u.chr=fklCharBufToChar(oStr->str+2,oStr->size-2);
	return r;
}

static FklNastNode* createNum(const FklString* oStr,uint64_t line)
{
	FklNastNode* r=NULL;
	if(fklIsDoubleString(oStr))
	{
		r=fklCreateNastNode(FKL_NAST_F64,line);
		r->u.f64=fklStringToDouble(oStr);
	}
	else
	{
		r=fklCreateNastNode(FKL_NAST_I32,line);
		FklBigInt* bInt=fklCreateBigIntFromString(oStr);
		if(fklIsGtLtI64BigInt(bInt))
			r->u.bigInt=bInt;
		else
		{
			int64_t num=fklBigIntToI64(bInt);
			if(num>INT32_MAX||num<INT32_MIN)
			{
				r->type=FKL_NAST_I64;
				r->u.i64=num;
			}
			else
				r->u.i32=num;
			fklDestroyBigInt(bInt);
		}
	}
	return r;
}

static FklNastNode* createString(const FklString* oStr,uint64_t line)
{
	FklNastNode* r=fklCreateNastNode(FKL_NAST_STR,line);
	r->u.str=fklCopyString(oStr);
	return r;
}

static FklNastNode* createSymbol(const FklString* oStr,uint64_t line)
{
	FklNastNode* r=fklCreateNastNode(FKL_NAST_SYM,line);
	r->u.sym=fklAddSymbolToGlob(oStr)->id;
	return r;
}

static FklNastNode* (*literalNodeCreator[])(const FklString*,uint64_t)=
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
	r->node=fklMakeNastNodeRef(node);
	r->place=place;
	return r;
}

static void destroyNastElem(NastElem* n)
{
	fklDestroyNastNode(n->node);
	free(n);
}

typedef struct
{
	FklStringMatchPattern* pattern;
	uint32_t line;
	uint32_t cindex;
}MatchState;

static void destroyMatchState(MatchState* state)
{
	free(state);
}

#define PARENTHESE_0 ((void*)0)
#define PARENTHESE_1 ((void*)1)
#define QUOTE ((void*)2)
#define QSQUOTE ((void*)3)
#define UNQUOTE ((void*)4)
#define UNQTESP ((void*)5)
#define DOTTED ((void*)6)
#define VECTOR_0 ((void*)7)
#define VECTOR_1 ((void*)8)
#define BOX ((void*)9)
#define BVECTOR_0 ((void*)10)
#define BVECTOR_1 ((void*)11)
#define HASH_EQ_0 ((void*)12)
#define HASH_EQ_1 ((void*)13)
#define HASH_EQV_0 ((void*)14)
#define HASH_EQV_1 ((void*)15)
#define HASH_EQUAL_0 ((void*)16)
#define HASH_EQUAL_1 ((void*)17)

static int isBuiltInSingleStrPattern(FklStringMatchPattern* pattern)
{
	return pattern==QUOTE
		||pattern==QSQUOTE
		||pattern==UNQUOTE
		||pattern==UNQTESP
		||pattern==DOTTED
		||pattern==BOX
		;
}

static int isBuiltInHashTable(FklStringMatchPattern* pattern)
{
	return pattern==HASH_EQ_0
		||pattern==HASH_EQ_1
		||pattern==HASH_EQV_0
		||pattern==HASH_EQV_1
		||pattern==HASH_EQUAL_0
		||pattern==HASH_EQUAL_1
		;
}

static int isBuiltInParenthese(FklStringMatchPattern* pattern)
{
	return pattern==PARENTHESE_0
		||pattern==PARENTHESE_1
		;
}

static int isBuiltInVector(FklStringMatchPattern* pattern)
{
	return pattern==VECTOR_0
		||pattern==VECTOR_1
		;
}

static int isBuiltInBvector(FklStringMatchPattern* pattern)
{
	return pattern==BVECTOR_0
		||pattern==BVECTOR_1
		;
}

static int isLeftParenthese(const FklString* str)
{
	return str->size==1&&(str->str[0]=='('||str->str[0]=='[');
}

static int isVectorStart(const FklString* str)
{
	return str->size==2&&str->str[0]=='#'&&(str->str[1]=='('||str->str[1]=='[');
}

static int isBytevectorStart(const FklString* str)
{
	return str->size==5&&!strncmp(str->str,"#vu8",4)&&(str->str[4]=='('||str->str[4]=='[');
}

static int isRightParenthese(const FklString* str)
{
	return str->size==1&&(str->str[0]==')'||str->str[0]==']');
}

static FklStringMatchPattern* getHashPattern(const FklString* str)
{
	if(str->size==6&&!strncmp(str->str,"#hash",5))
	{
		if(str->str[5]=='(')
			return HASH_EQ_0;
		if(str->str[5]=='[')
			return HASH_EQ_1;
		return NULL;
	}
	else if(str->size==9&&!strncmp(str->str,"#hasheqv",8))
	{
		if(str->str[8]=='(')
			return HASH_EQV_0;
		if(str->str[8]=='[')
			return HASH_EQV_1;
		return NULL;
	}
	else if(str->size==11&&!strncmp(str->str,"#hashequal",10))
	{
		if(str->str[10]=='(')
			return HASH_EQUAL_0;
		if(str->str[10]=='[')
			return HASH_EQUAL_1;
		return NULL;
	}
	return NULL;
}

static int isHashTableStart(const FklString* str)
{
	return (str->size==6&&!strncmp(str->str,"#hash",5)&&(str->str[5]=='('||str->str[5]=='['))
		||(str->size==9&&!strncmp(str->str,"#hasheqv",8)&&(str->str[8]=='('||str->str[8]=='['))
		||(str->size==11&&!strncmp(str->str,"#hashequal",10)&&(str->str[10]=='('||str->str[10]=='['))
		;
}

static int isBuilInPatternStart(const FklString* str)
{
	return isLeftParenthese(str)
		||isVectorStart(str)
		||isBytevectorStart(str)
		||isHashTableStart(str)
		;
}

static int isBuiltInSingleStr(const FklString* str)
{
	const char* buf=str->str;
	return (str->size==1&&(buf[0]=='\''
				||buf[0]=='`'
				||buf[0]=='~'
				||buf[0]==','))
		||!fklStringCstrCmp(str,"~@")
		||!fklStringCstrCmp(str,"#&");
}

//static int isBuiltInPattern(FklStringMatchPattern* pattern)
//{
//	return isBuiltInSingleStrPattern(pattern)
//		||isBuiltInParenthese(pattern)
//		||isBuiltInVector(pattern)
//		||isBuiltInBvector(pattern)
//		||isBuiltInHashTable(pattern);
//}

//static MatchState* searchReverseStringCharMatchState(const FklString* str,FklPtrStack* matchStateStack)
//{
//	MatchState* top=fklTopPtrStack(matchStateStack);
//	uint32_t i=0;
//	for(;top&&!isBuiltInPattern(top->pattern)
//			&&top->cindex+i<top->pattern->num;)
//	{
//		const FklString* cpart=fklGetNthPartOfStringMatchPattern(top->pattern,top->cindex+i);
//		if(!fklIsVar(cpart)&&!fklStringcmp(str,cpart))
//		{
//			top->cindex+=i;
//			return top;
//		}
//		i++;
//	}
//	return NULL;
//}


static MatchState* createMatchState(FklStringMatchPattern* pattern,uint32_t line,uint32_t cindex)
{
	MatchState* state=(MatchState*)malloc(sizeof(MatchState));
	FKL_ASSERT(state);
	state->pattern=pattern;
	state->line=line;
	state->cindex=cindex;
	return state;
}

static FklNastNode* listProcesser(FklPtrStack* nodeStack,uint64_t line,size_t* errorLine)
{
	FklNastNode* retval=NULL;
	FklNastNode** cur=&retval;
	int r=1;
	for(size_t i=0;i<nodeStack->top;i++)
	{
		NastElem* elem=nodeStack->base[i];
		FklNastNode* node=elem->node;
		NastPlace place=elem->place;
		if(place==NAST_CAR)
		{
			(*cur)=fklMakeNastNodeRef(fklCreateNastNode(FKL_NAST_PAIR,node->curline));
			(*cur)->u.pair=fklCreateNastPair();
			(*cur)->u.pair->car=fklMakeNastNodeRef(node);
			cur=&(*cur)->u.pair->cdr;
		}
		else if((*cur)==NULL)
			(*cur)=fklMakeNastNodeRef(node);
		else
		{
			*errorLine=node->curline;
			r=0;
			for(size_t j=i;j<nodeStack->top;j++)
			{
				NastElem* elem=nodeStack->base[j];
				destroyNastElem(elem);
			}
			fklDestroyNastNode(retval);
			retval=NULL;
			break;
		}
		destroyNastElem(elem);
	}
	if(r&&!(*cur))
		(*cur)=fklMakeNastNodeRef(fklCreateNastNode(FKL_NAST_NIL,line));
	if(retval)
		retval->refcount=0;
	return retval;
}

static FklNastNode* vectorProcesser(FklPtrStack* nodeStack,uint64_t line,size_t* errorLine)
{
	FklNastNode* retval=fklCreateNastNode(FKL_NAST_VECTOR,line);
	retval->u.vec=fklCreateNastVector(nodeStack->top);
	for(size_t i=0;i<nodeStack->top;i++)
	{
		NastElem* elem=nodeStack->base[i];
		FklNastNode* node=elem->node;
		NastPlace place=elem->place;
		if(place==NAST_CAR)
			retval->u.vec->base[i]=fklMakeNastNodeRef(node);
		else
		{
			*errorLine=node->curline;
			for(size_t j=i;j<nodeStack->top;j++)
			{
				NastElem* elem=nodeStack->base[j];
				destroyNastElem(elem);
			}
			retval->refcount=1;
			fklDestroyNastNode(retval);
			retval=NULL;
			break;
		}
		destroyNastElem(elem);
	}
	return retval;
}

static FklNastNode* bytevectorProcesser(FklPtrStack* nodeStack,uint64_t line,size_t* errorLine)
{
	FklNastNode* retval=fklCreateNastNode(FKL_NAST_BYTEVECTOR,line);
	retval->u.bvec=fklCreateBytevector(nodeStack->top,NULL);
	for(size_t i=0;i<nodeStack->top;i++)
	{
		NastElem* elem=nodeStack->base[i];
		FklNastNode* node=elem->node;
		NastPlace place=elem->place;
		if(place==NAST_CAR
				&&(node->type==FKL_NAST_I32
					||node->type==FKL_NAST_I64
					||node->type==FKL_NAST_BIG_INT))
		{
			retval->u.bvec->ptr[i]=node->type==FKL_NAST_I32
				?node->u.i32
				:node->type==FKL_NAST_I64
				?node->u.i64
				:fklBigIntToI64(node->u.bigInt);
		}
		else
		{
			*errorLine=node->curline;
			for(size_t j=i;j<nodeStack->top;j++)
			{
				NastElem* elem=nodeStack->base[j];
				destroyNastElem(elem);
			}
			retval->refcount=1;
			fklDestroyNastNode(retval);
			retval=NULL;
			break;
		}
		destroyNastElem(elem);
	}
	return retval;
}

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

inline static FklNastNode* hashTableProcess(FklVMhashTableEqType type,FklPtrStack* nodeStack,uint64_t line,size_t* errorLine)
{
	FklNastNode* retval=fklCreateNastNode(FKL_NAST_HASHTABLE,line);
	retval->u.hash=fklCreateNastHash(type,nodeStack->top);
	for(size_t i=0;i<nodeStack->top;i++)
	{
		NastElem* elem=nodeStack->base[i];
		FklNastNode* node=elem->node;
		NastPlace place=elem->place;
		if(place==NAST_CAR&&node->type==FKL_NAST_PAIR)
		{
			retval->u.hash->items[i].car=fklMakeNastNodeRef(node->u.pair->car);
			retval->u.hash->items[i].cdr=fklMakeNastNodeRef(node->u.pair->cdr);
		}
		else
		{
			*errorLine=node->curline;
			for(size_t j=i;j<nodeStack->top;j++)
			{
				NastElem* elem=nodeStack->base[j];
				destroyNastElem(elem);
			}
			retval->refcount=1;
			fklDestroyNastNode(retval);
			retval=NULL;
			break;
		}
		destroyNastElem(elem);
	}
	return retval;
}

static FklNastNode* hashEqProcesser(FklPtrStack* nodeStack,uint64_t line,size_t* errorLine)
{
	return hashTableProcess(FKL_VM_HASH_EQ,nodeStack,line,errorLine);
}

static FklNastNode* hashEqvProcesser(FklPtrStack* nodeStack,uint64_t line,size_t* errorLine)
{
	return hashTableProcess(FKL_VM_HASH_EQV,nodeStack,line,errorLine);
}

static FklNastNode* hashEqualProcesser(FklPtrStack* nodeStack,uint64_t line,size_t* errorLine)
{
	return hashTableProcess(FKL_VM_HASH_EQUAL,nodeStack,line,errorLine);
}


static FklNastNode* (*nastStackProcessers[])(FklPtrStack*,uint64_t,size_t*)=
{
	listProcesser,
	listProcesser,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	vectorProcesser,
	vectorProcesser,
	NULL,
	bytevectorProcesser,
	bytevectorProcesser,
	hashEqProcesser,
	hashEqProcesser,
	hashEqvProcesser,
	hashEqvProcesser,
	hashEqualProcesser,
	hashEqualProcesser,
};

#define BUILTIN_PATTERN_START_PROCESS(PATTERN) FklStringMatchPattern* pattern=(PATTERN);\
						MatchState* state=createMatchState(pattern,token->line,0);\
						fklPushPtrStack(state,matchStateStack);\
						cStack=fklCreatePtrStack(32,16);\
						fklPushPtrStack(cStack,stackStack)\

FklNastNode* fklCreateNastNodeFromTokenStack(FklPtrStack* tokenStack,size_t* errorLine,const FklSid_t buildInHeadSymbolTable[4])
{
//	FklNastNode* retval=NULL;
//	if(!fklIsPtrStackEmpty(tokenStack))
//	{
//		FklPtrStack* matchStateStack=fklCreatePtrStack(32,16);
//		FklPtrStack* stackStack=fklCreatePtrStack(32,16);
//		FklPtrStack* elemStack=fklCreatePtrStack(32,16);
//		fklPushPtrStack(elemStack,stackStack);
//		for(uint32_t i=0;i<tokenStack->top;i++)
//		{
//			FklToken* token=tokenStack->base[i];
//			FklPtrStack* cStack=fklTopPtrStack(stackStack);
//			if(token->type>FKL_TOKEN_RESERVE_STR&&token->type<FKL_TOKEN_COMMENT)
//			{
//				FklNastNode* singleLiteral=literalNodeCreator[token->type-FKL_TOKEN_CHAR](token->value,token->line);
//				if(!singleLiteral)
//				{
//					*errorLine=token->line;
//					while(!fklIsPtrStackEmpty(matchStateStack))
//						destroyMatchState(fklPopPtrStack(matchStateStack));
//					fklDestroyPtrStack(matchStateStack);
//					while(!fklIsPtrStackEmpty(stackStack))
//					{
//						FklPtrStack* cStack=fklPopPtrStack(stackStack);
//						while(!fklIsPtrStackEmpty(cStack))
//						{
//							NastElem* elem=fklPopPtrStack(cStack);
//							destroyNastElem(elem);
//						}
//						fklDestroyPtrStack(cStack);
//					}
//					fklDestroyPtrStack(stackStack);
//					return NULL;
//				}
//				NastElem* elem=createNastElem(NAST_CAR,singleLiteral);
//				fklPushPtrStack(elem,cStack);
//			}
//			else if(token->type==FKL_TOKEN_RESERVE_STR)
//			{
//				//MatchState* state=searchReverseStringCharMatchState(token->value
//				//		,matchStateStack);
//				FklStringMatchPattern* pattern=NULL;//fklFindStringPattern(token->value);
//				if(pattern)
//				{
//					MatchState* state=createMatchState(pattern,token->line,1);
//					fklPushPtrStack(state,matchStateStack);
//					cStack=fklCreatePtrStack(32,16);
//					fklPushPtrStack(cStack,stackStack);
//					const FklString* part=NULL;//fklGetNthPartOfStringMatchPattern(pattern,state->cindex);
//					if(1)//fklIsVar(part)&&fklIsMustList(part))
//					{
//						cStack=fklCreatePtrStack(32,16);
//						fklPushPtrStack(cStack,stackStack);
//					}
//					continue;
//				}
//				else if(isBuilInPatternStart(token->value))
//				{
//					if(isLeftParenthese(token->value))
//					{
//						BUILTIN_PATTERN_START_PROCESS(token->value->str[0]=='('
//								?PARENTHESE_0
//								:PARENTHESE_1);
//					}
//					else if(isVectorStart(token->value))
//					{
//						BUILTIN_PATTERN_START_PROCESS(token->value->str[1]=='('
//								?VECTOR_0
//								:VECTOR_1);
//					}
//					else if(isBytevectorStart(token->value))
//					{
//						BUILTIN_PATTERN_START_PROCESS(token->value->str[4]=='('
//								?BVECTOR_0
//								:BVECTOR_1);
//					}
//					else if(isHashTableStart(token->value))
//					{
//						BUILTIN_PATTERN_START_PROCESS(getHashPattern(token->value));
//					}
//					continue;
//				}
//				else if(isBuiltInSingleStr(token->value))
//				{
//					const char* buf=token->value->str;
//					FklStringMatchPattern* pattern=buf[0]=='\''?
//						QUOTE:
//						buf[0]=='`'?
//						QSQUOTE:
//						buf[0]==','?
//						DOTTED:
//						buf[0]=='~'?
//						(token->value->size==1?
//						 UNQUOTE:
//						 buf[1]=='@'?
//						 UNQTESP:NULL):
//						(token->value->size==2&&buf[0]=='#'&&buf[1]=='&')?
//						BOX:
//						NULL;
//					MatchState* state=createMatchState(pattern,token->line,0);
//					fklPushPtrStack(state,matchStateStack);
//					cStack=fklCreatePtrStack(1,1);
//					fklPushPtrStack(cStack,stackStack);
//					continue;
//				}
//				else if(isRightParenthese(token->value))
//				{
//					fklPopPtrStack(stackStack);
//					MatchState* cState=fklPopPtrStack(matchStateStack);
//					FklNastNode* n=nastStackProcessers[(uintptr_t)cState->pattern](cStack,token->line,errorLine);
//					fklDestroyPtrStack(cStack);
//					destroyMatchState(cState);
//					if(!n)
//					{
//						while(!fklIsPtrStackEmpty(matchStateStack))
//							destroyMatchState(fklPopPtrStack(matchStateStack));
//						fklDestroyPtrStack(matchStateStack);
//						while(!fklIsPtrStackEmpty(stackStack))
//						{
//							FklPtrStack* cStack=fklPopPtrStack(stackStack);
//							while(!fklIsPtrStackEmpty(cStack))
//							{
//								NastElem* elem=fklPopPtrStack(cStack);
//								destroyNastElem(elem);
//							}
//							fklDestroyPtrStack(cStack);
//						}
//						fklDestroyPtrStack(stackStack);
//						return NULL;
//					}
//					NastElem* v=createNastElem(NAST_CAR,n);
//					cStack=fklTopPtrStack(stackStack);
//					fklPushPtrStack(v,cStack);
//				}
//			}
//			for(MatchState* state=fklTopPtrStack(matchStateStack);
//					state&&(isBuiltInSingleStrPattern(state->pattern)||(!isBuiltInParenthese(state->pattern)
//							&&!isBuiltInVector(state->pattern)
//							&&!isBuiltInBvector(state->pattern)
//							&&!isBuiltInHashTable(state->pattern)));
//					state=fklTopPtrStack(matchStateStack))
//			{
//				if(isBuiltInSingleStrPattern(state->pattern))
//				{
//					fklPopPtrStack(stackStack);
//					MatchState* cState=fklPopPtrStack(matchStateStack);
//					NastElem* postfix=fklPopPtrStack(cStack);
//					fklDestroyPtrStack(cStack);
//					cStack=fklTopPtrStack(stackStack);
//					if(state->pattern==DOTTED)
//					{
//						postfix->place=NAST_CDR;
//						fklPushPtrStack(postfix,cStack);
//					}
//					else if(state->pattern==BOX)
//					{
//						FklNastNode* box=fklCreateNastNode(FKL_NAST_BOX,token->line);
//						box->u.box=fklMakeNastNodeRef(postfix->node);
//						NastElem* v=createNastElem(NAST_CAR,box);
//						fklPushPtrStack(v,cStack);
//						destroyNastElem(postfix);
//					}
//					else
//					{
//						FklNastNode* prefix=fklCreateNastNode(FKL_NAST_SYM,token->line);
//						FklStringMatchPattern* pattern=cState->pattern;
//						FklSid_t prefixValue=pattern==QUOTE?
//							buildInHeadSymbolTable[BUILTIN_HEAD_QUOTE]:
//							pattern==QSQUOTE?
//							buildInHeadSymbolTable[BUILTIN_HEAD_QSQUOTE]:
//							pattern==UNQUOTE?
//							buildInHeadSymbolTable[BUILTIN_HEAD_UNQUOTE]:
//							buildInHeadSymbolTable[BUILTIN_HEAD_UNQTESP];
//						prefix->u.sym=prefixValue;
//						FklNastNode* tmp[]={prefix,postfix->node,};
//						FklNastNode* list=createNastList(tmp,2,prefix->curline);
//						destroyNastElem(postfix);
//						NastElem* v=createNastElem(NAST_CAR,list);
//						fklPushPtrStack(v,cStack);
//					}
//					destroyMatchState(cState);
//				}
//			}
//		}
//		NastElem* retvalElem=fklTopPtrStack(elemStack);
//		if(retvalElem)
//			retval=retvalElem->node;
//		free(retvalElem);
//		fklDestroyPtrStack(stackStack);
//		fklDestroyPtrStack(matchStateStack);
//		fklDestroyPtrStack(elemStack);
//	}
//	return retval;
}

#undef BUILTIN_PATTERN_START_PROCESS

void fklPrintNastNode(const FklNastNode* exp,FILE* fp)
{
	FklPtrQueue* queue=fklCreatePtrQueue();
	FklPtrStack* queueStack=fklCreatePtrStack(32,16);
	fklPushPtrQueue(createNastElem(NAST_CAR,(FklNastNode*)exp),queue);
	fklPushPtrStack(queue,queueStack);
	while(!fklIsPtrStackEmpty(queueStack))
	{
		FklPtrQueue* cQueue=fklTopPtrStack(queueStack);
		while(fklLengthPtrQueue(cQueue))
		{
			NastElem* e=fklPopPtrQueue(cQueue);
			FklNastNode* node=e->node;
			if(e->place==NAST_CDR)
				fputc(',',fp);
			destroyNastElem(e);
			switch(node->type)
			{
				case FKL_NAST_SYM:
					fklPrintRawSymbol(fklGetGlobSymbolWithId(node->u.sym)->symbol,fp);
					break;
				case FKL_NAST_BYTEVECTOR:
					fklPrintRawBytevector(node->u.bvec,fp);
					break;
				case FKL_NAST_STR:
					fklPrintRawString(node->u.str,fp);
					break;
				case FKL_NAST_I32:
					fprintf(fp,"%d",node->u.i32);
					break;
				case FKL_NAST_I64:
					fprintf(fp,"%ld",node->u.i64);
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
						fklPushPtrStack(vQueue,queueStack);
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
						fklPushPtrStack(bQueue,queueStack);
						cQueue=bQueue;
					}
					break;
				case FKL_NAST_HASHTABLE:
					{
						const static char* tmp[]=
						{
							"#hash",
							"#hasheqv",
							"#hashequal",
						};
						fputs(tmp[node->u.hash->type],fp);
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
						fklPushPtrStack(lQueue,queueStack);
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
		fklPopPtrStack(queueStack);
		fklDestroyPtrQueue(cQueue);
		if(!fklIsPtrStackEmpty(queueStack))
		{
			fputc(')',fp);
			cQueue=fklTopPtrStack(queueStack);
			if(fklLengthPtrQueue(cQueue)&&((NastElem*)fklFirstPtrQueue(cQueue))->place!=NAST_CDR)
				fputc(' ',fp);
		}
	}
	fklDestroyPtrStack(queueStack);
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
	FklPtrStack* stack=fklCreatePtrStack(32,16);
	fklPushPtrStack(node,stack);
	while(!fklIsPtrStackEmpty(stack))
	{
		FklNastNode* cur=fklPopPtrStack(stack);
		if(cur)
		{
			cur->refcount-=1;
			if(!cur->refcount)
			{
				switch(cur->type)
				{
					case FKL_NAST_I64:
					case FKL_NAST_I32:
					case FKL_NAST_SYM:
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
						fklPushPtrStack(cur->u.pair->car,stack);
						fklPushPtrStack(cur->u.pair->cdr,stack);
						destroyNastPair(cur->u.pair);
						break;
					case FKL_NAST_BOX:
						fklPushPtrStack(cur->u.box,stack);
						break;
					case FKL_NAST_VECTOR:
						for(size_t i=0;i<cur->u.vec->size;i++)
							fklPushPtrStack(cur->u.vec->base[i],stack);
						destroyNastVector(cur->u.vec);
						break;
					case FKL_NAST_HASHTABLE:
						for(size_t i=0;i<cur->u.hash->num;i++)
						{
							fklPushPtrStack(cur->u.hash->items[i].car,stack);
							fklPushPtrStack(cur->u.hash->items[i].cdr,stack);
						}
						destroyNastHash(cur->u.hash);
						break;
					default:
						FKL_ASSERT(0);
						break;
				}
				free(cur);
			}
		}
	}
	fklDestroyPtrStack(stack);
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

FklNastNode* fklCopyNastNode(const FklNastNode* node)
{
	FklPtrStack* stack=fklCreatePtrStack(32,16);
	FklPtrStack* cstack=fklCreatePtrStack(32,16);
	FklNastNode* r=NULL;
	fklPushPtrStack((void*)node,stack);
	fklPushPtrStack(&r,cstack);
	while(!fklIsPtrStackEmpty(stack))
	{
		const FklNastNode* root=fklPopPtrStack(stack);
		FklNastNode** pcur=fklPopPtrStack(cstack);
		FklNastNode* cur=fklCreateNastNode(root->type,root->curline);
		cur->refcount=1;
		switch(root->type)
		{
			case FKL_NAST_NIL:
				cur->u.str=NULL;
				break;
			case FKL_NAST_I32:
				cur->u.i32=root->u.i32;
				break;
			case FKL_NAST_I64:
				cur->u.i64=root->u.i64;
				break;
			case FKL_NAST_F64:
				cur->u.f64=root->u.f64;
				break;
			case FKL_NAST_CHR:
				cur->u.chr=root->u.chr;
				break;
			case FKL_NAST_BIG_INT:
				cur->u.bigInt=fklCopyBigInt(root->u.bigInt);
				break;
			case FKL_NAST_SYM:
				cur->u.sym=root->u.sym;
				break;
			case FKL_NAST_STR:
				cur->u.str=fklCopyString(root->u.str);
				break;
			case FKL_NAST_BYTEVECTOR:
				cur->u.bvec=fklCopyBytevector(root->u.bvec);
				break;
			case FKL_NAST_BOX:
				fklPushPtrStack(root->u.box,stack);
				fklPushPtrStack(&cur->u.box,cstack);
				break;
			case FKL_NAST_PAIR:
				fklPushPtrStack(root->u.pair->car,stack);
				fklPushPtrStack(root->u.pair->cdr,stack);
				cur->u.pair=fklCreateNastPair();
				fklPushPtrStack(&cur->u.pair->car,cstack);
				fklPushPtrStack(&cur->u.pair->cdr,cstack);
				break;
			case FKL_NAST_VECTOR:
				for(size_t i=0;i<root->u.vec->size;i++)
					fklPushPtrStack(root->u.vec->base[i],stack);
				cur->u.vec=fklCreateNastVector(root->u.vec->size);
				for(size_t i=0;cur->u.vec->size;i++)
					fklPushPtrStack(&cur->u.vec->base[i],cstack);
				break;
			case FKL_NAST_HASHTABLE:
				for(size_t i=0;i<root->u.hash->num;i++)
				{
					fklPushPtrStack(root->u.hash->items[i].car,stack);
					fklPushPtrStack(root->u.hash->items[i].cdr,stack);
				}
				cur->u.hash=fklCreateNastHash(root->u.hash->type,root->u.hash->num);
				for(size_t i=0;i<cur->u.hash->num;i++)
				{
					fklPushPtrStack(&cur->u.hash->items[i].car,cstack);
					fklPushPtrStack(&cur->u.hash->items[i].cdr,cstack);
				}
				break;
		}
		*pcur=cur;
	}
	fklDestroyPtrStack(stack);
	fklDestroyPtrStack(cstack);
	return r;
}

int fklNastNodeEqual(const FklNastNode* n0,const FklNastNode* n1)
{
	FklPtrStack* s0=fklCreatePtrStack(16,16);
	FklPtrStack* s1=fklCreatePtrStack(16,16);
	fklPushPtrStack((void*)n0,s0);
	fklPushPtrStack((void*)n1,s1);
	int r=1;
	while(!fklIsPtrStackEmpty(s0)&&!fklIsPtrStackEmpty(s1))
	{
		const FklNastNode* c0=fklPopPtrStack(s0);
		const FklNastNode* c1=fklPopPtrStack(s1);
		if(c0->type!=c1->type)
			r=0;
		else
		{
			switch(c0->type)
			{
				case FKL_NAST_SYM:
					r=c0->u.sym==c1->u.sym;
					break;
				case FKL_NAST_I32:
					r=c0->u.i32==c1->u.i32;
					break;
				case FKL_NAST_I64:
					r=c0->u.i64==c1->u.i64;
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
					fklPushPtrStack(c0->u.box,s0);
					fklPushPtrStack(c1->u.box,s1);
					break;
				case FKL_NAST_VECTOR:
					r=c0->u.vec->size==c1->u.vec->size;
					if(r)
					{
						for(size_t i=0;i<c0->u.vec->size;i++)
							fklPushPtrStack(c0->u.vec->base[i],s0);
						for(size_t i=0;i<c1->u.vec->size;i++)
							fklPushPtrStack(c1->u.vec->base[i],s0);
					}
					break;
				case FKL_NAST_HASHTABLE:
					r=c0->u.hash->type==c1->u.hash->type&&c0->u.hash->num==c1->u.hash->num;
					if(r)
					{
						for(size_t i=0;i<c0->u.hash->num;i++)
						{
							fklPushPtrStack(c0->u.hash->items[i].car,s0);
							fklPushPtrStack(c0->u.hash->items[i].cdr,s0);
						}
						for(size_t i=0;i<c1->u.hash->num;i++)
						{
							fklPushPtrStack(c1->u.hash->items[i].car,s0);
							fklPushPtrStack(c1->u.hash->items[i].cdr,s0);
						}
					}
					break;
				case FKL_NAST_PAIR:
					fklPushPtrStack(c0->u.pair->car,s0);
					fklPushPtrStack(c0->u.pair->cdr,s0);
					fklPushPtrStack(c1->u.pair->car,s1);
					fklPushPtrStack(c1->u.pair->cdr,s1);
					break;
			}
		}
		if(!r)
			break;
	}
	fklDestroyPtrStack(s0);
	fklDestroyPtrStack(s1);
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
	FklPtrStack* route;
	FklPtrStack* nast;
}CreateNastNodeQuest;

static CreateNastNodeQuest* createNastNodeQuest(uint64_t line
		,FklStringMatchPattern* pattern
		,FklPtrStack* route)
{
	CreateNastNodeQuest* r=(CreateNastNodeQuest*)malloc(sizeof(CreateNastNodeQuest));
	FKL_ASSERT(r);
	r->line=line;
	r->pattern=pattern;
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

FklNastNode* fklCreateNastNodeFromTokenStackAndMatchRoute(FklPtrStack* tokenStack
		,FklStringMatchRouteNode* route
		,size_t* errorLine
		,const FklSid_t t[4])
{
	FklPtrStack questStack={NULL,0,0,0,};
	FklPtrStack* routeStack=fklCreatePtrStack(1,16);
	FklNastNode* retval=NULL;
	fklPushPtrStack(route,routeStack);
	fklInitPtrStack(&questStack,32,16);
	CreateNastNodeQuest* firstQuest=createNastNodeQuest(getSingleToken(route,tokenStack)->line
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
			if(curRoute->children)
			{
				FklPtrStack* newRouteStack=fklCreatePtrStack(8,16);
				for(FklStringMatchRouteNode* child=curRoute->children
						;child
						;child=child->siblings)
					fklPushPtrStack(child,newRouteStack);
				CreateNastNodeQuest* quest=createNastNodeQuest(getSingleToken(curRoute
							,tokenStack)->line
						,curRoute->pattern
						,newRouteStack);
				fklPushPtrStack(quest,&questStack);
				break;
			}
			else
			{
				FklToken* token=getSingleToken(curRoute,tokenStack);
				FklNastNode* node=literalNodeCreator[token->type-FKL_TOKEN_CHAR](token->value
						,token->line);
				fklPushPtrStack(node,curNastStack);
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
				r=pattern->u.func(curQuest->nast,curQuest->line,errorLine);
			else
			{
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
