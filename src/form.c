#include<stdlib.h>
#include"form.h"

cell** dealArg(cell* argCell,env* curEnv,int num)
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
			eval(&((conspair*)argCell->value)->left,curEnv);
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
	cell** args=dealArg(&((conspair*)objCell->outer)->right,curEnv,1);
	if(args[0]->type==atm||args[0]->type==nil)return 1;
	objCell->type=((conspair*)args[0]->value)->left.type;
	objCell->value=((conspair*)args[0]->value)->left.value;
	if(objCell->type==con)
		((conspair*)objCell->value)->prev=(conspair*)objCell->outer;
	else if(objCell->type==atm)
		((atom*)objCell->value)->prev=(conspair*)objCell->outer;
	args[0]->type=nil;
	args[0]->value=NULL;
	deleteArg(args,1);
}
