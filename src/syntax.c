#include<stdlib.h>
#include<string.h>
#include"tool.h"
#include"syntax.h"
#include"preprocess.h"
static synRule* Head=NULL;

int (*checkAST(const cptr* objCptr))(const cptr*)
{
	synRule* current=NULL;
	for(current=Head;current!=NULL;current=current->next)
		if(current->check(objCptr))break;
	return current->check;
}

void addSynRule(int (*check)(const cptr*))
{
	synRule* current=NULL;
	if(!(current=(synRule*)malloc(sizeof(synRule))))errors(OUTOFMEMORY);
	current->check=check;
	current->next=Head;
	Head=current;
}

int isPreprocess(const cptr* objCptr)
{
	objCptr=(objCptr->type==PAR)?&((pair*)objCptr->value)->car:NULL;
	if(objCptr!=NULL&&nextCptr(objCptr)!=NULL&&nextCptr(nextCptr(objCptr))!=NULL)
	{
		if(objCptr->type!=ATM||nextCptr(objCptr)->type!=PAR)return 0;
		else
		{
			atom* tmpAtm=objCptr->value;
			if(tmpAtm->type!=SYM||strcmp(tmpAtm->value.str,"defmacro"))return 0;
		}
		return 1;
	}
	else return 0;
}

int isDefExpression(const cptr* objCptr)
{
	objCptr=(objCptr->type==PAR)?&((pair*)objCptr->value)->car:NULL;
	if(objCptr!=NULL&&nextCptr(objCptr)!=NULL&&nextCptr(nextCptr(objCptr))!=NULL)
	{
		if(objCptr->type!=ATM||nextCptr(objCptr)->type!=ATM)return 0;
		else
		{
			atom* first=objCptr->value;
			atom* second=nextCptr(objCptr)->value;
			if(first->type!=SYM||second->type!=SYM||strcmp(first->value.str,"define"))return 0;
		}
		return 1;
	}
	else return 0;
}

int isSetqExpression(const cptr* objCptr)
{
	objCptr=(objCptr->type==PAR)?&((pair*)objCptr->value)->car:NULL;
	if(objCptr!=NULL&&nextCptr(objCptr)!=NULL&&nextCptr(nextCptr(objCptr))!=NULL)
	{
		if(objCptr->type!=ATM||nextCptr(objCptr)->type!=ATM)return 0;
		else
		{
			atom* first=objCptr->value;
			atom* second=nextCptr(objCptr)->value;
			if(first->type!=SYM||second->type!=SYM||strcmp(first->value.str,"setq"))return 0;
		}
		return 1;
	}
	else return 0;
}

int isCondExpression(const cptr* objCptr)
{
	objCptr=(objCptr->type==PAR)?&((pair*)objCptr->value)->car:NULL;
	if(objCptr!=NULL)
	{
		if(objCptr->type==ATM)
		{
			atom* tmpAtm=objCptr->value;
			if(tmpAtm->type==SYM&&!strcmp(tmpAtm->value.str,"cond"))
			{
				for(objCptr=nextCptr(objCptr);objCptr!=NULL;objCptr=nextCptr(objCptr))
					if(!isLegal(objCptr)||objCptr->type!=PAR)return 0;
				return 1;
			}
		}
	}
	return 0;
}

int isSymbol(const cptr* objCptr)
{
	atom* tmpAtm=(objCptr->type==ATM)?objCptr->value:NULL;
	if(tmpAtm==NULL||tmpAtm->type!=SYM)return 0;
	return 1;
}

int isNil(const cptr* objCptr)
{
	if(objCptr->type==NIL)return 1;
	else if(objCptr->type==PAR)
	{
		pair* tmpPair=objCptr->value;
		if(tmpPair->car.type==NIL&&tmpPair->cdr.type==NIL)return 1;
	}
	return 0;
}

int isConst(const cptr* objCptr)
{
	if(isNil(objCptr))return 1;
	atom* tmpAtm=(objCptr->type==ATM)?objCptr->value:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type!=SYM)return 1;
	if(objCptr->type==PAR&&(isQuoteExpression(objCptr)||(((pair*)objCptr->value)->car.type==NIL&&((pair*)objCptr->value)->car.type==NIL)))return 1;
	return 0;
}

int isAndExpression(const cptr* objCptr)
{
	if(!isLegal(objCptr))return 0;
	objCptr=(objCptr->type==PAR)?&((pair*)objCptr->value)->car:NULL;
	if(objCptr!=NULL)
	{
		atom* tmpAtm=(objCptr->type==ATM)?objCptr->value:NULL;
		if(tmpAtm!=NULL&&tmpAtm->type==SYM&&!strcmp(tmpAtm->value.str,"and"))return 1;
	}
	return 0;
}

int isOrExpression(const cptr* objCptr)
{
	if(!isLegal(objCptr))return 0;
	objCptr=(objCptr->type==PAR)?&((pair*)objCptr->value)->car:NULL;
	if(objCptr!=NULL)
	{
		atom* tmpAtm=(objCptr->type==ATM)?objCptr->value:NULL;
		if(tmpAtm!=NULL&&tmpAtm->type==SYM&&!strcmp(tmpAtm->value.str,"or"))return 1;
	}
	return 0;
}

int isQuoteExpression(const cptr* objCptr)
{
	objCptr=(isLegal(objCptr)&&objCptr->type==PAR)?&((pair*)objCptr->value)->car:NULL;
	atom* tmpAtm=(objCptr!=NULL&&objCptr->type==ATM)?objCptr->value:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type==SYM&&!strcmp(tmpAtm->value.str,"quote"))return 1;
	return 0;
}

int isLambdaExpression(const cptr* objCptr)
{
	objCptr=(objCptr->type==PAR)?&((pair*)objCptr->value)->car:NULL;
	atom* tmpAtm=(objCptr!=NULL&&objCptr->type==ATM)?objCptr->value:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type==SYM&&!strcmp(tmpAtm->value.str,"lambda"))return 1;
	return 0;
}

int isListForm(const cptr* objCptr)
{
	if(objCptr->type==PAR&&isLegal(objCptr)&&!hasKeyWord(objCptr)&&!isNil(objCptr))return 1;
	return 0;
}

int isLegal(const cptr* objCptr)
{
	const cptr* tmpCptr=objCptr;
	if(objCptr->type==PAR)
	{
		objCptr=&((pair*)objCptr->value)->car;
		const cptr* prevCptr=NULL;
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
