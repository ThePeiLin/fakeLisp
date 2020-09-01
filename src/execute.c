#include"tool.c"
#include"execute.h"
static int (*ByteCodes[])(executor*,excode*)=
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

executor* newExecutor(byteCode* mainproc,byteCode* procs)
{
	executor* exe=(executor*)malloc(sizeof(executor));
	exe->mainproc=newExcode(mainproc);
	exe->procs=procs;
	exe->stack=newStack(0);
	exe->files=newFileStack();
	return exe;
}

int RunExecute(executor* exe)
{
}

int B_dummy(executor* exe,excode* proc)
{
	fprintf(stderr,"Wrong byte code!\n");
	exit(EXIT_FAILURE);
	return 1;
}

int B_push_nil(executor* exe,excode* proc)
{
	fakestack* stack=exe->stack;
	if(stack->tp>=stack->size)stack->values=(stackvalue*)realloc(stack->values,sizeof(stackvalue)*(stack->size+64));
	if(stack->values==NULL)errors(OUTOFMEMORY);
	stack->size+=64;
	stack->values[stack->tp].type=NIL;
	stack->values[stack->tp].value.obj=NULL;
	stack->tp+=1;
	proc->cp+=1;
	return 0;
}

int B_push_pair(executor* exe,excode* proc)
{
	fakepair* tmpair=newFakePair(NULL,NULL);
	fakestack* stack=exe->stack;
	if(stack->tp>=stack->size)stack->values=(stackvalue*)realloc(stack->values,sizeof(stackvalue)*(stack->size+64));
	if(stack->values==NULL)errors(OUTOFMEMORY);
	stack->size+=64;
	stack->values[stack->tp].type=PAR;
	stack->values[stack->tp].value.obj=tmpair;
	stack->tp+=1;
	proc->cp+=1;
	return 0;
}

int B_push_int(executor* exe,excode* proc)
{
	fakestack* stack=exe->stack;
	if(stack->tp>=stack->size)stack->values=(stackvalue*)realloc(stack->values,sizeof(stackvalue)*(stack->size+64));
	if(stack->values==NULL)errors(OUTOFMEMORY);
	stack->size+=64;
	stack->values[stack->tp].type=INT;
	stack->values[stack->tp].value.num=*(int32_t*)(proc->code+proc->cp+1);
	stack->tp+=1;
	proc->cp+=5;
	return 0;
}

int B_push_chr(executor* exe,excode* proc)
{
	fakestack* stack=exe->stack;
	if(stack->tp>=stack->size)stack->values=(stackvalue*)realloc(stack->values,sizeof(stackvalue)*(stack->size+64));
	if(stack->values==NULL)errors(OUTOFMEMORY);
	stack->size+=64;
	stack->values[stack->tp].type=CHR;
	stack->values[stack->tp].value.chr=*(proc->code+proc->cp+1);
	stack->tp+=1;
	proc->cp+=2;
	return 0;
}

int B_push_str(executor* exe,excode* proc)
{
	int len=strlen((char*)(proc->code+proc->cp+1));
	char* tmpStr=(char*)malloc(sizeof(char)*(len+1));
	if(tmpStr==NULL)errors(OUTOFMEMORY);
	strcpy(tmpStr,proc->code+proc->cp+1);
	fakestack* stack=exe->stack;
	if(stack->tp>=stack->size)stack->values=(stackvalue*)realloc(stack->values,sizeof(stackvalue)*(stack->size+64));
	if(stack->values==NULL)errors(OUTOFMEMORY);
	stack->size+=64;
	stack->values[stack->tp].type=STR;
	stack->values[stack->tp].value.obj=tmpStr;
	stack->tp+=1;
	proc->cp+=2+len;
	return 0;
}

int B_push_sym(executor* exe,excode* proc)
{
	int len=strlen((char*)(proc->code+proc->cp+1));
	char* tmpStr=(char*)malloc(sizeof(char)*(len+1));
	if(tmpStr==NULL)errors(OUTOFMEMORY);
	strcpy(tmpStr,proc->code+proc->cp+1);
	fakestack* stack=exe->stack;
	if(stack->tp>=stack->size)stack->values=(stackvalue*)realloc(stack->values,sizeof(stackvalue)*(stack->size+64));
	if(stack->values==NULL)errors(OUTOFMEMORY);
	stack->size+=64;
	stack->values[stack->tp].type=SYM;
	stack->values[stack->tp].value.obj=tmpStr;
	stack->tp+=1;
	proc->cp+=2+len;
	return 0;
}

int B_push_var(executor* exe,excode* proc)
{
	fakestack* stack=exe->stack;
	if(stack->tp>=stack->size)stack->values=(stackvalue*)realloc(stack->values,sizeof(stackvalue)*(stack->size+64));
	if(stack->values==NULL)errors(OUTOFMEMORY);
	stack->size+=64;
	int32_t countOfVar=*(int32_t*)(proc->code+proc->cp+1);
	varstack* curEnv=proc->localenv;
	while(countOfVar<curEnv->bound)curEnv=curEnv->prev;
	stackvalue* tmpValue=copyValue(curEnv->values+countOfVar-(curEnv->bound));
	stack->values[stack->tp]=*tmpValue;
	free(tmpValue);
	stack->tp+=1;
	proc->cp+=5;
	return 0;
}

int B_push_car(executor* exe,excode* proc)
{
	fakestack* stack=exe->stack;
	stackvalue* objValue=getTopValue(stack);
	if(objValue->type!=PAR)return 1;
	else
	{
		stackvalue* tmpValue=copyValue(getCar(objValue));
		stack->values[stack->tp-1]=*tmpValue;
		free(tmpValue);
		clearStackValue(objValue);
	}
	proc->cp+=1;
	return 0;
}

int B_push_cdr(executor* exe,excode* proc)
{
	fakestack* stack=exe->stack;
	stackvalue* objValue=getTopValue(stack);
	if(objValue->type!=PAR)return 1;
	else
	{
		stackvalue* tmpValue=copyValue(getCdr(objValue));
		stack->values[stack->tp-1]=*tmpValue;
		free(tmpValue);
		clearStackValue(objValue);
	}
	proc->cp+=1;
	return 0;
}

int B_push_proc(executor* exe,excode* proc)
{
	fakestack* stack=exe->stack;
	int32_t countOfProc=*(int32_t*)(proc->code+proc->cp+1);
	if(stack->tp>=stack->size)stack->values=(stackvalue*)realloc(stack->values,sizeof(stackvalue)*(stack->size+64));
	if(stack->values==NULL)errors(OUTOFMEMORY);
	stack->size+=64;
	stack->values[stack->tp].type=PRC;
	stack->values[stack->tp].value.obj=newExcode(exe->procs+countOfProc);
	stack->tp+=1;
	proc->cp+=5;
	return 0;
}

int B_pop(executor* exe,excode* proc)
{
	fakestack* stack=exe->stack;
	clearStackValue(getTopValue(stack));
	stack->tp-=1;
	if(stack->size-stack->tp>64)stack->values=(stackvalue*)realloc(stack->values,sizeof(stackvalue)*(stack->size-64));
	if(stack->values==NULL)errors(OUTOFMEMORY);
	stack->size-=64;
	proc->cp+=1;
	return 0;
}

int B_pop_var(executor* exe,excode* proc)
{
	fakestack* stack=exe->stack;
	int32_t countOfVar=*(int32_t*)(proc->code+proc->cp+1);
	varstack* curEnv=proc->localenv;
	while(countOfVar<curEnv->bound)curEnv=curEnv->prev;
	stackvalue* tmpValue=curEnv->values+countOfVar-(curEnv->bound);
	*tmpValue=*getTopValue(stack);
	stack->tp-=1;
	if(stack->size-stack->tp>64)stack->values=(stackvalue*)realloc(stack->values,sizeof(stackvalue)*(stack->size-64));
	if(stack->values==NULL)errors(OUTOFMEMORY);
	stack->size-=64;
	proc->cp+=5;
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
	return stack->values+stack->tp-1;
}

stackvalue* getValue(fakestack* stack,int32_t place)
{
	return stack->values+place;
}

stackvalue* getCar(stackvalue* obj)
{
	if(obj->type!=PAR)return NULL;
	else return ((fakepair*)obj->value.obj)->car;
}

stackvalue* getCdr(stackvalue* obj)
{
	if(obj->type!=PAR)return NULL;
	else return ((fakepair*)obj->value.obj)->cdr;
}
