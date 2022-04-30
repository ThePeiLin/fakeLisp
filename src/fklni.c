#include<fakeLisp/fklni.h>
#include<fakeLisp/utils.h>

int fklNiResBp(uint32_t* ap,FklVMstack* stack)
{
	if(*ap>stack->bp)
		return *ap-stack->bp;
	FklVMvalue* prevBp=stack->values[*ap-1];
	stack->bp=FKL_GET_I32(prevBp);
	*ap-=1;
	return 0;
}

void fklNiReturn(FklVMvalue* v,uint32_t* ap,FklVMstack* s)
{
	pthread_rwlock_wrlock(&s->lock);
	if(s->tp>=s->size)
	{
		s->values=(FklVMvalue**)realloc(s->values
				,sizeof(FklVMvalue*)*(s->size+64));
		FKL_ASSERT(s->values,__func__);
		s->size+=64;
	}
	if(*ap<s->tp&&(s->tp<=*ap+1||(!FKL_IS_MREF(s->values[*ap+1]))))
	{
		FklVMvalue* t=s->values[*ap];
		s->values[*ap]=v;
		s->values[s->tp]=t;
	}
	else
		s->values[*ap]=v;
	if(*ap<s->tp&&(FKL_IS_MREF(s->values[*ap+1])))
		s->values[*ap+1]=FKL_VM_NIL;
	*ap+=1;
	s->tp+=1;
	pthread_rwlock_unlock(&s->lock);
}

void fklNiReturnMref(uint32_t size,void* mem,uint32_t* ap,FklVMstack* s)
{
	pthread_rwlock_wrlock(&s->lock);
	if(s->tp>=s->size)
	{
		s->values=(FklVMvalue**)realloc(s->values
				,sizeof(FklVMvalue*)*(s->size+64));
		FKL_ASSERT(s->values,__func__);
		s->size+=64;
	}
	if(*ap<s->tp)
	{
		FklVMvalue* t=s->values[*ap];
		s->values[*ap]=mem;
		s->values[s->tp]=t;
	}
	else
		s->values[*ap]=mem;
	*ap+=1;
	s->tp+=1;
	if(s->tp>=s->size)
	{
		s->values=(FklVMvalue**)realloc(s->values
				,sizeof(FklVMvalue*)*(s->size+64));
		FKL_ASSERT(s->values,__func__);
		s->size+=64;
	}
	if(*ap<s->tp)
	{
		FklVMvalue* t=s->values[*ap];
		s->values[*ap]=FKL_MAKE_VM_MREF(size);
		s->values[s->tp]=t;
	}
	else
		s->values[*ap]=FKL_MAKE_VM_MREF(size);
	*ap+=1;
	s->tp+=1;
	pthread_rwlock_unlock(&s->lock);
}

inline void fklNiBegin(uint32_t* ap,FklVMstack* s)
{
	*ap=s->tp;
}

inline void fklNiEnd(uint32_t* ap,FklVMstack* s)
{
	pthread_rwlock_wrlock(&s->lock);
	s->tp=*ap;
	pthread_rwlock_unlock(&s->lock);
	*ap=0;
}

FklVMvalue* fklNiGetArg(uint32_t*ap,FklVMstack* stack)
{
	FklVMvalue* r=NULL;
	if(!(*ap>stack->bp))
		r=NULL;
	else
	{
		FklVMvalue* tmp=stack->values[*ap-1];
		*ap-=1;
		if(FKL_IS_REF(tmp))
		{
			*ap-=1;
			r=*(FklVMvalue**)(FKL_GET_PTR(tmp));
		}
		else if(FKL_IS_MREF(tmp))
		{
			void* ptr=stack->values[*ap-1];
			*ap-=1;
			r=FKL_MAKE_VM_CHR(*(char*)ptr);
		}
		else
			r=tmp;
	}
	return r;
}

FklVMvalue* fklNiNewVMvalue(FklValueType type,void* p,FklVMstack* s,FklVMheap* heap)
{
	return fklNewVMvalueToStack(type,p,s,heap);
}

FklVMvalue* fklNiPopTop(uint32_t* ap,FklVMstack* stack)
{
	if(!(*ap>stack->bp))
		return NULL;
	*ap-=1;
	return stack->values[*ap];
}
