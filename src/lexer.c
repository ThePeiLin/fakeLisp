#include<fakeLisp/lexer.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/reader.h>
#include<fakeLisp/pattern.h>
#include<ctype.h>
#include<string.h>

//size_t skipString(const char* str)
//{
//	size_t i=1;
//	for(;str[i]!='\0'&&(str[i-1]=='\\'||str[i]!='\"');i++);
//	if(str[i]=='\"')
//		i++;
//	return i;
//}
//
//static size_t skipStringIndexSize(const char* str,size_t index,size_t size,char ch)
//{
//	size_t i=1;
//	for(;index+i<size&&(str[index+i-1]=='\\'||str[index+i]!=ch);i++);
//	if(index+i<size&&str[index+i]==ch)
//		i++;
//	return i;
//}
//
//size_t skipUntilSpace(const FklString* str)
//{
//	size_t i=0;
//	size_t size=str->size;
//	const char* buf=str->str;
//	for(;i<size&&!isspace(buf[i]);i++)
//	{
//		if(buf[i]=='\"')
//			i+=skipString(buf+i);
//		if(buf[i]=='\0')
//			break;
//	}
//	return i;
//}
//
inline static int isCompleteStringBuf(const char* buf,size_t size,char ch,size_t* len)
{
	int mark=0;
	int markN=0;
	size_t i=0;
	for(;i<size&&markN<2;i++)
		if(buf[i]==ch&&(!i||buf[i-1]!='\\'))
		{
			mark=~mark;
			markN+=1;
		}
	if(len)
		*len=mark?0:i;
	return !mark;
}

//inline static int isCompleteString(const FklString* str,char ch)
//{
//	return isCompleteStringBuf(str->str,str->size,ch,NULL);
//}


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
//	FklUintStack* sizeStack=fklCreateUintStack(32,16);
//	FklUintStack* indexStack=fklCreateUintStack(32,16);
//	*num=countPartThatSplitByBlank(str,indexStack,sizeStack);
//	char** retval=(char**)malloc(sizeof(char*)*(*num));
//	FKL_ASSERT(retval);
//	for(uint32_t i=0;i<indexStack->top;i++)
//	{
//		retval[i]=fklCopyMemory(str+indexStack->base[i],sizeStack->base[i]+1);
//		retval[i][sizeStack->base[i]]='\0';
//	}
//	fklDestroyUintStack(indexStack);
//	fklDestroyUintStack(sizeStack);
//	return retval;
//}

//typedef struct
//{
//	FklStringMatchPattern* pattern;
//	uint32_t index;
//}MatchState;

//static MatchState* createMatchState(FklStringMatchPattern* pattern,uint32_t index)
//{
//	MatchState* state=(MatchState*)malloc(sizeof(MatchState));
//	FKL_ASSERT(state);
//	state->pattern=pattern;
//	state->index=index;
//	return state;
//}
//
//static void destroyMatchState(MatchState* state)
//{
//	free(state);
//}

//#define BUILT_IN_SEPARATOR_STR_NUM (15)
//#define PARENTHESE_0 ((void*)0)
//#define PARENTHESE_1 ((void*)1)
//#define QUOTE ((void*)2)
//#define QSQUOTE ((void*)3)
//#define UNQUOTE ((void*)4)
//#define UNQTESP ((void*)5)
//#define DOTTED ((void*)6)
//#define VECTOR_0 ((void*)7)
//#define VECTOR_1 ((void*)8)
//#define BOX ((void*)9)
//#define INCOMPLETE_SYMBOL ((void*)10)
//#define BVECTOR_0 ((void*)11)
//#define BVECTOR_1 ((void*)12)
//#define HASH_EQ_0 ((void*)13)
//#define HASH_EQ_1 ((void*)14)
//#define HASH_EQV_0 ((void*)15)
//#define HASH_EQV_1 ((void*)16)
//#define HASH_EQUAL_0 ((void*)17)
//#define HASH_EQUAL_1 ((void*)18)
//
//static const char* separatorStrSet[]=
//{
//	"~@",
//	"#(",
//	"#[",
//	"#vu8(",
//	"#vu8[",
//	"#hash(",
//	"#hash[",
//	"#hasheqv(",
//	"#hasheqv[",
//	"#hashequal(",
//	"#hashequal[",
//	"(",
//	",",
//	"#\\",
//	"\"",
//	"[",
//	";",
//	"#!",
//	"'",
//	"`",
//	"~",
//	"#&",
//	"|",
//};
//
//static int isBuiltInSingleStrPattern(FklStringMatchPattern* pattern)
//{
//	return pattern==QUOTE
//		||pattern==QSQUOTE
//		||pattern==UNQUOTE
//		||pattern==UNQTESP
//		||pattern==DOTTED
//		||pattern==BOX
//		;
//}
//
//static int isBuiltInPattern(FklStringMatchPattern* pattern)
//{
//	return pattern==PARENTHESE_0
//		||pattern==PARENTHESE_1
//		||pattern==QUOTE
//		||pattern==QSQUOTE
//		||pattern==UNQUOTE
//		||pattern==UNQTESP
//		||pattern==DOTTED
//		||pattern==VECTOR_0
//		||pattern==VECTOR_1
//		||pattern==BVECTOR_0
//		||pattern==BVECTOR_1
//		||pattern==BOX
//		||pattern==HASH_EQ_0
//		||pattern==HASH_EQ_1
//		||pattern==HASH_EQV_0
//		||pattern==HASH_EQV_1
//		||pattern==HASH_EQUAL_0
//		||pattern==HASH_EQUAL_1;
//}
//
//static int isBuiltInParenthese(FklStringMatchPattern* pattern)
//{
//	return pattern==PARENTHESE_0
//		||pattern==PARENTHESE_1
//		||pattern==VECTOR_0
//		||pattern==VECTOR_1
//		||pattern==BVECTOR_0
//		||pattern==BVECTOR_1
//		||pattern==HASH_EQ_0
//		||pattern==HASH_EQ_1
//		||pattern==HASH_EQV_0
//		||pattern==HASH_EQV_1
//		||pattern==HASH_EQUAL_0
//		||pattern==HASH_EQUAL_1;
//}

//static MatchState* searchReverseStringCharMatchState(const char* part,size_t index,size_t size,FklPtrStack* stack)
//{
//	if(index<size)
//	{
//		MatchState* topState=fklTopPtrStack(stack);
//		for(uint32_t i=stack->top;topState
//				&&!isBuiltInParenthese(topState->pattern)
//				&&(isBuiltInSingleStrPattern(topState->pattern)
//					||topState->pattern==INCOMPLETE_SYMBOL
//					||(fklIsVar(fklGetNthPartOfStringMatchPattern(topState->pattern,topState->index))
//						&&(topState->index==topState->pattern->num-1
//							||fklIsVar(fklGetNthPartOfStringMatchPattern(topState->pattern,topState->index+1)))));
//				i--)
//			topState=i==0?NULL:stack->base[i-1];
//		if(topState&&!isBuiltInPattern(topState->pattern))
//		{
//			const FklString* nextPart=fklGetNthPartOfStringMatchPattern(topState->pattern,topState->index);
//			uint32_t i=0;
//			for(;;)
//			{
//				nextPart=fklGetNthPartOfStringMatchPattern(topState->pattern,topState->index+i);
//				if(!(nextPart&&fklIsVar(nextPart)&&fklIsMustList(nextPart)))
//					break;
//				i++;
//			}
//			if(size-index>=nextPart->size&&topState->pattern!=NULL&&nextPart&&!fklIsVar(nextPart)&&!memcmp(nextPart->str,part+index,nextPart->size))
//			{
//				topState->index+=i;
//				return topState;
//			}
//		}
//		FklStringMatchPattern* pattern=fklFindStringPatternBuf(part+index,size-index);
//		if(pattern)
//			return createMatchState(pattern,0);
//		else if(size-index>1&&part[index]=='#'&&part[index+1]=='(')
//			return createMatchState(VECTOR_0,0);
//		else if(size-index>1&&part[index]=='#'&&part[index+1]=='[')
//			return createMatchState(VECTOR_1,0);
//		else if(size-index>4&&!strncmp("#vu8(",part+index,5))
//			return createMatchState(BVECTOR_0,0);
//		else if(size-index>4&&!strncmp("#vu8[",part+index,5))
//			return createMatchState(BVECTOR_1,0);
//		else if(size-index>5&&!strncmp("#hash(",part+index,6))
//			return createMatchState(HASH_EQ_0,0);
//		else if(size-index>5&&!strncmp("#hash[",part+index,6))
//			return createMatchState(HASH_EQ_1,0);
//		else if(size-index>8&&!strncmp("#hasheqv(",part+index,9))
//			return createMatchState(HASH_EQV_0,0);
//		else if(size-index>8&&!strncmp("#hasheqv[",part+index,9))
//			return createMatchState(HASH_EQV_1,0);
//		else if(size-index>10&&!strncmp("#hashequal(",part+index,11))
//			return createMatchState(HASH_EQUAL_0,0);
//		else if(size-index>10&&!strncmp("#hashequal[",part+index,11))
//			return createMatchState(HASH_EQUAL_1,0);
//		else if(part[index]=='(')
//			return createMatchState(PARENTHESE_0,0);
//		else if(part[index]==')')
//		{
//			if(topState&&topState->pattern==PARENTHESE_0&&topState->index==0)
//				return createMatchState(PARENTHESE_0,1);
//			else if(topState&&topState->pattern==VECTOR_0&&topState->index==0)
//				return createMatchState(VECTOR_0,1);
//			else if(topState&&topState->pattern==BVECTOR_0&&topState->index==0)
//				return createMatchState(BVECTOR_0,1);
//			else if(topState&&topState->pattern==HASH_EQ_0&&topState->index==0)
//				return createMatchState(HASH_EQ_0,1);
//			else if(topState&&topState->pattern==HASH_EQV_0&&topState->index==0)
//				return createMatchState(HASH_EQV_0,1);
//			else if(topState&&topState->pattern==HASH_EQUAL_0&&topState->index==0)
//				return createMatchState(HASH_EQUAL_0,1);
//			else
//				return NULL;
//		}
//		else if(part[index]=='[')
//			return createMatchState(PARENTHESE_1,0);
//		else if(part[index]==']')
//		{
//			if(topState&&topState->pattern==PARENTHESE_1&&topState->index==0)
//				return createMatchState(PARENTHESE_1,1);
//			else if(topState&&topState->pattern==VECTOR_1&&topState->index==0)
//				return createMatchState(VECTOR_1,1);
//			else if(topState&&topState->pattern==BVECTOR_1&&topState->index==0)
//				return createMatchState(BVECTOR_1,1);
//			else if(topState&&topState->pattern==HASH_EQ_1&&topState->index==0)
//				return createMatchState(HASH_EQ_1,1);
//			else if(topState&&topState->pattern==HASH_EQV_1&&topState->index==0)
//				return createMatchState(HASH_EQV_1,1);
//			else if(topState&&topState->pattern==HASH_EQUAL_1&&topState->index==0)
//				return createMatchState(HASH_EQUAL_1,1);
//			else
//				return NULL;
//		}
//		switch(part[index])
//		{
//			case '\'':
//				return createMatchState(QUOTE,0);
//				break;
//			case '`':
//				return createMatchState(QSQUOTE,0);
//				break;
//			case '~':
//				if(size-index>1&&part[index+1]=='@')
//					return createMatchState(UNQTESP,0);
//				return createMatchState(UNQUOTE,0);
//				break;
//			case ',':
//				return createMatchState(DOTTED,0);
//				break;
//			case '#':
//				if(size-index>1&&part[index+1]=='&')
//					return createMatchState(BOX,0);
//				break;
//		}
//	}
//	return NULL;
//}

//static int hasReverseStringNext(MatchState* state)
//{
//	FklStringMatchPattern* pattern=state->pattern;
//	for(uint32_t j=0;state->index+j<pattern->num;j++)
//	{
//		if(!fklIsVar(fklGetNthPartOfStringMatchPattern(pattern,state->index+j)))
//			return 1;
//	}
//	return 0;
//}

//static int searchReverseStringChar(const char* part,size_t index,size_t size,FklPtrStack* stack)
//{
//	if(index<size)
//	{
//		MatchState* topState=fklTopPtrStack(stack);
//		for(uint32_t i=stack->top;topState
//				&&!isBuiltInParenthese(topState->pattern)
//				&&(isBuiltInSingleStrPattern(topState->pattern)
//					||topState->pattern==INCOMPLETE_SYMBOL
//					||!hasReverseStringNext(topState));
//				i--)
//			topState=i==0?NULL:stack->base[i-1];
//		if(topState&&!isBuiltInPattern(topState->pattern))
//		{
//			const FklString* nextPart=fklGetNthPartOfStringMatchPattern(topState->pattern,topState->index);
//			uint32_t i=0;
//			for(;nextPart&&fklIsVar(nextPart);i++)
//				nextPart=fklGetNthPartOfStringMatchPattern(topState->pattern,topState->index+i);
//			if(size-index>nextPart->size&&topState->pattern!=NULL&&nextPart&&!fklIsVar(nextPart)&&!memcmp(nextPart->str,part+index,nextPart->size))
//				return 1;
//		}
//		FklStringMatchPattern* pattern=fklFindStringPatternBuf(part+index,size-index);
//		if(pattern)
//			return 1;
//		else if((size-index>1&&part[index]=='#'&&part[index+1]=='(')
//				||(size-index>1&&part[index]=='#'&&part[index+1]=='[')
//				||(size-index>4&&!strncmp("#vu8(",part+index,5))
//				||(size-index>4&&!strncmp("#vu8[",part+index,5))
//				||(size-index>5&&!strncmp("#hash(",part+index,6))
//				||(size-index>5&&!strncmp("#hash[",part+index,6))
//				||(size-index>8&&!strncmp("#hasheqv(",part+index,9))
//				||(size-index>8&&!strncmp("#hasheqv[",part+index,9))
//				||(size-index>10&&!strncmp("#hashequal(",part+index,11))
//				||(size-index>10&&!strncmp("#hashequal[",part+index,11))
//				||(part[index]=='(')
//				||(part[index]=='[')
//				)
//			return 1;
//		else if(part[index]==')')
//		{
//			if(topState&&(topState->pattern==PARENTHESE_0
//						||topState->pattern==VECTOR_0
//						||topState->pattern==BVECTOR_0
//						||topState->pattern==HASH_EQ_0
//						||topState->pattern==HASH_EQV_0
//						||topState->pattern==HASH_EQUAL_0)
//					&&topState->index==0)
//				return 1;
//			else
//				return 0;
//		}
//		else if(part[index]==']')
//		{
//			if(topState&&(topState->pattern==PARENTHESE_1
//						||topState->pattern==VECTOR_1
//						||topState->pattern==BVECTOR_1
//						||topState->pattern==HASH_EQ_1
//						||topState->pattern==HASH_EQV_1
//						||topState->pattern==HASH_EQUAL_1)
//					&&topState->index==0)
//				return 1;
//			else
//				return 0;
//		}
//		switch(part[index])
//		{
//			case '\'':
//				return 1;
//				break;
//			case '`':
//				return 1;
//				break;
//			case '~':
//				if(size-index>1&&part[index+1]=='@')
//					return 1;
//				return 1;
//				break;
//			case ',':
//				return 1;
//				break;
//			case '#':
//				if(size-index>1&&part[index+1]=='&')
//					return 1;
//				break;
//		}
//	}
//	return 0;
//}

//static int isBuiltInReserveStr(const char* part,size_t size)
//{
//	int r=0;
//	for(uint32_t i=0;i<BUILT_IN_SEPARATOR_STR_NUM;i++)
//		if(size>=strlen(separatorStrSet[i])&&!strncmp(part,separatorStrSet[i],strlen(separatorStrSet[i])))
//		{
//			r=1;
//			break;
//		}
//	return r;
//}

//static size_t getSymbolLen(const char* part,size_t index,size_t size,FklPtrStack* stack)
//{
//	size_t i=0;
//	for(;index+i<size&&!isspace(part[index+i])&&!isBuiltInReserveStr(part+i+index,size-i-index);i++)
//	{
//		int state=searchReverseStringChar(part,index+i,size,stack);
//		if(state)
//			break;
//	}
//	return i;
//}
//
//static size_t skipSpaceAndCountLine(const char* str,size_t index,size_t size,size_t* cline)
//{
//	size_t j=0;
//	for(;index+j<size&&isspace(str[index+j]);j++)
//		if(str[index+j]=='\n')
//			(*cline)++;
//	return j;
//}
//
//static int isDivider(const char* buf,size_t i,size_t size,FklPtrStack* matchStateStack)
//{
//	return isspace(buf[i])
//			||searchReverseStringChar(buf,i,size,matchStateStack)
//			||isBuiltInReserveStr(buf+i,size);
//}

int fklSplitStringPartsIntoToken(const char** parts
		,size_t* sizes
		,uint32_t inum
		,size_t* line
		,FklPtrStack* retvalStack
		,FklPtrStack* matchStateStack
		,uint32_t* pi
		,uint32_t* pj)
{
	return 0;
}

//int fklSplitStringPartsIntoToken(const char** parts
//,size_t* sizes
//,uint32_t inum
//,uint32_t* line
//,FklPtrStack* retvalStack
//,FklPtrStack* matchStateStack
//,uint32_t* pi
//,uint32_t* pj)
//{
//	int done=0;
//	uint32_t i=0;
//	for(;i<inum;i++)
//	{
//		size_t j=0;
//		for(;;)
//		{
//			j+=skipSpaceAndCountLine(parts[i],j,sizes[i],line);
//			if(j>=sizes[i])
//				break;
//			MatchState* state=searchReverseStringCharMatchState(parts[i],j,sizes[i],matchStateStack);
//			if(state)
//			{
//				if(isBuiltInParenthese(state->pattern))
//				{
//					if(state->index==0)
//					{
//						const char* parenthese=state->pattern==PARENTHESE_0?"(":
//							state->pattern==PARENTHESE_1?"[":
//							state->pattern==VECTOR_0?"#(":
//							state->pattern==VECTOR_1?"#[":
//							state->pattern==BVECTOR_0?"#vu8(":
//							state->pattern==BVECTOR_1?"#vu8[":
//							state->pattern==HASH_EQ_0?"#hash(":
//							state->pattern==HASH_EQ_1?"#hash[":
//							state->pattern==HASH_EQV_0?"#hasheqv(":
//							state->pattern==HASH_EQV_1?"#hasheqv[":
//							state->pattern==HASH_EQUAL_0?"#hashequal(":
//							"#hashequal[";
//						fklPushPtrStack(fklCreateToken(FKL_TOKEN_RESERVE_STR,fklCreateStringFromCstr(parenthese),*line),retvalStack);
//						fklPushPtrStack(state,matchStateStack);
//						j+=strlen(parenthese);
//						continue;
//					}
//					else if(state->index==1)
//					{
//						const char* parenthese=(state->pattern==PARENTHESE_0
//								||state->pattern==VECTOR_0
//								||state->pattern==BVECTOR_0
//								||state->pattern==HASH_EQ_0
//								||state->pattern==HASH_EQV_0
//								||state->pattern==HASH_EQUAL_0)?")":"]";
//						MatchState* prevState=fklTopPtrStack(matchStateStack);
//						if(!isBuiltInParenthese(prevState->pattern))
//						{
//							destroyMatchState(state);
//							while(!fklIsPtrStackEmpty(matchStateStack))
//								destroyMatchState(fklPopPtrStack(matchStateStack));
//							return 2;
//						}
//						fklPushPtrStack(fklCreateToken(FKL_TOKEN_RESERVE_STR,fklCreateStringFromCstr(parenthese),*line),retvalStack);
//						destroyMatchState(prevState);
//						fklPopPtrStack(matchStateStack);
//						destroyMatchState(state);
//						j++;
//					}
//				}
//				else if(isBuiltInSingleStrPattern(state->pattern))
//				{
//					const char* str=state->pattern==QSQUOTE?
//						"`":
//						state->pattern==QUOTE?
//						"'":
//						state->pattern==UNQUOTE?
//						"~":
//						state->pattern==UNQTESP?
//						"~@":
//						state->pattern==DOTTED?
//						",":
//						state->pattern==BOX?
//						"#&":
//						NULL;
//					fklPushPtrStack(fklCreateToken(FKL_TOKEN_RESERVE_STR,fklCreateStringFromCstr(str),*line),retvalStack);
//					fklPushPtrStack(state,matchStateStack);
//					j+=strlen(str);
//					continue;
//				}
//				else if(state->index<state->pattern->num)
//				{
//					const FklString* curPart=fklGetNthPartOfStringMatchPattern(state->pattern,state->index);
//					MatchState* prevState=fklTopPtrStack(matchStateStack);
//					if(state->index!=0&&prevState->pattern!=state->pattern)
//					{
//						while(!fklIsPtrStackEmpty(matchStateStack))
//							destroyMatchState(fklPopPtrStack(matchStateStack));
//						return 2;
//					}
//					state->index++;
//					if(state->index-1==0)
//						fklPushPtrStack(state,matchStateStack);
//					fklPushPtrStack(fklCreateToken(FKL_TOKEN_RESERVE_STR,fklCopyString(curPart),*line),retvalStack);
//					j+=curPart->size;
//					if(state->index<state->pattern->num)
//						continue;
//				}
//			}
//			else if(parts[i][j]=='\"')
//			{
//				size_t sumLen=skipStringIndexSize(parts[i],j,sizes[i],'\"');
//				size_t lastLen=sumLen;
//				FklString* str=fklCreateString(sumLen,parts[i]+j);
//				int complete=isCompleteString(str,'"');
//				for(;!complete&&j+lastLen>=sizes[i];)
//				{
//					i++;
//					if(i>=inum)
//					{
//						i--;
//						break;
//					}
//					j=0;
//					fklStringCharBufCat(&str,parts[i],sizes[i]);
//					sumLen=skipUntilSpace(str);
//					complete=isCompleteString(str,'"');
//					lastLen=sumLen-lastLen;
//				}
//				done|=!complete;
//				if(complete)
//				{
//					fklPushPtrStack(fklCreateToken(FKL_TOKEN_STRING,fklCopyString(str),*line),retvalStack);
//					(*line)+=fklCountChar(str->str,'\n',str->size);
//					j+=lastLen;
//					free(str);
//				}
//				else
//				{
//					free(str);
//					break;
//				}
//			}
//			else if(parts[i][j]=='|')
//			{
//				size_t sumLen=skipStringIndexSize(parts[i],j,sizes[i],'|');
//				size_t lastLen=sumLen;
//				FklString* str=fklCreateString(sumLen,parts[i]+j);
//				int complete=isCompleteString(str,'|');
//				for(;!complete&&j+lastLen>=sizes[i];)
//				{
//					i++;
//					if(i>=inum)
//					{
//						i--;
//						break;
//					}
//					j=0;
//					fklStringCharBufCat(&str,parts[i],sizes[i]);
//					sumLen=skipUntilSpace(str);
//					complete=isCompleteString(str,'|');
//					lastLen=sumLen-lastLen;
//				}
//				done|=!complete;
//				if(complete)
//				{
//					size_t size=0;
//					char* s=fklCastEscapeCharBuf(str->str+1,'|',&size);
//					MatchState* topState=fklTopPtrStack(matchStateStack);
//					(*line)+=fklCountChar(str->str,'\n',str->size);
//					j+=lastLen;
//					free(str);
//					if(!topState||topState->pattern!=INCOMPLETE_SYMBOL)
//					{
//						fklPushPtrStack(fklCreateToken(FKL_TOKEN_SYMBOL,fklCreateString(size,s),*line),retvalStack);
//						free(s);
//						if(j<sizes[i]&&(parts[i][j]=='|'||!isDivider(parts[i],j,sizes[i],matchStateStack)))
//						{
//							fklPushPtrStack(createMatchState(INCOMPLETE_SYMBOL,0),matchStateStack);
//							continue;
//						}
//					}
//					else
//					{
//						FklToken* topToken=fklTopPtrStack(retvalStack);
//						fklStringCharBufCat(&topToken->value,s,size);
//						free(s);
//						fklPopPtrStack(matchStateStack);
//						if(j<sizes[i]&&(parts[i][j]=='|'||!isDivider(parts[i],j,sizes[i],matchStateStack)))
//						{
//							fklPushPtrStack(topState,matchStateStack);
//							continue;
//						}
//						else
//							destroyMatchState(topState);
//					}
//				}
//				else
//				{
//					free(str);
//					break;
//				}
//			}
//			else if(sizes[i]-j>1&&!strncmp(parts[i]+j,"#\\",strlen("#\\")))
//			{
//				size_t len=getSymbolLen(parts[i],j+3,sizes[i],matchStateStack)+3;
//				fklPushPtrStack(fklCreateToken(FKL_TOKEN_CHAR,fklCreateString(len,parts[i]+j),*line),retvalStack);
//				j+=len;
//			}
//			else if(parts[i][j]==';'||(sizes[i]-j>1&&!strncmp(parts[i]+j,"#!",strlen("#!"))))
//			{
//				size_t len=0;
//				for(;j+len<sizes[i]&&parts[i][j+len]!='\n';len++);
//				fklPushPtrStack(fklCreateToken(FKL_TOKEN_COMMENT,fklCreateString(len,parts[i]+j),*line),retvalStack);
//				j+=len;
//				continue;
//			}
//			else
//			{
//				size_t len=getSymbolLen(parts[i],j,sizes[i],matchStateStack);
//				if(len)
//				{
//					MatchState* topState=fklTopPtrStack(matchStateStack);
//					if(!topState||topState->pattern!=INCOMPLETE_SYMBOL)
//					{
//						FklString* symbol=fklCreateString(len,parts[i]+j);
//						j+=len;
//						FklToken* sym=fklCreateToken(FKL_TOKEN_SYMBOL,symbol,*line);
//						fklPushPtrStack(sym,retvalStack);
//						if(j<sizes[i]&&parts[i][j]=='|')
//						{
//							fklPushPtrStack(createMatchState(INCOMPLETE_SYMBOL,0),matchStateStack);
//							continue;
//						}
//						else if(fklIsNumberString(symbol))
//							sym->type=FKL_TOKEN_NUM;
//					}
//					else
//					{
//						FklToken* topToken=fklTopPtrStack(retvalStack);
//						fklStringCharBufCat(&topToken->value,parts[i]+j,len);
//						j+=len;
//						if(topState&&j<sizes[i]&&parts[i][j]!='|')
//							destroyMatchState(fklPopPtrStack(matchStateStack));
//						else
//							continue;
//					}
//				}
//				else
//				{
//					while(!fklIsPtrStackEmpty(matchStateStack))
//						destroyMatchState(fklPopPtrStack(matchStateStack));
//					return 2;
//				}
//			}
//			for(MatchState* topState=fklTopPtrStack(matchStateStack);
//					topState
//					&&(isBuiltInSingleStrPattern(topState->pattern)||!isBuiltInParenthese(topState->pattern));
//					topState=fklTopPtrStack(matchStateStack))
//			{
//				if(isBuiltInSingleStrPattern(topState->pattern))
//					destroyMatchState(fklPopPtrStack(matchStateStack));
//				else if(!isBuiltInParenthese(topState->pattern))
//				{
//					const FklString* curPart=fklGetNthPartOfStringMatchPattern(topState->pattern,topState->index);
//					if(curPart&&fklIsVar(curPart)&&!fklIsMustList(curPart))
//						topState->index++;
//					else if(curPart&&!fklIsVar(curPart))
//					{
//						while(!fklIsPtrStackEmpty(matchStateStack))
//							destroyMatchState(fklPopPtrStack(matchStateStack));
//						return 2;
//					}
//					if(topState->index>=topState->pattern->num)
//						destroyMatchState(fklPopPtrStack(matchStateStack));
//					else
//						break;
//				}
//			}
//			if(fklIsPtrStackEmpty(matchStateStack))
//				break;
//		}
//		j+=skipSpaceAndCountLine(parts[i],j,sizes[i],line);
//		if(pj)*pj=j;
//		if(fklIsPtrStackEmpty(matchStateStack))
//			break;
//	}
//	if(pi)*pi=i;
//	done|=!fklIsPtrStackEmpty(matchStateStack);
//	return done;
//}

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

inline static int charbufEqual(const char* buf,size_t n,const char* s,size_t l)
{
	return n>=l&&!memcmp(s,buf,l);
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

static int matchStrInStrs(const char* buf
		,size_t n
		,FklStringMatchState* strs)
{
	for(;strs;strs=strs->next)
	{
		FklString* s=getPartFromState(strs,strs->index)->u.str;
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

#define DEFAULT_TOKEN_HEADER_STR       (0)
#define DEFAULT_TOKEN_HEADER_SYM       (1)
#define DEFAULT_TOKEN_HEADER_COMMENT_0 (2)
#define DEFAULT_TOKEN_HEADER_COMMENT_1 (3)
#define DEFAULT_TOKEN_HEADER_CHR       (4)

static const struct DefaultTokenHeader
{
	size_t l;
	const char* s;
}
DefaultTokenHeaders[]=
{
	{1,"\"",},
	{1,"|",},
	{1,";",},
	{2,"#!",},
	{2,"#\\",},
	{0,NULL,},
};

inline static int isComment(const char* buf,size_t n)
{
	return charbufEqual(buf,n
			,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_COMMENT_0].s
			,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_COMMENT_0].l)
		||charbufEqual(buf,n
				,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_COMMENT_1].s
				,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_COMMENT_1].l);
}

inline static int matchStrInDefaultHeaders(const char* buf,size_t n)
{
	for(const struct DefaultTokenHeader* cur=&DefaultTokenHeaders[0];cur->l;cur++)
	{
		size_t l=cur->l;
		const char* s=cur->s;
		if(charbufEqual(buf,n,s,l))
			return 1;
	}
	return 0;
}

static size_t getDefaultSymbolLen(const char* buf
		,size_t n
		,FklStringMatchState* strs
		,FklStringMatchState* boxes
		,FklStringMatchPattern* patterns)
{
	size_t i=0;
	for(;i<n
			&&!isspace(buf[i])
			&&!matchStrInStrs(&buf[i],n-i,strs)
			&&!matchStrInBoxes(&buf[i],n-i,boxes)
			&&!matchStrInPatterns(&buf[i],n-i,patterns)
			&&!matchStrInDefaultHeaders(&buf[i],n-i)
			;i++);
	return i;
}

static FklToken* createDefaultToken(const char* buf
		,size_t n
		,FklStringMatchState* strs
		,FklStringMatchState* boxes
		,FklStringMatchPattern* patterns
		,size_t line
		,size_t* jInc)
{
	if(charbufEqual(buf,n
				,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_CHR].s
				,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_CHR].l))
	{
		size_t len=getDefaultSymbolLen(&buf[3],n-3,strs,boxes,patterns)+3;
		*jInc=len;
		return fklCreateToken(FKL_TOKEN_CHAR,fklCreateString(len,buf),line);
	}
	else if(charbufEqual(buf,n
				,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_STR].s
				,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_STR].l))
	{
		size_t len=0;
		int complete=isCompleteStringBuf(buf,n,'"',&len);
		if(complete)
		{
			*jInc=len;
			size_t size=0;
			char* s=fklCastEscapeCharBuf(&buf[1],'"',&size);
			FklString* str=fklCreateString(size,s);
			free(s);
			return fklCreateToken(FKL_TOKEN_STRING,str,line);
		}
		else
			return fklCreateToken(FKL_TOKEN_STRING,NULL,line);
	}
	else
	{
		int shouldBeSymbol=0;
		size_t i=0;
		FklUintStack lenStack={NULL,0,0,0,};
		fklInitUintStack(&lenStack,2,4);
		while(i<n)
		{
			size_t len=0;
			if(charbufEqual(&buf[i],n-i
						,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_SYM].s
						,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_SYM].l))
			{
				shouldBeSymbol=1;
				int complete=isCompleteStringBuf(&buf[i],n-i,'|',&len);
				if(!complete)
				{
					fklUninitUintStack(&lenStack);
					return fklCreateToken(FKL_TOKEN_SYMBOL,NULL,line);
				}
			}
			else
				len=getDefaultSymbolLen(&buf[i],n-i,strs,boxes,patterns);
			if(!len)
				break;
			i+=len;
			fklPushUintStack(len,&lenStack);
		}
		FklToken* t=NULL;
		if(i)
		{
			i=0;
			FklString* str=fklCreateEmptyString();
			while(!fklIsUintStackEmpty(&lenStack))
			{
				uint64_t len=fklPopUintStack(&lenStack);
				if(charbufEqual(&buf[i],n-i
							,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_SYM].s
							,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_SYM].l))
				{
					size_t size=0;
					char* s=fklCastEscapeCharBuf(&buf[i+1],'|',&size);
					fklStringCharBufCat(&str,s,size);
					free(s);
				}
				else
					fklStringCharBufCat(&str,&buf[i],len);
				i+=len;
			}
			*jInc=i;
			t=fklCreateToken(FKL_TOKEN_SYMBOL,str,line);
			if(!shouldBeSymbol&&fklIsNumberString(t->value))
				t->type=FKL_TOKEN_NUM;
		}
		fklUninitUintStack(&lenStack);
		return t;
	}
}

static int updateStrState(const char* buf
		,size_t n
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
			r=1;
			if(cur->index<getSizeFromPattern(cur->pattern))
			{
				*pstate=cur->next;
				addStateIntoSet(cur,&prev->str,&prev->box,&prev->sym);
				continue;
			}
		}
		pstate=&(*pstate)->next;
	}
	return r;
}

static int updateBoxState(const char* buf
		,size_t n
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
			if(cur->index<getSizeFromPattern(cur->pattern))
			{
				*pstate=cur->next;
				addStateIntoSet(cur,&prev->str,&prev->box,&prev->sym);
				continue;
			}
		}
		pstate=&(*pstate)->next;
	}
	if(!r)
		prev->box=boxes;
	return r;
}

static void updateSymState(const char* buf
		,size_t n
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
}

static FklStringMatchState* getRollBack(FklStringMatchState** pcur,FklStringMatchState* prev)
{
	FklStringMatchState* r=NULL;
	for(;*pcur!=prev;)
	{
		FklStringMatchState* cur=*pcur;
		*pcur=cur->next;
		cur->next=r;
		cur->index--;
		r=cur;
	}
	return r;
}

static void rollBack(FklStringMatchState* strs
		,FklStringMatchState* boxes
		,FklStringMatchState* syms
		,FklStringMatchSet* oset)
{
	if(syms)
	{
		FklStringMatchState* ostrs=getRollBack(&oset->str,strs);
		FklStringMatchState* oboxes=getRollBack(&oset->box,boxes);
		for(FklStringMatchState* cur=oset->sym;cur;cur=cur->next)
			cur->index--;
		for(FklStringMatchState* cur=ostrs;cur;)
		{
			FklStringMatchState* t=cur->next;
			addStateIntoSet(cur,&oset->str,&oset->box,&oset->sym);
			cur=t;
		}
		for(FklStringMatchState* cur=oboxes;cur;)
		{
			FklStringMatchState* t=cur->next;
			addStateIntoSet(cur,&oset->str,&oset->box,&oset->sym);
			cur=t;
		}
	}
	else
		oset->str=strs;
}

static FklStringMatchSet* updatePreviusSet(FklStringMatchSet* set
		,const char* buf
		,size_t n
		,FklStringMatchPattern* patterns
		,FklToken** pt
		,size_t line
		,size_t* jInc)
{
	if(isComment(buf,n))
	{
		size_t len=0;
		for(;len<n&&buf[len]!='\n';len++);
		*pt=fklCreateToken(FKL_TOKEN_COMMENT,fklCreateString(len,buf),line);
		*jInc=len;
		return set;
	}
	FklStringMatchSet* r=NULL;
	if(set==FKL_STRING_PATTERN_UNIVERSAL_SET)
	{
		r=matchInUniversSet(buf,n,patterns,pt,line,NULL);
		if(!*pt)
			*pt=createDefaultToken(buf,n
					,NULL,NULL,patterns,line
					,jInc);
		else
			*jInc=(*pt)->value->size;
	}
	else
	{
		FklStringMatchState* strs=set->str;
		FklStringMatchState* boxes=set->box;
		FklStringMatchState* syms=set->sym;
		set->sym=NULL;
		set->box=NULL;
		set->str=NULL;
		int b=!updateStrState(buf,n,strs,set,pt,line)
			&&!updateBoxState(buf,n,boxes,set,patterns,pt,line);
		if(b)
			updateSymState(buf,n,syms,set,patterns,pt,line);
		FklStringMatchSet* oset=set;
		if(set->str==NULL&&set->box==NULL&&set->sym==NULL)
			set=set->prev;
		FklStringMatchSet* nset=NULL;
		if(b)
		{
			nset=matchInUniversSet(buf,n
					,patterns
					,pt
					,line
					,set);
			if(!*pt&&set)
				*pt=createDefaultToken(buf,n
						,set->str,set->box
						,patterns,line,jInc);
			else
				*jInc=(*pt)->value->size;
			if(!*pt||(*pt)->value==NULL)
			{
				rollBack(strs,boxes,syms,oset);
				set=oset;
			}
		}
		else
			*jInc=(*pt)->value->size;
		r=(nset==NULL)?set:nset;
	}
	return r;
}

FklStringMatchSet* fklSplitStringIntoTokenWithPattern(const char* buf
		,size_t size
		,size_t* line
		,size_t* pj
		,FklPtrStack* retvalStack
		,FklStringMatchSet* matchSet
		,FklStringMatchPattern* patterns)
{
	size_t j=*pj;
	while(j<size&&matchSet)
	{
		FklToken* token=NULL;
		size_t jInc=0;
		matchSet=updatePreviusSet(matchSet
				,&buf[j]
				,size-j
				,patterns
				,&token
				,*line
				,&jInc);
		if(token)
		{
			if(token->value==NULL)
			{
				free(token);
				*pj=j;
				break;
			}
			fklPushPtrStack(token,retvalStack);
			j+=jInc;
			line+=fklCountCharInString(token->value,'\n');
		}
		else
		{
			j++;
			if(buf[j]=='\n')
				line++;
		}
	}
	*pj=j;
	return matchSet;
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
		"reserve-str",
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
