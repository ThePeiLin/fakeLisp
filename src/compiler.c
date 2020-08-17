#include<stdlib.h>
#include<string.h>
#include"compiler.h"
#include"syntax.h"
#include"tool.h"
#include"fake.h"
#include"opcode.h"

complr* newCompiler(intpr* inter)
{
	complr* tmp=(complr*)malloc(sizeof(complr));
	if(tmp==NULL)errors(OUTOFMEMORY);
	tmp->inter=inter;
	tmp->glob=newCompEnv(NULL);
	tmp->procs=NULL;
	return tmp;
}

void freeCompiler(complr* comp)
{
	freeIntpr(comp->inter);
	destroyCompEnv(comp->glob);
	rawproc* tmp=comp->procs;
	while(tmp!=NULL)
	{
		rawproc* prev=tmp;
		tmp=tmp->next;
		freeByteCode(prev->proc);
		free(prev);
	}
	free(comp);
}

compEnv* newCompEnv(compEnv* prev)
{
	compEnv* tmp=(compEnv*)malloc(sizeof(compEnv));
	if(tmp==NULL)errors(OUTOFMEMORY);
	tmp->prev=prev;
	tmp->symbols=NULL;
	return tmp;
}

void destroyCompEnv(compEnv* objEnv)
{
	compDef* tmpDef=objEnv->symbols;
	while(tmpDef!=NULL)
	{
		compDef* prev=tmpDef;
		tmpDef=tmpDef->next;
		free(prev->symName);
		free(prev);
	}
	free(objEnv);
}

compDef* findCompDef(const char* name,compEnv* curEnv)
{
	if(curEnv->symbols==NULL)return NULL;
	else
	{
		compDef* curDef=curEnv->symbols;
		compDef* prev=NULL;
		while(curDef&&strcmp(name,curDef->symName))
			curDef=curDef->next;
		return curDef;
	}
}
compDef* addCompDef(compEnv* curEnv,const char* name)
{
	if(curEnv->symbols==NULL)
	{
		if(!(curEnv->symbols=(compDef*)malloc(sizeof(compDef))))errors(OUTOFMEMORY);
		if(!(curEnv->symbols->symName=(char*)malloc(sizeof(char)*(strlen(name)+1))))errors(OUTOFMEMORY);
		strcpy(curEnv->symbols->symName,name);
		curEnv->symbols->count=(curEnv->prev==NULL)?0:curEnv->prev->symbols->count+1;
		curEnv->symbols->next=NULL;
		return curEnv->symbols;
	}
	else
	{
		compDef* curDef=findCompDef(name,curEnv);
		if(curDef==NULL)
		{
			compDef* prevDef=curEnv->symbols;
			while(prevDef->next!=NULL)prevDef=prevDef->next;
			if(!(curDef=(compDef*)malloc(sizeof(compDef))))errors(OUTOFMEMORY);
			if(!(curDef->symName=(char*)malloc(sizeof(char)*(strlen(name)+1))))errors(OUTOFMEMORY);
			strcpy(curDef->symName,name);
			prevDef->next=curDef;
			curDef->count=prevDef->count+1;
		}
		return curDef;
	}
}

byteCode* compile(const cptr* objCptr,compEnv* curEnv,complr* comp)
{
	if(isConst(objCptr))return compileConst(objCptr,curEnv,comp);
	if(isSymbol(objCptr))return compileSym(objCptr,curEnv,comp);
	if(isConst(objCptr))return compileConst(objCptr,curEnv,comp);
	if(isDefExpression(objCptr))return compileDef(objCptr,curEnv,comp);
	if(isSetqExpression(objCptr))return compileSym(objCptr,curEnv,comp);
	if(isCondExpression(objCptr))return compileCond(objCptr,curEnv,comp);
	if(isAndExpression(objCptr))return compileAnd(objCptr,curEnv,comp);
	if(isOrExpression(objCptr))return compileOr(objCptr,curEnv,comp);
	if(isLambdaExpression(objCptr))return compileLambda(objCptr,curEnv,comp);
	if(isListForm(objCptr))return compileListForm(objCptr,curEnv,comp);
}

byteCode* compileAtom(const cptr* objCptr)
{
	atom* tmpAtm=objCptr->value;
	byteCode* tmp=NULL;
	switch(tmpAtm->type)
	{
		case SYM:
		case STR:
			tmp=createByteCode(sizeof(char)+strlen(tmpAtm->value.str)+1);
			tmp->code[0]=FAKE_PUSH_STR;
			strcpy(tmp->code+1,tmpAtm->value.str);
			break;
		case INT:
			tmp=createByteCode(sizeof(char)+sizeof(int32_t));
			tmp->code[0]=FAKE_PUSH_INT;
			*(int32_t*)(tmp->code+1)=tmpAtm->value.num;
			break;
		case DBL:
			tmp=createByteCode(sizeof(char)+sizeof(double));
			tmp->code[0]=FAKE_PUSH_DBL;
			*(double*)(tmp->code+1)=tmpAtm->value.dbl;
			break;
		case CHR:
			tmp=createByteCode(sizeof(char)+sizeof(char));
			tmp->code[0]=FAKE_PUSH_CHR;
			tmp->code[1]=tmpAtm->value.chr;
			break;
	}
	return tmp;
}

byteCode* compileNil()
{
	byteCode* tmp=createByteCode(1);
	tmp->code[0]=FAKE_PUSH_NIL;
	return tmp;
}

byteCode* compilePair(const cptr* objCptr)
{
	byteCode* tmp=createByteCode(0);
	byteCode* beFree=NULL;
	pair* objPair=objCptr->value;
	pair* tmpPair=objPair;
	byteCode* popToCar=createByteCode(1);
	byteCode* popToCdr=createByteCode(1);
	byteCode* pushPair=createByteCode(1);
	popToCar->code[0]=FAKE_POP_CAR;
	popToCdr->code[0]=FAKE_POP_CDR;
	pushPair->code[0]=FAKE_PUSH_PAIR;
	const cptr* tmpCptr=objCptr;
	objCptr=&objPair->car;
	while(objCptr!=NULL)
	{
		if(objCptr->type==PAR)
		{
			beFree=tmp;
			tmp=codeCat(pushPair,tmp);
			freeByteCode(beFree);
			objPair=objCptr->value;
			objCptr=&objPair->car;
			continue;
		}
		else if(objCptr->type==ATM||objCptr==NIL)
		{
			beFree=tmp;
			byteCode* tmp1=(objCptr->type==ATM)?compileAtom(objCptr):compileNil();
			byteCode* tmp2=codeCat(tmp1,(objCptr==&objPair->car)?popToCar:popToCdr);
			tmp=codeCat(tmp,tmp2);
			freeByteCode(tmp1);
			freeByteCode(tmp2);
			freeByteCode(beFree);
			if(objPair!=NULL&&objCptr==&objPair->car)
			{
				objCptr=&objPair->cdr;
				continue;
			}
		}
		if(objPair!=NULL&&objCptr==&objPair->cdr)
		{
			pair* prev=NULL;
			if(objPair->prev==NULL)break;
			while(objPair->prev!=NULL&&objPair!=tmpPair)
			{
				beFree=tmp;
				prev=objPair;
				objPair=objPair->prev;
				tmp=codeCat(tmp,(prev==objPair->car.value)?popToCar:popToCdr);
				freeByteCode(beFree);
				if(prev==objPair->car.value)break;
			}
			if(objPair!=NULL)objCptr=&objPair->cdr;
			if(objPair==tmpPair&&prev==objPair->cdr.value)break;
		}
		if(objPair==NULL)break;
	}
	freeByteCode(popToCar);
	freeByteCode(popToCdr);
	freeByteCode(pushPair);
	return tmp;
}

byteCode* compileQuote(const cptr* objCptr)
{
	objCptr=&((pair*)objCptr->value)->car;
	objCptr=nextCptr(objCptr);
	switch(objCptr->type)
	{
		case PAR:return compilePair(objCptr);
		case ATM:return compileAtom(objCptr);
		case NIL:return compileNil();
	}
}

byteCode* compileConst(const cptr* objCptr,compEnv* curEnv,complr* comp)
{
	switch(objCptr->type)
	{
		case ATM:return compileAtom(objCptr);
		case NIL:return compileNil();
		case PAR:return compileQuote(objCptr);
	}
}

byteCode* compileListForm(const cptr* objCptr,compEnv* curEnv,complr* comp)
{
	pair* tmpPair=objCptr->value;
	byteCode* beFree=NULL;	
	byteCode* tmp1=NULL;
	byteCode* tmp=createByteCode(0);
	byteCode* setBound=createByteCode(1);
	byteCode* resBound=createByteCode(1);
	setBound->code[0]=FAKE_SET_BOUND;
	resBound->code[0]=FAKE_RES_BOUND;
	while(objCptr!=NULL)
	{
		if(isListForm(objCptr))
		{
			beFree=tmp;
			tmp=codeCat(tmp,setBound);
			freeByteCode(beFree);
			for(objCptr=&((pair*)objCptr->value)->car;nextCptr(objCptr)!=NULL;objCptr=nextCptr(objCptr));
			continue;
		}
		else
		{
			beFree=tmp;
			tmp1=compile(objCptr,curEnv,comp);
			tmp=codeCat(tmp,tmp1);
			freeByteCode(tmp1);
			freeByteCode(beFree);
		}
		if(prevCptr(objCptr)!=NULL)objCptr=prevCptr(objCptr);
		else
		{
			beFree=tmp;
			tmp=codeCat(tmp,resBound);
			if(objCptr->outer==tmpPair)break;
			objCptr=prevCptr(&objCptr->outer->prev->car);
		}
	}
	freeByteCode(setBound);
	freeByteCode(resBound);
	return tmp;
}

byteCode* compileDef(const cptr* objCptr,compEnv* curEnv,complr* comp)
{
	pair* tmpPair=objCptr->value;
	const cptr* fir=objCptr->value;
	const cptr* sec=nextCptr(fir);
	const cptr* tir=nextCptr(sec);
	byteCode* tmp=createByteCode(0);
	byteCode* beFree=NULL;
	byteCode* tmp1=NULL;
	byteCode* popVar=createByteCode(sizeof(char)+sizeof(int32_t));
	popVar->code[0]=FAKE_POP_VAR;
	while(isDefExpression(tir))			
	{
		fir=&((pair*)tir->value)->car;
		sec=nextCptr(fir);
		tir=nextCptr(sec);
	}
	beFree=tmp;
	tmp1=compile(tir,curEnv,comp);
	tmp=codeCat(tmp,tmp1);
	freeByteCode(beFree);
	freeByteCode(tmp1);	
	for(;;)
	{
		atom* tmpAtm=sec->value;
		compDef* tmpDef=addCompDef(curEnv,tmpAtm->value.str);
		*(int32_t*)(popVar->code+sizeof(char))=tmpDef->count;
		beFree=tmp;
		tmp=codeCat(tmp,popVar);
		freeByteCode(beFree);
		if(fir->outer==tmpPair)break;
		else
		{
			tir=&fir->outer->prev->car;
			sec=prevCptr(tir);
			fir=prevCptr(sec);
		}
	}
	freeByteCode(popVar);
	return tmp;
}

byteCode* compileSetq(const cptr* objCptr,compEnv* curEnv,complr* comp)
{
	pair* tmpPair=objCptr->value;
	const cptr* fir=objCptr->value;
	const cptr* sec=nextCptr(fir);
	const cptr* tir=nextCptr(sec);
	byteCode* tmp=createByteCode(0);
	byteCode* beFree=NULL;
	byteCode* tmp1=NULL;
	byteCode* popVar=createByteCode(sizeof(char)+sizeof(int32_t));
	popVar->code[0]=FAKE_POP_VAR;
	while(isSetqExpression(tir))
	{
		fir=&((pair*)tir->value)->car;
		sec=nextCptr(tir);
		tir=nextCptr(fir);
	}
	beFree=tmp;
	tmp1=compile(tir,curEnv,comp);
	tmp=codeCat(tmp,tmp1);
	freeByteCode(beFree);
	freeByteCode(tmp1);
	for(;;)
	{
		atom* tmpAtm=sec->value;
		compDef* tmpDef=NULL;
		compEnv* tmpEnv=curEnv;
		while(tmpEnv!=NULL&&tmpDef==NULL)
		{
			tmpDef=findCompDef(tmpAtm->value.str,tmpEnv);
			tmpEnv=tmpEnv->prev;
		}
		if(tmpDef==NULL)
		{
			freeByteCode(tmp);
			freeByteCode(popVar);
			return NULL;
		}
		else
		{
			*(int32_t*)(popVar->code+sizeof(char))=tmpDef->count;
			beFree=tmp;
			tmp=codeCat(tmp,popVar);
			freeByteCode(beFree);
			if(fir->outer==tmpPair)break;
			else
			{
				tir=&fir->outer->prev->car;
				sec=prevCptr(tir);
				fir=prevCptr(sec);
			}
		}
	}
	freeByteCode(popVar);
	return tmp;
}

byteCode* compileSym(const cptr* objCptr,compEnv* curEnv,complr* comp)
{
	byteCode* pushVar=createByteCode(sizeof(char)+sizeof(int32_t));
	atom* tmpAtm=objCptr->value;
	compDef* tmpDef=NULL;
	compEnv* tmpEnv=curEnv;
	while(tmpEnv!=NULL&&tmpDef==NULL)
	{
		tmpDef=findCompDef(tmpAtm->value.str,tmpEnv);
		tmpEnv=tmpEnv->prev;
	}
	if(tmpDef==NULL)
	{
		freeByteCode(pushVar);
		return NULL;
	}
	else
	{
		*(int32_t*)(pushVar->code+sizeof(char))=tmpDef->count;
	}
	return pushVar;
}

byteCode* compileAnd(const cptr* objCptr,compEnv* curEnv,complr* comp)
{
	pair* tmpPair=objCptr->value;
	byteCode* jumpiffalse=createByteCode(sizeof(char)+sizeof(int32_t));
	byteCode* push1=createByteCode(sizeof(char)+sizeof(int32_t));
	byteCode* pop=createByteCode(sizeof(char));
	byteCode* tmp=createByteCode(0);
	pop->code[0]=FAKE_POP;
	jumpiffalse->code[0]=FAKE_JMP_IF_FALSE;
	push1->code[0]=FAKE_PUSH_INT;
	*(int32_t*)(push1->code+sizeof(char))=1;
	while(objCptr!=NULL)
	{
		if(isAndExpression(objCptr))
		{
			for(objCptr=&((pair*)objCptr->value)->car;nextCptr(objCptr)!=NULL;objCptr=nextCptr(objCptr));
			continue;
		}
		else if(prevCptr(objCptr)!=NULL)
		{
			byteCode* beFree=tmp;
			byteCode* tmp1=compile(objCptr,curEnv,comp);
			tmp=codeCat(tmp1,tmp);
			freeByteCode(beFree);
			beFree=tmp;
			*(int32_t*)(jumpiffalse->code+sizeof(char))=tmp->size+pop->size;
			byteCode* tmp2=codeCat(jumpiffalse,pop);
			tmp=codeCat(tmp2,tmp);
			freeByteCode(beFree);
		}
		if(prevCptr(objCptr)!=NULL)objCptr=prevCptr(objCptr);
		else
		{
			byteCode* beFree=tmp;
			tmp=codeCat(push1,tmp);
			freeByteCode(beFree);
			if(objCptr->outer==tmpPair)break;
			objCptr=prevCptr(&objCptr->outer->prev->car);		
		}
	}
	freeByteCode(pop);
	freeByteCode(jumpiffalse);
	freeByteCode(push1);
	return tmp;
}

byteCode* compileOr(const cptr* objCptr,compEnv* curEnv,complr* comp)
{
	pair* tmpPair=objCptr->value;
	byteCode* jumpifture=createByteCode(sizeof(char)+sizeof(int32_t));
	byteCode* pushNil=createByteCode(sizeof(char));
	byteCode* pop=createByteCode(sizeof(char));
	byteCode* tmp=createByteCode(0);
	pop->code[0]=FAKE_POP;
	jumpifture->code[0]=FAKE_JMP_IF_TURE;
	while(objCptr!=NULL)
	{
		if(isOrExpression(objCptr))
		{
			for(objCptr=&((pair*)objCptr->value)->car;nextCptr(objCptr)!=NULL;objCptr=nextCptr(objCptr));
			continue;
		}
		else if(prevCptr(objCptr)!=NULL)
		{
			byteCode* beFree=tmp;
			byteCode* tmp1=compile(objCptr,curEnv,comp);
			tmp=codeCat(tmp1,tmp);
			freeByteCode(beFree);
			beFree=tmp;
			*(int32_t*)(jumpifture->code+sizeof(char))=tmp->size+pop->size;
			byteCode* tmp2=codeCat(jumpifture,pop);
			tmp=codeCat(tmp2,tmp);
			freeByteCode(tmp2);
			freeByteCode(beFree);
		}
		if(prevCptr(objCptr)!=NULL)objCptr=prevCptr(objCptr);
		else
		{
			byteCode* beFree=tmp;
			tmp=codeCat(pushNil,tmp);
			freeByteCode(beFree);
			if(objCptr->outer==tmpPair)break;
			objCptr=prevCptr(&objCptr->outer->prev->car);		
		}	
	}
	freeByteCode(pop);
	freeByteCode(jumpifture);
	freeByteCode(pushNil);
	return tmp;
}
byteCode* compileLambda(const cptr* objCptr,compEnv* curEnv,complr* comp)
{
	pair* tmpPair=objCptr->value;
	pair* objPair=NULL;
	byteCode* tmp=createByteCode(0);
	byteCode* proc=createByteCode(0);
	compEnv* tmpEnv=NULL;
	byteCode* initProc=createByteCode(sizeof(char)+sizeof(int32_t));
	byteCode* popVar=createByteCode(sizeof(char)+sizeof(int32_t));
	byteCode* pushProc=createByteCode(sizeof(char)+sizeof(int32_t));
	initProc->code[0]=FAKE_INIT_PROC;
	popVar->code[0]=FAKE_POP_VAR;
	pushProc->code[0]=FAKE_PUSH_PROC;
	for(;;objCptr=nextCptr(objCptr))
	{
		rawproc* tmpRawProc=NULL;
		if(isLambdaExpression(objCptr))
		{
			objPair=objCptr->value;
			tmpRawProc=addRawProc(proc,comp);
			const cptr* argCptr=&((pair*)nextCptr(&((pair*)objCptr->value)->car)->value)->car;
			while(argCptr!=NULL)
			{
				atom* tmpAtm=argCptr->value;
				compDef* tmpDef=addCompDef(tmpEnv,tmpAtm->value.str);
				*(int32_t*)(popVar->code+sizeof(char))=tmpDef->count;
				byteCode* beFree=tmpRawProc->proc;
				tmpRawProc->proc=codeCat(tmpRawProc->proc,popVar);
				freeByteCode(beFree);
				argCptr=nextCptr(argCptr);
			}
			objCptr=nextCptr(nextCptr(&((pair*)objCptr->value)->car));
			continue;
		}
		else
		{
			byteCode* tmp1=compile(objCptr,tmpEnv,comp);
			byteCode* beFree=tmpRawProc->proc;
			tmpRawProc->proc=codeCat(tmpRawProc->proc,tmp1);
			freeByteCode(tmp1);
			freeByteCode(beFree);
			objCptr=nextCptr(objCptr);
		}
		if(objCptr==NULL)
		{
			*(int32_t*)(pushProc->code+sizeof(char))=tmpRawProc->count;
			*(int32_t*)(initProc->code+sizeof(char))=tmpEnv->symbols->count;
			byteCode* tmp1=codeCat(pushProc,initProc);
			if(tmpRawProc->next==NULL)
			{
				byteCode* beFree=tmp;
				tmp=codeCat(tmp,tmp1);
				freeByteCode(beFree);
			}
			else
			{
				byteCode* beFree=tmpRawProc->next->proc;
				tmpRawProc->next->proc=codeCat(tmpRawProc->next->proc,tmp1);
				freeByteCode(beFree);
			}
			freeByteCode(tmp1);
			if(objPair!=tmpPair)
			{
				objPair=objPair->prev;
				objCptr=nextCptr(&objPair->car);
				while(objPair->prev!=NULL&&objPair->prev->cdr.value==objPair)objPair=objPair->prev;
				tmpEnv=tmpEnv->prev;
				tmpRawProc=tmpRawProc->next;
				continue;
			}
		}
	}
	freeByteCode(initProc);
	freeByteCode(pushProc);
	freeByteCode(popVar);
	freeByteCode(proc);
	return tmp;
}

byteCode* compileCond(const cptr* objCptr,compEnv* curEnv,complr* comp)
{
	int i=0;
	const cptr* cond=NULL;
	const cptr** list=NULL;
	list=(const cptr**)malloc(0);
	if(list==NULL)errors(OUTOFMEMORY);
	pair* tmpPair=objCptr->value;
	byteCode* push1=createByteCode(sizeof(char)+sizeof(int32_t));
	byteCode* pushnil=createByteCode(sizeof(char));
	byteCode* jumpiffalse=createByteCode(sizeof(char)+sizeof(int32_t));
	byteCode* jump=createByteCode(sizeof(char)+sizeof(int32_t));
	byteCode* pop=createByteCode(sizeof(char));
	byteCode* tmp=createByteCode(0);
	pop->code[0]=FAKE_POP;
	pushnil->code[0]=FAKE_PUSH_NIL;
	jumpiffalse->code[0]=FAKE_JMP_IF_FALSE;
	jump->code[0]=FAKE_JMP;
	push1->code[0]=FAKE_PUSH_INT;
	*(int32_t*)(jumpiffalse->code+sizeof(char))=pushnil->size;
	*(int32_t*)(push1->code+sizeof(char))=1;
	byteCode* endofcond=codeCat(push1,jumpiffalse);
	byteCode* beFree=endofcond;
	endofcond=codeCat(endofcond,pushnil);
	freeByteCode(beFree);
	for(;;objCptr=prevCptr(objCptr))
	{
		if(isCondExpression(objCptr))
		{
			i++;
			const cptr** tmplist=NULL;
			tmplist=realloc(list,i*sizeof(cptr*));
			if(tmplist==NULL)errors(OUTOFMEMORY);
			list[i]=objCptr;
			for(cond=&((pair*)objCptr->value)->car;nextCptr(cond)!=NULL;cond=nextCptr(cond));
			for(objCptr=&((pair*)cond->value)->car;nextCptr(objCptr)!=NULL;objCptr=nextCptr(objCptr));
			byteCode* beFree=tmp;
			tmp=codeCat(tmp,endofcond);
			*(int32_t*)(jump->code+sizeof(char))=tmp->size;
			beFree=tmp;
			tmp=codeCat(tmp,jump);
			freeByteCode(beFree);
			continue;
		}
		else
		{
			byteCode* tmp1=compile(objCptr,curEnv,comp);
			if(prevCptr(objCptr)==NULL)
			{
				*(int32_t*)(jumpiffalse->code+sizeof(char))=tmp->size+pop->size;
				byteCode* tmp2=codeCat(jumpiffalse,pop);
				byteCode* beFree=tmp1;
				tmp1=codeCat(tmp1,tmp2);
				freeByteCode(beFree);
				freeByteCode(tmp2);
			}
			byteCode* beFree=tmp;
			tmp=codeCat(tmp1,tmp);
			freeByteCode(beFree);
		}
		if(prevCptr(objCptr)==NULL)
		{
			cond=prevCptr(cond);
			if(cond->outer==tmpPair)break;
			if(prevCptr(cond)==NULL)
			{
				objCptr=list[i];
				i--;
				const cptr** tmplist=NULL;
				tmplist=(const cptr**)realloc(list,i*sizeof(cptr*));
				if(tmplist==NULL)errors(OUTOFMEMORY);
			}
		}
	}
	freeByteCode(endofcond);
	freeByteCode(pushnil);
	freeByteCode(pop);
	freeByteCode(jumpiffalse);
	freeByteCode(jump);
	return tmp;
}

byteCode* codeCat(const byteCode* fir,const byteCode* sec)
{
	byteCode* tmp=createByteCode(fir->size+sec->size);
	memcpy(tmp->code,fir->code,fir->size);
	memcpy(tmp->code+fir->size,sec->code,sec->size);
	return tmp;
}

byteCode* createByteCode(unsigned int size)
{
	byteCode* tmp=NULL;
	if(!(tmp=(byteCode*)malloc(sizeof(byteCode))))errors(OUTOFMEMORY);
	tmp->size=size;
	if(!(tmp->code=(char*)calloc(size,sizeof(char*))))errors(OUTOFMEMORY);
	return tmp;
}

void freeByteCode(byteCode* obj)
{
	free(obj->code);
	free(obj);
}

rawproc* newRawProc(int32_t count)
{
	rawproc* tmp=(rawproc*)malloc(sizeof(rawproc));
	if(tmp==NULL)errors(OUTOFMEMORY);
	tmp->count=count;
	tmp->proc=NULL;
	tmp->next=NULL;
	return tmp;
}

rawproc* addRawProc(byteCode* proc,complr* comp)
{
	byteCode* tmp=createByteCode(proc->size);
	memcpy(tmp->code,proc->code,proc->size);
	rawproc* tmpProc=newRawProc((comp->procs==NULL)?0:comp->procs->count+1);
	tmpProc->proc=tmp;
	tmpProc->next=comp->procs;
	comp->procs=tmpProc;
	return tmpProc;
}
