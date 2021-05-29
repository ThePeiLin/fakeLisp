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

static int envNodeCmp(const void* a,const void* b)
{
	return ((*(VMenvNode**)a)->id-(*(VMenvNode**)b)->id);
}

static int32_t getSymbolIdInByteCode(const char*);
extern char* builtInSymbolList[NUMOFBUILTINSYMBOL];
pthread_rwlock_t GClock=PTHREAD_RWLOCK_INITIALIZER;
FakeVMlist GlobFakeVMs={0,NULL};

static CRL* newCRL(VMpair* pair,int32_t count)
{
	CRL* tmp=(CRL*)malloc(sizeof(CRL));
	if(!tmp)
		errors("newCRL",__FILE__,__LINE__);
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
	B_cast_to_chr,
	B_cast_to_int,
	B_cast_to_dbl,
	B_cast_to_str,
	B_cast_to_sym,
	B_cast_to_byte,
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
};

FakeVM* newFakeVM(ByteCode* mainproc)
{
	FakeVM* exe=(FakeVM*)malloc(sizeof(FakeVM));
	if(exe==NULL)errors("newFakeVM",__FILE__,__LINE__);
	VMcode* tmpVMcode=NULL;
	if(mainproc!=NULL)
	{
		tmpVMcode=newVMcode(mainproc->code,mainproc->size,0);
		exe->mainproc=newFakeProcess(tmpVMcode,NULL);
		freeVMcode(tmpVMcode);
	}
	exe->argc=0;
	exe->argv=NULL;
	exe->curproc=exe->mainproc;
	exe->mark=1;
	exe->chan=NULL;
	exe->stack=newVMstack(0);
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

FakeVM* newTmpFakeVM(ByteCode* mainproc)
{
	FakeVM* exe=(FakeVM*)malloc(sizeof(FakeVM));
	if(exe==NULL)errors("newTmpFakeVM",__FILE__,__LINE__);
	if(mainproc!=NULL)
		exe->mainproc=newFakeProcess(newVMcode(mainproc->code,mainproc->size,0),NULL);
	exe->curproc=exe->mainproc;
	exe->mark=1;
	exe->argc=0;
	exe->argv=NULL;
	exe->chan=NULL;
	exe->stack=newVMstack(0);
	exe->heap=newVMheap();
	exe->callback=NULL;
	exe->VMid=-1;
	return exe;
}

void initGlobEnv(VMenv* obj,VMheap* heap,SymbolTable* table)
{
	obj->size=NUMOFBUILTINSYMBOL;
	obj->list=(VMenvNode**)malloc(sizeof(VMenvNode*)*NUMOFBUILTINSYMBOL);
	if(obj->list==NULL)errors("initGlobEnv",__FILE__,__LINE__);
	int32_t tmpInt=EOF;
	obj->list[0]=newVMenvNode(newNilValue(heap),findSymbol(builtInSymbolList[0],table)->id);
	obj->list[1]=newVMenvNode(newVMvalue(IN32,&tmpInt,heap,1),findSymbol(builtInSymbolList[1],table)->id);
	obj->list[2]=newVMenvNode(newVMvalue(FP,newVMfp(stdin),heap,1),findSymbol(builtInSymbolList[2],table)->id);
	obj->list[3]=newVMenvNode(newVMvalue(FP,newVMfp(stdout),heap,1),findSymbol(builtInSymbolList[3],table)->id);
	obj->list[4]=newVMenvNode(newVMvalue(FP,newVMfp(stderr),heap,1),findSymbol(builtInSymbolList[4],table)->id);
	mergeSort(obj->list,obj->size,sizeof(VMenvNode*),envNodeCmp);
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
		freeMessage(tmpCh->head);
		tmpCh->head=newThreadMessage(getTopValue(exe->stack),exe->heap);
		tmpCh->tail=tmpCh->head;
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
			int32_t cp=curproc->cp+curproc->code->offset;
			LineNumTabNode* node=findLineNumTabNode(cp,exe->lnt);
			fprintf(stderr,"In file \"%s\",line %d\n",exe->table->idl[node->fid]->symbol,node->line);
			fprintf(stderr,"%s",codeName[(int)tmpCode->code[curproc->cp]].codeName);
			switch(status)
			{
				case WRONGARG:
					fprintf(stderr,":Wrong arguements.\n");
					break;
				case STACKERROR:
					fprintf(stderr,":Stack error.\n");
					break;
				case TOOMANYARG:
					fprintf(stderr,":Too many arguements.\n");
					break;
				case TOOFEWARG:
					fprintf(stderr,":Too few arguements.\n");
					break;
				case CANTCREATETHREAD:
					fprintf(stderr,":Can't create thread.n");
					break;
				case THREADERROR:
					fprintf(stderr,":Thread error.\n");
					break;
				case SYMUNDEFINE:
					sid=getSymbolIdInByteCode(tmpCode->code+curproc->cp);
					fprintf(stderr," %s",exe->table->idl[sid]->symbol);
					fprintf(stderr,":Symbol is undefined.\n");
					break;
				case INVOKEERROR:
					fprintf(stderr,":Try to invoke \"");
					DBG_printVMvalue(exe->stack->values[exe->stack->tp-1],stderr);
					fprintf(stderr,"\"\n");
					break;
				case LOADDLLFAILD:
					fprintf(stderr,":Faild to load dll:\"%s\"\n",exe->stack->values[exe->stack->tp-1]->u.str->str);
					break;
				case INVALIDSYMBOL:
					fprintf(stderr,":Invalid symbol:%s\n",exe->stack->values[exe->stack->tp-1]->u.str->str);
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
		if(exe->heap->size>exe->heap->threshold)
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
	DBG_printVMstack(exe->stack,stderr,1);
	putc('\n',stderr);
	fprintf(stderr,"stack->tp==%d,stack->size==%d\n",stack->tp,stack->size);
	fprintf(stderr,"cp=%d stack->bp=%d\n%s\n",curproc->cp,stack->bp,codeName[(int)tmpCode->code[curproc->cp]].codeName);
	DBG_printVMenv(exe->curproc->localenv,stderr);
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

int B_push_env_var(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* topValue=getTopValue(stack);
	if(topValue->type!=STR&&topValue->type!=SYM)
		return WRONGARG;
	SymTabNode* stn=findSymbol(topValue->u.str->str,exe->table);
	if(stn==NULL)
		return INVALIDSYMBOL;
	int32_t idOfVar=stn->id;
	VMenv* curEnv=exe->curproc->localenv;
	VMenvNode* tmp=NULL;
	while(curEnv&&!tmp)
	{
		tmp=findVMenvNode(idOfVar,curEnv);
		curEnv=curEnv->prev;
	}
	if(tmp==NULL)
		return INVALIDSYMBOL;
	stack->values[stack->tp-1]=tmp->value;
	proc->cp+=1;
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
	int32_t sizeOfProc=*(int32_t*)(tmpCode->code+proc->cp+1);
	const char* codeStr=tmpCode->code+proc->cp+1+sizeof(int32_t);
	if(stack->tp>=stack->size)
	{
		stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size+64));
		if(stack->values==NULL)errors("B_push_proc",__FILE__,__LINE__);
		stack->size+=64;
	}
	VMcode* code=newVMcode(codeStr,sizeOfProc,proc->cp+1+sizeof(int32_t));
	increaseVMenvRefcount(proc->localenv);
	code->prevEnv=proc->localenv;
	VMvalue* objValue=newVMvalue(PRC,code,exe->heap,1);
	stack->values[stack->tp]=objValue;
	stack->tp+=1;
	proc->cp+=5+sizeOfProc;
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
	if(!(stack->tp>stack->bp))return STACKERROR;
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

int B_pop_arg(FakeVM* exe)
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

int B_pop_rest_arg(FakeVM* exe)
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
		if(stack->tp>stack->bp)
			tmp->u.pair->cdr=newVMvalue(PAIR,newVMpair(exe->heap),exe->heap,1);
		else break;
		tmp=tmp->u.pair->cdr;
	}
	*pValue=obj;
	stackRecycle(exe);
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

int B_pop_env(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMcode* tmpCode=proc->code;
	VMvalue* topValue=getTopValue(stack);
	if(topValue->type!=PRC)
		return WRONGARG;
	VMenv** ppEnv=&topValue->u.prc->prevEnv;
	VMenv* tmpEnv=*ppEnv;
	int32_t i=0;
	int32_t scope=*(int32_t*)(tmpCode->code+proc->cp+1);
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
		return STACKERROR;
	proc->cp+=5;
	return 0;
}

int B_swap(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	if(stack->tp<2)
		return TOOFEWARG;
	VMvalue* topValue=getTopValue(stack);
	VMvalue* otherValue=getValue(stack,stack->tp-2);
	stack->values[stack->tp-1]=otherValue;
	stack->values[stack->tp-2]=topValue;
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
		if(stack->values==NULL)errors("B_pack_cc",__FILE__,__LINE__);
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
	proc->cp+=1;
	return 0;
}

int B_sub(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* firValue=getValue(stack,stack->tp-1);
	VMvalue* secValue=getValue(stack,stack->tp-2);
	if((firValue->type!=IN32&&firValue->type!=DBL)||(secValue->type!=IN32&&secValue->type!=DBL))return WRONGARG;
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
	proc->cp+=1;
	return 0;
}

int B_mul(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* firValue=getValue(stack,stack->tp-1);
	VMvalue* secValue=getValue(stack,stack->tp-2);
	if((firValue->type!=IN32&&firValue->type!=DBL)||(secValue->type!=IN32&&secValue->type!=DBL))return WRONGARG;
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
	if((firValue->type!=IN32&&firValue->type!=DBL)||(secValue->type!=IN32&&secValue->type!=DBL))return WRONGARG;
	if((firValue->type==DBL&&fabs(*firValue->u.dbl)==0)||(firValue->type==IN32&&*firValue->u.in32==0))
		tmpValue=newNilValue(exe->heap);
	else
	{
		double result=((secValue->type==DBL)?*secValue->u.dbl:*secValue->u.in32)/((firValue->type==DBL)?*firValue->u.dbl:*firValue->u.in32);
		tmpValue=newVMvalue(DBL,&result,exe->heap,1);
	}
	stack->tp-=1;
	stackRecycle(exe);
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
	if((firValue->type!=IN32&&firValue->type!=DBL)||(secValue->type!=IN32&&secValue->type!=DBL))return WRONGARG;
	if(!(*firValue->u.in32))
		tmpValue=newNilValue(exe->heap);
	else
	{
		int32_t result=((int32_t)((secValue->type==DBL)?*secValue->u.dbl:*secValue->u.in32))%((int32_t)((firValue->type==DBL)?*firValue->u.dbl:*firValue->u.in32));
		tmpValue=newVMvalue(IN32,&result,exe->heap,1);
	}
	stack->tp-=1;
	stackRecycle(exe);
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
		if(getVMpairCar(topValue)->type==NIL&&getVMpairCdr(topValue)->type==NIL)
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
	if(topValue->type==NIL||(topValue->type==PAIR&&(getVMpairCar(topValue)->type==NIL&&getVMpairCdr(topValue)->type==NIL)))
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
		"dll",
		"dlproc",
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
	stack->bp=*prevBp->u.in32;
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
	if(tmpValue->type!=PRC&&tmpValue->type!=CONT&&tmpValue->type!=DLPROC)return INVOKEERROR;
	if(tmpValue->type==PRC)
	{
		VMcode* tmpCode=tmpValue->u.prc;
		VMprocess* prevProc=hasSameProc(tmpCode,proc);
		if(isTheLastExpress(proc,prevProc)&&prevProc)
			prevProc->cp=0;
		else
		{
			VMprocess* tmpProc=newFakeProcess(tmpCode,proc);
			tmpProc->localenv=newVMenv(tmpCode->prevEnv);
			exe->curproc=tmpProc;
		}
		stack->tp-=1;
		stackRecycle(exe);
		proc->cp+=1;
	}
	else if(tmpValue->type==CONT)
	{
		stack->tp-=1;
		stackRecycle(exe);
		proc->cp+=1;
		VMcontinuation* cc=tmpValue->u.cont;
		createCallChainWithContinuation(exe,cc);
	}
	else
	{
		stack->tp-=1;
		stackRecycle(exe);
		proc->cp+=1;
		DllFunc dllfunc=tmpValue->u.dlproc->func;
		dllfunc(exe,&GClock);
	}
	return 0;
}

int B_jmp_if_true(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMcode* tmpCode=proc->code;
	VMvalue* tmpValue=getTopValue(stack);
	if(!(tmpValue->type==NIL||(tmpValue->type==PAIR&&getVMpairCar(tmpValue)->type==NIL&&getVMpairCdr(tmpValue)->type==NIL)))
	{
		int32_t where=*(int32_t*)(tmpCode->code+proc->cp+sizeof(char));
		proc->cp+=where;
	}
	proc->cp+=5;
	return 0;
}

int B_jmp_if_false(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMcode* tmpCode=proc->code;
	VMvalue* tmpValue=getTopValue(stack);
	stackRecycle(exe);
	if(tmpValue->type==NIL||(tmpValue->type==PAIR&&getVMpairCar(tmpValue)->type==NIL&&getVMpairCdr(tmpValue)->type==NIL))
	{
		int32_t where=*(int32_t*)(tmpCode->code+proc->cp+sizeof(char));
		proc->cp+=where;
	}
	proc->cp+=5;
	return 0;
}

int B_jmp(FakeVM* exe)
{
	VMprocess* proc=exe->curproc;
	VMcode* tmpCode=proc->code;
	int32_t where=*(int32_t*)(tmpCode->code+proc->cp+sizeof(char));
	proc->cp+=where+5;
	return 0;
}

int B_file(FakeVM* exe)
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

int B_dll(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMheap* heap=exe->heap;
	VMvalue* dllName=getTopValue(stack);
	if(dllName->type!=STR&&dllName->type!=SYM)
		return WRONGARG;
	VMDll* dll=newVMDll(dllName->u.str->str);
	if(!dll)
		return LOADDLLFAILD;
	stack->values[stack->tp-1]=newVMvalue(DLL,dll,heap,1);
	proc->cp+=1;
	return 0;
}

int B_dlsym(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMheap* heap=exe->heap;
	VMvalue* symbol=getTopValue(stack);
	VMvalue* dll=getValue(stack,stack->tp-2);
	if((symbol->type!=SYM&&symbol->type!=STR)||dll->type!=DLL)
		return WRONGARG;
	char prefix[]="FAKE_";
	char* realDlFuncName=(char*)malloc(sizeof(char)*(strlen(prefix)+strlen(symbol->u.str->str)+1));
	if(!realDlFuncName)
		errors("B_dlsym",__FILE__,__LINE__);
	sprintf(realDlFuncName,"%s%s",prefix,symbol->u.str->str);
	DllFunc funcAddress=getAddress(realDlFuncName,dll->u.dll->handle);
	if(!funcAddress)
	{
		free(realDlFuncName);
		return INVALIDSYMBOL;
	}
	free(realDlFuncName);
	VMDlproc* dlproc=newVMDlproc(funcAddress,dll->u.dll);
	stack->tp-=1;
	stackRecycle(exe);
	stack->values[stack->tp-1]=newVMvalue(DLPROC,dlproc,heap,1);
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
	else return WRONGARG;
	proc->cp+=1;
	return 0;
}

int B_not(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* tmpValue=getTopValue(stack);
	if(tmpValue->type==NIL||(tmpValue->type==PAIR&&getVMpairCar(tmpValue)->type==NIL&&getVMpairCdr(tmpValue)->type==NIL))
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
			tmpValue->u.str->str=intToString(*topValue->u.in32);
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
			tmpValue->u.str->str=intToString((long)*topValue->u.in32);
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
		while(getVMpairCdr(lastpair)->type==PAIR)lastpair=getVMpairCdr(lastpair);
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
	uint8_t* str=(uint8_t*)malloc(sizeof(uint8_t)*(*size->u.in32));
	if(str==NULL)errors("B_getb",__FILE__,__LINE__);
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
		if(!str)
			errors("B_getb",__FILE__,__LINE__);
		VMvalue* tmpBary=newVMvalue(BYTS,NULL,exe->heap,1);
		tmpBary->u.byts=newEmptyByteArry();
		tmpBary->u.byts->size=*size->u.in32;
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
	CRL* head=NULL;
	writeVMvalue(obj,objFile,0,&head);
	while(head)
	{
		CRL* prev=head;
		head=head->next;
		free(prev);
	}
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
	CRL* head=NULL;
	princVMvalue(obj,objFile,&head);
	while(head)
	{
		CRL* prev=head;
		head=head->next;
		free(prev);
	}
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
	FakeVM* threadVM=newThreadVM(threadProc->u.prc,exe->heap);
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
	int32_t max=*maxSize->u.in32;
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
	while(tmpCh->max>=0&&tmpCh->size>tmpCh->max);
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

VMstack* newVMstack(int32_t size)
{
	VMstack* tmp=(VMstack*)malloc(sizeof(VMstack));
	if(tmp==NULL)errors("newVMstack",__FILE__,__LINE__);
	tmp->size=size;
	tmp->tp=0;
	tmp->bp=0;
	tmp->values=(VMvalue**)malloc(size*sizeof(VMvalue*));
	if(tmp->values==NULL)errors("newVMstack",__FILE__,__LINE__);
	tmp->tpsi=0;
	tmp->tptp=0;
	tmp->tpst=NULL;
	return tmp;
}

void writeVMvalue(VMvalue* objValue,FILE* fp,int8_t mode,CRL** h)
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
			if(mode==1)printProc(objValue->u.prc,fp);
			else fprintf(fp,"#<proc>");
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
						writeVMvalue(tmpValue,fp,mode,h);
				tmpValue=getVMpairCdr(objValue);
				if(tmpValue->type>NIL&&tmpValue->type<PAIR)
				{
					putc(',',fp);
					writeVMvalue(tmpValue,fp,mode,h);
				}
				else if(tmpValue->type==PAIR&&(CRLcount=findCRLcount(tmpValue->u.pair,*h))!=-1)
				{
					fprintf(fp,",#%d#",CRLcount);
					break;
				}
				else
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
				else
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
	VMprocess* curproc=exe->curproc;
	VMcode* tmpCode=curproc->code;
	if(stack->size-stack->tp>64)
	{
		stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size-64));
		if(stack->values==NULL)
		{
			fprintf(stderr,"stack->tp==%d,stack->size==%d\n",stack->tp,stack->size);
			fprintf(stderr,"cp=%d\n%s\n",curproc->cp,codeName[(int)tmpCode->code[curproc->cp]].codeName);
			errors("stackRecycle",__FILE__,__LINE__);
		}
		stack->size-=64;
	}
}

VMcode* newBuiltInProc(ByteCode* proc)
{
	VMcode* tmp=(VMcode*)malloc(sizeof(VMcode));
	if(tmp==NULL)errors("newBuiltInProc",__FILE__,__LINE__);
	tmp->prevEnv=NULL;
	tmp->refcount=0;
	if(proc!=NULL)
	{
		tmp->offset=0;
		tmp->size=proc->size;
		tmp->code=proc->code;
	}
	else
	{
		tmp->offset=0;
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
	increaseVMcodeRefcount(code);
	return tmp;
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
			writeVMvalue(tmp,fp,0,&head);
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
	writeVMvalue(v,fp,0,&head);
	while(head)
	{
		CRL* prev=head;
		head=head->next;
		free(prev);
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
			if((where+proc->cp+5)!=size)return 0;
		}
		else if(proc->cp!=size-1)return 0;
		if(proc==same)break;
		proc=proc->prev;
	}
	return 1;
}

void DBG_printVMenv(VMenv* curEnv,FILE* fp)
{
	if(curEnv->size==0)
		fprintf(fp,"This ENV is empty!");
	else
	{
		fprintf(fp,"ENV:");
		for(int i=0;i<curEnv->size;i++)
		{
			VMvalue* tmp=curEnv->list[i]->value;
			CRL* head=NULL;
			writeVMvalue(tmp,fp,0,&head);
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
					for(;i<curEnv->size;i++)
						pushComStack(curEnv->list[i]->value,stack);
				}
			}
			else if(root->type==CONT)
			{
				uint32_t i=0;
				for(;i<root->u.cont->stack->tp;i++)
					pushComStack(root->u.cont->stack->values[i],stack);
				for(i=0;i<root->u.cont->size;i++)
				{
					VMenv* env=root->u.cont->status[i].env;
					uint32_t j=0;
					for(;j<env->size;j++)
						pushComStack(env->list[i]->value,stack);
				}
			}
			else if(root->type==CHAN)
			{
				pthread_mutex_lock(&root->u.chan->lock);
				ThreadMessage* head=root->u.chan->head;
				for(;head;head=head->next)
					pushComStack(head->message,stack);
				pthread_mutex_unlock(&root->u.chan->lock);
			}
		}
	}
	freeComStack(stack);
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

FakeVM* newThreadVM(VMcode* mainCode,VMheap* heap)
{
	FakeVM* exe=(FakeVM*)malloc(sizeof(FakeVM));
	if(exe==NULL)errors("newThreadVM",__FILE__,__LINE__);
	exe->mainproc=newFakeProcess(mainCode,NULL);
	exe->mainproc->localenv=newVMenv(mainCode->prevEnv);
	exe->curproc=exe->mainproc;
	exe->mark=1;
	exe->argc=0;
	exe->argv=NULL;
	exe->chan=newChanl(1);
	exe->stack=newVMstack(0);
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
		increaseVMenvRefcount(cur->localenv);
		cur->code=cc->status[i].proc;
		increaseVMcodeRefcount(cur->code);
		curproc=cur;
	}
	vm->curproc=curproc;
	while(curproc->prev)
		curproc=curproc->prev;
	vm->mainproc=curproc;
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
		case FAKE_POP_ARG:
		case FAKE_POP_REST_ARG:
			return *(int32_t*)(code+sizeof(char)+sizeof(int32_t));
			break;
	}
	return -1;
}


