#include<stdlib.h>
#include<string.h>
#include"tool.h"
#include"syntax.h"
static synRule* Head=NULL;

cptr* checkAST(cptr* objCptr)
{
	synRule* objRule=NULL;
	objRule=findSynRule(((atom*)objCptr->value)->value);
	if(objRule!=NULL)
		return objRule->check(objCptr);
	return NULL;
}

void addSynRule(const char* name,int (*check)(cptr*))
{
	synRule* current=NULL;
	if(!(current=(synRule*)malloc(sizeof(synRule))))errors(OUTOFMEMORY);
	strcpy(current->tokenName,name);
	current->check=check;
	current->next=Head;
	Head=current;
}

synRule* findSynRule(const char* name)
{
	current=Head;
	while(current!=NULL&&strcmp(current->tokenName,name))current=current->next;
	return current;
}
