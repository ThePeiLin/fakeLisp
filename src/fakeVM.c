#include"tool.c"
#include"fakeVM.h"
static int (*ByteCodes[])(fakeVM*,excode*)=
{
	B_dummy,
	B_push_nil,
	B_push_pair,
	B_push_int,
	B_push_chr,
	B_push_str,
	B_push_sym,
	B_push_var,
	B_push_car,
	B_push_cdr,
	B_push_proc,
	B_pop,
	B_pop_var,
	B_pop_rest_var,
	B_pop_car,
	B_pop_cdr,
	B_add,
	B_sub,
	B_mul,
	B_div,
	B_mod,
	B_atom,
	B_null,
	B_init_proc,
	B_end_proc,
	B_set_bp,
	B_res_bp,
	B_open,
	B_close,
	B_eq,
	B_ne,
	B_gt,
	B_lt,
	B_le,
	B_not,
	B_jump_if_ture,
	B_jump_if_false,
	B_in,
	B_out,
	B_go,
	B_run,
	B_send,
	B_accept
};

fakeVM* newExecutor(byteCode* mainproc,byteCode* procs)
{
	fakeVM* exe=(fakeVM*)malloc(sizeof(fakeVM));
	exe->mainproc=newExcode(mainproc);
	exe->procs=procs;
	exe->stack=newStack(0);
	exe->files=newFileStack();
	return exe;
}

int RunExecute(fakeVM* exe)
{
}

int B_dummy(fakeVM* exe,excode* proc)
{
	fprintf(stderr,"Wrong byte code!\n");
	exit(EXIT_FAILURE);
	return 1;
}

int B_push_nil(fakeVM* exe,excode* proc)
{
	fakestack* stack=exe->stack;
	if(stack->tp>=stack->size)stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size+64));
	if(stack->values==NULL)errors(OUTOFMEMORY);
	stack->size+=64;
	stack->values[stack->tp]=NULL;
	stack->tp+=1;
	proc->cp+=1;
	return 0;
}

int B_push_pair(fakeVM* exe,excode* proc)
{
	fakepair* tmpair=newFakePair(NULL,NULL);
	fakestack* stack=exe->stack;
	if(stack->tp>=stack->size)stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size+64));
	if(stack->values==NULL)errors(OUTOFMEMORY);
	stack->size+=64;
	stackvalue* objValue=newStackValue(PAR);
	objValue->value.par.car=NULL;
	objValue->value.par.cdr=NULL;
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=1;
	return 0;
}

int B_push_int(fakeVM* exe,excode* proc)
{
	fakestack* stack=exe->stack;
	if(stack->tp>=stack->size)stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size+64));
	if(stack->values==NULL)errors(OUTOFMEMORY);
	stack->size+=64;
	stackvalue* objValue=newStackValue(INT);
	objValue->value.num=*(int32_t*)(proc->code+proc->cp+1);
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=5;
	return 0;
}

int B_push_chr(fakeVM* exe,excode* proc)
{
	fakestack* stack=exe->stack;
	if(stack->tp>=stack->size)stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size+64));
	if(stack->values==NULL)errors(OUTOFMEMORY);
	stack->size+=64;
	stackvalue* objValue=newStackValue(CHR);
	objValue->value.num=*(proc->code+proc->cp+1);
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=2;
	return 0;
}

int B_push_dbl(fakeVM* exe,excode* proc)
{
	fakestack* stack=exe->stack;
	if(stack->tp>=stack->size)stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size+64));
	if(stack->values==NULL)errors(OUTOFMEMORY);
	stack->size+=64;
	stackvalue* objValue=newStackValue(DBL);
	objValue->value.num=*(double*)(proc->code+proc->cp+1);
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=9;
	return 0;
}

int B_push_str(fakeVM* exe,excode* proc)
{
	int len=strlen((char*)(proc->code+proc->cp+1));
	char* tmpStr=(char*)malloc(sizeof(char)*(len+1));
	if(tmpStr==NULL)errors(OUTOFMEMORY);
	strcpy(tmpStr,proc->code+proc->cp+1);
	fakestack* stack=exe->stack;
	if(stack->tp>=stack->size)stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size+64));
	if(stack->values==NULL)errors(OUTOFMEMORY);
	stack->size+=64;
	stackvalue* objValue=newStackValue(STR);
	objValue->value.str=tmpStr;
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=2+len;
	return 0;
}

int B_push_sym(fakeVM* exe,excode* proc)
{
	int len=strlen((char*)(proc->code+proc->cp+1));
	char* tmpStr=(char*)malloc(sizeof(char)*(len+1));
	if(tmpStr==NULL)errors(OUTOFMEMORY);
	strcpy(tmpStr,proc->code+proc->cp+1);
	fakestack* stack=exe->stack;
	if(stack->tp>=stack->size)stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size+64));
	if(stack->values==NULL)errors(OUTOFMEMORY);
	stack->size+=64;
	stackvalue* objValue=newStackValue(SYM);
	objValue->value.str=tmpStr;
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=2+len;
	return 0;
}

int B_push_var(fakeVM* exe,excode* proc)
{
	fakestack* stack=exe->stack;
	if(stack->tp>=stack->size)stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size+64));
	if(stack->values==NULL)errors(OUTOFMEMORY);
	stack->size+=64;
	int32_t countOfVar=*(int32_t*)(proc->code+proc->cp+1);
	varstack* curEnv=proc->localenv;
	while(countOfVar<curEnv->bound)curEnv=curEnv->prev;
	stackvalue* tmpValue=copyValue(*(curEnv->values+countOfVar-(curEnv->bound)));
	stack->values[stack->tp]=tmpValue;
	stack->tp+=1;
	proc->cp+=5;
	return 0;
}

int B_push_car(fakeVM* exe,excode* proc)
{
	fakestack* stack=exe->stack;
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

int B_push_cdr(fakeVM* exe,excode* proc)
{
	fakestack* stack=exe->stack;
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

int B_push_proc(fakeVM* exe,excode* proc)
{
	fakestack* stack=exe->stack;
	int32_t countOfProc=*(int32_t*)(proc->code+proc->cp+1);
	if(stack->tp>=stack->size)stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size+64));
	if(stack->values==NULL)errors(OUTOFMEMORY);
	stack->size+=64;
	stackvalue* objValue=newStackValue(PRC);
	objValue->value.prc=newExcode(exe->procs+countOfProc);
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=5;
	return 0;
}

int B_pop(fakeVM* exe,excode* proc)
{
	fakestack* stack=exe->stack;
	freeStackValue(getTopValue(stack));
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

int B_pop_var(fakeVM* exe,excode* proc)
{
	fakestack* stack=exe->stack;
	int32_t countOfVar=*(int32_t*)(proc->code+proc->cp+1);
	varstack* curEnv=proc->localenv;
	while(countOfVar<curEnv->bound)curEnv=curEnv->prev;
	if(countOfVar-curEnv->bound>=curEnv->size)curEnv->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size+64));
	curEnv->size+=64;
	stackvalue** pValue=curEnv->values+countOfVar-(curEnv->bound);
	freeStackValue(*pValue);
	*pValue=getTopValue(stack);
	stack->tp-=1;
	if(stack->size-stack->tp>64)
	{
		stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size-64));
		if(stack->values==NULL)errors(OUTOFMEMORY);
		stack->size-=64;
	}
	proc->cp+=5;
	return 0;
}

int B_pop_rest_var(fakeVM* exe,excode* proc)
{
	fakestack* stack=exe->stack;
	int32_t countOfVar=*(int32_t*)(proc->code+proc->cp+1);
	varstack* curEnv=proc->localenv;
	while(countOfVar<curEnv->bound)curEnv=curEnv->prev;
	if(countOfVar-curEnv->bound>=curEnv->size)curEnv->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size+64));
	curEnv->size+=64;
	stackvalue** tmpValue=curEnv->values+countOfVar-(curEnv->bound);
	freeStackValue(*tmpValue);
	stackvalue* obj=newStackValue(PAR);
	stackvalue* tmp=obj;
	for(;;)
	{
		stackvalue* topValue=getTopValue(stack);
		tmp->value.par.car=topValue;
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

int B_pop_car(fakeVM* exe,excode* proc)
{
	fakestack* stack=exe->stack;
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

int B_pop_cdr(fakeVM* exe,excode* proc)
{
	fakestack* stack=exe->stack;
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

int B_add(fakeVM* exe,excode* proc)
{
	fakestack* stack=exe->stack;
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
		stack->values[stack->tp]=tmpValue;
	}
	else if(firValue->type==INT&&secValue->type==INT)
	{
		int32_t result=firValue->value.num+secValue->value.num;
		stackvalue* tmpValue=newStackValue(INT);
		tmpValue->value.num=result;
		stack->values[stack->tp]=tmpValue;
	}
	freeStackValue(firValue);
	freeStackValue(secValue);
	return 0;
}

int B_sub(fakeVM* exe,excode* proc)
{
	fakestack* stack=exe->stack;
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
		double result=((firValue->type==DBL)?firValue->value.dbl:firValue->value.num)-((secValue->type==DBL)?secValue->value.dbl:secValue->value.num);
		stackvalue* tmpValue=newStackValue(DBL);
		tmpValue->value.dbl=result;
		stack->values[stack->tp]=tmpValue;
	}
	else if(firValue->type==INT&&secValue->type==INT)
	{
		int32_t result=firValue->value.num-secValue->value.num;
		stackvalue* tmpValue=newStackValue(INT);
		tmpValue->value.num=result;
		stack->values[stack->tp]=tmpValue;
	}
	freeStackValue(firValue);
	freeStackValue(secValue);
	return 0;
}

int B_mul(fakeVM* exe,excode* proc)
{
	fakestack* stack=exe->stack;
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
		stack->values[stack->tp]=tmpValue;
	}
	else if(firValue->type==INT&&secValue->type==INT)
	{
		int32_t result=firValue->value.num*secValue->value.num;
		stackvalue* tmpValue=newStackValue(INT);
		tmpValue->value.num=result;
		stack->values[stack->tp]=tmpValue;
	}
	freeStackValue(firValue);
	freeStackValue(secValue);
	return 0;
}

int B_div(fakeVM* exe,excode* proc)
{
	fakestack* stack=exe->stack;
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
	double result=((firValue->type==DBL)?firValue->value.dbl:firValue->value.num)/((secValue->type==DBL)?secValue->value.dbl:secValue->value.num);
	stackvalue* tmpValue=newStackValue(DBL);
	tmpValue->value.dbl=result;
	stack->values[stack->tp]=tmpValue;
	freeStackValue(firValue);
	freeStackValue(secValue);
	return 0;
}

int B_mod(fakeVM* exe,excode* proc)
{
	fakestack* stack=exe->stack;
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
	int32_t result=((int32_t)((firValue->type==DBL)?firValue->value.dbl:firValue->value.num))%((int32_t)((secValue->type==DBL)?secValue->value.dbl:secValue->value.num));
	stackvalue* tmpValue=newStackValue(INT);
	tmpValue->value.dbl=result;
	stack->values[stack->tp]=tmpValue;
	freeStackValue(firValue);
	freeStackValue(secValue);
	return 0;
}

excode* newExcode(byteCode* proc)
{
	excode* tmp=(excode*)malloc(sizeof(excode));
	if(tmp==NULL)errors(OUTOFMEMORY);
	tmp->prev=NULL;
	tmp->localenv=NULL;
	tmp->cp=0;
	tmp->refcount=0;
	tmp->size=proc->size;
	tmp->code=proc->code;
	return tmp;
}

char* copyStr(const char* str)
{
	char* tmp=(char*)malloc(sizeof(char)*(strlen(str)+1));
	if(tmp==NULL)errors(OUTOFMEMORY);
	strcpy(tmp,str);
	return tmp;
}

stackvalue* getTopValue(fakestack* stack)
{
	return *(stack->values+stack->tp-1);
}

stackvalue* getValue(fakestack* stack,int32_t place)
{
	return *(stack->values+place);
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
