#include<fakeLisp/fklni.h>

static inline FklVMvalue* get_value(uint32_t* ap,FklVM* s)
{
	return *ap>0?s->base[--(*ap)]:NULL;
}

int fklNiResBp(uint32_t* ap,FklVM* stack)
{
	if(*ap>stack->bp)
		return *ap-stack->bp;
	stack->bp=FKL_GET_FIX(get_value(ap,stack));
	return 0;
}

void inline fklNiSetBpWithTp(FklVM* s)
{
	fklPushVMvalue(FKL_MAKE_VM_FIX(s->bp),s);
	s->bp=s->tp;
}

uint32_t fklNiSetBp(uint32_t nbp,FklVM* s)
{
	fklNiReturn(FKL_MAKE_VM_FIX(s->bp),&nbp,s);
	s->bp=nbp;
	return nbp;
}

FklVMvalue** fklNiReturn(FklVMvalue* v,uint32_t* ap,FklVM* s)
{
	fklAllocMoreStack(s);
	FklVMvalue** r=&s->base[*ap];
	if(*ap<s->tp)
	{
		FklVMvalue* t=s->base[*ap];
		*r=v;
		s->base[s->tp]=t;
	}
	else
		*r=v;
	(*ap)++;
	s->tp++;
	return r;
}

inline void fklNiBegin(uint32_t* ap,FklVM* s)
{
	*ap=s->tp;
}

inline void fklNiEnd(uint32_t* ap,FklVM* s)
{
	s->tp=*ap;
	*ap=0;
}

FklVMvalue* fklNiGetArg(uint32_t*ap,FklVM* stack)
{
	if(*ap>stack->bp)
		return stack->base[--(*ap)];
	return NULL;
}

void fklNiDoSomeAfterSetLoc(FklVMvalue* v,uint32_t idx,FklVMframe* f,FklVM* exe)
{
	if(FKL_IS_PROC(v)&&v->u.proc->sid==0)
	{
		FklFuncPrototype* pt=fklGetCompoundFrameProcPrototype(f,exe);
		v->u.proc->sid=pt->loc[idx].k.id;
	}
	else if(FKL_IS_DLPROC(v)&&v->u.dlproc->sid==0)
	{
		FklFuncPrototype* pt=fklGetCompoundFrameProcPrototype(f,exe);
		v->u.dlproc->sid=pt->loc[idx].k.id;
	}
	else if(FKL_IS_USERDATA(v)&&fklUdHasSetqHook(v->u.ud))
		fklDoSomeAfterSetqVMudata(v->u.ud,idx,f,exe);
}

void fklNiDoSomeAfterSetRef(FklVMvalue* v,uint32_t idx,FklVMframe* f,FklVM* exe)
{
	if(FKL_IS_PROC(v)&&v->u.proc->sid==0)
	{
		FklFuncPrototype* pt=fklGetCompoundFrameProcPrototype(f,exe);
		v->u.proc->sid=pt->refs[idx].k.id;
	}
	else if(FKL_IS_DLPROC(v)&&v->u.dlproc->sid==0)
	{
		FklFuncPrototype* pt=fklGetCompoundFrameProcPrototype(f,exe);
		v->u.dlproc->sid=pt->refs[idx].k.id;
	}
	else if(FKL_IS_USERDATA(v)&&fklUdHasSetqHook(v->u.ud))
		fklDoSomeAfterSetqVMudata(v->u.ud,idx,f,exe);
}
