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

static void _bc_princ(void* p,FILE* fp)
{
	fklPrintByteCode(p,fp);
}

static void* _bc_copy(void* p)
{
	FklByteCode* bc=p;
	FklByteCode* code=fklNewByteCode(bc->size);
	memcpy(code->code,bc->code,bc->size);
	return code;
}

static void _bc_append(void** a,void* b)
{
	fklCodeCat(*a,b);
}

static int _bc_equal(const FklVMudata* a,const FklVMudata* b)
{
	if(a->type!=b->type)
		return 0;
	else
	{
		FklByteCode* bc0=a->data;
		FklByteCode* bc1=b->data;
		if(bc0->size!=bc1->size)
			return 0;
		return memcmp(bc0->code,bc1->code,bc0->size);
	}
}

static FklVMudMethodTable FklcBcMethodTable=
{
	.__princ=_bc_princ,
	.__prin1=_bc_princ,
	.__finalizer=_bc_finalizer,
	.__equal=_bc_equal,
	.__call=NULL,
	.__cmp=NULL,
	.__write=NULL,
	.__atomic=NULL,
	.__append=_bc_append,
	.__copy=_bc_copy,
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
	FklcBcUdSid=fklAddSymbolCstr("fbc",fklcGetOuterSymbolTable())->id;
	FklcRel=rel;
}

void fklcCodeAppend(FklByteCode** fir,const FklByteCode* sec)
{
	if(!*fir)
		*fir=fklNewByteCode(0);
	fklCodeCat(*fir,sec);
}
