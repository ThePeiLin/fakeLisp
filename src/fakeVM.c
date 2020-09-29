#include"tool.h"
#include"fakeVM.h"
#include"opcode.h"
#include<string.h>
#include<math.h>
#include<termios.h>
#include<unistd.h>
#define NUMOFBUILTINSYMBOL 46
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
	B_cast_to_chr,
	B_cast_to_int,
	B_cast_to_dbl,
	B_cast_to_str,
	B_cast_to_sym,
	B_is_chr,
	B_is_int,
	B_is_dbl,
	B_is_str,
	B_is_sym,
	B_is_prc,
	B_nth,
	B_length,
	B_append,
	B_str_cat,
	B_open,
	B_close,
	B_eq,
	B_gt,
	B_ge,
	B_lt,
	B_le,
	B_not,
	B_getc,
	B_getch,
	B_ungetc,
	B_read,
	B_write,
	B_tell,
	B_seek,
	B_rewind,/*
	B_go,
	B_wait,
	B_send,
	B_accept*/
};

byteCode P_cons=
{
	25,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_PAIR,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_POP_CAR,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_POP_CDR,
		FAKE_END_PROC
	}
};

byteCode P_car=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_CAR,
		FAKE_END_PROC
	}
};

byteCode P_cdr=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_CDR,
		FAKE_END_PROC
	}
};

byteCode P_atom=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_ATOM,
		FAKE_END_PROC
	}
};

byteCode P_null=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_NULL,
		FAKE_END_PROC
	}
};

byteCode P_isint=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_IS_INT,
		FAKE_END_PROC
	}
};

byteCode P_ischr=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_IS_CHR,
		FAKE_END_PROC
	}
};

byteCode P_isdbl=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_IS_DBL,
		FAKE_END_PROC
	}
};

byteCode P_isstr=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_IS_STR,
		FAKE_END_PROC
	}
};

byteCode P_issym=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_IS_SYM,
		FAKE_END_PROC
	}
};

byteCode P_isprc=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_IS_PRC,
		FAKE_END_PROC
	}
};

byteCode P_eq=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_EQ,
		FAKE_END_PROC
	}
};

byteCode P_gt=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_GT,
		FAKE_END_PROC
	}
};

byteCode P_ge=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_GE,
		FAKE_END_PROC
	}
};

byteCode P_lt=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_LT,
		FAKE_END_PROC
	}
};

byteCode P_le=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_LE,
		FAKE_END_PROC
	}
};

byteCode P_not=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_NOT,
		FAKE_END_PROC
	}
};

byteCode P_dbl=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_CAST_TO_DBL,
		FAKE_END_PROC
	}
};

byteCode P_str=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_CAST_TO_STR,
		FAKE_END_PROC
	}
};

byteCode P_sym=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_CAST_TO_SYM,
		FAKE_END_PROC
	}
};

byteCode P_chr=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_CAST_TO_CHR,
		FAKE_END_PROC
	}
};

byteCode P_int=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_CAST_TO_INT,
		FAKE_END_PROC
	}
};

byteCode P_add=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_ADD,
		FAKE_END_PROC
	}
};

byteCode P_sub=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_SUB,
		FAKE_END_PROC
	}
};

byteCode P_mul=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_MUL,
		FAKE_END_PROC
	}
};

byteCode P_div=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_DIV,
		FAKE_END_PROC
	}
};

byteCode P_mod=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_MOD,
		FAKE_END_PROC
	}
};

byteCode P_nth=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_NTH,
		FAKE_END_PROC
	}
};

byteCode P_length=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_LENGTH,
		FAKE_END_PROC
	}
};

byteCode P_append=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_APPEND,
		FAKE_END_PROC
	}
};

byteCode P_strcat=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_STR_CAT,
		FAKE_END_PROC
	}
};

byteCode P_open=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_OPEN,
		FAKE_END_PROC
	}
};

byteCode P_close=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_CLOSE,
		FAKE_END_PROC
	}
};

byteCode P_getc=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_GETC,
		FAKE_END_PROC
	}
};

byteCode P_getch=
{
	3,
	(char[])
	{
		FAKE_RES_BP,
		FAKE_GETCH,
		FAKE_END_PROC
	}
};

byteCode P_ungetc=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_UNGETC,
		FAKE_END_PROC
	}
};

byteCode P_read=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_READ,
		FAKE_END_PROC
	}
};

byteCode P_write=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_WRITE,
		FAKE_END_PROC
	}
};

byteCode P_tell=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_TELL,
		FAKE_END_PROC
	}
};

byteCode P_seek=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_SEEK,
		FAKE_END_PROC
	}
};

byteCode P_rewind=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_REWIND,
		FAKE_END_PROC
	}
};

fakeVM* newFakeVM(byteCode* mainproc,byteCode* procs)
{
	fakeVM* exe=(fakeVM*)malloc(sizeof(fakeVM));
	if(exe==NULL)errors(OUTOFMEMORY);
	exe->mainproc=newFakeProcess(newExcode(mainproc),NULL);
	exe->curproc=exe->mainproc;
	exe->procs=procs;
	exe->stack=newStack(0);
	exe->files=newFileStack();
	return exe;
}

void initGlobEnv(varstack* obj)
{
	byteCode buildInProcs[]=
	{
		P_cons,
		P_car,
		P_cdr,
		P_atom,
		P_null,
		P_ischr,
		P_isint,
		P_isdbl,
		P_isstr,
		P_issym,
		P_isprc,
		P_eq,
		P_gt,
		P_ge,
		P_lt,
		P_le,
		P_not,
		P_dbl,
		P_str,
		P_sym,
		P_chr,
		P_int,
		P_add,
		P_sub,
		P_mul,
		P_div,
		P_mod,
		P_nth,
		P_length,
		P_append,
		P_strcat,
		P_open,
		P_close,
		P_getc,
		P_getch,
		P_ungetc,
		P_read,
		P_write,
		P_tell,
		P_seek,
		P_rewind
	};
	obj->size=NUMOFBUILTINSYMBOL;
	obj->values=(stackvalue**)realloc(obj->values,sizeof(stackvalue*)*NUMOFBUILTINSYMBOL);
	if(obj->values==NULL)errors(OUTOFMEMORY);
	obj->values[0]=NULL;
	obj->values[1]=newStackValue(INT);
	obj->values[1]->value.num=1;
	obj->values[2]=newStackValue(INT);
	obj->values[2]->value.num=0;
	obj->values[3]=newStackValue(INT);
	obj->values[3]->value.num=1;
	obj->values[4]=newStackValue(INT);
	obj->values[4]->value.num=2;
	int i=5;
	for(;i<NUMOFBUILTINSYMBOL;i++)
	{
		obj->values[i]=newStackValue(PRC);
		obj->values[i]->value.prc=newBuiltInProc(copyByteCode(buildInProcs+(i-5)));
	}
}

void runFakeVM(fakeVM* exe)
{
	while(exe->curproc!=NULL&&exe->curproc->cp!=exe->curproc->code->size)
	{
		fakeprocess* curproc=exe->curproc;
		excode* tmpCode=curproc->code;
	//	fprintf(stderr,"%s\n",codeName[tmpCode->code[curproc->cp]].codeName);
		switch(ByteCodes[tmpCode->code[curproc->cp]](exe))
		{
			case 1:
				fprintf(stderr,"%s\n",codeName[tmpCode->code[curproc->cp]].codeName);
				printAllStack(exe->stack);
				putc('\n',stderr);
				fprintf(stderr,"error:Wrong arguements!\n");
				exit(EXIT_FAILURE);
			case 2:	
				fprintf(stderr,"%s\n",codeName[tmpCode->code[curproc->cp]].codeName);
				printAllStack(exe->stack);
				putc('\n',stderr);
				fprintf(stderr,"error:Stack error!\n");
				exit(EXIT_FAILURE);
		}
	//	fprintf(stderr,"=========\n");
	//	fprintf(stderr,"stack->tp=%d\n",exe->stack->tp);
	//	printAllStack(exe->stack);
	//	putc('\n',stderr);
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
	fakeprocess* proc=exe->curproc;
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
	fakeprocess* proc=exe->curproc;
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
	fakeprocess* proc=exe->curproc;
	excode* tmpCode=proc->code;
	if(stack->tp>=stack->size)
	{
		stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size+64));
		if(stack->values==NULL)errors(OUTOFMEMORY);
		stack->size+=64;
	}
	stackvalue* objValue=newStackValue(INT);
	objValue->value.num=*(int32_t*)(tmpCode->code+proc->cp+1);
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=5;
	return 0;
}

int B_push_chr(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	excode* tmpCode=proc->code;
	if(stack->tp>=stack->size)
	{
		stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size+64));
		if(stack->values==NULL)errors(OUTOFMEMORY);
		stack->size+=64;
	}
	stackvalue* objValue=newStackValue(CHR);
	objValue->value.num=*(tmpCode->code+proc->cp+1);
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=2;
	return 0;
}

int B_push_dbl(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	excode* tmpCode=proc->code;
	if(stack->tp>=stack->size)
	{
		stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size+64));
		if(stack->values==NULL)errors(OUTOFMEMORY);
		stack->size+=64;
	}
	stackvalue* objValue=newStackValue(DBL);
	objValue->value.dbl=*(double*)(tmpCode->code+proc->cp+1);
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=9;
	return 0;
}

int B_push_str(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	excode* tmpCode=proc->code;
	int len=strlen((char*)(tmpCode->code+proc->cp+1));
	char* tmpStr=(char*)malloc(sizeof(char)*(len+1));
	if(tmpStr==NULL)errors(OUTOFMEMORY);
	strcpy(tmpStr,tmpCode->code+proc->cp+1);
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
	fakeprocess* proc=exe->curproc;
	excode* tmpCode=proc->code;
	int len=strlen((char*)(tmpCode->code+proc->cp+1));
	char* tmpStr=(char*)malloc(sizeof(char)*(len+1));
	if(tmpStr==NULL)errors(OUTOFMEMORY);
	strcpy(tmpStr,tmpCode->code+proc->cp+1);
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
	fakeprocess* proc=exe->curproc;
	excode* tmpCode=proc->code;
	if(stack->tp>=stack->size)
	{
		stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size+64));
		if(stack->values==NULL)errors(OUTOFMEMORY);
		stack->size+=64;
	}
	int32_t countOfVar=*(int32_t*)(tmpCode->code+proc->cp+1);
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
	fakeprocess* proc=exe->curproc;
	stackvalue* objValue=getTopValue(stack);
	if(objValue==NULL||(objValue->type!=PAR&&objValue->type!=STR))return 1;
	if(objValue->type==PAR)
	{
		stack->values[stack->tp-1]=copyValue(getCar(objValue));
	}
	else
	{
		stackvalue* chr=newStackValue(CHR);
		chr->value.chr=objValue->value.str[0];
		stack->values[stack->tp-1]=chr;
	}
	freeStackValue(objValue);
	proc->cp+=1;
	return 0;
}

int B_push_cdr(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	stackvalue* objValue=getTopValue(stack);
	if(objValue==NULL||(objValue->type!=PAR&&objValue->type!=STR))return 1;
	if(objValue->type==PAR)
	{
		stack->values[stack->tp-1]=copyValue(getCdr(objValue));
	}
	else
	{
		char* str=objValue->value.str;
		if(strlen(str)==0)stack->values[stack->tp-1]=NULL;
		else
		{
			stackvalue* tmpStr=newStackValue(STR);
			tmpStr->value.str=copyStr(objValue->value.str+1);
			stack->values[stack->tp-1]=tmpStr;
		}
	}
	freeStackValue(objValue);
	proc->cp+=1;
	return 0;
}

int B_push_top(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
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
	fakeprocess* proc=exe->curproc;
	excode* tmpCode=proc->code;
	int32_t countOfProc=*(int32_t*)(tmpCode->code+proc->cp+1);
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
	fakeprocess* proc=exe->curproc;
	freeStackValue(getTopValue(stack));
	stack->values[stack->tp-1]=NULL;
	stack->tp-=1;
	stackRecycle(stack);
	proc->cp+=1;
	return 0;
}

int B_pop_var(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	excode* tmpCode=proc->code;
	int32_t countOfVar=*(int32_t*)(tmpCode->code+proc->cp+1);
	varstack* curEnv=proc->localenv;
	while(curEnv->bound==-1||countOfVar<curEnv->bound)curEnv=curEnv->prev;
	stackvalue** pValue=NULL;
	if(countOfVar-curEnv->bound>=curEnv->size)
	{
		curEnv->size+=1;
		curEnv->values=(stackvalue**)realloc(curEnv->values,sizeof(stackvalue*)*curEnv->size);
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
		stackRecycle(stack);
	}else return 2;
	proc->cp+=5;
	return 0;
}

int B_pop_rest_var(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	excode* tmpCode=proc->code;
	int32_t countOfVar=*(int32_t*)(tmpCode->code+proc->cp+1);
	varstack* curEnv=proc->localenv;
	while(curEnv->bound==-1||countOfVar<curEnv->bound)curEnv=curEnv->prev;
	stackvalue** tmpValue=NULL;
	if(countOfVar-curEnv->bound>=curEnv->size)
	{
		curEnv->size+=1;
		curEnv->values=(stackvalue**)realloc(curEnv->values,sizeof(stackvalue*)*curEnv->size);
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
		stackRecycle(stack);
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
	fakeprocess* proc=exe->curproc;
	stackvalue* topValue=getTopValue(stack);
	stackvalue* objValue=getValue(stack,stack->tp-2);
	if(objValue==NULL||objValue->type!=PAR)return 1;
	freeStackValue(objValue->value.par.car);
	objValue->value.par.car=copyValue(topValue);
	freeStackValue(topValue);
	stack->tp-=1;
	stackRecycle(stack);
	proc->cp+=1;
	return 0;
}

int B_pop_cdr(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	stackvalue* topValue=getTopValue(stack);
	stackvalue* objValue=getValue(stack,stack->tp-2);
	if(objValue==NULL||objValue->type!=PAR)return 1;
	freeStackValue(objValue->value.par.cdr);
	objValue->value.par.cdr=copyValue(topValue);
	freeStackValue(topValue);
	stack->tp-=1;
	stackRecycle(stack);
	proc->cp+=1;
	return 0;
}

int B_add(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	stackvalue* firValue=getValue(stack,stack->tp-1);
	stackvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(stack);
	if((firValue==NULL||secValue==NULL)||(firValue->type!=INT&&firValue->type!=DBL)||(secValue->type!=INT&&secValue->type!=DBL))return 1;
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
	fakeprocess* proc=exe->curproc;
	stackvalue* firValue=getValue(stack,stack->tp-1);
	stackvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(stack);
	if((firValue==NULL||secValue==NULL)||(firValue->type!=INT&&firValue->type!=DBL)||(secValue->type!=INT&&secValue->type!=DBL))return 1;
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
	fakeprocess* proc=exe->curproc;
	stackvalue* firValue=getValue(stack,stack->tp-1);
	stackvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(stack);
	if((firValue==NULL||secValue==NULL)||(firValue->type!=INT&&firValue->type!=DBL)||(secValue->type!=INT&&secValue->type!=DBL))return 1;
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
	fakeprocess* proc=exe->curproc;
	stackvalue* firValue=getValue(stack,stack->tp-1);
	stackvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(stack);
	if((firValue==NULL||secValue==NULL)||(firValue->type!=INT&&firValue->type!=DBL)||(secValue->type!=INT&&secValue->type!=DBL))return 1;
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
	fakeprocess* proc=exe->curproc;
	stackvalue* firValue=getValue(stack,stack->tp-1);
	stackvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(stack);
	if((firValue==NULL||secValue==NULL)||(firValue->type!=INT&&firValue->type!=DBL)||(secValue->type!=INT&&secValue->type!=DBL))return 1;
	int32_t result=((int32_t)((secValue->type==DBL)?secValue->value.dbl:secValue->value.num))%((int32_t)((firValue->type==DBL)?firValue->value.dbl:firValue->value.num));
	stackvalue* tmpValue=newStackValue(INT);
	tmpValue->value.num=result;
	stack->values[stack->tp-1]=tmpValue;
	freeStackValue(firValue);
	freeStackValue(secValue);
	proc->cp+=1;
	return 0;
}

int B_atom(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
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
	fakeprocess* proc=exe->curproc;
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
	fakeprocess* proc=exe->curproc;
	excode* tmpCode=proc->code;
	stackvalue* topValue=getTopValue(stack);
	if(topValue->type!=PRC)return 1;
	int32_t boundOfProc=*(int32_t*)(tmpCode->code+proc->cp+1);
	topValue->value.prc->localenv=newVarStack(boundOfProc,1,tmpCode->localenv);
	proc->cp+=5;
	return 0;
}

int B_end_proc(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	stackvalue* tmpValue=getTopValue(stack);
	fakeprocess* tmpProc=exe->curproc;
//	fprintf(stdout,"End proc: %p\n",tmpProc->code);
	exe->curproc=exe->curproc->prev;
	excode* tmpCode=tmpProc->code;
	varstack* tmpEnv=tmpCode->localenv;
	if(tmpValue!=NULL&&tmpValue->type==PRC&&tmpValue->value.prc->localenv->prev==tmpEnv)tmpEnv->next=tmpValue->value.prc->localenv;
	if(tmpCode==tmpProc->prev->code)tmpCode->localenv=tmpProc->prev->localenv;
	else tmpCode->localenv=newVarStack(tmpEnv->bound,1,tmpEnv->prev);
	if(tmpEnv->prev!=NULL&&tmpEnv->prev->next==tmpEnv&&tmpEnv->next==NULL)tmpEnv->prev->next=tmpCode->localenv;
	tmpEnv->inProc=0;
	freeVarStack(tmpEnv);
	if(tmpCode->refcount==0)freeExcode(tmpCode);
	else tmpCode->refcount-=1;
	free(tmpProc);
	tmpProc=NULL;
	return 0;
}

int B_set_bp(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	stackvalue* prevBp=newStackValue(INT);
	prevBp->value.num=stack->bp;
	if(stack->tp>=stack->size)
	{
		stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size+64));
		if(stack->values==NULL)errors(OUTOFMEMORY);
		stack->size+=64;
	}
	stack->values[stack->tp]=prevBp;
	stack->tp+=1;
	stack->bp=stack->tp;
	proc->cp+=1;
	return 0;
}

int B_res_bp(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	if(stack->tp>stack->bp)return 2;
	stackvalue* prevBp=getTopValue(stack);
	stack->bp=prevBp->value.num;
	freeStackValue(prevBp);
	stack->tp-=1;
	stackRecycle(stack);
	proc->cp+=1;
	return 0;
}

int B_invoke(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	stackvalue* tmpValue=getTopValue(stack);
	if(tmpValue==NULL||tmpValue->type!=PRC)return 1;
	proc->cp+=1;
	excode* tmpCode=tmpValue->value.prc;
	fakeprocess* prevProc=hasSameProc(tmpCode,proc);
	if(stack->bp==1&&isTheLastExpress(proc,prevProc)&&prevProc)
		prevProc->cp=0;
	else 
	{
		fakeprocess* tmpProc=newFakeProcess(tmpCode,proc);
		if(tmpCode->localenv->prev!=NULL&&tmpCode->localenv->prev->next!=tmpCode->localenv)
		{
			tmpProc->localenv=newVarStack(tmpCode->localenv->bound,1,tmpCode->localenv->prev);
			if(!hasSameProc(tmpCode,proc))freeVarStack(tmpCode->localenv);
			tmpCode->localenv=tmpProc->localenv;
		}
		else tmpProc->localenv=tmpCode->localenv;
		exe->curproc=tmpProc;
	}
	free(tmpValue);
	stack->tp-=1;
	stackRecycle(stack);
	return 0;
}

int B_jump_if_ture(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	excode* tmpCode=proc->code;
	stackvalue* tmpValue=getTopValue(stack);
	if(!(tmpValue==NULL||(tmpValue->type==PAR&&tmpValue->value.par.car==NULL&&tmpValue->value.par.cdr==NULL)))
	{
		int32_t where=*(int32_t*)(tmpCode->code+proc->cp+sizeof(char));
		proc->cp+=where;
	}
	else
	{
		stack->tp-=1;
		stackRecycle(stack);
		freeStackValue(tmpValue);
	}
	proc->cp+=5;
	return 0;
}

int B_jump_if_false(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	excode* tmpCode=proc->code;
	stackvalue* tmpValue=getTopValue(stack);
	stack->tp-=1;
	stackRecycle(stack);
	if(tmpValue==NULL||(tmpValue->type==PAR&&tmpValue->value.par.car==NULL&&tmpValue->value.par.cdr==NULL))
	{
		int32_t where=*(int32_t*)(tmpCode->code+proc->cp+sizeof(char));
		proc->cp+=where;
	}
	freeStackValue(tmpValue);
	proc->cp+=5;
	return 0;
}

int B_jump(fakeVM* exe)
{
	fakeprocess* proc=exe->curproc;
	excode* tmpCode=proc->code;
	int32_t where=*(int32_t*)(tmpCode->code+proc->cp+sizeof(char));
	proc->cp+=where+5;
	return 0;
}

int B_open(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	filestack* files=exe->files;
	stackvalue* mode=getTopValue(stack);
	stackvalue* filename=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(stack);
	if(filename==NULL||mode==NULL||filename->type!=STR||mode->type!=STR)return 1;
	int32_t i=0;
	FILE* file=fopen(filename->value.str,mode->value.str);
	if(file==NULL)i=-1;
	if(i!=-1)
	{
		for(;i<files->size;i++)if(files->files[i]==NULL)break;
		if(i==files->size)
		{
			files->files=(FILE**)realloc(files->files,sizeof(FILE*)*(files->size+1));
			files->files[i]=file;
			files->size+=1;
		}
		else files->files[i]=file;
	}
	stackvalue* countOfFile=newStackValue(INT);
	countOfFile->value.num=i;
	stack->values[stack->tp-1]=countOfFile;
	freeStackValue(filename);
	freeStackValue(mode);
	proc->cp+=1;
	return 0;
}

int B_close(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	filestack* files=exe->files;
	stackvalue* topValue=getTopValue(stack);
	if(topValue==NULL||topValue->type!=INT)return 1;
	if(topValue->value.num>=files->size)return 2;
	FILE* objFile=files->files[topValue->value.num];
	if(fclose(objFile)==EOF)
	{
		files->files[topValue->value.num]=NULL;
		topValue->value.num=-1;
	}
	else files->files[topValue->value.num]=NULL;
	proc->cp+=1;
	return 0;
}

int B_eq(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	stackvalue* firValue=getTopValue(stack);
	stackvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(stack);
	if(stackvaluecmp(firValue,secValue))
	{
		stackvalue* tmpValue=newStackValue(INT);
		tmpValue->value.num=1;
		stack->values[stack->tp-1]=tmpValue;
	}
	else stack->values[stack->tp-1]=NULL;
	freeStackValue(firValue);
	freeStackValue(secValue);
	proc->cp+=1;
	return 0;
}

int B_gt(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	stackvalue* firValue=getTopValue(stack);
	stackvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(stack);
	if((firValue==NULL||secValue==NULL)||(firValue->type!=INT&&firValue->type!=DBL)||(secValue->type!=INT&&secValue->type!=DBL))return 1;
	if(firValue->type==DBL||secValue->type==DBL)
	{
		double result=((secValue->type==DBL)?secValue->value.dbl:secValue->value.num)-((firValue->type==DBL)?firValue->value.dbl:firValue->value.num);
		if(result>0)
		{
			stackvalue* tmpValue=newStackValue(INT);
			tmpValue->value.num=1;
			stack->values[stack->tp-1]=tmpValue;
		}
		else stack->values[stack->tp-1]=NULL;
	}
	else if(firValue->type==INT&&secValue->type==INT)
	{
		if(secValue->value.num>firValue->value.num)
		{
			stackvalue* tmpValue=newStackValue(INT);
			tmpValue->value.num=1;
			stack->values[stack->tp-1]=tmpValue;
		}
			else stack->values[stack->tp-1]=NULL;
	}
	freeStackValue(firValue);
	freeStackValue(secValue);
	proc->cp+=1;
	return 0;
}

int B_ge(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	stackvalue* firValue=getTopValue(stack);
	stackvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(stack);
	if((firValue==NULL||secValue==NULL)||(firValue->type!=INT&&firValue->type!=DBL)||(secValue->type!=INT&&secValue->type!=DBL))return 1;
	if(firValue->type==DBL||secValue->type==DBL)
	{
		double result=((secValue->type==DBL)?secValue->value.dbl:secValue->value.num)-((firValue->type==DBL)?firValue->value.dbl:firValue->value.num);
		if(result>=0)
		{
			stackvalue* tmpValue=newStackValue(INT);
			tmpValue->value.num=1;
			stack->values[stack->tp-1]=tmpValue;
		}
		else stack->values[stack->tp-1]=NULL;
	}
	else if(firValue->type==INT&&secValue->type==INT)
	{
		if(secValue->value.num>=firValue->value.num)
		{
			stackvalue* tmpValue=newStackValue(INT);
			tmpValue->value.num=1;
			stack->values[stack->tp-1]=tmpValue;
		}
		else stack->values[stack->tp-1]=NULL;
	}
	freeStackValue(firValue);
	freeStackValue(secValue);
	proc->cp+=1;
	return 0;
}

int B_lt(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	stackvalue* firValue=getTopValue(stack);
	stackvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(stack);
	if((firValue==NULL||secValue==NULL)||(firValue->type!=INT&&firValue->type!=DBL)||(secValue->type!=INT&&secValue->type!=DBL))return 1;
	if(firValue->type==DBL||secValue->type==DBL)
	{
		double result=((secValue->type==DBL)?secValue->value.dbl:secValue->value.num)-((firValue->type==DBL)?firValue->value.dbl:firValue->value.num);
		if(result<0)
		{
			stackvalue* tmpValue=newStackValue(INT);
			tmpValue->value.num=1;
			stack->values[stack->tp-1]=tmpValue;
		}
		else stack->values[stack->tp-1]=NULL;
	}
	else if(firValue->type==INT&&secValue->type==INT)
	{
		if(secValue->value.num<firValue->value.num)
		{
			stackvalue* tmpValue=newStackValue(INT);
			tmpValue->value.num=1;
			stack->values[stack->tp-1]=tmpValue;
		}
		else stack->values[stack->tp-1]=NULL;
	}
	freeStackValue(firValue);
	freeStackValue(secValue);
	proc->cp+=1;
	return 0;
}

int B_le(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	stackvalue* firValue=getTopValue(stack);
	stackvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(stack);
	if((firValue==NULL||secValue==NULL)||(firValue->type!=INT&&firValue->type!=DBL)||(secValue->type!=INT&&secValue->type!=DBL))return 1;

	if(firValue->type==DBL||secValue->type==DBL)
	{
		double result=((secValue->type==DBL)?secValue->value.dbl:secValue->value.num)-((firValue->type==DBL)?firValue->value.dbl:firValue->value.num);
		if(result<=0)
		{
			stackvalue* tmpValue=newStackValue(INT);
			tmpValue->value.num=1;
			stack->values[stack->tp-1]=tmpValue;
		}
		else stack->values[stack->tp-1]=NULL;
	}
	else if(firValue->type==INT&&secValue->type==INT)
	{
		if(secValue->value.num<=firValue->value.num)
		{
			stackvalue* tmpValue=newStackValue(INT);
			tmpValue->value.num=1;
			stack->values[stack->tp-1]=tmpValue;
		}
		else stack->values[stack->tp-1]=NULL;
	}
	freeStackValue(firValue);
	freeStackValue(secValue);
	proc->cp+=1;
	return 0;
}

int B_not(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	stackvalue* tmpValue=getTopValue(stack);
	if(tmpValue==NULL||(tmpValue->type==PAR&&tmpValue->value.par.car==NULL&&tmpValue->value.par.cdr==NULL))
	{
		freeStackValue(tmpValue);
		tmpValue=newStackValue(INT);
		tmpValue->value.num=1;
		stack->values[stack->tp-1]=tmpValue;
	}
	else
	{
		freeStackValue(tmpValue);
		stack->values[stack->tp-1]=NULL;
	}
	proc->cp+=1;
	return 0;
}

int B_cast_to_chr(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	stackvalue* topValue=getTopValue(stack);
	if(topValue==NULL||topValue->type==PAR||topValue->type==PRC)return 1;
	stackvalue* tmpValue=newStackValue(CHR);
	switch(topValue->type)
	{
		case INT:tmpValue->value.chr=topValue->value.num;break;
		case DBL:tmpValue->value.chr=(int32_t)topValue->value.dbl;break;
		case CHR:tmpValue->value.chr=topValue->value.chr;break;
		case STR:
		case SYM:tmpValue->value.chr=topValue->value.str[0];break;
	}
	stack->values[stack->tp-1]=tmpValue;
	freeStackValue(topValue);
	proc->cp+=1;
	return 0;
}

int B_cast_to_int(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	stackvalue* topValue=getTopValue(stack);
	if(topValue==NULL||topValue->type==PAR||topValue->type==PRC)return 1;
	stackvalue* tmpValue=newStackValue(INT);
	switch(topValue->type)
	{
		case INT:tmpValue->value.num=topValue->value.num;break;
		case DBL:tmpValue->value.num=(int32_t)topValue->value.dbl;break;
		case CHR:tmpValue->value.num=topValue->value.chr;break;
		case STR:
		case SYM:tmpValue->value.num=stringToInt(topValue->value.str);break;
	}
	stack->values[stack->tp-1]=tmpValue;
	freeStackValue(topValue);
	proc->cp+=1;
	return 0;
}

int B_cast_to_dbl(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	stackvalue* topValue=getTopValue(stack);
	if(topValue==NULL||topValue->type==PAR||topValue->type==PRC)return 1;
	stackvalue* tmpValue=newStackValue(DBL);
	switch(topValue->type)
	{
		case INT:tmpValue->value.dbl=(double)topValue->value.num;break;
		case DBL:tmpValue->value.dbl=topValue->value.dbl;break;
		case CHR:tmpValue->value.dbl=(double)(int32_t)topValue->value.chr;break;
		case STR:
		case SYM:tmpValue->value.dbl=stringToDouble(topValue->value.str);break;
	}
	stack->values[stack->tp-1]=tmpValue;
	freeStackValue(topValue);
	proc->cp+=1;
	return 0;
}

int B_cast_to_str(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	stackvalue* topValue=getTopValue(stack);
	if(topValue==NULL||topValue->type==PAR||topValue->type==PRC)return 1;
	stackvalue* tmpValue=newStackValue(STR);
	switch(topValue->type)
	{
		case INT:tmpValue->value.str=intToString(topValue->value.num);break;
		case DBL:tmpValue->value.str=doubleToString(topValue->value.dbl);break;
		case CHR:tmpValue->value.str=(char*)malloc(sizeof(char)*2);
				 if(tmpValue->value.str==NULL)errors(OUTOFMEMORY);
				 tmpValue->value.str[0]=topValue->value.chr;
				 tmpValue->value.str[1]='\0';
				 break;
		case STR:
		case SYM:tmpValue->value.str=copyStr(topValue->value.str);break;
	}
	stack->values[stack->tp-1]=tmpValue;
	freeStackValue(topValue);
	proc->cp+=1;
	return 0;
}

int B_cast_to_sym(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	stackvalue* topValue=getTopValue(stack);
	if(topValue==NULL||topValue->type==PAR||topValue->type==PRC)return 1;
	stackvalue* tmpValue=newStackValue(SYM);
	switch(topValue->type)
	{
		case INT:tmpValue->value.str=intToString(topValue->value.num);break;
		case DBL:tmpValue->value.str=doubleToString(topValue->value.dbl);break;
		case CHR:tmpValue->value.str=(char*)malloc(sizeof(char)*2);
				 if(tmpValue->value.str==NULL)errors(OUTOFMEMORY);
				 tmpValue->value.str[0]=topValue->value.chr;
				 tmpValue->value.str[1]='\0';
				 break;
		case STR:
		case SYM:tmpValue->value.str=copyStr(topValue->value.str);break;
	}
	stack->values[stack->tp-1]=tmpValue;
	freeStackValue(topValue);
	proc->cp+=1;
	return 0;
}

int B_is_chr(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	stackvalue* objValue=getTopValue(stack);
	if(objValue!=NULL&&objValue->type==CHR)
	{
		stackvalue* tmpValue=newStackValue(INT);
		tmpValue->value.num=1;
		stack->values[stack->tp-1]=tmpValue;
	}
	else stack->values[stack->tp-1]=NULL;
	freeStackValue(objValue);
	proc->cp+=1;
	return 0;
}

int B_is_int(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	stackvalue* objValue=getTopValue(stack);
	if(objValue!=NULL&&objValue->type==INT)
	{
		stackvalue* tmpValue=newStackValue(INT);
		tmpValue->value.num=1;
		stack->values[stack->tp-1]=tmpValue;
	}
	else stack->values[stack->tp-1]=NULL;
	freeStackValue(objValue);
	proc->cp+=1;
	return 0;
}

int B_is_dbl(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	stackvalue* objValue=getTopValue(stack);
	if(objValue!=NULL&&objValue->type==DBL)
	{
		stackvalue* tmpValue=newStackValue(INT);
		tmpValue->value.num=1;
		stack->values[stack->tp-1]=tmpValue;
	}
	else stack->values[stack->tp-1]=NULL;
	freeStackValue(objValue);
	proc->cp+=1;
	return 0;
}

int B_is_str(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	stackvalue* objValue=getTopValue(stack);
	if(objValue!=NULL&&objValue->type==STR)
	{
		stackvalue* tmpValue=newStackValue(INT);
		tmpValue->value.num=1;
		stack->values[stack->tp-1]=tmpValue;
	}
	else stack->values[stack->tp-1]=NULL;
	freeStackValue(objValue);
	proc->cp+=1;
	return 0;
}

int B_is_sym(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	stackvalue* objValue=getTopValue(stack);
	if(objValue!=NULL&&objValue->type==SYM)
	{
		stackvalue* tmpValue=newStackValue(INT);
		tmpValue->value.num=1;
		stack->values[stack->tp-1]=tmpValue;
	}
	else stack->values[stack->tp-1]=NULL;
	freeStackValue(objValue);
	proc->cp+=1;
	return 0;
}

int B_is_prc(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	stackvalue* objValue=getTopValue(stack);
	if(objValue!=NULL&&objValue->type==PRC)
	{
		stackvalue* tmpValue=newStackValue(INT);
		tmpValue->value.num=1;
		stack->values[stack->tp-1]=tmpValue;
	}
	else stack->values[stack->tp-1]=NULL;
	freeStackValue(objValue);
	proc->cp+=1;
	return 0;
}

int B_nth(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	stackvalue* place=getValue(stack,stack->tp-2);
	stackvalue* objlist=getTopValue(stack);
	stack->tp-1;
	stackRecycle(stack);
	if(objlist==NULL||place==NULL||(objlist->type!=PAR&&objlist->type!=STR)||place->type!=INT)return 1;
	if(objlist->type==PAR)
	{
		stackvalue* obj=getCar(objlist);
		objlist->value.par.car=NULL;
		stackvalue* objPair=getCdr(objlist);
		int i=0;
		for(;i<place->value.num;i++)
		{
			if(objPair==NULL)return 2;
			freeStackValue(obj);
			obj=getCar(objPair);
			objPair->value.par.car=NULL;
			objPair=getCdr(objPair);
		}
		stack->values[stack->tp-1]=obj;
	}
	else
	{
		stackvalue* objChr=newStackValue(CHR);
		objChr->value.chr=objlist->value.str[place->value.num];
		stack->values[stack->tp-1]=objChr;
	}
	freeStackValue(objlist);
	freeStackValue(place);
	proc->cp+=1;
	return 0;
}

int B_length(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	stackvalue* objlist=getTopValue(stack);
	if(objlist==NULL||(objlist->type!=PAR&&objlist->type!=STR))return 1;
	if(objlist->type==PAR)
	{
		int32_t i=0;
		for(stackvalue* tmp=objlist;tmp==NULL&&tmp->type==PAR;tmp=getCdr(tmp))i++;
		stackvalue* num=newStackValue(INT);
		num->value.num=i;
		stack->values[stack->tp-1]=num;
	}
	else
	{
		stackvalue* len=newStackValue(INT);
		len->value.num=strlen(objlist->value.str);
		stack->values[stack->tp-1]=len;
	}
	freeStackValue(objlist);
	proc->cp+=1;
	return 0;
}

int B_append(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	stackvalue* fir=getTopValue(stack);
	stackvalue* sec=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(stack);
	if(fir==NULL||sec==NULL||fir->type!=PAR||sec->type!=PAR)return 1;
	stackvalue* lastpair=fir;
	while(getCdr(lastpair)!=NULL)lastpair=getCdr(lastpair);
	lastpair->value.par.cdr=sec;
	stack->values[stack->tp-1]=fir;
	proc->cp+=1;
	return 0;
}

int B_str_cat(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	stackvalue* fir=getTopValue(stack);
	stackvalue* sec=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(stack);
	if(fir==NULL||sec==NULL||fir->type!=STR||sec->type!=STR)return 1;
	int firlen=strlen(fir->value.str);
	int seclen=strlen(sec->value.str);
	char* tmpStr=(char*)malloc(sizeof(char)*(firlen+seclen+1));
	if(tmpStr==NULL)errors(OUTOFMEMORY);
	strcpy(tmpStr,sec->value.str);
	strcat(tmpStr,fir->value.str);
	stackvalue* tmpValue=newStackValue(STR);
	tmpValue->value.str=tmpStr;
	stack->values[stack->tp-1]=tmpValue;
	freeStackValue(fir);
	freeStackValue(sec);
	proc->cp+=1;
	return 0;
}

int B_getc(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	filestack* files=exe->files;
	stackvalue* file=getTopValue(stack);
	if(file==NULL||file->type!=INT)return 1;
	if(file->value.num>=files->size)return 2;
	stackvalue* tmpChr=newStackValue(CHR);
	tmpChr->value.chr=getc(files->files[file->value.num]);
	stack->values[stack->tp-1]=tmpChr;
	freeStackValue(file);
	proc->cp+=1;
	return 0;
}

int B_getch(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	if(stack->tp>=stack->size)
	{
		stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size+64));
		if(stack->values==NULL)errors(OUTOFMEMORY);
		stack->size+=64;
	}
	stackvalue* tmpChr=newStackValue(CHR);
	tmpChr->value.chr=getch();
	stack->values[stack->tp]=tmpChr;
	stack->tp+=1;
	proc->cp+=1;
	return 0;
}

int B_ungetc(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	filestack* files=exe->files;
	stackvalue* file=getTopValue(stack);
	stackvalue* tmpChr=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(stack);
	if(file==NULL||tmpChr==NULL||file->type!=INT||tmpChr->type!=CHR)return 1;
	if(file->value.num>=files->size)return 2;
	tmpChr->value.chr=ungetc(tmpChr->value.chr,files->files[file->value.num]);
	freeStackValue(file);
	proc->cp+=1;
	return 0;
}

int B_read(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	filestack* files=exe->files;
	stackvalue* file=getTopValue(stack);
	if(file==NULL||file->type!=INT)return 1;
	if(file->value.num>=files->size)return 2;
	FILE* tmpFile=files->files[file->value.num];
	char* tmpString=getListFromFile(tmpFile);
	intpr* tmpIntpr=newIntpr(NULL,tmpFile);
	cptr* tmpCptr=createTree(tmpString,tmpIntpr);
	stackvalue* tmp=castCptrStackValue(tmpCptr);
	stack->values[stack->tp-1]=tmp;
	free(tmpIntpr);
	free(tmpString);
	deleteCptr(tmpCptr);
	free(tmpCptr);
	freeStackValue(file);
	proc->cp+=1;
	return 0;
}

int B_write(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	filestack* files=exe->files;
	stackvalue* file=getTopValue(stack);
	stackvalue* obj=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(stack);
	if(file==NULL||file->type!=INT)return 1;
	if(file->value.num>=files->size)return 2;
	FILE* objFile=files->files[file->value.num];
	if(objFile==NULL)return 2;
	printStackValue(obj,objFile);
	proc->cp+=1;
	return 0;
}

int B_tell(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	filestack* files=exe->files;
	stackvalue* file=getTopValue(stack);
	if(file==NULL||file->type!=INT)return 1;
	if(file->value.num>=files->size)return 2;
	FILE* objFile=files->files[file->value.num];
	if(objFile==NULL)return 2;
	else file->value.num=ftell(objFile);
	proc->cp+=1;
	return 0;
}

int B_seek(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	filestack* files=exe->files;
	stackvalue* where=getTopValue(stack);
	stackvalue* file=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(stack);
	if(where==NULL||file==NULL||where->type!=INT||file->type!=INT)return 1;
	if(file->value.num>=files->size)return 2;
	FILE* objFile=files->files[file->value.num];
	if(objFile==NULL)return 2;
	file->value.num=fseek(objFile,where->value.num,SEEK_CUR);
	freeStackValue(where);
	proc->cp+=1;
	return 0;
}

int B_rewind(fakeVM* exe)
{
	fakestack* stack=exe->stack;
	fakeprocess* proc=exe->curproc;
	filestack* files=exe->files;
	stackvalue* file=getTopValue(stack);
	if(file==NULL||file->type!=INT)return 1;
	if(file->value.num>=files->size)return 2;
	FILE* objFile=files->files[file->value.num];
	if(objFile==NULL)return 2;
	rewind(objFile);
	proc->cp+=1;
	return 0;
}

excode* newExcode(byteCode* proc)
{
	excode* tmp=(excode*)malloc(sizeof(excode));
	if(tmp==NULL)errors(OUTOFMEMORY);
	tmp->localenv=NULL;
	tmp->refcount=0;
	if(proc!=NULL)
	{
		tmp->size=proc->size;
		tmp->code=proc->code;
	}
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
	varstack* prev=(curEnv->prev!=NULL&&(curEnv->prev->next!=NULL&&curEnv->prev->next==curEnv))?curEnv->prev:NULL;
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
//	fprintf(stderr,"New env: %p\n",tmp);
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
	//	fprintf(stderr,"Free env:%p\n",obj);
		free(obj);
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

void printStackValue(stackvalue* objValue,FILE* fp)
{
	if(objValue==NULL)
	{
		fprintf(fp,"nil");
		return;
	}
	switch(objValue->type)
	{
		case INT:fprintf(fp,"%d",objValue->value.num);break;
		case DBL:fprintf(fp,"%lf",objValue->value.dbl);break;
		case CHR:putc(objValue->value.chr,fp);break;
		case SYM:fprintf(fp,"%s",objValue->value.str);break;
		case STR:fprintf(fp,"%s",objValue->value.str);break;
		case PRC:fprintf(fp,"<#proc>");break;
		case PAR:putc('(',fp);
				 printStackValue(objValue->value.par.car,fp);
				 putc(',',fp);
				 printStackValue(objValue->value.par.cdr,fp);
				 putc(')',fp);
				 break;
		default:fprintf(fp,"Bad value!");break;
	}
}

int stackvaluecmp(stackvalue* fir,stackvalue* sec)
{
	if(fir==NULL&&sec==NULL)return 1;
	if((fir==NULL||sec==NULL)&&fir!=sec)return 0;
	if(fir->type!=sec->type)return 0;
	else
	{
		switch(fir->type)
		{
			case INT:return fir->value.num==sec->value.num;
			case CHR:return fir->value.chr==sec->value.chr;
			case DBL:return fabs(fir->value.dbl-sec->value.dbl)==0;
			case STR:
			case SYM:return !strcmp(fir->value.str,sec->value.str);
			case PRC:return fir->value.prc==sec->value.prc;
			case PAR:return stackvaluecmp(fir->value.par.car,sec->value.par.car)&&stackvaluecmp(fir->value.par.cdr,sec->value.par.cdr);
		}
	}
}

void stackRecycle(fakestack* stack)
{
	if(stack->size-stack->tp>64)
	{
		stack->values=(stackvalue**)realloc(stack->values,sizeof(stackvalue*)*(stack->size-64));
		if(stack->values==NULL)errors(OUTOFMEMORY);
		stack->size-=64;
	}
}

excode* newBuiltInProc(byteCode* proc)
{
	excode* tmp=(excode*)malloc(sizeof(excode));
	if(tmp==NULL)errors(OUTOFMEMORY);
	tmp->localenv=newVarStack(0,1,NULL);
	if(proc!=NULL)
	{
		tmp->size=proc->size;
		tmp->code=proc->code;
	}
	return tmp;
}

fakeprocess* newFakeProcess(excode* code,fakeprocess* prev)
{
	fakeprocess* tmp=(fakeprocess*)malloc(sizeof(fakeprocess));
	if(tmp==NULL)errors(OUTOFMEMORY);
	tmp->prev=prev;
	tmp->cp=0;
	tmp->code=code;
//	fprintf(stdout,"New proc: %p\n",code);
	return tmp;
}

void printAllStack(fakestack* stack)
{
	if(stack->tp==0)printf("[#EMPTY]\n");
	else
	{
		int i=stack->tp-1;
		for(;i>=0;i--)
		{
			printStackValue(stack->values[i],stdout);
			putchar('\n');
		}
	}
}

stackvalue* castCptrStackValue(const cptr* objCptr)
{
	if(objCptr->type==ATM)
	{
		atom* tmpAtm=objCptr->value;
		stackvalue* tmp=newStackValue(tmpAtm->type);
		switch(tmpAtm->type)
		{
			case INT:tmp->value.num=tmpAtm->value.num;break;
			case DBL:tmp->value.dbl=tmpAtm->value.dbl;break;
			case CHR:tmp->value.chr=tmpAtm->value.chr;break;
			case SYM:
			case STR:tmp->value.str=copyStr(tmpAtm->value.str);break;
		}
		return tmp;
	}
	else if(objCptr->type==PAR)
	{
		pair* objPair=objCptr->value;
		stackvalue* tmp=newStackValue(PAR);
		tmp->value.par.car=castCptrStackValue(&objPair->car);
		tmp->value.par.cdr=castCptrStackValue(&objPair->cdr);
		return tmp;
	}
	else if(objCptr->type==NIL)return NULL;
}

fakeprocess* hasSameProc(excode* objCode,fakeprocess* curproc)
{
	while(curproc!=NULL&&curproc->code!=objCode)curproc=curproc->prev;
	return curproc;
}

int isTheLastExpress(const fakeprocess* proc,const fakeprocess* same)
{
	if(same==NULL)return 0;
	for(;;)
	{
		char* code=proc->code->code;
		int32_t size=proc->code->size;
		if(code[proc->cp]==FAKE_JMP)
		{
			int32_t where=*(int32_t*)(code+proc->cp+1);
			if((where+proc->cp+5)!=size-1)return 0;
		}
		else if(proc->cp!=size-1)return 0;
		if(proc==same)break;
		proc=proc->prev;
	}
	return 1;
}

int getch()
{
	struct termios oldt,newt;
	int ch;
	tcgetattr(STDIN_FILENO,&oldt);
	newt=oldt;
	newt.c_lflag &=~(ICANON|ECHO);
	tcsetattr(STDIN_FILENO,TCSANOW,&newt);
	ch=getchar();
	tcsetattr(STDIN_FILENO,TCSANOW,&oldt);
	return ch;
}
