#include<fakeLisp/fklni.h>
#include<fakeLisp/utils.h>

int fklNiResBp(size_t* ap,FklVMstack* stack)
{
	if(*ap>stack->bp)
		return *ap-stack->bp;
	stack->bp=fklPopUintStack(stack->bps);
	return 0;
}

void fklNiResTp(FklVMstack* stack)
{
//	pthread_rwlock_wrlock(&stack->lock);
	stack->tp=fklTopUintStack(stack->tps);
	fklStackRecycle(stack);
//	pthread_rwlock_unlock(&stack->lock);
}

void fklNiSetTp(FklVMstack* stack)
{
	fklPushUintStack(stack->tp,stack->tps);
}

void fklNiPopTp(FklVMstack* stack)
{
	fklPopUintStack(stack->tps);
}

FklVMvalue** fklNiGetTopSlot(FklVMstack* stack)
{
	return &stack->values[stack->tp-1];
}

void fklNiSetBp(uint64_t nbp,FklVMstack* s)
{
	fklPushUintStack(s->bp,s->bps);
	s->bp=nbp;
}

void fklNiReturn(FklVMvalue* v,size_t* ap,FklVMstack* s)
{
//	pthread_rwlock_wrlock(&s->lock);
	if(s->tp>=s->size)
	{
		s->values=(FklVMvalue**)realloc(s->values
				,sizeof(FklVMvalue*)*(s->size+64));
		FKL_ASSERT(s->values);
		s->size+=64;
	}
	if(*ap<s->tp)
	{
		FklVMvalue* t=s->values[*ap];
		s->values[*ap]=v;
		s->values[s->tp]=t;
	}
	else
		s->values[*ap]=v;
	*ap+=1;
	s->tp+=1;
//	pthread_rwlock_unlock(&s->lock);
}

inline void fklNiBegin(size_t* ap,FklVMstack* s)
{
	*ap=s->tp;
}

inline void fklNiEnd(size_t* ap,FklVMstack* s)
{
//	pthread_rwlock_wrlock(&s->lock);
	s->tp=*ap;
//	pthread_rwlock_unlock(&s->lock);
	*ap=0;
}

FklVMvalue* fklNiGetArg(size_t*ap,FklVMstack* stack)
{
	FklVMvalue* r=NULL;
	if(!(*ap>stack->bp))
		r=NULL;
	else
	{
		FklVMvalue* tmp=stack->values[*ap-1];
		*ap-=1;
		r=tmp;
	}
	return r;
}

FklVMvalue* fklNiPopTop(size_t* ap,FklVMstack* stack)
{
	if(!(*ap>stack->bp))
		return NULL;
	*ap-=1;
	return stack->values[*ap];
}

void fklNiDoSomeAfterSetq(FklVMvalue* v,FklSid_t sid)
{
	if(FKL_IS_PROC(v)&&v->u.proc->sid==0)
		v->u.proc->sid=sid;
	else if(FKL_IS_DLPROC(v)&&v->u.dlproc->sid==0)
		v->u.dlproc->sid=sid;
	else if(FKL_IS_USERDATA(v)&&v->u.ud->t->__setq_hook)
		v->u.ud->t->__setq_hook(v->u.ud->data,sid);
}
