#include<fakeLisp/vm.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/ast.h>
#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<math.h>
#include<pthread.h>
#include<ffi.h>
#ifndef _WIN32
#include<dlfcn.h>
#else
#include<tchar.h>
#endif

FklVMvalue* fklPopVMstack(FklVMstack* stack)
{
	if(!(stack->tp>stack->bp))
		return NULL;
	FklVMvalue* tmp=fklGetTopValue(stack);
	stack->tp-=1;
	return tmp;
}

FklVMenv* fklCastPreEnvToVMenv(FklPreEnv* pe,FklVMenv* prev,FklVMheap* heap)
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
	{
		FklVMvalue* v=fklCastCptrVMvalue(&tmpDef->obj,heap);
		FklVMenvNode* node=fklNewVMenvNode(v,fklAddSymbolToGlob(tmpDef->symbol)->id);
		fklAddVMenvNode(node,tmp);
	}
	return tmp;
}

FklVMstack* fklCopyStack(FklVMstack* stack)
{
	int32_t i=0;
	FklVMstack* tmp=(FklVMstack*)malloc(sizeof(FklVMstack));
	FKL_ASSERT(tmp,__func__);
	tmp->size=stack->size;
	tmp->tp=stack->tp;
	tmp->bp=stack->bp;
	tmp->values=(FklVMvalue**)malloc(sizeof(FklVMvalue*)*(tmp->size));
	FKL_ASSERT(tmp->values,__func__);
	for(;i<stack->tp;i++)
		tmp->values[i]=stack->values[i];
	tmp->tptp=stack->tptp;
	tmp->tpst=NULL;
	tmp->tpsi=stack->tpsi;
	if(tmp->tpsi)
	{
		tmp->tpst=(uint32_t*)malloc(sizeof(uint32_t)*tmp->tpsi);
		FKL_ASSERT(tmp->tpst,__func__);
		if(tmp->tptp)memcpy(tmp->tpst,stack->tpst,sizeof(int32_t)*(tmp->tptp));
	}
	return tmp;
}

FklVMtryBlock* fklNewVMtryBlock(FklSid_t sid,uint32_t tp,long int rtp)
{
	FklVMtryBlock* t=(FklVMtryBlock*)malloc(sizeof(FklVMtryBlock));
	FKL_ASSERT(t,__func__);
	t->sid=sid;
	t->hstack=fklNewPtrStack(32,16);
	t->tp=tp;
	t->rtp=rtp;
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

FklVMerrorHandler* fklNewVMerrorHandler(FklSid_t type,uint64_t scp,uint64_t cpc)
{
	FklVMerrorHandler* t=(FklVMerrorHandler*)malloc(sizeof(FklVMerrorHandler));
	FKL_ASSERT(t,__func__);
	t->type=type;
	t->proc.prevEnv=NULL;
	t->proc.scp=scp;
	t->proc.cpc=cpc;
	return t;
}

void fklFreeVMerrorHandler(FklVMerrorHandler* h)
{
	fklFreeVMenv(h->proc.prevEnv);
	free(h);
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
			if(h->type==err->type)
			{
				long int increment=exe->rstack->top-tb->rtp;
				unsigned int i=0;
				for(;i<increment;i++)
				{
					FklVMrunnable* runnable=fklPopPtrStack(exe->rstack);
					fklFreeVMenv(runnable->localenv);
					free(runnable);
				}
				FklVMrunnable* prevRunnable=fklTopPtrStack(exe->rstack);
				FklVMrunnable* r=fklNewVMrunnable(&h->proc);
				r->localenv=fklNewVMenv(prevRunnable->localenv);
				FklVMenv* curEnv=r->localenv;
				FklSid_t idOfError=tb->sid;
				FklVMenvNode* errorNode=fklFindVMenvNode(idOfError,curEnv);
				if(!errorNode)
					errorNode=fklAddVMenvNode(fklNewVMenvNode(NULL,idOfError),curEnv);
				errorNode->value=ev;
				fklPushPtrStack(r,exe->rstack);
				fklFreeVMerrorHandler(h);
				return 1;
			}
			fklFreeVMerrorHandler(h);
		}
		fklFreeVMtryBlock(fklPopPtrStack(exe->tstack));
	}
	fprintf(stderr,"error of %s :%s",err->who,err->message);
	void* i[3]={exe,(void*)255,(void*)1};
	exe->callback(i);
	return 255;
}

FklVMrunnable* fklNewVMrunnable(FklVMproc* code)
{
	FklVMrunnable* tmp=(FklVMrunnable*)malloc(sizeof(FklVMrunnable));
	FKL_ASSERT(tmp,__func__);
	tmp->sid=0;
	tmp->cp=0;
	tmp->scp=0;
	tmp->cpc=0;
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

char* fklGenInvalidSymbolErrorMessage(const char* str,FklVMrunnable* r,FklVM* exe)
{
	int32_t cp=r->cp;
	FklLineNumTabNode* node=fklFindLineNumTabNode(cp,exe->lnt);
	char* filename=fklGetGlobSymbolWithId(node->fid)->symbol;
	char* line=fklIntToString(node->line);
	size_t len=strlen("at line  of \n")+strlen(filename)+strlen(line)+1;
	char* lineNumber=(char*)malloc(sizeof(char)*len);
	FKL_ASSERT(lineNumber,__func__);
	sprintf(lineNumber,"at line %s of %s\n",line,filename);
	free(line);
	char* t=fklCopyStr("");
	t=fklStrCat(t,"Invalid symbol ");
	t=fklStrCat(t,str);
	t=fklStrCat(t," ");
	t=fklStrCat(t,lineNumber);
	free(lineNumber);
	for(uint32_t i=exe->rstack->top;i;i--)
	{
		FklVMrunnable* r=exe->rstack->base[i-1];
		if(r->sid!=0)
		{
			t=fklStrCat(t,"at proc:");
			t=fklStrCat(t,fklGetGlobSymbolWithId(r->sid)->symbol);
		}
		else
		{
			if(i>1)
				t=fklStrCat(t,"at <lambda>");
			else
				t=fklStrCat(t,"at <top>");
		}
		FklLineNumTabNode* node=fklFindLineNumTabNode(r->cp,exe->lnt);
		char* filename=fklGetGlobSymbolWithId(node->fid)->symbol;
		char* line=fklIntToString(node->line);
		size_t len=strlen("(:)\n")+strlen(filename)+strlen(line)+1;
		char* lineNumber=(char*)malloc(sizeof(char)*len);
		FKL_ASSERT(lineNumber,__func__);
		sprintf(lineNumber,"(%s:%s)\n",line,filename);
		free(line);
		t=fklStrCat(t,lineNumber);
		free(lineNumber);
	}

	return t;
}

char* fklGenErrorMessage(unsigned int type,FklVMrunnable* r,FklVM* exe)
{
	int32_t cp=r->cp;
	FklLineNumTabNode* node=fklFindLineNumTabNode(cp,exe->lnt);
	char* filename=fklGetGlobSymbolWithId(node->fid)->symbol;
	char* line=fklIntToString(node->line);
	size_t len=strlen("at line  of \n")+strlen(filename)+strlen(line)+1;
	char* lineNumber=(char*)malloc(sizeof(char)*len);
	FKL_ASSERT(lineNumber,__func__);
	sprintf(lineNumber,"at line %s of %s\n",line,filename);
	free(line);
	char* t=fklCopyStr("");
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
			t=fklStrCat(t,fklGetGlobSymbolWithId(fklGetSymbolIdInByteCode(exe->code+r->cp))->symbol);
			t=fklStrCat(t," is undefined ");
			break;
		case FKL_INVOKEERROR:
			t=fklStrCat(t,"Try to invoke an object that isn't procedure,continuation or native procedure ");
			break;
		case FKL_LOADDLLFAILD:
			t=fklStrCat(t,"Faild to load dll \"");
			t=fklStrCat(t,exe->stack->values[exe->stack->tp-1]->u.str);
			t=fklStrCat(t,"\" ");
			break;
		case FKL_INVALIDSYMBOL:
			t=fklStrCat(t,"Invalid symbol ");
			t=fklStrCat(t,exe->stack->values[exe->stack->tp-1]->u.str);
			t=fklStrCat(t," ");
			break;
		case FKL_DIVZERROERROR:
			t=fklStrCat(t,"Divided by zero ");
			break;
		case FKL_FILEFAILURE:
			t=fklStrCat(t,"Failed for file:\"");
			t=fklStrCat(t,exe->stack->values[exe->stack->tp-1]->u.str);
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
	}
	t=fklStrCat(t,lineNumber);
	free(lineNumber);
	for(uint32_t i=exe->rstack->top;i;i--)
	{
		FklVMrunnable* r=exe->rstack->base[i-1];
		if(r->sid!=0)
		{
			t=fklStrCat(t,"at proc:");
			t=fklStrCat(t,fklGetGlobSymbolWithId(r->sid)->symbol);
		}
		else
		{
			if(i>1)
				t=fklStrCat(t,"at <lambda>");
			else
				t=fklStrCat(t,"at <top>");
		}
		FklLineNumTabNode* node=fklFindLineNumTabNode(r->cp,exe->lnt);
		char* filename=fklGetGlobSymbolWithId(node->fid)->symbol;
		char* line=fklIntToString(node->line);
		size_t len=strlen("(:)\n")+strlen(filename)+strlen(line)+1;
		char* lineNumber=(char*)malloc(sizeof(char)*len);
		FKL_ASSERT(lineNumber,__func__);
		sprintf(lineNumber,"(%s:%s)\n",line,filename);
		free(line);
		t=fklStrCat(t,lineNumber);
		free(lineNumber);
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

int fklResBp(FklVMstack* stack)
{
	if(stack->tp>stack->bp)
		return stack->tp-stack->bp;
	FklVMvalue* prevBp=fklGetTopValue(stack);
	stack->bp=FKL_GET_I32(prevBp);
	stack->tp-=1;
	return 0;
}

typedef struct PrtElem
{
	enum PrintingState
	{
		PRT_CAR,
		PRT_CDR,
		PRT_REC_CAR,
		PRT_REC_CDR,
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
	{
		if(stack->base[i]==v)
		{
			*w=i;
			return 1;
		}
	}
	return 0;
}

static void scanCirRef(FklVMvalue* s,FklPtrStack* recStack)
{
	FklPtrStack* hasAccessed=fklNewPtrStack(32,16);
	FklPtrStack* toAccess=fklNewPtrStack(32,16);
	fklPushPtrStack(s,toAccess);
	while(!fklIsPtrStackEmpty(toAccess))
	{
		FklVMvalue* v=fklPopPtrStack(toAccess);
		if(FKL_IS_PAIR(v)||FKL_IS_VECTOR(v))
		{
			size_t w=0;
			if(isInStack(v,hasAccessed,&w))
			{
				fklPushPtrStack(v,recStack);
				continue;
			}
			fklPushPtrStack(v,hasAccessed);
			if(FKL_IS_PAIR(v))
			{
				fklPushPtrStack(v->u.pair->cdr,toAccess);
				fklPushPtrStack(v->u.pair->car,toAccess);
			}
			else
			{
				FklVMvec* vec=v->u.vec;
				for(size_t i=vec->size;i>0;i--)
					fklPushPtrStack(vec->base[i-1],toAccess);
			}
		}
	}
	fklFreePtrStack(hasAccessed);
	fklFreePtrStack(toAccess);
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
			fprintf(fp,"%s",fklGetGlobSymbolWithId(FKL_GET_SYM(v))->symbol);
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
						fprintf(fp,"%s",v->u.str);
						break;
					case FKL_MEM:
					case FKL_CHF:
						{
							FklVMmem* mem=v->u.chf;
							FklTypeId_t type=mem->type;
							if(fklIsNativeTypeId(type))
								fklPrintMemoryRef(type,mem,fp);
							else if(FKL_IS_CHF(v))
								fprintf(fp,"<#memref at %p>",mem->mem);
							else
								fprintf(fp,"<#mem at %p>",mem->mem);
						}
						break;
					case FKL_PROC:
						if(v->u.proc->sid)
							fprintf(fp,"<#proc: %s>",fklGetGlobSymbolWithId(v->u.proc->sid)->symbol);
						else
							fprintf(fp,"#<proc>");
						break;
					case FKL_BYTS:
						fklPrintByteStr(v->u.byts->size,v->u.byts->str,fp,0);
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
							fprintf(fp,"<#dlproc: %s>",fklGetGlobSymbolWithId(v->u.dlproc->sid)->symbol);
						else
							fprintf(fp,"<#dlproc>");
						break;
					case FKL_FLPROC:
						if(v->u.flproc->sid)
							fprintf(fp,"<#flproc: %s>",fklGetGlobSymbolWithId(v->u.flproc->sid)->symbol);
						else
							fprintf(fp,"<#flproc>");
						break;
					case FKL_ERR:
						fprintf(fp,"%s",v->u.err->message);
						break;
					default:
						fprintf(fp,"Bad value!");break;
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
			fputs(fklGetGlobSymbolWithId(FKL_GET_SYM(v))->symbol,fp);
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
				case FKL_MEM:
				case FKL_CHF:
					{
						FklVMmem* mem=v->u.chf;
						FklTypeId_t type=mem->type;
						if(fklIsNativeTypeId(type))
							fklPrintMemoryRef(type,mem,fp);
						else if(FKL_IS_CHF(v))
							fprintf(fp,"<#memref at %p>",mem->mem);
						else
							fprintf(fp,"<#mem at %p>",mem->mem);
					}
					break;
				case FKL_PROC:
					if(v->u.proc->sid)
						fprintf(fp,"<#proc: %s>"
								,fklGetGlobSymbolWithId(v->u.proc->sid)->symbol);
					else
						fputs("#<proc>",fp);
					break;
				case FKL_BYTS:
					fklPrintByteStr(v->u.byts->size,v->u.byts->str,fp,1);
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
					if(v->u.dlproc->sid)
						fprintf(fp,"#<dlproc: %s>"
								,fklGetGlobSymbolWithId(v->u.dlproc->sid)->symbol);
					else
						fputs("#<dlproc>",fp);
					break;
				case FKL_FLPROC:
					if(v->u.flproc->sid)
						fprintf(fp,"#<flproc: %s>"
								,fklGetGlobSymbolWithId(v->u.flproc->sid)->symbol);
					else
						fputs("#<flproc>",fp);
					break;
				case FKL_ERR:
					fprintf(fp,"#<err w:%s t:%s m:%s>"
							,v->u.err->who
							,fklGetGlobSymbolWithId(v->u.err->type)->symbol
							,v->u.err->message);
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
			if(e->state==PRT_REC_CAR||e->state==PRT_REC_CDR)
			{
				fprintf(fp,"#%lu#",(uintptr_t)e->v);
				free(e);
			}
			else
			{
				free(e);
				if(!FKL_IS_VECTOR(v)&&!FKL_IS_PAIR(v))
					atomPrinter(v,fp);
				else if(FKL_IS_VECTOR(v))
				{
					for(uint32_t i=0;i<recStack->top;i++)
						if(recStack->base[i]==v)
						{
							fprintf(fp,"#%u=",i);
							break;
						}
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
				else
				{
					for(uint32_t i=0;i<recStack->top;i++)
						if(recStack->base[i]==v)
						{
							fprintf(fp,"#%u=",i);
							break;
						}
					fputc('(',fp);
					FklPtrQueue* lQueue=fklNewPtrQueue();
					FklVMpair* p=v->u.pair;
					for(;;)
					{
						PrtElem* ce=NULL;
						size_t w=0;
						if(isInStack(p->car,recStack,&w))
							ce=newPrtElem(PRT_REC_CAR,(void*)w);
						else
							ce=newPrtElem(PRT_CAR,p->car);
						fklPushPtrQueue(ce,lQueue);
						if(isInStack(p->cdr,recStack,&w))
						{
							PrtElem* cdre=NULL;
							if(p->cdr!=v)
								cdre=newPrtElem(PRT_CDR,p->cdr);
							else
								cdre=newPrtElem(PRT_REC_CDR,(void*)w);
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
			if(fklLengthPtrQueue(cQueue)
					&&((PrtElem*)fklFirstPtrQueue(cQueue))->state!=PRT_CDR
					&&(((PrtElem*)fklFirstPtrQueue(cQueue))->state!=PRT_REC_CDR))
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
					&&(((PrtElem*)fklFirstPtrQueue(cQueue))->state!=PRT_REC_CDR))
				fputc(' ',fp);
		}
	}
	fklFreePtrStack(queueStack);
	fklFreePtrStack(recStack);
}

FklVMvalue* fklGET_VAL(FklVMvalue* P,FklVMheap* heap)
{
	if(P)
	{
		if(FKL_IS_REF(P))
			return *(FklVMvalue**)(FKL_GET_PTR(P));
		else if(FKL_IS_CHF(P))
		{
			FklVMvalue* t=NULL;
			FklVMmem* mem=P->u.chf;
			if(fklIsNativeTypeId(mem->type))
				t=fklMemoryCast(mem->type,mem,heap);
			else
				t=P;
			return t;
		}
		return P;
	}
	return P;
}

static inline int64_t getInt(FklVMvalue* p)
{
	return FKL_IS_I32(p)?FKL_GET_I32(p):p->u.i64;
}

int fklSET_REF(FklVMvalue* P,FklVMvalue* V)
{
	if(FKL_IS_MEM(P)||FKL_IS_CHF(P))
	{
		FklVMmem* mem=P->u.chf;
		if(mem->type<=0)
			return 1;
		else if(!fklIsNativeTypeId(mem->type))
		{
			if(fklIsPtrTypeId(mem->type)&&(FKL_IS_MEM(V)||FKL_IS_CHF(V)))
			{
				if((FKL_IS_MEM(V)||FKL_IS_CHF(V)))
				{
					FklVMmem* t=V->u.chf;
					*(void**)(mem->mem)=t->mem;
					return 0;
				}
				else if(FKL_IS_I32(V)||FKL_IS_I64(V))
				{
					*(void**)(mem->mem)=(void*)getInt(V);
					return 0;
				}
			}
			if(!FKL_IS_I32(V)&&!FKL_IS_I64(V))
				return 1;
			*(void**)(mem->mem)=(uint8_t*)(FKL_IS_I32(V)?FKL_GET_I32(V):V->u.i64);
		}
		else if(fklMemorySet(mem->type,mem,V))
			return 1;
		return 0;
	}
	else if(FKL_IS_REF(P))
	{
		*(FklVMvalue**)FKL_GET_PTR(P)=V;
		return 0;
	}
	else
		return 1;
}

FklVMvalue* fklGetTopValue(FklVMstack* stack)
{
	return stack->values[stack->tp-1];
}

FklVMvalue* fklGetValue(FklVMstack* stack,int32_t place)
{
	return stack->values[place];
}

VMcontinuation* fklNewVMcontinuation(FklVMstack* stack,FklPtrStack* rstack,FklPtrStack* tstack)
{
	int32_t size=rstack->top;
	int32_t i=0;
	VMcontinuation* tmp=(VMcontinuation*)malloc(sizeof(VMcontinuation));
	FKL_ASSERT(tmp,__func__);
	FklVMrunnable* state=(FklVMrunnable*)malloc(sizeof(FklVMrunnable)*size);
	FKL_ASSERT(state,__func__);
	int32_t tbnum=tstack->top;
	FklVMtryBlock* tb=(FklVMtryBlock*)malloc(sizeof(FklVMtryBlock)*tbnum);
	FKL_ASSERT(tb,__func__);
	tmp->stack=fklCopyStack(stack);
	tmp->num=size;
	for(;i<size;i++)
	{
		FklVMrunnable* cur=rstack->base[i];
		state[i].cp=cur->cp;
		fklIncreaseVMenvRefcount(cur->localenv);
		state[i].localenv=cur->localenv;
		state[i].cpc=cur->cpc;
		state[i].scp=cur->scp;
		state[i].sid=cur->sid;
		state[i].mark=cur->mark;
	}
	tmp->tnum=tbnum;
	for(i=0;i<tbnum;i++)
	{
		FklVMtryBlock* cur=tstack->base[i];
		tb[i].sid=cur->sid;
		FklPtrStack* hstack=cur->hstack;
		int32_t handlerNum=hstack->top;
		FklPtrStack* curHstack=fklNewPtrStack(handlerNum,handlerNum/2);
		int32_t j=0;
		for(;j<handlerNum;j++)
		{
			FklVMerrorHandler* curH=hstack->base[i];
			FklVMerrorHandler* h=fklNewVMerrorHandler(curH->type,curH->proc.scp,curH->proc.cpc);
			fklPushPtrStack(h,curHstack);
		}
		tb[i].hstack=curHstack;
		tb[i].rtp=cur->rtp;
		tb[i].tp=cur->tp;
	}
	tmp->state=state;
	tmp->tb=tb;
	return tmp;
}

void fklFreeVMcontinuation(VMcontinuation* cont)
{
	int32_t i=0;
	int32_t size=cont->num;
	int32_t tbsize=cont->tnum;
	FklVMstack* stack=cont->stack;
	FklVMrunnable* state=cont->state;
	FklVMtryBlock* tb=cont->tb;
	free(stack->tpst);
	free(stack->values);
	free(stack);
	for(;i<size;i++)
		fklFreeVMenv(state[i].localenv);
	for(i=0;i<tbsize;i++)
	{
		FklPtrStack* hstack=tb[i].hstack;
		while(!fklIsPtrStackEmpty(hstack))
		{
			FklVMerrorHandler* h=fklPopPtrStack(hstack);
			fklFreeVMerrorHandler(h);
		}
		fklFreePtrStack(hstack);
	}
	free(tb);
	free(state);
	free(cont);
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
			else if(FKL_IS_PAIR(root)||FKL_IS_VECTOR(root))
				cptrType=FKL_PAIR;
			else if(!FKL_IS_REF(root)&&!FKL_IS_CHF(root))
				cptrType=FKL_ATM;
			root1->type=cptrType;
			if(cptrType==FKL_ATM)
			{
				FklAstAtom* tmpAtm=fklNewAtom(FKL_SYM,NULL,root1->outer);
				FklVMptrTag tag=FKL_GET_TAG(root);
				switch(tag)
				{
					case FKL_SYM_TAG:
						tmpAtm->type=FKL_SYM;
						tmpAtm->value.str=fklCopyStr(fklGetGlobSymbolWithId(FKL_GET_SYM(root))->symbol);
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
									tmpAtm->value.str=fklCopyStr(root->u.str);
									break;
								case FKL_BYTS:
									tmpAtm->value.byts.size=root->u.byts->size;
									tmpAtm->value.byts.str=fklCopyMemory(root->u.byts->str,root->u.byts->size);
									break;
								case FKL_PROC:
									tmpAtm->type=FKL_SYM;
									tmpAtm->value.str=fklCopyStr("#<proc>");
									break;
								case FKL_DLPROC:
									tmpAtm->type=FKL_SYM;
									tmpAtm->value.str=fklCopyStr("#<dlproc>");
									break;
								case FKL_CONT:
									tmpAtm->type=FKL_SYM;
									tmpAtm->value.str=fklCopyStr("#<proc>");
									break;
								case FKL_CHAN:
									tmpAtm->type=FKL_SYM;
									tmpAtm->value.str=fklCopyStr("#<chan>");
									break;
								case FKL_FP:
									tmpAtm->type=FKL_SYM;
									tmpAtm->value.str=fklCopyStr("#<fp>");
									break;
								case FKL_ERR:
									tmpAtm->type=FKL_SYM;
									tmpAtm->value.str=fklCopyStr("#<err>");
									break;
								case FKL_MEM:
									tmpAtm->type=FKL_SYM;
									tmpAtm->value.str=fklCopyStr("#<mem>");
									break;
								case FKL_CHF:
									tmpAtm->type=FKL_SYM;
									tmpAtm->value.str=fklCopyStr("#<ref>");
									break;
								case FKL_VECTOR:
									tmpAtm->type=FKL_VECTOR;
									fklMakeAstVector(&tmpAtm->value.vec,root->u.vec->size,NULL);
									for(size_t i=0;i<root->u.vec->size;i++)
										fklPushPtrStack(root->u.vec->base[i],s1);
									for(size_t i=0;i<tmpAtm->value.vec.size;i++)
										fklPushPtrStack(&tmpAtm->value.vec.base[i],s2);
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
	return tmp;
}

void fklInitVMRunningResource(FklVM* vm,FklVMenv* vEnv,FklVMheap* heap,FklByteCodelnt* code,uint32_t start,uint32_t size)
{
	if(!vEnv->prev&&vEnv->num==0)
		fklInitGlobEnv(vEnv,heap);
	FklVMproc proc={
		.scp=start,
		.cpc=size,
		.sid=0,
		.prevEnv=NULL,
	};
	FklVMrunnable* mainrunnable=fklNewVMrunnable(&proc);
	mainrunnable->localenv=vEnv;
	vm->code=code->bc->code;
	vm->size=code->bc->size;
	fklPushPtrStack(mainrunnable,vm->rstack);
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
	free(vm->lnt);
	fklDeleteCallChain(vm);
	fklFreeVMstack(vm->stack);
	fklFreePtrStack(vm->rstack);
	fklFreePtrStack(vm->tstack);
	free(vm);
}
