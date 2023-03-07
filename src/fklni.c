#include<fakeLisp/fklni.h>
#include<fakeLisp/utils.h>

static inline FklVMvalue* get_value(uint32_t* ap,FklVMstack* s)
{
	return *ap>0?s->values[--(*ap)]:NULL;
}

int fklNiResBp(uint32_t* ap,FklVMstack* stack)
{
	if(*ap>stack->bp)
		return *ap-stack->bp;
	stack->bp=FKL_GET_FIX(get_value(ap,stack));
	//stack->bp=fklPopUintStack(&stack->bps);
	return 0;
}

//void fklNiResTp(FklVMstack* stack)
//{
////	pthread_rwlock_wrlock(&stack->lock);
//	stack->tp=fklTopUintStack(&stack->tps);
//	fklStackRecycle(stack);
////	pthread_rwlock_unlock(&stack->lock);
//}
//
//void fklNiSetTp(FklVMstack* stack)
//{
//	fklPushUintStack(stack->tp,&stack->tps);
//}
//
//void fklNiPopTp(FklVMstack* stack)
//{
//	fklPopUintStack(&stack->tps);
//}

void inline fklNiSetBpWithTp(FklVMstack* s)
{
	fklPushVMvalue(FKL_MAKE_VM_FIX(s->bp),s);
	s->bp=s->tp;
}

uint32_t fklNiSetBp(uint32_t nbp,FklVMstack* s)
{
	fklNiReturn(FKL_MAKE_VM_FIX(s->bp),&nbp,s);
	//fklPushUintStack(s->bp,&s->bps);
	s->bp=nbp;
	return nbp;
}

void fklNiReturn(FklVMvalue* v,uint32_t* ap,FklVMstack* s)
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
	(*ap)++;
	s->tp++;
//	pthread_rwlock_unlock(&s->lock);
}

inline void fklNiBegin(uint32_t* ap,FklVMstack* s)
{
	*ap=s->tp;
}

inline void fklNiEnd(uint32_t* ap,FklVMstack* s)
{
//	pthread_rwlock_wrlock(&s->lock);
	s->tp=*ap;
//	pthread_rwlock_unlock(&s->lock);
	*ap=0;
}

FklVMvalue* fklNiGetArg(uint32_t*ap,FklVMstack* stack)
{
	if(*ap>stack->bp)
		return stack->values[--(*ap)];
	return NULL;
}

void fklNiDoSomeAfterSetLoc(FklVMvalue* v,uint32_t idx,FklVMframe* f,FklVM* exe)
{
	if(FKL_IS_PROC(v)&&v->u.proc->sid==0)
	{
		FklPrototype* pt=fklGetCompoundFrameProcPrototype(f,exe);
		v->u.proc->sid=pt->loc[idx].id;
	}
	else if(FKL_IS_DLPROC(v)&&v->u.dlproc->sid==0)
	{
		FklPrototype* pt=fklGetCompoundFrameProcPrototype(f,exe);
		v->u.dlproc->sid=pt->loc[idx].id;
	}
	else if(FKL_IS_USERDATA(v)&&fklUdHasSetqHook(v->u.ud))
		fklDoSomeAfterSetqVMudata(v->u.ud,idx,f,exe);
}

void fklNiDoSomeAfterSetRef(FklVMvalue* v,uint32_t idx,FklVMframe* f,FklVM* exe)
{
	if(FKL_IS_PROC(v)&&v->u.proc->sid==0)
	{
		FklPrototype* pt=fklGetCompoundFrameProcPrototype(f,exe);
		v->u.proc->sid=pt->refs[idx].id;
	}
	else if(FKL_IS_DLPROC(v)&&v->u.dlproc->sid==0)
	{
		FklPrototype* pt=fklGetCompoundFrameProcPrototype(f,exe);
		v->u.dlproc->sid=pt->refs[idx].id;
	}
	else if(FKL_IS_USERDATA(v)&&fklUdHasSetqHook(v->u.ud))
		fklDoSomeAfterSetqVMudata(v->u.ud,idx,f,exe);
}
