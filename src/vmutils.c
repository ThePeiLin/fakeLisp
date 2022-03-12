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

typedef struct Cirular_Ref_List
{
	FklVMpair* pair;
	int32_t count;
	struct Cirular_Ref_List* next;
}CRL;


static CRL* newCRL(FklVMpair* pair,int32_t count)
{
	CRL* tmp=(CRL*)malloc(sizeof(CRL));
	FKL_ASSERT(tmp,"newCRL",__FILE__,__LINE__);
	tmp->pair=pair;
	tmp->count=count;
	tmp->next=NULL;
	return tmp;
}

static int32_t findCRLcount(FklVMpair* pair,CRL* h)
{
	for(;h;h=h->next)
	{
		if(h->pair==pair)
			return h->count;
	}
	return -1;
}

static FklVMpair* hasSameVMpair(FklVMpair* begin,FklVMpair* other,CRL* h)
{
	FklVMpair* tmpPair=NULL;
	if(findCRLcount(begin,h)!=-1||findCRLcount(other,h)!=-1)
		return NULL;
	if(begin==other)
		return begin;

	if((FKL_IS_PAIR(other->car)&&FKL_IS_PAIR(other->car->u.pair->car))&&FKL_IS_PAIR(begin->car))
		tmpPair=hasSameVMpair(begin->car->u.pair,other->car->u.pair->car->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((FKL_IS_PAIR(other->car)&&FKL_IS_PAIR(other->car->u.pair->cdr))&&FKL_IS_PAIR(begin->car))
		tmpPair=hasSameVMpair(begin->car->u.pair,other->car->u.pair->cdr->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((FKL_IS_PAIR(other->car)&&FKL_IS_PAIR(other->car->u.pair->car))&&FKL_IS_PAIR(begin->cdr))
		tmpPair=hasSameVMpair(begin->cdr->u.pair,other->car->u.pair->car->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((FKL_IS_PAIR(other->car)&&FKL_IS_PAIR(other->car->u.pair->cdr))&&FKL_IS_PAIR(begin->cdr))
		tmpPair=hasSameVMpair(begin->cdr->u.pair,other->car->u.pair->cdr->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((FKL_IS_PAIR(other->cdr)&&FKL_IS_PAIR(other->cdr->u.pair->car))&&FKL_IS_PAIR(begin->car))
		tmpPair=hasSameVMpair(begin->car->u.pair,other->cdr->u.pair->car->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((FKL_IS_PAIR(other->cdr)&&FKL_IS_PAIR(other->cdr->u.pair->cdr))&&FKL_IS_PAIR(begin->car))
		tmpPair=hasSameVMpair(begin->car->u.pair,other->cdr->u.pair->cdr->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((FKL_IS_PAIR(other->cdr)&&FKL_IS_PAIR(other->cdr->u.pair->car))&&FKL_IS_PAIR(begin->cdr))
		tmpPair=hasSameVMpair(begin->cdr->u.pair,other->cdr->u.pair->car->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((FKL_IS_PAIR(other->cdr)&&FKL_IS_PAIR(other->cdr->u.pair->cdr))&&FKL_IS_PAIR(begin->cdr))
		tmpPair=hasSameVMpair(begin->cdr->u.pair,other->cdr->u.pair->cdr->u.pair,h);
	if(tmpPair)
		return tmpPair;
	return NULL;
}

FklVMpair* isCircularReference(FklVMpair* begin,CRL* h)
{
	FklVMpair* tmpPair=NULL;
	if(FKL_IS_PAIR(begin->car))
		tmpPair=hasSameVMpair(begin,begin->car->u.pair,h);
	if(tmpPair)
		return tmpPair;
	if(FKL_IS_PAIR(begin->cdr))
		tmpPair=hasSameVMpair(begin,begin->cdr->u.pair,h);
	if(tmpPair)
		return tmpPair;
	return NULL;
}

int8_t isInTheCircle(FklVMpair* obj,FklVMpair* begin,FklVMpair* curPair)
{
	if(obj==curPair)
		return 1;
	if((FKL_IS_PAIR(curPair->car)&&begin==curPair->car->u.pair)||(FKL_IS_PAIR(curPair->cdr)&&begin==curPair->cdr->u.pair))
		return 0;
	return ((FKL_IS_PAIR(curPair->car))&&isInTheCircle(obj,begin,curPair->car->u.pair))||((FKL_IS_PAIR(curPair->cdr))&&isInTheCircle(obj,begin,curPair->cdr->u.pair));
}

uint8_t* fklCopyArry(size_t size,uint8_t* str)
{
	uint8_t* tmp=(uint8_t*)malloc(sizeof(uint8_t)*size);
	FKL_ASSERT(tmp,"fklCopyArry",__FILE__,__LINE__);
	memcpy(tmp,str,size);
	return tmp;
}

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
	FKL_ASSERT(tmp,"fklCopyStack",__FILE__,__LINE__);
	tmp->size=stack->size;
	tmp->tp=stack->tp;
	tmp->bp=stack->bp;
	tmp->values=(FklVMvalue**)malloc(sizeof(FklVMvalue*)*(tmp->size));
	FKL_ASSERT(tmp->values,"fklCopyStack",__FILE__,__LINE__);
	for(;i<stack->tp;i++)
		tmp->values[i]=stack->values[i];
	tmp->tptp=stack->tptp;
	tmp->tpst=NULL;
	tmp->tpsi=stack->tpsi;
	if(tmp->tpsi)
	{
		tmp->tpst=(uint32_t*)malloc(sizeof(uint32_t)*tmp->tpsi);
		FKL_ASSERT(tmp->tpst,"fklCopyStack",__FILE__,__LINE__);
		if(tmp->tptp)memcpy(tmp->tpst,stack->tpst,sizeof(int32_t)*(tmp->tptp));
	}
	return tmp;
}

FklVMtryBlock* fklNewVMtryBlock(FklSid_t sid,uint32_t tp,long int rtp)
{
	FklVMtryBlock* t=(FklVMtryBlock*)malloc(sizeof(FklVMtryBlock));
	FKL_ASSERT(t,"fklNewVMtryBlock",__FILE__,__LINE__);
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

FklVMerrorHandler* fklNewVMerrorHandler(FklSid_t type,uint32_t scp,uint32_t cpc)
{
	FklVMerrorHandler* t=(FklVMerrorHandler*)malloc(sizeof(FklVMerrorHandler));
	FKL_ASSERT(t,"fklNewVMerrorHandler",__FILE__,__LINE__);
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
	FKL_ASSERT(tmp,"fklNewVMrunnable",__FILE__,__LINE__);
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
	LineNumTabNode* node=fklFindLineNumTabNode(cp,exe->lnt);
	char* filename=fklGetGlobSymbolWithId(node->fid)->symbol;
	char* line=fklIntToString(node->line);
	size_t len=strlen("at line  of \n")+strlen(filename)+strlen(line)+1;
	char* lineNumber=(char*)malloc(sizeof(char)*len);
	FKL_ASSERT(lineNumber,"fklGenInvalidSymbolErrorMessag",__FILE__,__LINE__);
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
		LineNumTabNode* node=fklFindLineNumTabNode(r->cp,exe->lnt);
		char* filename=fklGetGlobSymbolWithId(node->fid)->symbol;
		char* line=fklIntToString(node->line);
		size_t len=strlen("(:)\n")+strlen(filename)+strlen(line)+1;
		char* lineNumber=(char*)malloc(sizeof(char)*len);
		FKL_ASSERT(lineNumber,"fklGenErrorMessage",__FILE__,__LINE__);
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
	LineNumTabNode* node=fklFindLineNumTabNode(cp,exe->lnt);
	char* filename=fklGetGlobSymbolWithId(node->fid)->symbol;
	char* line=fklIntToString(node->line);
	size_t len=strlen("at line  of \n")+strlen(filename)+strlen(line)+1;
	char* lineNumber=(char*)malloc(sizeof(char)*len);
	FKL_ASSERT(lineNumber,"fklGenErrorMessage",__FILE__,__LINE__);
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
		LineNumTabNode* node=fklFindLineNumTabNode(r->cp,exe->lnt);
		char* filename=fklGetGlobSymbolWithId(node->fid)->symbol;
		char* line=fklIntToString(node->line);
		size_t len=strlen("(:)\n")+strlen(filename)+strlen(line)+1;
		char* lineNumber=(char*)malloc(sizeof(char)*len);
		FKL_ASSERT(lineNumber,"fklGenErrorMessage",__FILE__,__LINE__);
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
		return FKL_TOOMANYARG;
	FklVMvalue* prevBp=fklGetTopValue(stack);
	stack->bp=FKL_GET_I32(prevBp);
	stack->tp-=1;
	return 0;
}

static void princVMvalue(FklVMvalue* objValue,FILE* fp,CRL** h)
{
	int access=!h;
	CRL* head=NULL;
	if(!h)h=&head;
	FklVMpair* cirPair=NULL;
	int32_t CRLcount=-1;
	int8_t isInCir=0;
	FklVMptrTag tag=FKL_GET_TAG(objValue);
	switch(tag)
	{
		case FKL_NIL_TAG:
			fprintf(fp,"()");
			break;
		case FKL_I32_TAG:
			fprintf(fp,"%d",FKL_GET_I32(objValue));
			break;
		case FKL_CHR_TAG:
			putc(FKL_GET_CHR(objValue),fp);
			break;
		case FKL_SYM_TAG:
			fprintf(fp,"%s",fklGetGlobSymbolWithId(FKL_GET_SYM(objValue))->symbol);
			break;
		case FKL_PTR_TAG:
			{
				switch(objValue->type)
				{
					case FKL_DBL:
						fprintf(fp,"%lf",objValue->u.dbl);
						break;
					case FKL_I64:
						fprintf(fp,"%ld",objValue->u.i64);
						break;
					case FKL_STR:
						fprintf(fp,"%s",objValue->u.str);
						break;
					case FKL_MEM:
					case FKL_CHF:
						{
							FklVMmem* mem=objValue->u.chf;
							FklTypeId_t type=mem->type;
							if(fklIsNativeTypeId(type))
								fklPrintMemoryRef(type,mem,fp);
							else if(IS_CHF(objValue))
								fprintf(fp,"<#memref at %p>",mem->mem);
							else
								fprintf(fp,"<#mem at %p>",mem->mem);
						}
						break;
					case FKL_PRC:
						if(objValue->u.prc->sid)
							fprintf(fp,"<#proc: %s>",fklGetGlobSymbolWithId(objValue->u.prc->sid)->symbol);
						else
							fprintf(fp,"#<proc>");
					case FKL_PAIR:
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
						for(;FKL_IS_PAIR(objValue);objValue=fklGetVMpairCdr(objValue))
						{
							FklVMvalue* tmpValue=fklGetVMpairCar(objValue);
							if(FKL_IS_PAIR(tmpValue)&&(CRLcount=findCRLcount(tmpValue->u.pair,*h))!=-1)
								fprintf(fp,"#%d#",CRLcount);
							else
								princVMvalue(tmpValue,fp,h);
							tmpValue=fklGetVMpairCdr(objValue);
							if(tmpValue!=FKL_VM_NIL&&!FKL_IS_PAIR(tmpValue))
							{
								putc(',',fp);
								princVMvalue(tmpValue,fp,h);
							}
							else if(FKL_IS_PAIR(tmpValue)&&(CRLcount=findCRLcount(tmpValue->u.pair,*h))!=-1)
							{
								fprintf(fp,",#%d#",CRLcount);
								break;
							}
							else if(tmpValue!=FKL_VM_NIL)
								putc(' ',fp);
						}
						putc(')',fp);
						break;
					case FKL_BYTS:
						fklPrintByteStr(objValue->u.byts->size,objValue->u.byts->str,fp,0);
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
						if(objValue->u.dlproc->sid)
							fprintf(fp,"<#dlproc: %s>",fklGetGlobSymbolWithId(objValue->u.dlproc->sid)->symbol);
						else
							fprintf(fp,"<#dlproc>");
						break;
					case FKL_FLPROC:
						if(objValue->u.flproc->sid)
							fprintf(fp,"<#flproc: %s>",fklGetGlobSymbolWithId(objValue->u.flproc->sid)->symbol);
						else
							fprintf(fp,"<#flproc>");
						break;
					case FKL_ERR:
						fprintf(fp,"%s",objValue->u.err->message);
						break;
					default:
						fprintf(fp,"Bad value!");break;
				}
			}
		default:
			break;
	}
	if(access)
	{
		head=*h;
		while(head)
		{
			CRL* prev=head;
			head=head->next;
			free(prev);
		}
	}
}


void fklPrincVMvalue(FklVMvalue* objValue,FILE* fp)
{
	princVMvalue(objValue,fp,NULL);
}

static void prin1VMvalue(FklVMvalue* objValue,FILE* fp,CRL** h)
{
	int access=!h;
	CRL* head=NULL;
	if(!h)h=&head;
	FklVMpair* cirPair=NULL;
	int8_t isInCir=0;
	int32_t CRLcount=-1;
	FklVMptrTag tag=FKL_GET_TAG(objValue);
	switch(tag)
	{
		case FKL_NIL_TAG:
			fprintf(fp,"()");
			break;
		case FKL_I32_TAG:
			fprintf(fp,"%d",FKL_GET_I32(objValue));
			break;
		case FKL_CHR_TAG:
			fklPrintRawChar(FKL_GET_CHR(objValue),fp);
			break;
		case FKL_SYM_TAG:
			fprintf(fp,"%s",fklGetGlobSymbolWithId(FKL_GET_SYM(objValue))->symbol);
			break;
		case FKL_PTR_TAG:
			{
				switch(objValue->type)
				{
					case FKL_DBL:
						fprintf(fp,"%lf",objValue->u.dbl);
						break;
					case FKL_I64:
						fprintf(fp,"%ld",objValue->u.i64);
						break;
					case FKL_STR:
						fklPrintRawString(objValue->u.str,fp);
						break;
					case FKL_MEM:
					case FKL_CHF:
						{
							FklVMmem* mem=objValue->u.chf;
							FklTypeId_t type=mem->type;
							if(fklIsNativeTypeId(type))
								fklPrintMemoryRef(type,mem,fp);
							else if(IS_CHF(objValue))
								fprintf(fp,"<#memref at %p>",mem->mem);
							else
								fprintf(fp,"<#mem at %p>",mem->mem);
						}
						break;
					case FKL_PRC:
						if(objValue->u.prc->sid)
							fprintf(fp,"<#proc: %s>",fklGetGlobSymbolWithId(objValue->u.prc->sid)->symbol);
						else
							fprintf(fp,"#<proc>");
						break;
					case FKL_PAIR:
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
						for(;FKL_IS_PAIR(objValue);objValue=fklGetVMpairCdr(objValue))
						{
							FklVMvalue* tmpValue=fklGetVMpairCar(objValue);
							if(FKL_IS_PAIR(tmpValue)&&(CRLcount=findCRLcount(tmpValue->u.pair,*h))!=-1)
								fprintf(fp,"#%d#",CRLcount);
							else
								prin1VMvalue(tmpValue,fp,h);
							tmpValue=fklGetVMpairCdr(objValue);
							if(tmpValue!=FKL_VM_NIL&&!FKL_IS_PAIR(tmpValue))
							{
								putc(',',fp);
								prin1VMvalue(tmpValue,fp,h);
							}
							else if(FKL_IS_PAIR(tmpValue)&&(CRLcount=findCRLcount(tmpValue->u.pair,*h))!=-1)
							{
								fprintf(fp,",#%d#",CRLcount);
								break;
							}
							else if(tmpValue!=FKL_VM_NIL)
								putc(' ',fp);
						}
						putc(')',fp);
						break;
					case FKL_BYTS:
						fklPrintByteStr(objValue->u.byts->size,objValue->u.byts->str,fp,1);
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
						if(objValue->u.dlproc->sid)
							fprintf(fp,"<#dlproc: %s>",fklGetGlobSymbolWithId(objValue->u.dlproc->sid)->symbol);
						else
							fprintf(fp,"<#dlproc>");
						break;
					case FKL_FLPROC:
						if(objValue->u.flproc->sid)
							fprintf(fp,"<#flproc: %s>",fklGetGlobSymbolWithId(objValue->u.flproc->sid)->symbol);
						else
							fprintf(fp,"<#flproc>");
						break;
					case FKL_ERR:
						fprintf(fp,"<#err w:%s t:%s m:%s>",objValue->u.err->who,fklGetGlobSymbolWithId(objValue->u.err->type)->symbol,objValue->u.err->message);
						break;
					default:fprintf(fp,"Bad value!");break;
				}
			}
		default:
			break;
	}
	if(access)
	{
		head=*h;
		while(head)
		{
			CRL* prev=head;
			head=head->next;
			free(prev);
		}
	}
}

void fklPrin1VMvalue(FklVMvalue* objValue,FILE* fp)
{
	prin1VMvalue(objValue,fp,NULL);
}

FklVMvalue* fklGET_VAL(FklVMvalue* P,FklVMheap* heap)
{
	if(P)
	{
		if(FKL_IS_REF(P))
			return *(FklVMvalue**)(FKL_GET_PTR(P));
		else if(IS_CHF(P))
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

int fklSET_REF(FklVMvalue* P,FklVMvalue* V)
{
	if(FKL_IS_MEM(P)||IS_CHF(P))
	{
		FklVMmem* mem=P->u.chf;
		if(mem->type<=0)
			return 1;
		else if(!fklIsNativeTypeId(mem->type))
		{
			if(!FKL_IS_I32(V)&&!FKL_IS_I64(V))
				return 1;
			mem->mem=(uint8_t*)(FKL_IS_I32(V)?FKL_GET_I32(V):V->u.i64);
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
	FKL_ASSERT(tmp,"fklNewVMcontinuation",__FILE__,__LINE__);
	FklVMrunnable* state=(FklVMrunnable*)malloc(sizeof(FklVMrunnable)*size);
	FKL_ASSERT(state,"fklNewVMcontinuation",__FILE__,__LINE__);
	int32_t tbnum=tstack->top;
	FklVMtryBlock* tb=(FklVMtryBlock*)malloc(sizeof(FklVMtryBlock)*tbnum);
	FKL_ASSERT(tb,"fklNewVMcontinuation",__FILE__,__LINE__);
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
	FklAstCptr* tmp=fklNewCptr(curline,NULL);
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
		else if(!FKL_IS_REF(root)&&!IS_CHF(root))
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
							case FKL_DBL:
								tmpAtm->value.dbl=root->u.dbl;
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
							case FKL_PRC:
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
	return tmp;
}

void fklInitVMRunningResource(FklVM* vm,FklVMenv* vEnv,FklVMheap* heap,FklByteCodelnt* code,uint32_t start,uint32_t size)
{
	if(!vEnv->prev)
	{
		fklIncreaseVMenvRefcount(vEnv);
		fklInitGlobEnv(vEnv,heap);
	}
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

void fklUnInitVMRunningResource(FklVM* vm)
{
	free(vm->lnt);
	fklDeleteCallChain(vm);
	fklFreeVMstack(vm->stack);
	fklFreePtrStack(vm->rstack);
	fklFreePtrStack(vm->tstack);
	free(vm);
}
