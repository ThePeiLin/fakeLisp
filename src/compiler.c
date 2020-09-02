#include<stdlib.h>
#include<string.h>
#include"compiler.h"
#include"syntax.h"
#include"tool.h"
#include"preprocess.h"
#include"opcode.h"

byteCode* compile(cptr* objCptr,compEnv* curEnv,intpr* inter,errorStatus* status)
{
	if((!macroExpand(objCptr)&&hasKeyWord(objCptr))||!isLegal(objCptr))
	{
		status->status=SYNTAXERROR;
		status->place=objCptr;
		return NULL;
	}
	if(isConst(objCptr))return compileConst(objCptr,curEnv,inter,status);
	if(isSymbol(objCptr))return compileSym(objCptr,curEnv,inter,status);
	if(isDefExpression(objCptr))return compileDef(objCptr,curEnv,inter,status);
	if(isSetqExpression(objCptr))return compileSetq(objCptr,curEnv,inter,status);
	if(isCondExpression(objCptr))return compileCond(objCptr,curEnv,inter,status);
	if(isAndExpression(objCptr))return compileAnd(objCptr,curEnv,inter,status);
	if(isOrExpression(objCptr))return compileOr(objCptr,curEnv,inter,status);
	if(isLambdaExpression(objCptr))return compileLambda(objCptr,curEnv,inter,status);
	if(isListForm(objCptr))return compileListForm(objCptr,curEnv,inter,status);
}

byteCode* compileAtom(cptr* objCptr)
{
	atom* tmpAtm=objCptr->value;
	byteCode* tmp=NULL;
	switch(tmpAtm->type)
	{
		case SYM:
			tmp=createByteCode(sizeof(char)+strlen(tmpAtm->value.str)+1);
			tmp->code[0]=FAKE_PUSH_SYM;
			strcpy(tmp->code+1,tmpAtm->value.str);
			break;
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

byteCode* compilePair(cptr* objCptr)
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
	while(objCptr!=NULL)
	{
		if(objCptr->type==PAR)
		{
			beFree=tmp;
			tmp=codeCat(tmp,pushPair);
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
			if(objPair==tmpPair&&(prev==objPair->cdr.value||prev==NULL))break;
		}
		if(objPair==NULL)break;
	}
	freeByteCode(popToCar);
	freeByteCode(popToCdr);
	freeByteCode(pushPair);
	return tmp;
}

byteCode* compileQuote(cptr* objCptr)
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

byteCode* compileConst(cptr* objCptr,compEnv* curEnv,intpr* inter,errorStatus* status)
{
		if(objCptr->type==ATM)return compileAtom(objCptr);
		if(isNil(objCptr))return compileNil();
		if(isQuoteExpression)return compileQuote(objCptr);
}

byteCode* compileListForm(cptr* objCptr,compEnv* curEnv,intpr* inter,errorStatus* status)
{
	cptr* headoflist=NULL;
	pair* tmpPair=objCptr->value;
	byteCode* beFree=NULL;
	byteCode* tmp1=NULL;
	byteCode* tmp=createByteCode(0);
	byteCode* setBp=createByteCode(1);
	byteCode* resBp=createByteCode(1);
	setBp->code[0]=FAKE_SET_BP;
	resBp->code[0]=FAKE_RES_BP;
	for(;;)
	{
		if(objCptr==NULL)
		{
			beFree=tmp;
			tmp=codeCat(tmp,resBp);
			freeByteCode(beFree);
			if(headoflist->outer==tmpPair)break;
			objCptr=prevCptr(&headoflist->outer->prev->car);
			for(headoflist=&headoflist->outer->prev->car;prevCptr(headoflist)!=NULL;headoflist=prevCptr(headoflist));
			continue;
		}
		if(isListForm(objCptr))
		{
			beFree=tmp;
			tmp=codeCat(tmp,setBp);
			freeByteCode(beFree);
			headoflist=&((pair*)objCptr->value)->car;
			for(objCptr=headoflist;nextCptr(objCptr)!=NULL;objCptr=nextCptr(objCptr));
		}
		else
		{
			beFree=tmp;
			tmp1=compile(objCptr,curEnv,inter,status);
			if(status->status!=0)
			{
				freeByteCode(tmp);
				tmp=NULL;
				break;
			}
			tmp=codeCat(tmp,tmp1);
			freeByteCode(tmp1);
			freeByteCode(beFree);
			objCptr=prevCptr(objCptr);
		}
	}
	freeByteCode(setBp);
	freeByteCode(resBp);
	return tmp;
}

byteCode* compileDef(cptr* tir,compEnv* curEnv,intpr* inter,errorStatus* status)
{
	pair* tmpPair=tir->value;
	cptr* fir=NULL;
	cptr* sec=NULL;
	cptr* objCptr=NULL;
	byteCode* tmp=createByteCode(0);
	byteCode* beFree=NULL;
	byteCode* tmp1=NULL;
	byteCode* pushTop=createByteCode(sizeof(char));
	byteCode* popVar=createByteCode(sizeof(char)+sizeof(int32_t));
	popVar->code[0]=FAKE_POP_VAR;
	pushTop->code[0]=FAKE_PUSH_TOP;
	while(isDefExpression(tir))			
	{
		fir=&((pair*)tir->value)->car;
		sec=nextCptr(fir);
		tir=nextCptr(sec);
	}
	objCptr=tir;
	for(;;)
	{
		atom* tmpAtm=sec->value;
		compDef* tmpDef=addCompDef(curEnv,tmpAtm->value.str);
		*(int32_t*)(popVar->code+sizeof(char))=tmpDef->count;
		beFree=tmp;
		byteCode* tmp2=codeCat(pushTop,popVar);
		tmp=codeCat(tmp,tmp2);
		freeByteCode(beFree);
		freeByteCode(tmp2);
		if(fir->outer==tmpPair)break;
		else
		{
			tir=&fir->outer->prev->car;
			sec=prevCptr(tir);
			fir=prevCptr(sec);
		}
	}
	beFree=tmp;
	tmp1=compile(objCptr,curEnv,inter,status);
	if(status->status!=0)
	{
		freeByteCode(tmp);
		freeByteCode(popVar);
		freeByteCode(pushTop);
		return NULL;
	}
	tmp=codeCat(tmp1,tmp);
	freeByteCode(beFree);
	freeByteCode(tmp1);	
	freeByteCode(popVar);
	freeByteCode(pushTop);
	return tmp;
}

byteCode* compileSetq(cptr* objCptr,compEnv* curEnv,intpr* inter,errorStatus* status)
{
	pair* tmpPair=objCptr->value;
	cptr* fir=&((pair*)objCptr->value)->car;
	cptr* sec=nextCptr(fir);
	cptr* tir=nextCptr(sec);
	byteCode* tmp=createByteCode(0);
	byteCode* beFree=NULL;
	byteCode* tmp1=NULL;
	byteCode* pushTop=createByteCode(sizeof(char));
	byteCode* popVar=createByteCode(sizeof(char)+sizeof(int32_t));
	popVar->code[0]=FAKE_POP_VAR;
	pushTop->code[0]=FAKE_PUSH_TOP;
	while(isSetqExpression(tir))
	{
		fir=&((pair*)tir->value)->car;
		sec=nextCptr(fir);
		tir=nextCptr(sec);
	}
	beFree=tmp;
	tmp1=compile(tir,curEnv,inter,status);
	if(status->status!=0)
	{
		freeByteCode(tmp);
		freeByteCode(popVar);
		freeByteCode(pushTop);
		return NULL;
	}
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
			status->status=SYNTAXERROR;
			status->place=objCptr;
			freeByteCode(tmp);
			freeByteCode(popVar);
			freeByteCode(pushTop);
			return NULL;
		}
		else
		{
			*(int32_t*)(popVar->code+sizeof(char))=tmpDef->count;
			byteCode* tmp2=codeCat(pushTop,popVar);
			beFree=tmp;
			tmp=codeCat(tmp,tmp2);
			freeByteCode(beFree);
			freeByteCode(tmp2);
			if(fir->outer==tmpPair)break;
			else
			{
				tir=&fir->outer->prev->car;
				sec=prevCptr(tir);
				fir=prevCptr(sec);
			}
		}
	}
	freeByteCode(pushTop);
	freeByteCode(popVar);
	return tmp;
}

byteCode* compileSym(cptr* objCptr,compEnv* curEnv,intpr* inter,errorStatus* status)
{
	byteCode* pushVar=createByteCode(sizeof(char)+sizeof(int32_t));
	pushVar->code[0]=FAKE_PUSH_VAR;
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
		status->status=SYNTAXERROR;
		status->place=objCptr;
		freeByteCode(pushVar);
		return NULL;
	}
	else
	{
		*(int32_t*)(pushVar->code+sizeof(char))=tmpDef->count;
	}
	return pushVar;
}

byteCode* compileAnd(cptr* objCptr,compEnv* curEnv,intpr* inter,errorStatus* status)
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
			byteCode* tmp1=compile(objCptr,curEnv,inter,status);
			if(status->status!=0)
			{
				freeByteCode(tmp);
				freeByteCode(pop);
				freeByteCode(jumpiffalse);
				freeByteCode(push1);
				return NULL;
			}
			tmp=codeCat(tmp1,tmp);
			freeByteCode(beFree);
			beFree=tmp;
			*(int32_t*)(jumpiffalse->code+sizeof(char))=tmp->size+pop->size;
			byteCode* tmp2=codeCat(jumpiffalse,pop);
			tmp=codeCat(tmp2,tmp);
			freeByteCode(beFree);
			objCptr=prevCptr(objCptr);
		}
		if(prevCptr(objCptr)==NULL)
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

byteCode* compileOr(cptr* objCptr,compEnv* curEnv,intpr* inter,errorStatus* status)
{
	pair* tmpPair=objCptr->value;
	byteCode* jumpifture=createByteCode(sizeof(char)+sizeof(int32_t));
	byteCode* pushnil=createByteCode(sizeof(char));
	byteCode* pop=createByteCode(sizeof(char));
	byteCode* tmp=createByteCode(0);
	pushnil->code[0]=FAKE_PUSH_NIL;
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
			byteCode* tmp1=compile(objCptr,curEnv,inter,status);
			if(status->status!=0)
			{
				freeByteCode(tmp);
				freeByteCode(jumpifture);
				freeByteCode(pushnil);
				return NULL;
			}
			tmp=codeCat(tmp1,tmp);
			freeByteCode(beFree);
			beFree=tmp;
			*(int32_t*)(jumpifture->code+sizeof(char))=tmp->size+pop->size;
			byteCode* tmp2=codeCat(jumpifture,pop);
			tmp=codeCat(tmp2,tmp);
			freeByteCode(tmp2);
			freeByteCode(beFree);
			objCptr=prevCptr(objCptr);
		}
		if(prevCptr(objCptr)==NULL)
		{
			byteCode* beFree=tmp;
			tmp=codeCat(pushnil,tmp);
			freeByteCode(beFree);
			if(objCptr->outer==tmpPair)break;
			objCptr=prevCptr(&objCptr->outer->prev->car);		
		}	
	}
	freeByteCode(pop);
	freeByteCode(jumpifture);
	freeByteCode(pushnil);
	return tmp;
}

byteCode* compileLambda(cptr* objCptr,compEnv* curEnv,intpr* inter,errorStatus* status)
{
	pair* tmpPair=objCptr->value;
	pair* objPair=NULL;
	rawproc* tmpRawProc=NULL;
	rawproc* prevRawProc=inter->procs;
	byteCode* tmp=createByteCode(0);
	byteCode* proc=createByteCode(0);
	compEnv* tmpEnv=curEnv;
	byteCode* initProc=createByteCode(sizeof(char)+sizeof(int32_t));
	byteCode* popVar=createByteCode(sizeof(char)+sizeof(int32_t));
	byteCode* pushProc=createByteCode(sizeof(char)+sizeof(int32_t));
	byteCode* endproc=createByteCode(sizeof(char));
	byteCode* pop=createByteCode(sizeof(char));
	endproc->code[0]=FAKE_END_PROC;
	initProc->code[0]=FAKE_INIT_PROC;
	popVar->code[0]=FAKE_POP_VAR;
	pushProc->code[0]=FAKE_PUSH_PROC;
	pop->code[0]=FAKE_POP;
	for(;;)
	{
		if(objCptr==NULL)
		{
			*(int32_t*)(pushProc->code+sizeof(char))=tmpRawProc->count;
			*(int32_t*)(initProc->code+sizeof(char))=(tmpEnv->symbols==NULL)?0:tmpEnv->symbols->count;
			byteCode* tmp1=codeCat(pushProc,initProc);
			if(tmpRawProc->next==prevRawProc)
			{
				byteCode* beFree=tmp;
				tmp=codeCat(tmp,tmp1);
				freeByteCode(beFree);
				beFree=tmp;
			}
			else
			{
				byteCode* beFree=tmpRawProc->next->proc;
				tmpRawProc->next->proc=codeCat(tmpRawProc->next->proc,tmp1);
				freeByteCode(beFree);
			}
			byteCode* beFree=tmpRawProc->proc;
			tmpRawProc->proc=codeCat(tmpRawProc->proc,endproc);
			freeByteCode(beFree);
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
			else break;
		}
		if(isLambdaExpression(objCptr))
		{
			tmpEnv=newCompEnv(tmpEnv);
			objPair=objCptr->value;
			objCptr=&objPair->car;
			tmpRawProc=addRawProc(proc,inter);
			cptr* argCptr=&((pair*)nextCptr(objCptr)->value)->car;
			while(argCptr!=NULL&&argCptr->type==ATM)
			{
				atom* tmpAtm=argCptr->value;
				compDef* tmpDef=addCompDef(tmpEnv,tmpAtm->value.str);
				*(int32_t*)(popVar->code+sizeof(char))=tmpDef->count;
				byteCode* beFree=tmpRawProc->proc;
				tmpRawProc->proc=codeCat(tmpRawProc->proc,popVar);
				freeByteCode(beFree);
				argCptr=nextCptr(argCptr);
			}
			objCptr=nextCptr(nextCptr(objCptr));
			continue;
		}
		else
		{
			byteCode* tmp1=compile(objCptr,tmpEnv,inter,status);
			if(status->status!=0)
			{
				freeByteCode(tmp);
				freeByteCode(initProc);
				freeByteCode(pushProc);
				freeByteCode(popVar);
				freeByteCode(proc);
				freeByteCode(pop);
				inter->procs=prevRawProc;
				while(tmpRawProc!=prevRawProc)
				{
					rawproc* prev=tmpRawProc;
					tmpRawProc=tmpRawProc->next;
					freeByteCode(prev->proc);
					free(prev);
				}
				return NULL;
			}
			if(nextCptr(objCptr)!=NULL)
			{
				byteCode* beFree=tmp1;
				tmp1=codeCat(tmp1,pop);
				freeByteCode(beFree);
			}
			byteCode* beFree=tmpRawProc->proc;
			tmpRawProc->proc=codeCat(tmpRawProc->proc,tmp1);
			freeByteCode(tmp1);
			freeByteCode(beFree);
			objCptr=nextCptr(objCptr);
		}
	}
	freeByteCode(initProc);
	freeByteCode(pushProc);
	freeByteCode(popVar);
	freeByteCode(proc);
	freeByteCode(pop);
	return tmp;
}

byteCode* compileCond(cptr* objCptr,compEnv* curEnv,intpr* inter,errorStatus* status)
{
	cptr* cond=NULL;
	byteCode* pushnil=createByteCode(sizeof(char));
	byteCode* jumpiffalse=createByteCode(sizeof(char)+sizeof(int32_t));
	byteCode* jump=createByteCode(sizeof(char)+sizeof(int32_t));
	byteCode* pop=createByteCode(sizeof(char));
	byteCode* tmp=createByteCode(0);
	byteCode* tmpCond=createByteCode(0);
	pop->code[0]=FAKE_POP;
	pushnil->code[0]=FAKE_PUSH_NIL;
	jumpiffalse->code[0]=FAKE_JMP_IF_FALSE;
	jump->code[0]=FAKE_JMP;
	for(cond=&((pair*)objCptr->value)->car;nextCptr(cond)!=NULL;cond=nextCptr(cond));
	while(prevCptr(cond)!=NULL)
	{
		for(objCptr=&((pair*)cond->value)->car;nextCptr(objCptr)!=NULL;objCptr=nextCptr(objCptr));
		while(objCptr!=NULL)
		{
			byteCode* tmp1=compile(objCptr,curEnv,inter,status);
			if(status->status!=0)
			{
				freeByteCode(tmp);
				freeByteCode(jumpiffalse);
				freeByteCode(jump);
				freeByteCode(pop);
				freeByteCode(tmpCond);
				return NULL;
			}
			if(prevCptr(objCptr)==NULL)
			{
				*(int32_t*)(jumpiffalse->code+sizeof(char))=tmpCond->size+pop->size+jump->size;
				byteCode* tmp2=codeCat(jumpiffalse,pop);
				byteCode* beFree=tmp1;
				tmp1=codeCat(tmp1,tmp2);
				freeByteCode(beFree);
				freeByteCode(tmp2);
			}
			if(prevCptr(objCptr)!=NULL&&nextCptr(objCptr)!=NULL)
			{
				byteCode* beFree=tmp1;
				tmp1=codeCat(tmp1,pop);
				freeByteCode(beFree);
			}
			byteCode* beFree=tmpCond;
			tmpCond=codeCat(tmp1,tmpCond);
			freeByteCode(beFree);
			objCptr=prevCptr(objCptr);
		}
		*(int32_t*)(jump->code+sizeof(char))=tmp->size+pushnil->size;
		byteCode* beFree=tmpCond;
		tmpCond=codeCat(tmpCond,jump);
		freeByteCode(beFree);
		beFree=tmp;
		tmp=codeCat(tmpCond,tmp);
		freeByteCode(beFree);
		freeByteCode(tmpCond);
		cond=prevCptr(cond);
	}
	byteCode* beFree=tmp;
	tmp=codeCat(tmp,pushnil);
	freeByteCode(beFree);
	freeByteCode(pushnil);
	freeByteCode(pop);
	freeByteCode(jumpiffalse);
	freeByteCode(jump);
	return tmp;
}

void printByteCode(const byteCode* tmpCode)
{
	int i=0;
	while(i<tmpCode->size)
	{
		int tmplen=0;
		printf("%s ",codeName[tmpCode->code[i]].codeName);
		switch(codeName[tmpCode->code[i]].len)
		{
			case -1:
				tmplen=strlen(tmpCode->code+i+1);
				printf("%s",tmpCode->code+i+1);
				i+=tmplen+2;
				break;
			case 0:
				i+=1;
				break;
			case 1:
				printf("%c",tmpCode->code[i+1]);
				i+=2;
				break;
			case 4:
				printf("%d",*(int32_t*)(tmpCode->code+i+1));
				i+=5;
				break;
			case 8:
				printf("%lf",*(double*)(tmpCode->code+i+1));
				i+=9;
				break;
		}
		putchar('\n');
	}
}
