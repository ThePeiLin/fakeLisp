#include<fakeLisp/vm.h>
#include<fakeLisp/opcode.h>
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

FklVMvalue* fklMakeVMf64(double f,FklVM* vm)
{
	return fklCreateVMvalueToStack(FKL_TYPE_F64,&f,vm);
}

FklVMvalue* fklMakeVMint(int64_t r64,FklVM* vm)
{
	if(r64>FKL_FIX_INT_MAX||r64<FKL_FIX_INT_MIN)
		return fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,fklCreateBigInt(r64),vm);
	else
		return FKL_MAKE_VM_FIX(r64);
}

FklVMvalue* fklMakeVMuint(uint64_t r64,FklVM* vm)
{
	if(r64>FKL_FIX_INT_MAX)
		return fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,fklCreateBigIntU(r64),vm);
	else
		return FKL_MAKE_VM_FIX(r64);
}

FklVMvalue* fklMakeVMintD(double r64,FklVM* vm)
{
	if(isgreater(r64,(double)FKL_FIX_INT_MAX)||isless(r64,FKL_FIX_INT_MIN))
		return fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,fklCreateBigIntD(r64),vm);
	else
		return FKL_MAKE_VM_FIX(r64);
}

inline int fklIsFixint(const FklVMvalue* p)
{
	return FKL_IS_FIX(p);
}

inline int fklIsInt(const FklVMvalue* p)
{
	return FKL_IS_FIX(p)||FKL_IS_BIG_INT(p);
}

inline int fklIsVMnumber(const FklVMvalue* p)
{
	return fklIsInt(p)||FKL_IS_F64(p);
}

static inline FklVMvalue* get_initial_fast_value(const FklVMvalue* pr)
{
	return FKL_IS_PAIR(pr)?pr->u.pair->cdr:FKL_VM_NIL;
}

static inline FklVMvalue* get_fast_value(const FklVMvalue* head)
{
	return (FKL_IS_PAIR(head)
			&&FKL_IS_PAIR(head->u.pair->cdr)
			&&FKL_IS_PAIR(head->u.pair->cdr->u.pair->cdr))
		?head->u.pair->cdr->u.pair->cdr
		:FKL_VM_NIL;
}

int fklIsList(const FklVMvalue* p)
{
	FklVMvalue* fast=get_initial_fast_value(p);
	for(;FKL_IS_PAIR(p)
			;p=p->u.pair->cdr
			,fast=get_fast_value(fast))
		if(fast==p)
			return 0;
	if(p!=FKL_VM_NIL)
		return 0;
	return 1;
}

int fklIsSymbolList(const FklVMvalue* p)
{
	for(;p!=FKL_VM_NIL;p=p->u.pair->cdr)
		if(!FKL_IS_PAIR(p)||!FKL_IS_SYM(p->u.pair->car))
			return 0;
	return 1;
}

inline int64_t fklGetInt(const FklVMvalue* p)
{
	return FKL_IS_FIX(p)
		?FKL_GET_FIX(p)
		:fklBigIntToI64(p->u.bigInt);
}

inline uint64_t fklGetUint(const FklVMvalue* p)
{
	return FKL_IS_FIX(p)
		?FKL_GET_FIX(p)
		:fklBigIntToU64(p->u.bigInt);
}

inline int fklVMnumberLt0(const FklVMvalue* p)
{
	return FKL_IS_FIX(p)
		?FKL_GET_FIX(p)<0
		:FKL_IS_F64(p)
		?isless(p->u.f64,0.0)
		:!FKL_IS_0_BIG_INT(p->u.bigInt)&&p->u.bigInt->neg;
}

inline double fklGetDouble(const FklVMvalue* p)
{
	return FKL_IS_FIX(p)
		?FKL_GET_FIX(p)
		:(FKL_IS_BIG_INT(p))
		?fklBigIntToDouble(p->u.bigInt)
		:p->u.f64;
}

FklVMerrorHandler* fklCreateVMerrorHandler(FklSid_t* typeIds,uint32_t errTypeNum,uint8_t* spc,uint64_t cpc)
{
	FklVMerrorHandler* t=(FklVMerrorHandler*)malloc(sizeof(FklVMerrorHandler));
	FKL_ASSERT(t);
	t->typeIds=typeIds;
	t->num=errTypeNum;
	t->proc.spc=spc;
	t->proc.end=spc+cpc;
	t->proc.sid=0;
	return t;
}

void fklDestroyVMerrorHandler(FklVMerrorHandler* h)
{
	free(h->typeIds);
	free(h);
}

inline static FklVMvalue* get_compound_frame_code_obj(FklVMframe* frame)
{
	return frame->u.c.proc->u.proc->codeObj;
}

inline void fklPrintErrBacktrace(FklVMvalue* ev,FklVM* exe)
{
	FklVMerror* err=ev->u.err;
	fprintf(stderr,"error in ");
	fklPrintString(err->who,stderr);
	fprintf(stderr,": ");
	fklPrintString(err->message,stderr);
	fprintf(stderr,"\n");
	for(FklVMframe* cur=exe->frames;cur;cur=cur->prev)
	{
		if(cur->type==FKL_FRAME_COMPOUND)
		{
			// if(fklGetCompoundFrameSid(cur)!=0)
			// {
			// 	fprintf(stderr,"at proc:");
			// 	fklPrintString(fklGetSymbolWithId(fklGetCompoundFrameSid(cur),exe->symbolTable)->symbol
			// 			,stderr);
			// }
			// else
			// {
			if(cur->prev)
			{
				FklVMproc* proc=fklGetCompoundFrameProc(cur)->u.proc;
				if(proc->sid)
				{
					fprintf(stderr,"at proc:");
					fklPrintString(fklGetSymbolWithId(proc->sid,exe->symbolTable)->symbol
							,stderr);
				}
				else
				{
					FklFuncPrototype* pt=NULL;fklGetCompoundFrameProcPrototype(cur,exe);
					FklSid_t sid=fklGetCompoundFrameSid(cur);
					if(!sid)
					{
						pt=fklGetCompoundFrameProcPrototype(cur,exe);
						sid=pt->sid;
					}
					if(pt->sid)
					{
						fprintf(stderr,"at proc:");
						fklPrintString(fklGetSymbolWithId(pt->sid,exe->symbolTable)->symbol
								,stderr);
					}
					else
					{
						fprintf(stderr,"at <proc ");
						if(pt->fid)
							fklPrintRawString(fklGetSymbolWithId(pt->fid,exe->symbolTable)->symbol,stderr);
						else
							fputs("stdin",stderr);
						fprintf(stderr,":%lu>",pt->line);
					}
				}
			}
			else
				fprintf(stderr,"at <top>");
			// }
			FklByteCodelnt* codeObj=get_compound_frame_code_obj(cur)->u.code;
			FklLineNumTabNode* node=fklFindLineNumTabNode(fklGetCompoundFrameCode(cur)-codeObj->bc->code
					,codeObj->ls
					,codeObj->l);
			if(node->fid)
			{
				fprintf(stderr,"(%u:",node->line);
				fklPrintString(fklGetSymbolWithId(node->fid,exe->symbolTable)->symbol,stderr);
				fprintf(stderr,")\n");
			}
			else
				fprintf(stderr,"(%u)\n",node->line);
		}
		else
			fklDoPrintBacktrace(cur,stderr,exe->symbolTable);
	}
}

int fklRaiseVMerror(FklVMvalue* ev,FklVM* exe)
{
	FklVMframe* frame=exe->frames;
	for(;frame;frame=frame->prev)
		if(frame->errorCallBack!=NULL&&frame->errorCallBack(frame,ev,exe))
			break;
	if(frame==NULL)
	{
		fklPrintErrBacktrace(ev,exe);
		longjmp(exe->buf,1);
	}
	return 255;
}

FklVMframe* fklCopyVMframe(FklVMframe* f,FklVMframe* prev,FklVM* exe)
{
	switch(f->type)
	{
		case FKL_FRAME_COMPOUND:
			return fklCreateVMframeWithCompoundFrame(f,prev,exe->gc);
			break;
		case FKL_FRAME_OTHEROBJ:
			{
				FklVMframe* r=fklCreateOtherObjVMframe(f->u.o.t,prev);
				fklDoCopyObjFrameContext(f,r,exe);
				return r;
			}
			break;
	}
	return NULL;
}

inline static void init_frame_var_ref(FklVMCompoundFrameVarRef* lr)
{
	lr->base=0;
	lr->lcount=0;
	lr->loc=NULL;
	lr->lref=NULL;
	lr->ref=NULL;
	lr->rcount=0;
	lr->lrefl=NULL;
}

FklVMframe* fklCreateVMframeWithCompoundFrame(const FklVMframe* f,FklVMframe* prev,FklVMgc* gc)
{
	FklVMframe* tmp=(FklVMframe*)malloc(sizeof(FklVMframe));
	FKL_ASSERT(tmp);
	tmp->type=FKL_FRAME_COMPOUND;
	tmp->prev=prev;
	tmp->errorCallBack=f->errorCallBack;
	FklVMCompoundFrameData* fd=&tmp->u.c;
	const FklVMCompoundFrameData* pfd=&f->u.c;
	fd->sid=pfd->sid;
	fd->pc=pfd->pc;
	fd->spc=pfd->spc;
	fd->end=pfd->end;
	fd->proc=FKL_VM_NIL;
	fklSetRef(&fd->proc,pfd->proc,gc);
	fd->mark=pfd->mark;
	fd->tail=pfd->tail;
	FklVMCompoundFrameVarRef* lr=&fd->lr;
	const FklVMCompoundFrameVarRef* plr=&pfd->lr;
	*lr=*plr;
	FklVMvarRef** lref=plr->lref?fklCopyMemory(plr->lref,sizeof(FklVMvarRef*)*plr->lcount):NULL;
	FklVMvarRefList* nl=NULL;
	for(FklVMvarRefList* ll=lr->lrefl;ll;ll=ll->next)
	{
		fklMakeVMvarRefRef(ll->ref);
		FklVMvarRefList* rl=(FklVMvarRefList*)malloc(sizeof(FklVMvarRefList));
		FKL_ASSERT(rl);
		rl->ref=ll->ref;
		rl->next=nl;
		nl=rl;
	}
	lr->lref=lref;
	lr->lrefl=nl;
	return tmp;
}

FklVMframe* fklCreateVMframeWithCodeObj(FklVMvalue* codeObj,FklVMframe* prev,FklVMgc* gc)
{
	FklVMproc* proc=fklCreateVMprocWithWholeCodeObj(codeObj,gc);
	proc->protoId=1;
	FklVMvalue* procV=fklCreateVMvalueNoGC(FKL_TYPE_PROC,proc,gc);
	return fklCreateVMframeWithProcValue(procV,NULL);
}

inline void fklInitMainVMframeWithProc(FklVM* exe
		,FklVMframe* tmp
		,FklVMproc* code
		,FklVMframe* prev
		,FklFuncPrototypes* pts)
{
	tmp->errorCallBack=NULL;
	tmp->type=FKL_FRAME_COMPOUND;
	tmp->prev=prev;

	FklVMCompoundFrameData* f=&tmp->u.c;
	f->sid=0;
	f->pc=NULL;
	f->spc=NULL;
	f->end=NULL;
	f->mark=0;
	f->tail=0;
	if(code)
	{
		f->pc=code->spc;
		f->spc=code->spc;
		f->end=code->end;
		f->sid=code->sid;
		FklVMCompoundFrameVarRef* lr=&f->lr;
		code->closure=lr->ref;
		code->rcount=lr->rcount;
		FklFuncPrototype* pt=&pts->pts[code->protoId];
		uint32_t count=pt->lcount;
		code->lcount=count;
		FklVMvalue** loc=fklAllocSpaceForLocalVar(exe,count);
		lr->base=0;
		lr->loc=loc;
		lr->lcount=count;
		lr->lref=NULL;
		lr->lrefl=NULL;
	}
	else
		init_frame_var_ref(&f->lr);
}

inline void fklUpdateAllVarRef(FklVMframe* f,FklVMvalue** locv)
{
	for(;f;f=f->prev)
		if(f->type==FKL_FRAME_COMPOUND)
		{
			FklVMCompoundFrameVarRef* lr=fklGetCompoundFrameLocRef(f);
			FklVMvalue** loc=&locv[lr->base];
			for(FklVMvarRefList* ll=lr->lrefl;ll;ll=ll->next)
			{
				FklVMvarRef* ref=ll->ref;
				if(ref->ref!=&ref->v)
					ref->ref=&loc[ref->idx];
			}
			lr->loc=loc;
		}
}

// inline void fklInitMainVMframeWithProcForRepl(FklVM* exe
// 		,FklVMframe* tmp
// 		,FklVMproc* code
// 		,FklVMframe* prev
// 		,FklFuncPrototypes* pts)
// {
// 	tmp->errorCallBack=NULL;
// 	tmp->type=FKL_FRAME_COMPOUND;
// 	tmp->prev=prev;
//
// 	FklVMCompoundFrameData* f=&tmp->u.c;
// 	f->sid=0;
// 	f->pc=NULL;
// 	f->spc=NULL;
// 	f->end=NULL;
// 	f->mark=0;
// 	f->tail=0;
// 	if(code)
// 	{
// 		f->pc=code->spc;
// 		f->spc=code->spc;
// 		f->end=code->end;
// 		f->sid=code->sid;
// 		FklVMCompoundFrameVarRef* lr=&f->lr;
// 		code->closure=lr->ref;
// 		code->count=lr->rcount;
// 		FklFuncPrototype* pt=&pts->pts[code->protoId];
// 		uint32_t count=pt->lcount;
// 		FklVMvalue** loc=(FklVMvalue**)realloc(exe->locv,count*sizeof(FklVMvalue*));
// 		FKL_ASSERT(loc||!count);
// 		memset(&loc[lr->lcount],0,(count-lr->lcount)*sizeof(FklVMvalue*));
// 		if(lr->lref&&loc!=lr->loc)
// 		{
// 			FklVMvarRef** lref=(FklVMvarRef**)realloc(lr->lref,count*sizeof(FklVMvarRef*));
// 			FKL_ASSERT(lref||!count);
// 			memset(&lref[lr->lcount],0,(count-lr->lcount)*sizeof(FklVMvalue*));
// 			for(FklVMvarRefList* l=lr->lrefl;l;l=l->next)
// 				l->ref->ref=&loc[l->ref->idx];
// 			lr->lref=lref;
// 		}
// 		exe->locv=loc;
// 		exe->ltp=count;
// 		exe->llast=count;
// 		lr->loc=loc;
// 		lr->lcount=count;
// 	}
// 	else
// 		init_frame_var_ref(&f->lr);
// }

FklVMframe* fklCreateVMframeWithProcValue(FklVMvalue* proc,FklVMframe* prev)
{
	FklVMproc* code=proc->u.proc;
	FklVMframe* tmp=(FklVMframe*)malloc(sizeof(FklVMframe));
	FKL_ASSERT(tmp);
	tmp->errorCallBack=NULL;
	tmp->type=FKL_FRAME_COMPOUND;
	tmp->prev=prev;

	FklVMCompoundFrameData* f=&tmp->u.c;
	f->sid=0;
	f->pc=NULL;
	f->spc=NULL;
	f->end=NULL;
	f->proc=NULL;
	f->mark=0;
	f->tail=0;
	init_frame_var_ref(&f->lr);
	if(code)
	{
		f->lr.ref=code->closure;
		f->lr.rcount=code->rcount;
		f->pc=code->spc;
		f->spc=code->spc;
		f->end=code->end;
		f->sid=code->sid;
		f->proc=proc;
	}
	return tmp;
}

FklVMframe* fklCreateOtherObjVMframe(const FklVMframeContextMethodTable* t,FklVMframe* prev)
{
	FklVMframe* r=(FklVMframe*)calloc(1,sizeof(FklVMframe));
	FKL_ASSERT(r);
	r->prev=prev;
	r->errorCallBack=NULL;
	r->type=FKL_FRAME_OTHEROBJ;
	r->u.o.t=t;
	return r;
}

void fklDestroyVMframe(FklVMframe* frame,FklVM* exe)
{
	if(frame->type==FKL_FRAME_OTHEROBJ)
		fklDoFinalizeObjFrame(frame,&exe->sf);
	else
		fklDoFinalizeCompoundFrame(frame,exe);
}

FklString* fklGenInvalidSymbolErrorMessage(char* str,int _free,FklBuiltInErrorType type)
{
	FklString* t=fklCreateEmptyString();
	switch(type)
	{
		case FKL_ERR_IMPORTFAILED:
			fklStringCstrCat(&t,"Failed to import dll:\"");
			fklStringCstrCat(&t,str);
			fklStringCstrCat(&t,"\"");
			break;
		case FKL_ERR_LOADDLLFAILD:
			{
				char* errStr=dlerror();
				if(errStr)
					fklStringCstrCat(&t,errStr);
				else
				{
					fklStringCstrCat(&t,"Faild to load dll \"");
					fklStringCstrCat(&t,str);
					fklStringCstrCat(&t,"\"");
				}
			}
			break;
		case FKL_ERR_INVALIDSYMBOL:
			fklStringCstrCat(&t,"Invalid symbol ");
			fklStringCstrCat(&t,str);
			break;
		case FKL_ERR_FILEFAILURE:
			fklStringCstrCat(&t,"Failed for file:\"");
			fklStringCstrCat(&t,str);
			fklStringCstrCat(&t,"\"");
			break;
		case FKL_ERR_SYMUNDEFINE:
			fklStringCstrCat(&t,"Symbol ");
			fklStringCstrCat(&t,str);
			fklStringCstrCat(&t," is undefined");
			break;
		default:
			break;
	}
	if(_free)
		free(str);
	return t;
}

FklString* fklGenErrorMessage(FklBuiltInErrorType type)
{
	static const char* builtinErrorMessages[]=
	{
		NULL,
		NULL,
		NULL,
		"Invalid expression",
		NULL,
		"Invalid pattern ",
		"Incorrect type of values",
		"Stack error",
		"Too many arguements",
		"Too few arguements",
		"Can't create thread",
		NULL,
		NULL,
		"Try to call an object that can't be call",
		NULL,
		NULL,
		NULL,
		"Unexpected eof",
		"Divided by zero",
		NULL,
		"Invalid value",
		"Invalid assign",
		"Invalid access",
		NULL,
		NULL,
		"Failed to create big-int from mem",
		"List differ in length",
		"Attempt to get a continuation cross C-call boundary",
		"Radix should be 8,10 or 16",
		"No value for key",
		"Number should not be less than 0",
		"Circular reference occurs",
		"Unsupported operation",
	};
	const char* s=builtinErrorMessages[type];
	FKL_ASSERT(s);
	return fklCreateStringFromCstr(s);
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
		PRT_HASH_ITEM,
	}state;
	FklVMvalue* v;
}PrtElem;

static PrtElem* createPrtElem(enum PrintingState state,FklVMvalue* v)
{
	PrtElem* r=(PrtElem*)malloc(sizeof(PrtElem));
	FKL_ASSERT(r);
	r->state=state;
	r->v=v;
	return r;
}

typedef struct
{
	FklVMvalue* v;
	size_t w;
}VMvalueHashItem;

static uintptr_t _VMvalue_hashFunc(void* key)
{
	FklVMvalue* v=*(FklVMvalue**)key;
	return (uintptr_t)v>>3;
}

static void _VMvalue_setVal(void* d0,void* d1)
{
	*(VMvalueHashItem*)d0=*(VMvalueHashItem*)d1;
}

static FklHashTableMetaTable VMvalueHashMetaTable=
{
	.size=sizeof(VMvalueHashItem),
	.__setKey=fklHashDefaultSetPtrKey,
	.__setVal=_VMvalue_setVal,
	.__hashFunc=_VMvalue_hashFunc,
	.__uninitItem=fklDoNothingUnintHashItem,
	.__keyEqual=fklHashPtrKeyEqual,
	.__getKey=fklHashDefaultGetKey,
};

FklHashTable* fklCreateValueSetHashtable(void)
{
	return fklCreateHashTable(&VMvalueHashMetaTable);
}

static void putValueInSet(FklHashTable* s,FklVMvalue* v)
{
	uint32_t num=s->num;
	VMvalueHashItem* h=fklPutHashItem(&v,s);
	h->w=num;
}

static int isInValueSet(FklVMvalue* v,FklHashTable* t,size_t* w)
{
	VMvalueHashItem* item=fklGetHashItem(&v,t);
	if(item&&w)
		*w=item->w;
	return item!=NULL;
}

void fklScanCirRef(FklVMvalue* s,FklHashTable* recValueSet)
{
	FklHashTable* totalAccessed=fklCreateValueSetHashtable();
	FklPtrStack beAccessed=FKL_STACK_INIT;
	fklInitPtrStack(&beAccessed,32,16);
	fklPushPtrStack(s,&beAccessed);
	while(!fklIsPtrStackEmpty(&beAccessed))
	{
		FklVMvalue* v=fklPopPtrStack(&beAccessed);
		if(FKL_IS_PAIR(v)||FKL_IS_VECTOR(v)||FKL_IS_BOX(v)||FKL_IS_HASHTABLE(v))
		{
			if(isInValueSet(v,totalAccessed,NULL))
			{
				putValueInSet(recValueSet,v);
				continue;
			}
			if(isInValueSet(v,recValueSet,NULL))
				continue;
			putValueInSet(totalAccessed,v);
			if(FKL_IS_PAIR(v))
			{
				if(v->u.pair->cdr==v||v->u.pair->car==v)
					putValueInSet(recValueSet,v);
				else
				{
					fklPushPtrStack(v->u.pair->cdr,&beAccessed);
					fklPushPtrStack(v->u.pair->car,&beAccessed);
				}
			}
			else if(FKL_IS_VECTOR(v))
			{
				FklVMvec* vec=v->u.vec;
				for(size_t i=vec->size;i>0;i--)
				{
					if(vec->base[i-1]==v)
						putValueInSet(recValueSet,v);
					else
						fklPushPtrStack(vec->base[i-1],&beAccessed);
				}
			}
			else if(FKL_IS_HASHTABLE(v))
			{
				FklHashTable* ht=v->u.hash;
				FklPtrStack stack=FKL_STACK_INIT;
				fklInitPtrStack(&stack,32,16);
				for(FklHashTableNodeList* list=ht->list;list;list=list->next)
					fklPushPtrStack(list->node->data,&stack);
				while(!fklIsPtrStackEmpty(&stack))
				{
					FklVMhashTableItem* item=fklPopPtrStack(&stack);
					if(item->v==v||item->key==v)
						putValueInSet(recValueSet,v);
					else
					{
						fklPushPtrStack(item->v,&beAccessed);
						fklPushPtrStack(item->key,&beAccessed);
					}
				}
				fklUninitPtrStack(&stack);
			}
			else
			{
				if(v->u.box==v)
					putValueInSet(recValueSet,v);
				else
					fklPushPtrStack(v->u.box,&beAccessed);
			}
		}
	}
	fklDestroyHashTable(totalAccessed);
	fklUninitPtrStack(&beAccessed);
}

#define VMVALUE_PRINTER_ARGS FklVMvalue* v,FILE* fp,FklSymbolTable* table
static void vmvalue_f64_printer(VMVALUE_PRINTER_ARGS)
{
	fprintf(fp,"%lf",v->u.f64);
}

static void vmvalue_big_int_printer(VMVALUE_PRINTER_ARGS)
{
	fklPrintBigInt(v->u.bigInt,fp);
}

static void vmvalue_string_princ(VMVALUE_PRINTER_ARGS)
{
	fwrite(v->u.str->str,v->u.str->size,1,fp);
}

static void vmvalue_bytevector_printer(VMVALUE_PRINTER_ARGS)
{
	fklPrintRawBytevector(v->u.bvec,fp);
}

static void vmvalue_userdata_princ(VMVALUE_PRINTER_ARGS)
{
	fklPrincVMudata(v->u.ud,fp,table);
}

static void vmvalue_proc_printer(VMVALUE_PRINTER_ARGS)
{
	if(v->u.proc->sid)
	{
		fprintf(fp,"#<proc ");
		fklPrintRawSymbol(fklGetSymbolWithId(v->u.proc->sid,table)->symbol,fp);
		fputc('>',fp);
	}
	else
		fprintf(fp,"#<proc %p>",v->u.proc);

}

static void vmvalue_chanl_printer(VMVALUE_PRINTER_ARGS)
{
	fprintf(fp,"#<chanl %p>",v->u.chan);
}

static void vmvalue_fp_printer(VMVALUE_PRINTER_ARGS)
{
	if(v->u.fp->fp==stdin)
		fputs("#<fp stdin>",fp);
	else if(v->u.fp->fp==stdout)
		fputs("#<fp stdout>",fp);
	else if(v->u.fp->fp==stderr)
		fputs("#<fp stderr>",fp);
	else
		fprintf(fp,"#<fp %p>",v->u.fp);
}

static void vmvalue_dll_printer(VMVALUE_PRINTER_ARGS)
{
	fprintf(fp,"#<dll %p>",v->u.dll);
}

static void vmvalue_dlproc_printer(VMVALUE_PRINTER_ARGS)
{
	if(v->u.dlproc->sid)
	{
		fprintf(fp,"#<dlproc ");
		fklPrintRawSymbol(fklGetSymbolWithId(v->u.dlproc->sid,table)->symbol,fp);
		fputc('>',fp);
	}
	else
		fprintf(fp,"#<dlproc %p>",v->u.dlproc);
}

static void vmvalue_error_princ(VMVALUE_PRINTER_ARGS)
{
	fklPrintString(v->u.err->message,fp);
}

static void vmvalue_code_obj_printer(VMVALUE_PRINTER_ARGS)
{
	fprintf(fp,"#<code-obj %p>",v->u.code);
}

static void (*VMvaluePtrPrincTable[FKL_VMVALUE_PTR_TYPE_NUM])(FklVMvalue*,FILE*,FklSymbolTable*)=
{
	vmvalue_f64_printer,
	vmvalue_big_int_printer,
	vmvalue_string_princ,
	NULL,
	NULL,
	NULL,
	vmvalue_bytevector_printer,
	vmvalue_userdata_princ,
	vmvalue_proc_printer,
	vmvalue_chanl_printer,
	vmvalue_fp_printer,
	vmvalue_dll_printer,
	vmvalue_dlproc_printer,
	vmvalue_error_princ,
	NULL,
	vmvalue_code_obj_printer,
};

static void vmvalue_ptr_ptr_princ(VMVALUE_PRINTER_ARGS)
{
	VMvaluePtrPrincTable[v->type](v,fp,table);
}

static void vmvalue_nil_ptr_print(VMVALUE_PRINTER_ARGS)
{
	fputs("()",fp);
}

static void vmvalue_fix_ptr_print(VMVALUE_PRINTER_ARGS)
{
	fprintf(fp,"%ld",FKL_GET_FIX(v));
}

static void vmvalue_sym_ptr_princ(VMVALUE_PRINTER_ARGS)
{
	fklPrintString(fklGetSymbolWithId(FKL_GET_SYM(v),table)->symbol,fp);
}

static void vmvalue_chr_ptr_princ(VMVALUE_PRINTER_ARGS)
{
	putc(FKL_GET_CHR(v),fp);
}

static void (*VMvaluePrincTable[FKL_PTR_TAG_NUM])(VMVALUE_PRINTER_ARGS)=
{
	vmvalue_ptr_ptr_princ,
	vmvalue_nil_ptr_print,
	vmvalue_fix_ptr_print,
	vmvalue_sym_ptr_princ,
	vmvalue_chr_ptr_princ,
};

static void princVMatom(FklVMvalue* v,FILE* fp,FklSymbolTable* table)
{
	VMvaluePrincTable[(FklVMptrTag)FKL_GET_TAG(v)](v,fp,table);
}

static void vmvalue_sym_ptr_prin1(VMVALUE_PRINTER_ARGS)
{
	fklPrintRawSymbol(fklGetSymbolWithId(FKL_GET_SYM(v),table)->symbol,fp);
}

static void vmvalue_chr_ptr_prin1(VMVALUE_PRINTER_ARGS)
{
	fklPrintRawChar(FKL_GET_CHR(v),fp);
}

static void vmvalue_string_prin1(VMVALUE_PRINTER_ARGS)
{
	fklPrintRawString(v->u.str,fp);
}

static void vmvalue_userdata_prin1(VMVALUE_PRINTER_ARGS)
{
	fklPrin1VMudata(v->u.ud,fp,table);
}

static void vmvalue_error_prin1(VMVALUE_PRINTER_ARGS)
{
	fprintf(fp,"#<err t:");
	fklPrintRawSymbol(fklGetSymbolWithId(v->u.err->type,table)->symbol,fp);
	fprintf(fp," w:");
	fklPrintRawString(v->u.err->who,fp);
	fprintf(fp," m:");
	fklPrintRawString(v->u.err->message,fp);
	fprintf(fp,">");
}

static void (*VMvaluePtrPrin1Table[FKL_VMVALUE_PTR_TYPE_NUM])(FklVMvalue*,FILE*,FklSymbolTable*)=
{
	vmvalue_f64_printer,
	vmvalue_big_int_printer,
	vmvalue_string_prin1,
	NULL,
	NULL,
	NULL,
	vmvalue_bytevector_printer,
	vmvalue_userdata_prin1,
	vmvalue_proc_printer,
	vmvalue_chanl_printer,
	vmvalue_fp_printer,
	vmvalue_dll_printer,
	vmvalue_dlproc_printer,
	vmvalue_error_prin1,
	NULL,
	vmvalue_code_obj_printer,
};

static void vmvalue_ptr_ptr_prin1(VMVALUE_PRINTER_ARGS)
{
	VMvaluePtrPrin1Table[v->type](v,fp,table);
}

static void (*VMvaluePrin1Table[FKL_PTR_TAG_NUM])(VMVALUE_PRINTER_ARGS)=
{
	vmvalue_ptr_ptr_prin1,
	vmvalue_nil_ptr_print,
	vmvalue_fix_ptr_print,
	vmvalue_sym_ptr_prin1,
	vmvalue_chr_ptr_prin1,
};

static void prin1VMatom(FklVMvalue* v,FILE* fp,FklSymbolTable* table)
{
	VMvaluePrin1Table[(FklVMptrTag)FKL_GET_TAG(v)](v,fp,table);
}
#undef VMVALUE_PRINTER_ARGS

void fklPrin1VMvalue(FklVMvalue* value,FILE* fp,FklSymbolTable* table)
{
	fklPrintVMvalue(value,fp,prin1VMatom,table);
}

void fklPrincVMvalue(FklVMvalue* value,FILE* fp,FklSymbolTable* table)
{
	fklPrintVMvalue(value,fp,princVMatom,table);
}

typedef struct
{
	PrtElem* key;
	PrtElem* v;
}HashPrtElem;

static HashPrtElem* createHashPrtElem(PrtElem* key,PrtElem* v)
{
	HashPrtElem* r=(HashPrtElem*)malloc(sizeof(HashPrtElem));
	FKL_ASSERT(r);
	r->key=key;
	r->v=v;
	return r;
}

void fklPrintVMvalue(FklVMvalue* value
		,FILE* fp
		,void(*atomPrinter)(FklVMvalue* v,FILE* fp,FklSymbolTable* table)
		,FklSymbolTable* table)
{
	FklHashTable* recValueSet=fklCreateValueSetHashtable();
	FklHashTable* hasPrintRecSet=fklCreateValueSetHashtable();
	fklScanCirRef(value,recValueSet);
	FklPtrQueue* queue=fklCreatePtrQueue();
	FklPtrStack queueStack=FKL_STACK_INIT;
	fklInitPtrStack(&queueStack,32,16);
	fklPushPtrQueue(createPrtElem(PRT_CAR,value),queue);
	fklPushPtrStack(queue,&queueStack);
	while(!fklIsPtrStackEmpty(&queueStack))
	{
		FklPtrQueue* cQueue=fklTopPtrStack(&queueStack);
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
			else if(e->state==PRT_HASH_ITEM)
			{
				fputc('(',fp);
				FklPtrQueue* iQueue=fklCreatePtrQueue();
				HashPrtElem* elem=(void*)e->v;
				fklPushPtrQueue(elem->key,iQueue);
				fklPushPtrQueue(elem->v,iQueue);
				fklPushPtrStack(iQueue,&queueStack);
				cQueue=iQueue;
				free(elem);
				free(e);
				continue;
			}
			else
			{
				free(e);
				if(!FKL_IS_VECTOR(v)&&!FKL_IS_PAIR(v)&&!FKL_IS_BOX(v)&&!FKL_IS_HASHTABLE(v))
					atomPrinter(v,fp,table);
				else
				{
					size_t i=0;
					if(isInValueSet(v,recValueSet,&i))
						fprintf(fp,"#%lu=",i);
					if(FKL_IS_VECTOR(v))
					{
						fputs("#(",fp);
						FklPtrQueue* vQueue=fklCreatePtrQueue();
						for(size_t i=0;i<v->u.vec->size;i++)
						{
							size_t w=0;
							if((isInValueSet(v->u.vec->base[i],recValueSet,&w)&&isInValueSet(v->u.vec->base[i],hasPrintRecSet,NULL))||v->u.vec->base[i]==v)
								fklPushPtrQueue(createPrtElem(PRT_REC_CAR,(void*)w)
										,vQueue);
							else
							{
								fklPushPtrQueue(createPrtElem(PRT_CAR,v->u.vec->base[i])
										,vQueue);
								putValueInSet(hasPrintRecSet,v->u.vec->base[i]);
							}
						}
						fklPushPtrStack(vQueue,&queueStack);
						cQueue=vQueue;
						continue;
					}
					else if(FKL_IS_BOX(v))
					{
						fputs("#&",fp);
						size_t w=0;
						if((isInValueSet(v->u.box,recValueSet,&w)&&isInValueSet(v->u.box,hasPrintRecSet,NULL))||v->u.box==v)
							fklPushPtrQueueToFront(createPrtElem(PRT_REC_BOX,(void*)w),cQueue);
						else
						{
							fklPushPtrQueueToFront(createPrtElem(PRT_BOX,v->u.box),cQueue);
							putValueInSet(hasPrintRecSet,v->u.box);
						}
						continue;
					}
					else if(FKL_IS_HASHTABLE(v))
					{
						fputs(fklGetVMhashTablePrefix(v->u.hash),fp);
						FklPtrQueue* hQueue=fklCreatePtrQueue();
						for(FklHashTableNodeList* list=v->u.hash->list;list;list=list->next)
						{
							FklVMhashTableItem* item=(FklVMhashTableItem*)list->node->data;
							PrtElem* keyElem=NULL;
							PrtElem* vElem=NULL;
							size_t w=0;
							if((isInValueSet(item->key,recValueSet,&w)&&isInValueSet(item->key,hasPrintRecSet,NULL))||item->key==v)
								keyElem=createPrtElem(PRT_REC_CAR,(void*)w);
							else
							{
								keyElem=createPrtElem(PRT_CAR,item->key);
								putValueInSet(hasPrintRecSet,item->key);
							}
							if((isInValueSet(item->v,recValueSet,&w)&&isInValueSet(item->key,hasPrintRecSet,NULL))||item->v==v)
								vElem=createPrtElem(PRT_REC_CDR,(void*)w);
							else
							{
								vElem=createPrtElem(PRT_CDR,item->v);
								putValueInSet(hasPrintRecSet,item->v);
							}
							fklPushPtrQueue(createPrtElem(PRT_HASH_ITEM,(void*)createHashPrtElem(keyElem,vElem)),hQueue);
						}
						fklPushPtrStack(hQueue,&queueStack);
						cQueue=hQueue;
						continue;
					}
					else
					{
						fputc('(',fp);
						FklPtrQueue* lQueue=fklCreatePtrQueue();
						FklVMpair* p=v->u.pair;
						for(;;)
						{
							PrtElem* ce=NULL;
							size_t w=0;
							if(isInValueSet(p->car,recValueSet,&w)&&(isInValueSet(p->car,hasPrintRecSet,NULL)||p->car==v))
								ce=createPrtElem(PRT_REC_CAR,(void*)w);
							else
							{
								ce=createPrtElem(PRT_CAR,p->car);
								putValueInSet(hasPrintRecSet,p->car);
							}
							fklPushPtrQueue(ce,lQueue);
							if(isInValueSet(p->cdr,recValueSet,&w))
							{
								PrtElem* cdre=NULL;
								if(p->cdr!=v&&!isInValueSet(p->cdr,hasPrintRecSet,NULL))
								{
									cdre=createPrtElem(PRT_CDR,p->cdr);
									putValueInSet(hasPrintRecSet,p->cdr);
								}
								else
								{
									cdre=createPrtElem(PRT_REC_CDR,(void*)w);
								}
								fklPushPtrQueue(cdre,lQueue);
								break;
							}
							FklVMpair* next=FKL_IS_PAIR(p->cdr)?p->cdr->u.pair:NULL;
							if(!next)
							{
								FklVMvalue* cdr=p->cdr;
								if(cdr!=FKL_VM_NIL)
									fklPushPtrQueue(createPrtElem(PRT_CDR,cdr),lQueue);
								break;
							}
							p=next;
						}
						fklPushPtrStack(lQueue,&queueStack);
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
		fklPopPtrStack(&queueStack);
		fklDestroyPtrQueue(cQueue);
		if(!fklIsPtrStackEmpty(&queueStack))
		{
			fputc(')',fp);
			cQueue=fklTopPtrStack(&queueStack);
			if(fklLengthPtrQueue(cQueue)
					&&((PrtElem*)fklFirstPtrQueue(cQueue))->state!=PRT_CDR
					&&((PrtElem*)fklFirstPtrQueue(cQueue))->state!=PRT_REC_CDR
					&&((PrtElem*)fklFirstPtrQueue(cQueue))->state!=PRT_BOX
					&&((PrtElem*)fklFirstPtrQueue(cQueue))->state!=PRT_REC_BOX)
				fputc(' ',fp);
		}
	}
	fklUninitPtrStack(&queueStack);
	fklDestroyHashTable(recValueSet);
	fklDestroyHashTable(hasPrintRecSet);
}

#include<ctype.h>
inline static int is_special_char_and_print_to_utstring(FklStringBuffer* s,char ch)
{
	int r=0;
	if((r=ch=='\n'))
		fklStringBufferPrintf(s,"\\n");
	else if((r=ch=='\t'))
		fklStringBufferPrintf(s,"\\t");
	else if((r=ch=='\v'))
		fklStringBufferPrintf(s,"\\v");
	else if((r=ch=='\a'))
		fklStringBufferPrintf(s,"\\a");
	else if((r=ch=='\b'))
		fklStringBufferPrintf(s,"\\b");
	else if((r=ch=='\f'))
		fklStringBufferPrintf(s,"\\f");
	else if((r=ch=='\r'))
		fklStringBufferPrintf(s,"\\r");
	else if((r=ch=='\x20'))
		fklStringBufferPrintf(s," ");
	return r;
}

inline static void print_raw_char_to_utstring(FklStringBuffer* s,char c)
{
	fklStringBufferPrintf(s,"#\\");
	if(isgraph(c))
	{
		if(c=='\\')
			fklStringBufferPrintf(s,"\\");
		else
			fklStringBufferPrintf(s,"%c",c);
	}
	else if(is_special_char_and_print_to_utstring(s,c));
	else
	{
		uint8_t j=c;
		fklStringBufferPrintf(s,"x");
		fklStringBufferPrintf(s,"%X",j/16);
		fklStringBufferPrintf(s,"%X",j%16);
	}
}

inline static void print_raw_string_to_ut_string_with_se(FklStringBuffer* s,FklString* fstr,char se)
{
	char buf[7]={0};
	fklStringBufferPrintf(s,"%c",se);
	size_t size=fstr->size;
	uint8_t* str=(uint8_t*)fstr->str;
	for(size_t i=0;i<size;)
	{
		unsigned int l=fklGetByteNumOfUtf8(&str[i],size-i);
		if(l==7)
		{
			uint8_t j=str[i];
			uint8_t h=j/16;
			uint8_t l=j%16;
			fklStringBufferPrintf(s,"\\x%c%c"
					,h<10?'0'+h:'A'+(h-10)
					,l<10?'0'+l:'A'+(l-10));
			i++;
		}
		else if(l==1)
		{
			if(str[i]==se)
				fklStringBufferPrintf(s,"\\%c",se);
			else if(str[i]=='\\')
				fklStringBufferPrintf(s,"\\\\");
			else if(isgraph(str[i]))
				fklStringBufferPrintf(s,"%c",str[i]);
			else if(is_special_char_and_print_to_utstring(s,str[i]));
			else
			{
				uint8_t j=str[i];
				uint8_t h=j/16;
				uint8_t l=j%16;
				fklStringBufferPrintf(s,"\\x%c%c"
						,h<10?'0'+h:'A'+(h-10)
						,l<10?'0'+l:'A'+(l-10));
			}
			i++;
		}
		else
		{
			strncpy(buf,(char*)&str[i],l);
			buf[l]='\0';
			fklStringBufferPrintf(s,"%s",buf);
			i+=l;
		}
	}
	fklStringBufferPrintf(s,"%c",se);
}

inline static void print_raw_string_to_ut_string(FklStringBuffer* s,FklString* f)
{
	print_raw_string_to_ut_string_with_se(s,f,'"');
}

inline static void print_raw_symbol_to_ut_string(FklStringBuffer* s,FklString* f)
{
	print_raw_string_to_ut_string_with_se(s,f,'|');
}

inline static void print_bytevector_to_ut_string(FklStringBuffer* s,FklBytevector* bvec)
{
	size_t size=bvec->size;
	uint8_t* ptr=bvec->ptr;
	fklStringBufferPrintf(s,"#vu8(");
	for(size_t i=0;i<size;i++)
	{
		fklStringBufferPrintf(s,"0x%X",ptr[i]);
		if(i<size-1)
			fklStringBufferPrintf(s," ");
	}
	fklStringBufferPrintf(s,")");
}

inline static void print_big_int_to_ut_string(FklStringBuffer* s,FklBigInt* a)
{
	if(!FKL_IS_0_BIG_INT(a)&&a->neg)
		fklStringBufferPrintf(s,"-");
	if(FKL_IS_0_BIG_INT(a))
		fklStringBufferPrintf(s,"0");
	else
	{
		FklU8Stack res=FKL_STACK_INIT;
		fklBigIntToRadixDigitsLe(a,10,&res);
		for(size_t i=res.top;i>0;i--)
		{
			uint8_t c=res.base[i-1];
			fklStringBufferPrintf(s,"%c",'0'+c);
		}
		fklUninitU8Stack(&res);
	}
}

#define VMVALUE_TO_UTSTRING_ARGS FklStringBuffer* result,FklVMvalue* v,FklSymbolTable* table

static void nil_ptr_to_ut_string(VMVALUE_TO_UTSTRING_ARGS)
{
	fklStringBufferPrintf(result,"()");
}

static void fix_ptr_to_ut_string(VMVALUE_TO_UTSTRING_ARGS)
{
	fklStringBufferPrintf(result,"%ld",FKL_GET_FIX(v));
}

static void sym_ptr_to_ut_string(VMVALUE_TO_UTSTRING_ARGS)
{
	print_raw_symbol_to_ut_string(result,fklGetSymbolWithId(FKL_GET_SYM(v),table)->symbol);
}

static void chr_ptr_to_ut_string(VMVALUE_TO_UTSTRING_ARGS)
{
	print_raw_char_to_utstring(result,FKL_GET_CHR(v));
}

static void vmvalue_f64_to_ut_string(VMVALUE_TO_UTSTRING_ARGS)
{
	fklStringBufferPrintf(result,"%lf",v->u.f64);
}

static void vmvalue_big_int_to_ut_string(VMVALUE_TO_UTSTRING_ARGS)
{
	print_big_int_to_ut_string(result,v->u.bigInt);
}

static void vmvalue_string_to_ut_string(VMVALUE_TO_UTSTRING_ARGS)
{
	print_raw_string_to_ut_string(result,v->u.str);
}

static void vmvalue_bytevector_to_ut_string(VMVALUE_TO_UTSTRING_ARGS)
{
	print_bytevector_to_ut_string(result,v->u.bvec);
}

static void vmvalue_userdata_to_ut_string(VMVALUE_TO_UTSTRING_ARGS)
{
	if(fklIsAbleToStringUd(v->u.ud))
	{
		FklString* s=fklUdToString(v->u.ud);
		fklStringBufferConcatWithString(result,s);
		free(s);
	}
	else
	{
		fklStringBufferPrintf(result,"#<");
		if(v->u.ud->type)
		{
			print_raw_symbol_to_ut_string(result,fklGetSymbolWithId(v->u.ud->type,table)->symbol);
			fklStringBufferPrintf(result," ");
		}
		fklStringBufferPrintf(result,"%p>",v->u.ud);
	}
}

static void vmvalue_proc_to_ut_string(VMVALUE_TO_UTSTRING_ARGS)
{
	if(v->u.proc->sid)
	{
		fklStringBufferPrintf(result,"#<proc ");
		print_raw_symbol_to_ut_string(result,fklGetSymbolWithId(v->u.proc->sid,table)->symbol);
		fklStringBufferPrintf(result,">");
	}
	else
		fklStringBufferPrintf(result,"#<proc %p>",v->u.proc);
}

static void vmvalue_chanl_to_ut_string(VMVALUE_TO_UTSTRING_ARGS)
{
	fklStringBufferPrintf(result,"#<chanl %p>",v->u.chan);
}

static void vmvalue_fp_to_ut_string(VMVALUE_TO_UTSTRING_ARGS)
{
	if(v->u.fp->fp==stdin)
		fklStringBufferPrintf(result,"#<fp stdin>");
	else if(v->u.fp->fp==stdout)
		fklStringBufferPrintf(result,"#<fp stdout>");
	else if(v->u.fp->fp==stderr)
		fklStringBufferPrintf(result,"#<fp stderr>");
	else
		fklStringBufferPrintf(result,"#<fp %p>",v->u.fp);
}

static void vmvalue_dll_to_ut_string(VMVALUE_TO_UTSTRING_ARGS)
{
	fklStringBufferPrintf(result,"#<dll %p>",v->u.dll);
}

static void vmvalue_dlproc_to_ut_string(VMVALUE_TO_UTSTRING_ARGS)
{
	if(v->u.dlproc->sid)
	{
		fklStringBufferPrintf(result,"#<dlproc ");
		print_raw_symbol_to_ut_string(result,fklGetSymbolWithId(v->u.dlproc->sid,table)->symbol);
		fklStringBufferPrintf(result,">");
	}
	else
		fklStringBufferPrintf(result,"#<dlproc %p>",v->u.dlproc);
}

static void vmvalue_error_to_ut_string(VMVALUE_TO_UTSTRING_ARGS)
{
	fklStringBufferPrintf(result,"#<err t:");
	print_raw_symbol_to_ut_string(result,fklGetSymbolWithId(v->u.err->type,table)->symbol);
	fklStringBufferPrintf(result,"w:");
	print_raw_string_to_ut_string(result,v->u.err->who);
	fklStringBufferPrintf(result,"m:");
	print_raw_string_to_ut_string(result,v->u.err->message);
	fklStringBufferPrintf(result,">");
}

static void vmvalue_code_obj_to_ut_string(VMVALUE_TO_UTSTRING_ARGS)
{
	fklStringBufferPrintf(result,"#<code-obj %p>",v->u.code);
}

static void (*atom_ptr_ptr_to_ut_string_printer_table[FKL_VMVALUE_PTR_TYPE_NUM])(VMVALUE_TO_UTSTRING_ARGS)=
{
	vmvalue_f64_to_ut_string,
	vmvalue_big_int_to_ut_string,
	vmvalue_string_to_ut_string,
	NULL,
	NULL,
	NULL,
	vmvalue_bytevector_to_ut_string,
	vmvalue_userdata_to_ut_string,
	vmvalue_proc_to_ut_string,
	vmvalue_chanl_to_ut_string,
	vmvalue_fp_to_ut_string,
	vmvalue_dll_to_ut_string,
	vmvalue_dlproc_to_ut_string,
	vmvalue_error_to_ut_string,
	NULL,
	vmvalue_code_obj_to_ut_string,
};

static void ptr_ptr_to_ut_string(VMVALUE_TO_UTSTRING_ARGS)
{
	atom_ptr_ptr_to_ut_string_printer_table[v->type](result,v,table);
}

static void (*atom_ptr_to_ut_string_printer_table[FKL_PTR_TAG_NUM])(VMVALUE_TO_UTSTRING_ARGS)=
{
	ptr_ptr_to_ut_string,
	nil_ptr_to_ut_string,
	fix_ptr_to_ut_string,
	sym_ptr_to_ut_string,
	chr_ptr_to_ut_string,
};

inline static void print_atom_to_ut_string(FklStringBuffer* result,FklVMvalue* v,FklSymbolTable* table)
{
	atom_ptr_to_ut_string_printer_table[(FklVMptrTag)FKL_GET_TAG(v)](result,v,table);
}

FklString* fklStringify(FklVMvalue* value,FklSymbolTable* table)
{
	FklStringBuffer result;
	fklInitStringBuffer(&result);
	FklHashTable* recValueSet=fklCreateValueSetHashtable();
	FklHashTable* hasPrintRecSet=fklCreateValueSetHashtable();
	fklScanCirRef(value,recValueSet);
	FklPtrQueue* queue=fklCreatePtrQueue();
	FklPtrStack queueStack=FKL_STACK_INIT;
	fklInitPtrStack(&queueStack,32,16);
	fklPushPtrQueue(createPrtElem(PRT_CAR,value),queue);
	fklPushPtrStack(queue,&queueStack);
	while(!fklIsPtrStackEmpty(&queueStack))
	{
		FklPtrQueue* cQueue=fklTopPtrStack(&queueStack);
		while(fklLengthPtrQueue(cQueue))
		{
			PrtElem* e=fklPopPtrQueue(cQueue);
			FklVMvalue* v=e->v;
			if(e->state==PRT_CDR||e->state==PRT_REC_CDR)
				fklStringBufferPrintf(&result,",");
			if(e->state==PRT_REC_CAR||e->state==PRT_REC_CDR||e->state==PRT_REC_BOX)
				fklStringBufferPrintf(&result,"#%lu",(uintptr_t)e->v);
			else if(e->state==PRT_HASH_ITEM)
			{
				fklStringBufferPrintf(&result,"(");
				FklPtrQueue* iQueue=fklCreatePtrQueue();
				HashPrtElem* elem=(void*)e->v;
				fklPushPtrQueue(elem->key,iQueue);
				fklPushPtrQueue(elem->v,iQueue);
				fklPushPtrStack(iQueue,&queueStack);
				cQueue=iQueue;
				free(elem);
				free(e);
				continue;
			}
			else
			{
				free(e);
				if(!FKL_IS_VECTOR(v)&&!FKL_IS_PAIR(v)&&!FKL_IS_BOX(v)&&!FKL_IS_HASHTABLE(v))
					print_atom_to_ut_string(&result,v,table);
				else
				{
					size_t i=0;
					if(isInValueSet(v,recValueSet,&i))
						fklStringBufferPrintf(&result,"#%lu=",i);
					if(FKL_IS_VECTOR(v))
					{
						fklStringBufferPrintf(&result,"#(");
						FklPtrQueue* vQueue=fklCreatePtrQueue();
						for(size_t i=0;i<v->u.vec->size;i++)
						{
							size_t w=0;
							if((isInValueSet(v->u.vec->base[i],recValueSet,&w)&&isInValueSet(v->u.vec->base[i],hasPrintRecSet,NULL))||v->u.vec->base[i]==v)
								fklPushPtrQueue(createPrtElem(PRT_REC_CAR,(void*)w)
										,vQueue);
							else
							{
								fklPushPtrQueue(createPrtElem(PRT_CAR,v->u.vec->base[i])
										,vQueue);
								putValueInSet(hasPrintRecSet,v->u.vec->base[i]);
							}
						}
						fklPushPtrStack(vQueue,&queueStack);
						cQueue=vQueue;
						continue;
					}
					else if(FKL_IS_BOX(v))
					{
						fklStringBufferPrintf(&result,"#&");
						size_t w=0;
						if((isInValueSet(v->u.box,recValueSet,&w)&&isInValueSet(v->u.box,hasPrintRecSet,NULL))||v->u.box==v)
							fklPushPtrQueueToFront(createPrtElem(PRT_REC_BOX,(void*)w),cQueue);
						else
						{
							fklPushPtrQueueToFront(createPrtElem(PRT_BOX,v->u.box),cQueue);
							putValueInSet(hasPrintRecSet,v->u.box);
						}
						continue;
					}
					else if(FKL_IS_HASHTABLE(v))
					{
						fklStringBufferPrintf(&result,"%s",fklGetVMhashTablePrefix(v->u.hash));
						FklPtrQueue* hQueue=fklCreatePtrQueue();
						for(FklHashTableNodeList* list=v->u.hash->list;list;list=list->next)
						{
							FklVMhashTableItem* item=(FklVMhashTableItem*)list->node->data;
							PrtElem* keyElem=NULL;
							PrtElem* vElem=NULL;
							size_t w=0;
							if((isInValueSet(item->key,recValueSet,&w)&&isInValueSet(item->key,hasPrintRecSet,NULL))||item->key==v)
								keyElem=createPrtElem(PRT_REC_CAR,(void*)w);
							else
							{
								keyElem=createPrtElem(PRT_CAR,item->key);
								putValueInSet(hasPrintRecSet,item->key);
							}
							if((isInValueSet(item->v,recValueSet,&w)&&isInValueSet(item->key,hasPrintRecSet,NULL))||item->v==v)
								vElem=createPrtElem(PRT_REC_CDR,(void*)w);
							else
							{
								vElem=createPrtElem(PRT_CDR,item->v);
								putValueInSet(hasPrintRecSet,item->v);
							}
							fklPushPtrQueue(createPrtElem(PRT_HASH_ITEM,(void*)createHashPrtElem(keyElem,vElem)),hQueue);
						}
						fklPushPtrStack(hQueue,&queueStack);
						cQueue=hQueue;
						continue;
					}
					else
					{
						fklStringBufferPrintf(&result,"(");
						FklPtrQueue* lQueue=fklCreatePtrQueue();
						FklVMpair* p=v->u.pair;
						for(;;)
						{
							PrtElem* ce=NULL;
							size_t w=0;
							if(isInValueSet(p->car,recValueSet,&w)&&(isInValueSet(p->car,hasPrintRecSet,NULL)||p->car==v))
								ce=createPrtElem(PRT_REC_CAR,(void*)w);
							else
							{
								ce=createPrtElem(PRT_CAR,p->car);
								putValueInSet(hasPrintRecSet,p->car);
							}
							fklPushPtrQueue(ce,lQueue);
							if(isInValueSet(p->cdr,recValueSet,&w))
							{
								PrtElem* cdre=NULL;
								if(p->cdr!=v&&!isInValueSet(p->cdr,hasPrintRecSet,NULL))
								{
									cdre=createPrtElem(PRT_CDR,p->cdr);
									putValueInSet(hasPrintRecSet,p->cdr);
								}
								else
								{
									cdre=createPrtElem(PRT_REC_CDR,(void*)w);
								}
								fklPushPtrQueue(cdre,lQueue);
								break;
							}
							FklVMpair* next=FKL_IS_PAIR(p->cdr)?p->cdr->u.pair:NULL;
							if(!next)
							{
								FklVMvalue* cdr=p->cdr;
								if(cdr!=FKL_VM_NIL)
									fklPushPtrQueue(createPrtElem(PRT_CDR,cdr),lQueue);
								break;
							}
							p=next;
						}
						fklPushPtrStack(lQueue,&queueStack);
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
				fklStringBufferPrintf(&result," ");
		}
		fklPopPtrStack(&queueStack);
		fklDestroyPtrQueue(cQueue);
		if(!fklIsPtrStackEmpty(&queueStack))
		{
			fklStringBufferPrintf(&result,")");
			cQueue=fklTopPtrStack(&queueStack);
			if(fklLengthPtrQueue(cQueue)
					&&((PrtElem*)fklFirstPtrQueue(cQueue))->state!=PRT_CDR
					&&((PrtElem*)fklFirstPtrQueue(cQueue))->state!=PRT_REC_CDR
					&&((PrtElem*)fklFirstPtrQueue(cQueue))->state!=PRT_BOX
					&&((PrtElem*)fklFirstPtrQueue(cQueue))->state!=PRT_REC_BOX)
				fklStringBufferPrintf(&result," ");
		}
	}
	fklUninitPtrStack(&queueStack);
	fklDestroyHashTable(recValueSet);
	fklDestroyHashTable(hasPrintRecSet);
	FklString* retval=fklStringBufferToString(&result);
	fklUninitStringBuffer(&result);
	return retval;
}

FklVMvalue* fklSetRef(FklVMvalue* volatile* pref,FklVMvalue* v,FklVMgc* gc)
{
	FklVMvalue* ref=*pref;
	*pref=v;
	FklGCstate running=fklGetGCstate(gc);
	if(running==FKL_GC_PROPAGATE||running==FKL_GC_COLLECT)
	{
		fklGC_toGrey(ref,gc);
		fklGC_toGrey(v,gc);
	}
	return ref;
}

FklVMvalue* fklGetTopValue(FklVM* exe)
{
	return exe->base[exe->tp-1];
}

size_t fklVMlistLength(FklVMvalue* v)
{
	size_t len=0;
	for(;FKL_IS_PAIR(v);v=fklGetVMpairCdr(v))len++;
	return len;
}

