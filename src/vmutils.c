#include<fakeLisp/vm.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/ast.h>
#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<math.h>
#include<pthread.h>
#ifndef _WIN32
#include<dlfcn.h>
#else
#include<tchar.h>
#endif

FklVMvalue* fklMakeVMint(int64_t r64,FklVMstack* s,FklVMheap* heap)
{
	if(r64>INT32_MAX||r64<INT32_MIN)
		return fklNewVMvalueToStack(FKL_I64,&r64,s,heap);
	else
		return FKL_MAKE_VM_I32(r64);
}

inline int fklIsFixint(FklVMvalue* p)
{
	return FKL_IS_I32(p)||FKL_IS_I64(p);
}

inline int fklIsInt(FklVMvalue* p)
{
	return FKL_IS_I32(p)||FKL_IS_I64(p)||FKL_IS_BIG_INT(p);
}

inline int fklIsVMnumber(FklVMvalue* p)
{
	return fklIsInt(p)||FKL_IS_F64(p);
}

int fklIsList(FklVMvalue* p)
{
	for(;p!=FKL_VM_NIL;p=p->u.pair->cdr)
		if(!FKL_IS_PAIR(p))
			return 0;
	return 1;
}

inline int64_t fklGetInt(FklVMvalue* p)
{
	return FKL_IS_I32(p)
		?FKL_GET_I32(p)
		:(FKL_IS_I64(p))?p->u.i64
		:fklBigIntToI64(p->u.bigInt);
}

inline double fklGetDouble(FklVMvalue* p)
{
	return FKL_IS_I32(p)?FKL_GET_I32(p)
		:(FKL_IS_I64(p))?p->u.i64
		:(FKL_IS_BIG_INT(p))?fklBigIntToDouble(p->u.bigInt)
		:p->u.f64;
}

FklVMvalue* fklPopVMstack(FklVMstack* stack)
{
	if(!(stack->tp>stack->bp))
		return NULL;
	FklVMvalue* tmp=fklGetTopValue(stack);
	stack->tp-=1;
	return tmp;
}

FklVMvalue* fklTopGet(FklVMstack* stack)
{
	pthread_rwlock_wrlock(&stack->lock);
	FklVMvalue* r=NULL;
	if(!(stack->tp>stack->bp))
		r=NULL;
	else
	{
		FklVMvalue* tmp=fklGetTopValue(stack);
		r=tmp;
	}
	pthread_rwlock_unlock(&stack->lock);
	return r;
}

void fklDecTop(FklVMstack* stack)
{
	pthread_rwlock_wrlock(&stack->lock);
	stack->tp--;
	pthread_rwlock_unlock(&stack->lock);
}

FklVMvalue* fklCastPreEnvToVMenv(FklPreEnv* pe,FklVMvalue* prev,FklVMheap* heap)
{
	int32_t size=0;
	FklPreDef* tmpDef=pe->symbols;
	while(tmpDef)
	{
		size++;
		tmpDef=tmpDef->next;
	}
	FklVMenv* tmp=fklNewVMenv(prev);
	for(tmpDef=pe->symbols;tmpDef;tmpDef=tmpDef->next)
		fklAddVMenvNodeWithV(fklAddSymbolToGlob(tmpDef->symbol)->id,fklCastCptrVMvalue(&tmpDef->obj,heap),tmp);
	return fklNewVMvalue(FKL_ENV,tmp,heap);
}

FklVMstack* fklCopyStack(FklVMstack* stack)
{
	int32_t i=0;
	FklVMstack* tmp=(FklVMstack*)malloc(sizeof(FklVMstack));
	FKL_ASSERT(tmp,__func__);
	pthread_rwlock_rdlock(&stack->lock);
	tmp->size=stack->size;
	tmp->tp=stack->tp;
	tmp->bp=stack->bp;
	pthread_rwlock_init(&tmp->lock,NULL);
	tmp->values=(FklVMvalue**)malloc(sizeof(FklVMvalue*)*(tmp->size));
	FKL_ASSERT(tmp->values,__func__);
	for(;i<stack->tp;i++)
		tmp->values[i]=stack->values[i];
	tmp->tps=fklNewUintStackFromStack(stack->tps);
	tmp->bps=fklNewUintStackFromStack(stack->bps);
	pthread_rwlock_unlock(&stack->lock);
	return tmp;
}

FklVMtryBlock* fklNewVMtryBlock(FklSid_t sid,uint32_t tp,FklVMrunnable* r)
{
	FklVMtryBlock* t=(FklVMtryBlock*)malloc(sizeof(FklVMtryBlock));
	FKL_ASSERT(t,__func__);
	t->sid=sid;
	t->hstack=fklNewPtrStack(32,16);
	t->tp=tp;
	t->curr=r;
	return t;
}

void fklFreeVMtryBlock(FklVMtryBlock* b)
{
	FklPtrStack* hstack=b->hstack;
	while(!fklIsPtrStackEmpty(hstack))
	{
		FklVMerrorHandler* h=fklPopPtrStack(hstack);
		fklFreeVMerrorHandler(h);
	}
	fklFreePtrStack(b->hstack);
	free(b);
}

FklVMerrorHandler* fklNewVMerrorHandler(FklSid_t* typeIds,uint32_t errTypeNum,uint64_t scp,uint64_t cpc)
{
	FklVMerrorHandler* t=(FklVMerrorHandler*)malloc(sizeof(FklVMerrorHandler));
	FKL_ASSERT(t,__func__);
	t->typeIds=typeIds;
	t->num=errTypeNum;
	t->proc.prevEnv=NULL;
	t->proc.scp=scp;
	t->proc.cpc=cpc;
	t->proc.sid=0;
	return t;
}

void fklFreeVMerrorHandler(FklVMerrorHandler* h)
{
	free(h->typeIds);
	free(h);
}

static int isShouldBeHandle(FklSid_t* typeIds,uint32_t num,FklSid_t type)
{
	if(num==0)
		return 1;
	for(uint32_t i=0;i<num;i++)
		if(typeIds[i]==type)
			return 1;
	return 0;
}

int fklRaiseVMerror(FklVMvalue* ev,FklVM* exe)
{
	FklVMerror* err=ev->u.err;
	while(!fklIsPtrStackEmpty(exe->tstack))
	{
		FklVMtryBlock* tb=fklTopPtrStack(exe->tstack);
		FklVMstack* stack=exe->stack;
		stack->tp=tb->tp;
		while(!fklIsPtrStackEmpty(tb->hstack))
		{
			FklVMerrorHandler* h=fklPopPtrStack(tb->hstack);
			if(isShouldBeHandle(h->typeIds,h->num,err->type))
			{
				FklVMrunnable* curr=tb->curr;
				for(FklVMrunnable* other=exe->rhead;other!=curr;)
				{
					FklVMrunnable* r=other;
					other=other->prev;
					free(r);
				}
				exe->rhead=curr;
				FklVMrunnable* prevRunnable=exe->rhead;
				FklVMrunnable* r=fklNewVMrunnable(&h->proc,prevRunnable);
				r->localenv=fklNewSaveVMvalue(FKL_ENV,fklNewVMenv(prevRunnable->localenv));
				fklAddToHeap(r->localenv,exe->heap);
				FklVMvalue* curEnv=r->localenv;
				FklSid_t idOfError=tb->sid;
				fklAddVMenvNode(idOfError,curEnv->u.env)->value=ev;
				fklFreeVMerrorHandler(h);
				exe->rhead=r;
				return 1;
			}
			fklFreeVMerrorHandler(h);
		}
		fklFreeVMtryBlock(fklPopPtrStack(exe->tstack));
	}
	fprintf(stderr,"error of ");
	fklPrintString(err->who,stderr);
	fprintf(stderr," :");
	fklPrintString(err->message,stderr);
	fprintf(stderr,"\n");
	for(FklVMrunnable* cur=exe->rhead;cur;cur=cur->prev)
	{
		if(cur->sid!=0)
		{
			fprintf(stderr,"at proc:");
			fklPrintString(fklGetGlobSymbolWithId(cur->sid)->symbol,stderr);
		}
		else
		{
			if(cur->prev)
				fprintf(stderr,"at <lambda>");
			else
				fprintf(stderr,"at <top>");
		}
		FklLineNumTabNode* node=fklFindLineNumTabNode(cur->cp,exe->lnt);
		if(node->fid)
		{
			fprintf(stderr,"(%u:",node->line);
			fklPrintString(fklGetGlobSymbolWithId(node->fid)->symbol,stderr);
			fprintf(stderr,")\n");
		}
		else
			fprintf(stderr,"(%u)\n",node->line);
	}
	void* i[3]={exe,(void*)255,(void*)1};
	exe->callback(i);
	return 255;
}

FklVMrunnable* fklNewVMrunnable(FklVMproc* code,FklVMrunnable* prev)
{
	FklVMrunnable* tmp=(FklVMrunnable*)malloc(sizeof(FklVMrunnable));
	FKL_ASSERT(tmp,__func__);
	tmp->sid=0;
	tmp->cp=0;
	tmp->scp=0;
	tmp->cpc=0;
	tmp->prev=prev;
	tmp->ccc=NULL;
	if(code)
	{
		tmp->cp=code->scp;
		tmp->scp=code->scp;
		tmp->cpc=code->cpc;
		tmp->sid=code->sid;
	}
	tmp->mark=0;
	return tmp;
}

void fklFreeVMrunnable(FklVMrunnable* runnable)
{
	FklVMcCC* curCCC=runnable->ccc;
	while(curCCC)
	{
		FklVMcCC* cur=curCCC;
		curCCC=cur->next;
		fklFreeVMcCC(cur);
	}
	free(runnable);
}

FklVMcCC* fklNewVMcCC(FklVMFuncK kFunc,void* ctx,size_t size,FklVMcCC* next)
{
	FklVMcCC* r=(FklVMcCC*)malloc(sizeof(FklVMcCC));
	FKL_ASSERT(r,__func__);
	r->kFunc=kFunc;
	r->ctx=ctx;
	r->size=size;
	r->next=next;
	return r;
}

FklVMcCC* fklCopyVMcCC(FklVMcCC* ccc)
{
	FklVMcCC* r=NULL;
	FklVMcCC** pr=&r;
	for(FklVMcCC* curCCC=ccc;curCCC;curCCC=curCCC->next,pr=&(*pr)->next)
		*pr=fklNewVMcCC(curCCC->kFunc,fklCopyMemory(curCCC->ctx,curCCC->size),curCCC->size,NULL);
	return r;
}

void fklFreeVMcCC(FklVMcCC* cc)
{
	free(cc->ctx);
	free(cc);
}

char* fklGenInvalidSymbolErrorMessage(char* str,int _free,FklErrorType type)
{
	char* t=fklCopyCstr("");
	switch(type)
	{
		case FKL_LOADDLLFAILD:
			t=fklStrCat(t,"Faild to load dll \"");
			t=fklStrCat(t,str);
			t=fklStrCat(t,"\" ");
			break;
		case FKL_INVALIDSYMBOL:
			t=fklStrCat(t,"Invalid symbol ");
			t=fklStrCat(t,str);
			t=fklStrCat(t," ");
			break;
		case FKL_FILEFAILURE:
			t=fklStrCat(t,"Failed for file:\"");
			t=fklStrCat(t,str);
			t=fklStrCat(t,"\" ");
		default:
			break;
	}
	if(_free)
		free(str);
	return t;
}

char* fklGenErrorMessage(FklErrorType type,FklVMrunnable* r,FklVM* exe)
{
	char* t=fklCopyCstr("");
	switch(type)
	{
		case FKL_WRONGARG:
			t=fklStrCat(t,"Wrong arguement ");
			break;
		case FKL_STACKERROR:
			t=fklStrCat(t,"Stack error ");
			break;
		case FKL_TOOMANYARG:
			t=fklStrCat(t,"Too many arguements ");
			break;
		case FKL_TOOFEWARG:
			t=fklStrCat(t,"Too few arguements ");
			break;
		case FKL_CANTCREATETHREAD:
			t=fklStrCat(t,"Can't create thread ");
			break;
		case FKL_SYMUNDEFINE:
			t=fklStrCat(t,"Symbol ");
			t=fklCstrStringCat(t,fklGetGlobSymbolWithId(fklGetSymbolIdInByteCode(exe->code+r->cp))->symbol);
			t=fklStrCat(t," is undefined ");
			break;
		case FKL_INVOKEERROR:
			t=fklStrCat(t,"Try to invoke an object that can't be invoke ");
			break;
		case FKL_CROSS_C_CALL_CONTINUATION:
			t=fklStrCat(t,"attempt to get a continuation cross C-call boundary ");
			break;
		case FKL_LOADDLLFAILD:
			t=fklStrCat(t,"Faild to load dll \"");
			{
				FklVMvalue* v=exe->stack->values[exe->stack->tp-1];
				char* str=fklStringToCstr(v->u.str);
				t=fklStrCat(t,str);
				free(str);
			}
			t=fklStrCat(t,"\" ");
			break;
		case FKL_INVALIDSYMBOL:
			t=fklStrCat(t,"Invalid symbol ");
			{
				FklVMvalue* v=exe->stack->values[exe->stack->tp-1];
				char* str=fklStringToCstr(v->u.str);
				t=fklStrCat(t,str);
				free(str);
			}
			t=fklStrCat(t," ");
			break;
		case FKL_DIVZERROERROR:
			t=fklStrCat(t,"Divided by zero ");
			break;
		case FKL_FILEFAILURE:
			t=fklStrCat(t,"Failed for file:\"");
			{
				FklVMvalue* v=exe->stack->values[exe->stack->tp-1];
				char* str=fklStringToCstr(v->u.str);
				t=fklStrCat(t,str);
				free(str);
			}
			t=fklStrCat(t,"\" ");
			break;
		case FKL_INVALIDASSIGN:
			t=fklStrCat(t,"Invalid assign ");
			break;
		case FKL_INVALIDACCESS:
			t=fklStrCat(t,"Invalid access ");
			break;
		case FKL_UNEXPECTEOF:
			t=fklStrCat(t,"Unexpected eof ");
			break;
		case FKL_FAILD_TO_CREATE_BIG_INT_FROM_MEM:
			t=fklStrCat(t,"Failed to create big-int from mem ");
			break;
		case FKL_LIST_DIFFER_IN_LENGTH:
			t=fklStrCat(t,"List differ in length ");
			break;
		default:
			break;
	}
	return t;
}

int32_t fklGetSymbolIdInByteCode(const uint8_t* code)
{
	char op=*code;
	switch(op)
	{
		case FKL_PUSH_VAR:
			return *(int32_t*)(code+sizeof(char));
			break;
		case FKL_POP_VAR:
		case FKL_POP_ARG:
		case FKL_POP_REST_ARG:
			return *(int32_t*)(code+sizeof(char)+sizeof(int32_t));
			break;
	}
	return -1;
}

typedef struct PrtElem
{
	enum PrintingState
	{
		PRT_CAR,
		PRT_CDR,
		PRT_REC_CAR,
		PRT_REC_CDR,
		PRT_BOX,
		PRT_REC_BOX,
	}state;
	FklVMvalue* v;
}PrtElem;

static PrtElem* newPrtElem(enum PrintingState state,FklVMvalue* v)
{
	PrtElem* r=(PrtElem*)malloc(sizeof(PrtElem));
	FKL_ASSERT(r,__func__);
	r->state=state;
	r->v=v;
	return r;
}

static int isInStack(FklVMvalue* v,FklPtrStack* stack,size_t* w)
{
	for(size_t i=0;i<stack->top;i++)
		if(stack->base[i]==v)
		{
			if(w)
				*w=i;
			return 1;
		}
	return 0;
}

static void scanCirRef(FklVMvalue* s,FklPtrStack* recStack)
{
	FklPtrStack* beAccessed=fklNewPtrStack(32,16);
	FklPtrStack* totalAccessed=fklNewPtrStack(32,16);
	fklPushPtrStack(s,beAccessed);
	while(!fklIsPtrStackEmpty(beAccessed))
	{
		FklVMvalue* v=fklPopPtrStack(beAccessed);
		if(FKL_IS_PAIR(v)||FKL_IS_VECTOR(v)||FKL_IS_BOX(v))
		{
			if(isInStack(v,totalAccessed,NULL))
			{
				fklPushPtrStack(v,recStack);
				continue;
			}
			if(isInStack(v,recStack,NULL))
				continue;
			fklPushPtrStack(v,totalAccessed);
			if(FKL_IS_PAIR(v))
			{
				if(v->u.pair->cdr==v)
					fklPushPtrStack(v,recStack);
				else
					fklPushPtrStack(v->u.pair->cdr,beAccessed);
				if(v->u.pair->car==v)
					fklPushPtrStack(v,recStack);
				else
					fklPushPtrStack(v->u.pair->car,beAccessed);
			}
			else if(FKL_IS_VECTOR(v))
			{
				FklVMvec* vec=v->u.vec;
				for(size_t i=vec->size;i>0;i--)
				{
					if(vec->base[i-1]==v)
						fklPushPtrStack(v,recStack);
					else
						fklPushPtrStack(vec->base[i-1],beAccessed);
				}
			}
			else
			{
				if(v->u.box==v)
					fklPushPtrStack(v,recStack);
				else
					fklPushPtrStack(v->u.box,beAccessed);
			}
		}
	}
	fklFreePtrStack(totalAccessed);
	fklFreePtrStack(beAccessed);
}

static void princVMatom(FklVMvalue* v,FILE* fp)
{
	FklVMptrTag tag=FKL_GET_TAG(v);
	switch(tag)
	{
		case FKL_NIL_TAG:
			fprintf(fp,"()");
			break;
		case FKL_I32_TAG:
			fprintf(fp,"%d",FKL_GET_I32(v));
			break;
		case FKL_CHR_TAG:
			putc(FKL_GET_CHR(v),fp);
			break;
		case FKL_SYM_TAG:
			fklPrintString(fklGetGlobSymbolWithId(FKL_GET_SYM(v))->symbol,fp);
			break;
		case FKL_PTR_TAG:
			{
				switch(v->type)
				{
					case FKL_F64:
						fprintf(fp,"%lf",v->u.f64);
						break;
					case FKL_I64:
						fprintf(fp,"%ld",v->u.i64);
						break;
					case FKL_STR:
						fwrite(v->u.str->str,v->u.str->size,1,fp);
						break;
					case FKL_PROC:
						if(v->u.proc->sid)
						{
							fprintf(fp,"<#proc: ");
							fklPrintString(fklGetGlobSymbolWithId(v->u.proc->sid)->symbol,fp);
							fputc('>',fp);
						}
						else
							fprintf(fp,"#<proc>");
						break;
					case FKL_CONT:
						fprintf(fp,"#<cont>");
						break;
					case FKL_CHAN:
						fprintf(fp,"#<chan>");
						break;
					case FKL_FP:
						fprintf(fp,"#<fp>");
						break;
					case FKL_DLL:
						fprintf(fp,"<#dll>");
						break;
					case FKL_DLPROC:
						if(v->u.dlproc->sid)
						{
							fprintf(fp,"<#dlproc: ");
							fklPrintString(fklGetGlobSymbolWithId(v->u.dlproc->sid)->symbol,fp);
							fputc('>',fp);
						}
						else
							fprintf(fp,"<#dlproc>");
						break;
					case FKL_ERR:
						fklPrintString(v->u.err->message,fp);
						break;
					case FKL_BIG_INT:
						fklPrintBigInt(v->u.bigInt,fp);
						break;
					case FKL_USERDATA:
						if(v->u.ud->t->__princ)
							v->u.ud->t->__princ(fp,v->u.ud->data);
						else
						{
							fprintf(fp,"#<");
							fklPrintString(fklGetGlobSymbolWithId(v->u.ud->type)->symbol,fp);
							fprintf(fp,":%p>",v->u.ud);
						}
						break;
					default:
						fputs("#<unknown>",fp);
						break;
				}
			}
		default:
			break;
	}
}

static void prin1VMatom(FklVMvalue* v,FILE* fp)
{
	FklVMptrTag tag=FKL_GET_TAG(v);
	switch(tag)
	{
		case FKL_NIL_TAG:
			fputs("()",fp);
			break;
		case FKL_I32_TAG:
			fprintf(fp,"%d",FKL_GET_I32(v));
			break;
		case FKL_CHR_TAG:
			fklPrintRawChar(FKL_GET_CHR(v),fp);
			break;
		case FKL_SYM_TAG:
			fklPrintRawSymbol(fklGetGlobSymbolWithId(FKL_GET_SYM(v))->symbol,fp);
			break;
		default:
			switch(v->type)
			{
				case FKL_F64:
					fprintf(fp,"%lf",v->u.f64);
					break;
				case FKL_I64:
					fprintf(fp,"%ld",v->u.i64);
					break;
				case FKL_STR:
					fklPrintRawString(v->u.str,fp);
					break;
				case FKL_PROC:
					if(v->u.proc->sid)
					{
						fprintf(fp,"<#proc: ");
						fklPrintString(fklGetGlobSymbolWithId(v->u.proc->sid)->symbol,fp);
						fputc('>',fp);
					}					else
						fputs("#<proc>",fp);
					break;
				case FKL_CONT:
					fputs("#<continuation>",fp);
					break;
				case FKL_CHAN:
					fputs("#<chanl>",fp);
					break;
				case FKL_FP:
					fputs("#<fp>",fp);
					break;
				case FKL_DLL:
					fputs("#<dll>",fp);
					break;
				case FKL_DLPROC:
					if(v->u.dlproc->sid){
						fprintf(fp,"<#dlproc: ");
						fklPrintString(fklGetGlobSymbolWithId(v->u.dlproc->sid)->symbol,fp);
						fputc('>',fp);
					}
					else
						fputs("#<dlproc>",fp);
					break;
				case FKL_ERR:
					fprintf(fp,"#<err w:");
					fklPrintString(v->u.err->who,fp);
					fprintf(fp," t:");
					fklPrintString(fklGetGlobSymbolWithId(v->u.err->type)->symbol,fp);
					fprintf(fp," m:");
					fklPrintString(v->u.err->message,fp);
					fprintf(fp,">");
					break;
				case FKL_BIG_INT:
					fklPrintBigInt(v->u.bigInt,fp);
					break;
				case FKL_USERDATA:
					if(v->u.ud->t->__prin1)
						v->u.ud->t->__prin1(fp,v->u.ud->data);
					else
					{
						fprintf(fp,"#<");
						fklPrintString(fklGetGlobSymbolWithId(v->u.ud->type)->symbol,fp);
						fprintf(fp,":%p>",v->u.ud);
					}
					break;
				default:
					fputs("#<unknown>",fp);
					break;
			}
	}
}

void fklPrin1VMvalue(FklVMvalue* value,FILE* fp)
{
	fklPrintVMvalue(value,fp,prin1VMatom);
}

void fklPrincVMvalue(FklVMvalue* value,FILE* fp)
{
	fklPrintVMvalue(value,fp,princVMatom);
}

void fklPrintVMvalue(FklVMvalue* value,FILE* fp,void(*atomPrinter)(FklVMvalue* v,FILE* fp))
{
	FklPtrStack* recStack=fklNewPtrStack(32,16);
	FklPtrStack* hasPrintRecStack=fklNewPtrStack(32,16);
	scanCirRef(value,recStack);
	FklPtrQueue* queue=fklNewPtrQueue();
	FklPtrStack* queueStack=fklNewPtrStack(32,16);
	fklPushPtrQueue(newPrtElem(PRT_CAR,value),queue);
	fklPushPtrStack(queue,queueStack);
	while(!fklIsPtrStackEmpty(queueStack))
	{
		FklPtrQueue* cQueue=fklTopPtrStack(queueStack);
		while(fklLengthPtrQueue(cQueue))
		{
			PrtElem* e=fklPopPtrQueue(cQueue);
			FklVMvalue* v=e->v;
			if(e->state==PRT_CDR||e->state==PRT_REC_CDR)
				fputc(',',fp);
			if(e->state==PRT_REC_CAR||e->state==PRT_REC_CDR||e->state==PRT_REC_BOX)
			{
				fprintf(fp,"#%lu#",(uintptr_t)e->v);
				free(e);
			}
			else
			{
				free(e);
				if(!FKL_IS_VECTOR(v)&&!FKL_IS_PAIR(v)&&!FKL_IS_BOX(v))
					atomPrinter(v,fp);
				else
				{
					for(uint32_t i=0;i<recStack->top;i++)
						if(recStack->base[i]==v)
						{
							fprintf(fp,"#%u=",i);
							break;
						}
					if(FKL_IS_VECTOR(v))
					{
						fputs("#(",fp);
						FklPtrQueue* vQueue=fklNewPtrQueue();
						for(size_t i=0;i<v->u.vec->size;i++)
						{
							size_t w=0;
							if(isInStack(v->u.vec->base[i],recStack,&w)&&v->u.vec->base[i]==v)
								fklPushPtrQueue(newPrtElem(PRT_REC_CAR,(void*)w)
										,vQueue);
							else
								fklPushPtrQueue(newPrtElem(PRT_CAR,v->u.vec->base[i])
										,vQueue);
						}
						fklPushPtrStack(vQueue,queueStack);
						cQueue=vQueue;
						continue;
					}
					else if(FKL_IS_BOX(v))
					{
						fputs("#&",fp);
						size_t w=0;
						if(isInStack(v->u.box,recStack,&w)&&v->u.box==v)
							fklPushPtrQueue(newPrtElem(PRT_REC_BOX,(void*)w),cQueue);
						else
							fklPushPtrQueue(newPrtElem(PRT_BOX,v->u.box),cQueue);
						continue;
					}
					else
					{
						fputc('(',fp);
						FklPtrQueue* lQueue=fklNewPtrQueue();
						FklVMpair* p=v->u.pair;
						for(;;)
						{
							PrtElem* ce=NULL;
							size_t w=0;
							if(isInStack(p->car,recStack,&w)&&(isInStack(p->car,hasPrintRecStack,&w)||p->car==v))
								ce=newPrtElem(PRT_REC_CAR,(void*)w);
							else
							{
								ce=newPrtElem(PRT_CAR,p->car);
								fklPushPtrStack(p->car,hasPrintRecStack);
							}
							fklPushPtrQueue(ce,lQueue);
							if(isInStack(p->cdr,recStack,&w))
							{
								PrtElem* cdre=NULL;
								if(p->cdr!=v&&!isInStack(p->cdr,hasPrintRecStack,NULL))
									cdre=newPrtElem(PRT_CDR,p->cdr);
								else
								{
									cdre=newPrtElem(PRT_REC_CDR,(void*)w);
								}
								fklPushPtrQueue(cdre,lQueue);
								break;
							}
							FklVMpair* next=FKL_IS_PAIR(p->cdr)?p->cdr->u.pair:NULL;
							if(!next)
							{
								FklVMvalue* cdr=p->cdr;
								if(cdr!=FKL_VM_NIL)
									fklPushPtrQueue(newPrtElem(PRT_CDR,cdr),lQueue);
								break;
							}
							p=next;
						}
						fklPushPtrStack(lQueue,queueStack);
						cQueue=lQueue;
						continue;
					}
				}
				
			}
			if(fklLengthPtrQueue(cQueue)
					&&((PrtElem*)fklFirstPtrQueue(cQueue))->state!=PRT_CDR
					&&((PrtElem*)fklFirstPtrQueue(cQueue))->state!=PRT_REC_CDR
					&&((PrtElem*)fklFirstPtrQueue(cQueue))->state!=PRT_BOX
					&&((PrtElem*)fklFirstPtrQueue(cQueue))->state!=PRT_REC_BOX)
				fputc(' ',fp);
		}
		fklPopPtrStack(queueStack);
		fklFreePtrQueue(cQueue);
		if(!fklIsPtrStackEmpty(queueStack))
		{
			fputc(')',fp);
			cQueue=fklTopPtrStack(queueStack);
			if(fklLengthPtrQueue(cQueue)
					&&((PrtElem*)fklFirstPtrQueue(cQueue))->state!=PRT_CDR
					&&((PrtElem*)fklFirstPtrQueue(cQueue))->state!=PRT_REC_CDR
					&&((PrtElem*)fklFirstPtrQueue(cQueue))->state!=PRT_BOX
					&&((PrtElem*)fklFirstPtrQueue(cQueue))->state!=PRT_REC_BOX)
				fputc(' ',fp);
		}
	}
	fklFreePtrStack(queueStack);
	fklFreePtrStack(recStack);
	fklFreePtrStack(hasPrintRecStack);
}

FklVMvalue* fklSetRef(FklVMvalue* volatile* pref,FklVMvalue* v,FklVMheap* h)
{
	FklVMvalue* ref=*pref;
	*pref=v;
	FklGCstate running=fklGetGCstate(h);
	if(running==FKL_GC_PROPAGATE||running==FKL_GC_COLLECT)
	{
//		if(by->mark==FKL_MARK_B)
//			fklGC_reGray(by,h);
//		else if(by->mark==FKL_MARK_G&&ref&&FKL_IS_PTR(ref)&&ref->mark==FKL_MARK_W)
			fklGC_toGray(ref,h);
			fklGC_toGray(v,h);
	}
	return ref;
}

FklVMvalue* fklGetTopValue(FklVMstack* stack)
{
	return stack->values[stack->tp-1];
}

FklVMvalue* fklGetValue(FklVMstack* stack,int32_t place)
{
	return stack->values[place];
}

FklAstCptr* fklCastVMvalueToCptr(FklVMvalue* value,int32_t curline)
{
	FklPtrStack* recStack=fklNewPtrStack(32,16);
	scanCirRef(value,recStack);
	FklAstCptr* tmp=NULL;
	if(!recStack->top)
	{
		tmp=fklNewCptr(curline,NULL);
		FklPtrStack* s1=fklNewPtrStack(32,16);
		FklPtrStack* s2=fklNewPtrStack(32,16);
		fklPushPtrStack(value,s1);
		fklPushPtrStack(tmp,s2);
		while(!fklIsPtrStackEmpty(s1))
		{
			FklVMvalue* root=fklPopPtrStack(s1);
			FklAstCptr* root1=fklPopPtrStack(s2);
			FklValueType cptrType=0;
			if(root==FKL_VM_NIL)
				cptrType=FKL_NIL;
			else if(FKL_IS_PAIR(root))
				cptrType=FKL_PAIR;
			else
				//if(!FKL_IS_REF(root)&&!FKL_IS_MREF(root))
				cptrType=FKL_ATM;
			root1->type=cptrType;
			if(cptrType==FKL_ATM)
			{
				FklAstAtom* tmpAtm=fklNewAtom(FKL_SYM,root1->outer);
				FklVMptrTag tag=FKL_GET_TAG(root);
				switch(tag)
				{
					case FKL_SYM_TAG:
						tmpAtm->type=FKL_SYM;
						tmpAtm->value.str=fklCopyString(fklGetGlobSymbolWithId(FKL_GET_SYM(root))->symbol);
						break;
					case FKL_I32_TAG:
						tmpAtm->type=FKL_I32;
						tmpAtm->value.i32=FKL_GET_I32(root);
						break;
					case FKL_CHR_TAG:
						tmpAtm->type=FKL_CHR;
						tmpAtm->value.chr=FKL_GET_CHR(root);
						break;
					case FKL_PTR_TAG:
						{
							tmpAtm->type=root->type;
							switch(root->type)
							{
								case FKL_F64:
									tmpAtm->value.f64=root->u.f64;
									break;
								case FKL_I64:
									tmpAtm->value.i64=root->u.i64;
									break;
								case FKL_STR:
									tmpAtm->value.str=fklCopyString(root->u.str);
									break;
								case FKL_PROC:
									tmpAtm->type=FKL_SYM;
									tmpAtm->value.str=fklNewStringFromCstr("#<proc>");
									break;
								case FKL_DLPROC:
									tmpAtm->type=FKL_SYM;
									tmpAtm->value.str=fklNewStringFromCstr("#<dlproc>");
									break;
								case FKL_CONT:
									tmpAtm->type=FKL_SYM;
									tmpAtm->value.str=fklNewStringFromCstr("#<proc>");
									break;
								case FKL_CHAN:
									tmpAtm->type=FKL_SYM;
									tmpAtm->value.str=fklNewStringFromCstr("#<chan>");
									break;
								case FKL_FP:
									tmpAtm->type=FKL_SYM;
									tmpAtm->value.str=fklNewStringFromCstr("#<fp>");
									break;
								case FKL_ERR:
									tmpAtm->type=FKL_SYM;
									tmpAtm->value.str=fklNewStringFromCstr("#<err>");
									break;
								case FKL_BOX:
									tmpAtm->type=FKL_BOX;
									fklPushPtrStack(root->u.box,s1);
									fklPushPtrStack(&tmpAtm->value.box,s2);
									break;
								case FKL_VECTOR:
									tmpAtm->type=FKL_VECTOR;
									fklMakeAstVector(&tmpAtm->value.vec,root->u.vec->size,NULL);
									for(size_t i=0;i<root->u.vec->size;i++)
										fklPushPtrStack(root->u.vec->base[i],s1);
									for(size_t i=0;i<tmpAtm->value.vec.size;i++)
										fklPushPtrStack(&tmpAtm->value.vec.base[i],s2);
									break;
								case FKL_BIG_INT:
									tmpAtm->type=FKL_BIG_INT;
									fklSetBigInt(&tmpAtm->value.bigInt,root->u.bigInt);
									break;
								default:
									return NULL;
									break;
							}
						}
						break;
					default:
						return NULL;
						break;
				}
				root1->u.atom=tmpAtm;
			}
			else if(cptrType==FKL_PAIR)
			{
				fklPushPtrStack(root->u.pair->car,s1);
				fklPushPtrStack(root->u.pair->cdr,s1);
				FklAstPair* tmpPair=fklNewPair(curline,root1->outer);
				root1->u.pair=tmpPair;
				tmpPair->car.outer=tmpPair;
				tmpPair->cdr.outer=tmpPair;
				fklPushPtrStack(&tmpPair->car,s2);
				fklPushPtrStack(&tmpPair->cdr,s2);
			}
		}
		fklFreePtrStack(s1);
		fklFreePtrStack(s2);
	}
	fklFreePtrStack(recStack);
	return tmp;
}

void fklInitVMRunningResource(FklVM* vm,FklVMvalue* vEnv,FklVMheap* heap,FklByteCodelnt* code,uint32_t start,uint32_t size)
{
	if((!vEnv->u.env->prev||vEnv->u.env->prev==FKL_VM_NIL)&&vEnv->u.env->num==0)
		fklInitGlobEnv(vEnv->u.env,heap);
	FklVMproc proc={
		.scp=start,
		.cpc=size,
		.sid=0,
		.prevEnv=NULL,
	};
	FklVMrunnable* mainrunnable=fklNewVMrunnable(&proc,NULL);
	mainrunnable->localenv=vEnv;
	vm->code=code->bc->code;
	vm->size=code->bc->size;
	vm->rhead=mainrunnable;
	vm->lnt=fklNewLineNumTable();
	vm->lnt->num=code->ls;
	vm->lnt->list=code->l;
	if(vm->heap!=heap)
	{
		fklFreeVMheap(vm->heap);
		vm->heap=heap;
	}
}

void fklUninitVMRunningResource(FklVM* vm)
{
	fklWaitGC(vm->heap);
	free(vm->lnt);
	fklDeleteCallChain(vm);
	fklFreeVMstack(vm->stack);
	fklFreePtrStack(vm->tstack);
	free(vm);
}

size_t fklVMlistLength(FklVMvalue* v)
{
	size_t len=0;
	for(;FKL_IS_PAIR(v);v=fklGetVMpairCdr(v))len++;
	return len;
}
