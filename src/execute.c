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
	fakesubstack* substack=newSubStack(1);
	substack->values[0].type=PAR;
	substack->values[0].value.par.car=0;
	substack->values[0].value.par.cdr=0;
	substack->tp+=1;
	fakestack* stack=exe->stack;
	if(stack->tp>=stack->size)stack->values=(stackvalue*)realloc(stack->values,sizeof(stackvalue)*(stack->size+64));
	if(stack->values==NULL)errors(OUTOFMEMORY);
	stack->size+=64;
	stack->values[stack->tp].type=PAR;
	stack->values[stack->tp].value.obj=substack;
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
	if(stack->tp>=stack->size)stack->values=(stackvalue*)realloc(stack->values,sizeof(stackvalue)*(stack->size+64));
	if(stack->values==NULL)errors(OUTOFMEMORY);
	stack->size+=64;
	fakesubstack* substack=stack->values[stack->tp-1].value.obj;
	substackvalue* tmpsubvalue=substack->values;
	if(tmpsubvalue->type!=PAR)return 1;
	if(substack->values[tmpsubvalue->value.par.car].type==PAR)
	{
		int32_t start=tmpsubvalue->value.par.car;
		int32_t end=(start<tmpsubvalue->value.par.cdr)?tmpsubvalue->value.par.cdr:substack->tp;
		fakesubstack* tmpsubstack=copyPair(substack,start,end);
		freeSubStack(substack);
		stack->values[stack->tp-1].value.obj=tmpsubstack;
		return 0;
	}
	else
	{
		stack->values[stack->tp-1].type=substack->values[tmpsubvalue->value.par.car].type;
		substackvalue* tmpsubstackvalue=copySubValue(substack->values+tmpsubvalue->value.par.car);
		if(stack->values[stack->tp-1].type==STR||stack->values[stack->tp-1].type==SYM||stack->values[stack->tp-1].type==PRC)
			stack->values[stack->tp-1].value.obj=tmpsubstackvalue->value.obj;
		else if(stack->values[stack->tp-1].type==CHR)
			stack->values[stack->tp-1].value.chr=tmpsubstackvalue->value.chr;
		else if(stack->values[stack->tp-1].type==INT)
			stack->values[stack->tp-1].value.num=tmpsubstackvalue->value.num;
		else if(stack->values[stack->tp-1].type==DBL)
			stack->values[stack->tp-1].value.dbl=tmpsubstackvalue->value.dbl;
		return 0;
	}
}

int B_push_cdr(executor* exe,excode* proc)
{
	fakestack* stack=exe->stack;
	if(stack->tp>=stack->size)stack->values=(stackvalue*)realloc(stack->values,sizeof(stackvalue)*(stack->size+64));
	if(stack->values==NULL)errors(OUTOFMEMORY);
	stack->size+=64;
	fakesubstack* substack=stack->values[stack->tp-1].value.obj;
	substackvalue* tmpsubvalue=substack->values;
	if(tmpsubvalue->type!=PAR)return 1;
	if(substack->values[tmpsubvalue->value.par.cdr].type==PAR)
	{
		int32_t start=tmpsubvalue->value.par.cdr;
		int32_t end=(start<tmpsubvalue->value.par.car)?tmpsubvalue->value.par.car:substack->tp;
		fakesubstack* tmpsubstack=copyPair(substack,start,end);
		freeSubStack(substack);
		stack->values[stack->tp-1].value.obj=tmpsubstack;
		return 0;
	}
	else
	{
		stack->values[stack->tp-1].type=substack->values[tmpsubvalue->value.par.cdr].type;
		substackvalue* tmpsubstackvalue=copySubValue(substack->values+tmpsubvalue->value.par.cdr);
		if(stack->values[stack->tp-1].type==STR||stack->values[stack->tp-1].type==SYM||stack->values[stack->tp-1].type==PRC)
			stack->values[stack->tp-1].value.obj=tmpsubstackvalue->value.obj;
		else if(stack->values[stack->tp-1].type==CHR)
			stack->values[stack->tp-1].value.chr=tmpsubstackvalue->value.chr;
		else if(stack->values[stack->tp-1].type==INT)
			stack->values[stack->tp-1].value.num=tmpsubstackvalue->value.num;
		else if(stack->values[stack->tp-1].type==DBL)
			stack->values[stack->tp-1].value.dbl=tmpsubstackvalue->value.dbl;
		return 0;
	}
}

int B_push_proc(executor* exe,excode* proc)
{
	fakestack* stack=exe->stack;
	int32_t countOfProc=*(int32_t*)(proc->code+proc->cp+1);
	if(stack->tp>=stack->size)stack->values=(stackvalue*)realloc(stack->values,sizeof(stackvalue)*(stack->size+64));
	if(stack->values==NULL)errors(OUTOFMEMORY);
	stack->size+=64;
	stack->values[stack->tp].type=PRC;
	stack->values[stack->tp].value.obj=newExcode(exe->procs[countOfProc]);
	stack->tp+=1;
	proc->cp+=5;
	return 0;

}

int B_pop(executor* exe,excode* proc)
{
	fakestack* stack=exe->stack;
	if(stack->size-(stack->tp-1)>64)stack->values=(stackvalue*)realloc(stack->values,sizeof(stackvalue)*(stack->size-64));
	if(stack->values==NULL)errors(OUTOFMEMORY);
	stack->size-=64;
	switch(stack->values[stack->tp-1].type)
	{
		case PAR:freeSubStack(stack->values[stack->tp-1].value.obj);break;
		case STR:
		case SYM:freeSubStack(stack->values[stack->tp-1].value.obj);break;
	}
	stack->tp-=1;
	proc->cp+=1;
	return 0;
}

fakesubstack* newSubStack(uint32_t size)
{
	fakesubstack* tmp=(fakesubstack*)malloc(sizeof(fakesubstack));
	if(tmp==NULL)errors(OUTOFMEMORY);
	tmp->tp=0;
	tmp->size=size;
	tmp->values=(substackvalue*)malloc(sizeof(substackvalue)*size);
	if(tmp->values==NULL)errors(OUTOFMEMORY);
	return tmp;
}

substackvalue* copySubValue(substackvalue* obj)
{
	substackvalue* tmp=(substackvalue*)malloc(sizeof(substackvalue));
	if(tmp==NULL)errors(OUTOFMEMORY);
	if(obj->type==STR||obj->type==SYM)
	{
		tmp->type=obj->type;
		tmp->value.obj=(char*)malloc(sizeof(char)*(strlen(obj->value.obj)+1));
		if(tmp->value.obj==NULL)errors(OUTOFMEMORY);
		strcpy(tmp->value.obj,obj->value.obj);
	}
	else
	{
		tmp->type=obj->type;
		tmp->value=obj->value;
	}
	return tmp;
}

stackvalue* copyValue(stackvalue* obj)
{
	stackvalue* tmp=(stackvalue*)malloc(sizeof(stackvalue));
	if(tmp==NULL)errors(OUTOFMEMORY);
	if(obj->type==STR||obj->type==SYM)
	{
		tmp->type=obj->type;
		tmp->value.obj=(char*)malloc(sizeof(char)*(strlen(obj->value.obj)+1));
		if(tmp->value.obj==NULL)errors(OUTOFMEMORY);
		strcpy(tmp->value.obj,obj->value.obj);
	}
	else if(obj->type==PAR)
	{
		tmp->type=PAR;
		tmp->value.obj=newSubStack(((fakesubstack*)obj->value.obj)->size);
		fakesubstack* firsubstack=tmp->value.obj;
		fakesubstack* secsubstack=obj->value.obj;
		while(firsubstack->tp<firsubstack->size)
		{
			substackvalue* tmpValue=copySubValue(secsubstack->values+firsubstack->tp);
			firsubstack->values[firsubstack->tp]=*tmpValue;
			free(tmpValue);
			firsubstack->tp+=1;
		}
	}
	else
	{
		tmp->type=obj->type;
		tmp->value=obj->value;
	}
	return tmp;
}

fakesubstack* copyPair(fakesubstack* obj,int32_t start,int32_t end)
{
	fakesubstack* tmp=newSubStack(end-start);
	while(tmp->tp<tmp->size)
	{
		substackvalue* tmpValue=copySubValue(obj->values+start+tmp->tp);
		tmp->values[tmp->tp]=*tmpValue;
		free(tmpValue);
		tmp->tp+=1;
	}
	return tmp;
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
