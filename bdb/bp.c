#include"bdb.h"
#include<fakeLisp/base.h>

static FKL_HASH_SET_KEY_VAL(breakpoint_set_key,BreakpointHashKey);

static FKL_HASH_SET_KEY_VAL(breakpoint_set_val,BreakpointHashItem);

static uintptr_t breakpoint_hash_func(const void* k)
{
	const BreakpointHashKey* kk=(const BreakpointHashKey*)k;
	return (kk->fid<<32)+kk->line+kk->pc;
}

static int breakpoint_key_equal(const void* a,const void* b)
{
	const BreakpointHashKey* aa=(const BreakpointHashKey*)a;
	const BreakpointHashKey* bb=(const BreakpointHashKey*)b;
	return aa->fid==bb->fid&&aa->line==bb->line&&aa->pc==bb->pc;
}

static void breakpoint_uninit(void* a)
{
	BreakpointHashItem* item=(BreakpointHashItem*)a;
	*(item->ins)=item->origin_ins;
}

static const FklHashTableMetaTable BreakpointHashMetaTable=
{
	.size=sizeof(BreakpointHashItem),
	.__getKey=fklHashDefaultGetKey,
	.__setKey=breakpoint_set_key,
	.__setVal=breakpoint_set_val,
	.__hashFunc=breakpoint_hash_func,
	.__keyEqual=breakpoint_key_equal,
	.__uninitItem=breakpoint_uninit,
};

void initBreakpointTable(FklHashTable* ht)
{
	fklInitHashTable(ht,&BreakpointHashMetaTable);
}

#include<fakeLisp/common.h>

static FklVMudMetaTable BreakpointWrapperMetaTable=
{
	.size=sizeof(BreakpointWrapper),
};

FklVMvalue* createBreakpointWrapper(FklVM* vm,const BreakpointHashItem* bp)
{
	FklVMvalue* r=fklCreateVMvalueUdata(vm,&BreakpointWrapperMetaTable,NULL);
	FKL_DECL_VM_UD_DATA(bpw,BreakpointWrapper,r);
	bpw->bp=bp;
	return r;
}

int isBreakpointWrapper(FklVMvalue* v)
{
	return FKL_IS_USERDATA(v)
		&&FKL_VM_UD(v)->t==&BreakpointWrapperMetaTable;
}

const BreakpointHashItem* getBreakpointFromWrapper(FklVMvalue* v)
{
	FKL_DECL_VM_UD_DATA(bpw,BreakpointWrapper,v);
	return bpw->bp;
}

