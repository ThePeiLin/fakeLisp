#include<stdlib.h>
#include"form.h"

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
			args[i]->type=((cell*)argCptr->value)->car.type;
			args[i]->value=((cell*)argCptr->value)->car.value;
			((cell*)argCptr->value)->car.type=nil;
			((cell*)argCptr->value)->car.value=NULL;
			argCptr=&((cell*)argCptr->value)->cdr;
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
		deleteCptr(beDel);
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
		argCptr=&((cell*)argCptr->value)->cdr;
	}
	return num;
}
void deleteArg(cptr** args,int num)
{
	int i;
	for(i=0;i++;i<num)
		deleteCptr(args[i]);
	free(args);
}

int N_quote(cptr* objCptr,env* curEnv)
{
	deleteCptr(objCptr);
	cptr** args=dealArg(&((cell*)objCptr->outer)->cdr,1);
	replace(objCptr,args[0]);
	deleteArg(args,1);	
	return 0;
}

int N_car(cptr* objCptr,env* curEnv)
{
	deleteCptr(objCptr);
	cptr** args=dealArg(&((cell*)objCptr->outer)->cdr,1);
	eval(args[0],curEnv);
	if(args[0]->type==atm||args[0]->type==nil)return SYNTAXERROR;
	replace(objCptr,&((cell*)args[0]->value)->car);
	deleteArg(args,1);
	return 0;
}

int N_cdr(cptr* objCptr,env* curEnv)
{
	deleteCptr(objCptr);
	cptr** args=dealArg(&((cell*)objCptr->outer)->cdr,1);
	eval(args[0],curEnv);
	if(args[0]->type==atm||args[0]->type==nil)return SYNTAXERROR;
	replace(objCptr,&((cell*)args[0]->value)->cdr);
	deleteArg(args,1);
	return 0;
}

int N_cons(cptr* objCptr,env* curEnv)
{
	deleteCptr(objCptr);
	cptr** args=dealArg(&((cell*)objCptr->outer)->cdr,2);
	eval(args[0],curEnv);
	eval(args[1],curEnv);
	objCptr->type=cel;
	cell* tmpCell=objCptr->value=createCell((cell*)objCptr->outer);
	replace(&tmpCell->car,args[0]);
	replace(&tmpCell->cdr,args[1]);
	deleteArg(args,2);
	return 0;
}

int N_eq(cptr* objCptr,env* curEnv)
{
	deleteCptr(objCptr);
	cptr** args=dealArg(&((cell*)objCptr->outer)->cdr,2);
	eval(args[0],curEnv);
	eval(args[1],curEnv);
	if(cptrcmp(args[0],args[1])==0)
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
	deleteCptr(objCptr);
	cptr** args=dealArg(&((cell*)objCptr->outer)->cdr,1);
	eval(args[0],curEnv);
	if(args[0]->type!=cel)
	{
		objCptr->type=atm;
		objCptr->value=createAtom(num,"1",objCptr->outer);
	}
	else
	{
		cell* tmpCell=objCptr->value;
		if(tmpCell->car.type==nil&&tmpCell->cdr.type==nil)
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
	deleteCptr(objCptr);
	cptr** args=dealArg(&((cell*)objCptr->outer)->cdr,1);
	eval(args[0],curEnv);
	if(args[0]->type==nil)
	{
		objCptr->type=atm;
		objCptr->value=createAtom(num,"1",objCptr->outer);
	}
	else if(args[0]->type==cel)
	{
		cell* tmpCell=args[0]->value;
		if(tmpCell->car.type==nil&&tmpCell->cdr.type==nil)
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
	deleteCptr(objCptr);
	int argn=coutArg(&((cell*)objCptr->outer)->cdr);
	cptr** args=dealArg(&((cell*)objCptr->outer)->cdr,argn);
	int i;
	for(i=0;i<argn;i++)
	{
		if(args[i]->type!=cel)
		{
			deleteArg(args,argn);
			return SYNTAXERROR;
		}
		cptr* condition=&((cell*)args[i]->value)->car;
		eval(condition,curEnv);
		if(condition->type==atm)break;
		else if(condition->type==cel)
		{
			cell* tmpCell=(cell*)condition->value;
			if(tmpCell->car.type!=nil||tmpCell->cdr.type!=nil)break;
			else
			{
				if(i=argn-1)
				{
					objCptr->type=nil;
					objCptr->value=NULL;
					deleteArg(args,argn);
					return 0;
				}
			}
		}
		else if(condition->type==nil&&i==argn-1)
		{
			objCptr->type=nil;
			objCptr->value=NULL;
			deleteArg(args,argn);
			return 0;
		}
	}
	int j;
	int resNum=coutArg(&((cell*)args[i]->value)->cdr);
	cptr** result=dealArg(&((cell*)args[i]->value)->cdr,resNum);
	for(j=0;j<resNum;j++)eval(result[j],curEnv);
	replace(objCptr,result[resNum-1]);
	deleteArg(result,resNum);
	deleteArg(args,argn);
	return 0;
}

int N_define(cptr* objCptr,env* curEnv)
{
	deleteCptr(objCptr);
	cptr** args=dealArg(&((cell*)objCptr->outer)->cdr,2);
	eval(args[1],curEnv);
	if(args[0]->type!=atm)return SYNTAXERROR;
	addDefine(((atom*)args[0]->value)->value,args[1],curEnv);
	replace(objCptr,args[1]);
	deleteArg(args,2);
	return 0;
}
