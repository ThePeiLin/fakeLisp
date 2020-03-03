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
		if(argCptr->type==nil)
			args[i]=createCptr(NULL);
		else if(argCptr->type==atm)
			return NULL;
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
	{
		deleteCptr(args[i]);
		free(args[i]);
	}
	free(args);
}

int N_quote(cptr* objCptr,env* curEnv)
{
	deleteCptr(objCptr);
	cptr** args=dealArg(&((cell*)objCptr->outer)->cdr,1);
	if(args==NULL)return SYNTAXERROR;
	replace(objCptr,args[0]);
	deleteArg(args,1);	
	return 0;
}

int N_car(cptr* objCptr,env* curEnv)
{
	deleteCptr(objCptr);
	cptr** args=dealArg(&((cell*)objCptr->outer)->cdr,1);
	if(args==NULL)return SYNTAXERROR;
	int status=eval(args[0],curEnv);
	if(status!=0)return status;
	if(args[0]->type==atm||args[0]->type==nil)return SYNTAXERROR;
	replace(objCptr,&((cell*)args[0]->value)->car);
	deleteArg(args,1);
	return 0;
}

int N_cdr(cptr* objCptr,env* curEnv)
{
	deleteCptr(objCptr);
	cptr** args=dealArg(&((cell*)objCptr->outer)->cdr,1);
	if(args==NULL)return SYNTAXERROR;
	int status=eval(args[0],curEnv);
	if(status!=0)return status;
	if(args[0]->type==atm||args[0]->type==nil)return SYNTAXERROR;
	replace(objCptr,&((cell*)args[0]->value)->cdr);
	deleteArg(args,1);
	return 0;
}

int N_cons(cptr* objCptr,env* curEnv)
{
	deleteCptr(objCptr);
	cptr** args=dealArg(&((cell*)objCptr->outer)->cdr,2);
	if(args==NULL)return SYNTAXERROR;
	int status=eval(args[0],curEnv);
	if(status!=0)return status;
	status=eval(args[1],curEnv);
	if(status!=0)return status;
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
	if(args==NULL)return SYNTAXERROR;
	int status=eval(args[0],curEnv);
	if(status!=0)return SYNTAXERROR;
	status=eval(args[1],curEnv);
	if(status!=0)return SYNTAXERROR;
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
	if(args==NULL)return SYNTAXERROR;
	int status=eval(args[0],curEnv);
	if(status!=0)return SYNTAXERROR;
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
	if(args==NULL)return SYNTAXERROR;
	int status=eval(args[0],curEnv);
	if(status!=0)return SYNTAXERROR;
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
	int condNum=coutArg(&objCptr->outer->cdr);
	cptr** condition=dealArg(&objCptr->outer->cdr,condNum);
	if(condition==NULL)return SYNTAXERROR;
	int i;
	for(i=0;i<condNum;i++)
	{
		if(condition[i]->type!=cel)
		{
			deleteArg(condition,condNum);
			return SYNTAXERROR;
		}
		int expNum=coutArg(condition[i]);
		cptr** expression=dealArg(condition[i],expNum);
		if(expression==NULL)return SYNTAXERROR;
		int status=eval(expression[0],curEnv);
		if(status!=0)return status;
		if(expression[0]->type==nil)
		{
			deleteArg(expression,expNum);
			continue;
		}
		int j;
		for(j=1;j<expNum;j++)
		{
			int status=eval(expression[j],curEnv);
			if(status!=0)return status;
		}
		replace(objCptr,expression[expNum-1]);
		deleteArg(expression,expNum);
		deleteArg(condition,condNum);
		return 0;
	}
	objCptr->type=nil;
	objCptr->value=NULL;
	return 0;
}

int N_define(cptr* objCptr,env* curEnv)
{
	deleteCptr(objCptr);
	cptr** args=dealArg(&((cell*)objCptr->outer)->cdr,2);
	if(args==NULL)return SYNTAXERROR;
	int status=eval(args[1],curEnv);
	if(status!=0)return SYNTAXERROR;
	if(args[0]->type!=atm)return SYNTAXERROR;
	addDefine(((atom*)args[0]->value)->value,args[1],curEnv);
	replace(objCptr,args[1]);
	deleteArg(args,2);
	return 0;
}

int N_lambda(cptr* objCptr,env* curEnv)
{
	deleteCptr(objCptr);
	int expNum=coutArg(&((cell*)objCptr->outer)->cdr);
	cptr** expression=dealArg(&((cell*)objCptr->outer)->cdr,expNum);
	if(expression==NULL)return SYNTAXERROR;
	int fakeArgNum=coutArg(expression[0]);
	cptr** fakeArg=dealArg(expression[0],fakeArgNum);
	if(fakeArg==NULL)return SYNTAXERROR;
	if(((cell*)objCptr->outer)->prev==NULL)return SYNTAXERROR;
	cptr** realArg=dealArg(&objCptr->outer->prev->cdr,fakeArgNum);
	if(realArg==NULL)return SYNTAXERROR;
	int i;
	for(i=0;i<fakeArgNum;i++)
	{
		int status=eval(realArg[i],curEnv);
		if(status!=0)return status;
	}
	env* tmpEnv=newEnv();
	for(i=0;i<fakeArgNum;i++)
	{
		atom* tmpAtm=NULL;
		if(fakeArg[i]->type==atm)
			tmpAtm=fakeArg[i]->value;
		else
			return SYNTAXERROR;
		addDefine(tmpAtm->value,realArg[i],tmpEnv);
	}
	for(i=1;i<expNum;i++)
	{
		int status=eval(expression[i],tmpEnv);
		if(status!=0)return status;
	}
	replace(objCptr,expression[expNum-1]);
	deleteArg(realArg,fakeArgNum);
	deleteArg(expression,expNum);
	destroyEnv(tmpEnv);
	return 0;
}
