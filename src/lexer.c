#include<fakeLisp/lexer.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/parser.h>
#include<fakeLisp/pattern.h>
#include<fakeLisp/vm.h>
#include<string.h>
#include<ctype.h>

static FklSid_t builtInPattern_head_quote=0;
static FklSid_t builtInPattern_head_qsquote=0;
static FklSid_t builtInPattern_head_unquote=0;
static FklSid_t builtInPattern_head_unqtesp=0;

void fklInitLexer(void)
{
	builtInPattern_head_quote=fklAddSymbolToGlobCstr("quote")->id;
	builtInPattern_head_qsquote=fklAddSymbolToGlobCstr("qsquote")->id;
	builtInPattern_head_unquote=fklAddSymbolToGlobCstr("unquote")->id;
	builtInPattern_head_unqtesp=fklAddSymbolToGlobCstr("unqtesp")->id;
}

FklNastNode* fklNewNastNodeFromCstr(const char* cStr)
{
	FklPtrStack* tokenStack=fklNewPtrStack(8,16);
	size_t size=strlen(cStr);
	uint32_t line=0;
	uint32_t i,j=0;
	FklPtrStack* matchStateStack=fklNewPtrStack(8,16);
	fklSplitStringPartsIntoToken(&cStr
			,&size
			,1
			,&line
			,tokenStack
			,matchStateStack
			,&i
			,&j);
	size_t errorLine=0;
	FklNastNode* retval=fklNewNastNodeFromTokenStack(tokenStack,&errorLine);
	while(!fklIsPtrStackEmpty(tokenStack))
		fklFreeToken(fklPopPtrStack(tokenStack));
	while(!fklIsPtrStackEmpty(matchStateStack))
		free(fklPopPtrStack(matchStateStack));
	fklFreePtrStack(tokenStack);
	fklFreePtrStack(matchStateStack);
	return retval;
}

static FklNastNode* newNastNode(FklValueType type,uint64_t line)
{
	FklNastNode* r=(FklNastNode*)malloc(sizeof(FklNastNode));
	FKL_ASSERT(r);
	r->curline=line;
	r->type=type;
	r->u.str=NULL;
	r->refcount=0;
	return r;
}

static FklNastPair* newNastPair(void)
{
	FklNastPair* pair=(FklNastPair*)malloc(sizeof(FklNastPair));
	FKL_ASSERT(pair);
	pair->car=NULL;
	pair->cdr=NULL;
	return pair;
}

static FklNastVector* newNastVector(size_t size)
{
	FklNastVector* vec=(FklNastVector*)malloc(sizeof(FklNastVector));
	FKL_ASSERT(vec);
	vec->size=size;
	vec->base=(FklNastNode**)calloc(size,sizeof(FklNastNode*));
	FKL_ASSERT(vec->base);
	return vec;
}

static FklNastNode* newNastList(FklNastNode** a,size_t num,uint64_t line)
{
	FklNastNode* r=NULL;
	FklNastNode** cur=&r;
	for(size_t i=0;i<num;i++)
	{
		(*cur)=newNastNode(FKL_TYPE_PAIR,line);
		(*cur)->u.pair=newNastPair();
		(*cur)->u.pair->car=a[i];
		cur=&(*cur)->u.pair->cdr;
	}
	(*cur)=newNastNode(FKL_TYPE_NIL,line);
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
	FklNastNode* r=newNastNode(FKL_TYPE_CHR,line);
	r->u.chr=fklCharBufToChar(oStr->str+2,oStr->size-2);
	return r;
}

static FklNastNode* createNum(const FklString* oStr,uint64_t line)
{
	FklNastNode* r=NULL;
	if(fklIsDoubleString(oStr))
	{
		r=newNastNode(FKL_TYPE_F64,line);
		r->u.f64=fklStringToDouble(oStr);
	}
	else
	{
		r=newNastNode(FKL_TYPE_I32,line);
		FklBigInt* bInt=fklNewBigIntFromString(oStr);
		if(fklIsGtLtI64BigInt(bInt))
			r->u.bigInt=bInt;
		else
		{
			int64_t num=fklBigIntToI64(bInt);
			if(num>INT32_MAX||num<INT32_MIN)
			{
				r->type=FKL_TYPE_I64;
				r->u.i64=num;
			}
			else
				r->u.i32=num;
			fklFreeBigInt(bInt);
		}
	}
	return r;
}

static FklNastNode* createString(const FklString* oStr,uint64_t line)
{
	size_t size=0;
	char* str=fklCastEscapeCharBuf(oStr->str+1,'\"',&size);
	FklNastNode* r=newNastNode(FKL_TYPE_STR,line);
	r->u.str=fklNewString(size,str);
	free(str);
	return r;
}

static FklNastNode* createSymbol(const FklString* oStr,uint64_t line)
{
	FklNastNode* r=newNastNode(FKL_TYPE_SYM,line);
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

static NastElem* newNastElem(NastPlace place,FklNastNode* node)
{
	NastElem* r=(NastElem*)malloc(sizeof(NastElem));
	FKL_ASSERT(r);
	r->node=fklMakeNastNodeRef(node);
	r->place=place;
	return r;
}

static void freeNastElem(NastElem* n)
{
	fklFreeNastNode(n->node);
	free(n);
}

typedef struct
{
	FklStringMatchPattern* pattern;
	uint32_t line;
	uint32_t cindex;
}MatchState;

static void freeMatchState(MatchState* state)
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


static MatchState* newMatchState(FklStringMatchPattern* pattern,uint32_t line,uint32_t cindex)
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
			(*cur)=fklMakeNastNodeRef(newNastNode(FKL_TYPE_PAIR,node->curline));
			(*cur)->u.pair=newNastPair();
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
				NastElem* elem=nodeStack->base[i];
				freeNastElem(elem);
			}
			fklFreeNastNode(retval);
			retval=NULL;
			break;
		}
		freeNastElem(elem);
	}
	if(r&&!(*cur))
		(*cur)=fklMakeNastNodeRef(newNastNode(FKL_TYPE_NIL,line));
	retval->refcount=0;
	return retval;
}

static FklNastNode* vectorProcesser(FklPtrStack* nodeStack,uint64_t line,size_t* errorLine)
{
	FklNastNode* retval=newNastNode(FKL_TYPE_VECTOR,line);
	retval->u.vec=newNastVector(nodeStack->top);
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
				NastElem* elem=nodeStack->base[i];
				freeNastElem(elem);
			}
			fklFreeNastNode(retval);
			retval=NULL;
			break;
		}
		freeNastElem(elem);
	}
	return retval;
}

static FklNastNode* bytevectorProcesser(FklPtrStack* nodeStack,uint64_t line,size_t* errorLine)
{
	FklNastNode* retval=newNastNode(FKL_TYPE_BYTEVECTOR,line);
	retval->u.bvec=fklNewBytevector(nodeStack->top,NULL);
	for(size_t i=0;i<nodeStack->top;i++)
	{
		NastElem* elem=nodeStack->base[i];
		FklNastNode* node=elem->node;
		NastPlace place=elem->place;
		if(place==NAST_CAR
				&&(node->type==FKL_TYPE_I32
					||node->type==FKL_TYPE_I64
					||node->type==FKL_TYPE_BIG_INT))
		{
			retval->u.bvec->ptr[i]=node->type==FKL_TYPE_I32
				?node->u.i32
				:node->type==FKL_TYPE_I64
				?node->u.i64
				:fklBigIntToI64(node->u.bigInt);
			fklFreeNastNode(node);
		}
		else
		{
			*errorLine=node->curline;
			for(size_t j=i;j<nodeStack->top;j++)
			{
				NastElem* elem=nodeStack->base[i];
				freeNastElem(elem);
			}
			fklFreeNastNode(retval);
			retval=NULL;
			break;
		}
		freeNastElem(elem);
	}
	return retval;
}

static FklNastHashTable* newNastHash(FklVMhashTableEqType type,size_t num)
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
	FklNastNode* retval=newNastNode(FKL_TYPE_HASHTABLE,line);
	retval->u.hash=newNastHash(type,nodeStack->top);
	for(size_t i=0;i<nodeStack->top;i++)
	{
		NastElem* elem=nodeStack->base[i];
		FklNastNode* node=elem->node;
		NastPlace place=elem->place;
		if(place==NAST_CAR&&node->type==FKL_TYPE_PAIR)
		{
			retval->u.hash->items[i].car=fklMakeNastNodeRef(node->u.pair->car);
			retval->u.hash->items[i].cdr=fklMakeNastNodeRef(node->u.pair->cdr);
			fklFreeNastNode(node);
		}
		else
		{
			*errorLine=node->curline;
			for(size_t j=i;j<nodeStack->top;j++)
			{
				NastElem* elem=nodeStack->base[i];
				freeNastElem(elem);
			}
			fklFreeNastNode(retval);
			retval=NULL;
			break;
		}
		freeNastElem(elem);
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
						MatchState* state=newMatchState(pattern,token->line,0);\
						fklPushPtrStack(state,matchStateStack);\
						cStack=fklNewPtrStack(32,16);\
						fklPushPtrStack(cStack,stackStack)\

FklNastNode* fklNewNastNodeFromTokenStack(FklPtrStack* tokenStack,size_t* errorLine)
{
	FklNastNode* retval=NULL;
	if(!fklIsPtrStackEmpty(tokenStack))
	{
		FklPtrStack* matchStateStack=fklNewPtrStack(32,16);
		FklPtrStack* stackStack=fklNewPtrStack(32,16);
		FklPtrStack* elemStack=fklNewPtrStack(32,16);
		fklPushPtrStack(elemStack,stackStack);
		for(uint32_t i=0;i<tokenStack->top;i++)
		{
			FklToken* token=tokenStack->base[i];
			FklPtrStack* cStack=fklTopPtrStack(stackStack);
			if(token->type>FKL_TOKEN_RESERVE_STR&&token->type<FKL_TOKEN_COMMENT)
			{
				FklNastNode* singleLiteral=literalNodeCreator[token->type-FKL_TOKEN_CHAR](token->value,token->line);
				NastElem* elem=newNastElem(NAST_CAR,singleLiteral);
				fklPushPtrStack(elem,cStack);
			}
			else if(token->type==FKL_TOKEN_RESERVE_STR)
			{
				//MatchState* state=searchReverseStringCharMatchState(token->value
				//		,matchStateStack);
				FklStringMatchPattern* pattern=fklFindStringPattern(token->value);
				if(pattern)
				{
					MatchState* state=newMatchState(pattern,token->line,1);
					fklPushPtrStack(state,matchStateStack);
					cStack=fklNewPtrStack(32,16);
					fklPushPtrStack(cStack,stackStack);
					const FklString* part=fklGetNthPartOfStringMatchPattern(pattern,state->cindex);
					if(fklIsVar(part)&&fklIsMustList(part))
					{
						cStack=fklNewPtrStack(32,16);
						fklPushPtrStack(cStack,stackStack);
					}
					continue;
				}
				else if(isBuilInPatternStart(token->value))
				{
					if(isLeftParenthese(token->value))
					{
						BUILTIN_PATTERN_START_PROCESS(token->value->str[0]=='('
								?PARENTHESE_0
								:PARENTHESE_1);
					}
					else if(isVectorStart(token->value))
					{
						BUILTIN_PATTERN_START_PROCESS(token->value->str[1]=='('
								?VECTOR_0
								:VECTOR_1);
					}
					else if(isBytevectorStart(token->value))
					{
						BUILTIN_PATTERN_START_PROCESS(token->value->str[4]=='('
								?BVECTOR_0
								:BVECTOR_1);
					}
					else if(isHashTableStart(token->value))
					{
						BUILTIN_PATTERN_START_PROCESS(getHashPattern(token->value));
					}
					continue;
				}
				else if(isBuiltInSingleStr(token->value))
				{
					const char* buf=token->value->str;
					FklStringMatchPattern* pattern=buf[0]=='\''?
						QUOTE:
						buf[0]=='`'?
						QSQUOTE:
						buf[0]==','?
						DOTTED:
						buf[0]=='~'?
						(token->value->size==1?
						 UNQUOTE:
						 buf[1]=='@'?
						 UNQTESP:NULL):
						(token->value->size==2&&buf[0]=='#'&&buf[1]=='&')?
						BOX:
						NULL;
					MatchState* state=newMatchState(pattern,token->line,0);
					fklPushPtrStack(state,matchStateStack);
					cStack=fklNewPtrStack(1,1);
					fklPushPtrStack(cStack,stackStack);
					continue;
				}
				else if(isRightParenthese(token->value))
				{
					fklPopPtrStack(stackStack);
					MatchState* cState=fklPopPtrStack(matchStateStack);
					FklNastNode* n=nastStackProcessers[(uintptr_t)cState->pattern](cStack,token->line,errorLine);
					fklFreePtrStack(cStack);
					freeMatchState(cState);
					if(n)
					{
						NastElem* v=newNastElem(NAST_CAR,n);
						cStack=fklTopPtrStack(stackStack);
						fklPushPtrStack(v,cStack);
					}
					else
					{
						while(!fklIsPtrStackEmpty(matchStateStack))
							freeMatchState(fklPopPtrStack(matchStateStack));
						fklFreePtrStack(matchStateStack);
						while(!fklIsPtrStackEmpty(stackStack))
						{
							FklPtrStack* cStack=fklPopPtrStack(stackStack);
							while(!fklIsPtrStackEmpty(cStack))
							{
								NastElem* elem=fklPopPtrStack(cStack);
								freeNastElem(elem);
							}
							fklFreePtrStack(cStack);
						}
						fklFreePtrStack(stackStack);
						return NULL;
					}
				}
			}
			for(MatchState* state=fklTopPtrStack(matchStateStack);
					state&&(isBuiltInSingleStrPattern(state->pattern)||(!isBuiltInParenthese(state->pattern)
							&&!isBuiltInVector(state->pattern)
							&&!isBuiltInBvector(state->pattern)
							&&!isBuiltInHashTable(state->pattern)));
					state=fklTopPtrStack(matchStateStack))
			{
				if(isBuiltInSingleStrPattern(state->pattern))
				{
					fklPopPtrStack(stackStack);
					MatchState* cState=fklPopPtrStack(matchStateStack);
					NastElem* postfix=fklPopPtrStack(cStack);
					fklFreePtrStack(cStack);
					cStack=fklTopPtrStack(stackStack);
					if(state->pattern==DOTTED)
					{
						postfix->place=NAST_CDR;
						fklPushPtrStack(postfix,cStack);
					}
					else if(state->pattern==BOX)
					{
						FklNastNode* box=newNastNode(FKL_TYPE_BOX,token->line);
						box->u.box=postfix->node;
						NastElem* v=newNastElem(NAST_CAR,box);
						fklPushPtrStack(v,cStack);
						freeNastElem(postfix);
					}
					else
					{
						FklNastNode* prefix=newNastNode(FKL_TYPE_SYM,token->line);
						FklStringMatchPattern* pattern=cState->pattern;
						FklSid_t prefixValue=pattern==QUOTE?
							builtInPattern_head_quote:
							pattern==QSQUOTE?
							builtInPattern_head_qsquote:
							pattern==UNQUOTE?
							builtInPattern_head_unquote:
							builtInPattern_head_unqtesp;
						prefix->u.sym=prefixValue;
						FklNastNode* tmp[]={prefix,postfix->node,};
						FklNastNode* list=newNastList(tmp,2,prefix->curline);
						free(postfix);
						NastElem* v=newNastElem(NAST_CAR,list);
						fklPushPtrStack(v,cStack);
					}
					freeMatchState(cState);
				}
			}
		}
		NastElem* retvalElem=fklTopPtrStack(elemStack);
		if(retvalElem)
			retval=retvalElem->node;
		free(retvalElem);
		fklFreePtrStack(stackStack);
		fklFreePtrStack(matchStateStack);
		fklFreePtrStack(elemStack);
	}
	return retval;
}

#undef BUILTIN_PATTERN_START_PROCESS

void fklPrintNastNode(const FklNastNode* exp,FILE* fp)
{
	FklPtrQueue* queue=fklNewPtrQueue();
	FklPtrStack* queueStack=fklNewPtrStack(32,16);
	fklPushPtrQueue(newNastElem(NAST_CAR,(FklNastNode*)exp),queue);
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
			freeNastElem(e);
			switch(node->type)
			{
				case FKL_TYPE_SYM:
					fklPrintRawSymbol(fklGetGlobSymbolWithId(node->u.sym)->symbol,fp);
					break;
				case FKL_TYPE_BYTEVECTOR:
					fklPrintRawBytevector(node->u.bvec,fp);
					break;
				case FKL_TYPE_STR:
					fklPrintRawString(node->u.str,fp);
					break;
				case FKL_TYPE_I32:
					fprintf(fp,"%d",node->u.i32);
					break;
				case FKL_TYPE_I64:
					fprintf(fp,"%ld",node->u.i64);
					break;
				case FKL_TYPE_F64:
					fprintf(fp,"%lf",node->u.f64);
					break;
				case FKL_TYPE_CHR:
					fklPrintRawChar(node->u.chr,fp);
					break;
				case FKL_TYPE_VECTOR:
					fputs("#(",fp);
					{
						FklPtrQueue* vQueue=fklNewPtrQueue();
						for(size_t i=0;i<node->u.vec->size;i++)
							fklPushPtrQueue(newNastElem(NAST_CAR,node->u.vec->base[i]),vQueue);
						fklPushPtrStack(vQueue,queueStack);
						cQueue=vQueue;
						continue;
					}
					break;
				case FKL_TYPE_BIG_INT:
					fklPrintBigInt(node->u.bigInt,fp);
					break;
				case FKL_TYPE_BOX:
					fputs("#&",fp);
					fklPushPtrQueue(newNastElem(NAST_BOX,node->u.box),cQueue);
					break;
				case FKL_TYPE_HASHTABLE:
					{
						const static char* tmp[]=
						{
							"#hash",
							"#hasheqv",
							"#hashequal",
						};
						fputs(tmp[node->u.hash->type],fp);
					}
//					fklPushPtrQueue(newNastElem(NAST_BOX,node->u.hash->items),cQueue);
					break;
				case FKL_TYPE_NIL:
					fputs("()",fp);
					break;
				case FKL_TYPE_PAIR:
					{
						fputc('(',fp);
						FklPtrQueue* lQueue=fklNewPtrQueue();
						FklNastNode* cur=node;
						for(;cur->type==FKL_TYPE_PAIR;cur=cur->u.pair->cdr)
						{
							FklNastNode* c=cur->u.pair->car;
							NastElem* ce=newNastElem(NAST_CAR,c);
							fklPushPtrQueue(ce,lQueue);
						}
						if(cur->type!=FKL_TYPE_NIL)
						{
							NastElem* cdre=newNastElem(NAST_CDR,cur);
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
		fklFreePtrQueue(cQueue);
		if(!fklIsPtrStackEmpty(queueStack))
		{
			fputc(')',fp);
			cQueue=fklTopPtrStack(queueStack);
			if(fklLengthPtrQueue(cQueue)&&((NastElem*)fklFirstPtrQueue(cQueue))->place!=NAST_CDR)
				fputc(' ',fp);
		}
	}
	fklFreePtrStack(queueStack);
}

static void freeNastPair(FklNastPair* pair)
{
	free(pair);
}

static void freeNastVector(FklNastVector* vec)
{
	free(vec->base);
	free(vec);
}

static void freeNastHash(FklNastHashTable* hash)
{
	free(hash->items);
	free(hash);
}

void fklFreeNastNode(FklNastNode* node)
{
	FklPtrStack* stack=fklNewPtrStack(32,16);
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
					case FKL_TYPE_I64:
					case FKL_TYPE_I32:
					case FKL_TYPE_SYM:
					case FKL_TYPE_NIL:
					case FKL_TYPE_CHR:
					case FKL_TYPE_F64:
						break;
					case FKL_TYPE_STR:
						free(cur->u.str);
						break;
					case FKL_TYPE_BYTEVECTOR:
						free(cur->u.bvec);
						break;
					case FKL_TYPE_BIG_INT:
						fklFreeBigInt(cur->u.bigInt);
						break;
					case FKL_TYPE_PAIR:
						fklPushPtrStack(cur->u.pair->car,stack);
						fklPushPtrStack(cur->u.pair->cdr,stack);
						freeNastPair(cur->u.pair);
						break;
					case FKL_TYPE_BOX:
						fklPushPtrStack(cur->u.box,stack);
						break;
					case FKL_TYPE_VECTOR:
						for(size_t i=0;i<cur->u.vec->size;i++)
							fklPushPtrStack(cur->u.vec->base[i],stack);
						freeNastVector(cur->u.vec);
						break;
					case FKL_TYPE_HASHTABLE:
						for(size_t i=0;i<cur->u.hash->num;i++)
						{
							fklPushPtrStack(cur->u.hash->items[i].car,stack);
							fklPushPtrStack(cur->u.hash->items[i].cdr,stack);
						}
						freeNastHash(cur->u.hash);
						break;
					default:
						FKL_ASSERT(0);
						break;
				}
				free(cur);
			}
		}
	}
	fklFreePtrStack(stack);
}

static int nastNodeEqual(const FklNastNode* n0,const FklNastNode* n1)
{
	FklPtrStack* s0=fklNewPtrStack(32,16);
	FklPtrStack* s1=fklNewPtrStack(32,16);
	fklPushPtrStack((void*)n0,s0);
	fklPushPtrStack((void*)n1,s1);
	int r=1;
	while(!fklIsPtrStackEmpty(s0)&&!fklIsPtrStackEmpty(s1)&&r)
	{
		FklNastNode* root0=fklPopPtrStack(s0);
		FklNastNode* root1=fklPopPtrStack(s1);
		if(root0!=root1)
		{
			if(root0->type!=root1->type)
			{
				r=0;
				break;
			}
			else
			{
				switch(root0->type)
				{
					case FKL_TYPE_SYM:
						r=root0->u.sym==root1->u.sym;
						break;
					case FKL_TYPE_STR:
						r=!fklStringcmp(root0->u.str,root1->u.str);
						break;
					case FKL_TYPE_BYTEVECTOR:
						r=!fklBytevectorcmp(root0->u.bvec,root1->u.bvec);
						break;
					case FKL_TYPE_CHR:
						r=root0->u.chr==root1->u.chr;
						break;
					case FKL_TYPE_I32:
						r=root0->u.i32==root1->u.i32;
						break;
					case FKL_TYPE_I64:
						r=root0->u.i64==root1->u.i64;
						break;
					case FKL_TYPE_BIG_INT:
						r=fklCmpBigInt(root0->u.bigInt,root1->u.bigInt);
						break;
					case FKL_TYPE_F64:
						r=root0->u.f64==root1->u.f64;
						break;
					case FKL_TYPE_PAIR:
						fklPushPtrStack(root0->u.pair->cdr,s0);
						fklPushPtrStack(root0->u.pair->car,s0);
						fklPushPtrStack(root1->u.pair->cdr,s1);
						fklPushPtrStack(root1->u.pair->car,s1);
						break;
					case FKL_TYPE_VECTOR:
						if(root0->u.vec->size!=root1->u.vec->size)
							r=0;
						else
						{
							size_t size=root0->u.vec->size;
							for(size_t i=0;i<size;i++)
							{
								fklPushPtrStack(root0->u.vec->base[i],s0);
								fklPushPtrStack(root1->u.vec->base[i],s1);
							}
						}
						break;
					case FKL_TYPE_HASHTABLE:
						if(root0->u.hash->type!=root1->u.hash->type
								||root0->u.hash->num!=root1->u.hash->num)
							r=0;
						else
						{
							size_t num=root0->u.hash->num;
							for(size_t i=0;i<num;i++)
							{
								fklPushPtrStack(root0->u.hash->items[i].car,s0);
								fklPushPtrStack(root0->u.hash->items[i].cdr,s0);
								fklPushPtrStack(root1->u.hash->items[i].car,s1);
								fklPushPtrStack(root1->u.hash->items[i].cdr,s1);
							}
						}
						break;
					case FKL_TYPE_NIL:
						break;
					default:
						FKL_ASSERT(0);
				}
			}
		}
	}
	fklFreePtrStack(s0);
	fklFreePtrStack(s1);
	return r;
}

typedef struct
{
	FklSid_t id;
	FklNastNode* node;
}PatternMatchingHashTableItem;

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
	return &((PatternMatchingHashTableItem*)item)->id;
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
	.__freeItem=_pattern_matching_hash_free_item,
	.__keyEqual=_pattern_matching_hash_key_equal,
	.__getKey=_pattern_matching_hash_get_key,
};

FklNastNode* fklPatternMatchingHashTableRef(FklSid_t sid,FklHashTable* ht)
{
	PatternMatchingHashTableItem* item=fklGetHashItem(&sid,ht);
	return item->node;
}

inline static PatternMatchingHashTableItem* newPatternMatchingHashTableItem(FklSid_t id,FklNastNode* node)
{
	PatternMatchingHashTableItem* r=(PatternMatchingHashTableItem*)malloc(sizeof(PatternMatchingHashTableItem));
	FKL_ASSERT(r);
	r->id=id;
	r->node=node;
	return r;
}

static void patternMatchingHashTableSet(FklSid_t sid,FklNastNode* node,FklHashTable* ht)
{
	fklPutReplHashItem(newPatternMatchingHashTableItem(sid,node),ht);
}

FklHashTable* fklNewPatternMatchingHashTable(void)
{
	return fklNewHashTable(8,8,2,0.75,1,&Codegen_hash_method_table);
}

int fklPatternMatch(const FklNastNode* pattern,FklNastNode* exp,FklHashTable* ht)
{
	if(exp->type!=FKL_TYPE_PAIR)
		return 0;
	if(exp->u.pair->car->type!=FKL_TYPE_SYM
			||pattern->u.pair->car->u.sym!=exp->u.pair->car->u.sym)
		return 0;
	FklPtrStack* s0=fklNewPtrStack(32,16);
	FklPtrStack* s1=fklNewPtrStack(32,16);
	fklPushPtrStack(pattern->u.pair->cdr,s0);
	fklPushPtrStack(exp->u.pair->cdr,s1);
	while(!fklIsPtrStackEmpty(s0)&&!fklIsPtrStackEmpty(s1))
	{
		FklNastNode* n0=fklPopPtrStack(s0);
		FklNastNode* n1=fklPopPtrStack(s1);
		if(n0->type==FKL_TYPE_SYM)
			patternMatchingHashTableSet(n0->u.sym,n1,ht);
		else if(n0->type==FKL_TYPE_PAIR&&n1->type==FKL_TYPE_PAIR)
		{
			fklPushPtrStack(n0->u.pair->cdr,s0);
			fklPushPtrStack(n0->u.pair->car,s0);
			fklPushPtrStack(n1->u.pair->cdr,s1);
			fklPushPtrStack(n1->u.pair->car,s1);
		}
		else if(!nastNodeEqual(n0,n1))
		{
			fklFreePtrStack(s0);
			fklFreePtrStack(s1);
			return 0;
		}
	}
	fklFreePtrStack(s0);
	fklFreePtrStack(s1);
	return 1;
}

int fklIsNastNodeList(const FklNastNode* list)
{
	for(;list->type==FKL_TYPE_PAIR;list=list->u.pair->cdr);
	return list->type==FKL_TYPE_NIL;
}

FklNastNode* fklMakeNastNodeRef(FklNastNode* n)
{
	n->refcount+=1;
	return n;
}
