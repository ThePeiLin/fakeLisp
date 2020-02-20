#include<stdlib.h>
#include"form.h"
#include"tool.h"

cell** dealArg(cell* argCell,int num)
{
	cell* tmpCell=(argCell->type==con)?argCell:NULL;
	cell* beDel=createCell(NULL);
	beDel->type=tmpCell->type;
	beDel->value=tmpCell->value;
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
		((conspair*)tmpCell->value)->prev=NULL;
		tmpCell->type=argCell->type;
		tmpCell->value=argCell->value;
		if(argCell->type==con)
			((conspair*)argCell->value)->prev=(conspair*)tmpCell->outer;
		else if(argCell->type==atm)
			((atom*)argCell->value)->prev=(conspair*)tmpCell->outer;
		argCell->type=nil;
		argCell->value=NULL;
	}
	deleteCons(beDel);
	free(beDel);
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
	conspair* objCons=(conspair*)objCell->outer;
	if(objCons==NULL)objCell->type=str;
	else
	{
		if(objCons->right.type==nil||objCons->right.type==atm)
			return SYNTAXERROR;
		else 
		{
			conspair* argCons=objCons->right.value;
			free(objCons->left.value);
			objCons->left.type=argCons->left.type;
			objCons->left.value=argCons->left.value;
			if(argCons->left.type==con)
				((conspair*)argCons->left.value)->prev=objCons;
			else if(argCons->left.type==atm)
				((atom*)argCons->left.value)->prev=objCons;
			objCons->right.type=argCons->right.type;
			objCons->right.value=argCons->right.value;
			if(argCons->right.type==con)
				((conspair*)argCons->right.value)->prev=objCons;
			else if(argCons->right.type==atm)
				((atom*)argCons->right.value)->prev=objCons;
			free(argCons);
		}
	}
	return 0;
}

int N_car(cell* objCell,env* curEnv)
{
	deleteCons(objCell);
	cell** args=dealArg(&((conspair*)objCell->outer)->right,1);
	eval(args[0],curEnv);
	if(args[0]->type==atm||args[0]->type==nil)return SYNTAXERROR;
	objCell->type=((conspair*)args[0]->value)->left.type;
	objCell->value=((conspair*)args[0]->value)->left.value;
	if(objCell->type==con)
		((conspair*)objCell->value)->prev=(conspair*)objCell->outer;
	else if(objCell->type==atm)
		((atom*)objCell->value)->prev=(conspair*)objCell->outer;
	((conspair*)args[0]->value)->left.type=nil;
	((conspair*)args[0]->value)->left.value=NULL;
	deleteArg(args,1);
}

int N_cdr(cell* objCell,env* curEnv)
{
	deleteCons(objCell);
	cell** args=dealArg(&((conspair*)objCell->outer)->right,1);
	eval(args[0],curEnv);
	if(args[0]->type==atm||args[0]->type==nil)return SYNTAXERROR;
	objCell->type=((conspair*)args[0]->value)->right.type;
	objCell->value=((conspair*)args[0]->value)->right.value;
	if(objCell->type==con)
		((conspair*)objCell->value)->prev=(conspair*)objCell->outer;
	else if(objCell->type==atm)
		((atom*)objCell->value)->prev=(conspair*)objCell->outer;
	((conspair*)args[0]->value)->right.type=nil;
	((conspair*)args[0]->value)->right.value=NULL;
	deleteArg(args,1);
	return 0;
}

int N_cons(cell* objCell,env* curEnv)
{
	deleteCons(objCell);
	cell** args=dealArg(&((conspair*)objCell->outer)->right,2);
	eval(args[0],curEnv);
	eval(args[1],curEnv);
	objCell->type=con;
	objCell->value=createCons((conspair*)objCell->outer);
	conspair* tmpCons=(conspair*)objCell->value;
	tmpCons->left.type=args[0]->type;
	tmpCons->left.value=args[0]->value;
	if(args[0]->type==con)
		((conspair*)args[0]->value)->prev=tmpCons;
	else if(args[0]->type==atm)
		((conspair*)args[0]->value)->prev=tmpCons;
	tmpCons->right.type=args[1]->type;
	tmpCons->right.value=args[1]->value;
	if(args[1]->type==con)
		((conspair*)args[1]->value)->prev=tmpCons;
	else if(args[1]->type==atm)
		((atom*)args[1]->value)->prev=tmpCons;
	args[0]->type=nil;
	args[0]->value=NULL;
	args[1]->type=nil;
	args[1]->value=NULL;
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
