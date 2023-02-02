#include<fakeLisp/vm.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/opcode.h>
#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<math.h>
#include<float.h>
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
	if(r64>INT32_MAX||r64<INT32_MIN)
		return fklCreateVMvalueToStack(FKL_TYPE_I64,&r64,vm);
	else
		return FKL_MAKE_VM_I32(r64);
}

FklVMvalue* fklMakeVMuint(uint64_t r64,FklVM* vm)
{
	if(r64>INT64_MAX)
	{
		FklBigInt* bi=fklCreateBigIntU(r64);
		return fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,bi,vm);
	}
	else if(r64>INT32_MAX)
		return fklCreateVMvalueToStack(FKL_TYPE_I64,&r64,vm);
	else
		return FKL_MAKE_VM_I32(r64);
}

FklVMvalue* fklMakeVMintD(double r64,FklVM* vm)
{
	if(r64-INT32_MAX>DBL_EPSILON||r64-INT32_MIN<-DBL_EPSILON)
		return fklCreateVMvalueToStack(FKL_TYPE_I64,&r64,vm);
	else
		return FKL_MAKE_VM_I32(r64);
}

inline int fklIsFixint(const FklVMvalue* p)
{
	return FKL_IS_I32(p)||FKL_IS_I64(p);
}

inline int fklIsInt(const FklVMvalue* p)
{
	return FKL_IS_I32(p)||FKL_IS_I64(p)||FKL_IS_BIG_INT(p);
}

inline int fklIsVMnumber(const FklVMvalue* p)
{
	return fklIsInt(p)||FKL_IS_F64(p);
}

int fklIsList(const FklVMvalue* p)
{
	for(;p!=FKL_VM_NIL;p=p->u.pair->cdr)
		if(!FKL_IS_PAIR(p))
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
	return FKL_IS_I32(p)
		?FKL_GET_I32(p)
		:(FKL_IS_I64(p))?p->u.i64
		:fklBigIntToI64(p->u.bigInt);
}

inline uint64_t fklGetUint(const FklVMvalue* p)
{
	return FKL_IS_I32(p)
		?FKL_GET_I32(p)
		:(FKL_IS_I64(p))?p->u.i64
		:fklBigIntToU64(p->u.bigInt);
}

inline int fklVMnumberLt0(const FklVMvalue* p)
{
	return fklIsFixint(p)
		?fklGetInt(p)<0
		:FKL_IS_F64(p)
		?0.0-p->u.f64>DBL_EPSILON
		:!FKL_IS_0_BIG_INT(p->u.bigInt)&&p->u.bigInt->neg;
}

inline double fklGetDouble(const FklVMvalue* p)
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
//	pthread_rwlock_wrlock(&stack->lock);
	FklVMvalue* r=NULL;
	if(!(stack->tp>stack->bp))
		r=NULL;
	else
	{
		FklVMvalue* tmp=fklGetTopValue(stack);
		r=tmp;
	}
//	pthread_rwlock_unlock(&stack->lock);
	return r;
}

void fklDecTop(FklVMstack* stack)
{
//	pthread_rwlock_wrlock(&stack->lock);
	stack->tp--;
//	pthread_rwlock_unlock(&stack->lock);
}

FklVMstack* fklCopyStack(FklVMstack* stack)
{
	int32_t i=0;
	FklVMstack* tmp=(FklVMstack*)malloc(sizeof(FklVMstack));
	FKL_ASSERT(tmp);
//	pthread_rwlock_rdlock(&stack->lock);
	tmp->size=stack->size;
	tmp->tp=stack->tp;
	tmp->bp=stack->bp;
//	pthread_rwlock_init(&tmp->lock,NULL);
	tmp->values=(FklVMvalue**)malloc(sizeof(FklVMvalue*)*(tmp->size));
	FKL_ASSERT(tmp->values);
	for(;i<stack->tp;i++)
		tmp->values[i]=stack->values[i];
	tmp->tps=(FklUintStack){NULL,0,0,0};
	fklInitUintStackWithStack(&tmp->tps,&stack->tps);
	tmp->bps=(FklUintStack){NULL,0,0,0};
	fklInitUintStackWithStack(&tmp->bps,&stack->bps);
//	pthread_rwlock_unlock(&stack->lock);
	return tmp;
}

FklVMerrorHandler* fklCreateVMerrorHandler(FklSid_t* typeIds,uint32_t errTypeNum,uint64_t scp,uint64_t cpc)
{
	FklVMerrorHandler* t=(FklVMerrorHandler*)malloc(sizeof(FklVMerrorHandler));
	FKL_ASSERT(t);
	t->typeIds=typeIds;
	t->num=errTypeNum;
	t->proc.prevEnv=NULL;
	t->proc.scp=scp;
	t->proc.cpc=cpc;
	t->proc.sid=0;
	return t;
}

void fklDestroyVMerrorHandler(FklVMerrorHandler* h)
{
	free(h->typeIds);
	free(h);
}

static void threadErrorCallBack(void* a)
{
	void** e=(void**)a;
	int* i=(int*)a;
	FklVM* exe=e[0];
	longjmp(exe->buf,i[(sizeof(void*)*2)/sizeof(int)]);
}

int fklRaiseVMerror(FklVMvalue* ev,FklVM* exe)
{
	FklVMframe* frame=exe->frames;
	void* i[3]={exe,(void*)255,(void*)1};
	for(;frame;frame=frame->prev)
		if(frame->errorCallBack!=NULL&&frame->errorCallBack(frame,ev,exe,i))
			break;
	if(frame==NULL)
	{
		FklVMerror* err=ev->u.err;
		fprintf(stderr,"error of ");
		fklPrintString(err->who,stderr);
		fprintf(stderr,": ");
		fklPrintString(err->message,stderr);
		fprintf(stderr,"\n");
		for(FklVMframe* cur=exe->frames;cur;cur=cur->prev)
		{
			if(cur->type==FKL_FRAME_COMPOUND)
			{
				if(fklGetCompoundFrameSid(cur)!=0)
				{
					fprintf(stderr,"at proc:");
					fklPrintString(fklGetSymbolWithId(fklGetCompoundFrameSid(cur),exe->symbolTable)->symbol
							,stderr);
				}
				else
				{
					if(cur->prev)
						fprintf(stderr,"at <lambda>");
					else
						fprintf(stderr,"at <top>");
				}
				FklByteCodelnt* codeObj=fklGetCompoundFrameCodeObj(cur)->u.code;
				FklLineNumTabNode* node=fklFindLineNumTabNode(fklGetCompoundFrameCp(cur)
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
		threadErrorCallBack(i);
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

FklVMframe* fklCreateVMframeWithCompoundFrame(const FklVMframe* f,FklVMframe* prev,FklVMgc* gc)
{
	FklVMframe* tmp=(FklVMframe*)malloc(sizeof(FklVMframe));
	FKL_ASSERT(tmp);
	tmp->u.c.sid=f->u.c.sid;
	tmp->u.c.cp=f->u.c.cp;
	tmp->u.c.scp=f->u.c.scp;
	tmp->u.c.cpc=f->u.c.cpc;
	tmp->prev=prev;
	tmp->u.c.sid=f->u.c.sid;
	fklSetRef(&tmp->u.c.codeObj,f->u.c.codeObj,gc);
	fklSetRef(&tmp->u.c.localenv,f->u.c.localenv,gc);
	tmp->u.c.code=f->u.c.codeObj->u.code->bc->code;
	tmp->u.c.mark=f->u.c.mark;
	tmp->errorCallBack=f->errorCallBack;
	tmp->type=FKL_FRAME_COMPOUND;
	return tmp;
}

FklVMframe* fklCreateVMframeWithCodeObj(FklVMvalue* codeObj,FklVMframe* prev,FklVMgc* gc)
{
	FklVMframe* tmp=(FklVMframe*)malloc(sizeof(FklVMframe));
	FKL_ASSERT(tmp);
	tmp->prev=prev;
	tmp->u.c.sid=0;
	fklSetRef(&tmp->u.c.codeObj,codeObj,gc);
	tmp->u.c.code=codeObj->u.code->bc->code;
	tmp->u.c.cp=0;
	tmp->u.c.scp=0;
	tmp->u.c.cpc=codeObj->u.code->bc->size;
	tmp->u.c.mark=0;
	tmp->errorCallBack=NULL;
	tmp->type=FKL_FRAME_COMPOUND;
	return tmp;
}

FklVMframe* fklCreateVMframeWithProc(FklVMproc* code,FklVMframe* prev)
{
	FklVMframe* tmp=(FklVMframe*)malloc(sizeof(FklVMframe));
	FKL_ASSERT(tmp);
	tmp->u.c.sid=0;
	tmp->u.c.cp=0;
	tmp->u.c.scp=0;
	tmp->u.c.cpc=0;
	tmp->prev=prev;
	tmp->u.c.codeObj=NULL;
	tmp->u.c.code=NULL;
	if(code)
	{
		tmp->u.c.codeObj=code->codeObj;
		tmp->u.c.code=code->codeObj->u.code->bc->code;
		tmp->u.c.cp=code->scp;
		tmp->u.c.scp=code->scp;
		tmp->u.c.cpc=code->cpc;
		tmp->u.c.sid=code->sid;
	}
	tmp->u.c.mark=0;
	tmp->errorCallBack=NULL;
	tmp->type=FKL_FRAME_COMPOUND;
	return tmp;
}

FklVMframe* fklCreateOtherObjVMframe(const FklVMframeContextMethodTable* t,FklVMframe* prev)
{
	FklVMframe* r=(FklVMframe*)malloc(sizeof(FklVMframe));
	FKL_ASSERT(r);
	r->prev=prev;
	r->errorCallBack=NULL;
	r->type=FKL_FRAME_OTHEROBJ;
	r->u.o.t=t;
	r->u.o.data[0]=NULL;
	r->u.o.data[1]=NULL;
	r->u.o.data[2]=NULL;
	r->u.o.data[3]=NULL;
	r->u.o.data[4]=NULL;
	r->u.o.data[5]=NULL;
	return r;
}

void fklDestroyVMframe(FklVMframe* frame)
{
	if(frame->type==FKL_FRAME_OTHEROBJ)
		fklDoFinalizeObjFrame(frame);
	else
		free(frame);
}

FklString* fklGenInvalidSymbolErrorMessage(char* str,int _free,FklBuiltInErrorType type)
{
	FklString* t=fklCreateEmptyString();
	switch(type)
	{
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
		NULL,
	};
	const char* s=builtinErrorMessages[type];
	FKL_ASSERT(s);
	return fklCreateStringFromCstr(s);
}

int32_t fklGetSymbolIdInByteCode(const uint8_t* code)
{
	char op=*code;
	switch(op)
	{
		case FKL_OP_PUSH_VAR:
			return *(int32_t*)(code+sizeof(char));
			break;
		case FKL_OP_POP_VAR:
		case FKL_OP_POP_ARG:
		case FKL_OP_POP_REST_ARG:
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

static VMvalueHashItem* createVMvalueHashItem(FklVMvalue* key,size_t w)
{
	VMvalueHashItem* r=(VMvalueHashItem*)malloc(sizeof(VMvalueHashItem));
	FKL_ASSERT(r);
	r->v=key;
	r->w=w;
	return r;
}

static size_t _VMvalue_hashFunc(void* key)
{
	FklVMvalue* v=*(FklVMvalue**)key;
	return (uintptr_t)v>>3;
}

static void _VMvalue_destroyItem(void* item)
{
	free(item);
}

static int _VMvalue_keyEqual(void* pkey0,void* pkey1)
{
	FklVMvalue* k0=*(FklVMvalue**)pkey0;
	FklVMvalue* k1=*(FklVMvalue**)pkey1;
	return k0==k1;
}

static void* _VMvalue_getKey(void* item)
{
	return &((VMvalueHashItem*)item)->v;
}

static FklHashTableMethodTable VMvalueHashMethodTable=
{
	.__hashFunc=_VMvalue_hashFunc,
	.__destroyItem=_VMvalue_destroyItem,
	.__keyEqual=_VMvalue_keyEqual,
	.__getKey=_VMvalue_getKey,
};

FklHashTable* fklCreateValueSetHashtable(void)
{
	return fklCreateHashTable(32,8,2,0.75,1,&VMvalueHashMethodTable);
}

static void putValueInSet(FklHashTable* s,FklVMvalue* v)
{
	size_t w=s->num;
	fklPutNoRpHashItem(createVMvalueHashItem(v,w),s);
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
	FklPtrStack beAccessed={NULL,0,0,0};
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
				FklVMhashTable* ht=v->u.hash;
				FklPtrStack stack={NULL,0,0,0};
				fklInitPtrStack(&stack,32,16);
				for(FklHashTableNodeList* list=ht->ht->list;list;list=list->next)
					fklPushPtrStack(list->node->item,&stack);
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

static void princVMatom(FklVMvalue* v,FILE* fp,FklSymbolTable* table)
{
	FklVMptrTag tag=FKL_GET_TAG(v);
	switch(tag)
	{
		case FKL_TAG_NIL:
			fprintf(fp,"()");
			break;
		case FKL_TAG_I32:
			fprintf(fp,"%d",FKL_GET_I32(v));
			break;
		case FKL_TAG_CHR:
			putc(FKL_GET_CHR(v),fp);
			break;
		case FKL_TAG_SYM:
			fklPrintString(fklGetSymbolWithId(FKL_GET_SYM(v),table)->symbol,fp);
			break;
		case FKL_TAG_PTR:
			{
				switch(v->type)
				{
					case FKL_TYPE_F64:
						fprintf(fp,"%lf",v->u.f64);
						break;
					case FKL_TYPE_I64:
						fprintf(fp,"%ld",v->u.i64);
						break;
					case FKL_TYPE_STR:
						fwrite(v->u.str->str,v->u.str->size,1,fp);
						break;
					case FKL_TYPE_BYTEVECTOR:
						fklPrintRawBytevector(v->u.bvec,fp);
						break;
					case FKL_TYPE_PROC:
						if(v->u.proc->sid)
						{
							fprintf(fp,"#<proc ");
							fklPrintString(fklGetSymbolWithId(v->u.proc->sid,table)->symbol,fp);
							fputc('>',fp);
						}
						else
							fprintf(fp,"#<proc>");
						break;
					case FKL_TYPE_CONT:
						fputs("#<continuation>",fp);
						break;
					case FKL_TYPE_CHAN:
						fputs("#<chanl>",fp);
						break;
					case FKL_TYPE_FP:
						if(v->u.fp->fp==stdin)
							fputs("#<fp stdin>",fp);
						else if(v->u.fp->fp==stdout)
							fputs("#<fp stdout>",fp);
						else if(v->u.fp->fp==stderr)
							fputs("#<fp stderr>",fp);
						else
							fputs("#<fp>",fp);
						break;
					case FKL_TYPE_DLL:
						fprintf(fp,"#<dll>");
						break;
					case FKL_TYPE_ENV:
						fprintf(fp,"#<env>");
						break;
					case FKL_TYPE_DLPROC:
						if(v->u.dlproc->sid)
						{
							fprintf(fp,"#<dlproc ");
							fklPrintString(fklGetSymbolWithId(v->u.dlproc->sid,table)->symbol,fp);
							fputc('>',fp);
						}
						else
							fprintf(fp,"#<dlproc>");
						break;
					case FKL_TYPE_ERR:
						fklPrintString(v->u.err->message,fp);
						break;
					case FKL_TYPE_BIG_INT:
						fklPrintBigInt(v->u.bigInt,fp);
						break;
					case FKL_TYPE_CODE_OBJ:
						fprintf(fp,"#<code-obj>");
						break;
					case FKL_TYPE_USERDATA:
						if(v->u.ud->t->__princ)
							v->u.ud->t->__princ(v->u.ud->data,fp,table,v->u.ud->pd);
						else
						{
							fprintf(fp,"#<");
							fklPrintString(fklGetSymbolWithId(v->u.ud->type,table)->symbol,fp);
							fprintf(fp," %p>",v->u.ud);
						}
						break;
					default:
						fputs("#<unknown>",fp);
						break;
				}
			}
			break;
		default:
			FKL_ASSERT(0);
			break;
	}
}

static void prin1VMatom(FklVMvalue* v,FILE* fp,FklSymbolTable* table)
{
	FklVMptrTag tag=FKL_GET_TAG(v);
	switch(tag)
	{
		case FKL_TAG_NIL:
			fputs("()",fp);
			break;
		case FKL_TAG_I32:
			fprintf(fp,"%d",FKL_GET_I32(v));
			break;
		case FKL_TAG_CHR:
			fklPrintRawChar(FKL_GET_CHR(v),fp);
			break;
		case FKL_TAG_SYM:
			fklPrintRawSymbol(fklGetSymbolWithId(FKL_GET_SYM(v),table)->symbol,fp);
			break;
		case FKL_TAG_PTR:
			{
				switch(v->type)
				{
					case FKL_TYPE_F64:
						fprintf(fp,"%lf",v->u.f64);
						break;
					case FKL_TYPE_I64:
						fprintf(fp,"%ld",v->u.i64);
						break;
					case FKL_TYPE_STR:
						fklPrintRawString(v->u.str,fp);
						break;
					case FKL_TYPE_BYTEVECTOR:
						fklPrintRawBytevector(v->u.bvec,fp);
						break;
					case FKL_TYPE_PROC:
						if(v->u.proc->sid)
						{
							fprintf(fp,"#<proc ");
							fklPrintRawSymbol(fklGetSymbolWithId(v->u.proc->sid,table)->symbol,fp);
							fputc('>',fp);
						}
						else
							fputs("#<proc>",fp);
						break;
					case FKL_TYPE_CONT:
						fputs("#<continuation>",fp);
						break;
					case FKL_TYPE_CHAN:
						fputs("#<chanl>",fp);
						break;
					case FKL_TYPE_FP:
						if(v->u.fp->fp==stdin)
							fputs("#<fp stdin>",fp);
						else if(v->u.fp->fp==stdout)
							fputs("#<fp stdout>",fp);
						else if(v->u.fp->fp==stderr)
							fputs("#<fp stderr>",fp);
						else
							fputs("#<fp>",fp);
						break;
					case FKL_TYPE_DLL:
						fputs("#<dll>",fp);
						break;
					case FKL_TYPE_DLPROC:
						if(v->u.dlproc->sid){
							fprintf(fp,"#<dlproc ");
							fklPrintRawSymbol(fklGetSymbolWithId(v->u.dlproc->sid,table)->symbol,fp);
							fputc('>',fp);
						}
						else
							fputs("#<dlproc>",fp);
						break;
					case FKL_TYPE_ENV:
						fprintf(fp,"#<env>");
						break;
					case FKL_TYPE_ERR:
						fprintf(fp,"#<err w:");
						fklPrintRawString(v->u.err->who,fp);
						fprintf(fp," t:");
						fklPrintRawSymbol(fklGetSymbolWithId(v->u.err->type,table)->symbol,fp);
						fprintf(fp," m:");
						fklPrintRawString(v->u.err->message,fp);
						fprintf(fp,">");
						break;
					case FKL_TYPE_BIG_INT:
						fklPrintBigInt(v->u.bigInt,fp);
						break;
					case FKL_TYPE_CODE_OBJ:
						fprintf(fp,"#<code-obj>");
						break;
					case FKL_TYPE_USERDATA:
						if(v->u.ud->t->__prin1)
							v->u.ud->t->__prin1(v->u.ud->data,fp,table,v->u.ud->pd);
						else
						{
							fprintf(fp,"#<");
							fklPrintRawSymbol(fklGetSymbolWithId(v->u.ud->type,table)->symbol,fp);
							fprintf(fp," %p>",v->u.ud);
						}
						break;
					default:
						fputs("#<unknown>",fp);
						break;
				}
			}
			break;
		default:
			FKL_ASSERT(0);
			break;
	}
}

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
	FklPtrStack queueStack={NULL,0,0,0};
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
						const static char* tmp[]=
						{
							"#hash(",
							"#hasheqv(",
							"#hashequal(",
						};
						fputs(tmp[v->u.hash->type],fp);
						FklPtrQueue* hQueue=fklCreatePtrQueue();
						for(FklHashTableNodeList* list=v->u.hash->ht->list;list;list=list->next)
						{
							FklVMhashTableItem* item=list->node->item;
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

static void atomStringify(FklString** pr,FklVMvalue* v,FklSymbolTable* table)
{
	FklVMptrTag tag=FKL_GET_TAG(v);
	switch(tag)
	{
		case FKL_TAG_NIL:
			fklStringCstrCat(pr,"()");
			break;
		case FKL_TAG_I32:
			{
				FklString* s=fklIntToString(FKL_GET_I32(v));
				fklStringCat(pr,s);
				free(s);
			}
			break;
		case FKL_TAG_CHR:
			{
				char c_str[FKL_MAX_STRING_SIZE]={0};
				fklWriteCharAsCstr(FKL_GET_CHR(v),c_str,FKL_MAX_STRING_SIZE);
				fklStringCstrCat(pr,c_str);
			}
			break;
		case FKL_TAG_SYM:
			{
				FklString* s=fklStringToRawSymbol(fklGetSymbolWithId(FKL_GET_SYM(v),table)->symbol);
				fklStringCat(pr,s);
				free(s);
			}
			break;
		case FKL_TAG_PTR:
			{
				switch(v->type)
				{
					case FKL_TYPE_F64:
						{
							FklString* s=fklDoubleToString(v->u.f64);
							fklStringCat(pr,s);
							free(s);
						}
						break;
					case FKL_TYPE_I64:
						{
							FklString* s=fklIntToString(v->u.i64);
							fklStringCat(pr,s);
							free(s);
						}
						break;
					case FKL_TYPE_STR:
						{
							FklString* s=fklStringToRawString(v->u.str);
							fklStringCat(pr,s);
							free(s);
						}
						break;
					case FKL_TYPE_BYTEVECTOR:
						{
							FklString* s=fklBytevectorToString(v->u.bvec);
							fklStringCat(pr,s);
							free(s);
						}
						break;
					case FKL_TYPE_PROC:
						if(v->u.proc->sid)
						{
							fklStringCstrCat(pr,"#<proc ");
							FklString* s=fklStringToRawSymbol(fklGetSymbolWithId(v->u.proc->sid,table)->symbol);
							fklStringCat(pr,s);
							free(s);
							fklStringCstrCat(pr,">");
						}
						else
							fklStringCstrCat(pr,"#<proc>");
						break;
					case FKL_TYPE_CONT:
						fklStringCstrCat(pr,"#<continuation>");
						break;
					case FKL_TYPE_CHAN:
						fklStringCstrCat(pr,"#<chanl>");
						break;
					case FKL_TYPE_FP:
						if(v->u.fp->fp==stdin)
							fklStringCstrCat(pr,"#<fp stdin>");
						else if(v->u.fp->fp==stdout)
							fklStringCstrCat(pr,"#<fp stdout>");
						else if(v->u.fp->fp==stderr)
							fklStringCstrCat(pr,"#<fp stderr>");
						else
							fklStringCstrCat(pr,"#<fp>");
						break;
					case FKL_TYPE_DLL:
						fklStringCstrCat(pr,"#<dll>");
						break;
					case FKL_TYPE_DLPROC:
						if(v->u.dlproc->sid)
						{
							fklStringCstrCat(pr,"#<dlproc ");
							FklString* s=fklStringToRawSymbol(fklGetSymbolWithId(v->u.proc->sid,table)->symbol);
							fklStringCat(pr,s);
							free(s);
							fklStringCstrCat(pr,">");
						}
						else
							fklStringCstrCat(pr,"#<dlproc>");
						break;
					case FKL_TYPE_ENV:
						fklStringCstrCat(pr,"#<env>");
						break;
					case FKL_TYPE_ERR:
						{
							fklStringCstrCat(pr,"#<err w:");
							FklString* s=fklStringToRawString(v->u.err->who);
							fklStringCat(pr,s);
							free(s);
							fklStringCstrCat(pr,"t:");
							s=fklStringToRawSymbol(fklGetSymbolWithId(v->u.err->type,table)->symbol);
							fklStringCat(pr,s);
							free(s);
							fklStringCstrCat(pr,"m:");
							s=fklStringToRawString(v->u.err->message);
							fklStringCat(pr,s);
							free(s);
							fklStringCstrCat(pr,">");
						}
						break;
					case FKL_TYPE_BIG_INT:
						{
							FklString* s=fklBigIntToString(v->u.bigInt,10);
							fklStringCat(pr,s);
							free(s);
						}
						break;
					case FKL_TYPE_CODE_OBJ:
						fklStringCstrCat(pr,"#<code-obj>");
						break;
					case FKL_TYPE_USERDATA:
						if(v->u.ud->t->__to_string)
						{
							FklString* s=v->u.ud->t->__to_string(v->u.ud->data);
							fklStringCat(pr,s);
							free(s);
						}
						else
						{
							fklStringCstrCat(pr,"#<");
							FklString* s=fklStringToRawSymbol(fklGetSymbolWithId(v->u.ud->type,table)->symbol);
							fklStringCat(pr,s);
							free(s);
							char c_str[FKL_MAX_STRING_SIZE]={0};
							snprintf(c_str,FKL_MAX_STRING_SIZE,"%p>",v->u.ud);
							fklStringCstrCat(pr,c_str);
						}
						break;
					default:
						fklStringCstrCat(pr,"#<unknown>");
						break;
				}
			}
			break;
		default:
			FKL_ASSERT(0);
			break;
	}
}

FklString* fklStringify(FklVMvalue* value,FklSymbolTable* table)
{
	FklString* retval=fklCreateEmptyString();
	FklHashTable* recValueSet=fklCreateValueSetHashtable();
	FklHashTable* hasPrintRecSet=fklCreateValueSetHashtable();
	fklScanCirRef(value,recValueSet);
	FklPtrQueue* queue=fklCreatePtrQueue();
	FklPtrStack queueStack={NULL,0,0,0};
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
				fklStringCstrCat(&retval,",");
			if(e->state==PRT_REC_CAR||e->state==PRT_REC_CDR||e->state==PRT_REC_BOX)
			{
				char c_str[FKL_MAX_STRING_SIZE]={0};
				snprintf(c_str,FKL_MAX_STRING_SIZE,"#%lu",(uintptr_t)e->v);
				fklStringCstrCat(&retval,c_str);
			}
			else if(e->state==PRT_HASH_ITEM)
			{
				fklStringCstrCat(&retval,"(");
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
					atomStringify(&retval,v,table);
				else
				{
					size_t i=0;
					if(isInValueSet(v,recValueSet,&i))
					{
						char c_str[FKL_MAX_STRING_SIZE]={0};
						snprintf(c_str,FKL_MAX_STRING_SIZE,"#%lu=",i);
						fklStringCstrCat(&retval,c_str);
					}
					if(FKL_IS_VECTOR(v))
					{
						fklStringCstrCat(&retval,"#(");
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
						fklStringCstrCat(&retval,"#&");
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
						const static char* tmp[]=
						{
							"#hash(",
							"#hasheqv(",
							"#hashequal(",
						};
						fklStringCstrCat(&retval,tmp[v->u.hash->type]);
						FklPtrQueue* hQueue=fklCreatePtrQueue();
						for(FklHashTableNodeList* list=v->u.hash->ht->list;list;list=list->next)
						{
							FklVMhashTableItem* item=list->node->item;
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
						fklStringCstrCat(&retval,"(");
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
				fklStringCstrCat(&retval," ");
		}
		fklPopPtrStack(&queueStack);
		fklDestroyPtrQueue(cQueue);
		if(!fklIsPtrStackEmpty(&queueStack))
		{
			fklStringCstrCat(&retval,")");
			cQueue=fklTopPtrStack(&queueStack);
			if(fklLengthPtrQueue(cQueue)
					&&((PrtElem*)fklFirstPtrQueue(cQueue))->state!=PRT_CDR
					&&((PrtElem*)fklFirstPtrQueue(cQueue))->state!=PRT_REC_CDR
					&&((PrtElem*)fklFirstPtrQueue(cQueue))->state!=PRT_BOX
					&&((PrtElem*)fklFirstPtrQueue(cQueue))->state!=PRT_REC_BOX)
				fklStringCstrCat(&retval," ");
		}
	}
	fklUninitPtrStack(&queueStack);
	fklDestroyHashTable(recValueSet);
	fklDestroyHashTable(hasPrintRecSet);
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

FklVMvalue* fklGetTopValue(FklVMstack* stack)
{
	return stack->values[stack->tp-1];
}

FklVMvalue* fklGetValue(FklVMstack* stack,int32_t place)
{
	return stack->values[place];
}

//static void addToList(FklAstCptr* fir,FklAstCptr* sec)
//{
//	while(fir->type==FKL_TYPE_PAIR)fir=&fir->u.pair->cdr;
//	fir->type=FKL_TYPE_PAIR;
//	fir->u.pair=fklCreatePair(sec->curline,fir->outer);
//	fir->u.pair->car.curline=sec->curline;
//	fir->u.pair->car.outer=fir->outer;
//	fir->u.pair->car.type=sec->type;
//	fir->u.pair->car.u.pair=sec->u.pair;
//	fir->u.pair->car.u.pair->prev=fir->u.pair;
//}

//void fklInitVMRunningResource(FklVM* vm,FklVMvalue* vEnv,FklVMgc* gc,FklByteCodelnt* code,uint32_t start,uint32_t size)
//{
//	FklVMproc proc={
//		.scp=start,
//		.cpc=size,
//		.sid=0,
//		.prevEnv=NULL,
//	};
//	FklVMframe* mainframe=fklCreateVMframe(&proc,NULL);
//	mainframe->localenv=vEnv;
//	vm->code=code->bc->code;
//	vm->size=code->bc->size;
//	vm->frames=mainframe;
//	vm->lnt=fklCreateLineNumTable();
//	vm->lnt->num=code->ls;
//	vm->lnt->list=code->l;
//	if(vm->gc!=gc)
//	{
//		fklDestroyVMgc(vm->gc);
//		vm->gc=gc;
//	}
//}

//void fklUninitVMRunningResource(FklVM* vm)
//{
//	fklWaitGC(vm->gc);
//	free(vm->lnt);
//	fklDestroyAllVMs(vm);
//}

size_t fklVMlistLength(FklVMvalue* v)
{
	size_t len=0;
	for(;FKL_IS_PAIR(v);v=fklGetVMpairCdr(v))len++;
	return len;
}

