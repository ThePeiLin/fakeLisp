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
	return tmp;
}

compEnv* newCompEnv(compEnv* prev)
{
	compEnv* tmp=(compEnv*)malloc(sizeof(compEnv));
	if(tmp==NULL)errors(OUTOFMEMORY);
	tmp->prev=prev;
	if(prev!=NULL)prev->next=tmp;
	tmp->next=NULL;
	tmp->symbols=NULL;
	return tmp;
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
compDef* addCompDef(compEnv* curEnv,const char* name,const cptr* objCptr)
{
	if(curEnv->symbols==NULL)
	{
		if(!(curEnv->symbols=(compDef*)malloc(sizeof(compDef))))errors(OUTOFMEMORY);
		if(!(curEnv->symbols->symName=(char*)malloc(sizeof(char)*(strlen(name)+1))))errors(OUTOFMEMORY);
		strcpy(curEnv->symbols->symName,name);
		replace(&curEnv->symbols->obj,objCptr);
		curEnv->symbols->count=0;
		curEnv->symbols->next=NULL;
		return curEnv->symbols;
	}
	else
	{
		compDef* curDef=findCompDef(name,curEnv);
		if(curDef==NULL)
		{
			if(!(curDef=(compDef*)malloc(sizeof(compDef))))errors(OUTOFMEMORY);
			if(!(curDef->symName=(char*)malloc(sizeof(char)*(strlen(name)+1))))errors(OUTOFMEMORY);
			strcpy(curDef->symName,name);
			curDef->next=curEnv->symbols;
			curDef->count=curDef->next->count+1;
			curEnv->symbols=curDef;
			replace(&curDef->obj,objCptr);
		}
		else
			replace(&curDef->obj,objCptr);
		return curDef;
	}
}

byteCode* compile(const cptr* objCptr)
{
	if(objCptr->type==ATM)
	{
		if(isConst(objCptr))return compileConst(objCptr);
		if(isSymbol(objCptr))return compileSym(objCptr);
	}
	else if(objCptr->type==PAR)
	{
		if(isConst(objCptr))return compileConst(objCptr);
		if(isDefExpression(objCptr))return compileDef(objCptr);
		if(isSetqExpression(objCptr))return compileSym(objCptr);
		if(isCondExpression(objCptr))return compileCond(objCptr);
		if(isAndExpression(objCptr))return compileAnd(objCptr);
		if(isOrExpression(objCptr))return compileOr(objCptr);
		if(isLambdaExpression(objCptr))return compileLambda(objCptr);
		if(isListForm(objCptr))return compileListForm(objCptr);
	}
	else if(objCptr->type==NIL)return compileNil();
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

byteCode* compileConst(const cptr* objCptr)
{
	switch(objCptr->type)
	{
		case ATM:return compileAtom(objCptr);
		case NIL:return compileNil();
		case PAR:return compileQuote(objCptr);
	}
}

byteCode* compileListForm(const cptr* objCptr)
{
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
			tmp1=compile(objCptr);
			tmp=codeCat(tmp,tmp1);
			freeByteCode(tmp1);
			freeByteCode(beFree);
		}
		if(prevCptr(objCptr)!=NULL)objCptr=prevCptr(objCptr);
		else
		{
			beFree=tmp;
			tmp=codeCat(tmp,resBound);
			objCptr=prevCptr(&objCptr->outer->prev->car);
		}
	}
	freeByteCode(setBound);
	freeByteCode(resBound);
	return tmp;
}

byteCode* compileAnd(const cptr* objCptr)
{
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
