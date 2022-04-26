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

FklVMvalue* fklMakeVMint(int64_t r64,FklVMheap* heap)
{
	if(r64>INT32_MAX||r64<INT32_MIN)
		return fklNewVMvalue(FKL_I64,&r64,heap);
	else
		return FKL_MAKE_VM_I32(r64);
}

inline int fklIsInt(FklVMvalue* p)
{
	return FKL_IS_I32(p)||FKL_IS_I64(p);
}

inline int64_t fklGetInt(FklVMvalue* p)
{
	return FKL_IS_I32(p)?FKL_GET_I32(p):p->u.i64;
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
	pthread_mutex_lock(&stack->lock);
	FklVMvalue* r=NULL;
	if(!(stack->tp>stack->bp))
		r=NULL;
	else
	{
		FklVMvalue* tmp=fklGetTopValue(stack);
		if(FKL_IS_REF(tmp))
		{
			stack->tp-=1;
			r=*(FklVMvalue**)(FKL_GET_PTR(tmp));
			stack->values[stack->tp-1]=r;
		}
		else if(FKL_IS_MREF(tmp))
		{
			stack->tp-=1;
			void* ptr=fklGetTopValue(stack);
			r=FKL_MAKE_VM_CHR(*(char*)ptr);
			stack->values[stack->tp-1]=r;
		}
		else
			r=tmp;
	}
	pthread_mutex_unlock(&stack->lock);
	return r;
}

void fklDecTop(FklVMstack* stack)
{
	pthread_mutex_lock(&stack->lock);
	stack->tp--;
	pthread_mutex_unlock(&stack->lock);
}

inline void fklSetAndPop(FklVMvalue* by,FklVMvalue** pV,FklVMstack* stack,FklVMheap* heap)
{
	FklVMvalue* t=fklTopGet(stack);
	*pV=t;
	if(by->mark==FKL_MARK_B
			&&FKL_IS_PTR(t)&&t->mark==FKL_MARK_W)
		fklGC_toGray(t,heap);
	fklDecTop(stack);
}

FklVMvalue* fklPopGetAndMark(FklVMstack* stack,FklVMheap* heap)
{
	pthread_mutex_lock(&stack->lock);
	FklVMvalue* r=fklPopGetAndMarkWithoutLock(stack,heap);
	pthread_mutex_unlock(&stack->lock);
	return r;
}

FklVMvalue* fklPopGetAndMarkWithoutLock(FklVMstack* stack,FklVMheap* heap)
{
	FklVMvalue* r=NULL;
	if(!(stack->tp>stack->bp))
		r=NULL;
	else
	{
		FklVMvalue* tmp=fklGetTopValue(stack);
		stack->tp-=1;
		if(FKL_IS_REF(tmp))
		{
			stack->tp-=1;
			r=*(FklVMvalue**)(FKL_GET_PTR(tmp));
		}
		else if(FKL_IS_MREF(tmp))
		{
			void* ptr=fklGetTopValue(stack);
			stack->tp-=1;
			r=FKL_MAKE_VM_CHR(*(char*)ptr);
		}
		else
			r=tmp;
	}
	if(r&&heap->running==FKL_GC_RUNNING&&FKL_IS_PTR(r))
		fklGC_toGray(r,heap);
	return r;
}

FklVMvalue* fklGetArg(FklVMstack* stack)
{
	pthread_mutex_lock(&stack->lock);
	FklVMvalue* r=NULL;
	if(!stack->ap)
		stack->ap=stack->tp;
	if(!(stack->ap>stack->bp))
		r=NULL;
	else
	{
		FklVMvalue* tmp=fklGetTopValue(stack);
		stack->ap-=1;
		if(FKL_IS_REF(tmp))
		{
			stack->ap-=1;
			r=*(FklVMvalue**)(FKL_GET_PTR(tmp));
		}
		if(FKL_IS_MREF(tmp))
		{
			void* ptr=fklGetTopValue(stack);
			stack->ap-=1;
			r=FKL_MAKE_VM_CHR(*(char*)ptr);
		}
		else
			r=tmp;
	}
	pthread_mutex_unlock(&stack->lock);
	return r;
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
	{
		FklVMvalue* v=fklCastCptrVMvalue(&tmpDef->obj,heap);
		FklVMenvNode* node=fklNewVMenvNode(v,fklAddSymbolToGlob(tmpDef->symbol)->id);
		fklAddVMenvNode(node,tmp);
	}
	return fklNewVMvalue(FKL_ENV,tmp,heap);
}

FklVMstack* fklCopyStack(FklVMstack* stack)
{
	int32_t i=0;
	FklVMstack* tmp=(FklVMstack*)malloc(sizeof(FklVMstack));
	FKL_ASSERT(tmp,__func__);
	pthread_mutex_lock(&stack->lock);
	tmp->size=stack->size;
	tmp->tp=stack->tp;
	tmp->bp=stack->bp;
	pthread_mutex_init(&tmp->lock,NULL);
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
	pthread_mutex_unlock(&stack->lock);
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
					free(runnable);
				}
				FklVMrunnable* prevRunnable=fklTopPtrStack(exe->rstack);
				FklVMrunnable* r=fklNewVMrunnable(&h->proc);
				r->localenv=fklNewVMvalue(FKL_ENV,fklNewVMenv(prevRunnable->localenv),exe->heap);
				FklVMvalue* curEnv=r->localenv;
				FklSid_t idOfError=tb->sid;
				FklVMenvNode* errorNode=fklFindVMenvNode(idOfError,curEnv->u.env);
				if(!errorNode)
					errorNode=fklAddVMenvNode(fklNewVMenvNode(NULL,idOfError),curEnv->u.env);
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
	//fklDBG_printVMstack(exe->stack,stderr,1);
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

char* fklGenInvalidSymbolErrorMessage(char* str,int _free,FklErrorType type,FklVMrunnable* r,FklVM* exe)
{
	int32_t cp=r->cp;
	FklLineNumTabNode* node=fklFindLineNumTabNode(cp,exe->lnt);
	char* filename=node->fid?fklGetGlobSymbolWithId(node->fid)->symbol:NULL;
	char* line=fklIntToString(node->line);
	size_t len=node->fid?strlen("at line  of \n")+strlen(filename)+strlen(line)+1
		:strlen("at line \n")+strlen(line)+1;
	char* lineNumber=(char*)malloc(sizeof(char)*len);
	FKL_ASSERT(lineNumber,__func__);
	if(node->fid)
		sprintf(lineNumber,"at line %s of %s\n",line,filename);
	else
		sprintf(lineNumber,"at line %s\n",line);
	free(line);
	char* t=fklCopyStr("");
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
		char* filename=node->fid?fklGetGlobSymbolWithId(node->fid)->symbol:NULL;
		char* line=fklIntToString(node->line);
		size_t len=node->fid?strlen("(:)\n")+strlen(filename)+strlen(line)+1
			:strlen("()\n")+strlen(line)+1;
		char* lineNumber=(char*)malloc(sizeof(char)*len);
		FKL_ASSERT(lineNumber,__func__);
		if(node->fid)
			sprintf(lineNumber,"(%s:%s)\n",line,filename);
		else
			sprintf(lineNumber,"(%s)\n",line);
		free(line);
		t=fklStrCat(t,lineNumber);
		free(lineNumber);
	}
	return t;
}

char* fklGenErrorMessage(FklErrorType type,FklVMrunnable* r,FklVM* exe)
{
	int32_t cp=r->cp;
	FklLineNumTabNode* node=fklFindLineNumTabNode(cp,exe->lnt);
	char* filename=node->fid?fklGetGlobSymbolWithId(node->fid)->symbol:NULL;
	char* line=fklIntToString(node->line);
	size_t len=node->fid?strlen("at line  of \n")+strlen(filename)+strlen(line)+1
		:strlen("at line \n")+strlen(line)+1;
	char* lineNumber=(char*)malloc(sizeof(char)*len);
	FKL_ASSERT(lineNumber,__func__);
	if(node->fid)
		sprintf(lineNumber,"at line %s of %s\n",line,filename);
	else
		sprintf(lineNumber,"at line %s\n",line);
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
			{
				FklVMvalue* v=exe->stack->values[exe->stack->tp-1];
				char* str=fklVMstrToCstr(v->u.str);
				t=fklStrCat(t,str);
				free(str);
			}
			t=fklStrCat(t,"\" ");
			break;
		case FKL_INVALIDSYMBOL:
			t=fklStrCat(t,"Invalid symbol ");
			{
				FklVMvalue* v=exe->stack->values[exe->stack->tp-1];
				char* str=fklVMstrToCstr(v->u.str);
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
				char* str=fklVMstrToCstr(v->u.str);
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
		default:
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
		char* filename=node->fid?fklGetGlobSymbolWithId(node->fid)->symbol:NULL;
		char* line=fklIntToString(node->line);
		size_t len=node->fid?strlen("(:)\n")+strlen(filename)+strlen(line)+1
			:strlen("()\n")+strlen(line)+1;
		char* lineNumber=(char*)malloc(sizeof(char)*len);
		FKL_ASSERT(lineNumber,__func__);
		if(node->fid)
			sprintf(lineNumber,"(%s:%s)\n",line,filename);
		else
			sprintf(lineNumber,"(%s)\n",line);
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
	FklVMvalue* prevBp=stack->values[stack->tp-1];
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
	FklPtrStack* toAccess=fklNewPtrStack(32,16);
	fklPushPtrStack(s,toAccess);
	while(!fklIsPtrStackEmpty(toAccess))
	{
		FklVMvalue* v=fklPopPtrStack(toAccess);
		if(FKL_IS_PAIR(v)||FKL_IS_VECTOR(v))
		{
			if(isInStack(v,totalAccessed,NULL))
			{
				fklPushPtrStack(v,recStack);
				continue;
			}
			if(isInStack(v,recStack,NULL))
				continue;
			fklPushPtrStack(v,beAccessed);
			if(FKL_IS_PAIR(v))
			{
				if(v->u.pair->cdr==v)
					fklPushPtrStack(v,recStack);
				else
					fklPushPtrStack(v->u.pair->cdr,toAccess);
				if(v->u.pair->car==v)
					fklPushPtrStack(v,recStack);
				else
					fklPushPtrStack(v->u.pair->car,toAccess);
			}
			else
			{
				FklVMvec* vec=v->u.vec;
				for(size_t i=vec->size;i>0;i--)
				{
					if(vec->base[i-1]==v)
						fklPushPtrStack(v,recStack);
					else
						fklPushPtrStack(vec->base[i-1],toAccess);
				}
			}
		}
		if(beAccessed->top&&!toAccess->top)
			fklPushPtrStack(fklPopPtrStack(beAccessed),totalAccessed);
	}
	fklFreePtrStack(totalAccessed);
	fklFreePtrStack(beAccessed);
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
						fwrite(v->u.str->str,v->u.str->size,1,fp);
						//fprintf(fp,"%s",v->u.str);
						break;
					case FKL_PROC:
						if(v->u.proc->sid)
							fprintf(fp,"<#proc: %s>",fklGetGlobSymbolWithId(v->u.proc->sid)->symbol);
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
							fprintf(fp,"<#dlproc: %s>",fklGetGlobSymbolWithId(v->u.dlproc->sid)->symbol);
						else
							fprintf(fp,"<#dlproc>");
						break;
					case FKL_ERR:
						fprintf(fp,"%s",v->u.err->message);
						break;
					case FKL_USERDATA:
						fprintf(fp,"#<userdata:%p>",v->u.p);
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

static void fklPrintRawVMstr(FklVMstr* str,FILE* fp)
{
	fklPrintRawCharBuf(str->str,str->size,fp);
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
					fklPrintRawVMstr(v->u.str,fp);
					break;
				case FKL_PROC:
					if(v->u.proc->sid)
						fprintf(fp,"<#proc: %s>"
								,fklGetGlobSymbolWithId(v->u.proc->sid)->symbol);
					else
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
					if(v->u.dlproc->sid)
						fprintf(fp,"#<dlproc: %s>"
								,fklGetGlobSymbolWithId(v->u.dlproc->sid)->symbol);
					else
						fputs("#<dlproc>",fp);
					break;
				case FKL_ERR:
					fprintf(fp,"#<err w:%s t:%s m:%s>"
							,v->u.err->who
							,fklGetGlobSymbolWithId(v->u.err->type)->symbol
							,v->u.err->message);
					break;
				case FKL_USERDATA:
					fprintf(fp,"#<userdata:%p>",v->u.p);
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
	fklFreePtrStack(hasPrintRecStack);
}

FklVMvalue* fklGET_VAL(FklVMvalue* P,FklVMheap* heap)
{
	if(P)
	{
		if(FKL_IS_REF(P))
			return *(FklVMvalue**)(FKL_GET_PTR(P));
		return P;
	}
	return P;
}

int fklSET_REF(FklVMvalue* P,FklVMvalue* V)
{
	if(FKL_IS_REF(P))
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
	int32_t tbsize=cont->tnum;
	FklVMstack* stack=cont->stack;
	FklVMrunnable* state=cont->state;
	FklVMtryBlock* tb=cont->tb;
	free(stack->tpst);
	free(stack->values);
	free(stack);
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
			else if(FKL_IS_PAIR(root))
				cptrType=FKL_PAIR;
			else if(!FKL_IS_REF(root)&&!FKL_IS_MREF(root))
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
						tmpAtm->value.sym=fklCopyStr(fklGetGlobSymbolWithId(FKL_GET_SYM(root))->symbol);
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
									tmpAtm->value.str.size=root->u.str->size;
									tmpAtm->value.str.str=fklCopyMemory(root->u.str->str,root->u.str->size);
									break;
								case FKL_PROC:
									tmpAtm->type=FKL_SYM;
									tmpAtm->value.sym=fklCopyStr("#<proc>");
									break;
								case FKL_DLPROC:
									tmpAtm->type=FKL_SYM;
									tmpAtm->value.sym=fklCopyStr("#<dlproc>");
									break;
								case FKL_CONT:
									tmpAtm->type=FKL_SYM;
									tmpAtm->value.sym=fklCopyStr("#<proc>");
									break;
								case FKL_CHAN:
									tmpAtm->type=FKL_SYM;
									tmpAtm->value.sym=fklCopyStr("#<chan>");
									break;
								case FKL_FP:
									tmpAtm->type=FKL_SYM;
									tmpAtm->value.sym=fklCopyStr("#<fp>");
									break;
								case FKL_ERR:
									tmpAtm->type=FKL_SYM;
									tmpAtm->value.sym=fklCopyStr("#<err>");
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
	fklFreePtrStack(recStack);
	return tmp;
}

void fklInitVMRunningResource(FklVM* vm,FklVMvalue* vEnv,FklVMheap* heap,FklByteCodelnt* code,uint32_t start,uint32_t size)
{
	if(!vEnv->u.env->prev&&vEnv->u.env->num==0)
		fklInitGlobEnv(vEnv->u.env,heap);
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
	fklGC_wait(vm->heap);
	free(vm->lnt);
	fklDeleteCallChain(vm);
	fklFreeVMstack(vm->stack);
	fklFreePtrStack(vm->rstack);
	fklFreePtrStack(vm->tstack);
	free(vm);
}
