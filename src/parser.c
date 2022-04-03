#include<fakeLisp/parser.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/reader.h>
#include<ctype.h>
#include<string.h>

static size_t skipSpace(const char* str)
{
	size_t i=0;
	for(;str[i]!='\0'&&isspace(str[i]);i++);
	return i;
}


size_t skipString(const char* str)
{
	size_t i=1;
	for(;str[i]!='\0'&&(str[i-1]=='\\'||str[i]!='\"');i++);
	if(str[i]=='\"')
		i++;
	return i;
}

size_t skipUntilSpace(const char* str)
{
	size_t i=0;
	for(;str[i]!='\0'&&!isspace(str[i]);i++)
	{
		if(str[i]=='\"')
			i+=skipString(str+i);
		if(str[i]=='\0')
			break;
	}
	return i;
}

int isCompleteString(const char* str)
{
	int mark=0;
	int markN=0;
	for(size_t i=0;str[i]!='\0'&&markN<2;i++)
		if(str[i]=='\"'&&(!i||str[i-1]!='\\'))
		{
			mark=~mark;
			markN+=1;
		}
	return !mark;
}

uint32_t countPartThatSplitByBlank(const char* str,FklUintStack* indexStack,FklUintStack* sizeStack)
{
	uint32_t retval=0;
	for(size_t i=0;str[i]!='\0';)
	{
		fklPushUintStack(i,indexStack);
		size_t size=skipUntilSpace(str+i);
		i+=size;
		fklPushUintStack(size,sizeStack);
		i+=skipSpace(str+i);
		retval++;
	}
	return retval;
}

char** spiltStringByBlank(const char* str,uint32_t* num)
{
	FKL_ASSERT(num,"spiltStringByBlank",__FILE__,__LINE__);
	FklUintStack* sizeStack=fklNewUintStack(32,16);
	FklUintStack* indexStack=fklNewUintStack(32,16);
	*num=countPartThatSplitByBlank(str,indexStack,sizeStack);
	char** retval=(char**)malloc(sizeof(char*)*(*num));
	FKL_ASSERT(retval,"spiltStringByBlank",__FILE__,__LINE__);
	for(uint32_t i=0;i<indexStack->top;i++)
	{
		retval[i]=fklCopyMemory(str+indexStack->base[i],sizeStack->base[i]+1);
		retval[i][sizeStack->base[i]]='\0';
	}
	fklFreeUintStack(indexStack);
	fklFreeUintStack(sizeStack);
	return retval;
}

FklToken* fklNewToken(FklTokenType type,const char* str,size_t len,uint32_t line)
{
	FklToken* token=(FklToken*)malloc(sizeof(FklToken));
	FKL_ASSERT(token,"fklNewToken",__FILE__,__LINE__);
	token->type=type;
	token->line=line;
	token->value=fklCopyMemory(str,len+1);
	token->value[len]='\0';
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
	FKL_ASSERT(state,"newMatchState",__FILE__,__LINE__);
	state->pattern=pattern;
	state->index=index;
	return state;
}

static void freeMatchState(MatchState* state)
{
	free(state);
}

#define BUILT_IN_SEPARATOR_STR_NUM (11)
#define PARENTHESE_0 ((void*)0)
#define PARENTHESE_1 ((void*)1)
#define QUOTE ((void*)2)
#define QSQUOTE ((void*)3)
#define UNQUOTE ((void*)4)
#define UNQTESP ((void*)5)
#define DOTTED ((void*)6)

static const char* separatorStrSet[]=
{
	"(",",","#\\","#b","\"","[",";","'","`","~","~@"
};

static int isBuiltInSingleStrPattern(FklStringMatchPattern* pattern)
{
	return pattern==QUOTE
		||pattern==QSQUOTE
		||pattern==UNQUOTE
		||pattern==UNQTESP
		||pattern==DOTTED
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
		;
}

static int isBuiltInParenthese(FklStringMatchPattern* pattern)
{
	return pattern==PARENTHESE_0
		||pattern==PARENTHESE_1;
}

static MatchState* searchReverseStringCharMatchState(const char* part,FklPtrStack* stack)
{
	MatchState* topState=fklTopPtrStack(stack);
	for(uint32_t i=stack->top;topState
			&&!isBuiltInParenthese(topState->pattern)
			&&(isBuiltInSingleStrPattern(topState->pattern)
				||(fklIsVar(fklGetNthPartOfStringMatchPattern(topState->pattern,topState->index))
					&&(topState->index==topState->pattern->num-1
						||fklIsVar(fklGetNthPartOfStringMatchPattern(topState->pattern,topState->index+1)))));
			i--)
		topState=i==0?NULL:stack->base[i-1];
	if(topState&&!isBuiltInPattern(topState->pattern))
	{
		char* nextPart=fklGetNthPartOfStringMatchPattern(topState->pattern,topState->index);
		uint32_t i=0;
		for(;;)
		{
			nextPart=fklGetNthPartOfStringMatchPattern(topState->pattern,topState->index+i);
			if(!(nextPart&&fklIsVar(nextPart)&&fklIsMustList(nextPart)))
				break;
			i++;
		}
		//char* nextPart=fklGetNthPartOfStringMatchPattern(topState->pattern,topState->index+1);
		if(topState->pattern!=NULL&&nextPart&&!fklIsVar(nextPart)&&!strncmp(nextPart,part,strlen(nextPart)))
		{
			topState->index+=i;
			return topState;
			//return newMatchState(topState->pattern,topState->index+i);
		}
	}
	FklStringMatchPattern* pattern=fklFindStringPattern(part);
	if(pattern)
		return newMatchState(pattern,0);
	for(size_t i=0;i<2;i++)
		if(part[0]=='(')
			return newMatchState(PARENTHESE_0,0);
		else if(part[0]==')')
		{
			if(topState&&topState->pattern==PARENTHESE_0&&topState->index==0)
				return newMatchState(PARENTHESE_0,1);
			else
				return NULL;
		}
	for(size_t i=0;i<2;i++)
		if(part[0]=='[')
			return newMatchState(PARENTHESE_1,0);
		else if(part[0]==']')
		{
			if(topState&&topState->pattern==PARENTHESE_1&&topState->index==0)
				return newMatchState(PARENTHESE_1,1);
			else
				return NULL;
		}
	switch(part[0])
	{
		case '\'':
			return newMatchState(QUOTE,0);
			break;
		case '`':
			return newMatchState(QSQUOTE,0);
			break;
		case '~':
			if(part[1]!='@')
				return newMatchState(UNQUOTE,0);
			return newMatchState(UNQTESP,0);
			break;
		case ',':
			return newMatchState(DOTTED,0);
			break;
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

static const char* searchReverseStringChar(const char* part,FklPtrStack* stack)
{
	MatchState* topState=fklTopPtrStack(stack);
	for(uint32_t i=stack->top;topState
			&&!isBuiltInParenthese(topState->pattern)
			&&(isBuiltInSingleStrPattern(topState->pattern)
				||!hasReverseStringNext(topState));
			i--)
		topState=i==0?NULL:stack->base[i-1];
	if(topState&&!isBuiltInPattern(topState->pattern))
	{
		char* nextPart=fklGetNthPartOfStringMatchPattern(topState->pattern,topState->index);
		uint32_t i=0;
		for(;nextPart&&fklIsVar(nextPart);i++)
			nextPart=fklGetNthPartOfStringMatchPattern(topState->pattern,topState->index+i);
		//char* nextPart=fklGetNthPartOfStringMatchPattern(topState->pattern,topState->index+1);
		if(topState->pattern!=NULL&&nextPart&&!fklIsVar(nextPart)&&!strncmp(nextPart,part,strlen(nextPart)))
			return nextPart;
	}
	FklStringMatchPattern* pattern=fklFindStringPattern(part);
	if(pattern)
		return pattern->parts[0];
	for(size_t i=0;i<2;i++)
		if(part[0]=='(')
			return ")";
		else if(part[0]==')')
		{
			if(topState&&topState->pattern==PARENTHESE_0&&topState->index==0)
				return "(";
			else
				return NULL;
		}
	for(size_t i=0;i<2;i++)
		if(part[0]=='[')
			return "[";
		else if(part[0]==']')
		{
			if(topState&&topState->pattern==PARENTHESE_1&&topState->index==0)
				return "]";
			else
				return NULL;
		}
	switch(part[0])
	{
		case '\'':
			return "\'";
			break;
		case '`':
			return "`";
			break;
		case '~':
			if(part[1]!='@')
				return "~@";
			return "~";
			break;
		case ',':
			return ",";
			break;
	}
	return NULL;
}

static int isBuiltInReserveStr(const char* part)
{
	for(uint32_t i=0;i<BUILT_IN_SEPARATOR_STR_NUM;i++)
		if(!strncmp(part,separatorStrSet[i],strlen(separatorStrSet[i])))
			return 1;
	return 0;
}

static size_t getSymbolLen(const char* part,FklPtrStack* stack)
{
	size_t i=0;
	for(;part[i]!='\0'&&!isspace(part[i])&&!isBuiltInReserveStr(part+i);i++)
	{
		const char* state=searchReverseStringChar(part+i,stack);
		if(state)
			break;
	}
	return i;
}

static uint32_t skipSpaceAndCountLine(const char* str,uint32_t* cline)
{
	uint32_t j=0;
	for(;isspace(str[j]);j++)
		if(str[j]=='\n')
			(*cline)++;
	return j;
}

int fklSplitStringPartsIntoToken(char** parts,uint32_t inum,uint32_t line,FklPtrStack* retvalStack,uint32_t* pi,uint32_t* pj)
{
	int done=0;
	FklPtrStack* matchStateStack=fklNewPtrStack(32,16);
	uint32_t i=0;
	for(;i<inum;i++)
	{
		size_t j=0;
		for(;;)
		{
			j+=skipSpaceAndCountLine(parts[i]+j,&line);
			if(parts[i][j]=='\0')
				break;
			MatchState* state=searchReverseStringCharMatchState(parts[i]+j,matchStateStack);
			if(state)
			{
				if(isBuiltInParenthese(state->pattern))
				{
					if(state->index==0)
					{
						const char* parenthese=state->pattern==(void*)0?"(":"[";
						fklPushPtrStack(fklNewToken(FKL_TOKEN_RESERVE_STR,parenthese,strlen(parenthese),line),retvalStack);
						fklPushPtrStack(state,matchStateStack);
						j++;
						continue;
					}
					else if(state->index==1)
					{
						const char* parenthese=state->pattern==(void*)0?")":"]";
						MatchState* prevState=fklTopPtrStack(matchStateStack);
						fklPushPtrStack(fklNewToken(FKL_TOKEN_RESERVE_STR,parenthese,strlen(parenthese),line),retvalStack);
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
						NULL;
					fklPushPtrStack(fklNewToken(FKL_TOKEN_RESERVE_STR,str,strlen(str),line),retvalStack);
					fklPushPtrStack(state,matchStateStack);
					j+=strlen(str);
					continue;
				}
				else if(state->index<state->pattern->num)
				{
					char* curPart=fklGetNthPartOfStringMatchPattern(state->pattern,state->index);
					size_t len=strlen(curPart);
					state->index++;
					if(state->index-1==0)
						fklPushPtrStack(state,matchStateStack);
					fklPushPtrStack(fklNewToken(FKL_TOKEN_RESERVE_STR,curPart,len,line),retvalStack);
					j+=len;
					if(state->index<state->pattern->num)
						continue;
				}
			}
			else if(parts[i][j]=='\"')
			{
				size_t sumLen=skipString(parts[i]+j);
				size_t lastLen=sumLen;
				char* str=fklCopyMemory(parts[i]+j,sumLen+1);
				str[sumLen]='\0';
				int complete=isCompleteString(str);
				for(;!complete&&parts[i][j+lastLen]=='\0';)
				{
					i++;
					if(i>=inum)
					{
						i--;
						break;
					}
					j=0;
					str=fklStrCat(str,parts[i]);
					sumLen=skipUntilSpace(str);
					complete=isCompleteString(str);
					lastLen=sumLen-lastLen;
				}
				str[sumLen]='\0';
				if(complete)
					fklPushPtrStack(fklNewToken(FKL_TOKEN_STRING,str,sumLen,line),retvalStack);
				line+=fklCountChar(str,'\n',-1);
				free(str);
				j+=lastLen;
				done|=!complete;
			}
			else if(!strncmp(parts[i]+j,"#\\",strlen("#\\")))
			{
				//size_t len=getSymbolLen(parts[i]+j+2,matchStateStack);
				char* symbol=fklGetStringAfterBackslash(parts[i]+j+2);
				size_t len=strlen(symbol);
				free(symbol);
				symbol=fklCopyMemory(parts[i]+j,len+1+2);
				symbol[len+2]='\0';
				fklPushPtrStack(fklNewToken(FKL_TOKEN_CHAR,symbol,len+2,line),retvalStack);
				j+=len+2;
				//j+=strlen(symbol)+2;
			}
			else if(!strncmp(parts[i]+j,"#b",strlen("#b")))
			{
				size_t len=getSymbolLen(parts[i]+j+2,matchStateStack);
				char* symbol=fklCopyMemory(parts[i]+j,len+1+2);
				symbol[len+2]='\0';
				fklPushPtrStack(fklNewToken(FKL_TOKEN_BYTS,symbol,len+2,line),retvalStack);
				j+=len+2;
			}
			else if(parts[i][j]==';')
			{
				uint32_t len=0;
				for(;parts[i][j+len]!='\0'&&parts[i][j+len]!='\n';len++);
				char* str=fklCopyMemory(parts[i]+j,len+1);
				str[len]='\0';
				fklPushPtrStack(fklNewToken(FKL_TOKEN_COMMENT,str,len,line),retvalStack);
				j+=len;
				free(str);
				continue;
			}
			else
			{
				size_t len=getSymbolLen(parts[i]+j,matchStateStack);
				if(len)
				{
					char* symbol=fklCopyMemory(parts[i]+j,len+1);
					symbol[len]='\0';
					FklTokenType type=FKL_TOKEN_SYMBOL;
					if(fklIsNum(symbol))
						type=FKL_TOKEN_NUM;
					fklPushPtrStack(fklNewToken(type,symbol,len,line),retvalStack);
					j+=len;
					free(symbol);
				}
				else
				{
					while(!fklIsPtrStackEmpty(matchStateStack))
						freeMatchState(fklPopPtrStack(matchStateStack));
					fklFreePtrStack(matchStateStack);
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
					char* curPart=fklGetNthPartOfStringMatchPattern(topState->pattern,topState->index);
					if(curPart&&fklIsVar(curPart)&&!fklIsMustList(curPart))
						topState->index++;
					if(topState->index>=topState->pattern->num)
						freeMatchState(fklPopPtrStack(matchStateStack));
					else
						break;
				}
			}
			if(fklIsPtrStackEmpty(matchStateStack))
				break;
		}
		j+=skipSpaceAndCountLine(parts[i]+j,&line);
		if(pj)*pj=j;
		if(fklIsPtrStackEmpty(matchStateStack))
			break;
	}
	if(pi)*pi=i;
	done|=!fklIsPtrStackEmpty(matchStateStack);
	while(!fklIsPtrStackEmpty(matchStateStack))
		freeMatchState(fklPopPtrStack(matchStateStack));
	fklFreePtrStack(matchStateStack);
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
		fprintf(fp,"%d,%s:%s\n",token->line,tokenTypeName[token->type],token->value);
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
