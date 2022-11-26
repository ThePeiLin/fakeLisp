#include"fbc.h"
#include"fsym.h"
#include<fakeLisp/symbol.h>
#include<fakeLisp/vm.h>
#include<string.h>
FklSid_t FklcBcUdSid=0;

extern FklSymbolTable* OuterSymbolTable;
static void _bc_finalizer(void* p)
{
	fklDestroyByteCode(p);
}

static void _bc_princ(void* p,FILE* fp)
{
	fklPrintByteCode(p,fp,OuterSymbolTable);
}

static void* _bc_copy(void* p)
{
	FklByteCode* bc=p;
	FklByteCode* code=fklCreateByteCode(bc->size);
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

static size_t _bc_length(void* data)
{
	FklByteCode* bc=data;
	return bc->size;
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
	.__length=_bc_length,
	.__hash=_bc_hash,
	.__setq_hook=NULL,
};

FklVMudata* fklcCreateFbcUd(FklByteCode* code,FklVMvalue* rel)
{
	return fklCreateVMudata(FklcBcUdSid,&FklcBcMethodTable,code,rel);
}

int fklcIsFbc(FklVMvalue* p)
{
	return FKL_IS_USERDATA(p)&&p->u.ud->type==FklcBcUdSid&&p->u.ud->t==&FklcBcMethodTable;
}

void fklcInit(FklVMdll* dll)
{
	FklcBcUdSid=fklAddSymbolToGlobCstr("fbc")->id;
	dll->pd=NULL;
}

void fklcCodeAppend(FklByteCode** fir,const FklByteCode* sec)
{
	if(!*fir)
		*fir=fklCreateByteCode(0);
	fklCodeCat(*fir,sec);
}

#define IS_LITERAL(V) ((V)==FKL_VM_NIL||fklIsVMnumber(V)||FKL_IS_CHR(V)||FKL_IS_STR(V)||FKL_IS_BYTEVECTOR(V)||FKL_IS_SYM(V))

#define IS_COMPILABLE(V) (IS_LITERAL(V)||FKL_IS_PAIR(V)||FKL_IS_BOX(V)||FKL_IS_VECTOR(V))

#define COMPILE_INTEGER(V) (FKL_IS_I32(V)?\
		fklCreatePushI32ByteCode(FKL_GET_I32(V)):\
		FKL_IS_I64(V)?\
		fklCreatePushI64ByteCode((V)->u.i64):\
		fklCreatePushBigIntByteCode((V)->u.bigInt))

#define COMPILE_NUMBER(V) (fklIsInt(V)?COMPILE_INTEGER(V):fklCreatePushF64ByteCode((V)->u.f64))

#define COMPILE_LITERAL(V) ((V)==FKL_VM_NIL?\
		fklCreatePushNilByteCode():\
		fklIsVMnumber(V)?\
		COMPILE_NUMBER(V):\
		FKL_IS_CHR(V)?\
		fklCreatePushCharByteCode(FKL_GET_CHR(V)):\
		FKL_IS_STR(V)?\
		fklCreatePushStrByteCode((V)->u.str):\
		FKL_IS_SYM(V)?\
		fklcCreatePushSidByteCode(FKL_GET_SYM(V)):\
		fklCreatePushBvecByteCode((V)->u.bvec))

FklByteCode* fklcCreatePushSidByteCode(FklSid_t a)
{
	return fklCreatePushSidByteCode(fklAddSymbol(fklGetGlobSymbolWithId(a)->symbol,OuterSymbolTable)->id);
}

FklByteCode* fklcCreatePushObjByteCode(FklVMvalue* obj)
{
	FklPtrStack* stack=fklCreatePtrStack(32,16);
	fklPushPtrStack(obj,stack);
	FklByteCode* retval=fklCreateByteCode(0);
	while(!fklIsPtrStackEmpty(stack))
	{
		FklVMvalue* t=fklPopPtrStack(stack);
		if(!IS_COMPILABLE(t))
		{
			fklDestroyPtrStack(stack);
			fklDestroyByteCode(retval);
			return NULL;
		}
		if(IS_LITERAL(t))
		{
			FklByteCode* tmp=COMPILE_LITERAL(t);
			fklReCodeCat(tmp,retval);
			fklDestroyByteCode(tmp);
		}
		else
		{
			FklByteCode* tmp=NULL;
			if(FKL_IS_PAIR(t))
			{
				tmp=fklCreateByteCode(sizeof(char));
				tmp->code[0]=FKL_OP_PUSH_PAIR;
				fklPushPtrStack(t->u.pair->car,stack);
				fklPushPtrStack(t->u.pair->cdr,stack);
			}
			else if(FKL_IS_BOX(t))
			{
				tmp=fklCreateByteCode(sizeof(char));
				tmp->code[0]=FKL_OP_PUSH_BOX;
				fklPushPtrStack(t->u.box,stack);
			}
			else if(FKL_IS_HASHTABLE(t))
			{
				tmp=fklCreateByteCode(sizeof(char)+sizeof(uint64_t));
				FklOpcode hashtype=0;
				switch(t->u.hash->type)
				{
					case FKL_VM_HASH_EQ:
						hashtype=FKL_OP_PUSH_HASHTABLE_EQ;
						break;
					case FKL_VM_HASH_EQV:
						hashtype=FKL_OP_PUSH_HASHTABLE_EQV;
						break;
					case FKL_VM_HASH_EQUAL:
						hashtype=FKL_OP_PUSH_HASHTABLE_EQUAL;
						break;
				}
				tmp->code[0]=hashtype;
				fklSetU64ToByteCode(tmp->code+sizeof(char),t->u.hash->ht->num);
				for(FklHashTableNodeList* list=t->u.hash->ht->list;list;list=list->next)
				{
					FklVMhashTableItem* item=list->node->item;
					fklPushPtrStack(item->key,stack);
					fklPushPtrStack(item->v,stack);
				}
			}
			else if(FKL_IS_VECTOR(t))
			{
				tmp=fklCreateByteCode(sizeof(char)+sizeof(uint64_t));
				tmp->code[0]=FKL_OP_PUSH_VECTOR;
				fklSetU64ToByteCode(tmp->code+sizeof(char),t->u.vec->size);
				for(size_t i=0;i<t->u.vec->size;i++)
					fklPushPtrStack(t->u.vec->base[i],stack);
			}
			fklReCodeCat(tmp,retval);
			fklDestroyByteCode(tmp);
		}
	}
	fklDestroyPtrStack(stack);
	return retval;
}

#undef COMPILE_INTEGER
#undef COMPILE_NUMBER
#undef COMPILE_LITERAL
#undef IS_LITERAL
#undef IS_COMPILABLE
