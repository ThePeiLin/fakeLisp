#include<fakeLisp/fklni.h>
int fklNiResBp(FklVMstack* stack)
{
	if(stack->ap>stack->bp)
		return stack->ap-stack->bp;
	FklVMvalue* prevBp=stack->values[stack->ap-1];
	stack->bp=FKL_GET_I32(prevBp);
	stack->ap-=1;
	return 0;
}

void fklNiReturn(FklVMvalue* v,FklVMstack* s)
{
	pthread_mutex_lock(&s->lock);
	if(s->ap)
		s->tp=s->ap;
	if(s->tp>=s->size)
	{
		s->values=(FklVMvalue**)realloc(s->values
				,sizeof(FklVMvalue*)*(s->size+64));
		FKL_ASSERT(s->values,__func__);
		if(s->values==NULL)
		{
			fprintf(stderr,"In file \"%s\" line %d\n",__FILE__,__LINE__);\
				perror(__func__);
			exit(1);
		}
		s->size+=64;
	}
	s->values[s->tp]=v;
	s->tp+=1;
	s->ap=0;
	pthread_mutex_unlock(&s->lock);
}

FklVMvalue* fklNiGetArg(FklVMstack* stack)
{
	pthread_mutex_lock(&stack->lock);
	FklVMvalue* r=NULL;
	if(!stack->ap)
		stack->ap=stack->tp;
	if(!(stack->ap>stack->bp))
		r=NULL;
	else
	{
		FklVMvalue* tmp=fklGetTopValue(stack);
		stack->ap-=1;
		if(FKL_IS_REF(tmp))
		{
			stack->ap-=1;
			r=*(FklVMvalue**)(FKL_GET_PTR(tmp));
		}
		if(FKL_IS_MREF(tmp))
		{
			void* ptr=fklGetTopValue(stack);
			stack->ap-=1;
			r=FKL_MAKE_VM_CHR(*(char*)ptr);
		}
		else
			r=tmp;
	}
	pthread_mutex_unlock(&stack->lock);
	return r;
}


