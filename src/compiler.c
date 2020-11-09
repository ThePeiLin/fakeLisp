#include<stdlib.h>
#include<string.h>
#include"compiler.h"
#include"syntax.h"
#include"tool.h"
#include"preprocess.h"
#include"opcode.h"

ByteCode* compile(ANS_cptr* objCptr,CompEnv* curEnv,intpr* inter,ErrorStatus* status)
{
	for(;;)
	{
		if(isConst(objCptr))return compileConst(objCptr,curEnv,inter,status);
		if(isSymbol(objCptr))return compileSym(objCptr,curEnv,inter,status);
		if(isDefExpression(objCptr))return compileDef(objCptr,curEnv,inter,status);
		if(isSetqExpression(objCptr))return compileSetq(objCptr,curEnv,inter,status);
		if(isSetfExpression(objCptr))return compileSetf(objCptr,curEnv,inter,status);
		if(isCondExpression(objCptr))return compileCond(objCptr,curEnv,inter,status);
		if(isAndExpression(objCptr))return compileAnd(objCptr,curEnv,inter,status);
		if(isOrExpression(objCptr))return compileOr(objCptr,curEnv,inter,status);
		if(isLambdaExpression(objCptr))return compileLambda(objCptr,curEnv,inter,status);
		if(PreMacroExpand(objCptr))continue;
		else if(hasKeyWord(objCptr))
		{
			status->status=SYNTAXERROR;
			status->place=objCptr;
			return NULL;
		}
		else if(isListForm(objCptr))return compileListForm(objCptr,curEnv,inter,status);
	}
}

ByteCode* compileAtom(ANS_cptr* objCptr)
{
	ANS_atom* tmpAtm=objCptr->value;
	ByteCode* tmp=NULL;
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
		case BYTE:
			tmp=createByteCode(sizeof(char)+sizeof(int32_t)+tmpAtm->value.byte.size);
			tmp->code[0]=FAKE_PUSH_BYTE;
			*(int32_t*)(tmp->code+1)=tmpAtm->value.byte.size;
			memcpy(tmp->code+5,tmpAtm->value.byte.arry,tmpAtm->value.byte.size);
			break;
	}
	return tmp;
}

ByteCode* compileNil()
{
	ByteCode* tmp=createByteCode(1);
	tmp->code[0]=FAKE_PUSH_NIL;
	return tmp;
}

ByteCode* compilePair(ANS_cptr* objCptr)
{
	ByteCode* tmp=createByteCode(0);
	ByteCode* beFree=NULL;
	ANS_pair* objPair=objCptr->value;
	ANS_pair* tmpPair=objPair;
	ByteCode* popToCar=createByteCode(1);
	ByteCode* popToCdr=createByteCode(1);
	ByteCode* pushPair=createByteCode(1);
	popToCar->code[0]=FAKE_POP_CAR;
	popToCdr->code[0]=FAKE_POP_CDR;
	pushPair->code[0]=FAKE_PUSH_PAIR;
	while(objCptr!=NULL)
	{
		if(objCptr->type==PAIR)
		{
			beFree=tmp;
			tmp=codeCat(tmp,pushPair);
			freeByteCode(beFree);
			objPair=objCptr->value;
			objCptr=&objPair->car;
			continue;
		}
		else if(objCptr->type==ATM||objCptr->type==NIL)
		{
			beFree=tmp;
			ByteCode* tmp1=(objCptr->type==ATM)?compileAtom(objCptr):compileNil();
			ByteCode* tmp2=codeCat(tmp1,(objCptr==&objPair->car)?popToCar:popToCdr);
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
			ANS_pair* prev=NULL;
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

ByteCode* compileQuote(ANS_cptr* objCptr)
{
	objCptr=&((ANS_pair*)objCptr->value)->car;
	objCptr=nextCptr(objCptr);
	switch(objCptr->type)
	{
		case PAIR:return compilePair(objCptr);
		case ATM:return compileAtom(objCptr);
		case NIL:return compileNil();
	}
}

ByteCode* compileConst(ANS_cptr* objCptr,CompEnv* curEnv,intpr* inter,ErrorStatus* status)
{
		if(objCptr->type==ATM)return compileAtom(objCptr);
		if(isNil(objCptr))return compileNil();
		if(isQuoteExpression)return compileQuote(objCptr);
}

ByteCode* compileListForm(ANS_cptr* objCptr,CompEnv* curEnv,intpr* inter,ErrorStatus* status)
{
	ANS_cptr* headoflist=NULL;
	ANS_pair* tmpPair=objCptr->value;
	ByteCode* beFree=NULL;
	ByteCode* tmp1=NULL;
	ByteCode* tmp=createByteCode(0);
	ByteCode* setBp=createByteCode(1);
	ByteCode* invoke=createByteCode(1);
	setBp->code[0]=FAKE_SET_BP;
	invoke->code[0]=FAKE_INVOKE;
	for(;;)
	{
		if(objCptr==NULL)
		{
			beFree=tmp;
			tmp=codeCat(tmp,invoke);
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
			headoflist=&((ANS_pair*)objCptr->value)->car;
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
	freeByteCode(invoke);
	return tmp;
}

ByteCode* compileDef(ANS_cptr* tir,CompEnv* curEnv,intpr* inter,ErrorStatus* status)
{
	ANS_pair* tmpPair=tir->value;
	ANS_cptr* fir=NULL;
	ANS_cptr* sec=NULL;
	ANS_cptr* objCptr=NULL;
	ByteCode* tmp=createByteCode(0);
	ByteCode* beFree=NULL;
	ByteCode* tmp1=NULL;
	ByteCode* pushTop=createByteCode(sizeof(char));
	ByteCode* popVar=createByteCode(sizeof(char)+sizeof(int32_t));
	popVar->code[0]=FAKE_POP_VAR;
	pushTop->code[0]=FAKE_PUSH_TOP;
	while(isDefExpression(tir))
	{
		fir=&((ANS_pair*)tir->value)->car;
		sec=nextCptr(fir);
		tir=nextCptr(sec);
	}
	objCptr=tir;
	if(isLambdaExpression(objCptr))
	{
		for(;;)
		{
			ANS_atom* tmpAtm=sec->value;
			CompDef* tmpDef=addCompDef(curEnv,tmpAtm->value.str);
			*(int32_t*)(popVar->code+sizeof(char))=tmpDef->count;
			beFree=tmp;
			ByteCode* tmp2=codeCat(pushTop,popVar);
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
		tmp1=compile(objCptr,curEnv,inter,status);
		if(status->status!=0)
		{
			freeByteCode(tmp);
			freeByteCode(popVar);
			freeByteCode(pushTop);
			return NULL;
		}
	}
	else
	{
		tmp1=compile(objCptr,curEnv,inter,status);
		if(status->status!=0)
		{
			freeByteCode(tmp);
			freeByteCode(popVar);
			freeByteCode(pushTop);
			return NULL;
		}
		for(;;)
		{
			ANS_atom* tmpAtm=sec->value;
			CompDef* tmpDef=addCompDef(curEnv,tmpAtm->value.str);
			*(int32_t*)(popVar->code+sizeof(char))=tmpDef->count;
			beFree=tmp;
			ByteCode* tmp2=codeCat(pushTop,popVar);
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
	beFree=tmp;
	tmp=codeCat(tmp1,tmp);
	freeByteCode(beFree);
	freeByteCode(tmp1);
	freeByteCode(popVar);
	freeByteCode(pushTop);
	return tmp;
}

ByteCode* compileSetq(ANS_cptr* objCptr,CompEnv* curEnv,intpr* inter,ErrorStatus* status)
{
	ANS_pair* tmpPair=objCptr->value;
	ANS_cptr* fir=&((ANS_pair*)objCptr->value)->car;
	ANS_cptr* sec=nextCptr(fir);
	ANS_cptr* tir=nextCptr(sec);
	ByteCode* tmp=createByteCode(0);
	ByteCode* beFree=NULL;
	ByteCode* tmp1=NULL;
	ByteCode* pushTop=createByteCode(sizeof(char));
	ByteCode* popVar=createByteCode(sizeof(char)+sizeof(int32_t));
	popVar->code[0]=FAKE_POP_VAR;
	pushTop->code[0]=FAKE_PUSH_TOP;
	while(isSetqExpression(tir))
	{
		fir=&((ANS_pair*)tir->value)->car;
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
		ANS_atom* tmpAtm=sec->value;
		CompDef* tmpDef=NULL;
		CompEnv* tmpEnv=curEnv;
		while(tmpEnv!=NULL&&tmpDef==NULL)
		{
			tmpDef=findCompDef(tmpAtm->value.str,tmpEnv);
			tmpEnv=tmpEnv->prev;
		}
		if(tmpDef==NULL)
		{
			status->status=SYMUNDEFINE;
			status->place=sec;
			freeByteCode(tmp);
			freeByteCode(popVar);
			freeByteCode(pushTop);
			return NULL;
		}
		else
		{
			*(int32_t*)(popVar->code+sizeof(char))=tmpDef->count;
			ByteCode* tmp2=codeCat(pushTop,popVar);
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

ByteCode* compileSetf(ANS_cptr* objCptr,CompEnv* curEnv,intpr* inter,ErrorStatus* status)
{
	ANS_cptr* fir=&((ANS_pair*)objCptr->value)->car;
	ANS_cptr* sec=nextCptr(fir);
	ANS_cptr* tir=nextCptr(sec);
	ByteCode* tmp=createByteCode(0);
	ByteCode* beFree=NULL;
	ByteCode* popRef=createByteCode(sizeof(char));
	popRef->code[0]=FAKE_POP_REF;
	ByteCode* tmp1=NULL;
	tmp1=compile(sec,curEnv,inter,status);
	if(status->status!=0)
	{
		freeByteCode(tmp);
		freeByteCode(popRef);
		return NULL;
	}
	beFree=tmp;
	tmp=codeCat(tmp,tmp1);
	freeByteCode(beFree);
	ByteCode* tmp2=compile(tir,curEnv,inter,status);
	if(status->status!=0)
	{
		freeByteCode(tmp);
		freeByteCode(tmp1);
		freeByteCode(popRef);
		return NULL;
	}
	beFree=tmp;
	tmp=codeCat(tmp,tmp2);
	freeByteCode(beFree);
	beFree=tmp;
	tmp=codeCat(tmp,popRef);
	freeByteCode(beFree);
	freeByteCode(popRef);
	freeByteCode(tmp1);
	freeByteCode(tmp2);
	return tmp;
}

ByteCode* compileSym(ANS_cptr* objCptr,CompEnv* curEnv,intpr* inter,ErrorStatus* status)
{
	ByteCode* pushVar=createByteCode(sizeof(char)+sizeof(int32_t));
	pushVar->code[0]=FAKE_PUSH_VAR;
	ANS_atom* tmpAtm=objCptr->value;
	CompDef* tmpDef=NULL;
	CompEnv* tmpEnv=curEnv;
	while(tmpEnv!=NULL&&tmpDef==NULL)
	{
		tmpDef=findCompDef(tmpAtm->value.str,tmpEnv);
		tmpEnv=tmpEnv->prev;
	}
	if(tmpDef==NULL)
	{
		status->status=SYMUNDEFINE;
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

ByteCode* compileAnd(ANS_cptr* objCptr,CompEnv* curEnv,intpr* inter,ErrorStatus* status)
{
	ANS_pair* tmpPair=objCptr->value;
	ByteCode* jumpiffalse=createByteCode(sizeof(char)+sizeof(int32_t));
	ByteCode* push1=createByteCode(sizeof(char)+sizeof(int32_t));
	ByteCode* pop=createByteCode(sizeof(char));
	ByteCode* tmp=createByteCode(0);
	jumpiffalse->code[0]=FAKE_JMP_IF_FALSE;
	push1->code[0]=FAKE_PUSH_INT;
	pop->code[0]=FAKE_POP;
	*(int32_t*)(push1->code+sizeof(char))=1;
	for(objCptr=&((ANS_pair*)objCptr->value)->car;nextCptr(objCptr)!=NULL;objCptr=nextCptr(objCptr));
	for(;prevCptr(objCptr)!=NULL;objCptr=prevCptr(objCptr))
	{
		if(isDefExpression(objCptr))
		{
			status->status=ILLEGALEXPR;
			status->place=objCptr;
			freeByteCode(tmp);
			freeByteCode(jumpiffalse);
			freeByteCode(push1);
			return NULL;
		}
		ByteCode* tmp1=compile(objCptr,curEnv,inter,status);
		ByteCode* beFree=tmp1;
		if(status->status!=0)
		{
			freeByteCode(tmp);
			freeByteCode(jumpiffalse);
			freeByteCode(push1);
			return NULL;
		}
		*(int32_t*)(jumpiffalse->code+sizeof(char))=tmp->size;
		tmp1=codeCat(tmp1,jumpiffalse);
		freeByteCode(beFree);
		beFree=tmp1;
		tmp1=codeCat(pop,tmp1);
		freeByteCode(beFree);
		beFree=tmp;
		tmp=codeCat(tmp1,tmp);
		freeByteCode(beFree);
	}
	ByteCode* beFree=tmp;
	tmp=codeCat(push1,tmp);
	freeByteCode(beFree);
	freeByteCode(jumpiffalse);
	freeByteCode(push1);
	return tmp;
}

ByteCode* compileOr(ANS_cptr* objCptr,CompEnv* curEnv,intpr* inter,ErrorStatus* status)
{
	ANS_pair* tmpPair=objCptr->value;
	ByteCode* jumpifture=createByteCode(sizeof(char)+sizeof(int32_t));
	ByteCode* pushnil=createByteCode(sizeof(char));
	ByteCode* tmp=createByteCode(0);
	pushnil->code[0]=FAKE_PUSH_NIL;
	jumpifture->code[0]=FAKE_JMP_IF_TURE;
	while(objCptr!=NULL)
	{
		if(isOrExpression(objCptr))
		{
			for(objCptr=&((ANS_pair*)objCptr->value)->car;nextCptr(objCptr)!=NULL;objCptr=nextCptr(objCptr));
			continue;
		}
		else if(prevCptr(objCptr)!=NULL)
		{
			if(isDefExpression(objCptr))
			{
				status->status=ILLEGALEXPR;
				status->place=objCptr;
				freeByteCode(tmp);
				freeByteCode(jumpifture);
				freeByteCode(pushnil);
				return NULL;
			}
			ByteCode* beFree=tmp;
			ByteCode* tmp1=compile(objCptr,curEnv,inter,status);
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
			*(int32_t*)(jumpifture->code+sizeof(char))=tmp->size;
			tmp=codeCat(jumpifture,tmp);
			freeByteCode(beFree);
			objCptr=prevCptr(objCptr);
		}
		if(prevCptr(objCptr)==NULL)
		{
			ByteCode* beFree=tmp;
			tmp=codeCat(pushnil,tmp);
			freeByteCode(beFree);
			if(objCptr->outer==tmpPair)break;
			objCptr=prevCptr(&objCptr->outer->prev->car);		
		}	
	}
	freeByteCode(jumpifture);
	freeByteCode(pushnil);
	return tmp;
}

ByteCode* compileLambda(ANS_cptr* objCptr,CompEnv* curEnv,intpr* inter,ErrorStatus* status)
{
	ANS_cptr* tmpCptr=objCptr;
	ANS_pair* tmpPair=objCptr->value;
	ANS_pair* objPair=NULL;
	RawProc* tmpRawProc=NULL;
	RawProc* prevRawProc=inter->procs;
	ByteCode* tmp=createByteCode(0);
	ByteCode* proc=createByteCode(0);
	CompEnv* tmpEnv=curEnv;
	ByteCode* initProc=createByteCode(sizeof(char)+sizeof(int32_t));
	ByteCode* popVar=createByteCode(sizeof(char)+sizeof(int32_t));
	ByteCode* pushProc=createByteCode(sizeof(char)+sizeof(int32_t));
	ByteCode* endproc=createByteCode(sizeof(char));
	ByteCode* pop=createByteCode(sizeof(char));
	ByteCode* resBp=createByteCode(sizeof(char));
	ByteCode* popRestVar=createByteCode(sizeof(char)+sizeof(int32_t));
	endproc->code[0]=FAKE_END_PROC;
	initProc->code[0]=FAKE_INIT_PROC;
	popVar->code[0]=FAKE_POP_VAR;
	pushProc->code[0]=FAKE_PUSH_PROC;
	resBp->code[0]=FAKE_RES_BP;
	pop->code[0]=FAKE_POP;
	popRestVar->code[0]=FAKE_POP_REST_VAR;
	for(;;)
	{
		if(objCptr==NULL)
		{
			*(int32_t*)(pushProc->code+sizeof(char))=tmpRawProc->count;
			*(int32_t*)(initProc->code+sizeof(char))=(tmpEnv->symbols==NULL)?-1:tmpEnv->symbols->count;
			ByteCode* tmp1=codeCat(pushProc,initProc);
			if(tmpRawProc->next==prevRawProc)
			{
				ByteCode* beFree=tmp;
				tmp=codeCat(tmp,tmp1);
				freeByteCode(beFree);
				beFree=tmp;
			}
			else
			{
				ByteCode* beFree=tmpRawProc->next->proc;
				tmpRawProc->next->proc=codeCat(tmpRawProc->next->proc,tmp1);
				freeByteCode(beFree);
			}
			ByteCode* beFree=tmpRawProc->proc;
			tmpRawProc->proc=codeCat(tmpRawProc->proc,endproc);
			freeByteCode(beFree);
			freeByteCode(tmp1);
			CompEnv* prev=tmpEnv;
			tmpEnv=tmpEnv->prev;
			destroyCompEnv(prev);
			if(objPair!=tmpPair)
			{
				objPair=objPair->prev;
				objCptr=nextCptr(&objPair->car);
				while(objPair->prev!=NULL&&objPair->prev->cdr.value==objPair)objPair=objPair->prev;
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
			if(nextCptr(objCptr)->type==PAIR)
			{
				ANS_cptr* argCptr=&((ANS_pair*)nextCptr(objCptr)->value)->car;
				while(argCptr!=NULL&&argCptr->type!=NIL)
				{
					ANS_atom* tmpAtm=(argCptr->type==ATM)?argCptr->value:NULL;
					if(argCptr->type!=ATM||tmpAtm==NULL||tmpAtm->type!=SYM)
					{
						status->status=SYNTAXERROR;
						status->place=tmpCptr;
						freeByteCode(tmp);
						freeByteCode(initProc);
						freeByteCode(pushProc);
						freeByteCode(popVar);
						freeByteCode(proc);
						freeByteCode(pop);
						freeByteCode(popRestVar);
						freeByteCode(resBp);
						inter->procs=prevRawProc;
						while(tmpRawProc!=prevRawProc)
						{
							RawProc* prev=tmpRawProc;
							tmpRawProc=tmpRawProc->next;
							freeByteCode(prev->proc);
							free(prev);
						}
						return NULL;
					}
					CompDef* tmpDef=addCompDef(tmpEnv,tmpAtm->value.str);
					*(int32_t*)(popVar->code+sizeof(char))=tmpDef->count;
					ByteCode* beFree=tmpRawProc->proc;
					tmpRawProc->proc=codeCat(tmpRawProc->proc,popVar);
					freeByteCode(beFree);
					if(nextCptr(argCptr)==NULL&&argCptr->outer->cdr.type==ATM)
					{
						ANS_atom* tmpAtom1=(argCptr->outer->cdr.type==ATM)?argCptr->outer->cdr.value:NULL;
						if(tmpAtom1!=NULL&&tmpAtom1->type!=SYM)
						{
							status->status=SYNTAXERROR;
							status->place=tmpCptr;
							freeByteCode(tmp);
							freeByteCode(initProc);
							freeByteCode(pushProc);
							freeByteCode(popVar);
							freeByteCode(proc);
							freeByteCode(pop);
							freeByteCode(popRestVar);
							freeByteCode(resBp);
							inter->procs=prevRawProc;
							while(tmpRawProc!=prevRawProc)
							{
								RawProc* prev=tmpRawProc;
								tmpRawProc=tmpRawProc->next;
								freeByteCode(prev->proc);
								free(prev);
							}
							return NULL;
						}
						tmpDef=addCompDef(tmpEnv,tmpAtom1->value.str);
						*(int32_t*)(popRestVar->code+sizeof(char))=tmpDef->count;
						ByteCode* beFree=tmpRawProc->proc;
						tmpRawProc->proc=codeCat(tmpRawProc->proc,popRestVar);
						freeByteCode(beFree);
					}
					argCptr=nextCptr(argCptr);
				}
			}
			else if(nextCptr(objCptr)->type==ATM)
			{
				ANS_atom* tmpAtm=nextCptr(objCptr)->value;
				if(tmpAtm->type!=SYM)
				{
					status->status=SYNTAXERROR;
					status->place=tmpCptr;
					freeByteCode(tmp);
					freeByteCode(initProc);
					freeByteCode(pushProc);
					freeByteCode(popVar);
					freeByteCode(proc);
					freeByteCode(pop);
					freeByteCode(popRestVar);
					freeByteCode(resBp);
					inter->procs=prevRawProc;
					while(tmpRawProc!=prevRawProc)
					{
						RawProc* prev=tmpRawProc;
						tmpRawProc=tmpRawProc->next;
						freeByteCode(prev->proc);
						free(prev);
					}
					return NULL;
				}
				CompDef* tmpDef=addCompDef(tmpEnv,tmpAtm->value.str);
				*(int32_t*)(popRestVar->code+sizeof(char))=tmpDef->count;
				ByteCode* beFree=tmpRawProc->proc;
				tmpRawProc->proc=codeCat(tmpRawProc->proc,popRestVar);
				freeByteCode(beFree);
			}
			ByteCode *beFree=tmpRawProc->proc;
			tmpRawProc->proc=codeCat(tmpRawProc->proc,resBp);
			freeByteCode(beFree);
			objCptr=nextCptr(nextCptr(objCptr));
			continue;
		}
		else
		{
			ByteCode* tmp1=compile(objCptr,tmpEnv,inter,status);
			if(status->status!=0)
			{
				freeByteCode(tmp);
				freeByteCode(initProc);
				freeByteCode(pushProc);
				freeByteCode(popVar);
				freeByteCode(proc);
				freeByteCode(pop);
				freeByteCode(popRestVar);
				freeByteCode(resBp);
				inter->procs=prevRawProc;
				while(tmpRawProc!=prevRawProc)
				{
					RawProc* prev=tmpRawProc;
					tmpRawProc=tmpRawProc->next;
					freeByteCode(prev->proc);
					free(prev);
				}
				return NULL;
			}
			if(nextCptr(objCptr)!=NULL)
			{
				ByteCode* beFree=tmp1;
				tmp1=codeCat(tmp1,pop);
				freeByteCode(beFree);
			}
			ByteCode* beFree=tmpRawProc->proc;
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
	freeByteCode(popRestVar);
	freeByteCode(resBp);
	return tmp;
}

ByteCode* compileCond(ANS_cptr* objCptr,CompEnv* curEnv,intpr* inter,ErrorStatus* status)
{
	ANS_cptr* cond=NULL;
	ByteCode* pushnil=createByteCode(sizeof(char));
	ByteCode* jumpiffalse=createByteCode(sizeof(char)+sizeof(int32_t));
	ByteCode* jump=createByteCode(sizeof(char)+sizeof(int32_t));
	ByteCode* pop=createByteCode(sizeof(char));
	ByteCode* tmp=createByteCode(0);
	pop->code[0]=FAKE_POP;
	pushnil->code[0]=FAKE_PUSH_NIL;
	jumpiffalse->code[0]=FAKE_JMP_IF_FALSE;
	jump->code[0]=FAKE_JMP;
	for(cond=&((ANS_pair*)objCptr->value)->car;nextCptr(cond)!=NULL;cond=nextCptr(cond));
	while(prevCptr(cond)!=NULL)
	{
		for(objCptr=&((ANS_pair*)cond->value)->car;nextCptr(objCptr)!=NULL;objCptr=nextCptr(objCptr));
		ByteCode* tmpCond=createByteCode(0);
		while(objCptr!=NULL)
		{
			if(isDefExpression(objCptr))
			{
				status->status=ILLEGALEXPR;
				status->place=objCptr;
				freeByteCode(tmp);
				freeByteCode(jumpiffalse);
				freeByteCode(jump);
				freeByteCode(pop);
				freeByteCode(tmpCond);
				return NULL;
			}
			ByteCode* tmp1=compile(objCptr,curEnv,inter,status);
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
				*(int32_t*)(jumpiffalse->code+sizeof(char))=tmpCond->size+((nextCptr(cond)==NULL)?0:jump->size)+((objCptr->outer->cdr.type==NIL)?pushnil->size:0);
				ByteCode* beFree=tmp1;
				tmp1=codeCat(tmp1,jumpiffalse);
				freeByteCode(beFree);
				if(objCptr->outer->cdr.type==NIL)
				{
					beFree=tmp1;
					ByteCode* tmp2=codeCat(pop,pushnil);
					tmp1=codeCat(tmp1,tmp2);
					freeByteCode(beFree);
					freeByteCode(tmp2);
				}
			}
			if(prevCptr(objCptr)!=NULL)
			{
				ByteCode* beFree=tmp1;
				tmp1=codeCat(pop,tmp1);
				freeByteCode(beFree);
			}
			ByteCode* beFree=tmpCond;
			tmpCond=codeCat(tmp1,tmpCond);
			freeByteCode(beFree);
			objCptr=prevCptr(objCptr);
		}
		ByteCode* beFree=tmpCond;
		if(prevCptr(prevCptr(cond))!=NULL)
		{
			tmpCond=codeCat(pop,tmpCond);
			freeByteCode(beFree);
		}
		if(nextCptr(cond)!=NULL)
		{
			beFree=tmpCond;
			*(int32_t*)(jump->code+sizeof(char))=tmp->size;
			tmpCond=codeCat(tmpCond,jump);
			freeByteCode(beFree);
		}
		beFree=tmp;
		tmp=codeCat(tmpCond,tmp);
		freeByteCode(beFree);
		freeByteCode(tmpCond);
		cond=prevCptr(cond);
	}
	freeByteCode(pushnil);
	freeByteCode(pop);
	freeByteCode(jumpiffalse);
	freeByteCode(jump);
	return tmp;
}
