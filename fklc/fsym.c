#include"fsym.h"
#include<fakeLisp/utils.h>
FklSymbolTable* OuterSymbolTable=NULL;

void fklcInitFsym(void)
{
	OuterSymbolTable=fklCreateSymbolTable();
}

FklSymbolTable* fklcGetOuterSymbolTable(void)
{
	return OuterSymbolTable;
}

void fklcUninitFsym(void)
{
	fklDestroySymbolTable(OuterSymbolTable);
}

FklSid_t fklcGetSymbolIdWithOuterSymbolId(FklSid_t sid)
{
	const FklString* sym=fklGetSymbolWithId(sid,OuterSymbolTable)->symbol;
	return fklAddSymbolToGlob(sym)->id;
}

static const char* FklcErrType[]=
{
	"invalid-type-declare",
	"invalid-type-name",
	"invalid-mem-mode",
	"invalid-selector",
	"invalid-assign",
};

FklSid_t fklcGetErrorType(FklFklcErrType type)
{
	static FklSid_t fklcErrorTypeId[sizeof(FklcErrType)/sizeof(const char*)]={0};
	if(!fklcErrorTypeId[type])
		fklcErrorTypeId[type]=fklAddSymbolToGlobCstr(FklcErrType[type])->id;
	return fklcErrorTypeId[type];
}

char* fklcGenErrorMessage(FklFklcErrType type)
{
	char* t=fklCopyCstr("");
	switch(type)
	{
		case FKL_FKLC_ERR_IMCOMPILABLE_OBJ_OCCUR:
			t=fklStrCat(t,"imcompilable obj occur ");
			break;
		case FKL_FKLC_ERR_INVALID_SYNTAX_PATTERN:
			t=fklStrCat(t,"invalid syntax pattern ");
			break;
		default:
			FKL_ASSERT(0);
			break;
	}
	return t;
}

static int isInStack(FklVMvalue* v,FklPtrStack* stack,size_t* w)
{
	for(size_t i=0;i<stack->top;i++)
		if(stack->base[i]==v)
		{
			if(w)
				*w=i;
			return 1;
		}
	return 0;
}

int fklcIsValidSyntaxPattern(FklVMvalue* pattern)
{
	if(!FKL_IS_PAIR(pattern))
		return 0;
	FklVMvalue* head=pattern->u.pair->car;
	if(!FKL_IS_SYM(head))
		return 0;
	FklVMvalue* body=pattern->u.pair->cdr;
	FklPtrStack* symbolStack=fklCreatePtrStack(32,16);
	FklPtrStack* stack=fklCreatePtrStack(32,16);
	fklPushPtrStack(body,stack);
	while(!fklIsPtrStackEmpty(stack))
	{
		FklVMvalue* value=fklPopPtrStack(stack);
		if(FKL_IS_PAIR(value))
		{
			fklPushPtrStack(value->u.pair->cdr,stack);
			fklPushPtrStack(value->u.pair->car,stack);
		}
		else if(FKL_IS_SYM(value))
		{
			if(isInStack(value,symbolStack,NULL))
			{
				fklDestroyPtrStack(stack);
				fklDestroyPtrStack(symbolStack);
				return 0;
			}
			fklPushPtrStack(value,symbolStack);
		}
	}
	fklDestroyPtrStack(stack);
	fklDestroyPtrStack(symbolStack);
	return 1;
}

int fklcMatchPattern(FklVMvalue* pattern,FklVMvalue* exp,FklVMhashTable* ht,FklVMgc* gc)
{
	if(!FKL_IS_PAIR(exp))
		return 1;
	if(pattern->u.pair->car!=exp->u.pair->car)
		return 1;
	FklPtrStack* s0=fklCreatePtrStack(32,16);
	FklPtrStack* s1=fklCreatePtrStack(32,16);
	fklPushPtrStack(pattern->u.pair->cdr,s0);
	fklPushPtrStack(exp->u.pair->cdr,s1);
	while(!fklIsPtrStackEmpty(s0)&&!fklIsPtrStackEmpty(s1))
	{
		FklVMvalue* v0=fklPopPtrStack(s0);
		FklVMvalue* v1=fklPopPtrStack(s1);
		if(FKL_IS_SYM(v0))
			fklSetVMhashTable(v0,v1,ht,gc);
		else if(FKL_IS_PAIR(v0)&&FKL_IS_PAIR(v1))
		{
			fklPushPtrStack(v0->u.pair->cdr,s0);
			fklPushPtrStack(v0->u.pair->car,s0);
			fklPushPtrStack(v1->u.pair->cdr,s1);
			fklPushPtrStack(v1->u.pair->car,s1);
		}
		else if(!fklVMvalueEqual(v0,v1))
		{
			fklDestroyPtrStack(s0);
			fklDestroyPtrStack(s1);
			return 1;
		}
	}
	fklDestroyPtrStack(s0);
	fklDestroyPtrStack(s1);
	return 0;
}
