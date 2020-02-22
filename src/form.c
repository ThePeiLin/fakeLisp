#include<stdlib.h>
#include"form.h"
#include"tool.h"

cell** dealArg(cell* argCell,int num)
{
	cell* tmpCell=(argCell->type==con)?argCell:NULL;
	cell** args=NULL;
	if(!(args=(cell**)malloc(num*sizeof(cell*))))errors(OUTOFMEMORY);
	int i;
	for(i=0;i<num;i++)
	{
		if(argCell->type==nil||argCell->type==atm)
			args[i]=createCell(NULL);
		else
		{
			args[i]=createCell(NULL);
			args[i]->type=((conspair*)argCell->value)->left.type;
			args[i]->value=((conspair*)argCell->value)->left.value;
			((conspair*)argCell->value)->left.type=nil;
			((conspair*)argCell->value)->left.value=NULL;
			argCell=&((conspair*)argCell->value)->right;
		}
	}
	if(tmpCell!=NULL)
	{
		cell* beDel=createCell(NULL);
		beDel->type=tmpCell->type;
		beDel->value=tmpCell->value;
		((conspair*)tmpCell->value)->prev=NULL;
		tmpCell->type=argCell->type;
		tmpCell->value=argCell->value;
		if(argCell->type==con)
			((conspair*)argCell->value)->prev=(conspair*)tmpCell->outer;
		else if(argCell->type==atm)
			((atom*)argCell->value)->prev=(conspair*)tmpCell->outer;
		argCell->type=nil;
		argCell->value=NULL;
		deleteCons(beDel);
		free(beDel);
	}
	return args;
}

void deleteArg(cell** args,int num)
{
	int i;
	for(i=0;i++;i<num)
		deleteCons(args[i]);
	free(args);
}

int N_quote(cell* objCell,env* curEnv)
{
	deleteCons(objCell);
	cell** args=dealArg(&((conspair*)objCell->outer)->right,1);
	replace(objCell,args[0]);
	deleteArg(args,1);	
	return 0;
}

int N_car(cell* objCell,env* curEnv)
{
	deleteCons(objCell);
	cell** args=dealArg(&((conspair*)objCell->outer)->right,1);
	eval(args[0],curEnv);
	if(args[0]->type==atm||args[0]->type==nil)return SYNTAXERROR;
	replace(objCell,&((conspair*)args[0]->value)->left);
	deleteArg(args,1);
	return 0;
}

int N_cdr(cell* objCell,env* curEnv)
{
	deleteCons(objCell);
	cell** args=dealArg(&((conspair*)objCell->outer)->right,1);
	eval(args[0],curEnv);
	if(args[0]->type==atm||args[0]->type==nil)return SYNTAXERROR;
	replace(objCell,&((conspair*)args[0]->value)->right);
	deleteArg(args,1);
	return 0;
}

int N_cons(cell* objCell,env* curEnv)
{
	deleteCons(objCell);
	cell** args=dealArg(&((conspair*)objCell->outer)->right,2);
	eval(args[0],curEnv);
	eval(args[0],curEnv);
	objCell->type=con;
	conspair* tmpCons=objCell->value=createCons((conspair*)objCell->outer);
	replace(&tmpCons->left,args[0]);
	replace(&tmpCons->right,args[1]);
	deleteArg(args,2);
	return 0;
}

int N_eq(cell* objCell,env* curEnv)
{
	deleteCons(objCell);
	cell** args=dealArg(&((conspair*)objCell->outer)->right,2);
	eval(args[0],curEnv);
	eval(args[1],curEnv);
	if(concmp(args[0],args[1])==0)
	{
		objCell->type=nil;
		objCell->value=NULL;
	}
	else
	{
		objCell->type=atm;
		objCell->value=createAtom(num,"1",objCell->outer);
	}
	deleteArg(args,2);
	return 0;
}

int N_atom(cell* objCell,env* curEnv)
{
	deleteCons(objCell);
	cell** args=dealArg(&((conspair*)objCell->outer)->right,1);
	eval(args[0],curEnv);
	if(args[0]->type!=con)
	{
		objCell->type=atm;
		objCell->value=createAtom(num,"1",objCell->outer);
	}
	else
	{
		conspair* tmpCons=objCell->value;
		if(tmpCons->left.type==nil&&tmpCons->right.type==nil)
		{
			objCell->type=atm;
			objCell->value=createAtom(num,"1",objCell->outer);
		}
		else
		{
			objCell->type=nil;
			objCell->value=NULL;
		}
	}
	deleteArg(args,1);
	return 0;
}

int N_null(cell* objCell,env* curEnv)
{
	deleteCons(objCell);
	cell** args=dealArg(&((conspair*)objCell->outer)->right,1);
	eval(args[0],curEnv);
	if(args[0]->type==nil)
	{
		objCell->type=atm;
		objCell->value=createAtom(num,"1",objCell->outer);
	}
	else if(args[0]->type==con)
	{
		conspair* tmpCons=args[0]->value;
		if(tmpCons->left.type==nil&&tmpCons->right.type==nil)
		{
			objCell->type=atm;
			objCell->value=createAtom(num,"1",objCell->outer);
		}
		else 
		{
			objCell->type=nil;
			objCell->value=NULL;
		}
	}
	else
	{
		objCell->type=nil;
		objCell->value=NULL;
	}
	deleteArg(args,1);
	return 0;
}
