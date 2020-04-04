#include<stdlib.h>
#include<string.h>
#include"syntax.h"
#include"compiler.h"
#include"tool.h"

cptr* analyseAST(cptr* objCptr)
{
	if(!analyseSyntaxError(objCptr))return objCptr;
	int size=countAST(cptr* objCptr);
	cptr** ASTs=divideAST(objCptr);
	int i;
	for(i=0;i<size;i++)
	{
		cptr* tmp=checkAST(ASTs[i]);
		if(tmp!=NULL)return tmp;;
	}
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
