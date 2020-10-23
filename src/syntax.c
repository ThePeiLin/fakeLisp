#include<stdlib.h>
#include<string.h>
#include"tool.h"
#include"syntax.h"
#include"preprocess.h"
static synRule* Head=NULL;

int (*checkAST(const Cptr* objCptr))(const Cptr*)
{
	synRule* current=NULL;
	for(current=Head;current!=NULL;current=current->next)
		if(current->check(objCptr))break;
	return current->check;
}

void addSynRule(int (*check)(const Cptr*))
{
	synRule* current=NULL;
	if(!(current=(synRule*)malloc(sizeof(synRule))))errors(OUTOFMEMORY,__FILE__,__LINE__);
	current->check=check;
	current->next=Head;
	Head=current;
}

int isPreprocess(const Cptr* objCptr)
{
	objCptr=(objCptr->type==PAIR)?&((ANS_pair*)objCptr->value)->car:NULL;
	if(objCptr!=NULL&&nextCptr(objCptr)!=NULL&&nextCptr(nextCptr(objCptr))!=NULL)
	{
		if(objCptr->type!=ATM||nextCptr(objCptr)->type!=PAIR)return 0;
		else
		{
			ANS_atom* tmpAtm=objCptr->value;
			if(tmpAtm->type!=SYM||strcmp(tmpAtm->value.str,"defmacro"))return 0;
		}
		return 1;
	}
	else return 0;
}

int isDefExpression(const Cptr* objCptr)
{
	objCptr=(objCptr->type==PAIR)?&((ANS_pair*)objCptr->value)->car:NULL;
	if(objCptr!=NULL&&nextCptr(objCptr)!=NULL&&nextCptr(nextCptr(objCptr))!=NULL)
	{
		if(objCptr->type!=ATM||nextCptr(objCptr)->type!=ATM)return 0;
		else
		{
			ANS_atom* first=objCptr->value;
			ANS_atom* second=nextCptr(objCptr)->value;
			if(first->type!=SYM||second->type!=SYM||strcmp(first->value.str,"define"))return 0;
		}
		return 1;
	}
	else return 0;
}

int isSetqExpression(const Cptr* objCptr)
{
	objCptr=(objCptr->type==PAIR)?&((ANS_pair*)objCptr->value)->car:NULL;
	if(objCptr!=NULL&&nextCptr(objCptr)!=NULL&&nextCptr(nextCptr(objCptr))!=NULL)
	{
		if(objCptr->type!=ATM||nextCptr(objCptr)->type!=ATM)return 0;
		else
		{
			ANS_atom* first=objCptr->value;
			ANS_atom* second=nextCptr(objCptr)->value;
			if(first->type!=SYM||second->type!=SYM||strcmp(first->value.str,"setq"))return 0;
		}
		return 1;
	}
	else return 0;
}

int isCondExpression(const Cptr* objCptr)
{
	objCptr=(objCptr->type==PAIR)?&((ANS_pair*)objCptr->value)->car:NULL;
	if(objCptr!=NULL)
	{
		if(objCptr->type==ATM)
		{
			ANS_atom* tmpAtm=objCptr->value;
			if(tmpAtm->type==SYM&&!strcmp(tmpAtm->value.str,"cond"))
			{
				for(objCptr=nextCptr(objCptr);objCptr!=NULL;objCptr=nextCptr(objCptr))
					if(!isLegal(objCptr)||objCptr->type!=PAIR)return 0;
				return 1;
			}
		}
	}
	return 0;
}

int isSymbol(const Cptr* objCptr)
{
	ANS_atom* tmpAtm=(objCptr->type==ATM)?objCptr->value:NULL;
	if(tmpAtm==NULL||tmpAtm->type!=SYM)return 0;
	return 1;
}

int isNil(const Cptr* objCptr)
{
	if(objCptr->type==NIL)return 1;
	else if(objCptr->type==PAIR)
	{
		ANS_pair* tmpPair=objCptr->value;
		if(tmpPair->car.type==NIL&&tmpPair->cdr.type==NIL)return 1;
	}
	return 0;
}

int isConst(const Cptr* objCptr)
{
	if(isNil(objCptr))return 1;
	ANS_atom* tmpAtm=(objCptr->type==ATM)?objCptr->value:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type!=SYM)return 1;
	if(objCptr->type==PAIR&&(isQuoteExpression(objCptr)||(((ANS_pair*)objCptr->value)->car.type==NIL&&((ANS_pair*)objCptr->value)->car.type==NIL)))return 1;
	return 0;
}

int isAndExpression(const Cptr* objCptr)
{
	if(!isLegal(objCptr))return 0;
	objCptr=(objCptr->type==PAIR)?&((ANS_pair*)objCptr->value)->car:NULL;
	if(objCptr!=NULL)
	{
		ANS_atom* tmpAtm=(objCptr->type==ATM)?objCptr->value:NULL;
		if(tmpAtm!=NULL&&tmpAtm->type==SYM&&!strcmp(tmpAtm->value.str,"and"))return 1;
	}
	return 0;
}

int isOrExpression(const Cptr* objCptr)
{
	if(!isLegal(objCptr))return 0;
	objCptr=(objCptr->type==PAIR)?&((ANS_pair*)objCptr->value)->car:NULL;
	if(objCptr!=NULL)
	{
		ANS_atom* tmpAtm=(objCptr->type==ATM)?objCptr->value:NULL;
		if(tmpAtm!=NULL&&tmpAtm->type==SYM&&!strcmp(tmpAtm->value.str,"or"))return 1;
	}
	return 0;
}

int isQuoteExpression(const Cptr* objCptr)
{
	objCptr=(isLegal(objCptr)&&objCptr->type==PAIR)?&((ANS_pair*)objCptr->value)->car:NULL;
	ANS_atom* tmpAtm=(objCptr!=NULL&&objCptr->type==ATM)?objCptr->value:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type==SYM&&!strcmp(tmpAtm->value.str,"quote"))return 1;
	return 0;
}

int isLambdaExpression(const Cptr* objCptr)
{
	objCptr=(objCptr->type==PAIR)?&((ANS_pair*)objCptr->value)->car:NULL;
	ANS_atom* tmpAtm=(objCptr!=NULL&&objCptr->type==ATM)?objCptr->value:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type==SYM&&!strcmp(tmpAtm->value.str,"lambda"))return 1;
	return 0;
}

int isListForm(const Cptr* objCptr)
{
	if(objCptr->type==PAIR&&isLegal(objCptr)&&!hasKeyWord(objCptr)&&!isNil(objCptr))return 1;
	return 0;
}

int isLegal(const Cptr* objCptr)
{
	const Cptr* tmpCptr=objCptr;
	if(objCptr->type==PAIR)
	{
		objCptr=&((ANS_pair*)objCptr->value)->car;
		const Cptr* prevCptr=NULL;
		while(objCptr!=NULL)
		{
			prevCptr=objCptr;
			objCptr=nextCptr(objCptr);
		}
		if(prevCptr->outer->cdr.type!=NIL)return 0;
		synRule* current=NULL;
		for(current=Head;current!=NULL;current=current->next)
			if(!current->check(tmpCptr))return 0;
	}
	return 1;
}

void initSyntax()
{
	addSynRule(isDefExpression);
	addSynRule(isSetqExpression);
	addSynRule(isLambdaExpression);
	addSynRule(isCondExpression);
	addSynRule(isConst);
	addSynRule(isListForm);
	addSynRule(isSymbol);
	addSynRule(isAndExpression);
	addSynRule(isOrExpression);
}
