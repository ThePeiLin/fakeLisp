#include"fbc.h"
#include"fsym.h"
#include<fakeLisp/symbol.h>
#include<fakeLisp/vm.h>
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
