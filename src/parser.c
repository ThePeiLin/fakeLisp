#include<fakeLisp/parser.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/reader.h>
#include<ctype.h>
#include<string.h>

//static size_t skipSpace(const char* str)
//{
//	size_t i=0;
//	for(;str[i]!='\0'&&isspace(str[i]);i++);
//	return i;
//}


size_t skipString(const char* str)
{
	size_t i=1;
	for(;str[i]!='\0'&&(str[i-1]=='\\'||str[i]!='\"');i++);
	if(str[i]=='\"')
		i++;
	return i;
}

static size_t skipStringIndexSize(const char* str,size_t index,size_t size,char ch)
{
	size_t i=1;
	for(;index+i<size&&(str[index+i-1]=='\\'||str[index+i]!=ch);i++);
	if(index+i<size&&str[index+i]==ch)
		i++;
	return i;
}

size_t skipUntilSpace(const FklString* str)
{
	size_t i=0;
	size_t size=str->size;
	const char* buf=str->str;
	for(;i<size&&!isspace(buf[i]);i++)
	{
		if(buf[i]=='\"')
			i+=skipString(buf+i);
		if(buf[i]=='\0')
			break;
	}
	return i;
}

int isCompleteString(const FklString* str,char ch)
{
	int mark=0;
	int markN=0;
	size_t size=str->size;
	const char* buf=str->str;
	for(size_t i=0;i<size&&markN<2;i++)
		if(buf[i]==ch&&(!i||buf[i-1]!='\\'))
		{
			mark=~mark;
			markN+=1;
		}
	return !mark;
}

//uint32_t countPartThatSplitByBlank(const char* str,FklUintStack* indexStack,FklUintStack* sizeStack)
//{
//	uint32_t retval=0;
//	for(size_t i=0;str[i]!='\0';)
//	{
//		fklPushUintStack(i,indexStack);
//		size_t size=skipUntilSpace(str+i);
//		i+=size;
//		fklPushUintStack(size,sizeStack);
//		i+=skipSpace(str+i);
//		retval++;
//	}
//	return retval;
//}

//static char** spiltStringByBlank(const char* str,uint32_t* num)
//{
//	FKL_ASSERT(num);
//	FklUintStack* sizeStack=fklNewUintStack(32,16);
//	FklUintStack* indexStack=fklNewUintStack(32,16);
//	*num=countPartThatSplitByBlank(str,indexStack,sizeStack);
//	char** retval=(char**)malloc(sizeof(char*)*(*num));
//	FKL_ASSERT(retval);
//	for(uint32_t i=0;i<indexStack->top;i++)
//	{
//		retval[i]=fklCopyMemory(str+indexStack->base[i],sizeStack->base[i]+1);
//		retval[i][sizeStack->base[i]]='\0';
//	}
//	fklFreeUintStack(indexStack);
//	fklFreeUintStack(sizeStack);
//	return retval;
//}

FklToken* fklNewTokenCopyStr(FklTokenType type,const FklString* str,uint32_t line)
{
	FklToken* token=(FklToken*)malloc(sizeof(FklToken));
	FKL_ASSERT(token);
	token->type=type;
	token->line=line;
	token->value=fklCopyString(str);
	return token;
}

FklToken* fklNewToken(FklTokenType type,FklString* str,uint32_t line)
{
	FklToken* token=(FklToken*)malloc(sizeof(FklToken));
	FKL_ASSERT(token);
	token->type=type;
	token->line=line;
	token->value=str;
	return token;
}

void fklFreeToken(FklToken* token)
{
	free(token->value);
	free(token);
}

typedef struct
{
	FklStringMatchPattern* pattern;
	uint32_t index;
}MatchState;

static MatchState* newMatchState(FklStringMatchPattern* pattern,uint32_t index)
{
	MatchState* state=(MatchState*)malloc(sizeof(MatchState));
	FKL_ASSERT(state);
	state->pattern=pattern;
	state->index=index;
	return state;
}

static void freeMatchState(MatchState* state)
{
	free(state);
}

#define BUILT_IN_SEPARATOR_STR_NUM (15)
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
#define INCOMPLETE_SYMBOL ((void*)10)
#define BVECTOR_0 ((void*)11)
#define BVECTOR_1 ((void*)12)
#define HASH_EQ_0 ((void*)13)
#define HASH_EQ_1 ((void*)14)
#define HASH_EQV_0 ((void*)15)
#define HASH_EQV_1 ((void*)16)
#define HASH_EQUAL_0 ((void*)17)
#define HASH_EQUAL_1 ((void*)18)

static const char* separatorStrSet[]=
{
	"~@",
	"#(",
	"#[",
	"#vu8(",
	"#vu8[",
	"#hash(",
	"#hash[",
	"#hasheqv(",
	"#hasheqv[",
	"#hashequal(",
	"#hashequal[",
	"(",
	",",
	"#\\",
	"\"",
	"[",
	";",
	"#!",
	"'",
	"`",
	"~",
	"#&",
	"|",
};

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

static int isBuiltInPattern(FklStringMatchPattern* pattern)
{
	return pattern==PARENTHESE_0
		||pattern==PARENTHESE_1
		||pattern==QUOTE
		||pattern==QSQUOTE
		||pattern==UNQUOTE
		||pattern==UNQTESP
		||pattern==DOTTED
		||pattern==VECTOR_0
		||pattern==VECTOR_1
		||pattern==BVECTOR_0
		||pattern==BVECTOR_1
		||pattern==BOX
		||pattern==HASH_EQ_0
		||pattern==HASH_EQ_1
		||pattern==HASH_EQV_0
		||pattern==HASH_EQV_1
		||pattern==HASH_EQUAL_0
		||pattern==HASH_EQUAL_1;
}

static int isBuiltInParenthese(FklStringMatchPattern* pattern)
{
	return pattern==PARENTHESE_0
		||pattern==PARENTHESE_1
		||pattern==VECTOR_0
		||pattern==VECTOR_1
		||pattern==BVECTOR_0
		||pattern==BVECTOR_1
		||pattern==HASH_EQ_0
		||pattern==HASH_EQ_1
		||pattern==HASH_EQV_0
		||pattern==HASH_EQV_1
		||pattern==HASH_EQUAL_0
		||pattern==HASH_EQUAL_1;
}

static MatchState* searchReverseStringCharMatchState(const char* part,size_t index,size_t size,FklPtrStack* stack)
{
	if(index<size)
	{
		MatchState* topState=fklTopPtrStack(stack);
		for(uint32_t i=stack->top;topState
				&&!isBuiltInParenthese(topState->pattern)
				&&(isBuiltInSingleStrPattern(topState->pattern)
					||topState->pattern==INCOMPLETE_SYMBOL
					||(fklIsVar(fklGetNthPartOfStringMatchPattern(topState->pattern,topState->index))
						&&(topState->index==topState->pattern->num-1
							||fklIsVar(fklGetNthPartOfStringMatchPattern(topState->pattern,topState->index+1)))));
				i--)
			topState=i==0?NULL:stack->base[i-1];
		if(topState&&!isBuiltInPattern(topState->pattern))
		{
			const FklString* nextPart=fklGetNthPartOfStringMatchPattern(topState->pattern,topState->index);
			uint32_t i=0;
			for(;;)
			{
				nextPart=fklGetNthPartOfStringMatchPattern(topState->pattern,topState->index+i);
				if(!(nextPart&&fklIsVar(nextPart)&&fklIsMustList(nextPart)))
					break;
				i++;
			}
			if(size-index>=nextPart->size&&topState->pattern!=NULL&&nextPart&&!fklIsVar(nextPart)&&!memcmp(nextPart->str,part+index,nextPart->size))
			{
				topState->index+=i;
				return topState;
			}
		}
		FklStringMatchPattern* pattern=fklFindStringPatternBuf(part+index,size-index);
		if(pattern)
			return newMatchState(pattern,0);
		else if(size-index>1&&part[index]=='#'&&part[index+1]=='(')
			return newMatchState(VECTOR_0,0);
		else if(size-index>1&&part[index]=='#'&&part[index+1]=='[')
			return newMatchState(VECTOR_1,0);
		else if(size-index>4&&!strncmp("#vu8(",part+index,5))
			return newMatchState(BVECTOR_0,0);
		else if(size-index>4&&!strncmp("#vu8[",part+index,5))
			return newMatchState(BVECTOR_1,0);
		else if(size-index>5&&!strncmp("#hash(",part+index,6))
			return newMatchState(HASH_EQ_0,0);
		else if(size-index>5&&!strncmp("#hash[",part+index,6))
			return newMatchState(HASH_EQ_1,0);
		else if(size-index>8&&!strncmp("#hasheqv(",part+index,9))
			return newMatchState(HASH_EQV_0,0);
		else if(size-index>8&&!strncmp("#hasheqv[",part+index,9))
			return newMatchState(HASH_EQV_1,0);
		else if(size-index>10&&!strncmp("#hashequal(",part+index,11))
			return newMatchState(HASH_EQUAL_0,0);
		else if(size-index>10&&!strncmp("#hashequal[",part+index,11))
			return newMatchState(HASH_EQUAL_1,0);
		else if(part[index]=='(')
			return newMatchState(PARENTHESE_0,0);
		else if(part[index]==')')
		{
			if(topState&&topState->pattern==PARENTHESE_0&&topState->index==0)
				return newMatchState(PARENTHESE_0,1);
			else if(topState&&topState->pattern==VECTOR_0&&topState->index==0)
				return newMatchState(VECTOR_0,1);
			else if(topState&&topState->pattern==BVECTOR_0&&topState->index==0)
				return newMatchState(BVECTOR_0,1);
			else if(topState&&topState->pattern==HASH_EQ_0&&topState->index==0)
				return newMatchState(HASH_EQ_0,1);
			else if(topState&&topState->pattern==HASH_EQV_0&&topState->index==0)
				return newMatchState(HASH_EQV_0,1);
			else if(topState&&topState->pattern==HASH_EQUAL_0&&topState->index==0)
				return newMatchState(HASH_EQUAL_0,1);
			else
				return NULL;
		}
		else if(part[index]=='[')
			return newMatchState(PARENTHESE_1,0);
		else if(part[index]==']')
		{
			if(topState&&topState->pattern==PARENTHESE_1&&topState->index==0)
				return newMatchState(PARENTHESE_1,1);
			else if(topState&&topState->pattern==VECTOR_1&&topState->index==0)
				return newMatchState(VECTOR_1,1);
			else if(topState&&topState->pattern==BVECTOR_1&&topState->index==0)
				return newMatchState(BVECTOR_1,1);
			else if(topState&&topState->pattern==HASH_EQ_1&&topState->index==0)
				return newMatchState(HASH_EQ_1,1);
			else if(topState&&topState->pattern==HASH_EQV_1&&topState->index==0)
				return newMatchState(HASH_EQV_1,1);
			else if(topState&&topState->pattern==HASH_EQUAL_1&&topState->index==0)
				return newMatchState(HASH_EQUAL_1,1);
			else
				return NULL;
		}
		switch(part[index])
		{
			case '\'':
				return newMatchState(QUOTE,0);
				break;
			case '`':
				return newMatchState(QSQUOTE,0);
				break;
			case '~':
				if(size-index>1&&part[index+1]=='@')
					return newMatchState(UNQTESP,0);
				return newMatchState(UNQUOTE,0);
				break;
			case ',':
				return newMatchState(DOTTED,0);
				break;
			case '#':
				if(size-index>1&&part[index+1]=='&')
					return newMatchState(BOX,0);
				break;
		}
	}
	return NULL;
}

static int hasReverseStringNext(MatchState* state)
{
	FklStringMatchPattern* pattern=state->pattern;
	for(uint32_t j=0;state->index+j<pattern->num;j++)
	{
		if(!fklIsVar(fklGetNthPartOfStringMatchPattern(pattern,state->index+j)))
			return 1;
	}
	return 0;
}

static int searchReverseStringChar(const char* part,size_t index,size_t size,FklPtrStack* stack)
{
	if(index<size)
	{
		MatchState* topState=fklTopPtrStack(stack);
		for(uint32_t i=stack->top;topState
				&&!isBuiltInParenthese(topState->pattern)
				&&(isBuiltInSingleStrPattern(topState->pattern)
					||topState->pattern==INCOMPLETE_SYMBOL
					||!hasReverseStringNext(topState));
				i--)
			topState=i==0?NULL:stack->base[i-1];
		if(topState&&!isBuiltInPattern(topState->pattern))
		{
			const FklString* nextPart=fklGetNthPartOfStringMatchPattern(topState->pattern,topState->index);
			uint32_t i=0;
			for(;nextPart&&fklIsVar(nextPart);i++)
				nextPart=fklGetNthPartOfStringMatchPattern(topState->pattern,topState->index+i);
			if(size-index>nextPart->size&&topState->pattern!=NULL&&nextPart&&!fklIsVar(nextPart)&&!memcmp(nextPart->str,part+index,nextPart->size))
				return 1;
		}
		FklStringMatchPattern* pattern=fklFindStringPatternBuf(part+index,size-index);
		if(pattern)
			return 1;
		else if((size-index>1&&part[index]=='#'&&part[index+1]=='(')
				||(size-index>1&&part[index]=='#'&&part[index+1]=='[')
				||(size-index>4&&!strncmp("#vu8(",part+index,5))
				||(size-index>4&&!strncmp("#vu8[",part+index,5))
				||(size-index>5&&!strncmp("#hash(",part+index,6))
				||(size-index>5&&!strncmp("#hash[",part+index,6))
				||(size-index>8&&!strncmp("#hasheqv(",part+index,9))
				||(size-index>8&&!strncmp("#hasheqv[",part+index,9))
				||(size-index>10&&!strncmp("#hashequal(",part+index,11))
				||(size-index>10&&!strncmp("#hashequal[",part+index,11))
				||(part[index]=='(')
				||(part[index]=='[')
				)
			return 1;
		else if(part[index]==')')
		{
			if(topState&&(topState->pattern==PARENTHESE_0
						||topState->pattern==VECTOR_0
						||topState->pattern==BVECTOR_0
						||topState->pattern==HASH_EQ_0
						||topState->pattern==HASH_EQV_0
						||topState->pattern==HASH_EQUAL_0)
					&&topState->index==0)
				return 1;
			else
				return 0;
		}
		else if(part[index]==']')
		{
			if(topState&&(topState->pattern==PARENTHESE_1
						||topState->pattern==VECTOR_1
						||topState->pattern==BVECTOR_1
						||topState->pattern==HASH_EQ_1
						||topState->pattern==HASH_EQV_1
						||topState->pattern==HASH_EQUAL_1)
					&&topState->index==0)
				return 1;
			else
				return 0;
		}
		switch(part[index])
		{
			case '\'':
				return 1;
				break;
			case '`':
				return 1;
				break;
			case '~':
				if(size-index>1&&part[index+1]=='@')
					return 1;
				return 1;
				break;
			case ',':
				return 1;
				break;
			case '#':
				if(size-index>1&&part[index+1]=='&')
					return 1;
				break;
		}
	}
	return 0;
}

static int isBuiltInReserveStr(const char* part,size_t size)
{
	int r=0;
	for(uint32_t i=0;i<BUILT_IN_SEPARATOR_STR_NUM;i++)
		if(size>=strlen(separatorStrSet[i])&&!strncmp(part,separatorStrSet[i],strlen(separatorStrSet[i])))
		{
			r=1;
			break;
		}
	return r;
}

static size_t getSymbolLen(const char* part,size_t index,size_t size,FklPtrStack* stack)
{
	size_t i=0;
	for(;index+i<size&&!isspace(part[index+i])&&!isBuiltInReserveStr(part+i+index,size-i-index);i++)
	{
		int state=searchReverseStringChar(part,index+i,size,stack);
		if(state)
			break;
	}
	return i;
}

static size_t skipSpaceAndCountLine(const char* str,size_t index,size_t size,uint32_t* cline)
{
	size_t j=0;
	for(;index+j<size&&isspace(str[index+j]);j++)
		if(str[index+j]=='\n')
			(*cline)++;
	return j;
}

static int isDivider(const char* buf,size_t i,size_t size,FklPtrStack* matchStateStack)
{
	return isspace(buf[i])
			||searchReverseStringChar(buf,i,size,matchStateStack)
			||isBuiltInReserveStr(buf+i,size);
}

int fklSplitStringPartsIntoToken(const char** parts,size_t* sizes,uint32_t inum,uint32_t* line,FklPtrStack* retvalStack,FklPtrStack* matchStateStack,uint32_t* pi,uint32_t* pj)
{
	int done=0;
	uint32_t i=0;
	for(;i<inum;i++)
	{
		size_t j=0;
		for(;;)
		{
			j+=skipSpaceAndCountLine(parts[i],j,sizes[i],line);
			if(j>=sizes[i])
				break;
			MatchState* state=searchReverseStringCharMatchState(parts[i],j,sizes[i],matchStateStack);
			if(state)
			{
				if(isBuiltInParenthese(state->pattern))
				{
					if(state->index==0)
					{
						const char* parenthese=state->pattern==PARENTHESE_0?"(":
							state->pattern==PARENTHESE_1?"[":
							state->pattern==VECTOR_0?"#(":
							state->pattern==VECTOR_1?"#[":
							state->pattern==BVECTOR_0?"#vu8(":
							state->pattern==BVECTOR_1?"#vu8[":
							state->pattern==HASH_EQ_0?"#hash(":
							state->pattern==HASH_EQ_1?"#hash[":
							state->pattern==HASH_EQV_0?"#hasheqv(":
							state->pattern==HASH_EQV_1?"#hasheqv[":
							state->pattern==HASH_EQUAL_0?"#hashequal(":
							"#hashequal[";
						fklPushPtrStack(fklNewToken(FKL_TOKEN_RESERVE_STR,fklNewStringFromCstr(parenthese),*line),retvalStack);
						fklPushPtrStack(state,matchStateStack);
						j+=strlen(parenthese);
						continue;
					}
					else if(state->index==1)
					{
						const char* parenthese=(state->pattern==PARENTHESE_0
								||state->pattern==VECTOR_0
								||state->pattern==BVECTOR_0
								||state->pattern==HASH_EQ_0
								||state->pattern==HASH_EQV_0
								||state->pattern==HASH_EQUAL_0)?")":"]";
						MatchState* prevState=fklTopPtrStack(matchStateStack);
						if(!isBuiltInParenthese(prevState->pattern))
						{
							freeMatchState(state);
							while(!fklIsPtrStackEmpty(matchStateStack))
								freeMatchState(fklPopPtrStack(matchStateStack));
							return 2;
						}
						fklPushPtrStack(fklNewToken(FKL_TOKEN_RESERVE_STR,fklNewStringFromCstr(parenthese),*line),retvalStack);
						freeMatchState(prevState);
						fklPopPtrStack(matchStateStack);
						freeMatchState(state);
						j++;
					}
				}
				else if(isBuiltInSingleStrPattern(state->pattern))
				{
					const char* str=state->pattern==QSQUOTE?
						"`":
						state->pattern==QUOTE?
						"'":
						state->pattern==UNQUOTE?
						"~":
						state->pattern==UNQTESP?
						"~@":
						state->pattern==DOTTED?
						",":
						state->pattern==BOX?
						"#&":
						NULL;
					fklPushPtrStack(fklNewToken(FKL_TOKEN_RESERVE_STR,fklNewStringFromCstr(str),*line),retvalStack);
					fklPushPtrStack(state,matchStateStack);
					j+=strlen(str);
					continue;
				}
				else if(state->index<state->pattern->num)
				{
					const FklString* curPart=fklGetNthPartOfStringMatchPattern(state->pattern,state->index);
					MatchState* prevState=fklTopPtrStack(matchStateStack);
					if(state->index!=0&&prevState->pattern!=state->pattern)
					{
						while(!fklIsPtrStackEmpty(matchStateStack))
							freeMatchState(fklPopPtrStack(matchStateStack));
						return 2;
					}
					state->index++;
					if(state->index-1==0)
						fklPushPtrStack(state,matchStateStack);
					fklPushPtrStack(fklNewToken(FKL_TOKEN_RESERVE_STR,fklCopyString(curPart),*line),retvalStack);
					j+=curPart->size;
					if(state->index<state->pattern->num)
						continue;
				}
			}
			else if(parts[i][j]=='\"')
			{
				size_t sumLen=skipStringIndexSize(parts[i],j,sizes[i],'\"');
				size_t lastLen=sumLen;
				FklString* str=fklNewString(sumLen,parts[i]+j);
				int complete=isCompleteString(str,'"');
				for(;!complete&&j+lastLen>=sizes[i];)
				{
					i++;
					if(i>=inum)
					{
						i--;
						break;
					}
					j=0;
					fklStringCharBufCat(&str,parts[i],sizes[i]);
					sumLen=skipUntilSpace(str);
					complete=isCompleteString(str,'"');
					lastLen=sumLen-lastLen;
				}
				done|=!complete;
				if(complete)
				{
					fklPushPtrStack(fklNewToken(FKL_TOKEN_STRING,fklCopyString(str),*line),retvalStack);
					(*line)+=fklCountChar(str->str,'\n',str->size);
					j+=lastLen;
					free(str);
				}
				else
				{
					free(str);
					break;
				}
			}
			else if(parts[i][j]=='|')
			{
				size_t sumLen=skipStringIndexSize(parts[i],j,sizes[i],'|');
				size_t lastLen=sumLen;
				FklString* str=fklNewString(sumLen,parts[i]+j);
				int complete=isCompleteString(str,'|');
				for(;!complete&&j+lastLen>=sizes[i];)
				{
					i++;
					if(i>=inum)
					{
						i--;
						break;
					}
					j=0;
					fklStringCharBufCat(&str,parts[i],sizes[i]);
					sumLen=skipUntilSpace(str);
					complete=isCompleteString(str,'|');
					lastLen=sumLen-lastLen;
				}
				done|=!complete;
				if(complete)
				{
					size_t size=0;
					char* s=fklCastEscapeCharBuf(str->str+1,'|',&size);
					MatchState* topState=fklTopPtrStack(matchStateStack);
					(*line)+=fklCountChar(str->str,'\n',str->size);
					j+=lastLen;
					free(str);
					if(!topState||topState->pattern!=INCOMPLETE_SYMBOL)
					{
						fklPushPtrStack(fklNewToken(FKL_TOKEN_SYMBOL,fklNewString(size,s),*line),retvalStack);
						free(s);
						if(j<sizes[i]&&(parts[i][j]=='|'||!isDivider(parts[i],j,sizes[i],matchStateStack)))
						{
							fklPushPtrStack(newMatchState(INCOMPLETE_SYMBOL,0),matchStateStack);
							continue;
						}
					}
					else
					{
						FklToken* topToken=fklTopPtrStack(retvalStack);
						fklStringCharBufCat(&topToken->value,s,size);
						free(s);
						fklPopPtrStack(matchStateStack);
						if(j<sizes[i]&&(parts[i][j]=='|'||!isDivider(parts[i],j,sizes[i],matchStateStack)))
						{
							fklPushPtrStack(topState,matchStateStack);
							continue;
						}
						else
							freeMatchState(topState);
					}
				}
				else
				{
					free(str);
					break;
				}
			}
			else if(sizes[i]-j>1&&!strncmp(parts[i]+j,"#\\",strlen("#\\")))
			{
				size_t len=getSymbolLen(parts[i],j+3,sizes[i],matchStateStack)+3;
				fklPushPtrStack(fklNewToken(FKL_TOKEN_CHAR,fklNewString(len,parts[i]+j),*line),retvalStack);
				j+=len;
			}
			else if(parts[i][j]==';'||(sizes[i]-j>1&&!strncmp(parts[i]+j,"#!",strlen("#!"))))
			{
				size_t len=0;
				for(;j+len<sizes[i]&&parts[i][j+len]!='\n';len++);
				fklPushPtrStack(fklNewToken(FKL_TOKEN_COMMENT,fklNewString(len,parts[i]+j),*line),retvalStack);
				j+=len;
				continue;
			}
			else
			{
				size_t len=getSymbolLen(parts[i],j,sizes[i],matchStateStack);
				if(len)
				{
					MatchState* topState=fklTopPtrStack(matchStateStack);
					if(!topState||topState->pattern!=INCOMPLETE_SYMBOL)
					{
						FklString* symbol=fklNewString(len,parts[i]+j);
						j+=len;
						FklToken* sym=fklNewToken(FKL_TOKEN_SYMBOL,symbol,*line);
						fklPushPtrStack(sym,retvalStack);
						if(j<sizes[i]&&parts[i][j]=='|')
						{
							fklPushPtrStack(newMatchState(INCOMPLETE_SYMBOL,0),matchStateStack);
							continue;
						}
						else if(fklIsNumberString(symbol))
							sym->type=FKL_TOKEN_NUM;
					}
					else
					{
						FklToken* topToken=fklTopPtrStack(retvalStack);
						fklStringCharBufCat(&topToken->value,parts[i]+j,len);
						j+=len;
						if(topState&&j<sizes[i]&&parts[i][j]!='|')
							freeMatchState(fklPopPtrStack(matchStateStack));
						else
							continue;
					}
				}
				else
				{
					while(!fklIsPtrStackEmpty(matchStateStack))
						freeMatchState(fklPopPtrStack(matchStateStack));
					return 2;
				}
			}
			for(MatchState* topState=fklTopPtrStack(matchStateStack);
					topState
					&&(isBuiltInSingleStrPattern(topState->pattern)||!isBuiltInParenthese(topState->pattern));
					topState=fklTopPtrStack(matchStateStack))
			{
				if(isBuiltInSingleStrPattern(topState->pattern))
					freeMatchState(fklPopPtrStack(matchStateStack));
				else if(!isBuiltInParenthese(topState->pattern))
				{
					const FklString* curPart=fklGetNthPartOfStringMatchPattern(topState->pattern,topState->index);
					if(curPart&&fklIsVar(curPart)&&!fklIsMustList(curPart))
						topState->index++;
					else if(curPart&&!fklIsVar(curPart))
					{
						while(!fklIsPtrStackEmpty(matchStateStack))
							freeMatchState(fklPopPtrStack(matchStateStack));
						return 2;
					}
					if(topState->index>=topState->pattern->num)
						freeMatchState(fklPopPtrStack(matchStateStack));
					else
						break;
				}
			}
			if(fklIsPtrStackEmpty(matchStateStack))
				break;
		}
		j+=skipSpaceAndCountLine(parts[i],j,sizes[i],line);
		if(pj)*pj=j;
		if(fklIsPtrStackEmpty(matchStateStack))
			break;
	}
	if(pi)*pi=i;
	done|=!fklIsPtrStackEmpty(matchStateStack);
	return done;
}

void fklPrintToken(FklPtrStack* tokenStack,FILE* fp)
{
	static const char* tokenTypeName[]=
	{
		"reserve_str",
		"char",
		"num",
		"byts",
		"string",
		"symbol",
		"comment",
	};
	for(uint32_t i=0;i<tokenStack->top;i++)
	{
		FklToken* token=tokenStack->base[i];
		fprintf(fp,"%d,%s:",token->line,tokenTypeName[token->type]);
		fklPrintString(token->value,fp);
		fputc('\n',fp);
	}
}

int fklIsAllComment(FklPtrStack* tokenStack)
{
	if(fklIsPtrStackEmpty(tokenStack))
		return 0;
	for(uint32_t i=0;i<tokenStack->top;i++)
	{
		FklToken* token=tokenStack->base[i];
		if(token->type!=FKL_TOKEN_COMMENT)
			return 0;
	}
	return 1;
}
