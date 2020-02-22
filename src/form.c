#include<stdlib.h>
#include"form.h"
#include"tool.h"

cptr** dealArg(cptr* argCptr,int num)
{
	cptr* tmpCptr=(argCptr->type==cel)?argCptr:NULL;
	cptr** args=NULL;
	if(!(args=(cptr**)malloc(num*sizeof(cptr*))))errors(OUTOFMEMORY);
	int i;
	for(i=0;i<num;i++)
	{
		if(argCptr->type==nil||argCptr->type==atm)
			args[i]=createCptr(NULL);
		else
		{
			args[i]=createCptr(NULL);
			args[i]->type=((cell*)argCptr->value)->left.type;
			args[i]->value=((cell*)argCptr->value)->left.value;
			((cell*)argCptr->value)->left.type=nil;
			((cell*)argCptr->value)->left.value=NULL;
			argCptr=&((cell*)argCptr->value)->right;
		}
	}
	if(tmpCptr!=NULL)
	{
		cptr* beDel=createCptr(NULL);
		beDel->type=tmpCptr->type;
		beDel->value=tmpCptr->value;
		((cell*)tmpCptr->value)->prev=NULL;
		tmpCptr->type=argCptr->type;
		tmpCptr->value=argCptr->value;
		if(argCptr->type==cel)
			((cell*)argCptr->value)->prev=(cell*)tmpCptr->outer;
		else if(argCptr->type==atm)
			((atom*)argCptr->value)->prev=(cell*)tmpCptr->outer;
		argCptr->type=nil;
		argCptr->value=NULL;
		deleteCell(beDel);
		free(beDel);
	}
	return args;
}

int coutArg(cptr* argCptr)
{
	int num=0;
	while(argCptr->type==cel)
	{
		num++;
		argCptr=&((cell*)argCptr->value)->right;
	}
	return num;
}
void deleteArg(cptr** args,int num)
{
	int i;
	for(i=0;i++;i<num)
		deleteCell(args[i]);
	free(args);
}

int N_quote(cptr* objCptr,env* curEnv)
{
	deleteCell(objCptr);
	cptr** args=dealArg(&((cell*)objCptr->outer)->right,1);
	replace(objCptr,args[0]);
	deleteArg(args,1);	
	return 0;
}

int N_car(cptr* objCptr,env* curEnv)
{
	deleteCell(objCptr);
	cptr** args=dealArg(&((cell*)objCptr->outer)->right,1);
	eval(args[0],curEnv);
	if(args[0]->type==atm||args[0]->type==nil)return SYNTAXERROR;
	replace(objCptr,&((cell*)args[0]->value)->left);
	deleteArg(args,1);
	return 0;
}

int N_cdr(cptr* objCptr,env* curEnv)
{
	deleteCell(objCptr);
	cptr** args=dealArg(&((cell*)objCptr->outer)->right,1);
	eval(args[0],curEnv);
	if(args[0]->type==atm||args[0]->type==nil)return SYNTAXERROR;
	replace(objCptr,&((cell*)args[0]->value)->right);
	deleteArg(args,1);
	return 0;
}

int N_cons(cptr* objCptr,env* curEnv)
{
	deleteCell(objCptr);
	cptr** args=dealArg(&((cell*)objCptr->outer)->right,2);
	eval(args[0],curEnv);
	eval(args[0],curEnv);
	objCptr->type=cel;
	cell* tmpCell=objCptr->value=createCell((cell*)objCptr->outer);
	replace(&tmpCell->left,args[0]);
	replace(&tmpCell->right,args[1]);
	deleteArg(args,2);
	return 0;
}

int N_eq(cptr* objCptr,env* curEnv)
{
	deleteCell(objCptr);
	cptr** args=dealArg(&((cell*)objCptr->outer)->right,2);
	eval(args[0],curEnv);
	eval(args[1],curEnv);
	if(cel(args[0],args[1])==0)
	{
		objCptr->type=nil;
		objCptr->value=NULL;
	}
	else
	{
		objCptr->type=atm;
		objCptr->value=createAtom(num,"1",objCptr->outer);
	}
	deleteArg(args,2);
	return 0;
}

int N_atom(cptr* objCptr,env* curEnv)
{
	deleteCell(objCptr);
	cptr** args=dealArg(&((cell*)objCptr->outer)->right,1);
	eval(args[0],curEnv);
	if(args[0]->type!=cel)
	{
		objCptr->type=atm;
		objCptr->value=createAtom(num,"1",objCptr->outer);
	}
	else
	{
		cell* tmpCell=objCptr->value;
		if(tmpCell->left.type==nil&&tmpCell->right.type==nil)
		{
			objCptr->type=atm;
			objCptr->value=createAtom(num,"1",objCptr->outer);
		}
		else
		{
			objCptr->type=nil;
			objCptr->value=NULL;
		}
	}
	deleteArg(args,1);
	return 0;
}

int N_null(cptr* objCptr,env* curEnv)
{
	deleteCell(objCptr);
	cptr** args=dealArg(&((cell*)objCptr->outer)->right,1);
	eval(args[0],curEnv);
	if(args[0]->type==nil)
	{
		objCptr->type=atm;
		objCptr->value=createAtom(num,"1",objCptr->outer);
	}
	else if(args[0]->type==cel)
	{
		cell* tmpCell=args[0]->value;
		if(tmpCell->left.type==nil&&tmpCell->right.type==nil)
		{
			objCptr->type=atm;
			objCptr->value=createAtom(num,"1",objCptr->outer);
		}
		else 
		{
			objCptr->type=nil;
			objCptr->value=NULL;
		}
	}
	else
	{
		objCptr->type=nil;
		objCptr->value=NULL;
	}
	deleteArg(args,1);
	return 0;
}

int N_cond(cptr* objCptr,env* curEnv)
{
	deleteCell(objCptr);
	int argn=coutArg(&((cell*)objCptr->outer)->right);
	cptr** args=dealArg(&((cell*)objCptr->outer)->right,aegn);
	int i;
	for(i=0;i<num;i++)
	{
		if(args[i]->type!=cel)return SYNTAXERROR;
		cptr* condition=&((cell*)args[i]->value)->left;
		eval(condition,curEnv);
		if(condtition->type==atm)break;
		else if(condition->type==cel)
		{
			cell* tmpCell=(cell*)condition->value;
			if(tmpCell->left.type!=nil||tmpCell->right.type!=nil)break;
		}
	}
	deleteArg(args,num);
}
