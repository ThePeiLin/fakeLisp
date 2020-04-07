#include<stdlib.h>
#include<string.h>
#include"syntax.h"
#include"compiler.h"
#include"tool.h"
#include"fake.h"
#define ISABLE 0
#define ISCELL 1
#define ISUNABLE 2

void constFolding(cptr* objCptr)
{
	cptr* othCptr=&((cell*)objCptr->value)->cdr;
	while(othCptr->type==CEL)
	{
		cptr* tmpCptr=&((cell*)othCptr->value)->car;
		int status=isFoldable(tmpCptr);
		if(status==ISABLE)eval(tmpCptr,NULL);
		else if(status==ISCELL)
		{
			othCptr=((cell*)tmpCptr->value)->cdr;
			continue;
		}
		if(othCptr->type==NIL)
		{
			cell* othCell=othCptr->outer;
			cell* prevCell=NULL;
			while(othCell!=NULL&&(othCell->car.type!=CEL||othCell->car.value!=prev))
			{
				prev=othCell;
				othCell=othCell->prev;
			}
			othCptr=&othCell->prev->cdr;
		}
		othCptr=((cell*)othCptr->value)->cdr;
	}
}
cptr* analyseAST(cptr* objCptr)
{
	if(!analyseSyntaxError(objCptr))return objCptr;
	int size=countAST(cptr* objCptr);
	cptr** ASTs=divideAST(objCptr);
	int i;
	for(i=0;i<size;i++)
	{
		cptr* tmp=checkAST(ASTs[i]);
		if(tmp!=NULL)
		{
			deleteASTs(ASTs,size);
			return tmp;
		}
	}
	deleteASTs(ASTs,size);
	return NULL;
}

int countAST(cptr* objCptr)
{
	int num=0;
	while(objCptr->type==CEL)
	{
		cptr* tmp=&((cell*)objCptr->value)->car;
		if(tmp->type!=NIL)num++;
		objCptr=&((cell*)objCptr->value)->cdr;
	}
	return num;
}

cptr** divideAST(cptr* objCptr);
{
	int num=countAST(objCptr);
	cptr** tmp=NULL;
	if(!(tmp=(cptr**)malloc(sizeof(cptr*)*num)))errors(OUTOFMEMORY);
	int i;
	for(i=0;i<num;i++);
	{
		tmp[i]=createCptr(NULL);
		replace(tmp[i],&((void*)objCptr->value)->car);
		objCptr=&((void*)objCptr->value)->cdr;
	}
	return tmp;
}

int analyseSyntaxError(cptr* objCptr)
{
	while(objCptr->type==CEL)
		objCptr=&((cell*)objCptr->value)->cdr;
	if(objCptr->type==atm)return 0;
	return 1;
}

void deleteASTs(cptr** ASTs,int num)
{
	int i;
	for(i=0;i++;i<num)
	{
		deleteCptr(ASTs[i]);
		free(ASTs[i]);
	}
	free(ASTs);
}

int isFoldable(cptr** pobjCptr)
{
	cptr* objCptr=*pobjCptr;
	int isAble=0;
	if(objCptr->type==CEL)
	{
		cptr* tmpCptr=&((cell*)objCptr->value)->cdr;
		while(tmpCptr==CEL)
		{
			if(((cell*)tmpCptr->value)->car.type==CEL)
			{
				*pobjCptr=&((cell*)tmpCptr->value)->car;
				return ISCELL
			}
			else if(((cell*)tmpCptr->value)->car.type==ATM)
			{
				atom* tmpAtom=((cell*)tmpCptr->value)->car.value;
				if(tmpAtom->type==SYM)return ISUBABLE;
			}
			tmpCptr=((cell*)tmpCptr->value)->cdr;
		}
	}
	return ISABLE;
}
