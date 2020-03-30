#include<stdlib.h>
#include"form.h"
#include"pretreatment.h"

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
			replace(args[i],&((cell*)argCptr->value)->car);
			deleteCptr(&((cell*)argCptr->value)->car);
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
			((cell*)argCptr->value)->prev=tmpCptr->outer;
		else if(argCptr->type==atm)
			((atom*)argCptr->value)->prev=tmpCptr->outer;
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
		cptr* tmp=&((cell*)argCptr->value)->car;
		if(tmp->type!=nil)num++;
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

int isNil(cptr* objCptr)
{
	if(objCptr->type==nil)return 1;
	else if(objCptr->type==cel)
	{
		cell* tmpCell=objCptr->value;
		if(tmpCell->car.type==nil&&tmpCell->cdr.type==nil)return 1;
	}
	return 0;
}

int N_quote(cptr* objCptr,env* curEnv)
{
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,1);
	replace(objCptr,args[0]);
	deleteArg(args,1);	
	return 0;
}

int N_car(cptr* objCptr,env* curEnv)
{
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,1);
	int status=eval(args[0],curEnv);
	if(status!=0)
	{
		deleteArg(args,1);
		return status;
	}
	if(args[0]->type==atm||args[0]->type==nil)
	{
		deleteArg(args,1);
		return SYNTAXERROR;
	}
	replace(objCptr,&((cell*)args[0]->value)->car);
	deleteArg(args,1);
	return 0;
}

int N_cdr(cptr* objCptr,env* curEnv)
{
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,1);
	int status=eval(args[0],curEnv);
	if(status!=0)
	{
		deleteArg(args,1);
		return status;
	}
	if(args[0]->type==atm||args[0]->type==nil)
	{
		deleteArg(args,1);
		return SYNTAXERROR;
	}
	replace(objCptr,&((cell*)args[0]->value)->cdr);
	deleteArg(args,1);
	return 0;
}

int N_cons(cptr* objCptr,env* curEnv)
{
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,2);
	int status=eval(args[0],curEnv);
	if(status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	status=eval(args[1],curEnv);
	if(status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	objCptr->type=cel;
	cell* tmpCell=objCptr->value=createCell(objCptr->outer);
	replace(&tmpCell->car,args[0]);
	replace(&tmpCell->cdr,args[1]);
	deleteArg(args,2);
	return 0;
}

int N_eq(cptr* objCptr,env* curEnv)
{
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,2);
	int status=eval(args[0],curEnv);
	if(status!=0)
	{
		deleteArg(args,2);
		return SYNTAXERROR;
	}
	status=eval(args[1],curEnv);
	if(status!=0)
	{
		deleteArg(args,2);
		return SYNTAXERROR;
	}
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
	cptr** args=dealArg(&objCptr->outer->cdr,1);
	int status=eval(args[0],curEnv);
	if(status!=0)
	{
		deleteArg(args,1);
		return SYNTAXERROR;
	}
	if(args[0]->type!=cel)
	{
		objCptr->type=atm;
		objCptr->value=createAtom(num,"1",objCptr->outer);
	}
	else
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
	deleteArg(args,1);
	return 0;
}

int N_null(cptr* objCptr,env* curEnv)
{
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,1);
	int status=eval(args[0],curEnv);
	if(status!=0)
	{
		deleteArg(args,1);
		return status;
	}
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

int N_and(cptr* objCptr,env* curEnv)
{
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,2);
	int status=eval(args[0],curEnv);
	if(status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	status=eval(args[1],curEnv);
	if(status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	if(isNil(args[0])||isNil(args[1]))
	{
		objCptr->type=nil;
		objCptr->value=NULL;
	}
	else
	{
		objCptr->type=atm;
		objCptr->value=createAtom(num,"1",objCptr->outer);
	}
	deleteArg(args,1);
	return 0;
}

int N_or(cptr* objCptr,env* curEnv)
{
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,2);
	int status=eval(args[0],curEnv);
	if(status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	status=eval(args[1],curEnv);
	if(status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	if(isNil(args[0])&&isNil(args[1]))
	{
		objCptr->type=nil;
		objCptr->value=NULL;
	}
	else
	{
		objCptr->type=atm;
		objCptr->value=createAtom(num,"1",objCptr->outer);
	}
	deleteArg(args,1);
	return 0;
}

int N_not(cptr* objCptr,env* curEnv)
{
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,1);
	int status=eval(args[0],curEnv);
	if(status!=0)
	{
		deleteArg(args,1);
		return status;
	}
	if(isNil(args[0]))
	{
		objCptr->type=atm;
		objCptr->value=createAtom(num,"1",objCptr->outer);
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
		int status=eval(expression[0],curEnv);
		if(status!=0)
		{
			deleteArg(condition,condNum);
			deleteArg(expression,expNum);
			return status;
		}
		if(expression[0]->type==nil)
		{
			deleteArg(expression,expNum);
			continue;
		}
		int j;
		for(j=1;j<expNum;j++)
		{
			int status=eval(expression[j],curEnv);
			if(status!=0)
			{
				deleteArg(condition,condNum);
				deleteArg(expression,expNum);
				return status;
			}
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
	cptr** args=dealArg(&objCptr->outer->cdr,2);
	int status=eval(args[1],curEnv);
	if(status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	if(args[0]->type!=atm)
	{
		deleteArg(args,2);
		return SYNTAXERROR;
	}
	addDefine(((atom*)args[0]->value)->value,args[1],curEnv);
	replace(objCptr,args[1]);
	deleteArg(args,2);
	return 0;
}

int N_lambda(cptr* objCptr,env* curEnv)
{
	deleteCptr(objCptr);
	if(objCptr->outer->prev==NULL)return SYNTAXERROR;
	env* tmpEnv=newEnv(curEnv);
	int i;
	int expNum=coutArg(&objCptr->outer->cdr);
	cptr** expression=dealArg(&objCptr->outer->cdr,expNum);
	int fakeArgNum=coutArg(expression[0]);
	if(fakeArgNum!=0)
	{
		cptr** fakeArg=dealArg(expression[0],fakeArgNum);
		cptr** realArg=dealArg(&objCptr->outer->prev->cdr,fakeArgNum);
		for(i=0;i<fakeArgNum;i++)
		{
			int status=eval(realArg[i],curEnv);
			if(status!=0)
			{
				deleteArg(fakeArg,fakeArgNum);
				deleteArg(realArg,fakeArgNum);
				return status;
			}
		}
		for(i=0;i<fakeArgNum;i++)
		{
			atom* tmpAtm=NULL;
			if(fakeArg[i]->type==atm)
				tmpAtm=fakeArg[i]->value;
			else
			{
				deleteArg(fakeArg,fakeArgNum);
				deleteArg(realArg,fakeArgNum);
				return SYNTAXERROR;
			}
			addDefine(tmpAtm->value,realArg[i],tmpEnv);
		}
		deleteArg(realArg,fakeArgNum);
		deleteArg(fakeArg,fakeArgNum);
	}
	for(i=1;i<expNum;i++)
	{
		int status=eval(expression[i],tmpEnv);
		if(status!=0)
		{
			deleteArg(expression,expNum);
			return status;
		}
	}
	replace(objCptr,expression[expNum-1]);
	deleteArg(expression,expNum);
	destroyEnv(tmpEnv);
	return 0;
}

int N_list(cptr* objCptr,env* curEnv)
{
	deleteCptr(objCptr);
	int argNum=coutArg(&objCptr->outer->cdr);
	cptr** args=dealArg(&objCptr->outer->cdr,argNum);
	int i;
	cptr* result=createCptr(NULL);
	cptr* tmp=result;
	for(i=0;i<argNum;i++)
	{
		int status=eval(args[i],curEnv);
		if(status!=0)
		{
			deleteArg(args,argNum);
			deleteCptr(result);
			return status;
		}
		tmp->type=cel;
		tmp->value=createCell(tmp->outer);
		cell* tmpCell=tmp->value;
		replace(&tmpCell->car,args[i]);
		tmp=&tmpCell->cdr;
	}
	replace(objCptr,result);
	deleteCptr(result);
	deleteArg(args,argNum);
	return 0;
}

int N_defmacro(cptr* objCptr,env* curEnv)
{
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,2);
	if(args[0]->type!=cel)
	{
		deleteArg(args,2);
		return SYNTAXERROR;
	}
	cptr* pattern=args[0];
	cptr* express=args[1];
	addMacro(pattern,express);
	deleteArg(args,2);
	objCptr->type=nil;
	objCptr->value=NULL;
	return 0;
}

int N_undef(cptr* objCptr,env* curEnv)
{
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,1);
	deleteMacro(args[0]);
	deleteArg(args,1);
	objCptr->type=nil;
	objCptr->value=NULL;
	return 0;
}

int N_add(cptr* objCptr,env* curEnv)
{
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,2);
	int status=eval(args[0],curEnv);
	if(status!=0||args[0]->type!=atm)
	{
		deleteArg(args,2);
		return status;
	}
	status=eval(args[1],curEnv);
	if(status!=0||args[1]->type!=atm)
	{
		deleteArg(args,2);
		return status;
	}
	long result=stringToInt(((atom*)args[0]->value)->value)+stringToInt(((atom*)args[1]->value)->value);
	char* tmpStr=intToString(result);
	objCptr->type=atm;
	objCptr->value=createAtom(num,tmpStr,objCptr->outer);
	free(tmpStr);
	deleteArg(args,2);
	return 0;
}

int N_sub(cptr* objCptr,env* curEnv)
{
	cptr** args=dealArg(&objCptr->outer->cdr,2);
	int status=eval(args[0],curEnv);
	if(status!=0||args[0]->type!=atm)
	{
		deleteArg(args,2);
		return status;
	}
	status=eval(args[1],curEnv);
	if(status!=0||args[1]->type!=atm)
	{
		deleteArg(args,2);
		return status;
	}
	long result=stringToInt(((atom*)args[0]->value)->value)-stringToInt(((atom*)args[1]->value)->value);
	char* tmpStr=intToString(result);
	objCptr->type=atm;
	objCptr->value=createAtom(num,tmpStr,objCptr->outer);
	free(tmpStr);
	deleteArg(args,2);
	return 0;
}

int N_mul(cptr* objCptr,env* curEnv)
{
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,2);
	int status=eval(args[0],curEnv);
	if(status!=0||args[0]->type!=atm)
	{
		deleteArg(args,2);
		return status;
	}
	status=eval(args[1],curEnv);
	if(status!=0||args[1]->type!=atm)
	{
		deleteArg(args,2);
		return status;
	}
	long result=stringToInt(((atom*)args[0]->value)->value)*stringToInt(((atom*)args[1]->value)->value);
	char* tmpStr=intToString(result);
	objCptr->type=atm;
	objCptr->value=createAtom(num,tmpStr,objCptr->outer);
	free(tmpStr);
	deleteArg(args,2);
	return 0;
}

int N_div(cptr* objCptr,env* curEnv)
{
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,2);
	int status=eval(args[0],curEnv);
	if(status!=0||args[0]->type!=atm)
	{
		deleteArg(args,2);
		return status;
	}
	status=eval(args[1],curEnv);
	if(status!=0||args[1]->type!=atm)
	{
		deleteArg(args,2);
		return status;
	}
	long result=stringToInt(((atom*)args[0]->value)->value)/stringToInt(((atom*)args[1]->value)->value);
	char* tmpStr=intToString(result);
	objCptr->type=atm;
	objCptr->value=createAtom(num,tmpStr,objCptr->outer);
	free(tmpStr);
	deleteArg(args,2);
	return 0;
}

int N_mod(cptr* objCptr,env* curEnv)
{
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,2);
	int status=eval(args[0],curEnv);
	if(status!=0||args[0]->type!=atm)
	{
		deleteArg(args,2);
		return status;
	}
	status=eval(args[1],curEnv);
	if(status!=0||args[1]->type!=atm)
	{
		deleteArg(args,2);
		return status;
	}
	long result=stringToInt(((atom*)args[0]->value)->value)%stringToInt(((atom*)args[1]->value)->value);
	char* tmpStr=intToString(result);
	objCptr->type=atm;
	objCptr->value=createAtom(num,tmpStr,objCptr->outer);
	free(tmpStr);
	deleteArg(args,2);
	return 0;
}
