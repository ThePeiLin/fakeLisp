#include"fbc.h"
#include"fsym.h"
#include<fakeLisp/symbol.h>
#include<fakeLisp/vm.h>
#include<string.h>
static FklVMvalue* FklcRel;
FklSid_t FklcBcUdSid=0;

static void _bc_finalizer(void* p)
{
	fklFreeByteCode(p);
}

static void _bc_princ(FILE* fp,void* p)
{
	fklPrintByteCode(p,fp);
}

static FklVMudMethodTable FklcBcMethodTable=
{
	.__princ=_bc_princ,
	.__prin1=NULL,
	.__finalizer=_bc_finalizer,
	.__equal=NULL,
	.__invoke=NULL,
	.__cmp=NULL,
	.__as_str=NULL,
	.__to_str=NULL,
	.__write=NULL,
};

FklVMudata* fklcNewFbcUd(FklByteCode* code)
{
	return fklNewVMudata(FklcBcUdSid,&FklcBcMethodTable,code,FklcRel);
}

int fklcIsFbc(FklVMvalue* p)
{
	return FKL_IS_USERDATA(p)&&p->u.ud->type==FklcBcUdSid&&p->u.ud->t==&FklcBcMethodTable;
}

void fklcInit(FklVMvalue* rel)
{
	FklcBcUdSid=fklAddSymbol("fbc",fklcGetOuterSymbolTable())->id;
	FklcRel=rel;
}

FklByteCode* fklcNewPushStrByteCode(const FklString* str)
{
	FklByteCode* tmp=fklNewByteCode(sizeof(char)+sizeof(str->size)+str->size);
	tmp->code[0]=FKL_PUSH_STR;
	fklSetU64ToByteCode(tmp->code+sizeof(char),str->size);
	memcpy(tmp->code+sizeof(char)+sizeof(str->size)
			,str->str
			,str->size);
	tmp=fklNewByteCode(sizeof(char)+sizeof(str->size)+str->size);
	tmp->code[0]=FKL_PUSH_STR;
	fklSetU64ToByteCode(tmp->code+sizeof(char),str->size);
	memcpy(tmp->code+sizeof(char)+sizeof(str->size)
			,str->str
			,str->size);
	return tmp;
}

void fklcCodeAppend(FklByteCode** fir,const FklByteCode* sec)
{
	if(!*fir)
		*fir=fklNewByteCode(0);
	fklCodeCat(*fir,sec);
}
