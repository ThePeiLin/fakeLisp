#define USE_CODE_NAME
#include"common.h"
#include"VMtool.h"
#include"reader.h"
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

static void sortVMenvList(VMenv*);
static int32_t getSymbolIdInByteCode(const char*);
extern char* builtInSymbolList[NUMOFBUILTINSYMBOL];
pthread_rwlock_t GClock=PTHREAD_RWLOCK_INITIALIZER;
FakeVMlist GlobFakeVMs={0,NULL};
static int (*ByteCodes[])(FakeVM*)=
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
	B_push_mod_proc,
	B_push_list_arg,
	B_pop_var,
	B_pop_rest_var,
	B_pop_car,
	B_pop_cdr,
	B_pop_ref,
	B_pack_cc,
	B_call_proc,
	B_set_tp,
	B_set_bp,
	B_invoke,
	B_res_tp,
	B_pop_tp,
	B_res_bp,
	B_jump_if_ture,
	B_jump_if_false,
	B_jump,
	B_add,
	B_sub,
	B_mul,
	B_div,
	B_rem,
	B_atom,
	B_null,
	B_type,
	B_cast_to_chr,
	B_cast_to_int,
	B_cast_to_dbl,
	B_cast_to_str,
	B_cast_to_sym,
	B_cast_to_byte,
	B_nth,
	B_length,
	B_appd,
	B_open,
	B_eq,
	B_eqn,
	B_equal,
	B_gt,
	B_ge,
	B_lt,
	B_le,
	B_not,
	B_read,
	B_getb,
	B_write,
	B_putb,
	B_princ,
	B_go,
	B_chanl,
	B_send,
	B_recv,
};

ByteCode P_cons=
{
	32,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_POP_VAR,0,0,0,0, 1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_PAIR,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_POP_CAR,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_POP_CDR,
	}
};

ByteCode P_car=
{
	16,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_CAR,
	}
};

ByteCode P_cdr=
{
	16,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_CDR,
	}
};

ByteCode P_atom=
{
	16,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_ATOM,
	}
};

ByteCode P_null=
{
	16,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_NULL,
	}
};

ByteCode P_type=
{
	16,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_TYPE_OF,
	}
};

ByteCode P_aply=
{
	32,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_POP_VAR,0,0,0,0, 1,0,0,0,
		FAKE_RES_BP,
		FAKE_SET_BP,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_PUSH_LIST_ARG,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_INVOKE,
	}
};

ByteCode P_eq=
{
	30,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_POP_VAR,0,0,0,0, 1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_EQ,
	}
};

ByteCode P_eqn=
{
	30,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_POP_VAR,0,0,0,0, 1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_EQN,
	}
};

ByteCode P_equal=
{
	30,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_POP_VAR,0,0,0,0, 1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_EQUAL,
	}
};

ByteCode P_gt=
{
	30,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_POP_VAR,0,0,0,0, 1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_GT,
	}
};

ByteCode P_ge=
{
	30,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_POP_VAR,0,0,0,0, 1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_GE,
	}
};

ByteCode P_lt=
{
	30,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_POP_VAR,0,0,0,0, 1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_LT,
	}
};

ByteCode P_le=
{
	30,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_POP_VAR,0,0,0,0, 1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_LE,
	}
};

ByteCode P_not=
{
	16,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_NOT,
	}
};

ByteCode P_dbl=
{
	16,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_CAST_TO_DBL,
	}
};

ByteCode P_str=
{
	16,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_CAST_TO_STR,
	}
};

ByteCode P_sym=
{
	16,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_CAST_TO_SYM,
	}
};

ByteCode P_chr=
{
	16,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_CAST_TO_CHR,
	}
};

ByteCode P_int=
{
	16,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_CAST_TO_INT,
	}
};

ByteCode P_byt=
{
	16,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_CAST_TO_BYTE,
	}
};

ByteCode P_add=
{
	30,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_POP_VAR,0,0,0,0, 1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_ADD,
	}
};

ByteCode P_sub=
{
	30,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_POP_VAR,0,0,0,0, 1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_SUB,
	}
};

ByteCode P_mul=
{
	30,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_POP_VAR,0,0,0,0, 1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_MUL,
	}
};

ByteCode P_div=
{
	30,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_POP_VAR,0,0,0,0, 1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_DIV,
	}
};

ByteCode P_rem=
{
	30,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_POP_VAR,0,0,0,0, 1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_REM,
	}
};

ByteCode P_nth=
{
	30,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_POP_VAR,0,0,0,0, 1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_NTH,
	}
};

ByteCode P_length=
{
	16,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_LENGTH,
	}
};

ByteCode P_appd=
{
	30,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_POP_VAR,0,0,0,0, 1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_APPD,
	}
};

ByteCode P_open=
{
	30,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_POP_VAR,0,0,0,0, 1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_OPEN,
	}
};

ByteCode P_read=
{
	16,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_READ,
	}
};

ByteCode P_getb=
{
	30,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_POP_VAR,0,0,0,0, 1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_GETB,
	}
};

ByteCode P_write=
{
	30,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_POP_VAR,0,0,0,0, 1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_WRITE,
	}
};

ByteCode P_putb=
{
	30,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_POP_VAR,0,0,0,0, 1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_PUTB,
	}
};

ByteCode P_princ=
{
	30,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_POP_VAR,0,0,0,0, 1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_PRINC,
	}
};

ByteCode P_go=
{
	30,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_POP_REST_VAR,0,0,0,0, 1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_GO,
	}
};

ByteCode P_chanl=
{
	16,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_CHANL,
	}
};

ByteCode P_send=
{
	30,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_POP_VAR,0,0,0,0, 1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_SEND,
	}
};

ByteCode P_recv=
{
	16,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_RECV,
	}
};

ByteCode P_clcc=
{
	32,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0, 0,0,0,0,
		FAKE_RES_BP,
		FAKE_PACK_CC,
		FAKE_POP_VAR,0,0,0,0, 1,0,0,0,
		FAKE_SET_BP,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_INVOKE,
	}
};

FakeVM* newFakeVM(ByteCode* mainproc,ByteCode* procs)
{
	FakeVM* exe=(FakeVM*)malloc(sizeof(FakeVM));
	if(exe==NULL)errors("newFakeVM",__FILE__,__LINE__);
	if(mainproc!=NULL)
		exe->mainproc=newFakeProcess(newVMcode(mainproc,0),NULL);
	exe->argc=0;
	exe->argv=NULL;
	exe->curproc=exe->mainproc;
	exe->modules=NULL;
	exe->procs=procs;
	exe->mark=1;
	exe->chan=NULL;
	exe->stack=newStack(0);
	exe->heap=newVMheap();
	exe->callback=NULL;
	FakeVM** ppFakeVM=NULL;
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
		GlobFakeVMs.VMs=(FakeVM**)realloc(GlobFakeVMs.VMs,sizeof(FakeVM*)*(size+1));
		if(size!=0&&GlobFakeVMs.VMs==NULL)errors("newFakeVM",__FILE__,__LINE__);
		GlobFakeVMs.VMs[size]=exe;
		GlobFakeVMs.size+=1;
		exe->VMid=size;
	}
	return exe;
}

FakeVM* newTmpFakeVM(ByteCode* mainproc,ByteCode* procs)
{
	FakeVM* exe=(FakeVM*)malloc(sizeof(FakeVM));
	if(exe==NULL)errors("newTmpFakeVM",__FILE__,__LINE__);
	if(mainproc!=NULL)
		exe->mainproc=newFakeProcess(newVMcode(mainproc,0),NULL);
	exe->curproc=exe->mainproc;
	exe->modules=NULL;
	exe->procs=procs;
	exe->mark=1;
	exe->argc=0;
	exe->argv=NULL;
	exe->stack=newStack(0);
	exe->heap=newVMheap();
	exe->callback=NULL;
	exe->VMid=-1;
	return exe;
}

void initGlobEnv(VMenv* obj,VMheap* heap,SymbolTable* table)
{
	ByteCode buildInProcs[]=
	{
		P_cons,
		P_car,
		P_cdr,
		P_atom,
		P_null,
		P_type,
		P_aply,
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
		P_rem,
		P_nth,
		P_length,
		P_appd,
		P_open,
		P_read,
		P_getb,
		P_write,
		P_putb,
		P_princ,
		P_go,
		P_chanl,
		P_send,
		P_recv,
		P_clcc
	};
	obj->size=NUMOFBUILTINSYMBOL;
	obj->list=(VMenvNode**)malloc(sizeof(VMenvNode*)*NUMOFBUILTINSYMBOL);
	if(obj->list==NULL)errors("initGlobEnv",__FILE__,__LINE__);
	int32_t tmpInt=EOF;
	obj->list[0]=newVMenvNode(newNilValue(heap),findSymbol(builtInSymbolList[0],table)->id);
	obj->list[1]=newVMenvNode(newVMvalue(IN32,&tmpInt,heap,1),findSymbol(builtInSymbolList[1],table)->id);
	obj->list[2]=newVMenvNode(newVMvalue(FP,newVMfp(stdin),heap,1),findSymbol(builtInSymbolList[2],table)->id);
	obj->list[3]=newVMenvNode(newVMvalue(FP,newVMfp(stdout),heap,1),findSymbol(builtInSymbolList[3],table)->id);
	obj->list[4]=newVMenvNode(newVMvalue(FP,newVMfp(stderr),heap,1),findSymbol(builtInSymbolList[4],table)->id);
	int i=5;
	for(;i<NUMOFBUILTINSYMBOL;i++)
	{
		ByteCode* tmp=copyByteCode(buildInProcs+(i-5));
		obj->list[i]=newVMenvNode(newVMvalue(PRC,newBuiltInProc(tmp),heap,1),findSymbol(builtInSymbolList[i],table)->id);
		free(tmp);
	}
	sortVMenvList(obj);
}

void* ThreadVMFunc(void* p)
{
	FakeVM* exe=(FakeVM*)p;
	int64_t status=runFakeVM(exe);
	Chanl* tmpCh=exe->chan;
	exe->chan=NULL;
	if(!status)
	{
		pthread_mutex_lock(&tmpCh->lock);
		if(tmpCh->tail==NULL)
		{
			tmpCh->head=newThreadMessage(getTopValue(exe->stack),exe->heap);
			tmpCh->tail=tmpCh->head;
		}
		pthread_mutex_unlock(&tmpCh->lock);
	}
	freeChanl(tmpCh);
	freeVMstack(exe->stack);
	exe->stack=NULL;
	exe->lnt=NULL;
	exe->table=NULL;
	if(status!=0)
		deleteCallChain(exe);
	exe->mark=0;
	return (void*)status;
}

int runFakeVM(FakeVM* exe)
{
	while(exe->curproc!=NULL)
	{
		pthread_rwlock_rdlock(&GClock);
		if(exe->curproc->cp>=exe->curproc->code->size)
		{
			VMprocess* tmpProc=exe->curproc;
			VMprocess* prev=exe->curproc->prev;
			VMcode* tmpCode=tmpProc->code;
			VMenv* tmpEnv=tmpProc->localenv;
			exe->curproc=prev;
			free(tmpProc);
			freeVMenv(tmpEnv);
			freeVMcode(tmpCode);
			continue;
		}

		VMprocess* curproc=exe->curproc;
		VMcode* tmpCode=curproc->code;
		int32_t cp=curproc->cp;
		int status=ByteCodes[(int)tmpCode->code[cp]](exe);
		if(status!=0)
		{
			int32_t sid;
			int32_t cp;
			int32_t pid;
			VMprocess* cur=curproc;
			if(status==TOOMANYARG||status==TOOFEWARG)
				cur=cur->prev;
			if(cur->code->id!=-1)
			{
				cp=cur->cp;
				pid=cur->code->id;
			}
			else
			{
				VMprocess* cur=curproc;
				while(cur->code->id==-1)
					cur=cur->prev;
				cp=cur->cp;
				pid=cur->code->id;
			}
			LineNumTabNode* node=findLineNumTabNode(pid,cp,exe->lnt);
			fprintf(stderr,"In file \"%s\",line %d\n",exe->table->idl[node->fid]->symbol,node->line);
			switch(status)
			{
				case WRONGARG:
					fprintf(stderr,"error:Wrong arguements.\n");
					break;
				case STACKERROR:
					fprintf(stderr,"error:Stack error.\n");
					break;
				case TOOMANYARG:
					fprintf(stderr,"error:Too many arguements.\n");
					break;
				case TOOFEWARG:
					fprintf(stderr,"error:Too few arguements.\n");
					break;
				case CANTCREATETHREAD:
					fprintf(stderr,"error:Can't create thread.n");
					break;
				case THREADERROR:
					fprintf(stderr,"error:Thread error.\n");
					break;
				case SYMUNDEFINE:
					sid=getSymbolIdInByteCode(tmpCode->code+curproc->cp);
					fprintf(stderr,"%s",exe->table->idl[sid]->symbol);
					fprintf(stderr,":Symbol is undefined.\n");
					break;
				case INVOKEERROR:
					fprintf(stderr,"error:Try to invoke \"");
					fprintValue(exe->stack->values[exe->stack->tp-1],stderr);
					fprintf(stderr,"\"\n");
					break;
			}
			if(exe->VMid==-1)
				return 1;
			else if(exe->VMid!=0)
				return status;
			int i[2]={status,1};
			exe->callback(i);
		}
		pthread_rwlock_unlock(&GClock);
		//if(exe->heap->size>exe->heap->threshold)
		{
			if(pthread_rwlock_trywrlock(&GClock))continue;
			int i=0;
			if(exe->VMid!=-1)
			{
				for(;i<GlobFakeVMs.size;i++)
				{
					if(GlobFakeVMs.VMs[i])
					{
						if(GlobFakeVMs.VMs[i]->mark)
							GC_mark(GlobFakeVMs.VMs[i]);
						else
						{
							pthread_join(GlobFakeVMs.VMs[i]->tid,NULL);
							free(GlobFakeVMs.VMs[i]);
							GlobFakeVMs.VMs[i]=NULL;
						}
					}
				}
			}
			else GC_mark(exe);
			GC_sweep(exe->heap);
			pthread_mutex_lock(&exe->heap->lock);
			exe->heap->threshold=exe->heap->size+THRESHOLD_SIZE;
			pthread_mutex_unlock(&exe->heap->lock);
			pthread_rwlock_unlock(&GClock);
		}
	}
	return 0;
}

int B_dummy(FakeVM* exe)
{
	VMprocess* curproc=exe->curproc;
	VMcode* tmpCode=curproc->code;
	ByteCode tmpByteCode={tmpCode->size,tmpCode->code};
	VMstack* stack=exe->stack;
	printByteCode(&tmpByteCode,stderr);
	putc('\n',stderr);
	printAllStack(exe->stack,stderr,1);
	putc('\n',stderr);
	fprintf(stderr,"stack->tp==%d,stack->size==%d\n",stack->tp,stack->size);
	fprintf(stderr,"cp=%d stack->bp=%d\n%s\n",curproc->cp,stack->bp,codeName[(int)tmpCode->code[curproc->cp]].codeName);
	printEnv(exe->curproc->localenv,stderr);
	putc('\n',stderr);
	fprintf(stderr,"Wrong byts code!\n");
	exit(EXIT_FAILURE);
	return WRONGARG;
}

int B_push_nil(FakeVM* exe)
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

int B_push_pair(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	if(stack->tp>=stack->size)
	{
		stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size+64));
		if(stack->values==NULL)errors("B_push_pair",__FILE__,__LINE__);
		stack->size+=64;
	}
	VMvalue* objValue=newVMvalue(PAIR,newVMpair(exe->heap),exe->heap,1);
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=1;
	return 0;

}

int B_push_int(FakeVM* exe)
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
	VMvalue* objValue=newVMvalue(IN32,tmpCode->code+proc->cp+1,exe->heap,1);
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=5;
	return 0;
}

int B_push_chr(FakeVM* exe)
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
	VMvalue* objValue=newVMvalue(CHR,tmpCode->code+proc->cp+1,exe->heap,1);
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=2;
	return 0;
}

int B_push_dbl(FakeVM* exe)
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
	VMvalue* objValue=newVMvalue(DBL,tmpCode->code+proc->cp+1,exe->heap,1);
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=9;
	return 0;
}

int B_push_str(FakeVM* exe)
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
	VMstr* tmpStr=newVMstr(tmpCode->code+proc->cp+1);
	VMvalue* objValue=newVMvalue(STR,tmpStr,exe->heap,1);
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=2+len;
	return 0;
}

int B_push_sym(FakeVM* exe)
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
	VMstr* tmpStr=newVMstr(tmpCode->code+proc->cp+1);
	VMvalue* objValue=newVMvalue(SYM,tmpStr,exe->heap,1);
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=2+len;
	return 0;
}

int B_push_byte(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMcode* tmpCode=proc->code;
	int32_t size=*(int32_t*)(tmpCode->code+proc->cp+1);
	uint8_t* tmp=createByteString(size);
	memcpy(tmp,tmpCode->code+proc->cp+5,size);
	if(stack->tp>=stack->size)
	{
		stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size+64));
		if(stack->values==NULL)errors("B_push_byte",__FILE__,__LINE__);
		stack->size+=64;
	}
	ByteString* tmpArry=newByteString(size,NULL);
	tmpArry->str=tmp;
	VMvalue* objValue=newVMvalue(BYTS,tmpArry,exe->heap,1);
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=5+size;
	return 0;
}

int B_push_var(FakeVM* exe)
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
	int32_t idOfVar=*(int32_t*)(tmpCode->code+proc->cp+1);
	VMenv* curEnv=proc->localenv;
	VMvalue* tmpValue=NULL;
	VMenvNode* tmp=NULL;
	while(curEnv&&!tmp)
	{
		tmp=findVMenvNode(idOfVar,curEnv);
		curEnv=curEnv->prev;
	}
	if(tmp==NULL)
		return SYMUNDEFINE;
	tmpValue=tmp->value;
	stack->values[stack->tp]=tmpValue;
	stack->tp+=1;
	proc->cp+=5;
	return 0;
}

int B_push_car(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* objValue=getTopValue(stack);
	if(objValue->type!=PAIR&&objValue->type!=STR&&objValue->type!=BYTS)return WRONGARG;
	if(objValue->type==PAIR)
	{
		stack->values[stack->tp-1]=getCar(objValue);
	}
	else if(objValue->type==STR)
	{
		VMvalue* ch=newVMvalue(CHR,&objValue->u.str[0],exe->heap,0);
		stack->values[stack->tp-1]=ch;
	}
	else if(objValue->type==BYTS)
	{
		ByteString* tmp=newByteString(1,NULL);
		tmp->str=objValue->u.byts->str;
		VMvalue* bt=newVMvalue(BYTS,tmp,exe->heap,0);
		stack->values[stack->tp-1]=bt;
	}
	proc->cp+=1;
	return 0;
}

int B_push_cdr(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* objValue=getTopValue(stack);
	if(objValue->type!=PAIR&&objValue->type!=STR&&objValue->type!=BYTS)return WRONGARG;
	if(objValue->type==PAIR)
	{
		stack->values[stack->tp-1]=getCdr(objValue);
	}
	else if(objValue->type==STR)
	{
		char* str=objValue->u.str->str;
		if(strlen(str)==0)stack->values[stack->tp-1]=newNilValue(exe->heap);
		else
		{
			VMstr* tmpVMstr=newVMstr(NULL);
			tmpVMstr->str=objValue->u.str->str+1;
			VMvalue* tmpStr=newVMvalue(STR,tmpVMstr,exe->heap,0);
			stack->values[stack->tp-1]=tmpStr;
		}
	}
	else if(objValue->type==BYTS)
	{
		if(objValue->u.byts->size==1)stack->values[stack->tp-1]=newNilValue(exe->heap);
		else
		{
			ByteString* tmp=newByteString(objValue->u.byts->size-1,NULL);
			tmp->str=objValue->u.byts->str+1;
			VMvalue* bt=newVMvalue(BYTS,tmp,exe->heap,0);
			stack->values[stack->tp-1]=bt;
		}
	}
	proc->cp+=1;
	return 0;
}

int B_push_top(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	if(stack->tp==stack->bp)return WRONGARG;
	if(stack->tp>=stack->size)
	{
		stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size+64));
		if(stack->values==NULL)errors("B_push_top",__FILE__,__LINE__);
		stack->size+=64;
	}
	stack->values[stack->tp]=getTopValue(stack);
	stack->tp+=1;
	proc->cp+=1;
	return 0;
}

int B_push_proc(FakeVM* exe)
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
	VMcode* code=newVMcode(exe->procs+countOfProc,countOfProc+1);
	code->localenv=newVMenv(proc->localenv);
	VMvalue* objValue=newVMvalue(PRC,code,exe->heap,1);
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=5;
	return 0;
}

int B_push_mod_proc(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMcode* tmpCode=proc->code;
	char* funcname=tmpCode->code+proc->cp+1;
	if(stack->tp>=stack->size)
	{
		stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size+64));
		if(stack->values==NULL)errors("B_push_proc",__FILE__,__LINE__);
		stack->size+=64;
	}
	ByteCode* dllFunc=newDllFuncProc(funcname);
	VMcode* tmp=newVMcode(dllFunc,-1);
	tmp->localenv=newVMenv(NULL);
	VMvalue* tmpVMvalue=newVMvalue(PRC,tmp,exe->heap,1);
	freeByteCode(dllFunc);
	stack->values[stack->tp]=tmpVMvalue;
	stack->tp+=1;
	proc->cp+=2+strlen(funcname);
	return 0;
}

int B_push_list_arg(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
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

int B_pop_var(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMcode* tmpCode=proc->code;
	if(!(stack->tp>stack->bp))return TOOFEWARG;
	int32_t scopeOfVar=*(int32_t*)(tmpCode->code+proc->cp+1);
	int32_t idOfVar=*(int32_t*)(tmpCode->code+proc->cp+5);
	VMenv* curEnv=proc->localenv;
	VMvalue** pValue=NULL;
	if(scopeOfVar>=0)
	{
		int32_t i=0;
		for(;i<scopeOfVar;i++)
			curEnv=curEnv->prev;
		VMenvNode* tmp=findVMenvNode(idOfVar,curEnv);
		if(!tmp)
			tmp=addVMenvNode(newVMenvNode(NULL,idOfVar),curEnv);
		pValue=&tmp->value;
	}
	else
	{
		VMenvNode* tmp=NULL;
		while(curEnv&&!tmp)
		{
			tmp=findVMenvNode(idOfVar,curEnv);
			curEnv=curEnv->prev;
		}
		if(tmp==NULL)
			return SYMUNDEFINE;
		pValue=&tmp->value;
	}
	VMvalue* topValue=getTopValue(stack);
	*pValue=newNilValue(exe->heap);
	(*pValue)->access=1;
	copyRef(*pValue,topValue);
	stack->tp-=1;
	stackRecycle(exe);
	proc->cp+=9;
	return 0;
}

int B_pop_rest_var(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMcode* tmpCode=proc->code;
	int32_t scopeOfVar=*(int32_t*)(tmpCode->code+proc->cp+1);
	int32_t idOfVar=*(int32_t*)(tmpCode->code+proc->cp+5);
	VMenv* curEnv=proc->localenv;
	VMvalue** pValue=NULL;
	if(scopeOfVar>=0)
	{
		int32_t i=0;
		for(;i<scopeOfVar;i++)
			curEnv=curEnv->prev;
		VMenvNode* tmp=findVMenvNode(idOfVar,curEnv);
		if(!tmp)
			tmp=addVMenvNode(newVMenvNode(NULL,idOfVar),curEnv);
		pValue=&tmp->value;
	}
	else
	{
		VMenvNode* tmp=NULL;
		while(curEnv&&!tmp)
		{
			tmp=findVMenvNode(idOfVar,curEnv);
			curEnv=curEnv->prev;
		}
		if(tmp==NULL)
			return SYMUNDEFINE;
		pValue=&tmp->value;
	}
	VMvalue* obj=NULL;
	VMvalue* tmp=NULL;
	if(stack->tp>stack->bp)
	{
		obj=newVMvalue(PAIR,newVMpair(exe->heap),exe->heap,1);
		tmp=obj;
	}
	else obj=newNilValue(exe->heap);
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
	*pValue=obj;
	proc->cp+=9;
	return 0;
}

int B_pop_car(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	VMvalue* objValue=getValue(stack,stack->tp-2);
	if(objValue->type!=PAIR)return WRONGARG;
	objValue->u.pair->car=topValue;
	stack->tp-=1;
	stackRecycle(exe);
	proc->cp+=1;
	return 0;
}

int B_pop_cdr(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	VMvalue* objValue=getValue(stack,stack->tp-2);
	if(objValue->type!=PAIR)return WRONGARG;
	objValue->u.pair->cdr=topValue;
	stack->tp-=1;
	stackRecycle(exe);
	proc->cp+=1;
	return 0;
}

int B_pop_ref(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	VMvalue* objValue=getValue(stack,stack->tp-2);
	if(objValue->access==0&&objValue->type!=topValue->type)return WRONGARG;
	if(objValue->type==topValue->type)
		writeRef(objValue,topValue);
	else
		copyRef(objValue,topValue);
	stack->tp-=1;
	stackRecycle(exe);
	proc->cp+=1;
	return 0;
}

int B_pack_cc(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	if(stack->tp>=stack->size)
	{
		stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size+64));
		if(stack->values==NULL)errors("B_push_int",__FILE__,__LINE__);
		stack->size+=64;
	}
	VMcontinuation* cc=newVMcontinuation(stack,proc);
	VMvalue* retval=newVMvalue(CONT,cc,exe->heap,1);
	stack->values[stack->tp]=retval;
	stack->tp+=1;
	proc->cp+=1;
	return 0;
}

int B_add(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* firValue=getValue(stack,stack->tp-1);
	VMvalue* secValue=getValue(stack,stack->tp-2);
	if((firValue->type!=IN32&&firValue->type!=DBL)||(secValue->type!=IN32&&secValue->type!=DBL))return WRONGARG;
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
	proc->cp+=1;
	return 0;
}

int B_sub(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* firValue=getValue(stack,stack->tp-1);
	VMvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(exe);
	if((firValue->type!=IN32&&firValue->type!=DBL)||(secValue->type!=IN32&&secValue->type!=DBL))return WRONGARG;
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

int B_mul(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* firValue=getValue(stack,stack->tp-1);
	VMvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(exe);
	if((firValue->type!=IN32&&firValue->type!=DBL)||(secValue->type!=IN32&&secValue->type!=DBL))return WRONGARG;
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

int B_div(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* firValue=getValue(stack,stack->tp-1);
	VMvalue* secValue=getValue(stack,stack->tp-2);
	VMvalue* tmpValue=NULL;
	stack->tp-=1;
	stackRecycle(exe);
	if((firValue->type!=IN32&&firValue->type!=DBL)||(secValue->type!=IN32&&secValue->type!=DBL))return WRONGARG;
	if((firValue->type==DBL&&fabs(*firValue->u.dbl)==0)||(firValue->type==IN32&&*firValue->u.num==0))
		tmpValue=newNilValue(exe->heap);
	else
	{
		double result=((secValue->type==DBL)?*secValue->u.dbl:*secValue->u.num)/((firValue->type==DBL)?*firValue->u.dbl:*firValue->u.num);
		tmpValue=newVMvalue(DBL,&result,exe->heap,1);
	}
	stack->values[stack->tp-1]=tmpValue;
	proc->cp+=1;
	return 0;
}

int B_rem(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* firValue=getValue(stack,stack->tp-1);
	VMvalue* secValue=getValue(stack,stack->tp-2);
	VMvalue* tmpValue=NULL;
	stack->tp-=1;
	stackRecycle(exe);
	if((firValue->type!=IN32&&firValue->type!=DBL)||(secValue->type!=IN32&&secValue->type!=DBL))return WRONGARG;
	if(!(*firValue->u.num))
		tmpValue=newNilValue(exe->heap);
	else
	{
		int32_t result=((int32_t)((secValue->type==DBL)?*secValue->u.dbl:*secValue->u.num))%((int32_t)((firValue->type==DBL)?*firValue->u.dbl:*firValue->u.num));
		tmpValue=newVMvalue(IN32,&result,exe->heap,1);
	}
	stack->values[stack->tp-1]=tmpValue;
	proc->cp+=1;
	return 0;
}

int B_atom(FakeVM* exe)
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

int B_null(FakeVM* exe)
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

int B_type(FakeVM* exe)
{
	char* a[]=
	{
		"nil",
		"int",
		"chr",
		"dbl",
		"sym",
		"str",
		"byts",
		"proc",
		"cont",
		"chan",
		"fp",
		"pair"
	};
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	int32_t type=topValue->type;
	VMstr* str=newVMstr(a[type]);
	stack->values[stack->tp-1]=newVMvalue(SYM,str,exe->heap,1);
	proc->cp+=1;
	return 0;
}

int B_call_proc(FakeVM* exe)
{
	VMprocess* proc=exe->curproc;
	VMcode* tmpCode=proc->code;
	char* funcname=tmpCode->code+proc->cp+1;
	char* headOfFunc="FAKE_";
	char* realfuncName=(char*)malloc(sizeof(char)*(strlen(headOfFunc)+strlen(funcname)+1));
	if(realfuncName==NULL)errors("B_call_proc",__FILE__,__LINE__);
	strcpy(realfuncName,headOfFunc);
	strcat(realfuncName,funcname);
	void* funcAddress=getAddress(realfuncName,exe->modules);
	free(realfuncName);
	if(!funcAddress)
		return STACKERROR;
	int retval=((ModFunc)funcAddress)(exe,&GClock);
	proc->cp+=2+strlen(funcname);
	return retval;
}

int B_set_tp(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	if(stack->tptp>=stack->tpsi)
	{
		stack->tpst=(int32_t*)realloc(stack->tpst,sizeof(int32_t)*(stack->tpsi+16));
		if(stack->tpst==NULL)errors("B_set_tp",__FILE__,__LINE__);
		stack->tpsi+=16;
	}
	stack->tpst[stack->tptp]=stack->tp;
	stack->tptp+=1;
	proc->cp+=1;
	return 0;
}

int B_set_bp(FakeVM* exe)
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

int B_res_tp(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	stack->tp=(stack->tptp)?stack->tpst[stack->tptp-1]:0;
	proc->cp+=1;
	return 0;
}

int B_pop_tp(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	stack->tptp-=1;
	if(stack->tpsi-stack->tptp>16)
	{
		stack->tpst=(int32_t*)realloc(stack->tpst,sizeof(int32_t)*(stack->tpsi-16));
		if(stack->tpst==NULL)
			errors("B_pop_tp",__FILE__,__LINE__);
		stack->tpsi-=16;
	}
	proc->cp+=1;
	return 0;
}

int B_res_bp(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	if(stack->tp>stack->bp)return TOOMANYARG;
	VMvalue* prevBp=getTopValue(stack);
	stack->bp=*prevBp->u.num;
	stack->tp-=1;
	stackRecycle(exe);
	proc->cp+=1;
	return 0;
}

int B_invoke(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* tmpValue=getTopValue(stack);
	if(tmpValue->type!=PRC&&tmpValue->type!=CONT)return INVOKEERROR;
	if(tmpValue->type==PRC)
	{
		VMcode* tmpCode=tmpValue->u.prc;
		VMprocess* prevProc=hasSameProc(tmpCode,proc);
		if(isTheLastExpress(proc,prevProc)&&prevProc)
			prevProc->cp=0;
		else
		{
			tmpCode->refcount+=1;
			VMprocess* tmpProc=newFakeProcess(tmpCode,proc);
			tmpProc->localenv=newVMenv(tmpCode->localenv->prev);
			exe->curproc=tmpProc;
		}
		stack->tp-=1;
		stackRecycle(exe);
		proc->cp+=1;
	}
	else
	{
		stack->tp-=1;
		stackRecycle(exe);
		proc->cp+=1;
		VMcontinuation* cc=tmpValue->u.cont;
		createCallChainWithContinuation(exe,cc);
	}
	return 0;
}

int B_jump_if_ture(FakeVM* exe)
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
	proc->cp+=5;
	return 0;
}

int B_jump_if_false(FakeVM* exe)
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

int B_jump(FakeVM* exe)
{
	VMprocess* proc=exe->curproc;
	VMcode* tmpCode=proc->code;
	int32_t where=*(int32_t*)(tmpCode->code+proc->cp+sizeof(char));
	proc->cp+=where+5;
	return 0;
}

int B_open(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMheap* heap=exe->heap;
	VMvalue* mode=getTopValue(stack);
	VMvalue* filename=getValue(stack,stack->tp-2);
	if(filename->type!=STR||mode->type!=STR)return WRONGARG;
	stack->tp-=1;
	stackRecycle(exe);
	FILE* file=fopen(filename->u.str->str,mode->u.str->str);
	if(!file)
		stack->values[stack->tp-1]=newNilValue(heap);
	else
		stack->values[stack->tp-1]=newVMvalue(FP,newVMfp(file),heap,1);
	proc->cp+=1;
	return 0;
}

int B_eq(FakeVM* exe)
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

int B_eqn(FakeVM* exe)
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
	else return WRONGARG;
	proc->cp+=1;
	return 0;
}

int B_equal(FakeVM* exe)
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

int B_gt(FakeVM* exe)
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
	else return WRONGARG;
	proc->cp+=1;
	return 0;
}

int B_ge(FakeVM* exe)
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
	else return WRONGARG;
	proc->cp+=1;
	return 0;
}

int B_lt(FakeVM* exe)
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
	else return WRONGARG;
	proc->cp+=1;
	return 0;
}

int B_le(FakeVM* exe)
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
	else return WRONGARG;
	proc->cp+=1;
	return 0;
}

int B_not(FakeVM* exe)
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

int B_cast_to_chr(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	if(topValue->type==PAIR||topValue->type==PRC)return WRONGARG;
	VMvalue* tmpValue=newVMvalue(CHR,NULL,exe->heap,1);
	switch(topValue->type)
	{
		case IN32:
			*tmpValue->u.chr=*topValue->u.num;
			break;
		case DBL:
			*tmpValue->u.chr=(int32_t)*topValue->u.dbl;
			break;
		case CHR:
			*tmpValue->u.chr=*topValue->u.chr;
			break;
		case STR:
		case SYM:
			*tmpValue->u.chr=topValue->u.str->str[0];
			break;
		case BYTS:
			*tmpValue->u.chr=*(char*)topValue->u.byts->str;
			break;
	}
	stack->values[stack->tp-1]=tmpValue;
	proc->cp+=1;
	return 0;
}

int B_cast_to_int(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	if(topValue->type==PAIR||topValue->type==PRC)return WRONGARG;
	VMvalue* tmpValue=newVMvalue(IN32,NULL,exe->heap,1);
	int32_t m=0;
	switch(topValue->type)
	{
		case IN32:
			*tmpValue->u.num=*topValue->u.num;
			break;
		case DBL:
			*tmpValue->u.num=(int32_t)*topValue->u.dbl;
			break;
		case CHR:
			*tmpValue->u.num=*topValue->u.chr;
			break;
		case STR:
		case SYM:
			*tmpValue->u.num=stringToInt(topValue->u.str->str);
			break;
		case BYTS:
			memcpy(&m,topValue->u.byts->str,topValue->u.byts->size);
			*tmpValue->u.num=m;
			break;
	}
	stack->values[stack->tp-1]=tmpValue;
	proc->cp+=1;
	return 0;
}

int B_cast_to_dbl(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	if(topValue->type==PAIR||topValue->type==PRC)return WRONGARG;
	VMvalue* tmpValue=newVMvalue(DBL,NULL,exe->heap,1);
	double d=0;
	switch(topValue->type)
	{
		case IN32:
			*tmpValue->u.dbl=(double)*topValue->u.num;
			break;
		case DBL:
			*tmpValue->u.dbl=*topValue->u.dbl;
			break;
		case CHR:
			*tmpValue->u.dbl=(double)(int32_t)*topValue->u.chr;
			break;
		case STR:
		case SYM:
			*tmpValue->u.dbl=stringToDouble(topValue->u.str->str);
			break;
		case BYTS:
			memcpy(&d,topValue->u.byts->str,topValue->u.byts->size);
			*tmpValue->u.dbl=d;
			break;
	}
	stack->values[stack->tp-1]=tmpValue;
	proc->cp+=1;
	return 0;
}

int B_cast_to_str(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	if(topValue->type==PAIR||topValue->type==PRC)return WRONGARG;
	VMstr* tmpStr=newVMstr(NULL);
	VMvalue* tmpValue=newVMvalue(STR,tmpStr,exe->heap,1);
	switch(topValue->type)
	{
		case IN32:
			tmpValue->u.str->str=intToString(*topValue->u.num);
			break;
		case DBL:
			tmpValue->u.str->str=doubleToString(*topValue->u.dbl);
			break;
		case CHR:
			tmpValue->u.str->str=(char*)malloc(sizeof(char)*2);
			if(tmpValue->u.str->str==NULL)errors("B_cast_to_str",__FILE__,__LINE__);
			tmpValue->u.str->str[0]=*topValue->u.chr;
			tmpValue->u.str->str[1]='\0';
			break;
		case STR:
		case SYM:
			tmpValue->u.str->str=copyStr(topValue->u.str->str);
			break;
		case BYTS:
			tmpValue->u.str->str=copyStr((char*)topValue->u.byts->str);
			break;
	}
	stack->values[stack->tp-1]=tmpValue;
	proc->cp+=1;
	return 0;
}

int B_cast_to_sym(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	if(topValue->type==PAIR||topValue->type==PRC)return WRONGARG;
	VMstr* tmpStr=newVMstr(NULL);
	VMvalue* tmpValue=newVMvalue(SYM,tmpStr,exe->heap,1);
	switch(topValue->type)
	{
		case IN32:
			tmpValue->u.str->str=intToString((long)*topValue->u.num);
			break;
		case DBL:
			tmpValue->u.str->str=doubleToString(*topValue->u.dbl);
			break;
		case CHR:
			tmpValue->u.str->str=(char*)malloc(sizeof(char)*2);
			if(tmpValue->u.str->str==NULL)errors("B_cast_to_sym",__FILE__,__LINE__);
			tmpValue->u.str->str[0]=*topValue->u.chr;
			tmpValue->u.str->str[1]='\0';
			break;
		case STR:
		case SYM:
			tmpValue->u.str->str=copyStr(topValue->u.str->str);
			break;
		case BYTS:
			tmpValue->u.str->str=copyStr((char*)topValue->u.byts->str);
			break;
	}
	stack->values[stack->tp-1]=tmpValue;
	proc->cp+=1;
	return 0;
}

int B_cast_to_byte(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	if(topValue->type==PAIR||topValue->type==PRC)return WRONGARG;
	VMvalue* tmpValue=newVMvalue(BYTS,NULL,exe->heap,1);
	tmpValue->u.byts=newEmptyByteArry();
	switch(topValue->type)
	{
		case IN32:
			tmpValue->u.byts->size=sizeof(int32_t);
			tmpValue->u.byts->str=createByteString(sizeof(int32_t));
			*(int32_t*)tmpValue->u.byts->str=*topValue->u.num;
			break;
		case DBL:
			tmpValue->u.byts->size=sizeof(double);
			tmpValue->u.byts->str=createByteString(sizeof(double));
			*(double*)tmpValue->u.byts->str=*topValue->u.dbl;
			break;
		case CHR:
			tmpValue->u.byts->size=sizeof(char);
			tmpValue->u.byts->str=createByteString(sizeof(char));
			*(char*)tmpValue->u.byts->str=*topValue->u.chr;
			break;
		case STR:
		case SYM:
			tmpValue->u.byts->size=strlen(topValue->u.str->str+1);
			tmpValue->u.byts->str=(uint8_t*)copyStr(topValue->u.str->str);
			break;
		case BYTS:
			tmpValue->u.byts->size=topValue->u.byts->size;
			tmpValue->u.byts->str=createByteString(topValue->u.byts->size);
			memcpy(tmpValue->u.byts->str,topValue->u.byts->str,topValue->u.byts->size);
			break;
	}
	stack->values[stack->tp-1]=tmpValue;
	proc->cp+=1;
	return 0;
}

int B_nth(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* place=getValue(stack,stack->tp-2);
	VMvalue* objlist=getTopValue(stack);
	if((objlist->type!=PAIR&&objlist->type!=STR&&objlist->type!=BYTS)||place->type!=IN32)return WRONGARG;
	if(objlist->type==PAIR)
	{
		VMvalue* obj=getCar(objlist);
		VMvalue* objPair=getCdr(objlist);
		int i=0;
		for(;i<*place->u.num;i++)
		{
			if(objPair->type!=PAIR)
			{
				obj=newNilValue(exe->heap);
				break;
			}
			obj=getCar(objPair);
			objPair=getCdr(objPair);
		}
		stack->tp-=1;
		stack->values[stack->tp-1]=obj;
		stackRecycle(exe);
	}
	else if(objlist->type==STR)
	{
		stack->tp-=1;
		stackRecycle(exe);
		VMvalue* objChr=newVMvalue(CHR,objlist->u.str->str+*place->u.num,exe->heap,0);
		stack->values[stack->tp-1]=objChr;
	}
	else if(objlist->type==BYTS)
	{
		stack->tp-=1;
		stackRecycle(exe);
		ByteString* tmpByte=newByteString(1,NULL);
		tmpByte->str=objlist->u.byts->str+*place->u.num;
		VMvalue* objByte=newVMvalue(BYTS,tmpByte,exe->heap,0);
		stack->values[stack->tp-1]=objByte;
	}
	proc->cp+=1;
	return 0;
}

int B_length(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* objlist=getTopValue(stack);
	if(objlist->type!=PAIR&&objlist->type!=STR&&objlist->type!=BYTS&&objlist->type!=CHAN)return WRONGARG;
	if(objlist->type==PAIR)
	{
		int32_t i=0;
		for(VMvalue* tmp=objlist;tmp!=NULL&&tmp->type==PAIR;tmp=getCdr(tmp))
			if(!(getCar(tmp)->type==NIL&&getCdr(tmp)->type==NIL))
				i++;
		stack->values[stack->tp-1]=newVMvalue(IN32,&i,exe->heap,1);
	}
	else if(objlist->type==STR)
	{
		int32_t len=strlen(objlist->u.str->str);
		stack->values[stack->tp-1]=newVMvalue(IN32,&len,exe->heap,1);
	}
	else if(objlist->type==BYTS)
		stack->values[stack->tp-1]=newVMvalue(IN32,&objlist->u.byts->size,exe->heap,1);
	else if(objlist->type==CHAN)
		stack->values[stack->tp-1]=newVMvalue(IN32,&objlist->u.chan->size,exe->heap,1);
	proc->cp+=1;
	return 0;
}

int B_appd(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* fir=getTopValue(stack);
	VMvalue* sec=getValue(stack,stack->tp-2);
	if(sec->type!=NIL&&sec->type!=PAIR&&sec->type!=STR&&sec->type!=BYTS)return WRONGARG;
	if(sec->type==PAIR)
	{
		VMvalue* copyOfsec=copyVMvalue(sec,exe->heap);
		VMvalue* lastpair=copyOfsec;
		while(getCdr(lastpair)->type!=NIL)lastpair=getCdr(lastpair);
		lastpair->u.pair->cdr=fir;
		stack->tp-=1;
		stackRecycle(exe);
		stack->values[stack->tp-1]=copyOfsec;
	}
	else if(sec->type==STR)
	{
		if(fir->type!=STR)return WRONGARG;
		int32_t firlen=strlen(fir->u.str->str);
		int32_t seclen=strlen(sec->u.str->str);
		char* tmpStr=(char*)malloc(sizeof(char)*(firlen+seclen+1));
		if(!tmpStr)errors("B_appd",__FILE__,__LINE__);
		strcpy(tmpStr,sec->u.str->str);
		strcat(tmpStr,fir->u.str->str);
		VMstr* tmpVMstr=newVMstr(NULL);
		tmpVMstr->str=tmpStr;
		VMvalue* tmpValue=newVMvalue(STR,tmpVMstr,exe->heap,1);
		stack->tp-=1;
		stackRecycle(exe);
		stack->values[stack->tp-1]=tmpValue;
	}
	else if(sec->type==BYTS)
	{
		int32_t firSize=fir->u.byts->size;
		int32_t secSize=sec->u.byts->size;
		VMvalue* tmpByte=newVMvalue(BYTS,NULL,exe->heap,1);
		tmpByte->u.byts=newEmptyByteArry();
		tmpByte->u.byts->size=firSize+secSize;
		uint8_t* tmpArry=createByteString(firSize+secSize);
		memcpy(tmpArry,sec->u.byts->str,secSize);
		memcpy(tmpArry+secSize,fir->u.byts->str,firSize);
		tmpByte->u.byts->str=tmpArry;
		stack->tp-=1;
		stackRecycle(exe);
		stack->values[stack->tp-1]=tmpByte;
	}
	else
	{
		stack->tp-=1;
		stackRecycle(exe);
		stack->values[stack->tp-1]=fir;
	}
	proc->cp+=1;
	return 0;
}

int B_read(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* file=getTopValue(stack);
	if(file->type!=FP)return WRONGARG;
	FILE* tmpFile=file->u.fp->fp;
	char* tmpString=baseReadSingle(tmpFile);
	Intpr* tmpIntpr=newTmpIntpr(NULL,tmpFile);
	AST_cptr* tmpCptr=baseCreateTree(tmpString,tmpIntpr);
	VMvalue* tmp=NULL;
	if(tmpCptr==NULL)
		tmp=newNilValue(exe->heap);
	else
		tmp=castCptrVMvalue(tmpCptr,exe->heap);
	stack->values[stack->tp-1]=tmp;
	free(tmpIntpr);
	free(tmpString);
	deleteCptr(tmpCptr);
	free(tmpCptr);
	proc->cp+=1;
	return 0;
}

int B_getb(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* file=getTopValue(stack);
	VMvalue* size=getValue(stack,stack->tp-2);
	if(file->type!=FP||size->type!=IN32)return WRONGARG;
	FILE* fp=file->u.fp->fp;
	uint8_t* str=(uint8_t*)malloc(sizeof(uint8_t)*(*size->u.num));
	if(str==NULL)errors("B_getb",__FILE__,__LINE__);
	int32_t realRead=0;
	realRead=fread(str,sizeof(uint8_t),*size->u.num,fp);
	stack->tp-=1;
	if(!realRead)
	{
		free(str);
		stack->values[stack->tp-1]=newNilValue(exe->heap);
	}
	else
	{
		str=(uint8_t*)realloc(str,sizeof(uint8_t)*realRead);
		if(!str)
			errors("B_getb",__FILE__,__LINE__);
		VMvalue* tmpBary=newVMvalue(BYTS,NULL,exe->heap,1);
		tmpBary->u.byts=newEmptyByteArry();
		tmpBary->u.byts->size=*size->u.num;
		tmpBary->u.byts->str=str;
		stack->values[stack->tp-1]=tmpBary;
	}
	stackRecycle(exe);
	proc->cp+=1;
	return 0;
}

int B_write(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* file=getTopValue(stack);
	VMvalue* obj=getValue(stack,stack->tp-2);
	if(file->type!=FP)return WRONGARG;
	FILE* objFile=file->u.fp->fp;
	stack->tp-=1;
	stackRecycle(exe);
	VMpair* tmpPair=(obj->type==PAIR)?obj->u.pair:NULL;
	printVMvalue(obj,tmpPair,objFile,0,0);
	proc->cp+=1;
	return 0;
}

int B_putb(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* file=getTopValue(stack);
	VMvalue* bt=getValue(stack,stack->tp-2);
	if(file->type!=FP||bt->type!=BYTS)return WRONGARG;
	FILE* objFile=file->u.fp->fp;
	stack->tp-=1;
	stackRecycle(exe);
	fwrite(bt->u.byts->str,sizeof(uint8_t),bt->u.byts->size,objFile);
	proc->cp+=1;
	return 0;
}

int B_princ(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* file=getTopValue(stack);
	VMvalue* obj=getValue(stack,stack->tp-2);
	if(file->type!=FP)return WRONGARG;
	FILE* objFile=file->u.fp->fp;
	stack->tp-=1;
	stackRecycle(exe);
	VMpair* tmpPair=(obj->type==PAIR)?obj->u.pair:NULL;
	princVMvalue(obj,tmpPair,objFile,0);
	proc->cp+=1;
	return 0;
}

int B_go(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* threadArg=getTopValue(stack);
	VMvalue* threadProc=getValue(stack,stack->tp-2);
	if(threadProc->type!=PRC||(threadArg->type!=PAIR&&threadArg->type!=NIL))
		return WRONGARG;
	if(exe->VMid==-1)
		return CANTCREATETHREAD;
	FakeVM* threadVM=newThreadVM(threadProc->u.prc,exe->procs,exe->heap,exe->modules);
	threadVM->callback=exe->callback;
	threadVM->table=exe->table;
	threadVM->lnt=exe->lnt;
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
		tmp->access=1;
		copyRef(tmp,threadArg->u.pair->car);
		threadVMstack->values[threadVMstack->tp]=tmp;
		threadVMstack->tp+=1;
		threadArg=threadArg->u.pair->cdr;
	}
	threadVM->chan->refcount+=1;
	Chanl* chan=threadVM->chan;
	stack->tp-=1;
	int32_t faildCode=pthread_create(&threadVM->tid,NULL,ThreadVMFunc,threadVM);
	if(faildCode)
	{
		threadVM->chan->refcount-=1;
		freeChanl(threadVM->chan);
		deleteCallChain(threadVM);
		threadVM->mark=0;
		freeVMstack(threadVM->stack);
		threadVM->stack=NULL;
		threadVM->lnt=NULL;
		threadVM->table=NULL;
		stack->values[stack->tp-1]=newVMvalue(IN32,&faildCode,exe->heap,1);
	}
	else
	{
		VMvalue* threadChanl=newVMvalue(CHAN,chan,exe->heap,1);
		stack->values[stack->tp-1]=threadChanl;
	}
	proc->cp+=1;
	return 0;
}

int B_chanl(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* maxSize=getTopValue(stack);
	if(maxSize->type!=IN32)
		return WRONGARG;
	int32_t max=*maxSize->u.num;
	VMvalue* objValue=newVMvalue(CHAN,newChanl(max),exe->heap,1);
	stack->values[stack->tp-1]=objValue;
	proc->cp+=1;
	return 0;
}

int B_send(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* ch=getTopValue(stack);
	if(ch->type!=CHAN)
		return WRONGARG;
	VMvalue* message=getValue(stack,stack->tp-2);
	Chanl* tmpCh=ch->u.chan;
	pthread_mutex_lock(&tmpCh->lock);
	if(tmpCh->tail==NULL)
	{
		tmpCh->head=newThreadMessage(message,exe->heap);
		tmpCh->tail=tmpCh->head;
	}
	else
	{
		ThreadMessage* cur=tmpCh->tail;
		cur->next=newThreadMessage(message,exe->heap);
		tmpCh->tail=cur->next;
	}
	tmpCh->size+=1;
	pthread_mutex_unlock(&tmpCh->lock);
	while(tmpCh->size>tmpCh->max);
	stack->tp-=1;
	stackRecycle(exe);
	proc->cp+=1;
	return 0;
}

int B_recv(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* ch=getTopValue(stack);
	if(ch->type!=CHAN)
		return WRONGARG;
	VMvalue* tmp=newNilValue(exe->heap);
	Chanl* tmpCh=ch->u.chan;
	tmp->access=1;
	while(tmpCh->head==NULL);
	pthread_mutex_lock(&tmpCh->lock);
	copyRef(tmp,tmpCh->head->message);
	ThreadMessage* prev=tmpCh->head;
	tmpCh->head=prev->next;
	if(tmpCh->head==NULL)
		tmpCh->tail=NULL;
	tmpCh->size-=1;
	pthread_mutex_unlock(&tmpCh->lock);
	free(prev);
	stack->values[stack->tp-1]=tmp;
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
	tmp->tpsi=0;
	tmp->tptp=0;
	tmp->tpst=NULL;
	return tmp;
}

void printVMvalue(VMvalue* objValue,VMpair* begin,FILE* fp,int8_t mode,int8_t isPrevPair)
{
	switch(objValue->type)
	{
		case NIL:
			fprintf(fp,"nil");
			break;
		case IN32:
			fprintf(fp,"%d",*objValue->u.num);
			break;
		case DBL:
			fprintf(fp,"%lf",*objValue->u.dbl);
			break;
		case CHR:
			printRawChar(*objValue->u.chr,fp);
			break;
		case SYM:
			fprintf(fp,"%s",objValue->u.str->str);
			break;
		case STR:
			printRawString(objValue->u.str->str,fp);
			break;
		case PRC:
			if(mode==1)printProc(objValue->u.prc,fp);
			else fprintf(fp,"#<proc>");
			break;
		case PAIR:
			if(isPrevPair)
				putc(' ',fp);
			else
				putc('(',fp);
			if(objValue->u.pair->car->type==PAIR&&objValue->u.pair->car->u.pair==begin)
				fprintf(fp,"##");
			else
			{
				if(objValue->u.pair->car->type!=NIL||isPrevPair)
					printVMvalue(objValue->u.pair->car,begin,fp,mode,0);
			}
			if(objValue->u.pair->cdr->type!=NIL)
			{
				if(objValue->u.pair->cdr->type!=PAIR)
					putc(',',fp);
				if(objValue->u.pair->cdr->type==PAIR&&objValue->u.pair->cdr->u.pair==begin)
					fprintf(fp,"##");
				else
					printVMvalue(objValue->u.pair->cdr,begin,fp,mode,1);
			}
			if(!isPrevPair)
				putc(')',fp);
			break;
		case BYTS:
			printByteStr(objValue->u.byts,fp,1);
			break;
		case CONT:
			fprintf(fp,"#<cont>");
			break;
		case CHAN:
			fprintf(fp,"#<chan>");
			break;
		case FP:
			fprintf(fp,"#<fp>");
			break;
		default:fprintf(fp,"Bad value!");break;
	}
}

void princVMvalue(VMvalue* objValue,VMpair* begin,FILE* fp,int8_t isPrevPair)
{
	switch(objValue->type)
	{
		case NIL:
			fprintf(fp,"nil");
			break;
		case IN32:
			fprintf(fp,"%d",*objValue->u.num);
			break;
		case DBL:
			fprintf(fp,"%lf",*objValue->u.dbl);
			break;
		case CHR:
			putc(*objValue->u.chr,fp);
			break;
		case SYM:
			fprintf(fp,"%s",objValue->u.str->str);
			break;
		case STR:
			fprintf(fp,"%s",objValue->u.str->str);
			break;
		case PRC:
			fprintf(fp,"#<proc>");
			break;
		case PAIR:
			if(isPrevPair)
				putc(' ',fp);
			else
				putc('(',fp);
			if(objValue->u.pair->car->type==PAIR&&objValue->u.pair->car->u.pair==begin)
				fprintf(fp,"##");
			else
			{
				if(objValue->u.pair->car->type!=NIL||isPrevPair)
					princVMvalue(objValue->u.pair->car,begin,fp,0);
			}
			if(objValue->u.pair->cdr->type!=NIL)
			{
				if(objValue->u.pair->cdr->type!=PAIR)
					putc(',',fp);
				if(objValue->u.pair->cdr->type==PAIR&&objValue->u.pair->cdr->u.pair==begin)
					fprintf(fp,"##");
				else
					princVMvalue(objValue->u.pair->cdr,begin,fp,1);
			}
			if(!isPrevPair)
				putc(')',fp);
			break;
		case BYTS:
			printByteStr(objValue->u.byts,fp,0);
			break;
		case CONT:
			fprintf(fp,"#<cont>");
			break;
		case CHAN:
			fprintf(fp,"#<chan>");
			break;
		case FP:
			fprintf(fp,"#<fp>");
			break;
		default:fprintf(fp,"Bad value!");break;
	}
}

void stackRecycle(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* curproc=exe->curproc;
	VMcode* tmpCode=curproc->code;
	if(stack->size-stack->tp>64)
	{
		//	size_t newSize=stack->size-64;
		//	if(newSize>0)
		//	{
		//ByteCode tmpByteCode={tmpCode->size,tmpCode->code};
		//printByteCode(&tmpByteCode,stderr);
		stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size-64));
		if(stack->values==NULL)
		{
			fprintf(stderr,"stack->tp==%d,stack->size==%d\n",stack->tp,stack->size);
			fprintf(stderr,"cp=%d\n%s\n",curproc->cp,codeName[(int)tmpCode->code[curproc->cp]].codeName);
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
	tmp->localenv=newVMenv(NULL);
	tmp->refcount=0;
	if(proc!=NULL)
	{
		tmp->id=-1;
		tmp->size=proc->size;
		tmp->code=proc->code;
	}
	else
	{
		tmp->id=-1;
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

void printAllStack(VMstack* stack,FILE* fp,int mode)
{
	if(fp!=stdout)fprintf(fp,"Current stack:\n");
	if(stack->tp==0)fprintf(fp,"[#EMPTY]\n");
	else
	{
		int i=stack->tp-1;
		for(;i>=0;i--)
		{
			if(mode&&stack->bp==i)
				fputs("->",stderr);
			if(fp!=stdout)fprintf(fp,"%d:",i);
			VMvalue* tmp=stack->values[i];
			VMpair* tmpPair=(tmp->type==PAIR)?tmp->u.pair:NULL;
			printVMvalue(tmp,tmpPair,fp,0,0);
			putc('\n',fp);
		}
	}
}

void fprintValue(VMvalue* v,FILE* fp)
{
	VMpair* p=(v->type==PAIR)?v->u.pair:NULL;
	printVMvalue(v,p,fp,0,0);
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
			if((where+proc->cp+5)!=size)return 0;
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
			VMvalue* tmp=curEnv->list[i]->value;
			VMpair* tmpPair=(tmp->type==PAIR)?tmp->u.pair:NULL;
			printVMvalue(tmp,tmpPair,fp,0,0);
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

void GC_mark(FakeVM* exe)
{
	GC_markValueInStack(exe->stack);
	GC_markValueInCallChain(exe->curproc);
	if(exe->chan)
	{
		pthread_mutex_lock(&exe->chan->lock);
		GC_markMessage(exe->chan->head);
		pthread_mutex_unlock(&exe->chan->lock);
	}
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
		else if(obj->type==CONT)
		{
			GC_markValueInStack(obj->u.cont->stack);
			int32_t i=0;
			for(;i<obj->u.cont->size;i++)
				GC_markValueInEnv(obj->u.cont->status[i].env);
		}
		else if(obj->type==CHAN)
		{
			pthread_mutex_lock(&obj->u.chan->lock);
			GC_markMessage(obj->u.chan->head);
			pthread_mutex_unlock(&obj->u.chan->lock);
		}
	}
}

void GC_markValueInEnv(VMenv* curEnv)
{
	int32_t i=0;
	for(;i<curEnv->size;i++)
		GC_markValue(curEnv->list[i]->value);
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

void GC_markMessage(ThreadMessage* head)
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

FakeVM* newThreadVM(VMcode* main,ByteCode* procs,VMheap* heap,Dlls* d)
{
	FakeVM* exe=(FakeVM*)malloc(sizeof(FakeVM));
	if(exe==NULL)errors("newThreadVM",__FILE__,__LINE__);
	exe->mainproc=newFakeProcess(main,NULL);
	exe->mainproc->localenv=newVMenv(main->localenv->prev);
	exe->curproc=exe->mainproc;
	exe->procs=procs;
	exe->mark=1;
	exe->argc=0;
	exe->argv=NULL;
	exe->modules=d;
	exe->chan=newChanl(1);
	main->refcount+=1;
	exe->stack=newStack(0);
	exe->heap=heap;
	exe->callback=NULL;
	FakeVM** ppFakeVM=NULL;
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
		GlobFakeVMs.VMs=(FakeVM**)realloc(GlobFakeVMs.VMs,sizeof(FakeVM*)*(size+1));
		if(size!=0&&GlobFakeVMs.VMs==NULL)errors("newThreadVM",__FILE__,__LINE__);
		GlobFakeVMs.VMs[size]=exe;
		GlobFakeVMs.size+=1;
		exe->VMid=size;
	}
	return exe;
}

void freeVMstack(VMstack* stack)
{
	free(stack->tpst);
	free(stack->values);
	free(stack);
}

ThreadMessage* newThreadMessage(VMvalue* val,VMheap* heap)
{
	ThreadMessage* tmp=(ThreadMessage*)malloc(sizeof(ThreadMessage));
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
	FakeVM* cur=GlobFakeVMs.VMs[0];
	if(cur->mainproc&&cur->mainproc->code)
	{
		freeVMcode(cur->mainproc->code);
		free(cur->mainproc);
	}
	freeVMstack(cur->stack);
	deleteAllDll(cur->modules);
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
		FakeVM* cur=GlobFakeVMs.VMs[i];
		if(cur)
			pthread_join(cur->tid,NULL);
	}
}

void deleteCallChain(FakeVM* exe)
{
	VMprocess* cur=exe->curproc;
	while(cur)
	{
		VMprocess* prev=cur;
		cur=cur->prev;
		freeVMenv(prev->localenv);
		freeVMcode(prev->code);
		free(prev);
	}
	exe->mainproc=NULL;
}

void createCallChainWithContinuation(FakeVM* vm,VMcontinuation* cc)
{
	VMstack* stack=vm->stack;
	VMstack* tmpStack=copyStack(cc->stack);
	int32_t i=stack->bp;
	for(;i<stack->tp;i++)
	{
		if(tmpStack->tp>=tmpStack->size)
		{
			tmpStack->values=(VMvalue**)realloc(tmpStack->values,sizeof(VMvalue*)*(tmpStack->size+64));
			if(tmpStack->values==NULL)errors("createCallChainWithContinuation",__FILE__,__LINE__);
			tmpStack->size+=64;
		}
		tmpStack->values[tmpStack->tp]=stack->values[i];
		tmpStack->tp+=1;
	}
	deleteCallChain(vm);
	VMprocess* curproc=NULL;
	vm->stack=tmpStack;
	freeVMstack(stack);
	for(i=cc->size-1;i>=0;i--)
	{
		VMprocess* cur=(VMprocess*)malloc(sizeof(VMprocess));
		if(!cur)errors("createCallChainWithContinuation",__FILE__,__LINE__);
		cur->prev=curproc;
		cur->cp=cc->status[i].cp;
		cur->localenv=cc->status[i].env;
		cur->localenv->refcount+=1;
		cur->code=cc->status[i].proc;
		cur->code->refcount+=1;
		curproc=cur;
	}
	vm->curproc=curproc;
	while(curproc->prev)
		curproc=curproc->prev;
	vm->mainproc=curproc;
}

void sortVMenvList(VMenv* env)
{
	int32_t i=0;
	for(;i<env->size;i++)
	{
		int32_t j=i+1;
		int32_t min=i;
		for(;j<env->size;j++)
			if(env->list[j]->id<env->list[min]->id)
				min=j;
		VMenvNode* t=env->list[i];
		env->list[i]=env->list[min];
		env->list[min]=t;
	}
}

int32_t getSymbolIdInByteCode(const char* code)
{
	char op=*code;
	switch(op)
	{
		case FAKE_PUSH_VAR:
			return *(int32_t*)(code+sizeof(char));
			break;
		case FAKE_POP_VAR:
		case FAKE_POP_REST_VAR:
			return *(int32_t*)(code+sizeof(char)+sizeof(int32_t));
			break;
	}
	return -1;
}
