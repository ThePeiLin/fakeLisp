#include<stdlib.h>
#include<string.h>
#include"tool.h"
#include"syntax.h"
#include"fake.h"
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
			if(first->type!=SYM||second->type!=SYM||strcmp(first->value,"define"))return 0;
		}
	}
	else return 0;
	return 1;
}

int isSymbol(const cptr* objCptr)
{
	atom* tmpAtm=(objCptr->type==ATM)?objCptr->value:NULL;
	if(tmpAtm==NULL||tmpAtm->type!=SYM)return 0;
	return 1;
}

int isConstant(const cptr* objCptr)
{
	if(objCptr->type==NIL)return 1;
	atom* tmpAtm=(objCptr->type==ATM)?objCptr->value:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type==SYM)return 0;
	if(isQuoteExpression(objCptr))return 1;
}

int isAndExpression(const cptr* objCptr)
{
	if(!isLegal(objCptr))return 0;
	objCptr=(objCptr->type==PAR)?&((pair*)objCptr->value)->car:NULL;
	if(objCptr!=NULL)
	{
		atom* tmpAtm=(objCptr->type==ATM)?objCptr->value:NULL;
		if(tmpAtm!=NULL&&!strcmp(tmpAtm->value,"and"))return 1;
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
		if(tmpAtm!=NULL&&!strcmp(tmpAtm->value,"or"))return 1;
	}
	return 0;
}

int isQuoteExpression(const cptr* objCptr)
{
	objCptr=(isLegal(objCptr)&&objCptr->type==PAR)?&((pair*)objCptr->value)->car:NULL;
	atom* tmpAtm=(objCptr!=NULL&&objCptr->type==ATM)?objCptr->value:NULL;
	if(tmpAtm!=NULL&&!strcmp(tmpAtm->value,"quote"))return 1;
	return 0;
}

int isLambdaExpression(const cptr* objCptr)
{
	objCptr=(objCptr->type==PAR)?&((pair*)objCptr->value)->car:NULL;
	atom* tmpAtm=(objCptr!=NULL&&objCptr->type==ATM)?objCptr->value:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type==SYM&&!strcmp(tmpAtm->value,"lambda"))return 1;
	return 0;
}

int isFunctionCall(const cptr* objCptr)
{
	if(isLegal(objCptr)&&!hasKeyWord(objCptr))return 1;
	return 0;
}

int isLegal(const cptr* objCptr)
{
	const cptr* tmpCptr=objCptr;
	if(tmpCptr->type==PAR)
	{
		tmpCptr=&((pair*)tmpCptr->value)->car;
		cptr* tmp=nextCptr(objCptr);
		while(tmp!=NULL)
		{
			tmp=nextCptr(tmp);
			tmpCptr=nextCptr(tmpCptr);
		}
		if(objCptr->outer->cdr.type!=NIL)return 0;
		synRule* current=NULL;
		for(current=Head;current!=NULL;current=current->next)
			if(!current->check(objCptr))return 0;
	}
	return 1;
}

/*void initSyntax()
{
	addSynRule(isDefExpression);
	addSynRule(isSetqExpression);
	addSynRule(isLambdaExpression);
	addSynRule(isCondExpression);
	addSynRule(isConstant);
	addSynRule(isFunctionCall);
	addSynRule(isSymbol);
	addSynRule(isAndExpression);
	addSynRule(isOrExpression);
}*/
