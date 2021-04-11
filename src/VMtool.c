#include"VMtool.h"
#include"common.h"
#include<string.h>
#include<stdio.h>
#include<math.h>
#include<pthread.h>

VMcode* newVMcode(ByteCode* proc,int32_t id)
{
	VMcode* tmp=(VMcode*)malloc(sizeof(VMcode));
	if(tmp==NULL)errors("newVMcode",__FILE__,__LINE__);
	pthread_mutex_init(&tmp->l,NULL);
	tmp->refcount=0;
	tmp->localenv=NULL;
	if(proc!=NULL)
	{
		tmp->id=id;
		tmp->size=proc->size;
		tmp->code=(char*)malloc(sizeof(char)*(tmp->size));
		if(tmp->code==NULL)errors("newVMcode",__FILE__,__LINE__);
		memcpy(tmp->code,proc->code,tmp->size);
	}
	else
	{
		tmp->id=id;
		tmp->size=0;
		tmp->code=NULL;
	}
	return tmp;
}

VMvalue* copyVMvalue(VMvalue* obj,VMheap* heap)
{
	VMvalue* tmp=NULL;
	switch(obj->type)
	{
		case NIL:
			tmp=newNilValue(heap);
			break;
		case IN32:
			tmp=newVMvalue(IN32,obj->u.num,heap,1);
			break;
		case DBL:
			tmp=newVMvalue(DBL,obj->u.dbl,heap,1);
			break;
		case CHR:
			tmp=newVMvalue(CHR,obj->u.chr,heap,1);
			break;
		case BYTS:
			tmp=newVMvalue(BYTS,newByteString(obj->u.byts->size,obj->u.byts->str),heap,1);
			break;
		case STR:
		case SYM:
			tmp=newVMvalue(obj->type,newVMstr(obj->u.str->str),heap,1);
			break;
		case PRC:
			tmp=newVMvalue(PRC,copyVMcode(obj->u.prc,heap),heap,1);
			break;
		case CHAN:
			tmp=newVMvalue(CHAN,copyChanl(obj->u.chan,heap),heap,1);
			break;
		case FP:
			tmp=newVMvalue(FP,obj->u.fp,heap,1);
			obj->u.fp->refcount+=1;
			break;
		case PAIR:
			tmp=newVMvalue(PAIR,newVMpair(heap),heap,1);
			tmp->u.pair->car=copyVMvalue(obj->u.pair->car,heap);
			tmp->u.pair->cdr=copyVMvalue(obj->u.pair->cdr,heap);
			break;
	}
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
	heap->size+=1;
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
			tmp->u.num=(access)?copyMemory(pValue,sizeof(int32_t)):pValue;
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

VMvalue* getCar(VMvalue* obj)
{
	if(obj->type!=PAIR)return NULL;
	else return obj->u.pair->car;
}

VMvalue* getCdr(VMvalue* obj)
{
	if(obj->type!=PAIR)return NULL;
	else return obj->u.pair->cdr;
}

int VMvaluecmp(VMvalue* fir,VMvalue* sec)
{
	if(fir==sec)return 1;
	if(fir->type!=sec->type)return 0;
	if(fir->u.all==sec->u.all)return 1;
	else
	{
		switch(fir->type)
		{
			case IN32:
				return *fir->u.num==*sec->u.num;
			case CHR:
				return *fir->u.chr==*sec->u.chr;
			case DBL:
				return fabs(*fir->u.dbl-*sec->u.dbl)==0;
			case STR:
			case SYM:
				return !strcmp(fir->u.str->str,sec->u.str->str);
			case PRC:
				return fir->u.prc==sec->u.prc;
			case PAIR:
				return VMvaluecmp(fir->u.pair->car,sec->u.pair->car)&&VMvaluecmp(fir->u.pair->cdr,sec->u.pair->cdr);
			case BYTS:
				return bytsStrEq(fir->u.byts,sec->u.byts);
		}
	}
	return 0;
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
				return *fir->u.num==*sec->u.num;
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
	double first=(fir->type==DBL)?*fir->u.dbl:*fir->u.num;
	double second=(sec->type==DBL)?*sec->u.dbl:*sec->u.num;
	if(fabs(first-second)==0)return 1;
	else return 0;
}

VMenv* newVMenv(VMenv* prev)
{
	VMenv* tmp=(VMenv*)malloc(sizeof(VMenv));
	if(tmp==NULL)errors("newVMenv",__FILE__,__LINE__);
	pthread_mutex_init(&tmp->l,NULL);
	tmp->size=0;
	tmp->list=NULL;
	tmp->prev=prev;
	increaseVMenvRefcount(prev);
	tmp->refcount=0;
	return tmp;
}

void increaseVMenvRefcount(VMenv* env)
{
	if(env!=NULL)
	{
		pthread_mutex_lock(&env->l);
		env->refcount+=1;
		pthread_mutex_unlock(&env->l);
	}
}

void decreaseVMenvRefcount(VMenv* env)
{
	if(env!=NULL)
	{
		pthread_mutex_lock(&env->l);
		env->refcount-=1;
		pthread_mutex_unlock(&env->l);
	}
}

void increaseVMcodeRefcount(VMcode* code)
{
	if(code!=NULL)
	{
		pthread_mutex_lock(&code->l);
		code->refcount+=1;
		pthread_mutex_unlock(&code->l);
	}
}

void decreaseVMcodeRefcount(VMcode* code)
{
	if(code!=NULL)
	{
		pthread_mutex_lock(&code->l);
		code->refcount-=1;
		pthread_mutex_unlock(&code->l);
	}
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

VMvalue* castCptrVMvalue(const AST_cptr* objCptr,VMheap* heap)
{
	if(objCptr->type==ATM)
	{
		AST_atom* tmpAtm=objCptr->value;
		VMvalue* tmp=NULL;
		switch((int)tmpAtm->type)
		{
			case IN32:
				tmp=newVMvalue(IN32,&tmpAtm->value.num,heap,1);
				break;
			case DBL:
				tmp=newVMvalue(DBL,&tmpAtm->value.dbl,heap,1);
				break;
			case CHR:
				tmp=newVMvalue(CHR,&tmpAtm->value.chr,heap,1);
				break;
			case BYTS:
				tmp=newVMvalue(BYTS,newByteString(tmpAtm->value.byts.size,tmpAtm->value.byts.str),heap,1);
				break;
			case SYM:
			case STR:
				tmp=newVMvalue(tmpAtm->type,newVMstr(tmpAtm->value.str),heap,1);
				break;
		}
		return tmp;
	}
	else if(objCptr->type==PAIR)
	{
		AST_pair* objPair=objCptr->value;
		VMpair* tmpPair=newVMpair(heap);
		VMvalue* tmp=newVMvalue(PAIR,tmpPair,heap,1);
		tmp->u.pair->car=castCptrVMvalue(&objPair->car,heap);
		tmp->u.pair->cdr=castCptrVMvalue(&objPair->cdr,heap);
		return tmp;
	}
	else if(objCptr->type==NIL)return newNilValue(heap);
	return NULL;
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

VMcode* copyVMcode(VMcode* obj,VMheap* heap)
{
	VMcode* tmp=(VMcode*)malloc(sizeof(VMcode));
	if(tmp==NULL)errors("copyVMcode",__FILE__,__LINE__);
	pthread_mutex_init(&tmp->l,NULL);
	tmp->refcount=0;
	tmp->size=obj->size;
	tmp->code=(char*)malloc(sizeof(char)*tmp->size);
	if(tmp->code==NULL)errors("copyVMcode",__FILE__,__LINE__);
	memcpy(tmp->code,obj->code,tmp->size);
	tmp->localenv=copyVMenv(obj->localenv,heap);
	return tmp;
}

VMenv* copyVMenv(VMenv* objEnv,VMheap* heap)
{
	VMenv* tmp=newVMenv(NULL);
	tmp->list=(VMenvNode**)malloc(sizeof(VMenvNode*)*objEnv->size);
	if(tmp->list==NULL)errors("copyVMenv",__FILE__,__LINE__);
	int i=0;
	for(;i<objEnv->size;i++)
	{
		VMenvNode* node=objEnv->list[i];
		VMvalue* v=copyVMvalue(node->value,heap);
		VMenvNode* tmp=newVMenvNode(v,node->id);
		objEnv->list[i]=tmp;
	}
	tmp->prev=(objEnv->prev->refcount==0)?copyVMenv(objEnv->prev,heap):objEnv->prev;
	return tmp;
}

void freeVMcode(VMcode* proc)
{
	if(!proc->refcount)
	{
		if(proc->localenv)
			freeVMenv(proc->localenv);
		free(proc->code);
		free(proc);
	}
	else
		decreaseVMcodeRefcount(proc);
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
			for(;i<prev->size;i++)
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
				fir->u.num=copyMemory(sec->u.num,sizeof(int32_t));
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
					fir->u.str->refcount+=1;
				}
				else
				{
					sec->u.str->refcount+=1;
					fir->u.str=sec->u.str;
				}
				break;
			case PAIR:
				sec->u.pair->refcount+=1;
				fir->u.pair=sec->u.pair;
				break;
			case PRC:
				increaseVMcodeRefcount(sec->u.prc);
				fir->u.prc=sec->u.prc;
				break;
			case CONT:
				sec->u.cont->refcount+=1;
				fir->u.cont=sec->u.cont;
				break;
			case BYTS:
				if(!sec->access)
					fir->u.byts=newByteString(sec->u.byts->size,sec->u.byts->str);
				else
				{
					sec->u.byts->refcount+=1;
					fir->u.byts=sec->u.byts;
				}
				break;
			case CHAN:
				sec->u.chan->refcount+=1;
				fir->u.chan=sec->u.chan;
				break;
			case FP:
				sec->u.fp->refcount+=1;
				fir->u.fp=sec->u.fp;
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
				*fir->u.num=*sec->u.num;
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
				fir->u.pair->refcount+=1;
				break;
			case PRC:
				fir->u.prc=sec->u.prc;
				increaseVMcodeRefcount(fir->u.prc);
				break;
			case CONT:
				fir->u.cont=sec->u.cont;
				fir->u.cont->refcount+=1;
				break;
			case CHAN:
				fir->u.chan=sec->u.chan;
				fir->u.chan->refcount+=1;
				break;
			case FP:
				fir->u.fp=sec->u.fp;
				fir->u.fp->refcount+=1;
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
					fir->u.byts->refcount+=1;
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
				else obj->u.str->refcount-=1;
				break;
			case PAIR:
				if(!obj->u.pair->refcount)
					free(obj->u.pair);
				else obj->u.pair->refcount-=1;
				break;
			case PRC:
				freeVMcode(obj->u.prc);
				break;
			case BYTS:
				if(!obj->u.byts->refcount)
				{
					if(obj->access)free(obj->u.byts->str);
					free(obj->u.byts);
				}
				else obj->u.byts->refcount-=1;
				break;
			case CONT:
				freeVMcontinuation(obj->u.cont);
				break;
			case CHAN:
				freeChanl(obj->u.chan);
				break;
			case FP:
				freeVMfp(obj->u.fp);
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

int32_t countCallChain(VMprocess* curproc)
{
	int32_t i=0;
	while(curproc)
	{
		i++;
		curproc=curproc->prev;
	}
	return i;
}

VMcontinuation* newVMcontinuation(VMstack* stack,VMprocess* curproc)
{
	int32_t size=countCallChain(curproc->prev);
	int32_t i=0;
	VMprocess* cur=curproc->prev;
	VMcontinuation* tmp=(VMcontinuation*)malloc(sizeof(VMcontinuation));
	if(!tmp)errors("newVMcontinuation",__FILE__,__LINE__);
	VMprocStatus* status=(VMprocStatus*)malloc(sizeof(VMprocStatus)*size);
	if(!status)errors("newVMcontinuation",__FILE__,__LINE__);
	tmp->stack=copyStack(stack);
	tmp->size=size;
	tmp->refcount=0;
	for(;i<size;i++)
	{
		status[i].cp=cur->cp;
		increaseVMenvRefcount(cur->localenv);
		status[i].env=cur->localenv;
		increaseVMcodeRefcount(cur->code);
		status[i].proc=cur->code;
		cur=cur->prev;
	}
	tmp->status=status;
	return tmp;
}

void freeVMcontinuation(VMcontinuation* cont)
{
	if(!cont->refcount)
	{
		int32_t i=0;
		int32_t size=cont->size;
		VMstack* stack=cont->stack;
		VMprocStatus* status=cont->status;
		free(stack->values);
		free(stack);
		for(;i<size;i++)
		{
			freeVMenv(status[i].env);
			freeVMcode(status[i].proc);
		}
		free(status);
		free(cont);
	}
	else
		cont->refcount-=1;
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
		tmp->tpst=(int32_t*)malloc(sizeof(int32_t)*tmp->tpsi);
		if(!tmp->tpst)
			errors("copyStack",__FILE__,__LINE__);
		if(tmp->tptp)memcpy(tmp->tpst,stack->tpst,sizeof(int32_t)*(tmp->tptp-1));
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
		env->size=1;
		env->list=(VMenvNode**)malloc(sizeof(VMenvNode*)*1);
		if(!env->list)errors("addVMenvNode",__FILE__,__LINE__);
		env->list[0]=node;
	}
	else
	{
		int32_t l=0;
		int32_t h=env->size-1;
		int32_t mid;
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
		env->size+=1;
		int32_t i=env->size-1;
		env->list=(VMenvNode**)realloc(env->list,sizeof(VMenvNode*)*env->size);
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
	int32_t h=env->size-1;
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


Chanl* newChanl(int32_t maxSize)
{
	Chanl* tmp=(Chanl*)malloc(sizeof(Chanl));
	if(!tmp)
		errors("newChanl",__FILE__,__LINE__);
	tmp->max=maxSize;
	tmp->size=0;
	tmp->refcount=0;
	pthread_mutex_init(&tmp->lock,NULL);
	tmp->head=NULL;
	tmp->tail=NULL;
	return tmp;
}

void freeChanl(Chanl* ch)
{
	if(ch->refcount)
		ch->refcount-=1;
	else
	{
		pthread_mutex_destroy(&ch->lock);
		freeMessage(ch->head);
		free(ch);
	}
}

void freeMessage(ThreadMessage* cur)
{
	while(cur!=NULL)
	{
		ThreadMessage* prev=cur;
		cur=cur->next;
		free(prev);
	}
}

Chanl* copyChanl(Chanl* ch,VMheap* heap)
{
	Chanl* tmpCh=newChanl(ch->max);
	tmpCh->size=ch->size;
	ThreadMessage* cur=ch->head;
	ThreadMessage* prev=NULL;
	while(cur)
	{
		ThreadMessage* tmp=(ThreadMessage*)malloc(sizeof(ThreadMessage));
		tmp->next=NULL;
		if(!tmp)
			errors("copyChanl",__FILE__,__LINE__);
		if(!tmpCh->head)
			tmpCh->head=tmp;
		if(prev)
			prev->next=tmp;
		tmp->message=copyVMvalue(cur->message,heap);
		cur=cur->next;
		prev=tmp;
	}
	tmpCh->tail=prev;
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
		fp->refcount-=1;
	else
	{
		if(fp->fp!=stdin&&fp->fp!=stdout&&fp->fp!=stderr)
			fclose(fp->fp);
		free(fp);
	}
}
