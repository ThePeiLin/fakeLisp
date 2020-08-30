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
	exe->mainproc=newExCode(mainproc);
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
	stackvalue* topValue=getTopValue(stack);
	if(substack->values[tmpsubvalue->value.par.car].type==PAR)
	{
		int32_t start=tmpsubvalue->value.par.car;
		int32_t end=(start<tmpsubvalue->value.par.cdr)?tmpsubvalue->value.par.cdr:substack->tp;
		fakesubstack* tmpsubstack=copyPair(substack,start,end);
		freeSubStack(substack);
		topValue->value.obj=tmpsubstack;
		return 0;
	}
	else
	{
		topValue->type=substack->values[tmpsubvalue->value.par.car].type;
		substackvalue* tmpsubstackvalue=copySubValue(substack->values+tmpsubvalue->value.par.car);
		switch(topValue->type)
		{
			case STR:
			case SYM:
			case PRC:
				topValue->value.obj=tmpsubstackvalue->value.obj;break;
			case CHR:
				topValue->value.chr=tmpsubstackvalue->value.chr;break;
			case INT:
				topValue->value.num=tmpsubstackvalue->value.num;break;
			case DBL:
				topValue->value.dbl=tmpsubstackvalue->value.dbl;break;
		}
	}
	return 0;
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
	stackvalue* topValue=getTopValue(stack);
	if(substack->values[tmpsubvalue->value.par.cdr].type==PAR)
	{
		int32_t start=tmpsubvalue->value.par.cdr;
		int32_t end=(start<tmpsubvalue->value.par.car)?tmpsubvalue->value.par.car:substack->tp;
		fakesubstack* tmpsubstack=copyPair(substack,start,end);
		freeSubStack(substack);
		topValue->value.obj=tmpsubstack;
		return 0;
	}
	else
	{
		topValue->type=substack->values[tmpsubvalue->value.par.cdr].type;
		substackvalue* tmpsubstackvalue=copySubValue(substack->values+tmpsubvalue->value.par.cdr);
		switch(topValue->type)
		{
			case STR:
			case SYM:
			case PRC:
				topValue->value.obj=tmpsubstackvalue->value.obj;break;
			case CHR:
				topValue->value.chr=tmpsubstackvalue->value.chr;break;
			case INT:
				topValue->value.num=tmpsubstackvalue->value.num;break;
			case DBL:
				topValue->value.dbl=tmpsubstackvalue->value.dbl;break;
		}
	}
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
	if(stack->size-(stack->tp-1)>64)stack->values=(stackvalue*)realloc(stack->values,sizeof(stackvalue)*(stack->size-64));
	if(stack->values==NULL)errors(OUTOFMEMORY);
	stack->size-=64;
	stackvalue* topValue=getTopValue(stack);
	switch(topValue->type)
	{
		case PAR:freesubstack(topValue->value.obj);break;
		case STR:
		case SYM:free(topValue->value.obj);break;
	}
	stack->tp-=1;
	proc->cp+=1;
	return 0;
}

int B_pop_var(executor* exe,excode* proc)
{
	fakestack* stack=exe->stack;
	if(stack->size-(stack->tp-1)>64)stack->values=(stackvalue*)realloc(stack->values,sizeof(stackvalue)*(stack->size-64));
	if(stack->values==NULL)errors(OUTOFMEMORY);
	stack->size-=64;
	int32_t countOfVar=*(int32_t*)(proc->code+proc->cp+1);
	varstack* curEnv=proc->localenv;
	while(countOfVar<curEnv->bound)curEnv=curEnv->prev;
	stackvalue* tmpValue=curEnv->values+countOfVar-(curEnv->bound);
	stackvalue* topValue=getTopValue(stack);
	switch(topValue->type)
	{
		case PAR:
			tmpValue->type=PAR;
			freeSubStack(tmpValue->value.obj);
			tmpValue->value.obj=copyPair(topValue->value.obj,0,((fakesubstack*)topValue->value.obj)->tp);
			freeSubStack(topValue->value.obj);
			break;
		case STR:
		case SYM:
			tmpValue->type=topValue->type;
			free(tmpValue->value.obj);
			tmpValue->value.obj=copyStr(topValue->value.obj);
			free(topValue->value.obj);
			break;
		case PRC:
			tmpValue->type=topValue->type;
			((excode*)tmpValue->value.obj)->refcount-=1;
			tmpValue->value=topValue->value;
			break;
		default:
			tmpValue->type=topValue->type;
			tmpValue->value=topValue->value;
			break;
	}
	stack->tp-=1;
	proc->cp+=5;
	return 0;
}

int B_pop_rest_var(executor* exe,excode* proc)
{
	fakestack* stack=exe->stack;
	int32_t size=0;
	int32_t tmpTp=stack->tp;
	for(;tmpTp>stack->bp;tmpTp--)
	{
		stackvalue* tmpValue=getValue(stack,stack->tp);
		if(tmpValue->type!=PAR)size+=1;
		else
			size+=((fakesubstack*)tmpValue->value.obj)->size;
	}
	fakesubstack* substack=newSubStack(size);
	int32_t prevPair=-1;
	while(stack->tp>stack->bp)
	{
		stackvalue* topValue=getTopValue(stack);
		prevPair=writeWithPair(topValue,substack,prevPair);
		switch(topValue->type)
		{
			case PAR:freesubstack(topValue->value.obj);break;
			case STR:
			case SYM:free(topValue->value.obj);break;
		}
		stack->tp-=1;
		if(stack->size-(stack->tp-1)>64)stack->values=(stackvalue*)realloc(stack->values,sizeof(stackvalue)*(stack->size-64));
		if(stack->values==NULL)errors(OUTOFMEMORY);
		stack->size-=64;
	}
	int32_t countOfVar=*(int32_t*)(proc->code+proc->cp+1);
	while(countOfVar<curEnv->bound)curEnv=curEnv->prev;
	stackvalue* tmpValue=curEnv->values+countOfVar-(curEnv->bound);
	switch(tmpValue->type)
	{
		case PAR:freeSubStack(tmpValue->value.obj);break;
		case PRC:((excode*)tmpValue->value.obj)->refcount-=1;break;
		case STR:
		case SYM:free(tmpValue->value.obj);break;
	}
	tmpValue->type=PAR;
	tmpValue->value.obj=substack;
	proc->cp+=5;
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
	if(end==-1)end=obj->tp;
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

substackvalue* getSubValue(fakesubstack* substack,int32_t place)
{
	return substack->values+place;
}

int32_t writeWithPair(stackvalue* obj,fakesubstack* substack,int32_t prev)
{
	int32_t newPrev=substack->tp;
	if(prev!=-1)
	{
		substackvalue* prevPair=getSubValue(substack,prev);
		prevPair->value.par.cdr=substack->tp;
	}
	substackvalue* newPair=getSubValue(substack,substack->tp);
	substack->tp+=1;
	newPair->value.par.car=substack->tp;
	newPair->value.par.cdr=0;
	substackvalue* tmpValue=getSubValue(substack,substack->tp);
	tmpValue->type=obj->type;
	switch(obj->type)
	{
		case STR:
		case SYM:
			tmpValue->value.obj=copyStr(obj->value.obj);
			substack->tp+=1;
			break;
		case INT:
			tmpValue->value.num=obj->value.num;
			substack->tp+=1;
			break;
		case DBL:
			tmpValue->value.dbl=obj->value.dbl;
			substack->tp+=1;
			break;
		case CHR:
			tmpValue->value.chr=obj->value.chr;
			substack->tp+=1;
			break;
		case PRC:
			tmpValue->value.obj=obj->value.obj;
			substack->tp+=1;
			break;
		case PAR:
			memcpy(tmpValue,copyPair(obj->value.obj,0,-1),((fakesubstack*)obj->value.obj)->size*sizeof(substackvalue));
			substack->tp+=((fakesubstack*)obj->value.obj)->size;
			break;
	}
	return newPrev;
}
