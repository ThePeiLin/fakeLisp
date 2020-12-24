#include"VMtool.h"
#include"tool.h"
#include<string.h>
#include<stdio.h>
#include<math.h>
#include<pthread.h>
VMcode* newVMcode(ByteCode* proc)
{
	VMcode* tmp=(VMcode*)malloc(sizeof(VMcode));
	if(tmp==NULL)errors("newVMcode",__FILE__,__LINE__);
	tmp->refcount=0;
	tmp->localenv=NULL;
	if(proc!=NULL)
	{
		tmp->size=proc->size;
		tmp->code=(char*)malloc(sizeof(char)*(tmp->size));
		if(tmp->code==NULL)errors("newVMcode",__FILE__,__LINE__);
		memcpy(tmp->code,proc->code,tmp->size);
	}
	else
	{
		tmp->size=0;
		tmp->code=NULL;
	}
	return tmp;
}

VMvalue* copyValue(VMvalue* obj,VMheap* heap)
{
	VMvalue* tmp=NULL;
	if(obj->type==NIL)
		tmp=newNilValue(heap);
	else if(obj->type==IN32)
		tmp=newVMvalue(IN32,obj->u.num,heap,1);
	else if(obj->type==DBL)
		tmp=newVMvalue(DBL,obj->u.dbl,heap,1);
	else if(obj->type==CHR)
		tmp=newVMvalue(CHR,obj->u.chr,heap,1);
	else if(obj->type==BYTA)
		tmp=newVMvalue(BYTA,obj->u.byta,heap,1);
	else if(obj->type==STR||obj->type==SYM)
		tmp=newVMvalue(obj->type,obj->u.str,heap,1);
	else if(obj->type==PRC)
		tmp=newVMvalue(PRC,copyVMcode(obj->u.prc,heap),heap,1);
	else if(obj->type==PAIR)
	{
		tmp=newVMvalue(PAIR,newVMpair(heap),heap,1);
		tmp->u.pair->car=copyValue(obj->u.pair->car,heap);
		tmp->u.pair->cdr=copyValue(obj->u.pair->cdr,heap);
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
	switch(type)
	{
		case NIL:tmp->u.all=NULL;break;
		case CHR:tmp->u.chr=(access)?copyMemory(pValue,sizeof(char)):pValue;break;
		case IN32:tmp->u.num=(access)?copyMemory(pValue,sizeof(int32_t)):pValue;break;
		case DBL:tmp->u.dbl=(access)?copyMemory(pValue,sizeof(double)):pValue;break;
		case SYM:
		case STR:
			tmp->u.str=pValue;break;
		case PAIR:
			tmp->u.pair=pValue;break;
		case PRC:
			tmp->u.prc=pValue;break;
		case BYTA:
			tmp->u.byta=pValue;break;
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
	else
	{
		switch(fir->type)
		{
			case IN32:return *fir->u.num==*sec->u.num;
			case CHR:return *fir->u.chr==*sec->u.chr;
			case DBL:return fabs(*fir->u.dbl-*sec->u.dbl)==0;
			case STR:
			case SYM:return !strcmp(fir->u.str->str,sec->u.str->str);
			case PRC:return fir->u.prc==sec->u.prc;
			case PAIR:return VMvaluecmp(fir->u.pair->car,sec->u.pair->car)&&VMvaluecmp(fir->u.pair->cdr,sec->u.pair->cdr);
			case BYTA:return bytaArryEq(fir->u.byta,sec->u.byta);
		}
	}
}

int subVMvaluecmp(VMvalue* fir,VMvalue* sec)
{
	if(fir==sec)return 1;
	else if(fir->type!=sec->type)return 0;
	else if(fir->type>=IN32&&fir->type<=SYM)
	{
		switch(fir->type)
		{
			case IN32:return *fir->u.num==*sec->u.num;
			case CHR:return *fir->u.chr==*sec->u.chr;
			case DBL:return fabs(*fir->u.dbl-*sec->u.dbl)==0;
			case SYM:return !strcmp(fir->u.str->str,sec->u.str->str);
		}
	}
	else if(fir->u.all!=sec->u.all)return 0;
}

int numcmp(VMvalue* fir,VMvalue* sec)
{
	double first=(fir->type==DBL)?*fir->u.dbl:*fir->u.num;
	double second=(sec->type==DBL)?*sec->u.dbl:*sec->u.num;
	if(fabs(first-second)==0)return 1;
	else return 0;
}

VMenv* newVMenv(int32_t bound,VMenv* prev)
{
	VMenv* tmp=(VMenv*)malloc(sizeof(VMenv));
	tmp->bound=bound;
	tmp->size=0;
	tmp->values=NULL;
	tmp->prev=prev;
	if(prev!=0)prev->refcount+=1;
	tmp->refcount=0;
	return tmp;
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
		switch(tmpAtm->type)
		{
			case IN32:tmp=newVMvalue(IN32,&tmpAtm->value.num,heap,1);break;
			case DBL:tmp=newVMvalue(DBL,&tmpAtm->value.dbl,heap,1);break;
			case CHR:tmp=newVMvalue(CHR,&tmpAtm->value.chr,heap,1);break;
			case BYTA:tmp=newVMvalue(BYTA,&tmpAtm->value.byta,heap,1);break;
			case SYM:
			case STR:tmp=newVMvalue(tmpAtm->type,newVMstr(tmpAtm->value.str),heap,1);break;
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
}

ByteArry* newByteArry(size_t size,uint8_t* arry)
{
	ByteArry* tmp=(ByteArry*)malloc(sizeof(ByteArry));
	if(tmp==NULL)errors("newByteArry",__FILE__,__LINE__);
	tmp->size=size;
	tmp->refcount=0;
	if(arry!=NULL)
		tmp->arry=copyArry(size,arry);
	return tmp;
}

ByteArry* copyByteArry(const ByteArry* obj)
{
	if(obj==NULL)return NULL;
	ByteArry* tmp=(ByteArry*)malloc(sizeof(ByteArry));
	if(tmp==NULL)errors("copyByteArry",__FILE__,__LINE__);
	int8_t* tmpArry=(int8_t*)malloc(tmp->size*sizeof(int8_t));
	if(tmpArry==NULL)errors("copyByteArry",__FILE__,__LINE__);
	memcpy(tmpArry,obj->arry,obj->size);
	tmp->size=obj->size;
	tmp->arry=tmpArry;
	return tmp;
}

ByteArry* newEmptyByteArry()
{
	ByteArry* tmp=(ByteArry*)malloc(sizeof(ByteArry));
	if(tmp==NULL)errors("newEmptyByteArry",__FILE__,__LINE__);
	return tmp;
}

uint8_t* copyArry(size_t size,uint8_t* arry)
{
	uint8_t* tmp=(uint8_t*)malloc(sizeof(uint8_t)*size);
	if(tmp==NULL)errors("copyArry",__FILE__,__LINE__);
	memcpy(tmp,arry,size);
	return tmp;
}

uint8_t* createByteArry(int32_t size)
{
	uint8_t* tmp=(uint8_t*)malloc(sizeof(uint8_t)*size);
	if(tmp==NULL)errors("createByteArry",__FILE__,__LINE__);
	return tmp;
}

VMcode* copyVMcode(VMcode* obj,VMheap* heap)
{
	VMcode* tmp=(VMcode*)malloc(sizeof(VMcode));
	if(tmp==NULL)errors("copyVMcode",__FILE__,__LINE__);
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
	VMenv* tmp=newVMenv(objEnv->size,NULL);
	tmp->bound=objEnv->bound;
	tmp->values=(VMvalue**)malloc(sizeof(VMvalue*));
	if(tmp->values==NULL)errors("copyVMenv",__FILE__,__LINE__);
	int i=0;
	for(;i<objEnv->size;i++)
		tmp->values[i]=copyValue(objEnv->values[i],heap);
	tmp->prev=(objEnv->prev->refcount==0)?copyVMenv(objEnv->prev,heap):objEnv->prev;
	return tmp;
}

void freeVMcode(VMcode* proc)
{
	if(proc->localenv)
		freeVMenv(proc->localenv);
	free(proc->code);
	free(proc);
	//printf("Free proc!\n");
}

void freeVMvalue(VMvalue* obj)
{
	if(obj->prev!=NULL)
		obj->prev->next=obj->next;
	if(obj->next!=NULL)
		obj->next->prev=obj->prev;
	if(obj->access)
	{
		switch(obj->type)
		{
			case BYTA:
				if(obj->access)
					free(obj->u.byta->arry);
				free(obj->u.byta);
				break;
			case SYM:
			case STR:
				freeVMstr(obj->u.str);break;
			case IN32:free(obj->u.num);break;
			case DBL:free(obj->u.dbl);break;
			case PRC:freeVMcode(obj->u.prc);break;
			case PAIR:free(obj->u.pair);break;
			case CHR:free(obj->u.chr);break;
		}
	}
	free(obj);
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
			free(prev->values);
			free(prev);
		}
		else
		{
			obj->refcount-=1;
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

FILE* getFile(filestack* files,int32_t count)
{
	if(count>=files->size)return NULL;
	return files->files[count];
}
