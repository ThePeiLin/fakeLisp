#include"tool.h"
#include"VMtool.h"
#include"fakeVM.h"
#include"opcode.h"
#include<string.h>
#include<math.h>
#ifdef _WIN32
#include<tchar.h>
#include<conio.h>
#include<windows.h>
#else
#include<termios.h>
#include<dlfcn.h>
#endif
#include<unistd.h>
#include<time.h>

pthread_rwlock_t GClock=PTHREAD_RWLOCK_INITIALIZER;
fakeVMStack GlobFakeVMs={0,NULL};
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
	B_push_list_arg,
	B_pop,
	B_pop_var,
	B_pop_rest_var,
	B_pop_car,
	B_pop_cdr,
	B_pop_ref,
	B_init_proc,
	B_call_proc,
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
	B_eqn,
	B_equal,
	B_gt,
	B_ge,
	B_lt,
	B_le,
	B_not,
	B_read,
	B_readb,
	B_write,
	B_writeb,
	B_princ,
	B_go,
	B_send,
	B_accept,
	B_getid,
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

ByteCode P_app=
{
	25,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_SET_BP,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_PUSH_LIST_ARG,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_INVOKE,
		FAKE_END_PROC,
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

ByteCode P_eqn=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_EQN,
		FAKE_END_PROC
	}
};

ByteCode P_equal=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_EQUAL,
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

//ByteCode P_tell=
//{
//	13,
//	(char[])
//	{
//		FAKE_POP_VAR,0,0,0,0,
//		FAKE_RES_BP,
//		FAKE_PUSH_VAR,0,0,0,0,
//		FAKE_TELL,
//		FAKE_END_PROC
//	}
//};
//
//ByteCode P_seek=
//{
//	23,
//	(char[])
//	{
//		FAKE_POP_VAR,0,0,0,0,
//		FAKE_POP_VAR,1,0,0,0,
//		FAKE_RES_BP,
//		FAKE_PUSH_VAR,0,0,0,0,
//		FAKE_PUSH_VAR,1,0,0,0,
//		FAKE_SEEK,
//		FAKE_END_PROC
//	}
//};
//
//ByteCode P_rewind=
//{
//	13,
//	(char[])
//	{
//		FAKE_POP_VAR,0,0,0,0,
//		FAKE_RES_BP,
//		FAKE_PUSH_VAR,0,0,0,0,
//		FAKE_REWIND,
//		FAKE_END_PROC
//	}
//};
//
//ByteCode P_exit=
//{
//	13,
//	(char[])
//	{
//		FAKE_POP_VAR,0,0,0,0,
//		FAKE_RES_BP,
//		FAKE_PUSH_VAR,0,0,0,0,
//		FAKE_EXIT,
//		FAKE_END_PROC
//	}
//};

ByteCode P_go=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_REST_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_GO,
		FAKE_END_PROC
	}
};

ByteCode P_send=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_SEND,
		FAKE_END_PROC
	}
};

ByteCode P_accept=
{
	3,
	(char[])
	{
		FAKE_RES_BP,
		FAKE_ACCEPT,
		FAKE_END_PROC
	}
};

ByteCode P_getid=
{
	3,
	(char[])
	{
		FAKE_RES_BP,
		FAKE_GETID,
		FAKE_END_PROC
	}
};

//ByteCode P_slp=
//{
//	23,
//	(char[])
//	{
//		FAKE_POP_VAR,0,0,0,0,
//		FAKE_POP_VAR,1,0,0,0,
//		FAKE_RES_BP,
//		FAKE_PUSH_VAR,0,0,0,0,
//		FAKE_PUSH_VAR,1,0,0,0,
//		FAKE_SLP,
//		FAKE_END_PROC
//	}
//};

fakeVM* newFakeVM(ByteCode* mainproc,ByteCode* procs)
{
	fakeVM* exe=(fakeVM*)malloc(sizeof(fakeVM));
	if(exe==NULL)errors("newFakeVM",__FILE__,__LINE__);
	if(mainproc!=NULL)
		exe->mainproc=newFakeProcess(newVMcode(mainproc),NULL);
	else
		exe->mainproc=newFakeProcess(NULL,NULL);
	exe->curproc=exe->mainproc;
	exe->modules=NULL;
	exe->procs=procs;
	exe->mark=1;
	exe->stack=newStack(0);
	exe->queueHead=NULL;
	exe->queueTail=NULL;
	exe->files=newFileStack();
	exe->heap=newVMheap();
	pthread_mutex_init(&exe->lock,NULL);
	fakeVM** ppFakeVM=NULL;
	int i=0;
	for(;i<GlobFakeVMs.size;i++)
		if(GlobFakeVMs.VMs[i]==NULL)
			ppFakeVM=GlobFakeVMs.VMs+i;
	if(ppFakeVM!=NULL)
	{
		exe->VMid=i;
		*ppFakeVM=exe;
	}
	else
	{
		int32_t size=GlobFakeVMs.size;
		GlobFakeVMs.VMs=(fakeVM**)realloc(GlobFakeVMs.VMs,sizeof(fakeVM*)*(size+1));
		if(size!=0&&GlobFakeVMs.VMs==NULL)errors("newFakeVM",__FILE__,__LINE__);
		GlobFakeVMs.VMs[size]=exe;
		GlobFakeVMs.size+=1;
		exe->VMid=size;
	}
	return exe;
}

void initGlobEnv(VMenv* obj,VMheap* heap)
{
	ByteCode buildInProcs[]=
	{
		P_cons,
		P_car,
		P_cdr,
		P_atom,
		P_null,
		P_app,
		P_ischr,
		P_isint,
		P_isdbl,
		P_isstr,
		P_issym,
		P_isprc,
		P_isbyt,
		P_eq,
		P_eqn,
		P_equal,
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
		P_read,
		P_readb,
		P_write,
		P_writeb,
		P_princ,
		P_go,
		P_send,
		P_accept,
		P_getid,
	};
	obj->size=NUMOFBUILTINSYMBOL;
	obj->values=(VMvalue**)realloc(obj->values,sizeof(VMvalue*)*NUMOFBUILTINSYMBOL);
	if(obj->values==NULL)errors("initGlobEnv",__FILE__,__LINE__);
	int32_t tmpInt=EOF;
	obj->values[0]=newNilValue(heap);
	obj->values[1]=newVMvalue(IN32,&tmpInt,heap,1);
	tmpInt=0;
	obj->values[2]=newVMvalue(IN32,&tmpInt,heap,1);
	tmpInt=1;
	obj->values[3]=newVMvalue(IN32,&tmpInt,heap,1);
	tmpInt=2;
	obj->values[4]=newVMvalue(IN32,&tmpInt,heap,1);
	int i=5;
	for(;i<NUMOFBUILTINSYMBOL;i++)
	{
		ByteCode* tmp=copyByteCode(buildInProcs+(i-5));
		obj->values[i]=newVMvalue(PRC,newBuiltInProc(tmp),heap,1);
		free(tmp);
	}
}

void* ThreadVMFunc(void* p)
{
	fakeVM* exe=(fakeVM*)p;
	runFakeVM(exe);
	exe->mark=0;
	pthread_mutex_destroy(&exe->lock);
	freeMessage(exe->queueHead);
	exe->queueHead=NULL;
	freeVMstack(exe->stack);
	exe->stack=NULL;
	return NULL;
}

void runFakeVM(fakeVM* exe)
{
	while(exe->curproc!=NULL&&exe->curproc->cp!=exe->curproc->code->size)
	{
		pthread_rwlock_rdlock(&GClock);
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
				case 4:fprintf(stderr,"error:Too much arguements!\n");
					   exit(EXIT_FAILURE);
				case 5:fprintf(stderr,"error:Too few arguements!\n");
					   exit(EXIT_FAILURE);
				case 6:fprintf(stderr,"error:Can't create thread!\n");
					   exit(EXIT_FAILURE);
				case 7:fprintf(stderr,"error:Thread error!\n");
					   exit(EXIT_FAILURE);
			}
		}
		pthread_rwlock_unlock(&GClock);
		if(exe->heap->size>exe->heap->threshold)
		{
			if(pthread_rwlock_trywrlock(&GClock))continue;
			int i=0;
			for(;i<GlobFakeVMs.size;i++)
			{
				//fprintf(stderr,"\nValue that be marked:\n");
				if(GlobFakeVMs.VMs[i]->mark)
					GC_mark(GlobFakeVMs.VMs[i]);
				else
				{
					free(GlobFakeVMs.VMs[i]);
					GlobFakeVMs.VMs[i]=NULL;
				}
			}
			//fprintf(stderr,"\n=======\nValue that be sweep:\n");
			GC_sweep(exe->heap);
			//fprintf(stderr,"======\n");
			pthread_mutex_lock(&exe->heap->lock);
			exe->heap->threshold=exe->heap->size+THRESHOLD_SIZE;
			pthread_mutex_unlock(&exe->heap->lock);
			pthread_rwlock_unlock(&GClock);
		}
	//	fprintf(stdout,"=========\n");
	//	fprintf(stderr,"stack->tp=%d\n",exe->stack->tp);
	//	printAllStack(exe->stack,stderr);
	//	putc('\n',stderr);
	}
}

int B_dummy(fakeVM* exe)
{
	VMprocess* curproc=exe->curproc;
	VMcode* tmpCode=curproc->code;
	ByteCode tmpByteCode={tmpCode->size,tmpCode->code};
	VMstack* stack=exe->stack;
	printByteCode(&tmpByteCode,stderr);
	putc('\n',stderr);
	printAllStack(exe->stack,stderr);
	putc('\n',stderr);
	fprintf(stderr,"stack->tp==%d,stack->size==%d\n",stack->tp,stack->size);
	fprintf(stderr,"cp=%d stack->bp=%d\n%s\n",curproc->cp,stack->bp,codeName[tmpCode->code[curproc->cp]].codeName);
	printEnv(exe->curproc->localenv,stderr);
	putc('\n',stderr);
	fprintf(stderr,"Wrong byta code!\n");
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
		if(stack->values==NULL)errors("B_push_nil",__FILE__,__LINE__);
		stack->size+=64;
	}
	stack->values[stack->tp]=newVMvalue(NIL,NULL,exe->heap,0);
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
		if(stack->values==NULL)errors("B_push_pair",__FILE__,__LINE__);
		stack->size+=64;
	}
	VMvalue* objValue=newVMvalue(PAIR,newVMpair(exe->heap),exe->heap,0);
	objValue->access=1;
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
		if(stack->values==NULL)errors("B_push_int",__FILE__,__LINE__);
		stack->size+=64;
	}
	//VMvalue* objValue=newVMvalue(IN32);
	VMvalue* objValue=newVMvalue(IN32,tmpCode->code+proc->cp+1,exe->heap,1);
	//objValue->u.num=*(int32_t*)(tmpCode->code+proc->cp+1);
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
		if(stack->values==NULL)errors("B_push_chr",__FILE__,__LINE__);
		stack->size+=64;
	}
	//VMvalue* objValue=newVMvalue(CHR);
	VMvalue* objValue=newVMvalue(CHR,tmpCode->code+proc->cp+1,exe->heap,1);
	//objValue->u.num=*(tmpCode->code+proc->cp+1);
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
		if(stack->values==NULL)errors("B_push_dbl",__FILE__,__LINE__);
		stack->size+=64;
	}
	//VMvalue* objValue=newVMvalue(DBL);
	VMvalue* objValue=newVMvalue(DBL,tmpCode->code+proc->cp+1,exe->heap,1);
	//objValue->u.dbl=*(double*)(tmpCode->code+proc->cp+1);
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
	if(stack->tp>=stack->size)
	{
		stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size+64));
		if(stack->values==NULL)errors("B_push_str",__FILE__,__LINE__);
		stack->size+=64;
	}
	//VMvalue* objValue=newVMvalue(STR);
	VMstr* tmpStr=newVMstr(tmpCode->code+proc->cp+1);
	VMvalue* objValue=newVMvalue(STR,tmpStr,exe->heap,1);
	//objValue->u.str=tmpStr;
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
	if(stack->tp>=stack->size)
	{
		stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size+64));
		if(stack->values==NULL)errors("B_push_sym",__FILE__,__LINE__);
		stack->size+=64;
	}
	//VMvalue* objValue=newVMvalue(SYM);
	VMstr* tmpStr=newVMstr(tmpCode->code+proc->cp+1);
	VMvalue* objValue=newVMvalue(SYM,tmpStr,exe->heap,1);
	//objValue->u.str=tmpStr;
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
		if(stack->values==NULL)errors("B_push_byte",__FILE__,__LINE__);
		stack->size+=64;
	}
	//VMvalue* objValue=newVMvalue(BYTA);
	//objValue->u.byta.size=size;
	//objValue->u.byta.arry=tmp;
	ByteArry* tmpArry=newByteArry(size,NULL);
	tmpArry->arry=tmp;
	VMvalue* objValue=newVMvalue(BYTA,tmpArry,exe->heap,1);
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
		if(stack->values==NULL)errors("B_push_var",__FILE__,__LINE__);
		stack->size+=64;
	}
	int32_t countOfVar=*(int32_t*)(tmpCode->code+proc->cp+1);
	VMenv* curEnv=proc->localenv;
	while(curEnv->bound==-1||countOfVar<curEnv->bound)curEnv=curEnv->prev;
	VMvalue* tmpValue=*(curEnv->values+countOfVar-(curEnv->bound));
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
	if(objValue->type!=PAIR&&objValue->type!=STR&&objValue->type!=BYTA)return 1;
	if(objValue->type==PAIR)
	{
		//stack->values[stack->tp-1]=copyValue(getCar(objValue));
		stack->values[stack->tp-1]=getCar(objValue);
	}
	else if(objValue->type==STR)
	{
		//VMvalue* ch=newVMvalue(CHR);
		VMvalue* ch=newVMvalue(CHR,&objValue->u.str[0],exe->heap,0);
		//ch->u.chr=objValue->u.str[0];
		stack->values[stack->tp-1]=ch;
	}
	else if(objValue->type==BYTA)
	{
		//VMvalue* bt=newVMvalue(BYTA);
		ByteArry* tmp=newByteArry(1,NULL);
		tmp->arry=objValue->u.byta->arry;
		VMvalue* bt=newVMvalue(BYTA,tmp,exe->heap,0);
		//bt->u.byta.size=1;
		//bt->u.byta.arry=createByteArry(1);
		//bt->u.byta.arry[0]=objValue->u.byta.arry[0];
		stack->values[stack->tp-1]=bt;
	}
	proc->cp+=1;
	return 0;
}

int B_push_cdr(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* objValue=getTopValue(stack);
	if(objValue->type!=PAIR&&objValue->type!=STR&&objValue->type!=BYTA)return 1;
	if(objValue->type==PAIR)
	{
		//stack->values[stack->tp-1]=copyValue(getCdr(objValue));
		stack->values[stack->tp-1]=getCdr(objValue);
	}
	else if(objValue->type==STR)
	{
		char* str=objValue->u.str->str;
		if(strlen(str)==0)stack->values[stack->tp-1]=newNilValue(exe->heap);
		else
		{
			//VMvalue* tmpStr=newVMvalue(STR);
			VMstr* tmpVMstr=newVMstr(NULL);
			tmpVMstr->str=objValue->u.str->str+1;
			VMvalue* tmpStr=newVMvalue(STR,tmpVMstr,exe->heap,0);
			//tmpStr->u.str=copyStr(objValue->u.str+1);
			stack->values[stack->tp-1]=tmpStr;
		}
	}
	else if(objValue->type==BYTA)
	{
		if(objValue->u.byta->size==1)stack->values[stack->tp-1]=newNilValue(exe->heap);
		else
		{
			//VMvalue* bt=newVMvalue(BYTA);
			ByteArry* tmp=newByteArry(objValue->u.byta->size-1,NULL);
			tmp->arry=objValue->u.byta->arry+1;
			VMvalue* bt=newVMvalue(BYTA,tmp,exe->heap,0);
			//int32_t size=objValue->u.byta.size-1;
			//bt->u.byta.size=size;
			//bt->u.byta.arry=createByteArry(size);
			//memcpy(bt->u.byta.arry,objValue->u.byta.arry+1,size);
			stack->values[stack->tp-1]=bt;
		}
	}
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
		if(stack->values==NULL)errors("B_push_top",__FILE__,__LINE__);
		stack->size+=64;
	}
	//stack->values[stack->tp]=copyValue(getTopValue(stack));
	stack->values[stack->tp]=getTopValue(stack);
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
		if(stack->values==NULL)errors("B_push_proc",__FILE__,__LINE__);
		stack->size+=64;
	}
	//VMvalue* objValue=newVMvalue(PRC);
	VMvalue* objValue=newVMvalue(PRC,newVMcode(exe->procs+countOfProc),exe->heap,1);
	//objValue->u.prc=newVMcode(exe->procs+countOfProc);
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=5;
	return 0;
}

int B_push_list_arg(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMcode* tmpCode=proc->code;
	VMvalue* tmpList=getTopValue(stack);
	stack->tp-=1;
	while(tmpList->type!=NIL)
	{
		if(stack->tp>=stack->size)
		{
			stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size+64));
			if(stack->values==NULL)errors("B_push_list_arg",__FILE__,__LINE__);
			stack->size+=64;
		}
		VMvalue* tmp=newNilValue(exe->heap);
		copyRef(tmp,tmpList->u.pair->car);
		stack->values[stack->tp]=tmp;
		stack->tp+=1;
		tmpList=tmpList->u.pair->cdr;
	}
	proc->cp+=1;
	return 0;
}

int B_pop(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
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
	if(!(stack->tp>stack->bp))return 5;
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
		pValue=curEnv->values+countOfVar-(curEnv->bound);
	VMvalue* topValue=getTopValue(stack);
	*pValue=newNilValue(exe->heap);
	(*pValue)->access=1;
	copyRef(*pValue,topValue);
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
		tmpValue=curEnv->values+countOfVar-(curEnv->bound);
	VMvalue* obj=newNilValue(exe->heap);
	VMvalue* tmp=NULL;
	if(stack->tp>stack->bp)
	{
		obj=newVMvalue(PAIR,newVMpair(exe->heap),exe->heap,1);
		tmp=obj;
	}
	while(stack->tp>stack->bp)
	{
		tmp->u.pair->car->access=1;
		VMvalue* topValue=getTopValue(stack);
		copyRef(tmp->u.pair->car,topValue);
		stack->tp-=1;
		stackRecycle(exe);
		if(stack->tp>stack->bp)
			tmp->u.pair->cdr=newVMvalue(PAIR,newVMpair(exe->heap),exe->heap,1);
		else break;
		tmp=tmp->u.pair->cdr;
	}
	*tmpValue=obj;
	proc->cp+=5;
	return 0;
}

int B_pop_car(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	VMvalue* objValue=getValue(stack,stack->tp-2);
	if(objValue->type!=PAIR)return 1;
	//freeVMvalue(objValue->u.pair->car);
	//objValue->u.pair->car=copyValue(topValue);
	copyRef(objValue->u.pair->car,topValue);
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
	if(objValue->type!=PAIR)return 1;
	//freeVMvalue(objValue->u.pair.cdr);
	//objValue->u.pair.cdr=copyValue(topValue);
	copyRef(objValue->u.pair->cdr,topValue);
	stack->tp-=1;
	stackRecycle(exe);
	proc->cp+=1;
	return 0;
}

int B_pop_ref(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	VMvalue* objValue=getValue(stack,stack->tp-2);
	if(objValue->access==0&&objValue->type!=topValue->type)return 1;
	if(objValue->type==topValue->type)
		writeRef(objValue,topValue);
	else
		copyRef(objValue,topValue);
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
	if((firValue->type!=IN32&&firValue->type!=DBL)||(secValue->type!=IN32&&secValue->type!=DBL))return 1;
	if(firValue->type==DBL||secValue->type==DBL)
	{
		double result=((firValue->type==DBL)?*firValue->u.dbl:*firValue->u.num)+((secValue->type==DBL)?*secValue->u.dbl:*secValue->u.num);
		VMvalue* tmpValue=newVMvalue(DBL,&result,exe->heap,1);
		stack->tp-=1;
		stackRecycle(exe);
		stack->values[stack->tp-1]=tmpValue;
	}
	else if(firValue->type==IN32&&secValue->type==IN32)
	{
		int32_t result=*firValue->u.num+*secValue->u.num;
		VMvalue* tmpValue=newVMvalue(IN32,&result,exe->heap,1);
		stack->tp-=1;
		stackRecycle(exe);
		stack->values[stack->tp-1]=tmpValue;
	}
	//freeVMvalue(firValue);
	//freeVMvalue(secValue);
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
	if((firValue->type!=IN32&&firValue->type!=DBL)||(secValue->type!=IN32&&secValue->type!=DBL))return 1;
	if(firValue->type==DBL||secValue->type==DBL)
	{
		double result=((secValue->type==DBL)?*secValue->u.dbl:*secValue->u.num)-((firValue->type==DBL)?*firValue->u.dbl:*firValue->u.num);
		VMvalue* tmpValue=newVMvalue(DBL,&result,exe->heap,1);
		stack->values[stack->tp-1]=tmpValue;
	}
	else if(firValue->type==IN32&&secValue->type==IN32)
	{
		int32_t result=*secValue->u.num-*firValue->u.num;
		VMvalue* tmpValue=newVMvalue(IN32,&result,exe->heap,1);
		stack->values[stack->tp-1]=tmpValue;
	}
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
	if((firValue->type!=IN32&&firValue->type!=DBL)||(secValue->type!=IN32&&secValue->type!=DBL))return 1;
	if(firValue->type==DBL||secValue->type==DBL)
	{
		double result=((firValue->type==DBL)?*firValue->u.dbl:*firValue->u.num)*((secValue->type==DBL)?*secValue->u.dbl:*secValue->u.num);
		VMvalue* tmpValue=newVMvalue(DBL,&result,exe->heap,1);
		stack->values[stack->tp-1]=tmpValue;
	}
	else if(firValue->type==IN32&&secValue->type==IN32)
	{
		int32_t result=*firValue->u.num*(*secValue->u.num);
		VMvalue* tmpValue=newVMvalue(IN32,&result,exe->heap,1);
		stack->values[stack->tp-1]=tmpValue;
	}
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
	if((firValue->type!=IN32&&firValue->type!=DBL)||(secValue->type!=IN32&&secValue->type!=DBL))return 1;
	double result=((secValue->type==DBL)?*secValue->u.dbl:*secValue->u.num)/((firValue->type==DBL)?*firValue->u.dbl:*firValue->u.num);
	VMvalue* tmpValue=newVMvalue(DBL,&result,exe->heap,1);
	stack->values[stack->tp-1]=tmpValue;
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
	if((firValue->type!=IN32&&firValue->type!=DBL)||(secValue->type!=IN32&&secValue->type!=DBL))return 1;
	int32_t result=((int32_t)((secValue->type==DBL)?*secValue->u.dbl:*secValue->u.num))%((int32_t)((firValue->type==DBL)?*firValue->u.dbl:*firValue->u.num));
	VMvalue* tmpValue=newVMvalue(IN32,&result,exe->heap,1);
	stack->values[stack->tp-1]=tmpValue;
	proc->cp+=1;
	return 0;
}

int B_rand(fakeVM* exe)
{
	srand((unsigned)time(NULL));
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* lim=getTopValue(stack);
	if(lim->type!=IN32)return 1;
	int32_t result=rand()%(*lim->u.num);
	if(lim->access)free(lim->u.num);
	lim->u.num=copyMemory(&result,sizeof(int32_t));
	proc->cp+=1;
	return 0;
}

int B_atom(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	if(topValue->type!=PAIR)
		stack->values[stack->tp-1]=newTrueValue(exe->heap);
	else
	{
		if(getCar(topValue)->type==NIL&&getCdr(topValue)->type==NIL)
			stack->values[stack->tp-1]=newTrueValue(exe->heap);
		else
			stack->values[stack->tp-1]=newNilValue(exe->heap);
	}
	proc->cp+=1;
	return 0;
}

int B_null(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	if(topValue->type==NIL||(topValue->type==PAIR&&(getCar(topValue)->type==NIL&&getCdr(topValue)->type==NIL)))
	{
		stack->values[stack->tp-1]=newTrueValue(exe->heap);
	}
	else stack->values[stack->tp-1]=newNilValue(exe->heap);
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
	topValue->u.prc->localenv=newVMenv(boundOfProc,tmpCode->localenv);
	proc->cp+=5;
	return 0;
}

int B_call_proc(fakeVM* exe)
{
	VMprocess* proc=exe->curproc;
	VMcode* tmpCode=proc->code;
	char* funcname=tmpCode->code+proc->cp+1;
	char* headOfFunc="FAKE_";
	char* realfuncName=(char*)malloc(sizeof(char)*(strlen(headOfFunc)+strlen(funcname)+1));
	if(realfuncName==NULL)errors("B_call_proc",__FILE__,__LINE__);
	strcpy(realfuncName,headOfFunc);
	strcat(realfuncName,funcname);
	proc->cp+=2+strlen(funcname);
	int retval=((ModFunc)getAddress(realfuncName,exe->modules))(exe,&GClock);
	free(realfuncName);
	return retval;
}

int B_end_proc(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* tmpProc=exe->curproc;
//	fprintf(stdout,"End proc: %p\n",tmpProc->code);
	VMprocess* prev=exe->curproc->prev;
	VMcode* tmpCode=tmpProc->code;
	VMenv* tmpEnv=tmpCode->localenv;
	exe->curproc=prev;
	if(tmpProc->prev!=NULL&&tmpCode==tmpProc->prev->code)
		tmpCode->localenv=tmpProc->prev->localenv;
	else
		tmpCode->localenv=newVMenv(tmpEnv->bound,tmpEnv->prev);
	free(tmpProc);
	freeVMenv(tmpEnv);
	if(!tmpCode->refcount)
		freeVMcode(tmpCode);
	else
		tmpCode->refcount-=1;
	return 0;
}

int B_set_bp(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* prevBp=newVMvalue(IN32,&stack->bp,exe->heap,1);
	if(stack->tp>=stack->size)
	{
		stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size+64));
		if(stack->values==NULL)errors("B_set_bp",__FILE__,__LINE__);
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
	if(stack->tp>stack->bp)return 4;
	VMvalue* prevBp=getTopValue(stack);
	stack->bp=*prevBp->u.num;
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
	if(tmpValue->type!=PRC)return 1;
	proc->cp+=1;
	VMcode* tmpCode=tmpValue->u.prc;
	VMprocess* prevProc=hasSameProc(tmpCode,proc);
	if(isTheLastExpress(proc,prevProc)&&prevProc)
		prevProc->cp=0;
	else
	{
		tmpCode->refcount+=1;
		VMprocess* tmpProc=newFakeProcess(tmpCode,proc);
		if(hasSameProc(tmpCode,proc))
		{
			tmpProc->localenv=newVMenv(tmpCode->localenv->bound,tmpCode->localenv->prev);
			tmpCode->localenv=tmpProc->localenv;
		}
		else
			tmpProc->localenv=tmpCode->localenv;
		exe->curproc=tmpProc;
	}
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
	if(!(tmpValue->type==NIL||(tmpValue->type==PAIR&&getCar(tmpValue)->type==NIL&&getCdr(tmpValue)->type==NIL)))
	{
		int32_t where=*(int32_t*)(tmpCode->code+proc->cp+sizeof(char));
		proc->cp+=where;
	}
	else
	{
		stack->tp-=1;
		stackRecycle(exe);
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
	if(tmpValue->type==NIL||(tmpValue->type==PAIR&&getCar(tmpValue)->type==NIL&&getCdr(tmpValue)->type==NIL))
	{
		int32_t where=*(int32_t*)(tmpCode->code+proc->cp+sizeof(char));
		proc->cp+=where;
	}
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
	if(filename->type!=STR||mode->type!=STR)return 1;
	stack->tp-=1;
	stackRecycle(exe);
	int32_t i=0;
	FILE* file=fopen(filename->u.str->str,mode->u.str->str);
	if(file==NULL)i=-1;
	if(i!=-1)
	{
		for(;i<files->size;i++)if(files->files[i]==NULL)break;
		pthread_mutex_lock(&files->lock);
		if(i==files->size)
		{
			files->files=(FILE**)realloc(files->files,sizeof(FILE*)*(files->size+1));
			files->files[i]=file;
			files->size+=1;
		}
		else files->files[i]=file;
		pthread_mutex_unlock(&files->lock);
	}
	VMvalue* countOfFile=newVMvalue(IN32,&i,exe->heap,1);
	stack->values[stack->tp-1]=countOfFile;
	proc->cp+=1;
	return 0;
}

int B_close(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	filestack* files=exe->files;
	VMvalue* topValue=getTopValue(stack);
	if(topValue->type!=IN32)return 1;
	if(*topValue->u.num>=files->size)return 2;
	pthread_mutex_lock(&files->lock);
	FILE* objFile=files->files[*topValue->u.num];
	if(fclose(objFile)==EOF)
	{
		files->files[*topValue->u.num]=NULL;
		*topValue->u.num=-1;
	}
	else files->files[*topValue->u.num]=NULL;
	pthread_mutex_unlock(&files->lock);
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
	if(subVMvaluecmp(firValue,secValue))
		stack->values[stack->tp-1]=newTrueValue(exe->heap);
	else stack->values[stack->tp-1]=newNilValue(exe->heap);
	proc->cp+=1;
	return 0;
}

int B_eqn(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* firValue=getTopValue(stack);
	VMvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(exe);
	if(firValue->type==DBL||secValue->type==DBL)
	{
		double result=((secValue->type==DBL)?*secValue->u.dbl:*secValue->u.num)-((firValue->type==DBL)?*firValue->u.dbl:*firValue->u.num);
		if(result==0)
			stack->values[stack->tp-1]=newTrueValue(exe->heap);
		else stack->values[stack->tp-1]=newNilValue(exe->heap);
	}
	else if(firValue->type==IN32&&secValue->type==IN32)
	{
		if(*secValue->u.num==*firValue->u.num)
			stack->values[stack->tp-1]=newTrueValue(exe->heap);
		else stack->values[stack->tp-1]=newNilValue(exe->heap);
	}
	else if(firValue->type==secValue->type&&(firValue->type==SYM||firValue->type==STR))
	{
		if(strcmp(secValue->u.str->str,firValue->u.str->str)==0)
			stack->values[stack->tp-1]=newTrueValue(exe->heap);
		else stack->values[stack->tp-1]=newNilValue(exe->heap);
	}
	else return 1;
	proc->cp+=1;
	return 0;
}

int B_equal(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* firValue=getTopValue(stack);
	VMvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(exe);
	if(VMvaluecmp(firValue,secValue))
		stack->values[stack->tp-1]=newTrueValue(exe->heap);
	else stack->values[stack->tp-1]=newNilValue(exe->heap);
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
	if(firValue->type==DBL||secValue->type==DBL)
	{
		double result=((secValue->type==DBL)?*secValue->u.dbl:*secValue->u.num)-((firValue->type==DBL)?*firValue->u.dbl:*firValue->u.num);
		if(result>0)
			stack->values[stack->tp-1]=newTrueValue(exe->heap);
		else stack->values[stack->tp-1]=newNilValue(exe->heap);
	}
	else if(firValue->type==IN32&&secValue->type==IN32)
	{
		if(*secValue->u.num>*firValue->u.num)
			stack->values[stack->tp-1]=newTrueValue(exe->heap);
		else stack->values[stack->tp-1]=newNilValue(exe->heap);
	}
	else if(firValue->type==secValue->type&&(firValue->type==SYM||firValue->type==STR))
	{
		if(strcmp(secValue->u.str->str,firValue->u.str->str)>0)
			stack->values[stack->tp-1]=newTrueValue(exe->heap);
		else stack->values[stack->tp-1]=newNilValue(exe->heap);
	}
	else return 1;
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
	if(firValue->type==DBL||secValue->type==DBL)
	{
		double result=((secValue->type==DBL)?*secValue->u.dbl:*secValue->u.num)-((firValue->type==DBL)?*firValue->u.dbl:*firValue->u.num);
		if(result>=0)
			stack->values[stack->tp-1]=newTrueValue(exe->heap);
		else stack->values[stack->tp-1]=newTrueValue(exe->heap);
	}
	else if(firValue->type==IN32&&secValue->type==IN32)
	{
		if(*secValue->u.num>=*firValue->u.num)
			stack->values[stack->tp-1]=newTrueValue(exe->heap);
		else stack->values[stack->tp-1]=newNilValue(exe->heap);
	}
	else if(firValue->type==secValue->type&&(firValue->type==SYM||firValue->type==STR))
	{
		if(strcmp(secValue->u.str->str,firValue->u.str->str)>=0)
			stack->values[stack->tp-1]=newTrueValue(exe->heap);
		else stack->values[stack->tp-1]=newNilValue(exe->heap);
	}
	else return 1;
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
	if(firValue->type==DBL||secValue->type==DBL)
	{
		double result=((secValue->type==DBL)?*secValue->u.dbl:*secValue->u.num)-((firValue->type==DBL)?*firValue->u.dbl:*firValue->u.num);
		if(result<0)
			stack->values[stack->tp-1]=newTrueValue(exe->heap);
		else stack->values[stack->tp-1]=newNilValue(exe->heap);
	}
	else if(firValue->type==IN32&&secValue->type==IN32)
	{
		if(*secValue->u.num<*firValue->u.num)
			stack->values[stack->tp-1]=newTrueValue(exe->heap);
		else stack->values[stack->tp-1]=newNilValue(exe->heap);
	}
	else if(firValue->type==secValue->type&&(firValue->type==SYM||firValue->type==STR))
	{
		if(strcmp(secValue->u.str->str,firValue->u.str->str)<0)
			stack->values[stack->tp-1]=newTrueValue(exe->heap);
		else stack->values[stack->tp-1]=newNilValue(exe->heap);
	}
	else return 1;
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
	if(firValue->type==DBL||secValue->type==DBL)
	{
		double result=((secValue->type==DBL)?*secValue->u.dbl:*secValue->u.num)-((firValue->type==DBL)?*firValue->u.dbl:*firValue->u.num);
		if(result<=0)
			stack->values[stack->tp-1]=newTrueValue(exe->heap);
		else stack->values[stack->tp-1]=newNilValue(exe->heap);
	}
	else if(firValue->type==IN32&&secValue->type==IN32)
	{
		if(*secValue->u.num<=*firValue->u.num)
			stack->values[stack->tp-1]=newTrueValue(exe->heap);
		else stack->values[stack->tp-1]=newNilValue(exe->heap);
	}
	else if(firValue->type==secValue->type&&(firValue->type==SYM||firValue->type==STR))
	{
		if(strcmp(secValue->u.str->str,firValue->u.str->str)<=0)
			stack->values[stack->tp-1]=newTrueValue(exe->heap);
		else stack->values[stack->tp-1]=newNilValue(exe->heap);
	}
	else return 1;
	proc->cp+=1;
	return 0;
}

int B_not(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* tmpValue=getTopValue(stack);
	if(tmpValue->type==NIL||(tmpValue->type==PAIR&&getCar(tmpValue)->type==NIL&&getCdr(tmpValue)->type==NIL))
		stack->values[stack->tp-1]=newTrueValue(exe->heap);
	else
		stack->values[stack->tp-1]=newNilValue(exe->heap);
	proc->cp+=1;
	return 0;
}

int B_cast_to_chr(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	if(topValue->type==PAIR||topValue->type==PRC)return 1;
	VMvalue* tmpValue=newVMvalue(CHR,NULL,exe->heap,1);
	switch(topValue->type)
	{
		case IN32:*tmpValue->u.chr=*topValue->u.num;break;
		case DBL:*tmpValue->u.chr=(int32_t)*topValue->u.dbl;break;
		case CHR:*tmpValue->u.chr=*topValue->u.chr;break;
		case STR:
		case SYM:*tmpValue->u.chr=topValue->u.str->str[0];break;
		case BYTA:*tmpValue->u.dbl=*(char*)topValue->u.byta->arry;break;
	}
	stack->values[stack->tp-1]=tmpValue;
	proc->cp+=1;
	return 0;
}

int B_cast_to_int(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	if(topValue->type==PAIR||topValue->type==PRC)return 1;
	VMvalue* tmpValue=newVMvalue(IN32,NULL,exe->heap,1);
	switch(topValue->type)
	{
		case IN32:*tmpValue->u.num=*topValue->u.num;break;
		case DBL:*tmpValue->u.num=(int32_t)*topValue->u.dbl;break;
		case CHR:*tmpValue->u.num=*topValue->u.chr;break;
		case STR:
		case SYM:*tmpValue->u.num=stringToInt(topValue->u.str->str);break;
		case BYTA:*tmpValue->u.dbl=*(int32_t*)topValue->u.byta->arry;break;
	}
	stack->values[stack->tp-1]=tmpValue;
	proc->cp+=1;
	return 0;
}

int B_cast_to_dbl(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	if(topValue->type==PAIR||topValue->type==PRC)return 1;
	VMvalue* tmpValue=newVMvalue(DBL,NULL,exe->heap,1);
	switch(topValue->type)
	{
		case IN32:*tmpValue->u.dbl=(double)*topValue->u.num;break;
		case DBL:*tmpValue->u.dbl=*topValue->u.dbl;break;
		case CHR:*tmpValue->u.dbl=(double)(int32_t)*topValue->u.chr;break;
		case STR:
		case SYM:*tmpValue->u.dbl=stringToDouble(topValue->u.str->str);break;
		case BYTA:*tmpValue->u.dbl=*(double*)topValue->u.byta->arry;break;
	}
	stack->values[stack->tp-1]=tmpValue;
	proc->cp+=1;
	return 0;
}

int B_cast_to_str(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	if(topValue->type==PAIR||topValue->type==PRC)return 1;
	VMstr* tmpStr=newVMstr(NULL);
	VMvalue* tmpValue=newVMvalue(STR,tmpStr,exe->heap,1);
	switch(topValue->type)
	{
		case IN32:tmpValue->u.str->str=intToString(*topValue->u.num);break;
		case DBL:tmpValue->u.str->str=doubleToString(*topValue->u.dbl);break;
		case CHR:tmpValue->u.str->str=(char*)malloc(sizeof(char)*2);
				 if(tmpValue->u.str->str==NULL)errors("B_cast_to_str",__FILE__,__LINE__);
				 tmpValue->u.str->str[0]=*topValue->u.chr;
				 tmpValue->u.str->str[1]='\0';
				 break;
		case STR:
		case SYM:tmpValue->u.str->str=copyStr(topValue->u.str->str);break;
		case BYTA:tmpValue->u.str->str=copyStr(topValue->u.byta->arry);break;
	}
	stack->values[stack->tp-1]=tmpValue;
	proc->cp+=1;
	return 0;
}

int B_cast_to_sym(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	if(topValue->type==PAIR||topValue->type==PRC)return 1;
	VMstr* tmpStr=newVMstr(NULL);
	VMvalue* tmpValue=newVMvalue(SYM,tmpStr,exe->heap,1);
	switch(topValue->type)
	{
		case IN32:tmpValue->u.str->str=intToString((long)*topValue->u.num);break;
		case DBL:tmpValue->u.str->str=doubleToString(*topValue->u.dbl);break;
		case CHR:tmpValue->u.str->str=(char*)malloc(sizeof(char)*2);
				 if(tmpValue->u.str->str==NULL)errors("B_cast_to_sym",__FILE__,__LINE__);
				 tmpValue->u.str->str[0]=*topValue->u.chr;
				 tmpValue->u.str->str[1]='\0';
				 break;
		case STR:
		case SYM:tmpValue->u.str->str=copyStr(topValue->u.str->str);break;
		case BYTA:tmpValue->u.str->str=copyStr(topValue->u.byta->arry);break;
	}
	stack->values[stack->tp-1]=tmpValue;
	proc->cp+=1;
	return 0;
}

int B_cast_to_byte(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	if(topValue->type==PAIR||topValue->type==PRC)return 1;
	VMvalue* tmpValue=newVMvalue(BYTA,NULL,exe->heap,1);
	tmpValue->u.byta=newEmptyByteArry();
	switch(topValue->type)
	{
		case IN32:tmpValue->u.byta->size=sizeof(int32_t);
				 tmpValue->u.byta->arry=createByteArry(sizeof(int32_t));
				 *(int32_t*)tmpValue->u.byta->arry=*topValue->u.num;
				 break;
		case DBL:tmpValue->u.byta->size=sizeof(double);
				 tmpValue->u.byta->arry=createByteArry(sizeof(double));
				 *(double*)tmpValue->u.byta->arry=*topValue->u.dbl;
				 break;
		case CHR:tmpValue->u.byta->size=sizeof(char);
				 tmpValue->u.byta->arry=createByteArry(sizeof(char));
				 *(char*)tmpValue->u.byta->arry=*topValue->u.chr;
				 break;
		case STR:
		case SYM:tmpValue->u.byta->size=strlen(topValue->u.str->str+1);
				 tmpValue->u.byta->arry=copyStr(topValue->u.str->str);
				 break;
		case BYTA:tmpValue->u.byta->size=topValue->u.byta->size;
				  tmpValue->u.byta->arry=createByteArry(topValue->u.byta->size);
				  memcpy(tmpValue->u.byta->arry,topValue->u.byta->arry,topValue->u.byta->size);
				  break;
	}
	stack->values[stack->tp-1]=tmpValue;
	proc->cp+=1;
	return 0;
}

int B_is_chr(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* objValue=getTopValue(stack);
	if(objValue->type==CHR)
		stack->values[stack->tp-1]=newTrueValue(exe->heap);
	else stack->values[stack->tp-1]=newNilValue(exe->heap);
	proc->cp+=1;
	return 0;
}

int B_is_int(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* objValue=getTopValue(stack);
	if(objValue->type==IN32)
		stack->values[stack->tp-1]=newTrueValue(exe->heap);
	else stack->values[stack->tp-1]=newNilValue(exe->heap);
	proc->cp+=1;
	return 0;
}

int B_is_dbl(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* objValue=getTopValue(stack);
	if(objValue->type==DBL)
		stack->values[stack->tp-1]=newTrueValue(exe->heap);
	else stack->values[stack->tp-1]=newNilValue(exe->heap);
	proc->cp+=1;
	return 0;
}

int B_is_str(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* objValue=getTopValue(stack);
	if(objValue->type==STR)
		stack->values[stack->tp-1]=newTrueValue(exe->heap);
	else stack->values[stack->tp-1]=newNilValue(exe->heap);
	proc->cp+=1;
	return 0;
}

int B_is_sym(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* objValue=getTopValue(stack);
	if(objValue->type==SYM)
		stack->values[stack->tp-1]=newTrueValue(exe->heap);
	else stack->values[stack->tp-1]=newNilValue(exe->heap);
	proc->cp+=1;
	return 0;
}

int B_is_prc(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* objValue=getTopValue(stack);
	if(objValue->type==PRC)
		stack->values[stack->tp-1]=newTrueValue(exe->heap);
	else stack->values[stack->tp-1]=newNilValue(exe->heap);
	proc->cp+=1;
	return 0;
}

int B_is_byte(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	if(topValue->type==BYTA)
		stack->values[stack->tp-1]=newTrueValue(exe->heap);
	else stack->values[stack->tp-1]=newNilValue(exe->heap);
	proc->cp+=1;
	return 0;
}

int B_nth(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* place=getValue(stack,stack->tp-2);
	VMvalue* objlist=getTopValue(stack);
	if((objlist->type!=PAIR&&objlist->type!=STR&&objlist->type!=BYTA)||place->type!=IN32)return 1;
	if(objlist->type==PAIR)
	{
		VMvalue* obj=getCar(objlist);
		VMvalue* objPair=getCdr(objlist);
		int i=0;
		for(;i<*place->u.num;i++)
		{
			if(objPair==NULL)
			{
				obj=newNilValue(exe->heap);
				break;
			}
			obj=getCar(objPair);
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
		VMvalue* objChr=newVMvalue(CHR,objlist->u.str->str+*place->u.num,exe->heap,0);
		stack->values[stack->tp-1]=objChr;
	}
	else if(objlist->type==BYTA)
	{
		stack->tp-=1;
		stackRecycle(exe);
		ByteArry* tmpByte=newByteArry(1,NULL);
		tmpByte->arry=objlist->u.byta->arry+*place->u.num;
		VMvalue* objByte=newVMvalue(BYTA,tmpByte,exe->heap,0);
		stack->values[stack->tp-1]=objByte;
	}
	proc->cp+=1;
	return 0;
}

int B_length(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* objlist=getTopValue(stack);
	if(objlist->type!=PAIR&&objlist->type!=STR&&objlist->type!=BYTA)return 1;
	if(objlist->type==PAIR)
	{
		int32_t i=0;
		for(VMvalue* tmp=objlist;tmp!=NULL&&tmp->type==PAIR;tmp=getCdr(tmp))i++;
		stack->values[stack->tp-1]=newVMvalue(IN32,&i,exe->heap,1);
	}
	else if(objlist->type==STR)
	{
		int32_t len=strlen(objlist->u.str->str);
		stack->values[stack->tp-1]=newVMvalue(IN32,&len,exe->heap,1);
	}
	else if(objlist->type==BYTA)
		stack->values[stack->tp-1]=newVMvalue(IN32,&objlist->u.byta->size,exe->heap,1);
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
	if(sec->type!=NIL&&sec->type!=PAIR)return 1;
	if(sec->type!=NIL)
	{
		VMvalue* copyOfsec=copyValue(sec,exe->heap);
		VMvalue* lastpair=copyOfsec;
		while(getCdr(lastpair)->type!=NIL)lastpair=getCdr(lastpair);
		lastpair->u.pair->cdr=fir;
		stack->values[stack->tp-1]=copyOfsec;
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
	if(fir->type!=STR||sec->type!=STR)return 1;
	int firlen=strlen(fir->u.str->str);
	int seclen=strlen(sec->u.str->str);
	char* tmpStr=(char*)malloc(sizeof(char)*(firlen+seclen+1));
	if(tmpStr==NULL)errors("B_str_cat",__FILE__,__LINE__);
	stack->tp-=1;
	stackRecycle(exe);
	strcpy(tmpStr,sec->u.str->str);
	strcat(tmpStr,fir->u.str->str);
	VMstr* tmpVMstr=newVMstr(NULL);
	tmpVMstr->str=tmpStr;
	VMvalue* tmpValue=newVMvalue(STR,tmpVMstr,exe->heap,0);
	tmpValue->access=1;
	stack->values[stack->tp-1]=tmpValue;
	proc->cp+=1;
	return 0;
}

int B_byte_cat(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* fir=getTopValue(stack);
	VMvalue* sec=getValue(stack,stack->tp-2);
	if(fir->type!=BYTA||sec->type!=BYTA)return 1;
	int32_t firSize=fir->u.byta->size;
	int32_t secSize=sec->u.byta->size;
	VMvalue* tmpByte=newVMvalue(BYTA,NULL,exe->heap,1);
	tmpByte->u.byta=newEmptyByteArry();
	tmpByte->u.byta->size=firSize+secSize;
	uint8_t* tmpArry=createByteArry(firSize+secSize);
	memcpy(tmpArry,sec->u.byta->arry,secSize);
	memcpy(tmpArry+firSize,fir->u.byta->arry,firSize);
	tmpByte->u.byta->arry=tmpArry;
	stack->tp-=1;
	stackRecycle(exe);
	stack->values[stack->tp-1]=tmpByte;
	proc->cp+=1;
	return 0;
}

int B_read(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	filestack* files=exe->files;
	VMvalue* file=getTopValue(stack);
	if(file->type!=IN32)return 1;
	FILE* tmpFile=getFile(files,*file->u.num);
	if(tmpFile==NULL)return 2;
	char* tmpString=getListFromFile(tmpFile);
	intpr* tmpIntpr=newTmpIntpr(NULL,tmpFile);
	AST_cptr* tmpCptr=createTree(tmpString,tmpIntpr);
	VMvalue* tmp=NULL;
	if(tmpCptr==NULL)return 3;
	tmp=castCptrVMvalue(tmpCptr,exe->heap);
	stack->values[stack->tp-1]=tmp;
	free(tmpIntpr);
	free(tmpString);
	deleteCptr(tmpCptr);
	free(tmpCptr);
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
	if(file->type!=IN32||size->type!=IN32)return 1;
	FILE* fp=getFile(files,*file->u.num);
	if(fp==NULL)return 2;
	VMvalue* tmpBary=newVMvalue(BYTA,NULL,exe->heap,1);
	uint8_t* arry=(uint8_t*)malloc(sizeof(uint8_t)*(*size->u.num));
	if(arry==NULL)errors("B_readb",__FILE__,__LINE__);
	fread(arry,sizeof(uint8_t),*size->u.num,fp);
	tmpBary->u.byta=newEmptyByteArry();
	tmpBary->u.byta->size=*size->u.num;
	tmpBary->u.byta->arry=arry;
	stack->tp-=1;
	stackRecycle(exe);
	stack->values[stack->tp-1]=tmpBary;
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
	if(file->type!=IN32)return 1;
	FILE* objFile=getFile(files,*file->u.num);
	if(objFile==NULL)return 2;
	stack->tp-=1;
	stackRecycle(exe);
	VMpair* tmpPair=(obj->type==PAIR)?obj->u.pair:NULL;
	printVMvalue(obj,tmpPair,objFile,0);
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
	if(file->type!=IN32||bt->type!=BYTA)return 1;
	FILE* objFile=getFile(files,*file->u.num);
	if(objFile==NULL)return 2;
	stack->tp-=1;
	stackRecycle(exe);
	fwrite(bt->u.byta->arry,sizeof(uint8_t),bt->u.byta->size,objFile);
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
	if(file->type!=IN32)return 1;
	if(*file->u.num>=files->size)return 2;
	FILE* objFile=files->files[*file->u.num];
	if(objFile==NULL)return 2;
	stack->tp-=1;
	stackRecycle(exe);
	VMpair* tmpPair=(obj->type==PAIR)?obj->u.pair:NULL;
	princVMvalue(obj,tmpPair,objFile);
	proc->cp+=1;
	return 0;
}

int B_tell(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	filestack* files=exe->files;
	VMvalue* file=getTopValue(stack);
	if(file->type!=IN32)return 1;
	if(*file->u.num>=files->size)return 2;
	FILE* objFile=files->files[*file->u.num];
	if(objFile==NULL)return 2;
	else
	{
		int32_t place=ftell(objFile);
		VMvalue* tmp=newVMvalue(IN32,&place,exe->heap,1);
	}
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
	if(where->type!=IN32||file->type!=IN32)return 1;
	if(*file->u.num>=files->size)return 2;
	FILE* objFile=files->files[*file->u.num];
	if(objFile==NULL)return 2;
	else
	{
		int32_t place=fseek(objFile,*where->u.num,SEEK_CUR);
		VMvalue* tmp=newVMvalue(IN32,&place,exe->heap,1);
	}
	proc->cp+=1;
	return 0;
}

int B_rewind(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	filestack* files=exe->files;
	VMvalue* file=getTopValue(stack);
	if(file->type!=IN32)return 1;
	if(*file->u.num>=files->size)return 2;
	FILE* objFile=files->files[*file->u.num];
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
	if(exitStatus->type!=IN32)return 1;
	pthread_rwlock_destroy(&GClock);
	exit(*exitStatus->u.num);
	proc->cp+=1;
	return 0;
}

int B_go(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* threadArg=getTopValue(stack);
	VMvalue* threadProc=getValue(stack,stack->tp-2);
	if(threadProc->type!=PRC||(threadArg->type!=PAIR&&threadArg->type!=NIL))
		return 1;
	fakeVM* threadVM=newThreadVM(threadProc->u.prc,exe->procs,exe->files,exe->heap,exe->modules);
	VMstack* threadVMstack=threadVM->stack;
	VMvalue* prevBp=newVMvalue(IN32,&threadVMstack->bp,exe->heap,1);
	if(threadVMstack->tp>=threadVMstack->size)
	{
		threadVMstack->values=(VMvalue**)realloc(threadVMstack->values,sizeof(VMvalue*)*(threadVMstack->size+64));
		if(threadVMstack->values==NULL)errors("B_go",__FILE__,__LINE__);
		threadVMstack->size+=64;
	}
	threadVMstack->values[threadVMstack->tp]=prevBp;
	threadVMstack->tp+=1;
	threadVMstack->bp=threadVMstack->tp;
	while(threadArg->type!=NIL)
	{
		if(threadVMstack->tp>=threadVMstack->size)
		{
			threadVMstack->values=(VMvalue**)realloc(threadVMstack->values,sizeof(VMvalue*)*(threadVMstack->size+64));
			if(threadVMstack->values==NULL)errors("B_go",__FILE__,__LINE__);
			threadVMstack->size+=64;
		}
		VMvalue* tmp=newNilValue(exe->heap);
		copyRef(tmp,threadArg->u.pair->car);
		threadVMstack->values[threadVMstack->tp]=tmp;
		threadVMstack->tp+=1;
		threadArg=threadArg->u.pair->cdr;
	}
	stack->tp-=1;
	if(pthread_create(&threadVM->tid,NULL,ThreadVMFunc,threadVM))
		return 6;
	else
		stack->values[stack->tp-1]=newVMvalue(IN32,&threadVM->VMid,exe->heap,1);
	proc->cp+=1;
	return 0;
}

int B_send(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* tid=getTopValue(stack);
	VMvalue* message=getValue(stack,stack->tp-2);
	if(tid->type!=IN32)return 1;
	if(*tid->u.num>=GlobFakeVMs.size)return 7;
	fakeVM* objVM=GlobFakeVMs.VMs[*tid->u.num];
	if(objVM==NULL||objVM->mark==0)return 7;
	pthread_mutex_lock(&objVM->lock);
	if(objVM->queueTail==NULL)
	{
		objVM->queueHead=newThreadMessage(message,exe->heap);
		objVM->queueTail=objVM->queueHead;
	}
	else
	{
		threadMessage* cur=objVM->queueTail;
		cur->next=newThreadMessage(message,exe->heap);
		objVM->queueTail=cur->next;
	}
	pthread_mutex_unlock(&objVM->lock);
	stack->tp-=1;
	stackRecycle(exe);
	proc->cp+=1;
	return 0;
}

int B_accept(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	if(stack->tp>=stack->size)
	{
		stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size+64));
		if(stack->values==NULL)errors("B_accept",__FILE__,__LINE__);
		stack->size+=64;
	}
	VMvalue* tmp=newNilValue(exe->heap);
	tmp->access=1;
	while(exe->queueHead==NULL);
	pthread_mutex_lock(&exe->lock);
	copyRef(tmp,exe->queueHead->message);
	threadMessage* prev=exe->queueHead;
	{
		exe->queueHead=prev->next;
		if(exe->queueHead==NULL)
			exe->queueTail=NULL;
	}
	free(prev);
	pthread_mutex_unlock(&exe->lock);
	stack->values[stack->tp]=tmp;
	stack->tp+=1;
	proc->cp+=1;
	return 0;
}

int B_getid(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	if(stack->tp>=stack->size)
	{
		stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size+64));
		if(stack->values==NULL)errors("B_getid",__FILE__,__LINE__);
		stack->size+=64;
	}
	VMvalue* tmp=newVMvalue(IN32,&exe->VMid,exe->heap,1);
	stack->values[stack->tp]=tmp;
	stack->tp+=1;
	proc->cp+=1;
	return 0;
}

int B_slp(fakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* second=getTopValue(stack);
	VMvalue* mode=getValue(stack,stack->tp-2);
	if(second->type!=IN32)return 1;
	if(mode->type==NIL||(mode->type==PAIR&&getCar(mode)->type==NIL&&getCdr(mode)->type==NIL))
		sleep(*second->u.num);
	else
	{
#ifdef _WIN32
		Sleep(*second->u.num);
#else
		usleep(*second->u.num);
#endif
	}
	stack->tp-=1;
	stack->values[stack->tp-1]=second;
	proc->cp+=1;
	return 0;
}

VMstack* newStack(int32_t size)
{
	VMstack* tmp=(VMstack*)malloc(sizeof(VMstack));
	if(tmp==NULL)errors("newStack",__FILE__,__LINE__);
	tmp->size=size;
	tmp->tp=0;
	tmp->bp=0;
	tmp->values=(VMvalue**)malloc(size*sizeof(VMvalue*));
	if(tmp->values==NULL)errors("newStack",__FILE__,__LINE__);
	return tmp;
}

filestack* newFileStack()
{
	filestack* tmp=(filestack*)malloc(sizeof(filestack));
	if(tmp==NULL)errors("newFileStack",__FILE__,__LINE__);
	tmp->size=3;
	pthread_mutex_init(&tmp->lock,NULL);
	tmp->files=(FILE**)malloc(sizeof(FILE*)*3);
	tmp->files[0]=stdin;
	tmp->files[1]=stdout;
	tmp->files[2]=stderr;
	return tmp;
}

void freeFileStack(filestack* s)
{
	int i=3;
	for(;i<s->size;i++)
		if(s->files[i])
			fclose(s->files[i]);
	free(s->files);
	pthread_mutex_destroy(&s->lock);
	free(s);
}

void printVMvalue(VMvalue* objValue,VMpair* begin,FILE* fp,int8_t mode)
{
	switch(objValue->type)
	{
		case NIL:fprintf(fp,"nil");break;
		case IN32:fprintf(fp,"%d",*objValue->u.num);break;
		case DBL:fprintf(fp,"%lf",*objValue->u.dbl);break;
		case CHR:printRawChar(*objValue->u.chr,fp);break;
		case SYM:fprintf(fp,"%s",objValue->u.str->str);break;
		case STR:printRawString(objValue->u.str->str,fp);break;
		case PRC:
				if(mode==1)printProc(objValue->u.prc,fp);
				else fprintf(fp,"<#proc>");break;
		case PAIR:
				putc('(',fp);
				if(objValue->u.pair->car->type==PAIR&&objValue->u.pair->car->u.pair==begin)
					fprintf(fp,"##");
				else
					printVMvalue(objValue->u.pair->car,begin,fp,mode);
				if(objValue->u.pair->cdr->type!=NIL)
				{
					putc(',',fp);
					if(objValue->u.pair->cdr->type==PAIR&&objValue->u.pair->cdr->u.pair==begin)
						fprintf(fp,"##");
					else
						printVMvalue(objValue->u.pair->cdr,begin,fp,mode);
				}
				putc(')',fp);
				break;
		case BYTA:
				printByteArry(objValue->u.byta,fp,1);
				break;
		default:fprintf(fp,"Bad value!");break;
	}
}

void princVMvalue(VMvalue* objValue,VMpair* begin,FILE* fp)
{
	switch(objValue->type)
	{
		case NIL:fprintf(fp,"nil");break;
		case IN32:fprintf(fp,"%d",*objValue->u.num);break;
		case DBL:fprintf(fp,"%lf",*objValue->u.dbl);break;
		case CHR:putc(*objValue->u.chr,fp);break;
		case SYM:fprintf(fp,"%s",objValue->u.str->str);break;
		case STR:fprintf(fp,"%s",objValue->u.str->str);break;
		case PRC:
				fprintf(fp,"<#proc>");break;
		case PAIR:
				putc('(',fp);
				if(objValue->u.pair->car->type==PAIR&&objValue->u.pair->car->u.pair==begin)
					fprintf(fp,"##");
				else
					princVMvalue(objValue->u.pair->car,begin,fp);
				if(objValue->u.pair->cdr->type!=NIL)
				{
					putc(',',fp);
					if(objValue->u.pair->cdr->type==PAIR&&objValue->u.pair->cdr->u.pair==begin)
						fprintf(fp,"##");
					else
						princVMvalue(objValue->u.pair->cdr,begin,fp);
				}
				putc(')',fp);
				break;
		case BYTA:
				printByteArry(objValue->u.byta,fp,0);
				break;
		default:fprintf(fp,"Bad value!");break;
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
				errors("stackRecycle",__FILE__,__LINE__);
			}
			stack->size-=64;
	//	}
	}
}

VMcode* newBuiltInProc(ByteCode* proc)
{
	VMcode* tmp=(VMcode*)malloc(sizeof(VMcode));
	if(tmp==NULL)errors("newBuiltInProc",__FILE__,__LINE__);
	tmp->localenv=newVMenv(0,NULL);
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

VMprocess* newFakeProcess(VMcode* code,VMprocess* prev)
{
	VMprocess* tmp=(VMprocess*)malloc(sizeof(VMprocess));
	if(tmp==NULL)errors("newFakeProcess",__FILE__,__LINE__);
	tmp->prev=prev;
	tmp->cp=0;
	tmp->code=code;
//	fprintf(stdout,"New proc: %p\n",code);
	return tmp;
}

void printAllStack(VMstack* stack,FILE* fp)
{
	fprintf(fp,"Current stack:\n");
	if(stack->tp==0)fprintf(fp,"[#EMPTY]\n");
	else
	{
		int i=stack->tp-1;
		for(;i>=0;i--)
		{
			fprintf(fp,"%d:",i);
			VMvalue* tmp=stack->values[i];
			VMpair* tmpPair=(tmp->type==PAIR)?tmp->u.pair:NULL;
			printVMvalue(tmp,tmpPair,fp,0);
			putc('\n',fp);
		}
	}
}

VMprocess* hasSameProc(VMcode* objCode,VMprocess* curproc)
{
	while(curproc!=NULL&&curproc->code!=objCode)
		curproc=curproc->prev;
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

void printEnv(VMenv* curEnv,FILE* fp)
{
	if(curEnv->size==0)
		fprintf(fp,"This ENV is empty!");
	else
	{
		fprintf(fp,"ENV:");
		for(int i=0;i<curEnv->size;i++)
		{
			VMvalue* tmp=curEnv->values[i];
			VMpair* tmpPair=(tmp->type==PAIR)?tmp->u.pair:NULL;
			printVMvalue(tmp,tmpPair,fp,0);
			putc(' ',fp);
		}
	}
}

void printProc(VMcode* code,FILE* fp)
{
	fputs("\n<#proc\n",fp);
	ByteCode tmp={code->size,code->code};
	printByteCode(&tmp,fp);
	fputc('>',fp);
}

void writeRef(VMvalue* fir,VMvalue* sec)
{
	if(fir->type>=IN32&&fir->type<=DBL)
	{
		switch(sec->type)
		{
			case IN32:*fir->u.num=*sec->u.num;break;
			case CHR:*fir->u.chr=*sec->u.chr;break;
			case DBL:*fir->u.dbl=*sec->u.dbl;break;
		}
	}
	else if(fir->type>=SYM&&fir->type<=PAIR)
	{
		if(fir->access)freeRef(fir);
		switch(fir->type)
		{
			case PAIR:
				fir->u.pair=sec->u.pair;
				fir->u.pair->refcount+=1;
				break;
			case PRC:
				fir->u.prc=sec->u.prc;
				fir->u.prc->refcount+=1;
				break;
			case SYM:
			case STR:
				if(!fir->access)
					memcpy(fir->u.str->str,sec->u.str->str,(strlen(fir->u.str->str)>strlen(sec->u.str->str))?strlen(sec->u.str->str):strlen(fir->u.str->str));
				else
				{
					fir->u.str=sec->u.str;
					fir->u.str+=1;
				}
				break;
			case BYTA:
				if(!fir->access)
					memcpy(fir->u.byta->arry,sec->u.byta->arry,(fir->u.byta->size>sec->u.byta->size)?sec->u.byta->size:fir->u.byta->size);
				else
				{
					fir->u.byta=sec->u.byta;
					fir->u.byta->refcount+=1;
				}
				break;
		}
	}
}

VMheap* newVMheap()
{
	VMheap* tmp=(VMheap*)malloc(sizeof(VMheap));
	if(tmp==NULL)errors("newVMheap",__FILE__,__LINE__);
	tmp->size=0;
	tmp->threshold=THRESHOLD_SIZE;
	tmp->head=NULL;
	pthread_mutex_init(&tmp->lock,NULL);
	return tmp;
}

void GC_mark(fakeVM* exe)
{
	GC_markValueInStack(exe->stack);
	GC_markValueInCallChain(exe->curproc);
	GC_markMessage(exe->queueHead);
}

void GC_markValue(VMvalue* obj)
{
	if(!obj->mark)
	{
		//princVMvalue(obj,stderr);
		//putc('\n',stderr);
		obj->mark=1;
		if(obj->type==PAIR)
		{
			GC_markValue(getCar(obj));
			GC_markValue(getCdr(obj));
		}
		else if(obj->type==PRC)
		{
			VMenv* curEnv=obj->u.prc->localenv;
			for(;curEnv!=NULL;curEnv=curEnv->prev)
				GC_markValueInEnv(curEnv);
		}
	}
}

void GC_markValueInEnv(VMenv* curEnv)
{
	int32_t i=0;
	for(;i<curEnv->size;i++)
		GC_markValue(curEnv->values[i]);
}

void GC_markValueInCallChain(VMprocess* cur)
{
	for(;cur!=NULL;cur=cur->prev)
	{
		VMenv* curEnv=cur->localenv;
		for(;curEnv!=NULL;curEnv=curEnv->prev)
			GC_markValueInEnv(curEnv);
	}
}

void GC_markValueInStack(VMstack* stack)
{
	int32_t i=0;
	for(;i<stack->tp;i++)
		GC_markValue(stack->values[i]);
}

void GC_markMessage(threadMessage* head)
{
	while(head!=NULL)
	{
		GC_markValue(head->message);
		head=head->next;
	}
}

void GC_sweep(VMheap* heap)
{
	VMvalue* cur=heap->head;
	while(cur!=NULL)
	{
		if(!cur->mark)
		{
			VMvalue* prev=cur;
			//princVMvalue(cur,stderr);
			//putc('\n',stderr);
			if(cur==heap->head)
				heap->head=cur->next;
			if(cur->next!=NULL)cur->next->prev=cur->prev;
			if(cur->prev!=NULL)cur->prev->next=cur->next;
			freeRef(cur);
			cur=cur->next;
			free(prev);
			heap->size-=1;
		}
		else
		{
			cur->mark=0;
			cur=cur->next;
		}
	}
}

void freeRef(VMvalue* obj)
{
	if(obj->type<SYM&&obj->type>NIL&&obj->access)
		free(obj->u.all);
	else if(obj->type>=SYM&&obj->type<ATM)
	{
		switch(obj->type)
		{
			case SYM:
			case STR:
				if(!obj->u.str->refcount)
				{
					if(obj->access)
						free(obj->u.str->str);
					free(obj->u.str);
				}
				else obj->u.str->refcount-=1;
				break;
			case PAIR:
				if(!obj->u.pair->refcount)
					free(obj->u.pair);
				else obj->u.pair->refcount-=1;
				break;
			case PRC:
				if(!obj->u.prc->refcount)
					freeVMcode(obj->u.prc);
				else obj->u.prc->refcount-=1;
				break;
			case BYTA:
				if(!obj->u.byta->refcount)
				{
					if(obj->access)free(obj->u.byta->arry);
					free(obj->u.byta);
				}
				else obj->u.byta->refcount-=1;
				break;
		}
	}
}

void copyRef(VMvalue* fir,VMvalue* sec)
{
	freeRef(fir);
	fir->type=sec->type;
	if(fir->type<SYM&&fir->type>NIL)
	{
		fir->access=1;
		switch(fir->type)
		{
			case IN32:
				fir->u.num=copyMemory(sec->u.num,sizeof(int32_t));
				break;
			case DBL:
				fir->u.dbl=copyMemory(sec->u.dbl,sizeof(double));
				break;
			case CHR:
				fir->u.chr=copyMemory(sec->u.chr,sizeof(char));
				break;
		}
	}
	else if(fir->type>=SYM&&fir->type<ATM)
	{
		switch(fir->type)
		{
			case SYM:
			case STR:
				if(!sec->access)
				{
					fir->u.str=newVMstr(sec->u.str->str);
					fir->u.str->refcount+=1;
				}
				else
				{
					sec->u.str->refcount+=1;
					fir->u.str=sec->u.str;
				}
				break;
			case PAIR:
				sec->u.pair->refcount+=1;
				fir->u.pair=sec->u.pair;
				break;
			case PRC:
				sec->u.prc->refcount+=1;
				fir->u.prc=sec->u.prc;
				break;
			case BYTA:
				if(!sec->access)
				{
					fir->u.byta=newByteArry(sec->u.byta->size,sec->u.byta->arry);
					fir->u.byta->refcount+=1;
				}
				else
				{
					sec->u.byta->refcount+=1;
					fir->u.byta=sec->u.byta;
				}
				break;
			case NIL:
				fir->u.all=NULL;
				break;
		}
	}
}

fakeVM* newThreadVM(VMcode* main,ByteCode* procs,filestack* files,VMheap* heap,Dlls* d)
{
	fakeVM* exe=(fakeVM*)malloc(sizeof(fakeVM));
	if(exe==NULL)errors("newThreadVM",__FILE__,__LINE__);
	exe->mainproc=newFakeProcess(main,NULL);
	exe->mainproc->localenv=main->localenv;
	exe->curproc=exe->mainproc;
	exe->procs=procs;
	exe->mark=1;
	exe->modules=d;
	main->refcount+=1;
	exe->stack=newStack(0);
	exe->queueHead=NULL;
	exe->queueTail=NULL;
	exe->files=files;
	exe->heap=heap;
	pthread_mutex_init(&exe->lock,NULL);
	fakeVM** ppFakeVM=NULL;
	int i=0;
	for(;i<GlobFakeVMs.size;i++)
		if(GlobFakeVMs.VMs[i]==NULL)
			ppFakeVM=GlobFakeVMs.VMs+i;
	if(ppFakeVM!=NULL)
	{
		exe->VMid=i;
		*ppFakeVM=exe;
	}
	else
	{
		int32_t size=GlobFakeVMs.size;
		GlobFakeVMs.VMs=(fakeVM**)realloc(GlobFakeVMs.VMs,sizeof(fakeVM*)*(size+1));
		if(size!=0&&GlobFakeVMs.VMs==NULL)errors("newThreadVM",__FILE__,__LINE__);
		GlobFakeVMs.VMs[size]=exe;
		GlobFakeVMs.size+=1;
		exe->VMid=size;
	}
	return exe;
}

void freeVMstack(VMstack* stack)
{
	free(stack->values);
	free(stack);
}

void freeMessage(threadMessage* cur)
{
	while(cur!=NULL)
	{
		threadMessage* prev=cur;
		cur=cur->next;
		free(prev);
	}
}

threadMessage* newThreadMessage(VMvalue* val,VMheap* heap)
{
	threadMessage* tmp=(threadMessage*)malloc(sizeof(threadMessage));
	if(tmp==NULL)errors("newThreadMessage",__FILE__,__LINE__);
	tmp->message=newNilValue(heap);
	tmp->message->access=1;
	copyRef(tmp->message,val);
	tmp->next=NULL;
	return tmp;
}

void freeAllVMs()
{
	int i=1;
	fakeVM* cur=GlobFakeVMs.VMs[0];
	pthread_mutex_destroy(&cur->lock);
	if(cur->mainproc->code)
		freeVMcode(cur->mainproc->code);
	free(cur->mainproc);
	freeVMstack(cur->stack);
	freeFileStack(cur->files);
	deleteAllDll(cur->modules);
	freeMessage(cur->queueHead);
	free(cur);
	for(;i<GlobFakeVMs.size;i++)
	{
		cur=GlobFakeVMs.VMs[i];
		if(cur!=NULL)
			free(cur);
	}
	free(GlobFakeVMs.VMs);
}

void freeVMheap(VMheap* h)
{
	GC_sweep(h);
	pthread_mutex_destroy(&h->lock);
	free(h);
}

void joinAllThread()
{
	int i=1;
	for(;i<GlobFakeVMs.size;i++)
	{
		fakeVM* cur=GlobFakeVMs.VMs[i];
		pthread_join(cur->tid,NULL);
	}
}

intpr* newTmpIntpr(const char* filename,FILE* fp)
{
	intpr* tmp=NULL;
	if(!(tmp=(intpr*)malloc(sizeof(intpr))))errors("newTmpIntpr",__FILE__,__LINE__);
	tmp->filename=copyStr(filename);
	if(fp!=stdin&&filename)
	{
#ifdef _WIN32
		char* rp=_fullpath(NULL,filename,0);
#else
		char* rp=realpath(filename,0);
#endif
		if(rp==NULL)
		{
			perror(rp);
			exit(EXIT_FAILURE);
		}
		tmp->curDir=getDir(rp);
		free(rp);
	}
	else
		tmp->curDir=NULL;
	tmp->file=fp;
	tmp->curline=1;
	tmp->procs=NULL;
	tmp->prev=NULL;
	tmp->modules=NULL;
	tmp->head=NULL;
	tmp->tail=NULL;
	tmp->glob=NULL;
	return tmp;
}
