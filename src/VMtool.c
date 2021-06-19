#include"VMtool.h"
#include"common.h"
#include<string.h>
#include<stdio.h>
#include<math.h>
#include<pthread.h>
#ifndef _WIN32
#include<dlfcn.h>
#else
#include<tchar.h>
#endif


pthread_mutex_t VMenvGlobalRefcountLock=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t VMprocGlobalRefcountLock=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t VMstrGlobalRefcountLock=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t VMpairGlobalRefcountLock=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t VMcontinuationGlobalRefcountLock=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ByteStringGlobalRefcountLock=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t VMfpGlobalRefcountLock=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t VMChanlGlobalRefcountLock=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t VMDllGlobalRefcountLock=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t VMDlprocGlobalRefcountLock=PTHREAD_MUTEX_INITIALIZER;

#define INCREASE_REFCOUNT(TYPE,PV) {\
	if((PV))\
	{\
		pthread_mutex_lock(&(TYPE##GlobalRefcountLock));\
		((TYPE*)(PV))->refcount+=1;\
		pthread_mutex_unlock(&(TYPE##GlobalRefcountLock));\
	}\
}

#define DECREASE_REFCOUNT(TYPE,PV) {\
	if((PV))\
	{\
		pthread_mutex_lock(&(TYPE##GlobalRefcountLock));\
		((TYPE*)(PV))->refcount-=1;\
		pthread_mutex_unlock(&(TYPE##GlobalRefcountLock));\
	}\
}

VMproc* newVMproc(uint32_t scp,uint32_t cpc)
{
	VMproc* tmp=(VMproc*)malloc(sizeof(VMproc));
	if(tmp==NULL)errors("newVMproc",__FILE__,__LINE__);
	tmp->refcount=0;
	tmp->prevEnv=NULL;
	tmp->scp=scp;
	tmp->cpc=cpc;
	return tmp;
}

VMvalue* copyVMvalue(VMvalue* obj,VMheap* heap)
{
	ComStack* s1=newComStack(32);
	ComStack* s2=newComStack(32);
	VMvalue* tmp=newNilValue(heap);
	pushComStack(obj,s1);
	pushComStack(tmp,s2);
	while(!isComStackEmpty(s1))
	{
		VMvalue* root=popComStack(s1);
		VMvalue* root1=popComStack(s2);
		root1->access=1;
		root1->type=root->type;
		switch(root->type)
		{
			case IN32:
				root1->u.in32=copyMemory(root->u.in32,sizeof(int32_t));
				break;
			case DBL:
				root1->u.dbl=copyMemory(root->u.dbl,sizeof(double));
				break;
			case CHR:
				root1->u.chr=copyMemory(root->u.chr,sizeof(char));
				break;
			case BYTS:
				root1->u.byts=newByteString(root->u.byts->size,root->u.byts->str);
				break;
			case STR:
			case SYM:
				root1->u.str=newVMstr(root->u.str->str);
				break;
			case CONT:
			case PRC:
			case FP:
			case DLL:
			case DLPROC:
				copyRef(root1,root);
				break;
			case CHAN:
				{
					VMChanl* objCh=root->u.chan;
					VMChanl* tmpCh=newVMChanl(objCh->max);
					QueueNode* cur=objCh->messages->head;
					for(;cur;cur=cur->next)
					{
						void* tmp=newNilValue(heap);
						pushComQueue(tmp,tmpCh->messages);
						pushComStack(cur->data,s1);
						pushComStack(tmp,s2);
					}
					root1->u.chan=tmpCh;;
				}
				break;
			case PAIR:
				root1->u.pair=newVMpair(heap);
				pushComStack(root1->u.pair->car,s2);
				pushComStack(root1->u.pair->cdr,s2);
				pushComStack(root->u.pair->car,s1);
				pushComStack(root->u.pair->cdr,s1);
				break;
		}
	}
	freeComStack(s1);
	freeComStack(s2);
	return tmp;
}

VMvalue* newVMvalue(ValueType type,void* pValue,VMheap* heap,int access)
{
	VMvalue* tmp=(VMvalue*)malloc(sizeof(VMvalue));
	if(tmp==NULL)errors("newVMvalue",__FILE__,__LINE__);
	tmp->type=type;
	tmp->mark=0;
	tmp->access=access;
	tmp->next=heap->head;
	tmp->prev=NULL;
	pthread_mutex_lock(&heap->lock);
	if(heap->head!=NULL)heap->head->prev=tmp;
	heap->head=tmp;
	heap->num+=1;
	pthread_mutex_unlock(&heap->lock);
	switch((int)type)
	{
		case NIL:
			tmp->u.all=NULL;
			break;
		case CHR:
			tmp->u.chr=(access)?copyMemory(pValue,sizeof(char)):pValue;
			break;
		case IN32:
			tmp->u.in32=(access)?copyMemory(pValue,sizeof(int32_t)):pValue;
			break;
		case DBL:
			tmp->u.dbl=(access)?copyMemory(pValue,sizeof(double)):pValue;
			break;
		case SYM:
		case STR:
			tmp->u.str=pValue;break;
		case PAIR:
			tmp->u.pair=pValue;break;
		case PRC:
			tmp->u.prc=pValue;break;
		case BYTS:
			tmp->u.byts=pValue;break;
		case CONT:
			tmp->u.cont=pValue;break;
		case CHAN:
			tmp->u.chan=pValue;break;
		case FP:
			tmp->u.fp=pValue;break;
		case DLL:
			tmp->u.dll=pValue;break;
		case DLPROC:
			tmp->u.dlproc=pValue;break;
	}
	return tmp;
}

VMvalue* newTrueValue(VMheap* heap)
{
	int32_t i=1;
	VMvalue* tmp=newVMvalue(IN32,&i,heap,1);
	return tmp;
}

VMvalue* newNilValue(VMheap* heap)
{
	VMvalue* tmp=newVMvalue(NIL,NULL,heap,0);
	return tmp;
}

VMvalue* getTopValue(VMstack* stack)
{
	return stack->values[stack->tp-1];
}

VMvalue* getValue(VMstack* stack,int32_t place)
{
	return stack->values[place];
}

VMvalue* getVMpairCar(VMvalue* obj)
{
	if(obj->type!=PAIR)return NULL;
	else return obj->u.pair->car;
}

VMvalue* getVMpairCdr(VMvalue* obj)
{
	if(obj->type!=PAIR)return NULL;
	else return obj->u.pair->cdr;
}

int VMvaluecmp(VMvalue* fir,VMvalue* sec)
{
	ComStack* s1=newComStack(32);
	ComStack* s2=newComStack(32);
	pushComStack(fir,s1);
	pushComStack(sec,s2);
	int r=1;
	while(!isComStackEmpty(s1))
	{
		VMvalue* root1=popComStack(s1);
		VMvalue* root2=popComStack(s2);
		if((root1==root2)||(root1->type!=root2->type)||(root1->u.all==root2->u.all))
			r=1;
		else
		{
			switch(root1->type)
			{
				case IN32:
					r=(*root1->u.in32==*root2->u.in32);
					break;
				case CHR:
					r=(*root1->u.chr==*root2->u.chr);
					break;
				case DBL:
					r=(fabs(*root1->u.dbl-*root2->u.dbl)==0);
					break;
				case STR:
				case SYM:
					r=(!strcmp(root1->u.str->str,root2->u.str->str));
					break;
				case PRC:
					r=(root1->u.prc==root2->u.prc);
					break;
				case PAIR:
					r=1;
					pushComStack(root1->u.pair->car,s1);
					pushComStack(root1->u.pair->cdr,s1);
					pushComStack(root2->u.pair->car,s2);
					pushComStack(root2->u.pair->cdr,s2);
					break;
				case BYTS:
					r=bytsStrEq(root1->u.byts,root2->u.byts);
					break;
			}
			if(!r)
				break;
		}
	}
	freeComStack(s1);
	freeComStack(s2);
	return r;
}

int subVMvaluecmp(VMvalue* fir,VMvalue* sec)
{
	if(fir==sec)return 1;
	else if(fir->type!=sec->type)return 0;
	else if(fir->type>=IN32&&fir->type<=SYM)
	{
		switch(fir->type)
		{
			case IN32:
				return *fir->u.in32==*sec->u.in32;
			case CHR:
				return *fir->u.chr==*sec->u.chr;
			case DBL:
				return fabs(*fir->u.dbl-*sec->u.dbl)==0;
			case SYM:
				return !strcmp(fir->u.str->str,sec->u.str->str);
		}
	}
	else if(fir->u.all==sec->u.all)return 1;
	return 0;
}

int numcmp(VMvalue* fir,VMvalue* sec)
{
	double first=(fir->type==DBL)?*fir->u.dbl:*fir->u.in32;
	double second=(sec->type==DBL)?*sec->u.dbl:*sec->u.in32;
	if(fabs(first-second)==0)return 1;
	else return 0;
}

VMenv* newVMenv(VMenv* prev)
{
	VMenv* tmp=(VMenv*)malloc(sizeof(VMenv));
	if(tmp==NULL)errors("newVMenv",__FILE__,__LINE__);
	tmp->num=0;
	tmp->list=NULL;
	tmp->prev=prev;
	increaseVMenvRefcount(prev);
	tmp->refcount=0;
	return tmp;
}

void increaseVMenvRefcount(VMenv* env)
{
	INCREASE_REFCOUNT(VMenv,env);
}

void decreaseVMenvRefcount(VMenv* env)
{
	DECREASE_REFCOUNT(VMenv,env);
}

void increaseVMprocRefcount(VMproc* code)
{
	INCREASE_REFCOUNT(VMproc,code);
}

void decreaseVMprocRefcount(VMproc* code)
{
	DECREASE_REFCOUNT(VMproc,code);
}

void increaseVMstrRefcount(VMstr* str)
{
	INCREASE_REFCOUNT(VMstr,str);
}

void decreaseVMstrRefcount(VMstr* str)
{
	DECREASE_REFCOUNT(VMstr,str);
}

void increaseVMcontRefcount(VMcontinuation* cont)
{
	INCREASE_REFCOUNT(VMcontinuation,cont);
}

void decreaseVMcontRefcount(VMcontinuation* cont)
{
	DECREASE_REFCOUNT(VMcontinuation,cont);
}

void increaseVMpairRefcount(VMpair* pair)
{
	INCREASE_REFCOUNT(VMpair,pair);
}

void decreaseVMpairRefcount(VMpair* pair)
{
	DECREASE_REFCOUNT(VMpair,pair);
}

void increaseByteStringRefcount(ByteString* byts)
{
	INCREASE_REFCOUNT(ByteString,byts);
}

void decreaseByteStringRefcount(ByteString* byts)
{
	DECREASE_REFCOUNT(ByteString,byts);
}

void increaseVMfpRefcount(VMfp* fp)
{
	INCREASE_REFCOUNT(VMfp,fp);
}

void decreaseVMfpRefcount(VMfp* fp)
{
	DECREASE_REFCOUNT(VMfp,fp);
}

void increaseVMChanlRefcount(VMChanl* chanl)
{
	INCREASE_REFCOUNT(VMChanl,chanl);
}

void decreaseVMChanlRefcount(VMChanl* chanl)
{
	DECREASE_REFCOUNT(VMChanl,chanl);
}

void increaseVMDllRefcount(VMDll* dll)
{
	INCREASE_REFCOUNT(VMDll,dll);
}

void decreaseVMDllRefcount(VMDll* dll)
{
	DECREASE_REFCOUNT(VMDll,dll);
}

void increaseVMDlprocRefcount(VMDlproc* dlproc)
{
	INCREASE_REFCOUNT(VMDlproc,dlproc);
}

void decreaseVMDlprocRefcount(VMDlproc* dlproc)
{
	DECREASE_REFCOUNT(VMDlproc,dlproc);
}

VMpair* newVMpair(VMheap* heap)
{
	VMpair* tmp=(VMpair*)malloc(sizeof(VMpair));
	if(tmp==NULL)errors("newVMpair",__FILE__,__LINE__);
	tmp->refcount=0;
	tmp->car=newNilValue(heap);
	tmp->cdr=newNilValue(heap);
	return tmp;
}

VMstr* newVMstr(const char* str)
{
	VMstr* tmp=(VMstr*)malloc(sizeof(VMstr));
	if(tmp==NULL)errors("newVMstr",__FILE__,__LINE__);
	tmp->refcount=0;
	tmp->str=NULL;
	if(str!=NULL)
	{
		tmp->str=(char*)malloc(sizeof(char)*(strlen(str)+1));
		if(tmp->str==NULL)errors("newVMstr",__FILE__,__LINE__);
		strcpy(tmp->str,str);
	}
	return tmp;
}

VMvalue* castCptrVMvalue(AST_cptr* objCptr,VMheap* heap)
{
	ComStack* s1=newComStack(32);
	ComStack* s2=newComStack(32);
	VMvalue* tmp=newNilValue(heap);
	pushComStack(objCptr,s1);
	pushComStack(tmp,s2);
	while(!isComStackEmpty(s1))
	{
		AST_cptr* root=popComStack(s1);
		VMvalue* root1=popComStack(s2);
		if(root->type==ATM)
		{
			AST_atom* tmpAtm=root->u.atom;
			root1->type=tmpAtm->type;
			root1->access=1;
			switch((int)tmpAtm->type)
			{
				case IN32:
					root1->u.in32=copyMemory(&tmpAtm->value,sizeof(int32_t));
					break;
				case DBL:
					root1->u.dbl=copyMemory(&tmpAtm->value.dbl,sizeof(double));
					break;
				case CHR:
					root1->u.chr=copyMemory(&tmpAtm->value.chr,sizeof(char));
					break;
				case BYTS:
					root1->u.byts=newByteString(tmpAtm->value.byts.size,tmpAtm->value.byts.str);
					break;
				case SYM:
				case STR:
					root1->u.str=newVMstr(tmpAtm->value.str);
					break;
			}
		}
		else if(root->type==PAIR)
		{
			AST_pair* objPair=root->u.pair;
			VMpair* tmpPair=newVMpair(heap);
			copyRef(root1,newVMvalue(PAIR,tmpPair,heap,1));
			pushComStack(&objPair->car,s1);
			pushComStack(&objPair->cdr,s1);
			pushComStack(tmpPair->car,s2);
			pushComStack(tmpPair->cdr,s2);
		}
	}
	freeComStack(s1);
	freeComStack(s2);
	return tmp;
}

ByteString* newByteString(size_t size,uint8_t* str)
{
	ByteString* tmp=(ByteString*)malloc(sizeof(ByteString));
	if(tmp==NULL)errors("newByteString",__FILE__,__LINE__);
	tmp->size=size;
	tmp->refcount=0;
	if(str!=NULL)
		tmp->str=copyArry(size,str);
	return tmp;
}

ByteString* copyByteArry(const ByteString* obj)
{
	if(obj==NULL)return NULL;
	ByteString* tmp=(ByteString*)malloc(sizeof(ByteString));
	if(tmp==NULL)errors("copyByteArry",__FILE__,__LINE__);
	uint8_t* tmpArry=(uint8_t*)malloc(tmp->size*sizeof(uint8_t));
	if(tmpArry==NULL)errors("copyByteArry",__FILE__,__LINE__);
	memcpy(tmpArry,obj->str,obj->size);
	tmp->size=obj->size;
	tmp->str=tmpArry;
	return tmp;
}

ByteString* newEmptyByteArry()
{
	ByteString* tmp=(ByteString*)malloc(sizeof(ByteString));
	if(tmp==NULL)errors("newEmptyByteArry",__FILE__,__LINE__);
	tmp->size=0;
	tmp->refcount=0;
	tmp->str=NULL;
	return tmp;
}

uint8_t* copyArry(size_t size,uint8_t* str)
{
	uint8_t* tmp=(uint8_t*)malloc(sizeof(uint8_t)*size);
	if(tmp==NULL)errors("copyArry",__FILE__,__LINE__);
	memcpy(tmp,str,size);
	return tmp;
}

uint8_t* createByteString(int32_t size)
{
	uint8_t* tmp=(uint8_t*)malloc(sizeof(uint8_t)*size);
	if(tmp==NULL)errors("createByteString",__FILE__,__LINE__);
	return tmp;
}

VMproc* copyVMproc(VMproc* obj,VMheap* heap)
{
	VMproc* tmp=(VMproc*)malloc(sizeof(VMproc));
	if(tmp==NULL)errors("copyVMproc",__FILE__,__LINE__);
	tmp->refcount=0;
	tmp->scp=obj->scp;
	tmp->cpc=obj->cpc;
	tmp->prevEnv=copyVMenv(obj->prevEnv,heap);
	return tmp;
}

VMenv* copyVMenv(VMenv* objEnv,VMheap* heap)
{
	VMenv* tmp=newVMenv(NULL);
	tmp->list=(VMenvNode**)malloc(sizeof(VMenvNode*)*objEnv->num);
	if(tmp->list==NULL)errors("copyVMenv",__FILE__,__LINE__);
	int i=0;
	for(;i<objEnv->num;i++)
	{
		VMenvNode* node=objEnv->list[i];
		VMvalue* v=copyVMvalue(node->value,heap);
		VMenvNode* tmp=newVMenvNode(v,node->id);
		objEnv->list[i]=tmp;
	}
	tmp->prev=(objEnv->prev->refcount==0)?copyVMenv(objEnv->prev,heap):objEnv->prev;
	return tmp;
}

void freeVMproc(VMproc* proc)
{
	if(!proc->refcount)
	{
		if(proc->prevEnv)
			freeVMenv(proc->prevEnv);
		free(proc);
	}
	else
		decreaseVMprocRefcount(proc);
}

void freeVMstr(VMstr* obj)
{
	free(obj->str);
	free(obj);
}

void freeVMenv(VMenv* obj)
{
	while(obj!=NULL)
	{
		if(!obj->refcount)
		{
			VMenv* prev=obj;
			obj=obj->prev;
			int32_t i=0;
			for(;i<prev->num;i++)
				freeVMenvNode(prev->list[i]);
			free(prev->list);
			free(prev);
		}
		else
		{
			decreaseVMenvRefcount(obj);
			break;
		}
	}
}

void releaseSource(pthread_rwlock_t* pGClock)
{
	pthread_rwlock_unlock(pGClock);
}

void lockSource(pthread_rwlock_t* pGClock)
{
	pthread_rwlock_rdlock(pGClock);
}

VMvalue* getArg(VMstack* stack)
{
	if(!(stack->tp>stack->bp))return NULL;
	VMvalue* tmp=getTopValue(stack);
	stack->tp-=1;
	return tmp;
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
				fir->u.in32=copyMemory(sec->u.in32,sizeof(int32_t));
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
					increaseVMstrRefcount(fir->u.str);
				}
				else
				{
					increaseVMstrRefcount(sec->u.str);
					fir->u.str=sec->u.str;
				}
				break;
			case PAIR:
				increaseVMpairRefcount(sec->u.pair);
				fir->u.pair=sec->u.pair;
				break;
			case PRC:
				increaseVMprocRefcount(sec->u.prc);
				fir->u.prc=sec->u.prc;
				break;
			case CONT:
				increaseVMcontRefcount(sec->u.cont);
				fir->u.cont=sec->u.cont;
				break;
			case BYTS:
				if(!sec->access)
					fir->u.byts=newByteString(sec->u.byts->size,sec->u.byts->str);
				else
				{
					increaseByteStringRefcount(sec->u.byts);
					fir->u.byts=sec->u.byts;
				}
				break;
			case CHAN:
				increaseVMChanlRefcount(sec->u.chan);
				fir->u.chan=sec->u.chan;
				break;
			case FP:
				increaseVMfpRefcount(sec->u.fp);
				fir->u.fp=sec->u.fp;
				break;
			case DLL:
				increaseVMDllRefcount(sec->u.dll);
				fir->u.dll=sec->u.dll;
				break;
			case DLPROC:
				increaseVMDlprocRefcount(sec->u.dlproc);
				fir->u.dlproc=sec->u.dlproc;
				break;
			case NIL:
				fir->u.all=NULL;
				break;
		}
	}
}

void writeRef(VMvalue* fir,VMvalue* sec)
{
	if(fir->type>=IN32&&fir->type<=DBL)
	{
		switch(sec->type)
		{
			case IN32:
				*fir->u.in32=*sec->u.in32;
				break;
			case CHR:
				*fir->u.chr=*sec->u.chr;
				break;
			case DBL:
				*fir->u.dbl=*sec->u.dbl;
				break;
		}
	}
	else if(fir->type>=SYM&&fir->type<=PAIR)
	{
		if(fir->access)freeRef(fir);
		switch(fir->type)
		{
			case PAIR:
				fir->u.pair=sec->u.pair;
				increaseVMpairRefcount(fir->u.pair);
				break;
			case PRC:
				fir->u.prc=sec->u.prc;
				increaseVMprocRefcount(fir->u.prc);
				break;
			case CONT:
				fir->u.cont=sec->u.cont;
				increaseVMcontRefcount(fir->u.cont);
				break;
			case CHAN:
				fir->u.chan=sec->u.chan;
				increaseVMChanlRefcount(fir->u.chan);
				break;
			case FP:
				fir->u.fp=sec->u.fp;
				increaseVMfpRefcount(fir->u.fp);
				break;
			case DLL:
				fir->u.dll=sec->u.dll;
				increaseVMDllRefcount(fir->u.dll);
				break;
			case DLPROC:
				fir->u.dlproc=sec->u.dlproc;
				increaseVMDlprocRefcount(fir->u.dlproc);
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
			case BYTS:
				if(!fir->access)
					memcpy(fir->u.byts->str,sec->u.byts->str,(fir->u.byts->size>sec->u.byts->size)?sec->u.byts->size:fir->u.byts->size);
				else
				{
					fir->u.byts=sec->u.byts;
					increaseByteStringRefcount(fir->u.byts);
				}
				break;
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
				else
					decreaseVMstrRefcount(obj->u.str);
				break;
			case PAIR:
				if(!obj->u.pair->refcount)
					free(obj->u.pair);
				else
					decreaseVMpairRefcount(obj->u.pair);
				break;
			case PRC:
				freeVMproc(obj->u.prc);
				break;
			case BYTS:
				if(!obj->u.byts->refcount)
				{
					if(obj->access)free(obj->u.byts->str);
					free(obj->u.byts);
				}
				else
					decreaseByteStringRefcount(obj->u.byts);
				break;
			case CONT:
				freeVMcontinuation(obj->u.cont);
				break;
			case CHAN:
				freeVMChanl(obj->u.chan);
				break;
			case FP:
				freeVMfp(obj->u.fp);
				break;
			case DLL:
				freeVMDll(obj->u.dll);
				break;
			case DLPROC:
				freeVMDlproc(obj->u.dlproc);
				break;
		}
	}
}

VMenv* castPreEnvToVMenv(PreEnv* pe,VMenv* prev,VMheap* heap,SymbolTable* table)
{
	int32_t size=0;
	PreDef* tmpDef=pe->symbols;
	while(tmpDef)
	{
		size++;
		tmpDef=tmpDef->next;
	}
	VMenv* tmp=newVMenv(prev);
	for(tmpDef=pe->symbols;tmpDef;tmpDef=tmpDef->next)
	{
		VMvalue* v=castCptrVMvalue(&tmpDef->obj,heap);
		VMenvNode* node=newVMenvNode(v,findSymbol(tmpDef->symbol,table)->id);
		addVMenvNode(node,tmp);
	}
	return tmp;
}

VMcontinuation* newVMcontinuation(VMstack* stack,ComStack* rstack)
{
	int32_t size=rstack->top;
	int32_t i=0;
	VMcontinuation* tmp=(VMcontinuation*)malloc(sizeof(VMcontinuation));
	if(!tmp)errors("newVMcontinuation",__FILE__,__LINE__);
	VMprocStatus* status=(VMprocStatus*)malloc(sizeof(VMprocStatus)*size);
	if(!status)errors("newVMcontinuation",__FILE__,__LINE__);
	tmp->stack=copyStack(stack);
	tmp->num=size-1;
	tmp->refcount=0;
	for(;i<size-1;i++)
	{
		VMrunnable* cur=rstack->data[i];
		status[i].cp=cur->cp;
		increaseVMenvRefcount(cur->localenv);
		status[i].env=cur->localenv;
		increaseVMprocRefcount(cur->proc);
		status[i].proc=cur->proc;
	}
	tmp->status=status;
	return tmp;
}

void freeVMcontinuation(VMcontinuation* cont)
{
	if(!cont->refcount)
	{
		int32_t i=0;
		int32_t size=cont->num;
		VMstack* stack=cont->stack;
		VMprocStatus* status=cont->status;
		free(stack->tpst);
		free(stack->values);
		free(stack);
		for(;i<size;i++)
		{
			freeVMenv(status[i].env);
			freeVMproc(status[i].proc);
		}
		free(status);
		free(cont);
	}
	else
		decreaseVMcontRefcount(cont);
}

VMstack* copyStack(VMstack* stack)
{
	int32_t i=0;
	VMstack* tmp=(VMstack*)malloc(sizeof(VMstack));
	if(!tmp)errors("copyStack",__FILE__,__LINE__);
	tmp->size=stack->size;
	tmp->tp=stack->tp;
	tmp->bp=stack->bp;
	tmp->values=(VMvalue**)malloc(sizeof(VMvalue*)*(tmp->size));
	if(!tmp->values)errors("copyStack",__FILE__,__LINE__);
	for(;i<stack->tp;i++)
		tmp->values[i]=stack->values[i];
	tmp->tptp=stack->tptp;
	tmp->tpst=NULL;
	tmp->tpsi=stack->tpsi;
	if(tmp->tpsi)
	{
		tmp->tpst=(uint32_t*)malloc(sizeof(uint32_t)*tmp->tpsi);
		if(!tmp->tpst)
			errors("copyStack",__FILE__,__LINE__);
		if(tmp->tptp)memcpy(tmp->tpst,stack->tpst,sizeof(int32_t)*(tmp->tptp));
	}
	return tmp;
}

VMenvNode* newVMenvNode(VMvalue* value,int32_t id)
{
	VMenvNode* tmp=(VMenvNode*)malloc(sizeof(VMenvNode));
	if(!tmp)errors("newVMenvNode",__FILE__,__LINE__);
	tmp->id=id;
	tmp->value=value;
	return tmp;
}

VMenvNode* addVMenvNode(VMenvNode* node,VMenv* env)
{
	if(!env->list)
	{
		env->num=1;
		env->list=(VMenvNode**)malloc(sizeof(VMenvNode*)*1);
		if(!env->list)errors("addVMenvNode",__FILE__,__LINE__);
		env->list[0]=node;
	}
	else
	{
		int32_t l=0;
		int32_t h=env->num-1;
		int32_t mid=0;
		while(l<=h)
		{
			mid=l+(h-l)/2;
			if(env->list[mid]->id>=node->id)
				h=mid-1;
			else
				l=mid+1;
		}
		if(env->list[mid]->id<=node->id)
			mid++;
		env->num+=1;
		int32_t i=env->num-1;
		env->list=(VMenvNode**)realloc(env->list,sizeof(VMenvNode*)*env->num);
		if(!env->list)errors("addVMenvNode",__FILE__,__LINE__);
		for(;i>mid;i--)
			env->list[i]=env->list[i-1];
		env->list[mid]=node;
	}
	return node;
}

VMenvNode* findVMenvNode(int32_t id,VMenv* env)
{
	if(!env->list)
		return NULL;
	int32_t l=0;
	int32_t h=env->num-1;
	int32_t mid;
	while(l<=h)
	{
		mid=l+(h-l)/2;
		if(env->list[mid]->id>id)
			h=mid-1;
		else if(env->list[mid]->id<id)
			l=mid+1;
		else
			return env->list[mid];
	}
	return NULL;
}

void freeVMenvNode(VMenvNode* node)
{
	free(node);
}


VMChanl* newVMChanl(int32_t maxSize)
{
	VMChanl* tmp=(VMChanl*)malloc(sizeof(VMChanl));
	if(!tmp)
		errors("newVMChanl",__FILE__,__LINE__);
	pthread_mutex_init(&tmp->lock,NULL);
	tmp->max=maxSize;
	tmp->refcount=0;
	tmp->messages=newComQueue();
	tmp->sendq=newComQueue();
	tmp->recvq=newComQueue();
	return tmp;
}

volatile int32_t getNumVMChanl(volatile VMChanl* ch)
{
	pthread_rwlock_rdlock((pthread_rwlock_t*)&ch->lock);
	uint32_t i=0;
	i=lengthComQueue(ch->messages);
	pthread_rwlock_unlock((pthread_rwlock_t*)&ch->lock);
	return i;
}

void freeVMChanl(VMChanl* ch)
{
	if(ch->refcount)
		decreaseVMChanlRefcount(ch);
	else
	{
		pthread_mutex_destroy(&ch->lock);
		freeComQueue(ch->messages);
		QueueNode* head=ch->sendq->head;
		for(;head;head=head->next)
			freeSendT(head->data);
		for(head=ch->recvq->head;head;head=head->next)
			freeRecvT(head->data);
		freeComQueue(ch->sendq);
		freeComQueue(ch->recvq);
		free(ch);
	}
}

VMChanl* copyVMChanl(VMChanl* ch,VMheap* heap)
{
	VMChanl* tmpCh=newVMChanl(ch->max);
	QueueNode* cur=ch->messages->head;
	ComQueue* tmp=newComQueue();
	for(;cur;cur=cur->next)
	{
		void* message=copyVMvalue(cur->data,heap);
		pushComQueue(message,tmp);
	}
	return tmpCh;
}

VMfp* newVMfp(FILE* fp)
{
	VMfp* tmp=(VMfp*)malloc(sizeof(VMfp));
	if(!tmp)
		errors("newVMfp",__FILE__,__LINE__);
	tmp->refcount=0;
	tmp->fp=fp;
	return tmp;
}

void freeVMfp(VMfp* fp)
{
	if(fp->refcount)
		decreaseVMfpRefcount(fp);
	else
	{
		if(fp->fp!=stdin&&fp->fp!=stdout&&fp->fp!=stderr)
			fclose(fp->fp);
		free(fp);
	}
}

VMDll* newVMDll(const char* dllName)
{
	VMDll* tmp=(VMDll*)malloc(sizeof(VMDll));
	if(!tmp)
		errors("newVMDll",__FILE__,__LINE__);
	tmp->refcount=0;
#ifdef _WIN32
	char filetype[]=".dll";
#else
	char filetype[]=".so";
#endif
	char* realDllName=(char*)malloc(sizeof(char)*(strlen(dllName)+strlen(filetype)+1));
	if(!realDllName)
		errors("newVMDll",__FILE__,__LINE__);
	sprintf(realDllName,"%s%s",dllName,filetype);
#ifdef _WIN32
	char* rpath=_fullpath(NULL,realDllName,0);
#else
	char* rpath=realpath(realDllName,0);
#endif
	if(!rpath)
	{
		perror(dllName);
		free(realDllName);
		free(tmp);
		return NULL;
	}
#ifdef _WIN32
	DllHandle handle=LoadLibrary(rpath);
	if(!handle)
	{
		TCHAR szBuf[128];
		LPVOID lpMsgBuf;
		DWORD dw = GetLastError();
		FormatMessage (
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
				NULL,
				dw,
				MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
				(LPTSTR) &lpMsgBuf,
				0, NULL );
		wsprintf(szBuf,
				_T("%s error Message (error code=%d): %s"),
				_T("CreateDirectory"), dw, lpMsgBuf);
		LocalFree(lpMsgBuf);
		fprintf(stderr,"%s\n",szBuf);
		free(rpath);
		free(realDllName);
		free(tmp);
		return NULL;
	}
#else
	DllHandle handle=dlopen(rpath,RTLD_LAZY);
	if(!handle)
	{
		perror(dlerror());
		free(rpath);
		free(realDllName);
		free(tmp);
		return NULL;
	}
#endif
	tmp->handle=handle;
	free(realDllName);
	free(rpath);
	return tmp;
}

void freeVMDll(VMDll* dll)
{
	if(dll->refcount)
		decreaseVMDllRefcount(dll);
	else
	{
#ifdef _WIN32
		FreeLibrary(dll->handle);
#else
		dlclose(dll->handle);
#endif
		free(dll);
	}
}

void* getAddress(const char* funcname,DllHandle dlhandle)
{
	void* pfunc=NULL;
#ifdef _WIN32
		pfunc=GetProcAddress(dlhandle,funcname);
#else
		pfunc=dlsym(dlhandle,funcname);
#endif
	return pfunc;
}

VMDlproc* newVMDlproc(DllFunc address,VMDll* dll)
{
	VMDlproc* tmp=(VMDlproc*)malloc(sizeof(VMDlproc));
	if(!tmp)
		errors("newVMDlproc",__FILE__,__LINE__);
	tmp->refcount=0;
	tmp->func=address;
	tmp->dll=dll;
	increaseVMDllRefcount(dll);
	return tmp;
}

void freeVMDlproc(VMDlproc* dlproc)
{
	if(dlproc->refcount)
		decreaseVMDlprocRefcount(dlproc);
	else
	{
		freeVMDll(dlproc->dll);
		free(dlproc);
	}
}

RecvT* newRecvT(FakeVM* v)
{
	RecvT* tmp=(RecvT*)malloc(sizeof(RecvT));
	tmp->v=v;
	pthread_cond_init(&tmp->cond,NULL);
	return tmp;
}

void freeRecvT(RecvT* r)
{
	pthread_cond_destroy(&r->cond);
	free(r);
}

SendT* newSendT(VMvalue* m)
{
	SendT* tmp=(SendT*)malloc(sizeof(SendT));
	tmp->m=m;
	pthread_cond_init(&tmp->cond,NULL);
	return tmp;
}

void freeSendT(SendT* s)
{
	pthread_cond_destroy(&s->cond);
	free(s);
}

void chanlRecv(RecvT* r,VMChanl* ch)
{
	pthread_mutex_lock(&ch->lock);
	if(!lengthComQueue(ch->messages))
	{
		pushComQueue(r,ch->recvq);
		pthread_cond_wait(&r->cond,&ch->lock);
	}
	VMvalue* t=getTopValue(r->v->stack);
	t->access=1;
	copyRef(t,popComQueue(ch->messages));
	if(lengthComQueue(ch->messages)<ch->max)
	{
		SendT* s=popComQueue(ch->sendq);
		if(s)
			pthread_cond_signal(&s->cond);
	}
	freeRecvT(r);
	pthread_mutex_unlock(&ch->lock);
}

void chanlSend(SendT*s,VMChanl* ch)
{
	pthread_mutex_lock(&ch->lock);
	if(lengthComQueue(ch->recvq))
	{
		RecvT* r=popComQueue(ch->recvq);
		if(r)
			pthread_cond_signal(&r->cond);
	}
	if(!ch->max||lengthComQueue(ch->messages)<ch->max-1)
		pushComQueue(s->m,ch->messages);
	else
	{
		if(lengthComQueue(ch->messages)>=ch->max-1)
		{
			pushComQueue(s,ch->sendq);
			if(lengthComQueue(ch->messages)==ch->max-1)
				pushComQueue(s->m,ch->messages);
			pthread_cond_wait(&s->cond,&ch->lock);
		}
	}
	freeSendT(s);
	pthread_mutex_unlock(&ch->lock);
}
