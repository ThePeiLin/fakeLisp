#include"tool.h"
#include"fakeVM.h"
#include<string.h>
static int (*ByteCodes[])(fakeVM*)=
{
	B_dummy,
	B_push_nil,
	B_push_pair,
	B_push_int,
	B_push_chr,
	B_push_dbl,
	B_push_str,
	B_push_sym,
	B_push_var,
	B_push_car,
	B_push_cdr,
	B_push_top,
	B_push_proc,
	B_pop,
	B_pop_var,
	B_pop_rest_var,
	B_pop_car,
	B_pop_cdr,
	B_init_proc,
	B_end_proc,
	B_set_bp,
	B_invoke,
	B_res_bp,
	B_jump_if_ture,
	B_jump_if_false,
	B_jump,
	B_add,
	B_sub,
	B_mul,
	B_div,
	B_mod,
	B_atom,
	B_null,
	B_open,
	B_close,/*
	B_eq,
	B_ne,
	B_gt,
	B_lt,
	B_le,
	B_not,
	B_in,
	B_out,
	B_go,
	B_run,
	B_send,
	B_accept*/
};

fakeVM* newFakeVM(byteCode* mainproc,byteCode* procs)
{
	fakeVM* exe=(fakeVM*)malloc(sizeof(fakeVM));
	exe->mainproc=newExcode(mainproc);
	exe->curproc=exe->mainproc;
	exe->procs=procs;
	exe->stack=newStack(0);
	exe->files=newFileStack();
	return exe;
}

void runFakeVM(fakeVM* exe)
{
	while(exe->curproc!=NULL&&exe->curproc->cp!=exe->curproc->size)
	{
		excode* curproc=exe->curproc;
		switch(ByteCodes[curproc->code[curproc->cp]](exe))
		{
			case 1:fprintf(stderr,"Wrong arguements!\n");
				   exit(EXIT_FAILURE);
			case 2:fprintf(stderr,"Stack error!\n");
				   exit(EXIT_FAILURE);
		}
	}
}

int B_dummy(fakeVM* exe)
{
	fprintf(stderr,"Wrong byte code!\n");
	exit(EXIT_FAILURE);
	return 1;
}

int B_push_nil(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	if(stack->tp>=stack->size)
	{
		stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size+64));
		if(stack->values==NULL)errors(OUTOFMEMORY);
		stack->size+=64;
	}
	stack->values[stack->tp]=NULL;
	stack->tp+=1;
	proc->cp+=1;
	return 0;
}

int B_push_pair(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	if(stack->tp>=stack->size)
	{
		stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size+64));
		if(stack->values==NULL)errors(OUTOFMEMORY);
		stack->size+=64;
	}
	stackvalue* objValue=newStackValue(PAR);
	objValue->value.par.car=NULL;
	objValue->value.par.cdr=NULL;
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=1;
	return 0;
}

int B_push_int(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	if(stack->tp>=stack->size)
	{
		stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size+64));
		if(stack->values==NULL)errors(OUTOFMEMORY);
		stack->size+=64;
	}
	stackvalue* objValue=newStackValue(INT);
	objValue->value.num=*(int32_t*)(proc->code+proc->cp+1);
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=5;
	return 0;
}

int B_push_chr(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	if(stack->tp>=stack->size)
	{
		stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size+64));
		if(stack->values==NULL)errors(OUTOFMEMORY);
		stack->size+=64;
	}
	stackvalue* objValue=newStackValue(CHR);
	objValue->value.num=*(proc->code+proc->cp+1);
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=2;
	return 0;
}

int B_push_dbl(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	if(stack->tp>=stack->size)
	{
		stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size+64));
		if(stack->values==NULL)errors(OUTOFMEMORY);
		stack->size+=64;
	}
	stackvalue* objValue=newStackValue(DBL);
	objValue->value.dbl=*(double*)(proc->code+proc->cp+1);
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=9;
	return 0;
}

int B_push_str(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	int len=strlen((char*)(proc->code+proc->cp+1));
	char* tmpStr=(char*)malloc(sizeof(char)*(len+1));
	if(tmpStr==NULL)errors(OUTOFMEMORY);
	strcpy(tmpStr,proc->code+proc->cp+1);
	if(stack->tp>=stack->size)
	{
		stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size+64));
		if(stack->values==NULL)errors(OUTOFMEMORY);
		stack->size+=64;
	}
	stackvalue* objValue=newStackValue(STR);
	objValue->value.str=tmpStr;
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=2+len;
	return 0;
}

int B_push_sym(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	int len=strlen((char*)(proc->code+proc->cp+1));
	char* tmpStr=(char*)malloc(sizeof(char)*(len+1));
	if(tmpStr==NULL)errors(OUTOFMEMORY);
	strcpy(tmpStr,proc->code+proc->cp+1);
	if(stack->tp>=stack->size)
	{
		stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size+64));
		if(stack->values==NULL)errors(OUTOFMEMORY);
		stack->size+=64;
	}
	stackvalue* objValue=newStackValue(SYM);
	objValue->value.str=tmpStr;
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=2+len;
	return 0;
}

int B_push_var(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	if(stack->tp>=stack->size)
	{
		stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size+64));
		if(stack->values==NULL)errors(OUTOFMEMORY);
		stack->size+=64;
	}
	int32_t countOfVar=*(int32_t*)(proc->code+proc->cp+1);
	varstack* curEnv=proc->localenv;
	while(curEnv->bound==-1||countOfVar<curEnv->bound)curEnv=curEnv->prev;
	stackvalue* tmpValue=copyValue(*(curEnv->values+countOfVar-(curEnv->bound)));
	stack->values[stack->tp]=tmpValue;
	stack->tp+=1;
	proc->cp+=5;
	return 0;
}

int B_push_car(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	stackvalue* objValue=getTopValue(stack);
	if(objValue->type!=PAR)return 1;
	else
	{
		stack->values[stack->tp-1]=copyValue(getCar(objValue));
		freeStackValue(objValue);
	}
	proc->cp+=1;
	return 0;
}

int B_push_cdr(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	stackvalue* objValue=getTopValue(stack);
	if(objValue->type!=PAR)return 1;
	else
	{
		stack->values[stack->tp-1]=copyValue(getCar(objValue));
		freeStackValue(objValue);
	}
	proc->cp+=1;
	return 0;
}

int B_push_top(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	if(stack->tp==stack->bp)return 1;
	if(stack->tp>=stack->size)
	{
		stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size+64));
		if(stack->values==NULL)errors(OUTOFMEMORY);
		stack->size+=64;
	}
	stack->values[stack->tp]=copyValue(getTopValue(stack));
	stack->tp+=1;
	proc->cp+=1;
	return 0;
}

int B_push_proc(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	int32_t countOfProc=*(int32_t*)(proc->code+proc->cp+1);
	if(stack->tp>=stack->size)
	{
		stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size+64));
		if(stack->values==NULL)errors(OUTOFMEMORY);
		stack->size+=64;
	}
	stackvalue* objValue=newStackValue(PRC);
	objValue->value.prc=newExcode(exe->procs+countOfProc);
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=5;
	return 0;
}

int B_pop(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	freeStackValue(getTopValue(stack));
	stack->values[stack->tp-1]=NULL;
	stack->tp-=1;
	if(stack->size-stack->tp>64)
	{
		stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size-64));
		if(stack->values==NULL)errors(OUTOFMEMORY);
		stack->size-=64;
	}
	proc->cp+=1;
	return 0;
}

int B_pop_var(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	int32_t countOfVar=*(int32_t*)(proc->code+proc->cp+1);
	varstack* curEnv=proc->localenv;
	while(curEnv->bound==-1||countOfVar<curEnv->bound)curEnv=curEnv->prev;
	stackvalue** pValue=NULL;
	if(countOfVar-curEnv->bound>=curEnv->size)
	{
		curEnv->values=(stackvalue**)realloc(curEnv->values,sizeof(stackvalue*)*(stack->size+1));
		curEnv->size+=1;
		pValue=curEnv->values+countOfVar-(curEnv->bound);
	}
	else 
	{
		pValue=curEnv->values+countOfVar-(curEnv->bound);
		freeStackValue(*pValue);
	}
	if(stack->tp>stack->bp)
	{
		*pValue=getTopValue(stack);
		stack->values[stack->tp-1]=NULL;
		stack->tp-=1;
		if(stack->size-stack->tp>64)
		{
			stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size-64));
			if(stack->values==NULL)errors(OUTOFMEMORY);
			stack->size-=64;
		}
	}else *pValue=NULL;
	proc->cp+=5;
	return 0;
}

int B_pop_rest_var(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	int32_t countOfVar=*(int32_t*)(proc->code+proc->cp+1);
	varstack* curEnv=proc->localenv;
	while(curEnv->bound==-1||countOfVar<curEnv->bound)curEnv=curEnv->prev;
	stackvalue** tmpValue=NULL;
	if(countOfVar-curEnv->bound>=curEnv->size)
	{
		curEnv->values=(stackvalue**)realloc(curEnv->values,sizeof(stackvalue*)*(stack->size+1));
		curEnv->size+=1;
		tmpValue=curEnv->values+countOfVar-(curEnv->bound);
	}
	else
	{
		tmpValue=curEnv->values+countOfVar-(curEnv->bound);
		freeStackValue(*tmpValue);
	}
	stackvalue* obj=newStackValue(PAR);
	stackvalue* tmp=obj;
	for(;;)
	{
		stackvalue* topValue=getTopValue(stack);
		tmp->value.par.car=copyValue(topValue);
		stack->tp-=1;
		if(stack->size-stack->tp>64)
		{
			stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size-64));
			if(stack->values==NULL)errors(OUTOFMEMORY);
			stack->size-=64;
		}
		if(stack->tp>stack->bp)
		{
			tmp->value.par.cdr=newStackValue(PAR);
			tmp=tmp->value.par.cdr;
		}
		else break;
	}
	*tmpValue=copyValue(obj);
	freeStackValue(obj);
	proc->cp+=5;
	return 0;
}

int B_pop_car(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	stackvalue* topValue=getTopValue(stack);
	stackvalue* objValue=getValue(stack,stack->tp-2);
	if(objValue->type!=PAR)return 1;
	freeStackValue(objValue->value.par.car);
	objValue->value.par.car=copyValue(topValue);
	freeStackValue(topValue);
	stack->tp-=1;
	if(stack->size-stack->tp>64)
	{
		stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size-64));
		if(stack->values==NULL)errors(OUTOFMEMORY);
		stack->size-=64;
	}
	proc->cp+=1;
	return 0;
}

int B_pop_cdr(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	stackvalue* topValue=getTopValue(stack);
	stackvalue* objValue=getValue(stack,stack->tp-2);
	if(objValue->type!=PAR)return 1;
	freeStackValue(objValue->value.par.cdr);
	objValue->value.par.cdr=copyValue(topValue);
	freeStackValue(topValue);
	stack->tp-=1;
	if(stack->size-stack->tp>64)
	{
		stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size-64));
		if(stack->values==NULL)errors(OUTOFMEMORY);
		stack->size-=64;
	}
	proc->cp+=1;
	return 0;
}

int B_add(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	stackvalue* firValue=getValue(stack,stack->tp-1);
	stackvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	if(stack->size-stack->tp>64)
	{
		stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size-64));
		if(stack->values==NULL)errors(OUTOFMEMORY);
		stack->size-=64;
	}
	if((firValue->type!=INT&&firValue->type!=DBL)||(secValue->type!=INT&&secValue->type!=DBL))return 1;
	if(firValue->type==DBL||secValue->type==DBL)
	{
		double result=((firValue->type==DBL)?firValue->value.dbl:firValue->value.num)+((secValue->type==DBL)?secValue->value.dbl:secValue->value.num);
		stackvalue* tmpValue=newStackValue(DBL);
		tmpValue->value.dbl=result;
		stack->values[stack->tp-1]=tmpValue;
	}
	else if(firValue->type==INT&&secValue->type==INT)
	{
		int32_t result=firValue->value.num+secValue->value.num;
		stackvalue* tmpValue=newStackValue(INT);
		tmpValue->value.num=result;
		stack->values[stack->tp-1]=tmpValue;
	}
	freeStackValue(firValue);
	freeStackValue(secValue);
	proc->cp+=1;
	return 0;
}

int B_sub(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	stackvalue* firValue=getValue(stack,stack->tp-1);
	stackvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	if(stack->size-stack->tp>64)
	{
		stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size-64));
		if(stack->values==NULL)errors(OUTOFMEMORY);
		stack->size-=64;
	}
	if((firValue->type!=INT&&firValue->type!=DBL)||(secValue->type!=INT&&secValue->type!=DBL))return 1;
	if(firValue->type==DBL||secValue->type==DBL)
	{
		double result=((secValue->type==DBL)?secValue->value.dbl:secValue->value.num)-((firValue->type==DBL)?firValue->value.dbl:firValue->value.num);
		stackvalue* tmpValue=newStackValue(DBL);
		tmpValue->value.dbl=result;
		stack->values[stack->tp-1]=tmpValue;
	}
	else if(firValue->type==INT&&secValue->type==INT)
	{
		int32_t result=secValue->value.num-firValue->value.num;
		stackvalue* tmpValue=newStackValue(INT);
		tmpValue->value.num=result;
		stack->values[stack->tp-1]=tmpValue;
	}
	freeStackValue(firValue);
	freeStackValue(secValue);
	proc->cp+=1;
	return 0;
}

int B_mul(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	stackvalue* firValue=getValue(stack,stack->tp-1);
	stackvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	if(stack->size-stack->tp>64)
	{
		stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size-64));
		if(stack->values==NULL)errors(OUTOFMEMORY);
		stack->size-=64;
	}
	if((firValue->type!=INT&&firValue->type!=DBL)||(secValue->type!=INT&&secValue->type!=DBL))return 1;
	if(firValue->type==DBL||secValue->type==DBL)
	{
		double result=((firValue->type==DBL)?firValue->value.dbl:firValue->value.num)*((secValue->type==DBL)?secValue->value.dbl:secValue->value.num);
		stackvalue* tmpValue=newStackValue(DBL);
		tmpValue->value.dbl=result;
		stack->values[stack->tp-1]=tmpValue;
	}
	else if(firValue->type==INT&&secValue->type==INT)
	{
		int32_t result=firValue->value.num*secValue->value.num;
		stackvalue* tmpValue=newStackValue(INT);
		tmpValue->value.num=result;
		stack->values[stack->tp-1]=tmpValue;
	}
	freeStackValue(firValue);
	freeStackValue(secValue);
	proc->cp+=1;
	return 0;
}

int B_div(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	stackvalue* firValue=getValue(stack,stack->tp-1);
	stackvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	if(stack->size-stack->tp>64)
	{
		stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size-64));
		if(stack->values==NULL)errors(OUTOFMEMORY);
		stack->size-=64;
	}
	if((firValue->type!=INT&&firValue->type!=DBL)||(secValue->type!=INT&&secValue->type!=DBL))return 1;
	double result=((secValue->type==DBL)?secValue->value.dbl:secValue->value.num)/((firValue->type==DBL)?firValue->value.dbl:firValue->value.num);
	stackvalue* tmpValue=newStackValue(DBL);
	tmpValue->value.dbl=result;
	stack->values[stack->tp-1]=tmpValue;
	freeStackValue(firValue);
	freeStackValue(secValue);
	proc->cp+=1;
	return 0;
}

int B_mod(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	stackvalue* firValue=getValue(stack,stack->tp-1);
	stackvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	if(stack->size-stack->tp>64)
	{
		stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size-64));
		if(stack->values==NULL)errors(OUTOFMEMORY);
		stack->size-=64;
	}
	if((firValue->type!=INT&&firValue->type!=DBL)||(secValue->type!=INT&&secValue->type!=DBL))return 1;
	int32_t result=((int32_t)((secValue->type==DBL)?secValue->value.dbl:secValue->value.num))%((int32_t)((firValue->type==DBL)?firValue->value.dbl:firValue->value.num));
	stackvalue* tmpValue=newStackValue(INT);
	tmpValue->value.dbl=result;
	stack->values[stack->tp-1]=tmpValue;
	freeStackValue(firValue);
	freeStackValue(secValue);
	proc->cp+=1;
	return 0;
}

int B_atom(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	stackvalue* topValue=getTopValue(stack);
	if(topValue==NULL||topValue->type!=PAR)
	{
		stackvalue* TRUE=newStackValue(INT);
		TRUE->value.num=1;
		stack->values[stack->tp-1]=TRUE;
	}
	else
	{
		if(topValue->value.par.car==NULL&&topValue->value.par.cdr==NULL)
		{
			stackvalue* TRUE=newStackValue(INT);
			TRUE->value.num=1;
			stack->values[stack->tp-1]=TRUE;
		}
		else
			stack->values[stack->tp-1]=NULL;
	}
	freeStackValue(topValue);
	proc->cp+=1;
	return 0;
}

int B_null(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	stackvalue* topValue=getTopValue(stack);
	if(topValue==NULL||(topValue->type==PAR&&(topValue->value.par.car==NULL&&topValue->value.par.cdr==NULL)))
	{
		stackvalue* TRUE=newStackValue(INT);
		TRUE->value.num=1;
		stack->values[stack->tp-1]=TRUE;
	}
	else stack->values[stack->tp-1]=NULL;
	freeStackValue(topValue);
	proc->cp+=1;
	return 0;
}

int B_init_proc(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	stackvalue* topValue=getTopValue(stack);
	if(topValue->type!=PRC)return 1;
	int32_t boundOfProc=*(int32_t*)(proc->code+proc->cp+1);
	topValue->value.prc->localenv=newVarStack(boundOfProc,1,proc->localenv);
	proc->cp+=5;
	return 0;

}

int B_end_proc(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	stackvalue* tmpValue=getTopValue(stack);
	excode* tmp=exe->curproc;
	varstack* tmpEnv=tmp->localenv;
	if(tmpValue->type==PRC&&tmpValue->value.prc->localenv->prev==tmpEnv)tmpEnv->next=tmpValue->value.prc->localenv;
	tmp->localenv=newVarStack(tmpEnv->bound,1,tmpEnv->prev);
	tmpEnv->inProc=0;
	freeVarStack(tmpEnv);
	exe->curproc=tmp->prev;
	if(tmp->refcount==0)freeExcode(tmp);
	else
	{
		tmp->refcount-=1;
		tmp->cp=0;
		tmp->prev=NULL;
	}
	return 0;
}

int B_set_bp(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	stackvalue* prevBp=newStackValue(INT);
	prevBp->value.num=stack->bp;
	stack->values[stack->tp]=prevBp;
	stack->tp+=1;
	stack->bp=stack->tp;
	proc->cp+=1;
	return 0;
}

int B_res_bp(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	if(stack->tp>stack->bp)return 2;
	stackvalue* prevBp=getTopValue(stack);
	stack->bp=prevBp->value.num;
	freeStackValue(prevBp);
	stack->tp-=1;
	if(stack->size-stack->tp>64)
	{
		stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size-64));
		if(stack->values==NULL)errors(OUTOFMEMORY);
		stack->size-=64;
	}
	proc->cp+=1;
	return 0;
}

int B_invoke(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	stackvalue* tmpValue=getTopValue(stack);
	if(tmpValue->type!=PRC)return 1;
	excode* tmpProc=tmpValue->value.prc;
	tmpProc->prev=exe->curproc;
	exe->curproc=tmpProc;
	free(tmpValue);
	stack->tp-=1;
	if(stack->size-stack->tp>64)
	{
		stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size-64));
		if(stack->values==NULL)errors(OUTOFMEMORY);
		stack->size-=64;
	}
	proc->cp+=1;
	return 0;
}

int B_jump_if_ture(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	stackvalue* tmpValue=getTopValue(stack);
	if(!(tmpValue==NULL||(tmpValue->type==PAR&&tmpValue->value.par.car==NULL&&tmpValue->value.par.cdr==NULL)))
	{
		int32_t where=*(int32_t*)(proc->code+proc->cp+sizeof(char));
		proc->cp+=where;
	}
	proc->cp+=5;
	return 0;
}

int B_jump_if_false(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	stackvalue* tmpValue=getTopValue(stack);
	if(tmpValue==NULL||(tmpValue->type==PAR&&tmpValue->value.par.car==NULL&&tmpValue->value.par.cdr==NULL))
	{
		int32_t where=*(int32_t*)(proc->code+proc->cp+sizeof(char));
		proc->cp+=where;
	}
	proc->cp+=5;
	return 0;
}

int B_jump(fakeVM* exe)
{
	excode* proc=exe->curproc;
	int32_t where=*(int32_t*)(proc->code+proc->cp+sizeof(char));
	proc->cp+=where+5;
	return 0;
}

int B_open(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	filestack* files=exe->files;
	stackvalue* mode=getTopValue(stack);
	stackValue* filename=getValue(stack,stack->tp-2);
	stack->tp-=1;
	if(firValue->type!=STR||secValue->type!=STR)return 1;
	int32_t i=0;
	FILE* file=fopen(filename->value.str,mode->value.str);
	if(file==NULL)i=-1;
	if(i!=-1)
	{
		for(;i<files->size;i++)if(files->files[i]!=NULL)break;
		if(i==size)
		{
			files->files=(FILE**)realloc(files->files,sizeof(FILE*)*(files->size+1));
			files->files[i]=file;
		}
		else files->files=[i]=file;
	}
	stackvalue* countOfFile=newStackValue(INT);
	countOfFile->value.num=(int32_t)i;
	stack->values[stack->tp-1]=countOfFile;
	freeStackValue(filename);
	freeStackValue(mode);
	proc->cp+=1;
	return 0;
}

int B_close(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	filestack* files=exe->files;
	stackvalue* topValue=getTopValue(stack);
	if(topValue->type!=INT)return 1;
	if(topValue->value.num>files->size)return 2;
	if(fclose(files->files[topValue->value.num])==EOF)topValue->value.num=-1;
	else files->files[topValue->value.num]=NULL;
	proc->cp+=1;
	return 0;
}

int B_eq(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	excode* proc=exe->curproc;
	stackvalue* firValue=getTopValue(stack);
	stackvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	if(stackvaluecmp(firValue,secValue))
	{
	}
}

excode* newExcode(byteCode* proc)
{
	excode* tmp=(excode*)malloc(sizeof(excode));
	if(tmp==NULL)errors(OUTOFMEMORY);
	tmp->prev=NULL;
	tmp->localenv=NULL;
	tmp->cp=0;
	tmp->refcount=0;
	if(proc!=NULL)
	{
		tmp->size=proc->size;
		tmp->code=proc->code;
	}
	return tmp;
}

char* copyStr(const char* str)
{
	char* tmp=(char*)malloc(sizeof(char)*(strlen(str)+1));
	if(tmp==NULL)errors(OUTOFMEMORY);
	strcpy(tmp,str);
	return tmp;
}

fakestack* newStack(uint32_t size)
{
	fakestack* tmp=(fakestack*)malloc(sizeof(fakestack));
	if(tmp==NULL)errors(OUTOFMEMORY);
	tmp->size=size;
	tmp->tp=0;
	tmp->bp=0;
	tmp->values=(stackvalue**)malloc(size*sizeof(stackvalue*));
	if(tmp->values==NULL)errors(OUTOFMEMORY);
	return tmp;
}

filestack* newFileStack()
{
	filestack* tmp=(filestack*)malloc(sizeof(filestack));
	if(tmp==NULL)errors(OUTOFMEMORY);
	tmp->size=3;
	tmp->files=(FILE**)malloc(sizeof(FILE*)*3);
	tmp->files[0]=stdin;
	tmp->files[1]=stdout;
	tmp->files[2]=stderr;
	return tmp;
}

stackvalue* newStackValue(valueType type)
{
	stackvalue* tmp=(stackvalue*)malloc(sizeof(stackvalue));
	if(tmp==NULL)errors(OUTOFMEMORY);
	tmp->type=type;
	switch(type)
	{
		case INT:tmp->value.num=0;break;
		case DBL:tmp->value.dbl=0;break;
		case CHR:tmp->value.chr='\0';break;
		case SYM:
		case STR:tmp->value.str=NULL;break;
		case PRC:tmp->value.prc=NULL;break;
		case PAR:tmp->value.par.car=NULL;
				 tmp->value.par.cdr=NULL;
	}
	return tmp;
}

void freeStackValue(stackvalue* obj)
{
	if(obj==NULL)return;
	if(obj->type==STR||obj->type==SYM)free(obj->value.str);
	else if(obj->type==PRC)
	{
		if(obj->value.prc->refcount==0)freeExcode(obj->value.prc);
		else obj->value.prc->refcount-=1;
	}
	else if(obj->type==PAR)
	{
		freeStackValue(obj->value.par.car);
		freeStackValue(obj->value.par.cdr);
	}
	free(obj);
}

stackvalue* copyValue(stackvalue* obj)
{
	if(obj==NULL)return NULL;
	stackvalue* tmp=newStackValue(obj->type);
	if(obj->type==STR||obj->type==SYM)tmp->value.str=copyStr(obj->value.str);
	else if(obj->type==PAR)
	{
		tmp->value.par.car=copyValue(obj->value.par.car);
		tmp->value.par.cdr=copyValue(obj->value.par.cdr);
	}
	else
	{
		tmp->value=obj->value;
		if(obj->type==PRC)obj->value.prc->refcount+=1;
	}
	return tmp;
}

void freeExcode(excode* proc)
{
	varstack* curEnv=proc->localenv;
	varstack* prev=(curEnv->prev!=NULL&&(curEnv->prev->next==NULL||curEnv->prev->next==curEnv))?curEnv->prev:NULL;
	curEnv->inProc=0;
	if(curEnv->next==NULL||curEnv->next->prev!=curEnv)freeVarStack(curEnv);
	curEnv=prev;
	while(curEnv!=NULL)
	{
		varstack* prev=(curEnv->prev!=NULL&&(curEnv->prev->next==NULL||curEnv->prev->next==curEnv))?curEnv->prev:NULL;
		freeVarStack(curEnv);
		curEnv=prev;
	}
	free(proc);
	//printf("Free proc!\n");
}

varstack* newVarStack(int32_t bound,int inProc,varstack* prev)
{
	varstack* tmp=(varstack*)malloc(sizeof(varstack));
	tmp->inProc=inProc;
	tmp->bound=bound;
	tmp->size=0;
	tmp->values=NULL;
	tmp->next=NULL;
	tmp->prev=prev;
	return tmp;
}

void freeVarStack(varstack* obj)
{
	if(obj->inProc==0&&(obj->next==NULL||obj->next->prev!=obj))
	{
		if(obj->values!=NULL)
		{
			stackvalue** tmp=obj->values;
			for(;tmp<obj->values+obj->size;tmp++)freeStackValue(*tmp);
		}
		if(obj->prev!=NULL&&obj->prev->next==obj)obj->prev->next=NULL;
		free(obj);
		//printf("Free env!\n");
	}
}

stackvalue* getTopValue(fakestack* stack)
{
	return stack->values[stack->tp-1];
}

stackvalue* getValue(fakestack* stack,int32_t place)
{
	return stack->values[place];
}

stackvalue* getCar(stackvalue* obj)
{
	if(obj->type!=PAR)return NULL;
	else return obj->value.par.car;
}

stackvalue* getCdr(stackvalue* obj)
{
	if(obj->type!=PAR)return NULL;
	else return obj->value.par.cdr;
}

void printStackValue(stackvalue* objValue)
{
	if(objValue==NULL)
	{
		printf("nil");
		return;
	}
	switch(objValue->type)
	{
		case INT:printf("%d",objValue->value.num);break;
		case DBL:printf("%lf",objValue->value.dbl);break;
		case CHR:printRawChar(objValue->value.chr,stdout);break;
		case SYM:printf("%s",objValue->value.str);break;
		case STR:printRawString(objValue->value.str,stdout);break;
		case PRC:printf("<#proc>");break;
		case PAR:putchar('(');
				 printStackValue(objValue->value.par.car);
				 putchar(',');
				 printStackValue(objValue->value.par.cdr);
				 putchar(')');
				 break;
		default:printf("Bad value!");break;
	}
}
