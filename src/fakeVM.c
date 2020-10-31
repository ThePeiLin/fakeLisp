#include"tool.h"
#include"fakeVM.h"
#include"opcode.h"
#include<string.h>
#include<math.h>
#include<pthread.h>
#ifdef _WIN32
#include<conio.h>
#else
#include<termios.h>
#endif
#include<unistd.h>
#include<time.h>
#define NUMOFBUILTINSYMBOL 54
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
	B_push_byte,
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
	B_rand,
	B_atom,
	B_null,
	B_cast_to_chr,
	B_cast_to_int,
	B_cast_to_dbl,
	B_cast_to_str,
	B_cast_to_sym,
	B_cast_to_byte,
	B_is_chr,
	B_is_int,
	B_is_dbl,
	B_is_str,
	B_is_sym,
	B_is_prc,
	B_is_byte,
	B_nth,
	B_length,
	B_append,
	B_str_cat,
	B_byte_cat,
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
	B_readb,
	B_write,
	B_writeb,
	B_princ,
	B_tell,
	B_seek,
	B_rewind,
	B_exit,/*
	B_go,
	B_wait,
	B_send,
	B_accept*/
};

ByteCode P_cons=
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

ByteCode P_car=
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

ByteCode P_cdr=
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

ByteCode P_atom=
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

ByteCode P_null=
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

ByteCode P_isint=
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

ByteCode P_ischr=
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

ByteCode P_isdbl=
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

ByteCode P_isstr=
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

ByteCode P_issym=
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

ByteCode P_isprc=
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

ByteCode P_isbyt=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_IS_BYTE,
		FAKE_END_PROC
	}
};

ByteCode P_eq=
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

ByteCode P_gt=
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

ByteCode P_ge=
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

ByteCode P_lt=
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

ByteCode P_le=
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

ByteCode P_not=
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

ByteCode P_dbl=
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

ByteCode P_str=
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

ByteCode P_sym=
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

ByteCode P_chr=
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

ByteCode P_int=
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

ByteCode P_byt=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_CAST_TO_BYTE,
		FAKE_END_PROC
	}
};

ByteCode P_add=
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

ByteCode P_sub=
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

ByteCode P_mul=
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

ByteCode P_div=
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

ByteCode P_mod=
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

ByteCode P_rand=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_RAND,
		FAKE_END_PROC
	}
};

ByteCode P_nth=
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

ByteCode P_length=
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

ByteCode P_append=
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

ByteCode P_strcat=
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

ByteCode P_bytcat=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_BYTE_CAT,
		FAKE_END_PROC
	}
};

ByteCode P_open=
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

ByteCode P_close=
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

ByteCode P_getc=
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

ByteCode P_getch=
{
	3,
	(char[])
	{
		FAKE_RES_BP,
		FAKE_GETCH,
		FAKE_END_PROC
	}
};

ByteCode P_ungetc=
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

ByteCode P_read=
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

ByteCode P_readb=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_READB,
		FAKE_END_PROC
	}
};

ByteCode P_write=
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

ByteCode P_writeb=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_WRITEB,
		FAKE_END_PROC
	}
};

ByteCode P_princ=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_PRINC,
		FAKE_END_PROC
	}
};

ByteCode P_tell=
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

ByteCode P_seek=
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

ByteCode P_rewind=
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

ByteCode P_exit=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_EXIT,
		FAKE_END_PROC
	}
};

fakeVM* newFakeVM(ByteCode* mainproc,ByteCode* procs)
{
	fakeVM* exe=(fakeVM*)malloc(sizeof(fakeVM));
	if(exe==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
	exe->mainproc=newFakeProcess(newVMcode(mainproc),NULL);
	exe->curproc=exe->mainproc;
	exe->procs=procs;
	exe->stack=newStack(0);
	exe->queue=NULL;
	exe->files=newFileStack();
	return exe;
}

void initGlobEnv(VMenv* obj)
{
	ByteCode buildInProcs[]=
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
		P_isbyt,
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
		P_byt,
		P_add,
		P_sub,
		P_mul,
		P_div,
		P_mod,
		P_rand,
		P_nth,
		P_length,
		P_append,
		P_strcat,
		P_bytcat,
		P_open,
		P_close,
		P_getc,
		P_getch,
		P_ungetc,
		P_read,
		P_readb,
		P_write,
		P_writeb,
		P_princ,
		P_tell,
		P_seek,
		P_rewind,
		P_exit
	};
	obj->size=NUMOFBUILTINSYMBOL;
	obj->values=(VMvalue**)realloc(obj->values,sizeof(VMvalue*)*NUMOFBUILTINSYMBOL);
	if(obj->values==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
	obj->values[0]=NULL;
	obj->values[1]=newVMvalue(INT);
	obj->values[1]->u.num=EOF;
	obj->values[2]=newVMvalue(INT);
	obj->values[2]->u.num=0;
	obj->values[3]=newVMvalue(INT);
	obj->values[3]->u.num=1;
	obj->values[4]=newVMvalue(INT);
	obj->values[4]->u.num=2;
	int i=5;
	for(;i<NUMOFBUILTINSYMBOL;i++)
	{
		obj->values[i]=newVMvalue(PRC);
		obj->values[i]->u.prc=newBuiltInProc(copyByteCode(buildInProcs+(i-5)));
	}
}

void runFakeVM(fakeVM* exe)
{
	while(exe->curproc!=NULL&&exe->curproc->cp!=exe->curproc->code->size)
	{
		VMprocess* curproc=exe->curproc;
		VMcode* tmpCode=curproc->code;
		ByteCode tmpByteCode={tmpCode->size,tmpCode->code};
	//	fprintf(stderr,"%s\n",codeName[tmpCode->code[curproc->cp]].codeName);
		int status=ByteCodes[tmpCode->code[curproc->cp]](exe);
		if(status!=0)
		{
			VMstack* stack=exe->stack;
			printByteCode(&tmpByteCode,stderr);
			putc('\n',stderr);
			printAllStack(exe->stack,stderr);
			putc('\n',stderr);
			fprintf(stderr,"stack->tp==%d,stack->size==%d\n",stack->tp,stack->size);
			fprintf(stderr,"cp=%d stack->bp=%d\n%s\n",curproc->cp,stack->bp,codeName[tmpCode->code[curproc->cp]].codeName);
			printEnv(exe->curproc->localenv,stderr);
			putc('\n',stderr);
			switch(status)
			{
				case 1:
					fprintf(stderr,"error:Wrong arguements!\n");
					exit(EXIT_FAILURE);
				case 2:	
					fprintf(stderr,"error:Stack error!\n");
					exit(EXIT_FAILURE);
				case 3:fprintf(stderr,"error:End of file!\n");
					   exit(EXIT_FAILURE);
			}
		}
	//	fprintf(stdout,"=========\n");
	//	fprintf(stderr,"stack->tp=%d\n",exe->stack->tp);
	//	printAllStack(exe->stack,stderr);
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
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	if(stack->tp>=stack->size)
	{
		stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size+64));
		if(stack->values==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
		stack->size+=64;
	}
	stack->values[stack->tp]=NULL;
	stack->tp+=1;
	proc->cp+=1;
	return 0;
}

int B_push_pair(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	if(stack->tp>=stack->size)
	{
		stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size+64));
		if(stack->values==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
		stack->size+=64;
	}
	VMvalue* objValue=newVMvalue(PAIR);
	//VMvalue* objValue=newVMvalue(PAIR,0,newVMpair());
	//objValue->access=1;
	objValue->u.pair.car=NULL;
	objValue->u.pair.cdr=NULL;
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=1;
	return 0;

}

int B_push_int(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMcode* tmpCode=proc->code;
	if(stack->tp>=stack->size)
	{
		stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size+64));
		if(stack->values==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
		stack->size+=64;
	}
	VMvalue* objValue=newVMvalue(INT);
	//VMvalue* objValue=newVMvalue(INT,1,tmpCode->code+proc->cp+1);
	objValue->u.num=*(int32_t*)(tmpCode->code+proc->cp+1);
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=5;
	return 0;
}

int B_push_chr(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMcode* tmpCode=proc->code;
	if(stack->tp>=stack->size)
	{
		stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size+64));
		if(stack->values==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
		stack->size+=64;
	}
	VMvalue* objValue=newVMvalue(CHR);
	//VMvalue* objValue=newVMvalue(CHR,1,tmpCode->code+proc->cp+1);
	objValue->u.num=*(tmpCode->code+proc->cp+1);
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=2;
	return 0;
}

int B_push_dbl(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMcode* tmpCode=proc->code;
	if(stack->tp>=stack->size)
	{
		stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size+64));
		if(stack->values==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
		stack->size+=64;
	}
	VMvalue* objValue=newVMvalue(DBL);
	//VMvalue* objValue=newVMvalue(DBL,1,tmpCode->code+proc->cp+1);
	objValue->u.dbl=*(double*)(tmpCode->code+proc->cp+1);
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=9;
	return 0;
}

int B_push_str(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMcode* tmpCode=proc->code;
	int len=strlen((char*)(tmpCode->code+proc->cp+1));
	char* tmpStr=(char*)malloc(sizeof(char)*(len+1));
	if(tmpStr==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
	strcpy(tmpStr,tmpCode->code+proc->cp+1);
	if(stack->tp>=stack->size)
	{
		stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size+64));
		if(stack->values==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
		stack->size+=64;
	}
	VMvalue* objValue=newVMvalue(STR);
	//VMvalue* objValue=newVMvalue(SYM,1,tmpCode->code+proc->cp+1);
	objValue->u.str=tmpStr;
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=2+len;
	return 0;
}

int B_push_sym(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMcode* tmpCode=proc->code;
	int len=strlen((char*)(tmpCode->code+proc->cp+1));
	char* tmpStr=(char*)malloc(sizeof(char)*(len+1));
	if(tmpStr==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
	strcpy(tmpStr,tmpCode->code+proc->cp+1);
	if(stack->tp>=stack->size)
	{
		stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size+64));
		if(stack->values==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
		stack->size+=64;
	}
	VMvalue* objValue=newVMvalue(SYM);
	//VMvalue* objValue=newVMvalue(SYM,1,tmpCode->code+proc->cp+1);
	objValue->u.str=tmpStr;
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=2+len;
	return 0;
}

int B_push_byte(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMcode* tmpCode=proc->code;
	int32_t size=*(int32_t*)(tmpCode->code+proc->cp+1);
	uint8_t* tmp=createByteArry(size);
	memcpy(tmp,tmpCode->code+proc->cp+5,size);
	if(stack->tp>=stack->size)
	{
		stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size+64));
		if(stack->values==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
		stack->size+=64;
	}
	VMvalue* objValue=newVMvalue(BYTE);
	objValue->u.byte.size=size;
	objValue->u.byte.arry=tmp;
	//ByteArry tmpArry={size,tmp};
	//VMvalue* objValue=newVMvalue(BYTE,1,&tmp);
	//free(tmp);
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=5+size;
	return 0;
}

int B_push_var(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMcode* tmpCode=proc->code;
	if(stack->tp>=stack->size)
	{
		stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size+64));
		if(stack->values==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
		stack->size+=64;
	}
	int32_t countOfVar=*(int32_t*)(tmpCode->code+proc->cp+1);
	VMenv* curEnv=proc->localenv;
	while(curEnv->bound==-1||countOfVar<curEnv->bound)curEnv=curEnv->prev;
	VMvalue* tmpValue=copyValue(*(curEnv->values+countOfVar-(curEnv->bound)));
	stack->values[stack->tp]=tmpValue;
	stack->tp+=1;
	proc->cp+=5;
	return 0;
}

int B_push_car(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* objValue=getTopValue(stack);
	if(objValue==NULL||(objValue->type!=PAIR&&objValue->type!=STR&&objValue->type!=BYTE))return 1;
	if(objValue->type==PAIR)
	{
		stack->values[stack->tp-1]=copyValue(getCar(objValue));
		//stack->values[stack->tp-1]=getCar(objValue);
	}
	else if(objValue->type==STR)
	{
		VMvalue* ch=newVMvalue(CHR);
		//VMvalue* ch=newVMvalue(CHR,0,&objValue->u.str[0]);
		ch->u.chr=objValue->u.str[0];
		stack->values[stack->tp-1]=ch;
	}
	else if(objValue->type==BYTE)
	{
		VMvalue* bt=newVMvalue(BYTE);
		//VMvalue* bt=newVMvalue(BYTE,0,newByteArry(1,objValue->u.byte->arry,0));
		bt->u.byte.size=1;
		bt->u.byte.arry=createByteArry(1);
		bt->u.byte.arry[0]=objValue->u.byte.arry[0];
		stack->values[stack->tp-1]=bt;
	}
	freeVMvalue(objValue);
	proc->cp+=1;
	return 0;
}

int B_push_cdr(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* objValue=getTopValue(stack);
	if(objValue==NULL||(objValue->type!=PAIR&&objValue->type!=STR&&objValue->type!=BYTE))return 1;
	if(objValue->type==PAIR)
	{
		stack->values[stack->tp-1]=copyValue(getCdr(objValue));
		//stack->values[stack->tp-1]=getCdr(objValue);
	}
	else if(objValue->type==STR)
	{
		char* str=objValue->u.str;
		if(strlen(str)==0)stack->values[stack->tp-1]=NULL;
		else
		{
			VMvalue* tmpStr=newVMvalue(STR);
			//VMvalue* tmpStr=newVMvalue(STR,0,objValue->u.str+1);
			tmpStr->u.str=copyStr(objValue->u.str+1);
			stack->values[stack->tp-1]=tmpStr;
		}
	}
	else if(objValue->type==BYTE)
	{
		if(objValue->u.byte.size==1)stack->values[stack->tp-1]=NULL;
		else
		{
			VMvalue* bt=newVMvalue(BYTE);
			//VMvalue* bt=newVMvalue(BYTE,0,newByteArry(objValue->u.byte->size-1,objValue->u.byte->arry+1,0));
			int32_t size=objValue->u.byte.size-1;
			bt->u.byte.size=size;
			bt->u.byte.arry=createByteArry(size);
			memcpy(bt->u.byte.arry,objValue->u.byte.arry+1,size);
			stack->values[stack->tp-1]=bt;
		}
	}
	freeVMvalue(objValue);
	proc->cp+=1;
	return 0;
}

int B_push_top(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	if(stack->tp==stack->bp)return 1;
	if(stack->tp>=stack->size)
	{
		stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size+64));
		if(stack->values==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
		stack->size+=64;
	}
	stack->values[stack->tp]=copyValue(getTopValue(stack));
	//stack->values[stack->tp]=getTopValue(stack);
	stack->tp+=1;
	proc->cp+=1;
	return 0;
}

int B_push_proc(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMcode* tmpCode=proc->code;
	int32_t countOfProc=*(int32_t*)(tmpCode->code+proc->cp+1);
	if(stack->tp>=stack->size)
	{
		stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size+64));
		if(stack->values==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
		stack->size+=64;
	}
	VMvalue* objValue=newVMvalue(PRC);
	//VMvalue* objValue=newVMvalue(PRC,1,newVMcode(exe->procs+countOfProc));
	objValue->u.prc=newVMcode(exe->procs+countOfProc);
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=5;
	return 0;
}

int B_pop(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	freeVMvalue(getTopValue(stack));
	stack->values[stack->tp-1]=NULL;
	stack->tp-=1;
	stackRecycle(exe);
	proc->cp+=1;
	return 0;
}

int B_pop_var(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMcode* tmpCode=proc->code;
	if(!(stack->tp>stack->bp))return 2;
	int32_t countOfVar=*(int32_t*)(tmpCode->code+proc->cp+1);
	VMenv* curEnv=proc->localenv;
	while(curEnv->bound==-1||countOfVar<curEnv->bound)curEnv=curEnv->prev;
	VMvalue** pValue=NULL;
	if(countOfVar-curEnv->bound>=curEnv->size)
	{
		curEnv->size+=1;
		curEnv->values=(VMvalue**)realloc(curEnv->values,sizeof(VMvalue*)*curEnv->size);
		pValue=curEnv->values+countOfVar-(curEnv->bound);
	}
	else
	{
		pValue=curEnv->values+countOfVar-(curEnv->bound);
		freeVMvalue(*pValue);
	}
	*pValue=getTopValue(stack);
	stack->values[stack->tp-1]=NULL;
	stack->tp-=1;
	stackRecycle(exe);
	proc->cp+=5;
	return 0;
}

int B_pop_rest_var(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMcode* tmpCode=proc->code;
	int32_t countOfVar=*(int32_t*)(tmpCode->code+proc->cp+1);
	VMenv* curEnv=proc->localenv;
	while(curEnv->bound==-1||countOfVar<curEnv->bound)curEnv=curEnv->prev;
	VMvalue** tmpValue=NULL;
	if(countOfVar-curEnv->bound>=curEnv->size)
	{
		curEnv->size+=1;
		curEnv->values=(VMvalue**)realloc(curEnv->values,sizeof(VMvalue*)*curEnv->size);
		tmpValue=curEnv->values+countOfVar-(curEnv->bound);
	}
	else
	{
		tmpValue=curEnv->values+countOfVar-(curEnv->bound);
		freeVMvalue(*tmpValue);
	}
	VMvalue* obj=newVMvalue(PAIR);
	VMvalue* tmp=obj;
	for(;;)
	{
		if(stack->tp>stack->bp)
		{
			VMvalue* topValue=getTopValue(stack);
			tmp->u.pair.car=copyValue(topValue);
			stack->tp-=1;
			stackRecycle(exe);
			tmp->u.pair.cdr=newVMvalue(PAIR);
			tmp=tmp->u.pair.cdr;
		}
		else break;
	}
	*tmpValue=copyValue(obj);
	freeVMvalue(obj);
	proc->cp+=5;
	return 0;
}

int B_pop_car(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	VMvalue* objValue=getValue(stack,stack->tp-2);
	if(objValue==NULL||objValue->type!=PAIR)return 1;
	freeVMvalue(objValue->u.pair.car);
	objValue->u.pair.car=copyValue(topValue);
	//objValue->u.pair->car=topValue;
	freeVMvalue(topValue);
	stack->tp-=1;
	stackRecycle(exe);
	proc->cp+=1;
	return 0;
}

int B_pop_cdr(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	VMvalue* objValue=getValue(stack,stack->tp-2);
	if(objValue==NULL||objValue->type!=PAIR)return 1;
	freeVMvalue(objValue->u.pair.cdr);
	objValue->u.pair.cdr=copyValue(topValue);
	freeVMvalue(topValue);
	stack->tp-=1;
	stackRecycle(exe);
	proc->cp+=1;
	return 0;
}

int B_add(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* firValue=getValue(stack,stack->tp-1);
	VMvalue* secValue=getValue(stack,stack->tp-2);
	if((firValue==NULL||secValue==NULL)||(firValue->type!=INT&&firValue->type!=DBL)||(secValue->type!=INT&&secValue->type!=DBL))return 1;
	if(firValue->type==DBL||secValue->type==DBL)
	{
		double result=((firValue->type==DBL)?firValue->u.dbl:firValue->u.num)+((secValue->type==DBL)?secValue->u.dbl:secValue->u.num);
		VMvalue* tmpValue=newVMvalue(DBL);
		tmpValue->u.dbl=result;
		stack->tp-=1;
		stackRecycle(exe);
		stack->values[stack->tp-1]=tmpValue;
	}
	else if(firValue->type==INT&&secValue->type==INT)
	{
		int32_t result=firValue->u.num+secValue->u.num;
		VMvalue* tmpValue=newVMvalue(INT);
		tmpValue->u.num=result;
		stack->tp-=1;
		stackRecycle(exe);
		stack->values[stack->tp-1]=tmpValue;
	}
	freeVMvalue(firValue);
	freeVMvalue(secValue);
	proc->cp+=1;
	return 0;
}

int B_sub(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* firValue=getValue(stack,stack->tp-1);
	VMvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(exe);
	if((firValue==NULL||secValue==NULL)||(firValue->type!=INT&&firValue->type!=DBL)||(secValue->type!=INT&&secValue->type!=DBL))return 1;
	if(firValue->type==DBL||secValue->type==DBL)
	{
		double result=((secValue->type==DBL)?secValue->u.dbl:secValue->u.num)-((firValue->type==DBL)?firValue->u.dbl:firValue->u.num);
		VMvalue* tmpValue=newVMvalue(DBL);
		tmpValue->u.dbl=result;
		stack->values[stack->tp-1]=tmpValue;
	}
	else if(firValue->type==INT&&secValue->type==INT)
	{
		int32_t result=secValue->u.num-firValue->u.num;
		VMvalue* tmpValue=newVMvalue(INT);
		tmpValue->u.num=result;
		stack->values[stack->tp-1]=tmpValue;
	}
	freeVMvalue(firValue);
	freeVMvalue(secValue);
	proc->cp+=1;
	return 0;
}

int B_mul(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* firValue=getValue(stack,stack->tp-1);
	VMvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(exe);
	if((firValue==NULL||secValue==NULL)||(firValue->type!=INT&&firValue->type!=DBL)||(secValue->type!=INT&&secValue->type!=DBL))return 1;
	if(firValue->type==DBL||secValue->type==DBL)
	{
		double result=((firValue->type==DBL)?firValue->u.dbl:firValue->u.num)*((secValue->type==DBL)?secValue->u.dbl:secValue->u.num);
		VMvalue* tmpValue=newVMvalue(DBL);
		tmpValue->u.dbl=result;
		stack->values[stack->tp-1]=tmpValue;
	}
	else if(firValue->type==INT&&secValue->type==INT)
	{
		int32_t result=firValue->u.num*secValue->u.num;
		VMvalue* tmpValue=newVMvalue(INT);
		tmpValue->u.num=result;
		stack->values[stack->tp-1]=tmpValue;
	}
	freeVMvalue(firValue);
	freeVMvalue(secValue);
	proc->cp+=1;
	return 0;
}

int B_div(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* firValue=getValue(stack,stack->tp-1);
	VMvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(exe);
	if((firValue==NULL||secValue==NULL)||(firValue->type!=INT&&firValue->type!=DBL)||(secValue->type!=INT&&secValue->type!=DBL))return 1;
	double result=((secValue->type==DBL)?secValue->u.dbl:secValue->u.num)/((firValue->type==DBL)?firValue->u.dbl:firValue->u.num);
	VMvalue* tmpValue=newVMvalue(DBL);
	tmpValue->u.dbl=result;
	stack->values[stack->tp-1]=tmpValue;
	freeVMvalue(firValue);
	freeVMvalue(secValue);
	proc->cp+=1;
	return 0;
}

int B_mod(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* firValue=getValue(stack,stack->tp-1);
	VMvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(exe);
	if((firValue==NULL||secValue==NULL)||(firValue->type!=INT&&firValue->type!=DBL)||(secValue->type!=INT&&secValue->type!=DBL))return 1;
	int32_t result=((int32_t)((secValue->type==DBL)?secValue->u.dbl:secValue->u.num))%((int32_t)((firValue->type==DBL)?firValue->u.dbl:firValue->u.num));
	VMvalue* tmpValue=newVMvalue(INT);
	tmpValue->u.num=result;
	stack->values[stack->tp-1]=tmpValue;
	freeVMvalue(firValue);
	freeVMvalue(secValue);
	proc->cp+=1;
	return 0;
}

int B_rand(fakeVM* exe)
{
	srand((unsigned)time(NULL));
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* lim=getTopValue(stack);
	if(lim==NULL||lim->type!=INT)return 1;
	int32_t result=rand()%(lim->u.num);
	lim->u.num=result;
	proc->cp+=1;
	return 0;
}

int B_atom(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	if(topValue==NULL||topValue->type!=PAIR)
	{
		VMvalue* TRUE=newVMvalue(INT);
		TRUE->u.num=1;
		stack->values[stack->tp-1]=TRUE;
	}
	else
	{
		if(topValue->u.pair.car==NULL&&topValue->u.pair.cdr==NULL)
		{
			VMvalue* TRUE=newVMvalue(INT);
			TRUE->u.num=1;
			stack->values[stack->tp-1]=TRUE;
		}
		else
			stack->values[stack->tp-1]=NULL;
	}
	freeVMvalue(topValue);
	proc->cp+=1;
	return 0;
}

int B_null(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	if(topValue==NULL||(topValue->type==PAIR&&(topValue->u.pair.car==NULL&&topValue->u.pair.cdr==NULL)))
	{
		VMvalue* TRUE=newVMvalue(INT);
		TRUE->u.num=1;
		stack->values[stack->tp-1]=TRUE;
	}
	else stack->values[stack->tp-1]=NULL;
	freeVMvalue(topValue);
	proc->cp+=1;
	return 0;
}

int B_init_proc(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMcode* tmpCode=proc->code;
	VMvalue* topValue=getTopValue(stack);
	if(topValue->type!=PRC)return 1;
	int32_t boundOfProc=*(int32_t*)(tmpCode->code+proc->cp+1);
	topValue->u.prc->localenv=newVMenv(boundOfProc,1,tmpCode->localenv);
	proc->cp+=5;
	return 0;
}

int B_end_proc(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMvalue* tmpValue=getTopValue(stack);
	VMprocess* tmpProc=exe->curproc;
//	fprintf(stdout,"End proc: %p\n",tmpProc->code);
	exe->curproc=exe->curproc->prev;
	VMcode* tmpCode=tmpProc->code;
	VMenv* tmpEnv=tmpCode->localenv;
	if(tmpValue!=NULL&&tmpValue->type==PRC&&tmpValue->u.prc->localenv->prev==tmpEnv)tmpEnv->next=tmpValue->u.prc->localenv;
	if(tmpCode==tmpProc->prev->code)tmpCode->localenv=tmpProc->prev->localenv;
	else tmpCode->localenv=newVMenv(tmpEnv->bound,1,tmpEnv->prev);
	if(tmpEnv->prev!=NULL&&tmpEnv->prev->next==tmpEnv&&tmpEnv->next==NULL)tmpEnv->prev->next=tmpCode->localenv;
	tmpEnv->inProc=0;
	freeVMenv(tmpEnv);
	if(tmpCode->refcount==0)freeVMcode(tmpCode);
	else tmpCode->refcount-=1;
	free(tmpProc);
	tmpProc=NULL;
	return 0;
}

int B_set_bp(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* prevBp=newVMvalue(INT);
	prevBp->u.num=stack->bp;
	if(stack->tp>=stack->size)
	{
		stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size+64));
		if(stack->values==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
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
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	if(stack->tp>stack->bp)return 2;
	VMvalue* prevBp=getTopValue(stack);
	stack->bp=prevBp->u.num;
	freeVMvalue(prevBp);
	stack->tp-=1;
	stackRecycle(exe);
	proc->cp+=1;
	return 0;
}

int B_invoke(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* tmpValue=getTopValue(stack);
	if(tmpValue==NULL||tmpValue->type!=PRC)return 1;
	proc->cp+=1;
	VMcode* tmpCode=tmpValue->u.prc;
	VMprocess* prevProc=hasSameProc(tmpCode,proc);
	if(isTheLastExpress(proc,prevProc)&&prevProc)
		prevProc->cp=0;
	else 
	{
		VMprocess* tmpProc=newFakeProcess(tmpCode,proc);
		if(tmpCode->localenv->prev!=NULL&&tmpCode->localenv->prev->next!=tmpCode->localenv)
		{
			tmpProc->localenv=newVMenv(tmpCode->localenv->bound,1,tmpCode->localenv->prev);
			if(!hasSameProc(tmpCode,proc))freeVMenv(tmpCode->localenv);
			tmpCode->localenv=tmpProc->localenv;
		}
		else tmpProc->localenv=tmpCode->localenv;
		exe->curproc=tmpProc;
	}
	free(tmpValue);
	stack->tp-=1;
	stackRecycle(exe);
	return 0;
}

int B_jump_if_ture(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMcode* tmpCode=proc->code;
	VMvalue* tmpValue=getTopValue(stack);
	if(!(tmpValue==NULL||(tmpValue->type==PAIR&&tmpValue->u.pair.car==NULL&&tmpValue->u.pair.cdr==NULL)))
	{
		int32_t where=*(int32_t*)(tmpCode->code+proc->cp+sizeof(char));
		proc->cp+=where;
	}
	else
	{
		stack->tp-=1;
		stackRecycle(exe);
		freeVMvalue(tmpValue);
	}
	proc->cp+=5;
	return 0;
}

int B_jump_if_false(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMcode* tmpCode=proc->code;
	VMvalue* tmpValue=getTopValue(stack);
	stackRecycle(exe);
	if(tmpValue==NULL||(tmpValue->type==PAIR&&tmpValue->u.pair.car==NULL&&tmpValue->u.pair.cdr==NULL))
	{
		int32_t where=*(int32_t*)(tmpCode->code+proc->cp+sizeof(char));
		proc->cp+=where;
	}
/*	else
	{
		stack->tp-=1;
		stackRecycle(exe);
		freeVMvalue(tmpValue);
	}*/
	proc->cp+=5;
	return 0;
}

int B_jump(fakeVM* exe)
{
	VMprocess* proc=exe->curproc;
	VMcode* tmpCode=proc->code;
	int32_t where=*(int32_t*)(tmpCode->code+proc->cp+sizeof(char));
	proc->cp+=where+5;
	return 0;
}

int B_open(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	filestack* files=exe->files;
	VMvalue* mode=getTopValue(stack);
	VMvalue* filename=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(exe);
	if(filename==NULL||mode==NULL||filename->type!=STR||mode->type!=STR)return 1;
	int32_t i=0;
	FILE* file=fopen(filename->u.str,mode->u.str);
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
	VMvalue* countOfFile=newVMvalue(INT);
	countOfFile->u.num=i;
	stack->values[stack->tp-1]=countOfFile;
	freeVMvalue(filename);
	freeVMvalue(mode);
	proc->cp+=1;
	return 0;
}

int B_close(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	filestack* files=exe->files;
	VMvalue* topValue=getTopValue(stack);
	if(topValue==NULL||topValue->type!=INT)return 1;
	if(topValue->u.num>=files->size)return 2;
	FILE* objFile=files->files[topValue->u.num];
	if(fclose(objFile)==EOF)
	{
		files->files[topValue->u.num]=NULL;
		topValue->u.num=-1;
	}
	else files->files[topValue->u.num]=NULL;
	proc->cp+=1;
	return 0;
}

int B_eq(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* firValue=getTopValue(stack);
	VMvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(exe);
	if(VMvaluecmp(firValue,secValue))
	{
		VMvalue* tmpValue=newVMvalue(INT);
		tmpValue->u.num=1;
		stack->values[stack->tp-1]=tmpValue;
	}
	else stack->values[stack->tp-1]=NULL;
	freeVMvalue(firValue);
	freeVMvalue(secValue);
	proc->cp+=1;
	return 0;
}

int B_gt(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* firValue=getTopValue(stack);
	VMvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(exe);
	if(firValue==NULL||secValue==NULL||firValue->type==PAIR||firValue->type==PRC||secValue->type==PAIR||secValue->type==PRC)return 1;
	if(firValue->type==DBL||secValue->type==DBL)
	{
		double result=((secValue->type==DBL)?secValue->u.dbl:secValue->u.num)-((firValue->type==DBL)?firValue->u.dbl:firValue->u.num);
		if(result>0)
		{
			VMvalue* tmpValue=newVMvalue(INT);
			tmpValue->u.num=1;
			stack->values[stack->tp-1]=tmpValue;
		}
		else stack->values[stack->tp-1]=NULL;
	}
	else if(firValue->type==INT&&secValue->type==INT)
	{
		if(secValue->u.num>firValue->u.num)
		{
			VMvalue* tmpValue=newVMvalue(INT);
			tmpValue->u.num=1;
			stack->values[stack->tp-1]=tmpValue;
		}
		else stack->values[stack->tp-1]=NULL;
	}
	else if(firValue->type==secValue->type&&(firValue->type==SYM||firValue->type==STR))
	{
		if(strcmp(secValue->u.str,firValue->u.str)>0)
		{
			VMvalue* tmpValue=newVMvalue(INT);
			tmpValue->u.num=1;
			stack->values[stack->tp-1]=tmpValue;
		}
		else stack->values[stack->tp-1]=NULL;
	}
	else return 1;
	freeVMvalue(firValue);
	freeVMvalue(secValue);
	proc->cp+=1;
	return 0;
}

int B_ge(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* firValue=getTopValue(stack);
	VMvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(exe);
	if(firValue==NULL||secValue==NULL||firValue->type==PAIR||firValue->type==PRC||secValue->type==PAIR||secValue->type==PRC)return 1;
	if(firValue->type==DBL||secValue->type==DBL)
	{
		double result=((secValue->type==DBL)?secValue->u.dbl:secValue->u.num)-((firValue->type==DBL)?firValue->u.dbl:firValue->u.num);
		if(result>=0)
		{
			VMvalue* tmpValue=newVMvalue(INT);
			tmpValue->u.num=1;
			stack->values[stack->tp-1]=tmpValue;
		}
		else stack->values[stack->tp-1]=NULL;
	}
	else if(firValue->type==INT&&secValue->type==INT)
	{
		if(secValue->u.num>=firValue->u.num)
		{
			VMvalue* tmpValue=newVMvalue(INT);
			tmpValue->u.num=1;
			stack->values[stack->tp-1]=tmpValue;
		}
		else stack->values[stack->tp-1]=NULL;
	}
	else if(firValue->type==secValue->type&&(firValue->type==SYM||firValue->type==STR))
	{
		if(strcmp(secValue->u.str,firValue->u.str)>=0)
		{
			VMvalue* tmpValue=newVMvalue(INT);
			tmpValue->u.num=1;
			stack->values[stack->tp-1]=tmpValue;
		}
		else stack->values[stack->tp-1]=NULL;
	}
	else return 1;
	freeVMvalue(firValue);
	freeVMvalue(secValue);
	proc->cp+=1;
	return 0;
}

int B_lt(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* firValue=getTopValue(stack);
	VMvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(exe);
	if(firValue==NULL||secValue==NULL||firValue->type==PAIR||firValue->type==PRC||secValue->type==PAIR||secValue->type==PRC)return 1;
	if(firValue->type==DBL||secValue->type==DBL)
	{
		double result=((secValue->type==DBL)?secValue->u.dbl:secValue->u.num)-((firValue->type==DBL)?firValue->u.dbl:firValue->u.num);
		if(result<0)
		{
			VMvalue* tmpValue=newVMvalue(INT);
			tmpValue->u.num=1;
			stack->values[stack->tp-1]=tmpValue;
		}
		else stack->values[stack->tp-1]=NULL;
	}
	else if(firValue->type==INT&&secValue->type==INT)
	{
		if(secValue->u.num<firValue->u.num)
		{
			VMvalue* tmpValue=newVMvalue(INT);
			tmpValue->u.num=1;
			stack->values[stack->tp-1]=tmpValue;
		}
		else stack->values[stack->tp-1]=NULL;
	}
	else if(firValue->type==secValue->type&&(firValue->type==SYM||firValue->type==STR))
	{
		if(strcmp(secValue->u.str,firValue->u.str)<0)
		{
			VMvalue* tmpValue=newVMvalue(INT);
			tmpValue->u.num=1;
			stack->values[stack->tp-1]=tmpValue;
		}
		else stack->values[stack->tp-1]=NULL;
	}
	else return 1;
	freeVMvalue(firValue);
	freeVMvalue(secValue);
	proc->cp+=1;
	return 0;
}

int B_le(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* firValue=getTopValue(stack);
	VMvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(exe);
	if(firValue==NULL||secValue==NULL||firValue->type==PAIR||firValue->type==PRC||secValue->type==PAIR||secValue->type==PRC)return 1;
	if(firValue->type==DBL||secValue->type==DBL)
	{
		double result=((secValue->type==DBL)?secValue->u.dbl:secValue->u.num)-((firValue->type==DBL)?firValue->u.dbl:firValue->u.num);
		if(result<=0)
		{
			VMvalue* tmpValue=newVMvalue(INT);
			tmpValue->u.num=1;
			stack->values[stack->tp-1]=tmpValue;
		}
		else stack->values[stack->tp-1]=NULL;
	}
	else if(firValue->type==INT&&secValue->type==INT)
	{
		if(secValue->u.num<=firValue->u.num)
		{
			VMvalue* tmpValue=newVMvalue(INT);
			tmpValue->u.num=1;
			stack->values[stack->tp-1]=tmpValue;
		}
		else stack->values[stack->tp-1]=NULL;
	}
	else if(firValue->type==secValue->type&&(firValue->type==SYM||firValue->type==STR))
	{
		if(strcmp(secValue->u.str,firValue->u.str)<=0)
		{
			VMvalue* tmpValue=newVMvalue(INT);
			tmpValue->u.num=1;
			stack->values[stack->tp-1]=tmpValue;
		}
		else stack->values[stack->tp-1]=NULL;
	}
	else return 1;
	freeVMvalue(firValue);
	freeVMvalue(secValue);
	proc->cp+=1;
	return 0;
}

int B_not(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* tmpValue=getTopValue(stack);
	if(tmpValue==NULL||(tmpValue->type==PAIR&&tmpValue->u.pair.car==NULL&&tmpValue->u.pair.cdr==NULL))
	{
		freeVMvalue(tmpValue);
		tmpValue=newVMvalue(INT);
		tmpValue->u.num=1;
		stack->values[stack->tp-1]=tmpValue;
	}
	else
	{
		freeVMvalue(tmpValue);
		stack->values[stack->tp-1]=NULL;
	}
	proc->cp+=1;
	return 0;
}

int B_cast_to_chr(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	if(topValue==NULL||topValue->type==PAIR||topValue->type==PRC)return 1;
	VMvalue* tmpValue=newVMvalue(CHR);
	switch(topValue->type)
	{
		case INT:tmpValue->u.chr=topValue->u.num;break;
		case DBL:tmpValue->u.chr=(int32_t)topValue->u.dbl;break;
		case CHR:tmpValue->u.chr=topValue->u.chr;break;
		case STR:
		case SYM:tmpValue->u.chr=topValue->u.str[0];break;
		case BYTE:tmpValue->u.dbl=*(char*)topValue->u.byte.arry;break;
	}
	stack->values[stack->tp-1]=tmpValue;
	freeVMvalue(topValue);
	proc->cp+=1;
	return 0;
}

int B_cast_to_int(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	if(topValue==NULL||topValue->type==PAIR||topValue->type==PRC)return 1;
	VMvalue* tmpValue=newVMvalue(INT);
	switch(topValue->type)
	{
		case INT:tmpValue->u.num=topValue->u.num;break;
		case DBL:tmpValue->u.num=(int32_t)topValue->u.dbl;break;
		case CHR:tmpValue->u.num=topValue->u.chr;break;
		case STR:
		case SYM:tmpValue->u.num=stringToInt(topValue->u.str);break;
		case BYTE:tmpValue->u.dbl=*(int32_t*)topValue->u.byte.arry;break;
	}
	stack->values[stack->tp-1]=tmpValue;
	freeVMvalue(topValue);
	proc->cp+=1;
	return 0;
}

int B_cast_to_dbl(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	if(topValue==NULL||topValue->type==PAIR||topValue->type==PRC)return 1;
	VMvalue* tmpValue=newVMvalue(DBL);
	switch(topValue->type)
	{
		case INT:tmpValue->u.dbl=(double)topValue->u.num;break;
		case DBL:tmpValue->u.dbl=topValue->u.dbl;break;
		case CHR:tmpValue->u.dbl=(double)(int32_t)topValue->u.chr;break;
		case STR:
		case SYM:tmpValue->u.dbl=stringToDouble(topValue->u.str);break;
		case BYTE:tmpValue->u.dbl=*(double*)topValue->u.byte.arry;break;
	}
	stack->values[stack->tp-1]=tmpValue;
	freeVMvalue(topValue);
	proc->cp+=1;
	return 0;
}

int B_cast_to_str(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	if(topValue==NULL||topValue->type==PAIR||topValue->type==PRC)return 1;
	VMvalue* tmpValue=newVMvalue(STR);
	switch(topValue->type)
	{
		case INT:tmpValue->u.str=intToString(topValue->u.num);break;
		case DBL:tmpValue->u.str=doubleToString(topValue->u.dbl);break;
		case CHR:tmpValue->u.str=(char*)malloc(sizeof(char)*2);
				 if(tmpValue->u.str==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
				 tmpValue->u.str[0]=topValue->u.chr;
				 tmpValue->u.str[1]='\0';
				 break;
		case STR:
		case SYM:tmpValue->u.str=copyStr(topValue->u.str);break;
		case BYTE:tmpValue->u.str=copyStr(topValue->u.byte.arry);break;
	}
	stack->values[stack->tp-1]=tmpValue;
	freeVMvalue(topValue);
	proc->cp+=1;
	return 0;
}

int B_cast_to_sym(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	if(topValue==NULL||topValue->type==PAIR||topValue->type==PRC)return 1;
	VMvalue* tmpValue=newVMvalue(SYM);
	switch(topValue->type)
	{
		case INT:tmpValue->u.str=intToString(topValue->u.num);break;
		case DBL:tmpValue->u.str=doubleToString(topValue->u.dbl);break;
		case CHR:tmpValue->u.str=(char*)malloc(sizeof(char)*2);
				 if(tmpValue->u.str==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
				 tmpValue->u.str[0]=topValue->u.chr;
				 tmpValue->u.str[1]='\0';
				 break;
		case STR:
		case SYM:tmpValue->u.str=copyStr(topValue->u.str);break;
		case BYTE:tmpValue->u.str=copyStr(topValue->u.byte.arry);break;
	}
	stack->values[stack->tp-1]=tmpValue;
	freeVMvalue(topValue);
	proc->cp+=1;
	return 0;
}

int B_cast_to_byte(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	if(topValue==NULL||topValue->type==PAIR||topValue->type==PRC)return 1;
	VMvalue* tmpValue=newVMvalue(BYTE);
	switch(topValue->type)
	{
		case INT:tmpValue->u.byte.size=sizeof(int32_t);
				 tmpValue->u.byte.arry=createByteArry(sizeof(int32_t));
				 *(int32_t*)tmpValue->u.byte.arry=topValue->u.num;
				 break;
		case DBL:tmpValue->u.byte.size=sizeof(double);
				 tmpValue->u.byte.arry=createByteArry(sizeof(double));
				 *(double*)tmpValue->u.byte.arry=topValue->u.dbl;
				 break;
		case CHR:tmpValue->u.byte.size=sizeof(char);
				 tmpValue->u.byte.arry=createByteArry(sizeof(char));
				 *(char*)tmpValue->u.byte.arry=topValue->u.chr;
				 break;
		case STR:
		case SYM:tmpValue->u.byte.size=strlen(tmpValue->u.str+1);
				 tmpValue->u.byte.arry=copyStr(topValue->u.str);
				 break;
		case BYTE:tmpValue->u.byte.size=topValue->u.byte.size;
				  tmpValue->u.byte.arry=createByteArry(topValue->u.byte.size);
				  memcpy(tmpValue->u.byte.arry,topValue->u.byte.arry,topValue->u.byte.size);
				  break;
	}
	stack->values[stack->tp-1]=tmpValue;
	freeVMvalue(topValue);
	proc->cp+=1;
	return 0;
}

int B_is_chr(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* objValue=getTopValue(stack);
	if(objValue!=NULL&&objValue->type==CHR)
	{
		VMvalue* tmpValue=newVMvalue(INT);
		tmpValue->u.num=1;
		stack->values[stack->tp-1]=tmpValue;
	}
	else stack->values[stack->tp-1]=NULL;
	freeVMvalue(objValue);
	proc->cp+=1;
	return 0;
}

int B_is_int(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* objValue=getTopValue(stack);
	if(objValue!=NULL&&objValue->type==INT)
	{
		VMvalue* tmpValue=newVMvalue(INT);
		tmpValue->u.num=1;
		stack->values[stack->tp-1]=tmpValue;
	}
	else stack->values[stack->tp-1]=NULL;
	freeVMvalue(objValue);
	proc->cp+=1;
	return 0;
}

int B_is_dbl(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* objValue=getTopValue(stack);
	if(objValue!=NULL&&objValue->type==DBL)
	{
		VMvalue* tmpValue=newVMvalue(INT);
		tmpValue->u.num=1;
		stack->values[stack->tp-1]=tmpValue;
	}
	else stack->values[stack->tp-1]=NULL;
	freeVMvalue(objValue);
	proc->cp+=1;
	return 0;
}

int B_is_str(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* objValue=getTopValue(stack);
	if(objValue!=NULL&&objValue->type==STR)
	{
		VMvalue* tmpValue=newVMvalue(INT);
		tmpValue->u.num=1;
		stack->values[stack->tp-1]=tmpValue;
	}
	else stack->values[stack->tp-1]=NULL;
	freeVMvalue(objValue);
	proc->cp+=1;
	return 0;
}

int B_is_sym(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* objValue=getTopValue(stack);
	if(objValue!=NULL&&objValue->type==SYM)
	{
		VMvalue* tmpValue=newVMvalue(INT);
		tmpValue->u.num=1;
		stack->values[stack->tp-1]=tmpValue;
	}
	else stack->values[stack->tp-1]=NULL;
	freeVMvalue(objValue);
	proc->cp+=1;
	return 0;
}

int B_is_prc(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* objValue=getTopValue(stack);
	if(objValue!=NULL&&objValue->type==PRC)
	{
		VMvalue* tmpValue=newVMvalue(INT);
		tmpValue->u.num=1;
		stack->values[stack->tp-1]=tmpValue;
	}
	else stack->values[stack->tp-1]=NULL;
	freeVMvalue(objValue);
	proc->cp+=1;
	return 0;
}

int B_is_byte(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	if(topValue!=NULL&&topValue->type==BYTE)
	{
		VMvalue* tmpValue=newVMvalue(INT);
		tmpValue->u.num=1;
		stack->values[stack->tp-1]=tmpValue;
	}
	else stack->values[stack->tp-1]=NULL;
	freeVMvalue(topValue);
	proc->cp+=1;
	return 0;
}

int B_nth(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* place=getValue(stack,stack->tp-2);
	VMvalue* objlist=getTopValue(stack);
	if(objlist==NULL||place==NULL||(objlist->type!=PAIR&&objlist->type!=STR&&objlist->type!=BYTE)||place->type!=INT)return 1;
	if(objlist->type==PAIR)
	{
		VMvalue* obj=getCar(objlist);
		objlist->u.pair.car=NULL;
		VMvalue* objPair=getCdr(objlist);
		int i=0;
		for(;i<place->u.num;i++)
		{
			if(objPair==NULL)return 2;
			freeVMvalue(obj);
			obj=getCar(objPair);
			objPair->u.pair.car=NULL;
			objPair=getCdr(objPair);
		}
		stack->tp-=1;
		stackRecycle(exe);
		stack->values[stack->tp-1]=obj;
	}
	else if(objlist->type==STR)
	{
		stack->tp-=1;
		stackRecycle(exe);
		VMvalue* objChr=newVMvalue(CHR);
		objChr->u.chr=objlist->u.str[place->u.num];
		stack->values[stack->tp-1]=objChr;
	}
	else if(objlist->type==BYTE)
	{
		stack->tp-=1;
		stackRecycle(exe);
		VMvalue* objByte=newVMvalue(BYTE);
		objByte->u.byte.size=1;
		objByte->u.byte.arry=createByteArry(sizeof(uint8_t));
		objByte->u.byte.arry[0]=objlist->u.byte.arry[place->u.num];
		stack->values[stack->tp-1]=objByte;
	}
	freeVMvalue(objlist);
	freeVMvalue(place);
	proc->cp+=1;
	return 0;
}

int B_length(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* objlist=getTopValue(stack);
	if(objlist==NULL||(objlist->type!=PAIR&&objlist->type!=STR&&objlist->type!=BYTE))return 1;
	if(objlist->type==PAIR)
	{
		int32_t i=0;
		for(VMvalue* tmp=objlist;tmp!=NULL&&tmp->type==PAIR;tmp=getCdr(tmp))i++;
		VMvalue* num=newVMvalue(INT);
		num->u.num=i;
		stack->values[stack->tp-1]=num;
	}
	else if(objlist->type==STR)
	{
		VMvalue* len=newVMvalue(INT);
		len->u.num=strlen(objlist->u.str);
		stack->values[stack->tp-1]=len;
	}
	else if(objlist->type==BYTE)
	{
		VMvalue* len=newVMvalue(INT);
		len->u.num=objlist->u.byte.size;
		stack->values[stack->tp-1]=len;
	}
	freeVMvalue(objlist);
	proc->cp+=1;
	return 0;
}

int B_append(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* fir=getTopValue(stack);
	VMvalue* sec=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(exe);
	if(sec!=NULL&&sec->type!=PAIR)return 1;
	if(sec!=NULL)
	{
		VMvalue* lastpair=sec;
		while(getCdr(lastpair)!=NULL)lastpair=getCdr(lastpair);
		lastpair->u.pair.cdr=fir;
		stack->values[stack->tp-1]=sec;
	}
	else stack->values[stack->tp-1]=fir;
	proc->cp+=1;
	return 0;
}

int B_str_cat(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* fir=getTopValue(stack);
	VMvalue* sec=getValue(stack,stack->tp-2);
	if(fir==NULL||sec==NULL||fir->type!=STR||sec->type!=STR)return 1;
	int firlen=strlen(fir->u.str);
	int seclen=strlen(sec->u.str);
	char* tmpStr=(char*)malloc(sizeof(char)*(firlen+seclen+1));
	if(tmpStr==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
	stack->tp-=1;
	stackRecycle(exe);
	strcpy(tmpStr,sec->u.str);
	strcat(tmpStr,fir->u.str);
	VMvalue* tmpValue=newVMvalue(STR);
	tmpValue->u.str=tmpStr;
	stack->values[stack->tp-1]=tmpValue;
	freeVMvalue(fir);
	freeVMvalue(sec);
	proc->cp+=1;
	return 0;
}

int B_byte_cat(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* fir=getTopValue(stack);
	VMvalue* sec=getValue(stack,stack->tp-2);
	if(fir==NULL||sec==NULL||fir->type!=BYTE||sec->type!=BYTE)return 1;
	int32_t firSize=fir->u.byte.size;
	int32_t secSize=sec->u.byte.size;
	VMvalue* tmpByte=newVMvalue(BYTE);
	tmpByte->u.byte.size=firSize+secSize;
	uint8_t* tmpArry=createByteArry(firSize+secSize);
	memcpy(tmpArry,sec->u.byte.arry,secSize);
	memcpy(tmpArry+firSize,fir->u.byte.arry,firSize);
	tmpByte->u.byte.arry=tmpArry;
	stack->tp-=1;
	stackRecycle(exe);
	stack->values[stack->tp-1]=tmpByte;
	freeVMvalue(fir);
	freeVMvalue(sec);
	proc->cp+=1;
	return 0;
}

int B_getc(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	filestack* files=exe->files;
	VMvalue* file=getTopValue(stack);
	if(file==NULL||file->type!=INT)return 1;
	if(file->u.num>=files->size)return 2;
	VMvalue* tmpChr=newVMvalue(CHR);
	tmpChr->u.chr=getc(files->files[file->u.num]);
	stack->values[stack->tp-1]=tmpChr;
	freeVMvalue(file);
	proc->cp+=1;
	return 0;
}

int B_getch(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	if(stack->tp>=stack->size)
	{
		stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size+64));
		if(stack->values==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
		stack->size+=64;
	}
	VMvalue* tmpChr=newVMvalue(CHR);
	tmpChr->u.chr=getch();
	stack->values[stack->tp]=tmpChr;
	stack->tp+=1;
	proc->cp+=1;
	return 0;
}

int B_ungetc(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	filestack* files=exe->files;
	VMvalue* file=getTopValue(stack);
	VMvalue* tmpChr=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(exe);
	if(file==NULL||tmpChr==NULL||file->type!=INT||tmpChr->type!=CHR)return 1;
	if(file->u.num>=files->size)return 2;
	tmpChr->u.chr=ungetc(tmpChr->u.chr,files->files[file->u.num]);
	freeVMvalue(file);
	proc->cp+=1;
	return 0;
}

int B_read(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	filestack* files=exe->files;
	VMvalue* file=getTopValue(stack);
	if(file==NULL||file->type!=INT)return 1;
	if(file->u.num>=files->size)return 2;
	FILE* tmpFile=files->files[file->u.num];
	if(tmpFile==NULL)return 2;
	char* tmpString=getListFromFile(tmpFile);
	intpr* tmpIntpr=newIntpr(NULL,tmpFile);
	ANS_cptr* tmpCptr=createTree(tmpString,tmpIntpr);
	VMvalue* tmp=NULL;
	if(tmpCptr==NULL)return 3;
	tmp=castCptrVMvalue(tmpCptr);
	stack->values[stack->tp-1]=tmp;
	free(tmpIntpr);
	free(tmpString);
	deleteCptr(tmpCptr);
	free(tmpCptr);
	freeVMvalue(file);
	proc->cp+=1;
	return 0;
}

int B_readb(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	filestack* files=exe->files;
	VMvalue* file=getTopValue(stack);
	VMvalue* size=getValue(stack,stack->tp-2);
	if(file==NULL||file->type!=INT||size==NULL||size->type!=INT)return 1;
	if(file->u.num>=files->size)return 2;
	FILE* fp=files->files[file->u.num];
	if(fp==NULL)return 2;
	VMvalue* tmpBary=newVMvalue(BYTE);
	uint8_t* arry=(uint8_t*)malloc(sizeof(uint8_t)*size->u.num);
	if(arry==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
	fread(arry,sizeof(uint8_t),size->u.num,fp);
	tmpBary->u.byte.size=size->u.num;
	tmpBary->u.byte.arry=arry;
	stack->tp-=1;
	stackRecycle(exe);
	stack->values[stack->tp-1]=tmpBary;
	freeVMvalue(size);
	freeVMvalue(file);
	proc->cp+=1;
	return 0;
}

int B_write(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	filestack* files=exe->files;
	VMvalue* file=getTopValue(stack);
	VMvalue* obj=getValue(stack,stack->tp-2);
	if(file==NULL||file->type!=INT)return 1;
	if(file->u.num>=files->size)return 2;
	FILE* objFile=files->files[file->u.num];
	if(objFile==NULL)return 2;
	stack->tp-=1;
	stackRecycle(exe);
	printVMvalue(obj,objFile,0);
	proc->cp+=1;
	return 0;
}

int B_writeb(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	filestack* files=exe->files;
	VMvalue* file=getTopValue(stack);
	VMvalue* bt=getValue(stack,stack->tp-2);
	if(file==NULL||file->type!=INT||bt==NULL||bt->type!=BYTE)return 1;
	if(file->u.num>=files->size)return 2;
	FILE* objFile=files->files[file->u.num];
	if(objFile==NULL)return 2;
	stack->tp-=1;
	stackRecycle(exe);
	fwrite(bt->u.byte.arry,sizeof(uint8_t),bt->u.byte.size,objFile);
	proc->cp+=1;
	return 0;
}

int B_princ(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	filestack* files=exe->files;
	VMvalue* file=getTopValue(stack);
	VMvalue* obj=getValue(stack,stack->tp-2);
	if(file==NULL||file->type!=INT)return 1;
	if(file->u.num>=files->size)return 2;
	FILE* objFile=files->files[file->u.num];
	if(objFile==NULL)return 2;
	stack->tp-=1;
	stackRecycle(exe);
	princVMvalue(obj,objFile);
	proc->cp+=1;
	return 0;
}

int B_tell(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	filestack* files=exe->files;
	VMvalue* file=getTopValue(stack);
	if(file==NULL||file->type!=INT)return 1;
	if(file->u.num>=files->size)return 2;
	FILE* objFile=files->files[file->u.num];
	if(objFile==NULL)return 2;
	else file->u.num=ftell(objFile);
	proc->cp+=1;
	return 0;
}

int B_seek(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	filestack* files=exe->files;
	VMvalue* where=getTopValue(stack);
	VMvalue* file=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(exe);
	if(where==NULL||file==NULL||where->type!=INT||file->type!=INT)return 1;
	if(file->u.num>=files->size)return 2;
	FILE* objFile=files->files[file->u.num];
	if(objFile==NULL)return 2;
	file->u.num=fseek(objFile,where->u.num,SEEK_CUR);
	freeVMvalue(where);
	proc->cp+=1;
	return 0;
}

int B_rewind(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	filestack* files=exe->files;
	VMvalue* file=getTopValue(stack);
	if(file==NULL||file->type!=INT)return 1;
	if(file->u.num>=files->size)return 2;
	FILE* objFile=files->files[file->u.num];
	if(objFile==NULL)return 2;
	rewind(objFile);
	proc->cp+=1;
	return 0;
}

int B_exit(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* exitStatus=getTopValue(stack);
	if(exitStatus->type!=INT)return 1;
	exit(exitStatus->u.num);
	proc->cp+=1;
	return 0;
}
VMcode* newVMcode(ByteCode* proc)
{
	VMcode* tmp=(VMcode*)malloc(sizeof(VMcode));
	if(tmp==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
	tmp->localenv=NULL;
	tmp->refcount=0;
	if(proc!=NULL)
	{
		tmp->size=proc->size;
		tmp->code=proc->code;
	}
	else
	{
		tmp->size=0;
		tmp->code=NULL;
	}
	return tmp;
}

VMstack* newStack(int32_t size)
{
	VMstack* tmp=(VMstack*)malloc(sizeof(VMstack));
	if(tmp==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
	tmp->size=size;
	tmp->tp=0;
	tmp->bp=0;
	tmp->values=(VMvalue**)malloc(size*sizeof(VMvalue*));
	if(tmp->values==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
	return tmp;
}

filestack* newFileStack()
{
	filestack* tmp=(filestack*)malloc(sizeof(filestack));
	if(tmp==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
	tmp->size=3;
	tmp->files=(FILE**)malloc(sizeof(FILE*)*3);
	tmp->files[0]=stdin;
	tmp->files[1]=stdout;
	tmp->files[2]=stderr;
	return tmp;
}

VMvalue* newVMvalue(ValueType type/*,int access,void* pValue,VMheap* heap*/)
{
	VMvalue* tmp=(VMvalue*)malloc(sizeof(VMvalue));
	if(tmp==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
	tmp->type=type;
	switch(type)
	{
		case INT:tmp->u.num=0;break;
		case DBL:tmp->u.dbl=0;break;
		case CHR:tmp->u.chr='\0';break;
		case SYM:
		case STR:tmp->u.str=NULL;break;
		case PRC:tmp->u.prc=NULL;break;
		case PAIR:tmp->u.pair.car=NULL;
				  tmp->u.pair.cdr=NULL;
				  break;
		case BYTE:tmp->u.byte.size=0;
				  tmp->u.byte.arry=NULL;
	}
	return tmp;
	/*
	VMvalue* tmp(VMvalue*)malloc(sizeof(VMvalue));
	if(tmp==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
	tmp->type=type;
	tmp->mark=0;
	tmp->access=access;
	tmp->next=heap->root;
	heap->root->prev=tmp;
	tmp->prev=NULL;
	heap->root=tmp;
	switch(type)
	{
		case CHR:tmp->u.chr=(access)?copyMemory(pValue,sizeof(char)):pValue;break;
		case INT:tmp->u.inte=(access)?copyMemory(pValue,sizeof(int32_t)):pValue;break;
		case DBL:tmp->u.dbl=(access)?copyMemory(pValue,sizeof(double)):pValue;break;
		case SYM:
		case STR:tmp->u.str=(access)?copyStr(pValue):pValue;break;
		case PAIR:tmp->u.pair=(access)?copyPair(pValue):pValue;break;
		case PRC:tmp->u.prc=pValue;break;
		case BYTE:tmp->u.byte=(access)?copyByteArry(pValue):pValue;
	}
	return tmp;
	*/
}

void freeVMvalue(VMvalue* obj)
{
	if(obj==NULL)return;
	if(obj->type==STR||obj->type==SYM)free(obj->u.str);
	else if(obj->type==PRC)
	{
		if(obj->u.prc->refcount==0)freeVMcode(obj->u.prc);
		else obj->u.prc->refcount-=1;
	}
	else if(obj->type==PAIR)
	{
		freeVMvalue(obj->u.pair.car);
		freeVMvalue(obj->u.pair.cdr);
	}
	else if(obj->type==BYTE)
		free(obj->u.byte.arry);
	free(obj);
}

VMvalue* copyValue(VMvalue* obj)
{
	if(obj==NULL)return NULL;
	VMvalue* tmp=newVMvalue(obj->type);
	if(obj->type==STR||obj->type==SYM)tmp->u.str=copyStr(obj->u.str);
	else if(obj->type==PAIR)
	{
		tmp->u.pair.car=copyValue(obj->u.pair.car);
		tmp->u.pair.cdr=copyValue(obj->u.pair.cdr);
	}
	else if(obj->type==BYTE)
	{
		tmp->u.byte.size=obj->u.byte.size;
		tmp->u.byte.arry=createByteArry(obj->u.byte.size);
		memcpy(tmp->u.byte.arry,obj->u.byte.arry,obj->u.byte.size);
	}
	else
	{
		tmp->u=obj->u;
		if(obj->type==PRC)obj->u.prc->refcount+=1;
	}
	return tmp;
}

void freeVMcode(VMcode* proc)
{
	VMenv* curEnv=proc->localenv;
	VMenv* prev=(curEnv->prev!=NULL&&(curEnv->prev->next!=NULL&&curEnv->prev->next==curEnv))?curEnv->prev:NULL;
	curEnv->inProc=0;
	if(curEnv->next==NULL||curEnv->next->prev!=curEnv)freeVMenv(curEnv);
	curEnv=prev;
	while(curEnv!=NULL)
	{
		VMenv* prev=(curEnv->prev!=NULL&&(curEnv->prev->next==NULL||curEnv->prev->next==curEnv))?curEnv->prev:NULL;
		freeVMenv(curEnv);
		curEnv=prev;
	}
	free(proc);
	//printf("Free proc!\n");
}

VMenv* newVMenv(int32_t bound,int inProc,VMenv* prev)
{
	VMenv* tmp=(VMenv*)malloc(sizeof(VMenv));
	tmp->inProc=inProc;
	tmp->bound=bound;
	tmp->size=0;
	tmp->values=NULL;
	tmp->next=NULL;
	tmp->prev=prev;
//	fprintf(stderr,"New PreEnv: %p\n",tmp);
	return tmp;
}

void freeVMenv(VMenv* obj)
{
	if(obj->inProc==0&&(obj->next==NULL||obj->next->prev!=obj))
	{
		if(obj->values!=NULL)
		{
			VMvalue** tmp=obj->values;
			for(;tmp<obj->values+obj->size;tmp++)freeVMvalue(*tmp);
		}
		if(obj->prev!=NULL&&obj->prev->next==obj)obj->prev->next=NULL;
	//	fprintf(stderr,"Free PreEnv:%p\n",obj);
		free(obj);
	}
}

VMvalue* getTopValue(VMstack* stack)
{
	return stack->values[stack->tp-1];
}

VMvalue* getValue(VMstack* stack,int32_t place)
{
	return stack->values[place];
}

VMvalue* getCar(VMvalue* obj)
{
	if(obj->type!=PAIR)return NULL;
	else return obj->u.pair.car;
}

VMvalue* getCdr(VMvalue* obj)
{
	if(obj->type!=PAIR)return NULL;
	else return obj->u.pair.cdr;
}

void printVMvalue(VMvalue* objValue,FILE* fp,int8_t mode)
{
	if(objValue==NULL)
	{
		fprintf(fp,"nil");
		return;
	}
	switch(objValue->type)
	{
		case INT:fprintf(fp,"%d",objValue->u.num);break;
		case DBL:fprintf(fp,"%lf",objValue->u.dbl);break;
		case CHR:printRawChar(objValue->u.chr,fp);break;
		case SYM:fprintf(fp,"%s",objValue->u.str);break;
		case STR:printRawString(objValue->u.str,fp);break;
		case PRC:
				if(mode==1)printProc(objValue,fp);
				else fprintf(fp,"<#proc>");break;
		case PAIR:putc('(',fp);
				 printVMvalue(objValue->u.pair.car,fp,mode);
				 if(objValue->u.pair.cdr!=NULL)
				 {
					putc(',',fp);
					printVMvalue(objValue->u.pair.cdr,fp,mode);
				 }
				 putc(')',fp);
				 break;
		case BYTE:printByteArry(&objValue->u.byte,fp);
				  break;
		default:fprintf(fp,"Bad value!");break;
	}
}

void princVMvalue(VMvalue* objValue,FILE* fp)
{
	if(objValue==NULL)
	{
		fprintf(fp,"nil");
		return;
	}
	switch(objValue->type)
	{
		case INT:fprintf(fp,"%d",objValue->u.num);break;
		case DBL:fprintf(fp,"%lf",objValue->u.dbl);break;
		case CHR:putc(objValue->u.chr,fp);break;
		case SYM:fprintf(fp,"%s",objValue->u.str);break;
		case STR:fprintf(fp,"%s",objValue->u.str);break;
		case PRC:fprintf(fp,"<#proc>");break;
		case PAIR:putc('(',fp);
				 printVMvalue(objValue->u.pair.car,fp,0);
				 if(objValue->u.pair.cdr!=NULL)
				 {
					putc(',',fp);
					printVMvalue(objValue->u.pair.cdr,fp,0);
				 }
				 putc(')',fp);
				 break;
		case BYTE:printByteArry(&objValue->u.byte,fp);
				  break;
		default:fprintf(fp,"Bad value!");break;
	}
}

int VMvaluecmp(VMvalue* fir,VMvalue* sec)
{
	if(fir==NULL&&sec==NULL)return 1;
	if((fir==NULL||sec==NULL)&&fir!=sec)return 0;
	if(fir->type!=sec->type)return 0;
	else
	{
		switch(fir->type)
		{
			case INT:return fir->u.num==sec->u.num;
			case CHR:return fir->u.chr==sec->u.chr;
			case DBL:return fabs(fir->u.dbl-sec->u.dbl)==0;
			case STR:
			case SYM:return !strcmp(fir->u.str,sec->u.str);
			case PRC:return fir->u.prc==sec->u.prc;
			case PAIR:return VMvaluecmp(fir->u.pair.car,sec->u.pair.car)&&VMvaluecmp(fir->u.pair.cdr,sec->u.pair.cdr);
		}
	}
}

void stackRecycle(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* curproc=exe->curproc;
	VMcode* tmpCode=curproc->code;
	ByteCode tmpByteCode={tmpCode->size,tmpCode->code};
	if(stack->size-stack->tp>64)
	{
	//	size_t newSize=stack->size-64;
	//	if(newSize>0)
	//	{
			printByteCode(&tmpByteCode,stderr);
			stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size-64));
			if(stack->values==NULL)
			{
				fprintf(stderr,"stack->tp==%d,stack->size==%d\n",stack->tp,stack->size);
				fprintf(stderr,"cp=%d\n%s\n",curproc->cp,codeName[tmpCode->code[curproc->cp]].codeName);
				errors(OUTOFMEMORY,__FILE__,__LINE__);
			}
			stack->size-=64;
	//	}
	}
}

VMcode* newBuiltInProc(ByteCode* proc)
{
	VMcode* tmp=(VMcode*)malloc(sizeof(VMcode));
	if(tmp==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
	tmp->localenv=newVMenv(0,1,NULL);
	if(proc!=NULL)
	{
		tmp->size=proc->size;
		tmp->code=proc->code;
	}
	else
	{
		tmp->size=0;
		tmp->code=NULL;
	}
	return tmp;
}

VMprocess* newFakeProcess(VMcode* code,VMprocess* prev)
{
	VMprocess* tmp=(VMprocess*)malloc(sizeof(VMprocess));
	if(tmp==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
	tmp->prev=prev;
	tmp->cp=0;
	tmp->code=code;
//	fprintf(stdout,"New proc: %p\n",code);
	return tmp;
}

void printAllStack(VMstack* stack,FILE* fp)
{
	if(stack->tp==0)fprintf(fp,"[#EMPTY]\n");
	else
	{
		int i=stack->tp-1;
		for(;i>=0;i--)
		{
			printVMvalue(stack->values[i],fp,0);
			putc('\n',fp);
		}
	}
}

VMvalue* castCptrVMvalue(const ANS_cptr* objCptr)
{
	if(objCptr->type==ATM)
	{
		ANS_atom* tmpAtm=objCptr->value;
		VMvalue* tmp=newVMvalue(tmpAtm->type);
		switch(tmpAtm->type)
		{
			case INT:tmp->u.num=tmpAtm->value.num;break;
			case DBL:tmp->u.dbl=tmpAtm->value.dbl;break;
			case CHR:tmp->u.chr=tmpAtm->value.chr;break;
			case BYTE:tmp->u.byte=copyByteArry(tmpAtm->value.byte);break;
			case SYM:
			case STR:tmp->u.str=copyStr(tmpAtm->value.str);break;
		}
		return tmp;
	}
	else if(objCptr->type==PAIR)
	{
		ANS_pair* objPair=objCptr->value;
		VMvalue* tmp=newVMvalue(PAIR);
		tmp->u.pair.car=castCptrVMvalue(&objPair->car);
		tmp->u.pair.cdr=castCptrVMvalue(&objPair->cdr);
		return tmp;
	}
	else if(objCptr->type==NIL)return NULL;
}

VMprocess* hasSameProc(VMcode* objCode,VMprocess* curproc)
{
	while(curproc!=NULL&&curproc->code!=objCode)curproc=curproc->prev;
	return curproc;
}

int isTheLastExpress(const VMprocess* proc,const VMprocess* same)
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

#ifndef _WIN32
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
#endif

void printEnv(VMenv* curEnv,FILE* fp)
{
	if(curEnv->size==0)
		fprintf(fp,"This ENV is empty!");
	else
	{
		fprintf(fp,"ENV:");
		for(int i=0;i<curEnv->size;i++)
		{
			printVMvalue(curEnv->values[i],fp,0);
			putc(' ',fp);
		}
	}
}

uint8_t* createByteArry(int32_t size)
{
	uint8_t* tmp=(uint8_t*)malloc(sizeof(uint8_t)*size);
	if(tmp==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
	return tmp;
}

void printProc(VMvalue* objValue,FILE* fp)
{
	fputs("\n<#proc\n",fp);
	ByteCode tmp={objValue->u.prc->size,objValue->u.prc->code};
	printByteCode(&tmp,fp);
	fputc('>',fp);
}

VMheap* createHeap()
{
	VMheap* tmp=(VMheap*)malloc(sizeof(VMheap));
	if(tmp==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
	tmp->size=0;
	tmp->threshold=THRESHOLD_SIZE;
	tmp->head=NULL;
	return tmp;
}

void GC_mark(fakeVM* exe)
{
	GC_markValueInStack(exe->stack);
	GC_markMessage(exe->queue);
}

void GC_sweep(VMheap* heap)
{
	VMvalue* cur=heap->head;
	while(cur!=NULL)
	{
		VMvalue* next=cur->next;
		if(!cur->mark)
		{
			if(cur->prev==NULL)
				heap->head=cur->next;
			freeVMvalue(cur);
		}
		else cur->mark=0;
		cur=next;
	}
}

void GC_markValueInStack(VMstack* stack)
{
	int32_t i=0;
	for(;i<stack->tp;i++)
	{
		VMvalue* tmp=stack->values[i];
		GC_markValue(tmp);
	}
}

void GC_markMessage(threadMessage* head)
{
	while(head!=NULL)
	{
		GC_markValue(head->message);
		head=head->next;
	}
}

void GC_markValue(VMvalue* value)
{
	value->mark=1;
	if(value->type==PAIR)
	{
		GC_markValue(value->u.pair.car);
		GC_markValue(value->u.pair.cdr);
	}
	else if(value->type==PRC)
		GC_markValueInEnv(value->u.prc->localenv);
}

void GC_markValueInEnv(VMenv* curEnv)
{
	int32_t i=0;
	for(;i<curEnv->size;i++)
		GC_markValue(curEnv->values[i]);
}

//void* copyMemory(void* pm,size_t size)
//{
//	void* tmp=(void*)malloc(size);
//	if(tmp==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
//	memcpy(tmp,pm,size);
//	return pm;
//}
//
//VMpair* newVMpair()
//{
//		VMpair* tmp=(VMpair*)malloc(sizeof(VMpair));
//		if(tmp==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
//		tmp->car=NULL;
//		tmp->cdr=NULL;
//		return tmp;
//}
//
//ByteArry* newByteArry(size_t size,uint8_t* arry,int access)
//{
//	ByteArry* tmp=(ByteArry*)malloc(sizeof(ByteArry));
//	if(tmp==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
//	tmp->size=size;
//	tmp->arry=(access)?copyArry(size,arry):arry;
//	return tmp;
//}
//
//uint8_t* copyArry(size_t size,uint8_t* arry)
//{
//	uint8_t* tmp=(uint8_t*)malloc(sizeof(uint8_t)*size);
//	if(tmp==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
//	memcpy(tmp,arry,size);
//	return tmp;
//}
