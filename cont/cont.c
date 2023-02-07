#include"cont.h"
#include<fakeLisp/utils.h>

static void _cont_finalizer(void* p)
{
	destroyContinuation(p);
}

static void _cont_call(FklVMvalue* v,FklVM* exe)
{
	Continuation* cc=v->u.ud->data;
	createCallChainWithContinuation(exe,cc);
}

static void _cont_atomic(void* p,FklVMgc* gc)
{
	Continuation* cont=(Continuation*)p;
	for(uint32_t i=0;i<cont->stack->tp;i++)
		fklGC_toGrey(cont->stack->values[i],gc);
	for(FklVMframe* curr=cont->curr;curr;curr=curr->prev)
		fklDoAtomicFrame(curr,gc);
}

static FklVMudMethodTable ContMethodTable=
{
	.__princ=NULL,
	.__prin1=NULL,
	.__finalizer=_cont_finalizer,
	.__equal=NULL,
	.__call=_cont_call,
	.__cmp=NULL,
	.__write=NULL,
	.__atomic=_cont_atomic,
	.__append=NULL,
	.__copy=NULL,
	.__length=NULL,
	.__hash=NULL,
	.__setq_hook=NULL,
};

int isCont(FklVMvalue* p)
{
	return FKL_IS_USERDATA(p)&&p->u.ud->t==&ContMethodTable;
}

Continuation* createContinuation(uint32_t ap,FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMframe* curr=exe->frames;
	Continuation* tmp=(Continuation*)malloc(sizeof(Continuation));
	FKL_ASSERT(tmp);
	tmp->stack=fklCopyStack(stack);
	tmp->stack->tp=ap;
	tmp->curr=NULL;
	for(FklVMframe* cur=curr;cur;cur=cur->prev)
		tmp->curr=fklCopyVMframe(cur,tmp->curr,exe);
	return tmp;
}

void destroyContinuation(Continuation* cont)
{
	FklVMstack* stack=cont->stack;
	FklVMframe* curr=cont->curr;
	while(curr)
	{
		FklVMframe* cur=curr;
		curr=curr->prev;
		fklDestroyVMframe(cur);
	}
	fklUninitUintStack(&stack->tps);
	fklUninitUintStack(&stack->bps);
	free(stack->values);
	free(stack);
	free(cont);
}

void createCallChainWithContinuation(FklVM* vm,Continuation* cc)
{
	FklVMstack* stack=vm->stack;
	FklVMstack* tmpStack=fklCopyStack(cc->stack);
	int32_t i=stack->bp;
	for(;i<stack->tp;i++)
	{
		if(tmpStack->tp>=tmpStack->size)
		{
			tmpStack->values=(FklVMvalue**)realloc(tmpStack->values,sizeof(FklVMvalue*)*(tmpStack->size+64));
			FKL_ASSERT(tmpStack->values);
			tmpStack->size+=64;
		}
		tmpStack->values[tmpStack->tp]=stack->values[i];
		tmpStack->tp+=1;
	}
	free(stack->values);
	fklUninitUintStack(&stack->tps);
	fklUninitUintStack(&stack->bps);
	stack->values=tmpStack->values;
	stack->bp=tmpStack->bp;
	stack->bps=tmpStack->bps;
	stack->size=tmpStack->size;
	stack->tp=tmpStack->tp;
	stack->tps=tmpStack->tps;
	free(tmpStack);
	fklDeleteCallChain(vm);
	vm->frames=NULL;
	for(FklVMframe* cur=cc->curr;cur;cur=cur->prev)
		vm->frames=fklCopyVMframe(cur,vm->frames,vm);
}

FklVMudata* createContUd(Continuation* cc,FklVMvalue* rel,FklVMvalue* pd)
{
	ContPublicData* pubd=pd->u.ud->data;
	return fklCreateVMudata(pubd->contSid,&ContMethodTable,cc,rel,pd);
}

ContPublicData* createContPd(FklSid_t id)
{
	ContPublicData* r=(ContPublicData*)malloc(sizeof(ContPublicData));
	FKL_ASSERT(r);
	r->contSid=id;
	return r;
}
