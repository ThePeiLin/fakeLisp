#include"VMtool.h"
#include"common.h"
#include"opcode.h"
#include<string.h>
#include<stdio.h>
#include<math.h>
#include<pthread.h>
#ifndef _WIN32
#include<dlfcn.h>
#else
#include<tchar.h>
#endif

extern SymbolTable GlobSymbolTable;
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

	if((IS_PAIR(other->car)&&IS_PAIR(other->car->u.pair->car))&&IS_PAIR(begin->car))
		tmpPair=hasSameVMpair(begin->car->u.pair,other->car->u.pair->car->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((IS_PAIR(other->car)&&IS_PAIR(other->car->u.pair->cdr))&&IS_PAIR(begin->car))
		tmpPair=hasSameVMpair(begin->car->u.pair,other->car->u.pair->cdr->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((IS_PAIR(other->car)&&IS_PAIR(other->car->u.pair->car))&&IS_PAIR(begin->cdr))
		tmpPair=hasSameVMpair(begin->cdr->u.pair,other->car->u.pair->car->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((IS_PAIR(other->car)&&IS_PAIR(other->car->u.pair->cdr))&&IS_PAIR(begin->cdr))
		tmpPair=hasSameVMpair(begin->cdr->u.pair,other->car->u.pair->cdr->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((IS_PAIR(other->cdr)&&IS_PAIR(other->cdr->u.pair->car))&&IS_PAIR(begin->car))
		tmpPair=hasSameVMpair(begin->car->u.pair,other->cdr->u.pair->car->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((IS_PAIR(other->cdr)&&IS_PAIR(other->cdr->u.pair->cdr))&&IS_PAIR(begin->car))
		tmpPair=hasSameVMpair(begin->car->u.pair,other->cdr->u.pair->cdr->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((IS_PAIR(other->cdr)&&IS_PAIR(other->cdr->u.pair->car))&&IS_PAIR(begin->cdr))
		tmpPair=hasSameVMpair(begin->cdr->u.pair,other->cdr->u.pair->car->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((IS_PAIR(other->cdr)&&IS_PAIR(other->cdr->u.pair->cdr))&&IS_PAIR(begin->cdr))
		tmpPair=hasSameVMpair(begin->cdr->u.pair,other->cdr->u.pair->cdr->u.pair,h);
	if(tmpPair)
		return tmpPair;
	return NULL;
}

VMpair* isCircularReference(VMpair* begin,CRL* h)
{
	VMpair* tmpPair=NULL;
	if(IS_PAIR(begin->car))
		tmpPair=hasSameVMpair(begin,begin->car->u.pair,h);
	if(tmpPair)
		return tmpPair;
	if(IS_PAIR(begin->cdr))
		tmpPair=hasSameVMpair(begin,begin->cdr->u.pair,h);
	if(tmpPair)
		return tmpPair;
	return NULL;
}

int8_t isInTheCircle(VMpair* obj,VMpair* begin,VMpair* curPair)
{
	if(obj==curPair)
		return 1;
	if((IS_PAIR(curPair->car)&&begin==curPair->car->u.pair)||(IS_PAIR(curPair->cdr)&&begin==curPair->cdr->u.pair))
		return 0;
	return ((IS_PAIR(curPair->car))&&isInTheCircle(obj,begin,curPair->car->u.pair))||((IS_PAIR(curPair->cdr))&&isInTheCircle(obj,begin,curPair->cdr->u.pair));
}



pthread_mutex_t VMenvGlobalRefcountLock=PTHREAD_MUTEX_INITIALIZER;

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
	FAKE_ASSERT(tmp,"newVMproc",__FILE__,__LINE__);
	tmp->prevEnv=NULL;
	tmp->scp=scp;
	tmp->cpc=cpc;
	return tmp;
}

VMvalue* copyVMvalue(VMvalue* obj,VMheap* heap)
{
	ComStack* s1=newComStack(32);
	ComStack* s2=newComStack(32);
	VMvalue* tmp=VM_NIL;
	pushComStack(obj,s1);
	pushComStack(&tmp,s2);
	while(!isComStackEmpty(s1))
	{
		VMvalue* root=popComStack(s1);
		VMptrTag tag=GET_TAG(root);
		VMvalue** root1=popComStack(s2);
		switch(tag)
		{
			case NIL_TAG:
			case IN32_TAG:
			case SYM_TAG:
			case CHR_TAG:
				*root1=root;
				break;
			case PTR_TAG:
				{
					ValueType type=root->type;
					switch(type)
					{
						case DBL:
							*root1=newVMvalue(DBL,root->u.dbl,heap);
							break;
						case BYTS:
							*root1=newVMvalue(BYTS,newVMByts(root->u.byts->size,root->u.byts->str),heap);
							break;
						case STR:
							*root1=newVMvalue(STR,copyStr(root->u.str),heap);
							break;
						case CONT:
						case PRC:
						case FP:
						case DLL:
						case DLPROC:
						case ERR:
							*root1=root;
							break;
						case CHAN:
							{
								VMChanl* objCh=root->u.chan;
								VMChanl* tmpCh=newVMChanl(objCh->max);
								QueueNode* cur=objCh->messages->head;
								for(;cur;cur=cur->next)
								{
									void* tmp=VM_NIL;
									pushComQueue(tmp,tmpCh->messages);
									pushComStack(cur->data,s1);
									pushComStack(tmp,s2);
								}
								*root1=newVMvalue(CHAN,tmpCh,heap);
							}
							break;
						case PAIR:
							*root1=newVMvalue(PAIR,newVMpair(),heap);
							pushComStack(&(*root1)->u.pair->car,s2);
							pushComStack(&(*root1)->u.pair->cdr,s2);
							pushComStack(root->u.pair->car,s1);
							pushComStack(root->u.pair->cdr,s1);
							break;
						default:
							break;
					}
					*root1=MAKE_VM_PTR(*root1);
				}
				break;
			default:
				return NULL;
				break;
		}
	}
	freeComStack(s1);
	freeComStack(s2);
	return tmp;
}

VMvalue* newVMvalue(ValueType type,void* pValue,VMheap* heap)
{
	switch((int)type)
	{
		case NIL:
			return VM_NIL;
			break;
		case CHR:
			return MAKE_VM_CHR(pValue);
			break;
		case IN32:
			return MAKE_VM_IN32(pValue);
			break;
		case SYM:
			return MAKE_VM_SYM(pValue);
			break;
		default:
			{
				VMvalue* tmp=(VMvalue*)malloc(sizeof(VMvalue));
				FAKE_ASSERT(tmp,"newVMvalue",__FILE__,__LINE__);
				tmp->type=type;
				tmp->mark=0;
				tmp->next=heap->head;
				tmp->prev=NULL;
				pthread_mutex_lock(&heap->lock);
				if(heap->head!=NULL)heap->head->prev=tmp;
				heap->head=tmp;
				heap->num+=1;
				pthread_mutex_unlock(&heap->lock);
				switch(type)
				{
					case DBL:
						tmp->u.dbl=copyMemory(pValue,sizeof(double));
						break;
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
					case ERR:
						tmp->u.err=pValue;break;
					default:
						return NULL;
						break;
				}
				return MAKE_VM_PTR(tmp);
			}
			break;
	}
}

VMvalue* newTrueValue(VMheap* heap)
{
	VMvalue* tmp=newVMvalue(IN32,(VMptr)1,heap);
	return tmp;
}

VMvalue* newNilValue(VMheap* heap)
{
	VMvalue* tmp=newVMvalue(NIL,NULL,heap);
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
	return obj->u.pair->car;
}

VMvalue* getVMpairCdr(VMvalue* obj)
{
	return obj->u.pair->cdr;
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
		if(!IS_PTR(root1)&&!IS_PTR(root2)&&root1!=root2)
			r=0;
		else if(GET_TAG(root1)!=GET_TAG(root2))
			r=0;
		else if(IS_PTR(root1)&&IS_PTR(root2))
		{
			if(root1->type!=root2->type)
				r=0;
			switch(root1->type)
			{
				case DBL:
					r=(fabs(*root1->u.dbl-*root2->u.dbl)==0);
					break;
				case STR:
					r=!strcmp(root1->u.str,root2->u.str);
					break;
				case BYTS:
					r=eqVMByts(root1->u.byts,root2->u.byts);
					break;
				case PAIR:
					r=1;
					pushComStack(root1->u.pair->car,s1);
					pushComStack(root1->u.pair->cdr,s1);
					pushComStack(root2->u.pair->car,s2);
					pushComStack(root2->u.pair->cdr,s2);
					break;
				default:
					r=(root1==root2);
					break;
			}
		}
		if(!r)
			break;
	}
	freeComStack(s1);
	freeComStack(s2);
	return r;
}

int subVMvaluecmp(VMvalue* fir,VMvalue* sec)
{
	return fir==sec;
}

int numcmp(VMvalue* fir,VMvalue* sec)
{
	if(GET_TAG(fir)==GET_TAG(sec)&&GET_TAG(fir)==IN32_TAG)
		return fir==sec;
	else
	{
		double first=(GET_TAG(fir)==IN32_TAG)?GET_IN32(fir):*fir->u.dbl;
		double second=(GET_TAG(sec)==IN32_TAG)?GET_IN32(sec):*sec->u.dbl;
		return fabs(first-second)==0;
	}
}

VMenv* newVMenv(VMenv* prev)
{
	VMenv* tmp=(VMenv*)malloc(sizeof(VMenv));
	FAKE_ASSERT(tmp,"newVMenv",__FILE__,__LINE__);
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

VMpair* newVMpair(void)
{
	VMpair* tmp=(VMpair*)malloc(sizeof(VMpair));
	FAKE_ASSERT(tmp,"newVMpair",__FILE__,__LINE__);
	tmp->car=VM_NIL;
	tmp->cdr=VM_NIL;
	return tmp;
}

VMvalue* castCptrVMvalue(AST_cptr* objCptr,VMheap* heap)
{
	ComStack* s1=newComStack(32);
	ComStack* s2=newComStack(32);
	VMvalue* tmp=VM_NIL;
	pushComStack(objCptr,s1);
	pushComStack(&tmp,s2);
	while(!isComStackEmpty(s1))
	{
		AST_cptr* root=popComStack(s1);
		VMvalue** root1=popComStack(s2);
		if(root->type==ATM)
		{
			AST_atom* tmpAtm=root->u.atom;
			switch(tmpAtm->type)
			{
				case IN32:
					*root1=MAKE_VM_IN32(tmpAtm->value.in32);
					break;
				case CHR:
					*root1=MAKE_VM_CHR(tmpAtm->value.chr);
					break;
				case SYM:
					*root1=MAKE_VM_SYM(addSymbolToGlob(tmpAtm->value.str)->id);
					break;
				case DBL:
					*root1=newVMvalue(DBL,&tmpAtm->value.dbl,heap);
					break;
				case BYTS:
					*root1=newVMvalue(BYTS,newVMByts(tmpAtm->value.byts.size,tmpAtm->value.byts.str),heap);
					break;
				case STR:
					*root1=newVMvalue(STR,copyStr(tmpAtm->value.str),heap);
					break;
				default:
					return NULL;
					break;
			}
		}
		else if(root->type==PAIR)
		{
			AST_pair* objPair=root->u.pair;
			VMpair* tmpPair=newVMpair();
			*root1=newVMvalue(PAIR,tmpPair,heap);
			pushComStack(&objPair->car,s1);
			pushComStack(&objPair->cdr,s1);
			pushComStack(&tmpPair->car,s2);
			pushComStack(&tmpPair->cdr,s2);
			*root1=MAKE_VM_PTR(*root1);
		}
	}
	freeComStack(s1);
	freeComStack(s2);
	return tmp;
}

VMByts* newVMByts(size_t size,uint8_t* str)
{
	VMByts* tmp=(VMByts*)malloc(sizeof(VMByts));
	FAKE_ASSERT(tmp,"newVMByts",__FILE__,__LINE__);
	tmp->size=size;
	tmp->str=(str==NULL)?NULL:copyArry(size,str);
	return tmp;
}

VMByts* copyVMByts(const VMByts* obj)
{
	if(obj==NULL)return NULL;
	VMByts* tmp=(VMByts*)malloc(sizeof(VMByts));
	FAKE_ASSERT(tmp,"copyVMByts",__FILE__,__LINE__);
	uint8_t* tmpArry=(uint8_t*)malloc(obj->size*sizeof(uint8_t));
	FAKE_ASSERT(tmpArry,"copyVMByts",__FILE__,__LINE__);
	memcpy(tmpArry,obj->str,obj->size);
	tmp->size=obj->size;
	tmp->str=tmpArry;
	return tmp;
}

VMByts* copyRefVMByts(size_t size,uint8_t* str)
{
	VMByts* tmp=(VMByts*)malloc(sizeof(VMByts));
	FAKE_ASSERT(tmp,"newVMByts",__FILE__,__LINE__);
	tmp->size=size;
	tmp->str=str;
	return tmp;
}

void VMBytsCat(VMByts* fir,const VMByts* sec)
{
	size_t firSize=fir->size;
	size_t secSize=sec->size;
	fir->str=(uint8_t*)realloc(fir->str,(firSize+secSize)*sizeof(uint8_t));
	FAKE_ASSERT(fir->str,"VMBytsCat",__FILE__,__LINE__);
	fir->size=firSize+secSize;
	memcpy(fir->str+firSize,sec->str,secSize);
}
VMByts* newEmptyVMByts()
{
	VMByts* tmp=(VMByts*)malloc(sizeof(VMByts));
	FAKE_ASSERT(tmp,"newEmptyVMByts",__FILE__,__LINE__);
	tmp->size=0;
	tmp->str=NULL;
	return tmp;
}

uint8_t* copyArry(size_t size,uint8_t* str)
{
	uint8_t* tmp=(uint8_t*)malloc(sizeof(uint8_t)*size);
	FAKE_ASSERT(tmp,"copyArry",__FILE__,__LINE__);
	memcpy(tmp,str,size);
	return tmp;
}

VMproc* copyVMproc(VMproc* obj,VMheap* heap)
{
	VMproc* tmp=(VMproc*)malloc(sizeof(VMproc));
	FAKE_ASSERT(tmp,"copyVMproc",__FILE__,__LINE__);
	tmp->scp=obj->scp;
	tmp->cpc=obj->cpc;
	tmp->prevEnv=copyVMenv(obj->prevEnv,heap);
	return tmp;
}

VMenv* copyVMenv(VMenv* objEnv,VMheap* heap)
{
	VMenv* tmp=newVMenv(NULL);
	tmp->list=(VMenvNode**)malloc(sizeof(VMenvNode*)*objEnv->num);
	FAKE_ASSERT(tmp->list,"copyVMenv",__FILE__,__LINE__);
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
	if(proc->prevEnv)
		freeVMenv(proc->prevEnv);
	free(proc);
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

VMvalue* popVMstack(VMstack* stack)
{
	if(!(stack->tp>stack->bp))
		return NULL;
	VMvalue* tmp=getTopValue(stack);
	stack->tp-=1;
	return tmp;
}

VMenv* castPreEnvToVMenv(PreEnv* pe,VMenv* prev,VMheap* heap)
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
		VMenvNode* node=newVMenvNode(v,addSymbolToGlob(tmpDef->symbol)->id);
		addVMenvNode(node,tmp);
	}
	return tmp;
}

VMcontinuation* newVMcontinuation(VMstack* stack,ComStack* rstack)
{
	int32_t size=rstack->top;
	int32_t i=0;
	VMcontinuation* tmp=(VMcontinuation*)malloc(sizeof(VMcontinuation));
	FAKE_ASSERT(tmp,"newVMcontinuation",__FILE__,__LINE__);
	VMrunnable* status=(VMrunnable*)malloc(sizeof(VMrunnable)*size);
	FAKE_ASSERT(status,"newVMcontinuation",__FILE__,__LINE__);
	tmp->stack=copyStack(stack);
	tmp->num=size-1;
	for(;i<size-1;i++)
	{
		VMrunnable* cur=rstack->data[i];
		status[i].cp=cur->cp;
		increaseVMenvRefcount(cur->localenv);
		status[i].localenv=cur->localenv;
		status[i].cpc=cur->cpc;
		status[i].scp=cur->scp;
		status[i].mark=cur->mark;
	}
	tmp->status=status;
	return tmp;
}

void freeVMcontinuation(VMcontinuation* cont)
{
	int32_t i=0;
	int32_t size=cont->num;
	VMstack* stack=cont->stack;
	VMrunnable* status=cont->status;
	free(stack->tpst);
	free(stack->values);
	free(stack);
	for(;i<size;i++)
		freeVMenv(status[i].localenv);
	free(status);
	free(cont);
}

VMstack* copyStack(VMstack* stack)
{
	int32_t i=0;
	VMstack* tmp=(VMstack*)malloc(sizeof(VMstack));
	FAKE_ASSERT(tmp,"copyStack",__FILE__,__LINE__);
	tmp->size=stack->size;
	tmp->tp=stack->tp;
	tmp->bp=stack->bp;
	tmp->values=(VMvalue**)malloc(sizeof(VMvalue*)*(tmp->size));
	FAKE_ASSERT(tmp->values,"copyStack",__FILE__,__LINE__);
	for(;i<stack->tp;i++)
		tmp->values[i]=stack->values[i];
	tmp->tptp=stack->tptp;
	tmp->tpst=NULL;
	tmp->tpsi=stack->tpsi;
	if(tmp->tpsi)
	{
		tmp->tpst=(uint32_t*)malloc(sizeof(uint32_t)*tmp->tpsi);
		FAKE_ASSERT(tmp->tpst,"copyStack",__FILE__,__LINE__);
		if(tmp->tptp)memcpy(tmp->tpst,stack->tpst,sizeof(int32_t)*(tmp->tptp));
	}
	return tmp;
}

VMenvNode* newVMenvNode(VMvalue* value,int32_t id)
{
	VMenvNode* tmp=(VMenvNode*)malloc(sizeof(VMenvNode));
	FAKE_ASSERT(tmp,"newVMenvNode",__FILE__,__LINE__);
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
		FAKE_ASSERT(env->list,"addVMenvNode",__FILE__,__LINE__);
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
		FAKE_ASSERT(env->list,"addVMenvNode",__FILE__,__LINE__);
		for(;i>mid;i--)
			env->list[i]=env->list[i-1];
		env->list[mid]=node;
	}
	return node;
}

VMenvNode* findVMenvNode(Sid_t id,VMenv* env)
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
	FAKE_ASSERT(tmp,"newVMChanl",__FILE__,__LINE__);
	pthread_mutex_init(&tmp->lock,NULL);
	tmp->max=maxSize;
	tmp->messages=newComQueue();
	tmp->sendq=newComQueue();
	tmp->recvq=newComQueue();
	return tmp;
}

int32_t getNumVMChanl(VMChanl* ch)
{
	pthread_rwlock_rdlock((pthread_rwlock_t*)&ch->lock);
	uint32_t i=0;
	i=lengthComQueue(ch->messages);
	pthread_rwlock_unlock((pthread_rwlock_t*)&ch->lock);
	return i;
}

void freeVMChanl(VMChanl* ch)
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

void freeVMfp(FILE* fp)
{
	if(fp!=stdin&&fp!=stdout&&fp!=stderr)
		fclose(fp);
}

DllHandle* newVMDll(const char* dllName)
{
#ifdef _WIN32
	char filetype[]=".dll";
#else
	char filetype[]=".so";
#endif
	size_t len=strlen(dllName)+strlen(filetype)+1;
	char* realDllName=(char*)malloc(sizeof(char)*len);
	FAKE_ASSERT(realDllName,"newVMDll",__FILE__,__LINE__);
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
		return NULL;
	}
#endif
	free(realDllName);
	free(rpath);
	return handle;
}

void freeVMDll(DllHandle* dll)
{
	if(dll)
	{
#ifdef _WIN32
		FreeLibrary(dll);
#else
		dlclose(dll);
#endif
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

VMDlproc* newVMDlproc(DllFunc address,VMvalue* dll)
{
	VMDlproc* tmp=(VMDlproc*)malloc(sizeof(VMDlproc));
	FAKE_ASSERT(tmp,"newVMDlproc",__FILE__,__LINE__);
	tmp->func=address;
	tmp->dll=dll;
	return tmp;
}

void freeVMDlproc(VMDlproc* dlproc)
{
	free(dlproc);
}

VMerror* newVMerror(const char* who,const char* type,const char* message)
{
	VMerror* t=(VMerror*)malloc(sizeof(VMerror));
	FAKE_ASSERT(t,"newVMerror",__FILE__,__LINE__);
	t->who=copyStr(who);
	t->type=addSymbolToGlob(type)->id;
	t->message=copyStr(message);
	return t;
}

VMerror* newVMerrorWithSid(const char* who,Sid_t type,const char* message)
{
	VMerror* t=(VMerror*)malloc(sizeof(VMerror));
	FAKE_ASSERT(t,"newVMerror",__FILE__,__LINE__);
	t->who=copyStr(who);
	t->type=type;
	t->message=copyStr(message);
	return t;
}

void freeVMerror(VMerror* err)
{
	free(err->who);
	free(err->message);
	free(err);
}

RecvT* newRecvT(FakeVM* v)
{
	RecvT* tmp=(RecvT*)malloc(sizeof(RecvT));
	FAKE_ASSERT(tmp,"newRecvT",__FILE__,__LINE__);
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
	FAKE_ASSERT(tmp,"newSendT",__FILE__,__LINE__);
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
	SET_RETURN("chanlRecv",popComQueue(ch->messages),r->v->stack);
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

VMTryBlock* newVMTryBlock(Sid_t sid,uint32_t tp,long int rtp)
{
	VMTryBlock* t=(VMTryBlock*)malloc(sizeof(VMTryBlock));
	FAKE_ASSERT(t,"newVMTryBlock",__FILE__,__LINE__);
	t->sid=sid;
	t->hstack=newComStack(32);
	t->tp=tp;
	t->rtp=rtp;
	return t;
}

void freeVMTryBlock(VMTryBlock* b)
{
	ComStack* hstack=b->hstack;
	while(!isComStackEmpty(hstack))
	{
		VMerrorHandler* h=popComStack(hstack);
		freeVMerrorHandler(h);
	}
	freeComStack(b->hstack);
	free(b);
}

VMerrorHandler* newVMerrorHandler(Sid_t type,VMproc* proc)
{
	VMerrorHandler* t=(VMerrorHandler*)malloc(sizeof(VMerrorHandler));
	FAKE_ASSERT(t,"newVMerrorHandler",__FILE__,__LINE__);
	t->type=type;
	t->proc=proc;
	return t;
}

void freeVMerrorHandler(VMerrorHandler* h)
{
	freeVMproc(h->proc);
	free(h);
}

int raiseVMerror(VMvalue* ev,FakeVM* exe)
{
	VMerror* err=ev->u.err;
	while(!isComStackEmpty(exe->tstack))
	{
		VMTryBlock* tb=topComStack(exe->tstack);
		VMstack* stack=exe->stack;
		stack->tp=tb->tp;
		while(!isComStackEmpty(tb->hstack))
		{
			VMerrorHandler* h=popComStack(tb->hstack);
			if(h->type==err->type)
			{
				long int increment=exe->rstack->top-tb->rtp;
				unsigned int i=0;
				for(;i<increment;i++)
				{
					VMrunnable* runnable=popComStack(exe->rstack);
					freeVMenv(runnable->localenv);
					free(runnable);
				}
				VMrunnable* prevRunnable=topComStack(exe->rstack);
				VMrunnable* r=newVMrunnable(h->proc);
				r->localenv=newVMenv(prevRunnable->localenv);
				VMenv* curEnv=r->localenv;
				Sid_t idOfError=tb->sid;
				VMenvNode* errorNode=findVMenvNode(idOfError,curEnv);
				if(!errorNode)
					errorNode=addVMenvNode(newVMenvNode(NULL,idOfError),curEnv);
				errorNode->value=ev;
				pushComStack(r,exe->rstack);
				freeVMerrorHandler(h);
				return 1;
			}
			freeVMerrorHandler(h);
		}
		freeVMTryBlock(popComStack(exe->tstack));
	}
	fprintf(stderr,"error of %s :%s",err->who,err->message);
	void* i[3]={exe,(void*)255,(void*)1};
	exe->callback(i);
	return 255;
}

VMrunnable* newVMrunnable(VMproc* code)
{
	VMrunnable* tmp=(VMrunnable*)malloc(sizeof(VMrunnable));
	FAKE_ASSERT(tmp,"newVMrunnable",__FILE__,__LINE__);
	if(code)
		tmp->cp=code->scp;
	tmp->scp=code->scp;
	tmp->cpc=code->cpc;
	tmp->mark=0;
	return tmp;
}

char* genErrorMessage(unsigned int type,VMrunnable* r,FakeVM* exe)
{
	int32_t cp=r->cp;
	LineNumTabNode* node=findLineNumTabNode(cp,exe->lnt);
	char* filename=GlobSymbolTable.idl[node->fid]->symbol;
	char* line=intToString(node->line);
	size_t len=strlen("at line  of \n")+strlen(filename)+strlen(line)+1;
	char* lineNumber=(char*)malloc(sizeof(char)*len);
	FAKE_ASSERT(lineNumber,"genErrorMessage",__FILE__,__LINE__);
	sprintf(lineNumber,"at line %s of %s\n",line,filename);
	free(line);
	char* t=copyStr("");
	switch(type)
	{
		case WRONGARG:
			t=strCat(t,"Wrong arguement ");
			break;
		case STACKERROR:
			t=strCat(t,"Stack error ");
			break;
		case TOOMANYARG:
			t=strCat(t,"Too many arguements ");
			break;
		case TOOFEWARG:
			t=strCat(t,"Too few arguements ");
			break;
		case CANTCREATETHREAD:
			t=strCat(t,"Can't create thread.\n");
			break;
		case SYMUNDEFINE:
			t=strCat(t,"Symbol ");
			t=strCat(t,GlobSymbolTable.idl[getSymbolIdInByteCode(exe->code+r->cp)]->symbol);
			t=strCat(t," is undefined ");
			break;
		case INVOKEERROR:
			t=strCat(t,"Try to invoke an object that isn't procedure,continuation or native procedure.\n");
			break;
		case LOADDLLFAILD:
			t=strCat(t,"Faild to load dll \"");
			t=strCat(t,exe->stack->values[exe->stack->tp-1]->u.str);
			t=strCat(t,"\" ");
			break;
		case INVALIDSYMBOL:
			t=strCat(t,"Invalid symbol ");
			t=strCat(t,exe->stack->values[exe->stack->tp-1]->u.str);
			t=strCat(t," ");
			break;
		case DIVZERROERROR:
			t=strCat(t,"Divided by zero ");
			break;
		case FILEFAILURE:
			t=strCat(t,"Failed for file:\"");
			t=strCat(t,exe->stack->values[exe->stack->tp-1]->u.str);
			t=strCat(t,"\" ");
			break;
	}
	t=strCat(t,lineNumber);
	free(lineNumber);
	return t;
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

int resBp(VMstack* stack)
{
	if(stack->tp>stack->bp)
		return TOOMANYARG;
	VMvalue* prevBp=getTopValue(stack);
	stack->bp=GET_IN32(prevBp);
	stack->tp-=1;
	return 0;
}

void princVMvalue(VMvalue* objValue,FILE* fp,CRL** h)
{
	int access=!h;
	CRL* head=NULL;
	if(!h)h=&head;
	VMpair* cirPair=NULL;
	int32_t CRLcount=-1;
	int8_t isInCir=0;
	VMptrTag tag=GET_TAG(objValue);
	switch(tag)
	{
		case NIL_TAG:
			fprintf(fp,"()");
			break;
		case IN32_TAG:
			fprintf(fp,"%d",GET_IN32(objValue));
			break;
		case CHR_TAG:
			putc(GET_CHR(objValue),fp);
			break;
		case SYM_TAG:
			fprintf(fp,"%s",getGlobSymbolWithId(GET_SYM(objValue))->symbol);
			break;
		case PTR_TAG:
			{
				switch(objValue->type)
				{
					case DBL:
						fprintf(fp,"%lf",*objValue->u.dbl);
						break;
					case STR:
						fprintf(fp,"%s",objValue->u.str);
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
						for(;IS_PAIR(objValue);objValue=getVMpairCdr(objValue))
						{
							VMvalue* tmpValue=getVMpairCar(objValue);
							if(IS_PAIR(tmpValue)&&(CRLcount=findCRLcount(tmpValue->u.pair,*h))!=-1)
								fprintf(fp,"#%d#",CRLcount);
							else
								princVMvalue(tmpValue,fp,h);
							tmpValue=getVMpairCdr(objValue);
							if(tmpValue!=VM_NIL&&!IS_PAIR(tmpValue))
							{
								putc(',',fp);
								princVMvalue(tmpValue,fp,h);
							}
							else if(IS_PAIR(tmpValue)&&(CRLcount=findCRLcount(tmpValue->u.pair,*h))!=-1)
							{
								fprintf(fp,",#%d#",CRLcount);
								break;
							}
							else if(tmpValue!=VM_NIL)
								putc(' ',fp);
						}
						putc(')',fp);
						break;
					case BYTS:
						printByteStr(objValue->u.byts->size,objValue->u.byts->str,fp,0);
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
					case ERR:
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

void writeVMvalue(VMvalue* objValue,FILE* fp,CRL** h)
{
	int access=!h;
	CRL* head=NULL;
	if(!h)h=&head;
	VMpair* cirPair=NULL;
	int8_t isInCir=0;
	int32_t CRLcount=-1;
	VMptrTag tag=GET_TAG(objValue);
	switch(tag)
	{
		case NIL_TAG:
			fprintf(fp,"()");
			break;
		case IN32_TAG:
			fprintf(fp,"%d",GET_IN32(objValue));
			break;
		case CHR_TAG:
			printRawChar(GET_CHR(objValue),fp);
			break;
		case SYM_TAG:
			fprintf(fp,"%s",getGlobSymbolWithId(GET_SYM(objValue))->symbol);
			break;
		case PTR_TAG:
			{
				switch(objValue->type)
				{
					case DBL:
						fprintf(fp,"%lf",*objValue->u.dbl);
						break;
					case STR:
						printRawString(objValue->u.str,fp);
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
						for(;IS_PAIR(objValue);objValue=getVMpairCdr(objValue))
						{
							VMvalue* tmpValue=getVMpairCar(objValue);
							if(IS_PAIR(tmpValue)&&(CRLcount=findCRLcount(tmpValue->u.pair,*h))!=-1)
								fprintf(fp,"#%d#",CRLcount);
							else
								writeVMvalue(tmpValue,fp,h);
							tmpValue=getVMpairCdr(objValue);
							if(tmpValue!=VM_NIL&&!IS_PAIR(tmpValue))
							{
								putc(',',fp);
								writeVMvalue(tmpValue,fp,h);
							}
							else if(IS_PAIR(tmpValue)&&(CRLcount=findCRLcount(tmpValue->u.pair,*h))!=-1)
							{
								fprintf(fp,",#%d#",CRLcount);
								break;
							}
							else if(tmpValue!=VM_NIL)
								putc(' ',fp);
						}
						putc(')',fp);
						break;
					case BYTS:
						printByteStr(objValue->u.byts->size,objValue->u.byts->str,fp,1);
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
					case ERR:
						fprintf(fp,"<#err w:%s t:%s m:%s>",objValue->u.err->who,getGlobSymbolWithId(objValue->u.err->type)->symbol,objValue->u.err->message);
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

int eqVMByts(const VMByts* fir,const VMByts* sec)
{
	if(fir->size!=sec->size)
		return 0;
	return !memcmp(fir->str,sec->str,sec->size);
}

VMvalue* newVMChref(VMvalue* from,char* obj)
{
	VMChref* tmp=(VMChref*)malloc(sizeof(VMChref));
	FAKE_ASSERT(tmp,"newVMChref",__FILE__,__LINE__);
	tmp->from=from;
	tmp->obj=obj;
	return MAKE_VM_CHF(tmp);
}

VMvalue* GET_VAL(VMvalue* P)
{
	if(IS_REF(P))
		return *(VMvalue**)(GET_PTR(P));
	else if(IS_CHF(P))
	{
		P=GET_PTR(P);
		VMvalue* t=MAKE_VM_CHR(*((VMChref*)P)->obj);
		free(P);
		return t;
	}
	return P;
}
