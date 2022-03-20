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
	return i;
}

size_t skipUntilSpace(const char* str)
{
	size_t i=0;
	for(;str[i]!='\0'&&!isspace(str[i]);i++)
	{
		if(str[i]=='\"')
			i+=skipString(str+i);
		if(str[i]=='\\'&&str[i+1]!='\0')
			i++;
	}
	return i;
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
FklToken* newToken(FklTokenType type,const char* str,size_t len)
{
	FklToken* token=(FklToken*)malloc(sizeof(FklToken));
	FKL_ASSERT(token,"newToken",__FILE__,__LINE__);
	token->type=type;
	token->value=fklCopyMemory(str,len+1);
	token->value[len]='\0';
	return token;
}

typedef struct
{
	FklStringMatchPattern* pattern;
	uint32_t index;
}MatchState;

MatchState* newMatchState(FklStringMatchPattern* pattern,uint32_t index)
{
	MatchState* state=(MatchState*)malloc(sizeof(MatchState));
	FKL_ASSERT(state,"newMatchState",__FILE__,__LINE__);
	state->pattern=pattern;
	state->index=index;
	return state;
}

#define BUILT_RESERVE_STR_NUM (6)

static const char* reserveStrSet[]=
{
	"(",")",",","#\\","#b","\""
};

MatchState* searchReverseStringChar(const char* part,FklPtrStack* stack)
{
	for(size_t i=0;i<2;i++)
		if(part[0]=='(')
			return newMatchState(NULL,0);
		else if(part[0]==')')
			return newMatchState(NULL,1);
	FklStringMatchPattern* pattern=fklFindStringPattern(part);
	if(pattern)
		return newMatchState(pattern,0);
	MatchState* topState=fklTopPtrStack(stack);
	if(topState&&topState->pattern)
	{
		char* nextReverseChar=fklGetNthReverseCharOfStringMatchPattern(topState->pattern,topState->index+1);
		if(topState->pattern!=NULL&&nextReverseChar&&!strncmp(nextReverseChar,part,strlen(nextReverseChar)))
			return newMatchState(topState->pattern,topState->index+1);
	}
	return NULL;
}

int isBuiltInReserveStr(const char* part)
{
	for(uint32_t i=0;i<BUILT_RESERVE_STR_NUM;i++)
		if(!strncmp(part,reserveStrSet[i],strlen(reserveStrSet[i])))
			return 1;
	return 0;
}

size_t getSymbolLen(const char* part,FklPtrStack* stack)
{
	size_t i=0;
	for(;part[i]!='\0'&&!isBuiltInReserveStr(part+i)&&!searchReverseStringChar(part+i,stack);i+=(part[i]=='\\')?2:1);
	return i;
}

FklPtrStack* spiltStringPartsIntoToken(char** parts,uint32_t inum)
{
	FklPtrStack* retvalStack=fklNewPtrStack(32,16);
	FklPtrStack* matchStateStack=fklNewPtrStack(32,16);
	for(uint32_t i=0;i<inum;i++)
	{
		for(size_t j=0;parts[i][j]!='\0';)
		{
			MatchState* state=searchReverseStringChar(parts[i]+j,matchStateStack);
			if(state)
			{
				if(state->pattern==NULL&&state->index==0)
				{
					fklPushPtrStack(newToken(FKL_TOKEN_RESERVE_STR,"(",strlen("(")),retvalStack);
					fklPushPtrStack(state,matchStateStack);
					j++;
				}
				else if(state->pattern==NULL&&state->index==1)
				{
					fklPushPtrStack(newToken(FKL_TOKEN_RESERVE_STR,")",strlen(")")),retvalStack);
					free(fklPopPtrStack(matchStateStack));
					j++;
				}
				else if(state->index<state->pattern->reserveCharNum-1)
				{
					if(state->index)
						free(fklPopPtrStack(matchStateStack));
					char* reserveStr=fklGetNthReverseCharOfStringMatchPattern(state->pattern,state->index);
					size_t len=strlen(reserveStr);
					fklPushPtrStack(state,matchStateStack);
					fklPushPtrStack(newToken(FKL_TOKEN_RESERVE_STR,reserveStr,len),retvalStack);
					j+=len;
				}
				else
					free(state);
			}
			if(parts[i][j]=='\0')
				continue;
			if(parts[i][j]==',')
			{
				fklPushPtrStack(newToken(FKL_TOKEN_RESERVE_STR,",",strlen(",")),retvalStack);
				j++;
			}
			else if(parts[i][j]=='\"')
			{
				size_t len=skipUntilSpace(parts[i]+j);
				fklPushPtrStack(newToken(FKL_TOKEN_STRING,parts[i]+j,len),retvalStack);
				j+=len;
			}
			else if(!strncmp(parts[i]+j,"#\\",strlen("#\\")))
			{
				size_t len=getSymbolLen(parts[i]+j+2,matchStateStack);
				char* symbol=fklCopyMemory(parts[i]+j,len+1+2);
				symbol[len+2]='\0';
				fklPushPtrStack(newToken(FKL_TOKEN_CHAR,symbol,len+2),retvalStack);
				j+=len+2;
			}
			else if(!strncmp(parts[i]+j,"#b",strlen("#b")))
			{
				size_t len=getSymbolLen(parts[i]+j+2,matchStateStack);
				char* symbol=fklCopyMemory(parts[i]+j,len+1+2);
				symbol[len+2]='\0';
				fklPushPtrStack(newToken(FKL_TOKEN_BYTS,symbol,len+2),retvalStack);
				j+=len+2;
			}
			else
			{
				size_t len=getSymbolLen(parts[i]+j,matchStateStack);
				char* symbol=fklCopyMemory(parts[i]+j,len+1);
				symbol[len]='\0';
				FklTokenType type=FKL_TOKEN_SYMBOL;
				if(fklIsNum(symbol))
					type=FKL_TOKEN_NUM;
				fklPushPtrStack(newToken(type,symbol,len),retvalStack);
				j+=len;
			}
		}
	}
	if(!fklIsPtrStackEmpty(matchStateStack))
		return NULL;
	return retvalStack;
}

void fklPrintToken(FklPtrStack* tokenStack)
{
	static const char* tokenTypeName[]=
	{
		"reserve_str",
		"char",
		"num",
		"byts",
		"string",
		"symbol",
	};
	for(uint32_t i=0;i<tokenStack->top;i++)
	{
		FklToken* token=tokenStack->base[i];
		fprintf(stderr,"%s:%s\n",tokenTypeName[token->type],token->value);
	}
}
