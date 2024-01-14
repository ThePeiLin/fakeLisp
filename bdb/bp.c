#include"bdb.h"
#include<fakeLisp/base.h>

static FKL_HASH_SET_KEY_VAL(breakpoint_set_key,uint64_t);

static FKL_HASH_SET_KEY_VAL(breakpoint_set_val,BreakpointHashItem);

static uintptr_t breakpoint_hash_func(const void* k)
{
	return *(const uint64_t*)k;
}

static int breakpoint_key_equal(const void* a,const void* b)
{
	const uint64_t* aa=(const uint64_t*)a;
	const uint64_t* bb=(const uint64_t*)b;
	return *aa==*bb;
}

// static void breakpoint_uninit(void* a)
// {
// 	BreakpointHashItem* item=(BreakpointHashItem*)a;
// 	*(item->ins)=item->origin_ins;
// }

static const FklHashTableMetaTable BreakpointHashMetaTable=
{
	.size=sizeof(BreakpointHashItem),
	.__getKey=fklHashDefaultGetKey,
	.__setKey=breakpoint_set_key,
	.__setVal=breakpoint_set_val,
	.__hashFunc=breakpoint_hash_func,
	.__keyEqual=breakpoint_key_equal,
	// .__uninitItem=breakpoint_uninit,
	.__uninitItem=fklDoNothingUninitHashItem,
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

FklVMvalue* createBreakpointWrapper(FklVM* vm,Breakpoint* bp)
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

Breakpoint* getBreakpointFromWrapper(FklVMvalue* v)
{
	FKL_DECL_VM_UD_DATA(bpw,BreakpointWrapper,v);
	return bpw->bp;
}

Breakpoint* delBreakpoint(DebugCtx* dctx,uint64_t num)
{
	BreakpointHashItem item={.num=0,.bp=NULL,};
	fklDelHashItem(&num,&dctx->breakpoints,&item);
	if(item.bp==NULL)
		return NULL;
	Breakpoint* bp=item.bp;
	Breakpoint* next=bp->next;
	Breakpoint* prev=bp->prev;

	if(prev)
		prev->next=next;
	else
		bp->ins->ptr=next;

	if(next)
		next->prev=prev;

	if(bp->ins->imm==1)
		*(bp->ins)=bp->origin_ins;
	else
		bp->ins->imm--;

	bp->compiled=1;

	if(bp->cond_exp)
	{
		fklDestroyNastNode(bp->cond_exp);
		bp->cond_exp=NULL;
	}

	if(bp->proc)
	{
		FklVMproc* proc=FKL_VM_PROC(bp->proc);
		fklPushUintStack(proc->protoId,&dctx->unused_prototype_id_for_cond_bp);
		bp->proc=NULL;
	}

	bp->is_deleted=1;
	bp->next=dctx->deleted_breakpoints;
	dctx->deleted_breakpoints=bp;

	return bp;
	// for(FklHashTableItem* list=dctx->breakpoints.first
	// 		;list
	// 		;list=list->next)
	// {
	// 	BreakpointHashItem* item=(BreakpointHashItem*)list->data;
	// 	if(item->num==num)
	// 	{
	// 		if(dctx->reached_breakpoint==item)
	// 		{
	// 			dctx->reached_breakpoint=NULL;
	// 			toggleVMint3(dctx->reached_thread);
	// 		}
	//
	// 		fklRemoveHashItem(&dctx->breakpoints,list);
	//
	// 		list->next=dctx->deleted_breakpoints;
	// 		dctx->deleted_breakpoints=list;
	// 		item->compiled=0;
	// 		if(item->cond_exp)
	// 		{
	// 			fklDestroyNastNode(item->cond_exp);
	// 			item->cond_exp=NULL;
	// 		}
	// 		if(item->proc)
	// 		{
	// 			FklVMproc* proc=FKL_VM_PROC(item->proc);
	// 			fklPushUintStack(proc->protoId,&dctx->unused_prototype_id_for_cond_bp);
	// 			item->proc=NULL;
	// 		}
	// 		return item;
	// 		break;
	// 	}
	// }
	return NULL;
}

void markBreakpointCondExpObj(DebugCtx* dctx,FklVMgc* gc)
{
	for(FklHashTableItem* list=dctx->breakpoints.first
			;list
			;list=list->next)
	{
		Breakpoint* item=((BreakpointHashItem*)list->data)->bp;
		if(item->cond_exp_obj)
			fklVMgcToGray(item->cond_exp_obj,gc);
	}
	for(Breakpoint* bp=dctx->deleted_breakpoints
			;bp
			;bp=bp->next)
	{
		if(bp->cond_exp_obj)
			fklVMgcToGray(bp->cond_exp_obj,gc);
	}
}

Breakpoint* createBreakpoint(uint64_t num
		,FklSid_t fid
		,uint32_t line
		,FklInstruction* ins
		,DebugCtx* ctx)
{
	Breakpoint* bp=(Breakpoint*)calloc(1,sizeof(Breakpoint));
	FKL_ASSERT(bp);
	bp->num=num;
	bp->fid=fid;
	bp->line=line;
	bp->ins=ins;
	if(ins->op==0)
	{
		ins->imm++;
		Breakpoint* prev=ins->ptr;
		bp->origin_ins=prev->origin_ins;
		prev->prev=bp;
		bp->next=prev;
		ins->ptr=bp;
	}
	else
	{
		bp->origin_ins=*(ins);
		ins->op=0;
		ins->ptr=bp;
		ins->imm=1;
	}
	return bp;
}

