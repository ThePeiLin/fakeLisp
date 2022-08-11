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

static size_t _bc_hash(void* data,FklPtrStack* s)
{
	FklByteCode* bc=data;
	size_t sum=0;
	for(size_t i=0;i<bc->size;i++)
		sum+=bc->code[i];
	return sum;
}

static FklVMudMethodTable FklcBcMethodTable=
{
	.__princ=_bc_princ,
	.__prin1=NULL,
	.__finalizer=_bc_finalizer,
	.__equal=_bc_equal,
	.__call=NULL,
	.__cmp=NULL,
	.__write=NULL,
	.__atomic=NULL,
	.__append=_bc_append,
	.__copy=_bc_copy,
	.__hash=_bc_hash,
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

#define IS_LITERAL(V) ((V)==FKL_VM_NIL||fklIsVMnumber(V)||FKL_IS_CHR(V)||FKL_IS_STR(V)||FKL_IS_BYTEVECTOR(V)||FKL_IS_SYM(V))

#define IS_COMPILABLE(V) (IS_LITERAL(V)||FKL_IS_PAIR(V)||FKL_IS_BOX(V)||FKL_IS_VECTOR(V))

#define COMPILE_INTEGER(V) (FKL_IS_I32(V)?\
		fklNewPushI32ByteCode(FKL_GET_I32(V)):\
		FKL_IS_I64(V)?\
		fklNewPushI64ByteCode((V)->u.i64):\
		fklNewPushBigIntByteCode((V)->u.bigInt))

#define COMPILE_NUMBER(V) (fklIsInt(V)?COMPILE_INTEGER(V):fklNewPushF64ByteCode((V)->u.f64))

#define COMPILE_LITERAL(V) ((V)==FKL_VM_NIL?\
		fklNewPushNilByteCode():\
		fklIsVMnumber(V)?\
		COMPILE_NUMBER(V):\
		FKL_IS_CHR(V)?\
		fklNewPushCharByteCode(FKL_GET_CHR(V)):\
		FKL_IS_STR(V)?\
		fklNewPushStrByteCode((V)->u.str):\
		FKL_IS_SYM(V)?\
		fklNewPushSidByteCode(FKL_GET_SYM(V)):\
		fklNewPushBvecByteCode((V)->u.bvec))

FklByteCode* fklcNewPushObjByteCode(FklVMvalue* obj)
{
	FklPtrStack* stack=fklNewPtrStack(32,16);
	fklPushPtrStack(obj,stack);
	FklByteCode* retval=fklNewByteCode(0);
	while(!fklIsPtrStackEmpty(stack))
	{
		FklVMvalue* t=fklPopPtrStack(stack);
		if(!IS_COMPILABLE(t))
		{
			fklFreePtrStack(stack);
			fklFreeByteCode(retval);
			return NULL;
		}
		if(IS_LITERAL(t))
		{
			FklByteCode* tmp=COMPILE_LITERAL(t);
			fklReCodeCat(tmp,retval);
		}
		else
		{
			FklByteCode* tmp=NULL;
			if(FKL_IS_PAIR(t))
			{
				tmp=fklNewByteCode(sizeof(char));
				tmp->code[0]=FKL_OP_PUSH_PAIR;
				fklPushPtrStack(t->u.pair->car,stack);
				fklPushPtrStack(t->u.pair->cdr,stack);
			}
			else if(FKL_IS_BOX(t))
			{
				tmp=fklNewByteCode(sizeof(char));
				tmp->code[0]=FKL_OP_PUSH_BOX;
				fklPushPtrStack(t->u.box,stack);
			}
			else
			{
				tmp=fklNewByteCode(sizeof(char)+sizeof(uint64_t));
				tmp->code[0]=FKL_OP_PUSH_VECTOR;
				fklSetU64ToByteCode(tmp->code+sizeof(char),t->u.vec->size);
				for(size_t i=0;i<t->u.vec->size;i++)
					fklPushPtrStack(t->u.vec->base[i],stack);
			}
			fklReCodeCat(tmp,retval);
		}
	}
	return retval;
}

#undef COMPILE_INTEGER
#undef COMPILE_NUMBER
#undef COMPILE_LITERAL
#undef IS_LITERAL
#undef IS_COMPILABLE
