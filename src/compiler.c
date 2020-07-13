#include<stdlib.h>
#include<string.h>
#include"syntax.h"
#include"compiler.h"
#include"tool.h"
#include"fake.h"
#define ISABLE 0
#define ISPAIR 1
#define ISUNABLE 2

void constFolding(cptr* objCptr)
{
	cptr* othCptr=&((pair*)objCptr->value)->cdr;
	while(othCptr->type==PAR)
	{
		cptr* tmpCptr=&((pair*)othCptr->value)->car;
		int status=isFoldable(&tmpCptr);
		if(status==ISABLE)eval(tmpCptr,NULL);
		else if(status==ISPAIR)
		{
			othCptr=((pair*)tmpCptr->value)->cdr;
			continue;
		}
		if(othCptr->type==NIL)
		{
			pair* othCell=othCptr->outer;
			pair* prevCell=NULL;
			while(othCell!=NULL&&(othCell->car.type!=PAR||othCell->car.value!=prev))
			{
				prev=othCell;
				othCell=othCell->prev;
			}
			othCptr=&othCell->prev->cdr;
		}
		othCptr=((pair*)othCptr->value)->cdr;
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
	while(objCptr->type==PAR)
	{
		cptr* tmp=&((pair*)objCptr->value)->car;
		if(tmp->type!=NIL)num++;
		objCptr=&((pair*)objCptr->value)->cdr;
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
	while(objCptr->type==PAR)
		objCptr=&((pair*)objCptr->value)->cdr;
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
	if(objCptr->type==PAR)
	{
		cptr* tmpCptr=&((pair*)objCptr->value)->cdr;
		while(tmpCptr==PAR)
		{
			if(((pair*)tmpCptr->value)->car.type==PAR)
			{
				*pobjCptr=&((pair*)tmpCptr->value)->car;
				return ISPAIR
			}
			else if(((pair*)tmpCptr->value)->car.type==ATM)
			{
				atom* tmpAtom=((pair*)tmpCptr->value)->car.value;
				if(tmpAtom->type==SYM)return ISUBABLE;
			}
			tmpCptr=((pair*)tmpCptr->value)->cdr;
		}
	}
	return ISABLE;
}
