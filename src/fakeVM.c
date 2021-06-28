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
#include<setjmp.h>

#define RAISE_BUILTIN_ERROR(ERRORTYPE,RUNNABLE,EXE) {\
	char* errorMessage=genErrorMessage((ERRORTYPE),(RUNNABLE),(EXE));\
	VMerror* err=newVMerror(builtInErrorType[(ERRORTYPE)],errorMessage);\
	free(errorMessage);\
	raiseVMerror(err,(EXE));\
	return;\
}

extern const char* builtInErrorType[19];

static int envNodeCmp(const void* a,const void* b)
{
	return ((*(VMenvNode**)a)->id-(*(VMenvNode**)b)->id);
}

static int32_t getSymbolIdInByteCode(const uint8_t*);
extern char* builtInSymbolList[NUMOFBUILTINSYMBOL];
pthread_rwlock_t GClock=PTHREAD_RWLOCK_INITIALIZER;
FakeVMlist GlobFakeVMs={0,NULL};

static char* genErrorMessage(unsigned int type,VMrunnable* r,FakeVM* exe)
{
	int32_t cp=r->cp;
	LineNumTabNode* node=findLineNumTabNode(cp,exe->lnt);
	char* filename=exe->table->idl[node->fid]->symbol;
	char* line=intToString(node->line);
	char* t=(char*)malloc(sizeof(char)*(strlen("In file \"\",line \n")+strlen(filename)+strlen(line)+1));
	FAKE_ASSERT(t,"genErrorMessage",__FILE__,__LINE__);
	sprintf(t,"In file \"%s\",line %s\n",filename,line);
	free(line);
	t=strCat(t,"error:");
	switch(type)
	{
		case WRONGARG:
			t=strCat(t,"Wrong arguement.\n");
			break;
		case STACKERROR:
			t=strCat(t,"Stack error.\n");
			break;
		case TOOMANYARG:
			t=strCat(t,"Too many arguements.\n");
			break;
		case TOOFEWARG:
			t=strCat(t,"Too few arguements.\n");
			break;
		case CANTCREATETHREAD:
			t=strCat(t,"Can't create thread.\n");
			break;
		case SYMUNDEFINE:
			t=strCat(t,"Symbol \"");
			t=strCat(t,exe->table->idl[getSymbolIdInByteCode(exe->code+r->cp)]->symbol);
			t=strCat(t,"\" is undefined.\n");
			break;
		case INVOKEERROR:
			t=strCat(t,"Try to invoke an object that isn't procedure,continuation or native procedure.\n");
			break;
		case LOADDLLFAILD:
			t=strCat(t,"Faild to load dll:\"");
			t=strCat(t,exe->stack->values[exe->stack->tp-1]->u.str->str);
			t=strCat(t,"\".\n");
			break;
		case INVALIDSYMBOL:
			t=strCat(t,"Invalid symbol:");
			t=strCat(t,exe->stack->values[exe->stack->tp-1]->u.str->str);
			t=strCat(t,"\n");
			break;
		case DIVZERROERROR:
			t=strCat(t,"Divided by zero.\n");
			break;
	}
	return t;
}

static CRL* newCRL(VMpair* pair,int32_t count)
{
	CRL* tmp=(CRL*)malloc(sizeof(CRL));
	FAKE_ASSERT(tmp,"newCRL",__FILE__,__LINE__);
	tmp->pair=pair;
	tmp->count=count;
	tmp->next=NULL;
	return tmp;
}

static int32_t findCRLcount(VMpair* pair,CRL* h)
{
	for(;h;h=h->next)
	{
		if(h->pair==pair)
			return h->count;
	}
	return -1;
}

static VMpair* hasSameVMpair(VMpair* begin,VMpair* other,CRL* h)
{
	VMpair* tmpPair=NULL;
	if(findCRLcount(begin,h)!=-1||findCRLcount(other,h)!=-1)
		return NULL;
	if(begin==other)
		return begin;

	if((other->car->type==PAIR&&other->car->u.pair->car->type==PAIR)&&begin->car->type==PAIR)
		tmpPair=hasSameVMpair(begin->car->u.pair,other->car->u.pair->car->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((other->car->type==PAIR&&other->car->u.pair->cdr->type==PAIR)&&begin->car->type==PAIR)
		tmpPair=hasSameVMpair(begin->car->u.pair,other->car->u.pair->cdr->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((other->car->type==PAIR&&other->car->u.pair->car->type==PAIR)&&begin->cdr->type==PAIR)
		tmpPair=hasSameVMpair(begin->cdr->u.pair,other->car->u.pair->car->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((other->car->type==PAIR&&other->car->u.pair->cdr->type==PAIR)&&begin->cdr->type==PAIR)
		tmpPair=hasSameVMpair(begin->cdr->u.pair,other->car->u.pair->cdr->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((other->cdr->type==PAIR&&other->cdr->u.pair->car->type==PAIR)&&begin->car->type==PAIR)
		tmpPair=hasSameVMpair(begin->car->u.pair,other->cdr->u.pair->car->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((other->cdr->type==PAIR&&other->cdr->u.pair->cdr->type==PAIR)&&begin->car->type==PAIR)
		tmpPair=hasSameVMpair(begin->car->u.pair,other->cdr->u.pair->cdr->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((other->cdr->type==PAIR&&other->cdr->u.pair->car->type==PAIR)&&begin->cdr->type==PAIR)
		tmpPair=hasSameVMpair(begin->cdr->u.pair,other->cdr->u.pair->car->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((other->cdr->type==PAIR&&other->cdr->u.pair->cdr->type==PAIR)&&begin->cdr->type==PAIR)
		tmpPair=hasSameVMpair(begin->cdr->u.pair,other->cdr->u.pair->cdr->u.pair,h);
	if(tmpPair)
		return tmpPair;
	return NULL;
}

VMpair* isCircularReference(VMpair* begin,CRL* h)
{
	VMpair* tmpPair=NULL;
	if(begin->car->type==PAIR)
		tmpPair=hasSameVMpair(begin,begin->car->u.pair,h);
	if(tmpPair)
		return tmpPair;
	if(begin->cdr->type==PAIR)
		tmpPair=hasSameVMpair(begin,begin->cdr->u.pair,h);
	if(tmpPair)
		return tmpPair;
	return NULL;
}

int8_t isInTheCircle(VMpair* obj,VMpair* begin,VMpair* curPair)
{
	if(obj==curPair)
		return 1;
	if((curPair->car->type==PAIR&&begin==curPair->car->u.pair)||(curPair->cdr->type==PAIR&&begin==curPair->cdr->u.pair))
		return 0;
	return ((curPair->car->type==PAIR)&&isInTheCircle(obj,begin,curPair->car->u.pair))||((curPair->cdr->type==PAIR)&&isInTheCircle(obj,begin,curPair->cdr->u.pair));
}

static void (*ByteCodes[])(FakeVM*)=
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
	B_push_env_var,
	B_push_car,
	B_push_cdr,
	B_push_top,
	B_push_proc,
	B_push_list_arg,
	B_pop_var,
	B_pop_arg,
	B_pop_rest_arg,
	B_pop_car,
	B_pop_cdr,
	B_pop_ref,
	B_pop_env,
	B_swap,
	B_pack_cc,
	B_set_tp,
	B_set_bp,
	B_invoke,
	B_res_tp,
	B_pop_tp,
	B_res_bp,
	B_jmp_if_true,
	B_jmp_if_false,
	B_jmp,
	B_add,
	B_sub,
	B_mul,
	B_div,
	B_rem,
	B_atom,
	B_null,
	B_type,
	B_chr,
	B_int,
	B_dbl,
	B_str,
	B_sym,
	B_byte,
	B_nth,
	B_length,
	B_appd,
	B_file,
	B_dll,
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
	B_dlsym,
	B_go,
	B_chanl,
	B_send,
	B_recv,
	B_error,
	B_raise,
	B_push_try,
	B_pop_try,
};

FakeVM* newFakeVM(ByteCode* mainCode)
{
	FakeVM* exe=(FakeVM*)malloc(sizeof(FakeVM));
	FAKE_ASSERT(exe,"newFakeVM",__FILE__,__LINE__);
	VMproc* tmpVMproc=NULL;
	exe->code=NULL;
	exe->size=0;
	exe->rstack=newComStack(32);
	if(mainCode!=NULL)
	{
		exe->code=copyMemory(mainCode->code,mainCode->size);
		exe->size=mainCode->size;
		tmpVMproc=newVMproc(0,mainCode->size);
		pushComStack(newVMrunnable(tmpVMproc),exe->rstack);
		freeVMproc(tmpVMproc);
	}
	exe->argc=0;
	exe->argv=NULL;
	exe->mark=1;
	exe->chan=NULL;
	exe->stack=newVMstack(0);
	exe->tstack=newComStack(32);
	exe->heap=newVMheap();
	exe->callback=NULL;
	FakeVM** ppFakeVM=NULL;
	int i=0;
	for(;i<GlobFakeVMs.num;i++)
		if(GlobFakeVMs.VMs[i]==NULL)
			ppFakeVM=GlobFakeVMs.VMs+i;
	if(ppFakeVM!=NULL)
	{
		exe->VMid=i;
		*ppFakeVM=exe;
	}
	else
	{
		int32_t size=GlobFakeVMs.num;
		GlobFakeVMs.VMs=(FakeVM**)realloc(GlobFakeVMs.VMs,sizeof(FakeVM*)*(size+1));
		FAKE_ASSERT(!size||GlobFakeVMs.VMs,"newFakeVM",__FILE__,__LINE__);
		GlobFakeVMs.VMs[size]=exe;
		GlobFakeVMs.num+=1;
		exe->VMid=size;
	}
	return exe;
}

FakeVM* newTmpFakeVM(ByteCode* mainCode)
{
	FakeVM* exe=(FakeVM*)malloc(sizeof(FakeVM));
	FAKE_ASSERT(exe,"newTmpFakeVM",__FILE__,__LINE__);
	exe->code=NULL;
	exe->size=0;
	exe->rstack=newComStack(32);
	if(mainCode!=NULL)
	{
		exe->code=copyMemory(mainCode->code,mainCode->size);
		exe->size=mainCode->size;
		pushComStack(newVMrunnable(newVMproc(0,mainCode->size)),exe->rstack);
	}
	exe->mark=1;
	exe->argc=0;
	exe->argv=NULL;
	exe->chan=NULL;
	exe->stack=newVMstack(0);
	exe->tstack=newComStack(32);
	exe->heap=newVMheap();
	exe->callback=NULL;
	exe->VMid=-1;
	return exe;
}

void initGlobEnv(VMenv* obj,VMheap* heap,SymbolTable* table)
{
	obj->num=NUMOFBUILTINSYMBOL;
	obj->list=(VMenvNode**)malloc(sizeof(VMenvNode*)*NUMOFBUILTINSYMBOL);
	FAKE_ASSERT(obj->list,"initGlobEnv",__FILE__,__LINE__);
	int32_t tmpInt=EOF;
	obj->list[0]=newVMenvNode(newNilValue(heap),findSymbol(builtInSymbolList[0],table)->id);
	obj->list[1]=newVMenvNode(newVMvalue(IN32,&tmpInt,heap,1),findSymbol(builtInSymbolList[1],table)->id);
	obj->list[2]=newVMenvNode(newVMvalue(FP,newVMfp(stdin),heap,1),findSymbol(builtInSymbolList[2],table)->id);
	obj->list[3]=newVMenvNode(newVMvalue(FP,newVMfp(stdout),heap,1),findSymbol(builtInSymbolList[3],table)->id);
	obj->list[4]=newVMenvNode(newVMvalue(FP,newVMfp(stderr),heap,1),findSymbol(builtInSymbolList[4],table)->id);
	mergeSort(obj->list,obj->num,sizeof(VMenvNode*),envNodeCmp);
}

void* ThreadVMFunc(void* p)
{
	FakeVM* exe=(FakeVM*)p;
	int64_t status=runFakeVM(exe);
	VMChanl* tmpCh=exe->chan;
	exe->chan=NULL;
	if(!status)
	{
		VMvalue* tmp=newNilValue(exe->heap);
		tmp->access=1;
		copyRef(tmp,getTopValue(exe->stack));
		SendT* t=newSendT(tmp);
		chanlSend(t,tmpCh);
	}
	freeVMChanl(tmpCh);
	freeVMstack(exe->stack);
	exe->stack=NULL;
	exe->lnt=NULL;
	exe->table=NULL;
	if(status!=0)
		deleteCallChain(exe);
	freeComStack(exe->tstack);
	freeComStack(exe->rstack);
	exe->mark=0;
	return (void*)status;
}

int runFakeVM(FakeVM* exe)
{
	while(!isComStackEmpty(exe->rstack))
	{
		VMrunnable* currunnable=topComStack(exe->rstack);
		pthread_rwlock_rdlock(&GClock);
		if(currunnable->cp>=currunnable->proc->cpc+currunnable->proc->scp)
		{
			VMrunnable* tmpProc=popComStack(exe->rstack);
			VMproc* tmpCode=tmpProc->proc;
			VMenv* tmpEnv=tmpProc->localenv;
			free(tmpProc);
			freeVMenv(tmpEnv);
			freeVMproc(tmpCode);
			continue;
		}

		int32_t cp=currunnable->cp;
		ByteCodes[(uint8_t)exe->code[cp]](exe);
		if(setjmp(exe->buf))
		{
			return 255;
		}
		pthread_rwlock_unlock(&GClock);
		if(exe->heap->num>exe->heap->threshold)
		{
			if(pthread_rwlock_trywrlock(&GClock))continue;
			int i=0;
			if(exe->VMid!=-1)
			{
				for(;i<GlobFakeVMs.num;i++)
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
			exe->heap->threshold=exe->heap->num+THRESHOLD_SIZE;
			pthread_mutex_unlock(&exe->heap->lock);
			pthread_rwlock_unlock(&GClock);
		}
	}
	return 0;
}

void B_dummy(FakeVM* exe)
{
	VMrunnable* currunnable=topComStack(exe->rstack);
	VMproc* tmpCode=currunnable->proc;
	ByteCode tmpByteCode={tmpCode->cpc,exe->code+tmpCode->scp};
	VMstack* stack=exe->stack;
	printByteCode(&tmpByteCode,stderr);
	putc('\n',stderr);
	DBG_printVMstack(exe->stack,stderr,1);
	putc('\n',stderr);
	fprintf(stderr,"stack->tp==%d,stack->size==%d\n",stack->tp,stack->size);
	fprintf(stderr,"cp=%d stack->bp=%d\n%s\n",currunnable->cp-currunnable->proc->scp,stack->bp,codeName[(uint8_t)exe->code[currunnable->cp]].codeName);
	DBG_printVMenv(currunnable->localenv,stderr);
	putc('\n',stderr);
	fprintf(stderr,"Wrong byts code!\n");
	exit(EXIT_FAILURE);
}

void B_push_nil(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	SET_RETURN("B_push_nil",newNilValue(exe->heap),stack);
	runnable->cp+=1;
}

void B_push_pair(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	SET_RETURN("B_push_pair",newVMvalue(PAIR,newVMpair(exe->heap),exe->heap,1),stack);
	runnable->cp+=1;

}

void B_push_int(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	SET_RETURN("B_push_int",newVMvalue(IN32,exe->code+runnable->cp+1,exe->heap,1),stack);
	runnable->cp+=5;
}

void B_push_chr(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	SET_RETURN("B_push_chr",newVMvalue(CHR,exe->code+runnable->cp+1,exe->heap,1),stack);
	runnable->cp+=2;
}

void B_push_dbl(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	SET_RETURN("B_push_dbl",newVMvalue(DBL,exe->code+runnable->cp+1,exe->heap,1),stack);
	runnable->cp+=9;
}

void B_push_str(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	int len=strlen((char*)(exe->code+runnable->cp+1));
	VMstr* tmpStr=newVMstr((char*)exe->code+runnable->cp+1);
	VMvalue* objValue=newVMvalue(STR,tmpStr,exe->heap,1);
	SET_RETURN("B_push_str",objValue,stack);
	runnable->cp+=2+len;
}

void B_push_sym(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	int len=strlen((char*)(exe->code+runnable->cp+1));
	VMstr* tmpStr=newVMstr((char*)exe->code+runnable->cp+1);
	VMvalue* objValue=newVMvalue(SYM,tmpStr,exe->heap,1);
	SET_RETURN("B_push_sym",objValue,stack);
	runnable->cp+=2+len;
}

void B_push_byte(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	int32_t size=*(int32_t*)(exe->code+runnable->cp+1);
	uint8_t* tmp=createByteString(size);
	memcpy(tmp,exe->code+runnable->cp+5,size);
	ByteString* tmpArry=newByteString(size,NULL);
	tmpArry->str=tmp;
	VMvalue* objValue=newVMvalue(BYTS,tmpArry,exe->heap,1);
	SET_RETURN("B_push_byte",objValue,stack);
	runnable->cp+=5+size;
}

void B_push_var(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	uint32_t idOfVar=*(uint32_t*)(exe->code+runnable->cp+1);
	VMenv* curEnv=runnable->localenv;
	VMvalue* tmpValue=NULL;
	VMenvNode* tmp=NULL;
	while(curEnv&&!tmp)
	{
		tmp=findVMenvNode(idOfVar,curEnv);
		curEnv=curEnv->prev;
	}
	if(tmp==NULL)
	{
		RAISE_BUILTIN_ERROR(SYMUNDEFINE,runnable,exe);
	}
	tmpValue=tmp->value;
	SET_RETURN("B_push_var",tmpValue,stack);
	runnable->cp+=5;
}

void B_push_env_var(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* topValue=getTopValue(stack);
	if(topValue->type!=STR&&topValue->type!=SYM)
	{
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	}
	SymTabNode* stn=findSymbol(topValue->u.str->str,exe->table);
	if(stn==NULL)
		RAISE_BUILTIN_ERROR(INVALIDSYMBOL,runnable,exe);
	int32_t idOfVar=stn->id;
	VMenv* curEnv=runnable->localenv;
	VMenvNode* tmp=NULL;
	while(curEnv&&!tmp)
	{
		tmp=findVMenvNode(idOfVar,curEnv);
		curEnv=curEnv->prev;
	}
	if(tmp==NULL)
		RAISE_BUILTIN_ERROR(INVALIDSYMBOL,runnable,exe);
	stack->values[stack->tp-1]=tmp->value;
	runnable->cp+=1;
}

void B_push_car(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* objValue=getTopValue(stack);
	if(objValue->type!=PAIR&&objValue->type!=STR&&objValue->type!=BYTS)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	if(objValue->type==PAIR)
	{
		stack->values[stack->tp-1]=getVMpairCar(objValue);
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
	runnable->cp+=1;
}

void B_push_cdr(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* objValue=getTopValue(stack);
	if(objValue->type!=PAIR&&objValue->type!=STR&&objValue->type!=BYTS)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	if(objValue->type==PAIR)
	{
		stack->values[stack->tp-1]=getVMpairCdr(objValue);
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
	runnable->cp+=1;
}

void B_push_top(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	if(stack->tp==stack->bp)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	SET_RETURN("B_push_top",getTopValue(stack),stack);
	runnable->cp+=1;
}

void B_push_proc(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	int32_t sizeOfProc=*(int32_t*)(exe->code+runnable->cp+1);
	VMproc* code=newVMproc(runnable->cp+1+sizeof(int32_t),sizeOfProc);
	increaseVMenvRefcount(runnable->localenv);
	code->prevEnv=runnable->localenv;
	VMvalue* objValue=newVMvalue(PRC,code,exe->heap,1);
	SET_RETURN("B_push_proc",objValue,stack);
	runnable->cp+=5+sizeOfProc;
}

void B_push_list_arg(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	ComStack* comStack=newComStack(32);
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* tmpList=getTopValue(stack);
	stack->tp-=1;
	for(;tmpList->type!=NIL;tmpList=getVMpairCdr(tmpList))
		pushComStack(getVMpairCar(tmpList),comStack);
	while(!isComStackEmpty(comStack))
	{
		VMvalue* t=popComStack(comStack);
		VMvalue* tmp=newNilValue(exe->heap);
		tmp->access=1;
		copyRef(tmp,t);
		SET_RETURN("B_push_list_arg",tmp,stack);
	}
	freeComStack(comStack);
	runnable->cp+=1;
}

void B_pop_var(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	if(!(stack->tp>stack->bp))
		RAISE_BUILTIN_ERROR(STACKERROR,runnable,exe);
	int32_t scopeOfVar=*(int32_t*)(exe->code+runnable->cp+1);
	uint32_t idOfVar=*(uint32_t*)(exe->code+runnable->cp+5);
	VMenv* curEnv=runnable->localenv;
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
			RAISE_BUILTIN_ERROR(SYMUNDEFINE,runnable,exe);
		pValue=&tmp->value;
	}
	VMvalue* topValue=getTopValue(stack);
	*pValue=newNilValue(exe->heap);
	(*pValue)->access=1;
	copyRef(*pValue,topValue);
	stack->tp-=1;
	stackRecycle(exe);
	runnable->cp+=9;
}

void B_pop_arg(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	if(!(stack->tp>stack->bp))
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	int32_t scopeOfVar=*(int32_t*)(exe->code+runnable->cp+1);
	uint32_t idOfVar=*(uint32_t*)(exe->code+runnable->cp+5);
	VMenv* curEnv=runnable->localenv;
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
			RAISE_BUILTIN_ERROR(SYMUNDEFINE,runnable,exe);
		pValue=&tmp->value;
	}
	VMvalue* topValue=getTopValue(stack);
	*pValue=newNilValue(exe->heap);
	(*pValue)->access=1;
	copyRef(*pValue,topValue);
	stack->tp-=1;
	stackRecycle(exe);
	runnable->cp+=9;
}

void B_pop_rest_arg(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	int32_t scopeOfVar=*(int32_t*)(exe->code+runnable->cp+1);
	uint32_t idOfVar=*(uint32_t*)(exe->code+runnable->cp+5);
	VMenv* curEnv=runnable->localenv;
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
			RAISE_BUILTIN_ERROR(SYMUNDEFINE,runnable,exe);
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
		if(stack->tp>stack->bp)
			tmp->u.pair->cdr=newVMvalue(PAIR,newVMpair(exe->heap),exe->heap,1);
		else break;
		tmp=tmp->u.pair->cdr;
	}
	*pValue=obj;
	stackRecycle(exe);
	runnable->cp+=9;
}

void B_pop_car(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* topValue=getTopValue(stack);
	VMvalue* objValue=getValue(stack,stack->tp-2);
	if(objValue->type!=PAIR)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	objValue->u.pair->car=topValue;
	stack->tp-=1;
	stackRecycle(exe);
	runnable->cp+=1;
}

void B_pop_cdr(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* topValue=getTopValue(stack);
	VMvalue* objValue=getValue(stack,stack->tp-2);
	if(objValue->type!=PAIR)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	objValue->u.pair->cdr=topValue;
	stack->tp-=1;
	stackRecycle(exe);
	runnable->cp+=1;
}

void B_pop_ref(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* topValue=getTopValue(stack);
	VMvalue* objValue=getValue(stack,stack->tp-2);
	if(objValue->access==0&&objValue->type!=topValue->type)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	if(objValue->type==topValue->type)
		writeRef(objValue,topValue);
	else
		copyRef(objValue,topValue);
	stack->tp-=1;
	stackRecycle(exe);
	runnable->cp+=1;
}

void B_pop_env(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* topValue=getTopValue(stack);
	if(topValue->type!=PRC)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	VMenv** ppEnv=&topValue->u.prc->prevEnv;
	VMenv* tmpEnv=*ppEnv;
	int32_t i=0;
	int32_t scope=*(int32_t*)(exe->code+runnable->cp+1);
	for(;i<scope&&tmpEnv;i++)
	{
		ppEnv=&tmpEnv->prev;
		tmpEnv=*ppEnv;
	}
	if(tmpEnv)
	{
		*ppEnv=tmpEnv->prev;
		tmpEnv->prev=NULL;
		freeVMenv(tmpEnv);
	}
	else
		RAISE_BUILTIN_ERROR(STACKERROR,runnable,exe);
	runnable->cp+=5;
}

void B_swap(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	if(stack->tp<2)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	VMvalue* topValue=getTopValue(stack);
	VMvalue* otherValue=getValue(stack,stack->tp-2);
	stack->values[stack->tp-1]=otherValue;
	stack->values[stack->tp-2]=topValue;
	runnable->cp+=1;
}

void B_pack_cc(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMcontinuation* cc=newVMcontinuation(stack,exe->rstack);
	VMvalue* retval=newVMvalue(CONT,cc,exe->heap,1);
	SET_RETURN("B_pack_cc",retval,stack);
	runnable->cp+=1;
}

void B_add(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* firValue=getValue(stack,stack->tp-1);
	VMvalue* secValue=getValue(stack,stack->tp-2);
	if((firValue->type!=IN32&&firValue->type!=DBL)||(secValue->type!=IN32&&secValue->type!=DBL))
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	if(firValue->type==DBL||secValue->type==DBL)
	{
		double result=((firValue->type==DBL)?*firValue->u.dbl:*firValue->u.in32)+((secValue->type==DBL)?*secValue->u.dbl:*secValue->u.in32);
		VMvalue* tmpValue=newVMvalue(DBL,&result,exe->heap,1);
		stack->tp-=1;
		stackRecycle(exe);
		stack->values[stack->tp-1]=tmpValue;
	}
	else if(firValue->type==IN32&&secValue->type==IN32)
	{
		int32_t result=*firValue->u.in32+*secValue->u.in32;
		VMvalue* tmpValue=newVMvalue(IN32,&result,exe->heap,1);
		stack->tp-=1;
		stackRecycle(exe);
		stack->values[stack->tp-1]=tmpValue;
	}
	runnable->cp+=1;
}

void B_sub(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* firValue=getValue(stack,stack->tp-1);
	VMvalue* secValue=getValue(stack,stack->tp-2);
	if((firValue->type!=IN32&&firValue->type!=DBL)||(secValue->type!=IN32&&secValue->type!=DBL))
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	if(firValue->type==DBL||secValue->type==DBL)
	{
		double result=((secValue->type==DBL)?*secValue->u.dbl:*secValue->u.in32)-((firValue->type==DBL)?*firValue->u.dbl:*firValue->u.in32);
		VMvalue* tmpValue=newVMvalue(DBL,&result,exe->heap,1);
		stack->tp-=1;
		stackRecycle(exe);
		stack->values[stack->tp-1]=tmpValue;
	}
	else if(firValue->type==IN32&&secValue->type==IN32)
	{
		int32_t result=*secValue->u.in32-*firValue->u.in32;
		VMvalue* tmpValue=newVMvalue(IN32,&result,exe->heap,1);
		stack->tp-=1;
		stackRecycle(exe);
		stack->values[stack->tp-1]=tmpValue;
	}
	runnable->cp+=1;
}

void B_mul(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* firValue=getValue(stack,stack->tp-1);
	VMvalue* secValue=getValue(stack,stack->tp-2);
	if((firValue->type!=IN32&&firValue->type!=DBL)||(secValue->type!=IN32&&secValue->type!=DBL))
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	if(firValue->type==DBL||secValue->type==DBL)
	{
		double result=((firValue->type==DBL)?*firValue->u.dbl:*firValue->u.in32)*((secValue->type==DBL)?*secValue->u.dbl:*secValue->u.in32);
		VMvalue* tmpValue=newVMvalue(DBL,&result,exe->heap,1);
		stack->tp-=1;
		stackRecycle(exe);
		stack->values[stack->tp-1]=tmpValue;
	}
	else if(firValue->type==IN32&&secValue->type==IN32)
	{
		int32_t result=*firValue->u.in32*(*secValue->u.in32);
		VMvalue* tmpValue=newVMvalue(IN32,&result,exe->heap,1);
		stack->tp-=1;
		stackRecycle(exe);
		stack->values[stack->tp-1]=tmpValue;
	}
	runnable->cp+=1;
}

void B_div(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* firValue=getValue(stack,stack->tp-1);
	VMvalue* secValue=getValue(stack,stack->tp-2);
	VMvalue* tmpValue=NULL;
	if((firValue->type!=IN32&&firValue->type!=DBL)||(secValue->type!=IN32&&secValue->type!=DBL))
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	if((firValue->type==DBL&&fabs(*firValue->u.dbl)==0)||(firValue->type==IN32&&*firValue->u.in32==0))
		RAISE_BUILTIN_ERROR(DIVZERROERROR,runnable,exe);
	double result=((secValue->type==DBL)?*secValue->u.dbl:*secValue->u.in32)/((firValue->type==DBL)?*firValue->u.dbl:*firValue->u.in32);
	tmpValue=newVMvalue(DBL,&result,exe->heap,1);
	stack->tp-=1;
	stackRecycle(exe);
	stack->values[stack->tp-1]=tmpValue;
	runnable->cp+=1;
}

void B_rem(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* firValue=getValue(stack,stack->tp-1);
	VMvalue* secValue=getValue(stack,stack->tp-2);
	VMvalue* tmpValue=NULL;
	if((firValue->type!=IN32&&firValue->type!=DBL)||(secValue->type!=IN32&&secValue->type!=DBL))
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	if((firValue->type==DBL&&fabs(*firValue->u.dbl)==0)||(firValue->type==IN32&&*firValue->u.in32==0))
		RAISE_BUILTIN_ERROR(DIVZERROERROR,runnable,exe);
	if(firValue->type==IN32&&secValue->type==IN32)
	{
		int32_t result=*secValue->u.in32%*firValue->u.in32;
		tmpValue=newVMvalue(IN32,&result,exe->heap,1);
	}
	else
	{
		double result=fmod(((secValue->type==DBL)?*secValue->u.dbl:*secValue->u.in32),((firValue->type==DBL)?*firValue->u.dbl:*firValue->u.in32));
		tmpValue=newVMvalue(DBL,&result,exe->heap,1);
	}
	stack->tp-=1;
	stackRecycle(exe);
	stack->values[stack->tp-1]=tmpValue;
	runnable->cp+=1;
}

void B_atom(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* topValue=getTopValue(stack);
	if(topValue->type!=PAIR)
		stack->values[stack->tp-1]=newTrueValue(exe->heap);
	else
	{
		if(getVMpairCar(topValue)->type==NIL&&getVMpairCdr(topValue)->type==NIL)
			stack->values[stack->tp-1]=newTrueValue(exe->heap);
		else
			stack->values[stack->tp-1]=newNilValue(exe->heap);
	}
	runnable->cp+=1;
}

void B_null(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* topValue=getTopValue(stack);
	if(topValue->type==NIL||(topValue->type==PAIR&&(getVMpairCar(topValue)->type==NIL&&getVMpairCdr(topValue)->type==NIL)))
	{
		stack->values[stack->tp-1]=newTrueValue(exe->heap);
	}
	else stack->values[stack->tp-1]=newNilValue(exe->heap);
	runnable->cp+=1;
}

void B_type(FakeVM* exe)
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
		"dll",
		"dlproc",
		"err",
		"pair"
	};
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* topValue=getTopValue(stack);
	int32_t type=topValue->type;
	VMstr* str=newVMstr(a[type]);
	stack->values[stack->tp-1]=newVMvalue(SYM,str,exe->heap,1);
	runnable->cp+=1;
}

void B_set_tp(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	if(stack->tptp>=stack->tpsi)
	{
		stack->tpst=(uint32_t*)realloc(stack->tpst,sizeof(uint32_t)*(stack->tpsi+16));
		FAKE_ASSERT(stack->tpst,"B_set_tp",__FILE__,__LINE__);
		stack->tpsi+=16;
	}
	stack->tpst[stack->tptp]=stack->tp;
	stack->tptp+=1;
	runnable->cp+=1;
}

void B_set_bp(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* prevBp=newVMvalue(IN32,&stack->bp,exe->heap,1);
	SET_RETURN("B_set_bp",prevBp,stack);
	stack->bp=stack->tp;
	runnable->cp+=1;
}

void B_res_tp(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	stack->tp=(stack->tptp)?stack->tpst[stack->tptp-1]:0;
	runnable->cp+=1;
}

void B_pop_tp(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	stack->tptp-=1;
	if(stack->tpsi-stack->tptp>16)
	{
		stack->tpst=(uint32_t*)realloc(stack->tpst,sizeof(uint32_t)*(stack->tpsi-16));
		FAKE_ASSERT(stack->tpst,"B_pop_tp",__FILE__,__LINE__);
		stack->tpsi-=16;
	}
	runnable->cp+=1;
}

void B_res_bp(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	if(stack->tp>stack->bp)
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	VMvalue* prevBp=getTopValue(stack);
	stack->bp=*prevBp->u.in32;
	stack->tp-=1;
	stackRecycle(exe);
	runnable->cp+=1;
}

void B_invoke(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* tmpValue=getTopValue(stack);
	if(tmpValue->type!=PRC&&tmpValue->type!=CONT&&tmpValue->type!=DLPROC)
		RAISE_BUILTIN_ERROR(INVOKEERROR,runnable,exe);
	if(tmpValue->type==PRC)
	{
		VMproc* tmpProc=tmpValue->u.prc;
		VMrunnable* prevProc=hasSameProc(tmpProc,exe->rstack);
		if(isTheLastExpress(runnable,prevProc,exe)&&prevProc)
		{
			prevProc->cp=prevProc->proc->scp;
			runnable->cp+=(runnable!=prevProc);
		}
		else
		{
			VMrunnable* tmpRunnable=newVMrunnable(tmpProc);
			tmpRunnable->localenv=newVMenv(tmpProc->prevEnv);
			pushComStack(tmpRunnable,exe->rstack);
			runnable->cp+=1;
		}
		stack->tp-=1;
		stackRecycle(exe);
	}
	else if(tmpValue->type==CONT)
	{
		stack->tp-=1;
		stackRecycle(exe);
		runnable->cp+=1;
		VMcontinuation* cc=tmpValue->u.cont;
		createCallChainWithContinuation(exe,cc);
	}
	else
	{
		stack->tp-=1;
		stackRecycle(exe);
		runnable->cp+=1;
		DllFunc dllfunc=tmpValue->u.dlproc->func;
		ErrorType et=dllfunc(exe,&GClock);
		if(et)
			RAISE_BUILTIN_ERROR(et,runnable,exe);
	}
}

void B_jmp_if_true(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* tmpValue=getTopValue(stack);
	if(!(tmpValue->type==NIL||(tmpValue->type==PAIR&&getVMpairCar(tmpValue)->type==NIL&&getVMpairCdr(tmpValue)->type==NIL)))
	{
		int32_t where=*(int32_t*)(exe->code+runnable->cp+sizeof(char));
		runnable->cp+=where;
	}
	runnable->cp+=5;
}

void B_jmp_if_false(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* tmpValue=getTopValue(stack);
	stackRecycle(exe);
	if(tmpValue->type==NIL||(tmpValue->type==PAIR&&getVMpairCar(tmpValue)->type==NIL&&getVMpairCdr(tmpValue)->type==NIL))
	{
		int32_t where=*(int32_t*)(exe->code+runnable->cp+sizeof(char));
		runnable->cp+=where;
	}
	runnable->cp+=5;
}

void B_jmp(FakeVM* exe)
{
	VMrunnable* runnable=topComStack(exe->rstack);
	int32_t where=*(int32_t*)(exe->code+runnable->cp+sizeof(char));
	runnable->cp+=where+5;
}

void B_file(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMheap* heap=exe->heap;
	VMvalue* mode=getTopValue(stack);
	VMvalue* filename=getValue(stack,stack->tp-2);
	if(filename->type!=STR||mode->type!=STR)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	stack->tp-=1;
	stackRecycle(exe);
	FILE* file=fopen(filename->u.str->str,mode->u.str->str);
	if(!file)
		stack->values[stack->tp-1]=newNilValue(heap);
	else
		stack->values[stack->tp-1]=newVMvalue(FP,newVMfp(file),heap,1);
	runnable->cp+=1;
}

void B_dll(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMheap* heap=exe->heap;
	VMvalue* dllName=getTopValue(stack);
	if(dllName->type!=STR&&dllName->type!=SYM)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	VMDll* dll=newVMDll(dllName->u.str->str);
	if(!dll)
		RAISE_BUILTIN_ERROR(LOADDLLFAILD,runnable,exe);
	stack->values[stack->tp-1]=newVMvalue(DLL,dll,heap,1);
	runnable->cp+=1;
}

void B_dlsym(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMheap* heap=exe->heap;
	VMvalue* symbol=getTopValue(stack);
	VMvalue* dll=getValue(stack,stack->tp-2);
	if((symbol->type!=SYM&&symbol->type!=STR)||dll->type!=DLL)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	char prefix[]="FAKE_";
	char* realDlFuncName=(char*)malloc(sizeof(char)*(strlen(prefix)+strlen(symbol->u.str->str)+1));
	FAKE_ASSERT(realDlFuncName,"B_dlsym",__FILE__,__LINE__);
	sprintf(realDlFuncName,"%s%s",prefix,symbol->u.str->str);
	DllFunc funcAddress=getAddress(realDlFuncName,dll->u.dll->handle);
	if(!funcAddress)
	{
		free(realDlFuncName);
		RAISE_BUILTIN_ERROR(INVALIDSYMBOL,runnable,exe);
	}
	free(realDlFuncName);
	VMDlproc* dlproc=newVMDlproc(funcAddress,dll->u.dll);
	stack->tp-=1;
	stackRecycle(exe);
	stack->values[stack->tp-1]=newVMvalue(DLPROC,dlproc,heap,1);
	runnable->cp+=1;
}

void B_eq(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* firValue=getTopValue(stack);
	VMvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(exe);
	if(subVMvaluecmp(firValue,secValue))
		stack->values[stack->tp-1]=newTrueValue(exe->heap);
	else stack->values[stack->tp-1]=newNilValue(exe->heap);
	runnable->cp+=1;
}

void B_eqn(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* firValue=getTopValue(stack);
	VMvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(exe);
	if(firValue->type==DBL||secValue->type==DBL)
	{
		double result=((secValue->type==DBL)?*secValue->u.dbl:*secValue->u.in32)-((firValue->type==DBL)?*firValue->u.dbl:*firValue->u.in32);
		if(result==0)
			stack->values[stack->tp-1]=newTrueValue(exe->heap);
		else stack->values[stack->tp-1]=newNilValue(exe->heap);
	}
	else if(firValue->type==IN32&&secValue->type==IN32)
	{
		if(*secValue->u.in32==*firValue->u.in32)
			stack->values[stack->tp-1]=newTrueValue(exe->heap);
		else stack->values[stack->tp-1]=newNilValue(exe->heap);
	}
	else if(firValue->type==secValue->type&&(firValue->type==SYM||firValue->type==STR))
	{
		if(strcmp(secValue->u.str->str,firValue->u.str->str)==0)
			stack->values[stack->tp-1]=newTrueValue(exe->heap);
		else stack->values[stack->tp-1]=newNilValue(exe->heap);
	}
	else RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	runnable->cp+=1;
}

void B_equal(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* firValue=getTopValue(stack);
	VMvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(exe);
	if(VMvaluecmp(firValue,secValue))
		stack->values[stack->tp-1]=newTrueValue(exe->heap);
	else stack->values[stack->tp-1]=newNilValue(exe->heap);
	runnable->cp+=1;
}

void B_gt(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* firValue=getTopValue(stack);
	VMvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(exe);
	if(firValue->type==DBL||secValue->type==DBL)
	{
		double result=((secValue->type==DBL)?*secValue->u.dbl:*secValue->u.in32)-((firValue->type==DBL)?*firValue->u.dbl:*firValue->u.in32);
		if(result>0)
			stack->values[stack->tp-1]=newTrueValue(exe->heap);
		else stack->values[stack->tp-1]=newNilValue(exe->heap);
	}
	else if(firValue->type==IN32&&secValue->type==IN32)
	{
		if(*secValue->u.in32>*firValue->u.in32)
			stack->values[stack->tp-1]=newTrueValue(exe->heap);
		else stack->values[stack->tp-1]=newNilValue(exe->heap);
	}
	else if(firValue->type==secValue->type&&(firValue->type==SYM||firValue->type==STR))
	{
		if(strcmp(secValue->u.str->str,firValue->u.str->str)>0)
			stack->values[stack->tp-1]=newTrueValue(exe->heap);
		else stack->values[stack->tp-1]=newNilValue(exe->heap);
	}
	else
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	runnable->cp+=1;
}

void B_ge(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* firValue=getTopValue(stack);
	VMvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(exe);
	if(firValue->type==DBL||secValue->type==DBL)
	{
		double result=((secValue->type==DBL)?*secValue->u.dbl:*secValue->u.in32)-((firValue->type==DBL)?*firValue->u.dbl:*firValue->u.in32);
		if(result>=0)
			stack->values[stack->tp-1]=newTrueValue(exe->heap);
		else stack->values[stack->tp-1]=newTrueValue(exe->heap);
	}
	else if(firValue->type==IN32&&secValue->type==IN32)
	{
		if(*secValue->u.in32>=*firValue->u.in32)
			stack->values[stack->tp-1]=newTrueValue(exe->heap);
		else stack->values[stack->tp-1]=newNilValue(exe->heap);
	}
	else if(firValue->type==secValue->type&&(firValue->type==SYM||firValue->type==STR))
	{
		if(strcmp(secValue->u.str->str,firValue->u.str->str)>=0)
			stack->values[stack->tp-1]=newTrueValue(exe->heap);
		else stack->values[stack->tp-1]=newNilValue(exe->heap);
	}
	else
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	runnable->cp+=1;
}

void B_lt(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* firValue=getTopValue(stack);
	VMvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(exe);
	if(firValue->type==DBL||secValue->type==DBL)
	{
		double result=((secValue->type==DBL)?*secValue->u.dbl:*secValue->u.in32)-((firValue->type==DBL)?*firValue->u.dbl:*firValue->u.in32);
		if(result<0)
			stack->values[stack->tp-1]=newTrueValue(exe->heap);
		else stack->values[stack->tp-1]=newNilValue(exe->heap);
	}
	else if(firValue->type==IN32&&secValue->type==IN32)
	{
		if(*secValue->u.in32<*firValue->u.in32)
			stack->values[stack->tp-1]=newTrueValue(exe->heap);
		else stack->values[stack->tp-1]=newNilValue(exe->heap);
	}
	else if(firValue->type==secValue->type&&(firValue->type==SYM||firValue->type==STR))
	{
		if(strcmp(secValue->u.str->str,firValue->u.str->str)<0)
			stack->values[stack->tp-1]=newTrueValue(exe->heap);
		else stack->values[stack->tp-1]=newNilValue(exe->heap);
	}
	else
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	runnable->cp+=1;
}

void B_le(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* firValue=getTopValue(stack);
	VMvalue* secValue=getValue(stack,stack->tp-2);
	stack->tp-=1;
	stackRecycle(exe);
	if(firValue->type==DBL||secValue->type==DBL)
	{
		double result=((secValue->type==DBL)?*secValue->u.dbl:*secValue->u.in32)-((firValue->type==DBL)?*firValue->u.dbl:*firValue->u.in32);
		if(result<=0)
			stack->values[stack->tp-1]=newTrueValue(exe->heap);
		else stack->values[stack->tp-1]=newNilValue(exe->heap);
	}
	else if(firValue->type==IN32&&secValue->type==IN32)
	{
		if(*secValue->u.in32<=*firValue->u.in32)
			stack->values[stack->tp-1]=newTrueValue(exe->heap);
		else stack->values[stack->tp-1]=newNilValue(exe->heap);
	}
	else if(firValue->type==secValue->type&&(firValue->type==SYM||firValue->type==STR))
	{
		if(strcmp(secValue->u.str->str,firValue->u.str->str)<=0)
			stack->values[stack->tp-1]=newTrueValue(exe->heap);
		else stack->values[stack->tp-1]=newNilValue(exe->heap);
	}
	else
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	runnable->cp+=1;
}

void B_not(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* tmpValue=getTopValue(stack);
	if(tmpValue->type==NIL||(tmpValue->type==PAIR&&getVMpairCar(tmpValue)->type==NIL&&getVMpairCdr(tmpValue)->type==NIL))
		stack->values[stack->tp-1]=newTrueValue(exe->heap);
	else
		stack->values[stack->tp-1]=newNilValue(exe->heap);
	runnable->cp+=1;
}

void B_chr(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* topValue=getTopValue(stack);
	if(topValue->type==PAIR||topValue->type==PRC)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	VMvalue* tmpValue=newVMvalue(CHR,NULL,exe->heap,1);
	switch(topValue->type)
	{
		case IN32:
			*tmpValue->u.chr=*topValue->u.in32;
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
	runnable->cp+=1;
}

void B_int(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* topValue=getTopValue(stack);
	if(topValue->type==PAIR||topValue->type==PRC)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	VMvalue* tmpValue=newVMvalue(IN32,NULL,exe->heap,1);
	int32_t m=0;
	switch(topValue->type)
	{
		case IN32:
			*tmpValue->u.in32=*topValue->u.in32;
			break;
		case DBL:
			*tmpValue->u.in32=(int32_t)*topValue->u.dbl;
			break;
		case CHR:
			*tmpValue->u.in32=*topValue->u.chr;
			break;
		case STR:
		case SYM:
			*tmpValue->u.in32=stringToInt(topValue->u.str->str);
			break;
		case BYTS:
			memcpy(&m,topValue->u.byts->str,topValue->u.byts->size);
			*tmpValue->u.in32=m;
			break;
	}
	stack->values[stack->tp-1]=tmpValue;
	runnable->cp+=1;
}

void B_dbl(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* topValue=getTopValue(stack);
	if(topValue->type==PAIR||topValue->type==PRC)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	VMvalue* tmpValue=newVMvalue(DBL,NULL,exe->heap,1);
	double d=0;
	switch(topValue->type)
	{
		case IN32:
			*tmpValue->u.dbl=(double)*topValue->u.in32;
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
	runnable->cp+=1;
}

void B_str(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* topValue=getTopValue(stack);
	if(topValue->type==PAIR||topValue->type==PRC)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	VMstr* tmpStr=newVMstr(NULL);
	VMvalue* tmpValue=newVMvalue(STR,tmpStr,exe->heap,1);
	switch(topValue->type)
	{
		case IN32:
			tmpValue->u.str->str=intToString(*topValue->u.in32);
			break;
		case DBL:
			tmpValue->u.str->str=doubleToString(*topValue->u.dbl);
			break;
		case CHR:
			tmpValue->u.str->str=(char*)malloc(sizeof(char)*2);
			FAKE_ASSERT(tmpValue->u.str->str,"B_str",__FILE__,__LINE__);
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
	runnable->cp+=1;
}

void B_sym(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* topValue=getTopValue(stack);
	if(topValue->type==PAIR||topValue->type==PRC)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	VMstr* tmpStr=newVMstr(NULL);
	VMvalue* tmpValue=newVMvalue(SYM,tmpStr,exe->heap,1);
	switch(topValue->type)
	{
		case IN32:
			tmpValue->u.str->str=intToString((long)*topValue->u.in32);
			break;
		case DBL:
			tmpValue->u.str->str=doubleToString(*topValue->u.dbl);
			break;
		case CHR:
			tmpValue->u.str->str=(char*)malloc(sizeof(char)*2);
			FAKE_ASSERT(tmpValue->u.str->str,"B_sym",__FILE__,__LINE__);
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
	runnable->cp+=1;
}

void B_byte(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* topValue=getTopValue(stack);
	if(topValue->type==PAIR||topValue->type==PRC)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	VMvalue* tmpValue=newVMvalue(BYTS,NULL,exe->heap,1);
	tmpValue->u.byts=newEmptyByteArry();
	switch(topValue->type)
	{
		case IN32:
			tmpValue->u.byts->size=sizeof(int32_t);
			tmpValue->u.byts->str=createByteString(sizeof(int32_t));
			*(int32_t*)tmpValue->u.byts->str=*topValue->u.in32;
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
	runnable->cp+=1;
}

void B_nth(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* place=getValue(stack,stack->tp-2);
	VMvalue* objlist=getTopValue(stack);
	if((objlist->type!=PAIR&&objlist->type!=STR&&objlist->type!=BYTS)||place->type!=IN32)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	if(objlist->type==PAIR)
	{
		VMvalue* obj=getVMpairCar(objlist);
		VMvalue* objPair=getVMpairCdr(objlist);
		int i=0;
		for(;i<*place->u.in32;i++)
		{
			if(objPair->type!=PAIR)
			{
				obj=newNilValue(exe->heap);
				break;
			}
			obj=getVMpairCar(objPair);
			objPair=getVMpairCdr(objPair);
		}
		stack->tp-=1;
		stack->values[stack->tp-1]=obj;
		stackRecycle(exe);
	}
	else if(objlist->type==STR)
	{
		stack->tp-=1;
		stackRecycle(exe);
		VMvalue* objChr=newVMvalue(CHR,objlist->u.str->str+*place->u.in32,exe->heap,0);
		stack->values[stack->tp-1]=objChr;
	}
	else if(objlist->type==BYTS)
	{
		stack->tp-=1;
		stackRecycle(exe);
		ByteString* tmpByte=newByteString(1,NULL);
		tmpByte->str=objlist->u.byts->str+*place->u.in32;
		VMvalue* objByte=newVMvalue(BYTS,tmpByte,exe->heap,0);
		stack->values[stack->tp-1]=objByte;
	}
	runnable->cp+=1;
}

void B_length(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* objlist=getTopValue(stack);
	if(objlist->type!=PAIR&&objlist->type!=STR&&objlist->type!=BYTS&&objlist->type!=CHAN)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	if(objlist->type==PAIR)
	{
		int32_t i=0;
		for(VMvalue* tmp=objlist;tmp!=NULL&&tmp->type==PAIR;tmp=getVMpairCdr(tmp))
			if(!(getVMpairCar(tmp)->type==NIL&&getVMpairCdr(tmp)->type==NIL))
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
	{
		int32_t i=getNumVMChanl((volatile VMChanl*)objlist->u.chan);
		stack->values[stack->tp-1]=newVMvalue(IN32,&i,exe->heap,1);
	}
	runnable->cp+=1;
}

void B_appd(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* fir=getTopValue(stack);
	VMvalue* sec=getValue(stack,stack->tp-2);
	if(sec->type!=NIL&&sec->type!=PAIR&&sec->type!=STR&&sec->type!=BYTS)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	if(sec->type==PAIR)
	{
		VMvalue* copyOfsec=copyVMvalue(sec,exe->heap);
		VMvalue* lastpair=copyOfsec;
		while(getVMpairCdr(lastpair)->type==PAIR)lastpair=getVMpairCdr(lastpair);
		lastpair->u.pair->cdr=fir;
		stack->tp-=1;
		stackRecycle(exe);
		stack->values[stack->tp-1]=copyOfsec;
	}
	else if(sec->type==STR)
	{
		if(fir->type!=STR)
			RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
		int32_t firlen=strlen(fir->u.str->str);
		int32_t seclen=strlen(sec->u.str->str);
		char* tmpStr=(char*)malloc(sizeof(char)*(firlen+seclen+1));
		FAKE_ASSERT(tmpStr,"B_appd",__FILE__,__LINE__);
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
	runnable->cp+=1;
}

void B_read(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* file=getTopValue(stack);
	if(file->type!=FP)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	FILE* tmpFile=file->u.fp->fp;
	int unexpectEOF=0;
	char* prev=NULL;
	char* tmpString=readInPattern(tmpFile,&prev,&unexpectEOF);
	if(prev)
		free(prev);
	if(unexpectEOF)
	{
		free(tmpString);
		RAISE_BUILTIN_ERROR(UNEXPECTEOF,runnable,exe);
	}
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
	runnable->cp+=1;
}

void B_getb(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* file=getTopValue(stack);
	VMvalue* size=getValue(stack,stack->tp-2);
	if(file->type!=FP||size->type!=IN32)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	FILE* fp=file->u.fp->fp;
	uint8_t* str=(uint8_t*)malloc(sizeof(uint8_t)*(*size->u.in32));
	FAKE_ASSERT(str,"B_getb",__FILE__,__LINE__);
	int32_t realRead=0;
	realRead=fread(str,sizeof(uint8_t),*size->u.in32,fp);
	stack->tp-=1;
	if(!realRead)
	{
		free(str);
		stack->values[stack->tp-1]=newNilValue(exe->heap);
	}
	else
	{
		str=(uint8_t*)realloc(str,sizeof(uint8_t)*realRead);
		FAKE_ASSERT(str,"B_getb",__FILE__,__LINE__);
		VMvalue* tmpBary=newVMvalue(BYTS,NULL,exe->heap,1);
		tmpBary->u.byts=newEmptyByteArry();
		tmpBary->u.byts->size=*size->u.in32;
		tmpBary->u.byts->str=str;
		stack->values[stack->tp-1]=tmpBary;
	}
	stackRecycle(exe);
	runnable->cp+=1;
}

void B_write(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* file=getTopValue(stack);
	VMvalue* obj=getValue(stack,stack->tp-2);
	if(file->type!=FP)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	FILE* objFile=file->u.fp->fp;
	stack->tp-=1;
	stackRecycle(exe);
	CRL* head=NULL;
	writeVMvalue(obj,objFile,&head);
	while(head)
	{
		CRL* prev=head;
		head=head->next;
		free(prev);
	}
	runnable->cp+=1;
}

void B_putb(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* file=getTopValue(stack);
	VMvalue* bt=getValue(stack,stack->tp-2);
	if(file->type!=FP||bt->type!=BYTS)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	FILE* objFile=file->u.fp->fp;
	stack->tp-=1;
	stackRecycle(exe);
	fwrite(bt->u.byts->str,sizeof(uint8_t),bt->u.byts->size,objFile);
	runnable->cp+=1;
}

void B_princ(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* file=getTopValue(stack);
	VMvalue* obj=getValue(stack,stack->tp-2);
	if(file->type!=FP)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	FILE* objFile=file->u.fp->fp;
	stack->tp-=1;
	stackRecycle(exe);
	CRL* head=NULL;
	princVMvalue(obj,objFile,&head);
	while(head)
	{
		CRL* prev=head;
		head=head->next;
		free(prev);
	}
	runnable->cp+=1;
}

void B_go(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* threadArg=getTopValue(stack);
	VMvalue* threadProc=getValue(stack,stack->tp-2);
	if(threadProc->type!=PRC||(threadArg->type!=PAIR&&threadArg->type!=NIL))
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	if(exe->VMid==-1)
		RAISE_BUILTIN_ERROR(CANTCREATETHREAD,runnable,exe);
	FakeVM* threadVM=newThreadVM(threadProc->u.prc,exe->heap);
	threadVM->callback=exe->callback;
	threadVM->table=exe->table;
	threadVM->lnt=exe->lnt;
	threadVM->code=exe->code;
	threadVM->size=exe->size;
	VMstack* threadVMstack=threadVM->stack;
	VMvalue* prevBp=newVMvalue(IN32,&threadVMstack->bp,exe->heap,1);
	if(threadVMstack->tp>=threadVMstack->size)
	{
		threadVMstack->values=(VMvalue**)realloc(threadVMstack->values,sizeof(VMvalue*)*(threadVMstack->size+64));
		FAKE_ASSERT(threadVMstack->values,"B_go",__FILE__,__LINE__);
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
			FAKE_ASSERT(threadVMstack->values,"B_go",__FILE__,__LINE__);
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
	VMChanl* chan=threadVM->chan;
	stack->tp-=1;
	int32_t faildCode=pthread_create(&threadVM->tid,NULL,ThreadVMFunc,threadVM);
	if(faildCode)
	{
		threadVM->chan->refcount-=1;
		freeVMChanl(threadVM->chan);
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
		VMvalue* threadVMChanl=newVMvalue(CHAN,chan,exe->heap,1);
		stack->values[stack->tp-1]=threadVMChanl;
	}
	runnable->cp+=1;
}

void B_chanl(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* maxSize=getTopValue(stack);
	if(maxSize->type!=IN32)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	int32_t max=*maxSize->u.in32;
	VMvalue* objValue=newVMvalue(CHAN,newVMChanl(max),exe->heap,1);
	stack->values[stack->tp-1]=objValue;
	runnable->cp+=1;
}

void B_send(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* ch=getTopValue(stack);
	if(ch->type!=CHAN)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	VMvalue* message=getValue(stack,stack->tp-2);
	VMChanl* tmpCh=ch->u.chan;
	VMvalue* tmp=newNilValue(exe->heap);
	tmp->access=1;
	copyRef(tmp,message);
	SendT* t=newSendT(tmp);
	chanlSend(t,tmpCh);
	stack->tp-=1;
	stackRecycle(exe);
	runnable->cp+=1;
}

void B_recv(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* ch=getTopValue(stack);
	if(ch->type!=CHAN)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	VMChanl* tmpCh=ch->u.chan;
	RecvT* t=newRecvT(exe);
	chanlRecv(t,tmpCh);
	runnable->cp+=1;
}

void B_error(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* message=getTopValue(stack);
	VMvalue* type=getValue(stack,stack->tp-2);
	if(type->type!=SYM||message->type!=STR)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	VMerror* err=newVMerror(type->u.str->str,message->u.str->str);
	VMvalue* toReturn=newVMvalue(ERR,err,exe->heap,1);
	stack->tp-=1;
	stackRecycle(exe);
	stack->values[stack->tp-1]=toReturn;
	runnable->cp+=1;
}

void B_raise(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* err=getTopValue(stack);
	if(err->type!=ERR)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	increaseVMerrorRefcount(err->u.err);
	raiseVMerror(err->u.err,exe);
	runnable->cp+=1;
}

void B_push_try(FakeVM* exe)
{
	VMrunnable* r=topComStack(exe->rstack);
	int32_t cpc=0;
	char* errSymbol=(char*)(exe->code+r->cp+(++cpc));
	VMTryBlock* tb=newVMTryBlock(errSymbol,exe->stack->tp);
	cpc+=strlen(errSymbol)+1;
	int32_t handlerNum=*(exe->code+r->cp+cpc);
	cpc+=sizeof(int32_t);
	unsigned int i=0;
	for(;i<handlerNum;i++)
	{
		char* type=(char*)exe->code+r->cp+cpc;
		cpc+=strlen(type)+1;
		uint32_t pCpc=*(exe->code+r->cp+cpc);
		cpc+=sizeof(int32_t);
		VMproc* p=newVMproc(r->cp+cpc,pCpc);
		VMerrorHandler* h=newVMerrorHandler(type,p);
		pushComStack(h,tb->hstack);
		cpc+=pCpc;
	}
	pushComStack(tb,exe->tstack);
	r->cp+=cpc;
}

void B_pop_try(FakeVM* exe)
{
	VMrunnable* r=topComStack(exe->rstack);
	VMTryBlock* tb=popComStack(exe->tstack);
	freeVMTryBlock(tb);
	r->cp+=1;
}

VMstack* newVMstack(int32_t size)
{
	VMstack* tmp=(VMstack*)malloc(sizeof(VMstack));
	FAKE_ASSERT(tmp,"newVMstack",__FILE__,__LINE__);
	tmp->size=size;
	tmp->tp=0;
	tmp->bp=0;
	tmp->values=(VMvalue**)malloc(size*sizeof(VMvalue*));
	FAKE_ASSERT(tmp->values,"newVMstack",__FILE__,__LINE__);
	tmp->tpsi=0;
	tmp->tptp=0;
	tmp->tpst=NULL;
	return tmp;
}

void writeVMvalue(VMvalue* objValue,FILE* fp,CRL** h)
{
	VMpair* cirPair=NULL;
	int8_t isInCir=0;
	int32_t CRLcount=-1;
	switch(objValue->type)
	{
		case NIL:
			fprintf(fp,"nil");
			break;
		case IN32:
			fprintf(fp,"%d",*objValue->u.in32);
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
			fprintf(fp,"#<proc>");
			break;
		case PAIR:
			cirPair=isCircularReference(objValue->u.pair,*h);
			if(cirPair)
				isInCir=isInTheCircle(objValue->u.pair,cirPair,cirPair);
			if(cirPair&&isInCir)
			{
				CRL* crl=newCRL(objValue->u.pair,(*h)?(*h)->count+1:0);
				crl->next=*h;
				*h=crl;
				fprintf(fp,"#%d=(",crl->count);
			}
			else
				putc('(',fp);
			for(;objValue->type==PAIR;objValue=getVMpairCdr(objValue))
			{
				VMvalue* tmpValue=getVMpairCar(objValue);
				if(tmpValue->type==PAIR&&(CRLcount=findCRLcount(tmpValue->u.pair,*h))!=-1)
					fprintf(fp,"#%d#",CRLcount);
				else
					if(tmpValue->type!=NIL||objValue->u.pair->cdr->type!=NIL)
						writeVMvalue(tmpValue,fp,h);
				tmpValue=getVMpairCdr(objValue);
				if(tmpValue->type>NIL&&tmpValue->type<PAIR)
				{
					putc(',',fp);
					writeVMvalue(tmpValue,fp,h);
				}
				else if(tmpValue->type==PAIR&&(CRLcount=findCRLcount(tmpValue->u.pair,*h))!=-1)
				{
					fprintf(fp,",#%d#",CRLcount);
					break;
				}
				else if(tmpValue->type!=NIL)
					putc(' ',fp);
			}
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
		case DLL:
			fprintf(fp,"<#dll>");
			break;
		case DLPROC:
			fprintf(fp,"<#dlproc>");
			break;
		default:fprintf(fp,"Bad value!");break;
	}
}

void princVMvalue(VMvalue* objValue,FILE* fp,CRL** h)
{
	VMpair* cirPair=NULL;
	int32_t CRLcount=-1;
	int8_t isInCir=0;
	switch(objValue->type)
	{
		case NIL:
			fprintf(fp,"nil");
			break;
		case IN32:
			fprintf(fp,"%d",*objValue->u.in32);
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
			cirPair=isCircularReference(objValue->u.pair,*h);
			if(cirPair)
				isInCir=isInTheCircle(objValue->u.pair,cirPair,cirPair);
			if(cirPair&&isInCir)
			{
				CRL* crl=newCRL(objValue->u.pair,(*h)?(*h)->count+1:0);
				crl->next=*h;
				*h=crl;
				fprintf(fp,"#%d=(",crl->count);
			}
			else
				putc('(',fp);
			for(;objValue->type==PAIR;objValue=getVMpairCdr(objValue))
			{
				VMvalue* tmpValue=getVMpairCar(objValue);
				if(tmpValue->type==PAIR&&(CRLcount=findCRLcount(tmpValue->u.pair,*h))!=-1)
					fprintf(fp,"#%d#",CRLcount);
				else
					if(tmpValue->type!=NIL||objValue->u.pair->cdr->type!=NIL)
						princVMvalue(tmpValue,fp,h);
				tmpValue=getVMpairCdr(objValue);
				if(tmpValue->type>NIL&&tmpValue->type<PAIR)
				{
					putc(',',fp);
					princVMvalue(tmpValue,fp,h);
				}
				else if(tmpValue->type==PAIR&&(CRLcount=findCRLcount(tmpValue->u.pair,*h))!=-1)
				{
					fprintf(fp,",#%d#",CRLcount);
					break;
				}
				else if(tmpValue->type!=NIL)
					putc(' ',fp);
			}
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
		case DLL:
			fprintf(fp,"<#dll>");
			break;
		case DLPROC:
			fprintf(fp,"<#dlproc>");
			break;
		default:fprintf(fp,"Bad value!");break;
	}
}

void stackRecycle(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* currunnable=topComStack(exe->rstack);
	if(stack->size-stack->tp>64)
	{
		stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size-64));
		if(stack->values==NULL)
		{
			fprintf(stderr,"stack->tp==%d,stack->size==%d\n",stack->tp,stack->size);
			fprintf(stderr,"cp=%d\n%s\n",currunnable->cp,codeName[(uint8_t)exe->code[currunnable->cp]].codeName);
			FAKE_ASSERT(stack->values,"stackRecycle",__FILE__,__LINE__);
		}
		stack->size-=64;
	}
}

void DBG_printVMstack(VMstack* stack,FILE* fp,int mode)
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
			CRL* head=NULL;
			writeVMvalue(tmp,fp,&head);
			while(head)
			{
				CRL* prev=head;
				head=head->next;
				free(prev);
			}
			putc('\n',fp);
		}
	}
}

void DBG_printVMvalue(VMvalue* v,FILE* fp)
{
	CRL* head=NULL;
	writeVMvalue(v,fp,&head);
	while(head)
	{
		CRL* prev=head;
		head=head->next;
		free(prev);
	}
}

void DBG_printVMByteCode(uint8_t* code,uint32_t s,uint32_t c,FILE* fp)
{
	ByteCode t={c,code+s};
	printByteCode(&t,fp);
}

VMrunnable* hasSameProc(VMproc* objProc,ComStack* rstack)
{
	size_t i=0;
	for(;i<rstack->top;i++)
	{
		VMrunnable* cur=rstack->data[i];
		if(cur->proc==objProc)
			return cur;
	}
	return NULL;
}

int isTheLastExpress(const VMrunnable* runnable,const VMrunnable* same,const FakeVM* exe)
{
	uint32_t j=0;
	size_t c=exe->rstack->top;
	if(same==NULL)return 0;
	uint32_t size=0;
	for(;;)
	{
		uint8_t* code=exe->code;
		uint32_t i=runnable->cp+1;
		size=runnable->proc->scp+runnable->proc->cpc;
		if(code[i]==FAKE_JMP)
		{
			int32_t where=*(int32_t*)(code+i+1);
			i+=where+5;
		}
		j=i;
		for(;i<size;i++)
		{
			if(code[i]!=FAKE_POP_TP)
				return 0;
		}
		if(runnable==same)break;
		runnable=exe->rstack->data[c-1];
		c--;
	}
	exe->stack->tptp-=size-j;
	return 1;
}

void DBG_printVMenv(VMenv* curEnv,FILE* fp)
{
	if(curEnv->num==0)
		fprintf(fp,"This ENV is empty!");
	else
	{
		fprintf(fp,"ENV:");
		for(int i=0;i<curEnv->num;i++)
		{
			VMvalue* tmp=curEnv->list[i]->value;
			CRL* head=NULL;
			writeVMvalue(tmp,fp,&head);
			while(head)
			{
				CRL* prev=head;
				head=head->next;
				free(prev);
			}
			putc(' ',fp);
		}
	}
}

VMheap* newVMheap()
{
	VMheap* tmp=(VMheap*)malloc(sizeof(VMheap));
	FAKE_ASSERT(tmp,"newVMheap",__FILE__,__LINE__);
	tmp->num=0;
	tmp->threshold=THRESHOLD_SIZE;
	tmp->head=NULL;
	pthread_mutex_init(&tmp->lock,NULL);
	return tmp;
}

void GC_mark(FakeVM* exe)
{
	GC_markValueInStack(exe->stack);
	GC_markValueInCallChain(exe->rstack);
	if(exe->chan)
	{
		pthread_mutex_lock(&exe->chan->lock);
		GC_markMessage(exe->chan->messages->head);
		GC_markSendT(exe->chan->sendq->head);
		pthread_mutex_unlock(&exe->chan->lock);
	}
}

void GC_markSendT(QueueNode* head)
{
	for(;head;head=head->next)
		GC_markValue(((SendT*)head->data)->m);
}

void GC_markValue(VMvalue* obj)
{
	ComStack* stack=newComStack(32);
	pushComStack(obj,stack);
	while(!isComStackEmpty(stack))
	{
		VMvalue* root=popComStack(stack);
		if(!root->mark)
		{
			root->mark=1;
			if(root->type==PAIR)
			{
				pushComStack(getVMpairCar(root),stack);
				pushComStack(getVMpairCdr(root),stack);
			}
			else if(root->type==PRC)
			{
				VMenv* curEnv=root->u.prc->prevEnv;
				for(;curEnv!=NULL;curEnv=curEnv->prev)
				{
					uint32_t i=0;
					for(;i<curEnv->num;i++)
						pushComStack(curEnv->list[i]->value,stack);
				}
			}
			else if(root->type==CONT)
			{
				uint32_t i=0;
				for(;i<root->u.cont->stack->tp;i++)
					pushComStack(root->u.cont->stack->values[i],stack);
				for(i=0;i<root->u.cont->num;i++)
				{
					VMenv* env=root->u.cont->status[i].env;
					uint32_t j=0;
					for(;j<env->num;j++)
						pushComStack(env->list[i]->value,stack);
				}
			}
			else if(root->type==CHAN)
			{
				pthread_mutex_lock(&root->u.chan->lock);
				QueueNode* head=root->u.chan->messages->head;
				for(;head;head=head->next)
					pushComStack(head->data,stack);
				for(head=root->u.chan->sendq->head;head;head=head->next)
					pushComStack(((SendT*)head->data)->m,stack);
				pthread_mutex_unlock(&root->u.chan->lock);
			}
		}
	}
	freeComStack(stack);
}

void GC_markValueInEnv(VMenv* curEnv)
{
	int32_t i=0;
	for(;i<curEnv->num;i++)
		GC_markValue(curEnv->list[i]->value);
}

void GC_markValueInCallChain(ComStack* rstack)
{
	size_t i=0;
	for(;i<rstack->top;i++)
	{
		VMrunnable* cur=rstack->data[i];
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

void GC_markMessage(QueueNode* head)
{
	while(head!=NULL)
	{
		GC_markValue(head->data);
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
			if(cur==heap->head)
				heap->head=cur->next;
			if(cur->next!=NULL)cur->next->prev=cur->prev;
			if(cur->prev!=NULL)cur->prev->next=cur->next;
			freeRef(cur);
			cur=cur->next;
			free(prev);
			heap->num-=1;
		}
		else
		{
			cur->mark=0;
			cur=cur->next;
		}
	}
}

FakeVM* newThreadVM(VMproc* mainCode,VMheap* heap)
{
	FakeVM* exe=(FakeVM*)malloc(sizeof(FakeVM));
	FAKE_ASSERT(exe,"newThreadVM",__FILE__,__LINE__);
	VMrunnable* t=newVMrunnable(mainCode);
	t->localenv=newVMenv(mainCode->prevEnv);
	exe->rstack=newComStack(32);
	pushComStack(t,exe->rstack);
	exe->mark=1;
	exe->argc=0;
	exe->argv=NULL;
	exe->chan=newVMChanl(0);
	exe->tstack=newComStack(32);
	exe->stack=newVMstack(0);
	exe->heap=heap;
	exe->callback=NULL;
	FakeVM** ppFakeVM=NULL;
	int i=0;
	for(;i<GlobFakeVMs.num;i++)
		if(GlobFakeVMs.VMs[i]==NULL)
			ppFakeVM=GlobFakeVMs.VMs+i;
	if(ppFakeVM!=NULL)
	{
		exe->VMid=i;
		*ppFakeVM=exe;
	}
	else
	{
		int32_t size=GlobFakeVMs.num;
		GlobFakeVMs.VMs=(FakeVM**)realloc(GlobFakeVMs.VMs,sizeof(FakeVM*)*(size+1));
		FAKE_ASSERT(!size||GlobFakeVMs.VMs,"newThreadVM",__FILE__,__LINE__);
		GlobFakeVMs.VMs[size]=exe;
		GlobFakeVMs.num+=1;
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

void freeAllVMs()
{
	int i=1;
	FakeVM* cur=GlobFakeVMs.VMs[0];
	freeComStack(cur->tstack);
	freeComStack(cur->rstack);
	freeVMstack(cur->stack);
	free(cur->code);
	free(cur);
	for(;i<GlobFakeVMs.num;i++)
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
	for(;i<GlobFakeVMs.num;i++)
	{
		FakeVM* cur=GlobFakeVMs.VMs[i];
		if(cur)
			pthread_join(cur->tid,NULL);
	}
}

void cancelAllThread()
{
	int i=1;
	for(;i<GlobFakeVMs.num;i++)
	{
		FakeVM* cur=GlobFakeVMs.VMs[i];
		if(cur)
		{
			pthread_cancel(cur->tid);
			pthread_join(cur->tid,NULL);
			freeVMChanl(cur->chan);
			deleteCallChain(cur);
			freeVMstack(cur->stack);
			freeComStack(cur->rstack);
			freeComStack(cur->tstack);
		}
	}
}

void deleteCallChain(FakeVM* exe)
{
	while(!isComStackEmpty(exe->rstack))
	{
		VMrunnable* cur=popComStack(exe->rstack);
		freeVMenv(cur->localenv);
		freeVMproc(cur->proc);
		free(cur);
	}
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
			FAKE_ASSERT(tmpStack->values,"createCallChainWithContinuation",__FILE__,__LINE__);
			tmpStack->size+=64;
		}
		tmpStack->values[tmpStack->tp]=stack->values[i];
		tmpStack->tp+=1;
	}
	deleteCallChain(vm);
	vm->stack=tmpStack;
	freeVMstack(stack);
	for(i=0;i<cc->num;i++)
	{
		VMrunnable* cur=(VMrunnable*)malloc(sizeof(VMrunnable));
		FAKE_ASSERT(cur,"createCallChainWithContinuation",__FILE__,__LINE__);
		cur->cp=cc->status[i].cp;
		cur->localenv=cc->status[i].env;
		increaseVMenvRefcount(cur->localenv);
		cur->proc=cc->status[i].proc;
		increaseVMprocRefcount(cur->proc);
		pushComStack(cur,vm->rstack);
	}
}

int32_t getSymbolIdInByteCode(const uint8_t* code)
{
	char op=*code;
	switch(op)
	{
		case FAKE_PUSH_VAR:
			return *(int32_t*)(code+sizeof(char));
			break;
		case FAKE_POP_VAR:
		case FAKE_POP_ARG:
		case FAKE_POP_REST_ARG:
			return *(int32_t*)(code+sizeof(char)+sizeof(int32_t));
			break;
	}
	return -1;
}
