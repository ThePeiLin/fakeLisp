#include<stdlib.h>
#include"tool.h"
#include"fake.h"
#include"form.h"
#include"preprocess.h"

cptr** dealArg(cptr* argCptr,int num)
{
	cptr* tmpCptr=(argCptr->type==PAR)?argCptr:NULL;
	cptr** args=NULL;
	if(!(args=(cptr**)malloc(num*sizeof(cptr*))))errors(OUTOFMEMORY);
	int i;
	for(i=0;i<num;i++)
	{
		if(argCptr->type==NIL||argCptr->type==ATM)
			args[i]=createCptr(NULL);
		else
		{
			args[i]=createCptr(NULL);
			replace(args[i],&((pair*)argCptr->value)->car);
			deleteCptr(&((pair*)argCptr->value)->car);
			argCptr=&((pair*)argCptr->value)->cdr;
		}
	}
	if(tmpCptr!=NULL)
	{
		cptr* beDel=createCptr(NULL);
		beDel->type=tmpCptr->type;
		beDel->value=tmpCptr->value;
		((pair*)tmpCptr->value)->prev=NULL;
		tmpCptr->type=argCptr->type;
		tmpCptr->value=argCptr->value;
		if(argCptr->type==PAR)
			((pair*)argCptr->value)->prev=tmpCptr->outer;
		else if(argCptr->type==ATM)
			((atom*)argCptr->value)->prev=tmpCptr->outer;
		argCptr->type=NIL;
		argCptr->value=NULL;
		deleteCptr(beDel);
		free(beDel);
	}
	return args;
}

int coutArg(cptr* argCptr)
{
	int num=0;
	while(argCptr->type==PAR)
	{
		cptr* tmp=&((pair*)argCptr->value)->car;
		if(tmp->type!=NIL)num++;
		argCptr=&((pair*)argCptr->value)->cdr;
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
	if(objCptr->type==NIL)return 1;
	else if(objCptr->type==PAR)
	{
		pair* tmpPair=objCptr->value;
		if(tmpPair->car.type==NIL&&tmpPair->cdr.type==NIL)return 1;
	}
	return 0;
}

errorStatus N_quote(cptr* objCptr,env* curEnv)
{
	errorStatus status={0,NULL};
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,1);
	replace(objCptr,args[0]);
	deleteArg(args,1);
	return status;
}

errorStatus N_car(cptr* objCptr,env* curEnv)
{
	errorStatus status={0,NULL};
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,1);
	status=eval(args[0],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,1);
		return status;
	}
	if(args[0]->type==ATM||args[0]->type==NIL)
	{
		status.status=SYNTAXERROR;
		status.place=createCptr(NULL);
		replace(status.place,args[0]);
		deleteArg(args,1);
		return status;
	}
	replace(objCptr,&((pair*)args[0]->value)->car);
	deleteArg(args,1);
	return status;
}

errorStatus N_cdr(cptr* objCptr,env* curEnv)
{
	errorStatus status={0,NULL};
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,1);
	status=eval(args[0],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,1);
		return status;
	}
	if(args[0]->type==ATM||args[0]->type==NIL)
	{
		status.status=SYNTAXERROR;
		status.place=createCptr(NULL);
		replace(status.place,args[0]);
		deleteArg(args,1);
		return status;
	}
	replace(objCptr,&((pair*)args[0]->value)->cdr);
	deleteArg(args,1);
	return status;
}

errorStatus N_cons(cptr* objCptr,env* curEnv)
{
	errorStatus status={0,NULL};
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,2);
	status=eval(args[0],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	status=eval(args[1],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	objCptr->type=PAR;
	pair* tmpPair=objCptr->value=createPair(objCptr->outer);
	replace(&tmpPair->car,args[0]);
	replace(&tmpPair->cdr,args[1]);
	deleteArg(args,2);
	return status;
}

errorStatus N_eq(cptr* objCptr,env* curEnv)
{
	errorStatus status={0,NULL};
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,2);
	status=eval(args[0],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	status=eval(args[1],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	if(cptrcmp(args[0],args[1])==0)
	{
		objCptr->type=NIL;
		objCptr->value=NULL;
	}
	else
	{
		atom* tmpAtm=createAtom(INT,NULL,objCptr->outer);
		objCptr->type=ATM;
		objCptr->value=tmpAtm;
		tmpAtm->value.num=1;
	}
	deleteArg(args,2);
	return status;
}

errorStatus N_atom(cptr* objCptr,env* curEnv)
{
	errorStatus status={0,NULL};
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,1);
	status=eval(args[0],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,1);
		return status;
	}
	if(args[0]->type!=PAR)
	{
		atom* tmpAtm=createAtom(INT,NULL,objCptr->outer);
		objCptr->type=ATM;
		objCptr->value=tmpAtm;
		tmpAtm->value.num=1;
	}
	else
	{
		pair* tmpPair=args[0]->value;
		if(tmpPair->car.type==NIL&&tmpPair->cdr.type==NIL)
		{
			atom* tmpAtm=createAtom(INT,NULL,objCptr->outer);
			objCptr->type=ATM;
			objCptr->value=tmpAtm;
			tmpAtm->value.num=1;
		}
		else
		{
			objCptr->type=NIL;
			objCptr->value=NULL;
		}
	}
	deleteArg(args,1);
	return status;
}

errorStatus N_null(cptr* objCptr,env* curEnv)
{
	errorStatus status={0,NULL};
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,1);
	status=eval(args[0],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,1);
		return status;
	}
	if(args[0]->type==NIL)
	{
		atom* tmpAtm=createAtom(INT,NULL,objCptr->outer);
		objCptr->type=ATM;
		objCptr->value=tmpAtm;
		tmpAtm->value.num=1;
	}
	else if(args[0]->type==PAR)
	{
		pair* tmpPair=args[0]->value;
		if(tmpPair->car.type==NIL&&tmpPair->cdr.type==NIL)
		{
			atom* tmpAtm=createAtom(INT,NULL,objCptr->outer);
			objCptr->type=ATM;
			objCptr->value=tmpAtm;
			tmpAtm->value.num=1;
		}
		else 
		{
			objCptr->type=NIL;
			objCptr->value=NULL;
		}
	}
	else
	{
		objCptr->type=NIL;
		objCptr->value=NULL;
	}
	deleteArg(args,1);
	return status;
}

errorStatus N_and(cptr* objCptr,env* curEnv)
{
	errorStatus status={0,NULL};
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,2);
	status=eval(args[0],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	status=eval(args[1],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	if(isNil(args[0])||isNil(args[1]))
	{
		objCptr->type=NIL;
		objCptr->value=NULL;
	}
	else
	{
		atom* tmpAtm=createAtom(INT,NULL,objCptr->outer);
		objCptr->type=ATM;
		objCptr->value=tmpAtm;
		tmpAtm->value.num=1;
	}
	deleteArg(args,1);
	return status;
}

errorStatus N_or(cptr* objCptr,env* curEnv)
{
	errorStatus status={0,NULL};
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,2);
	status=eval(args[0],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	status=eval(args[1],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	if(isNil(args[0])&&isNil(args[1]))
	{
		objCptr->type=NIL;
		objCptr->value=NULL;
	}
	else
	{
		atom* tmpAtm=createAtom(INT,NULL,objCptr->outer);
		objCptr->type=ATM;
		objCptr->value=tmpAtm;
		tmpAtm->value.num=1;
	}
	deleteArg(args,1);
	return status;
}

errorStatus N_not(cptr* objCptr,env* curEnv)
{
	errorStatus status={0,NULL};
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,1);
	status=eval(args[0],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,1);
		return status;
	}
	if(isNil(args[0]))
	{
		atom* tmpAtm=createAtom(INT,NULL,objCptr->outer);
		objCptr->type=ATM;
		objCptr->value=tmpAtm;
		tmpAtm->value.num=1;
	}
	else
	{
		objCptr->type=NIL;
		objCptr->value=NULL;
	}
	deleteArg(args,1);
	return status;
}

errorStatus N_cond(cptr* objCptr,env* curEnv)
{
	errorStatus status={0,NULL};
	deleteCptr(objCptr);
	int condNum=coutArg(&objCptr->outer->cdr);
	cptr** condition=dealArg(&objCptr->outer->cdr,condNum);
	int i;
	for(i=0;i<condNum;i++)
	{
		if(condition[i]->type!=PAR)
		{
			status.status=SYNTAXERROR;
			status.place=createCptr(NULL);
			replace(status.place,condition[i]);
			deleteArg(condition,condNum);
			return status;
		}
		int expNum=coutArg(condition[i]);
		cptr** expression=dealArg(condition[i],expNum);
		status=eval(expression[0],curEnv);
		if(status.status!=0)
		{
			deleteArg(condition,condNum);
			deleteArg(expression,expNum);
			return status;
		}
		if(expression[0]->type==NIL)
		{
			deleteArg(expression,expNum);
			continue;
		}
		int j;
		for(j=1;j<expNum;j++)
		{
			status=eval(expression[j],curEnv);
			if(status.status!=0)
			{
				deleteArg(condition,condNum);
				deleteArg(expression,expNum);
				return status;
			}
		}
		replace(objCptr,expression[expNum-1]);
		deleteArg(expression,expNum);
		deleteArg(condition,condNum);
		return status;
	}
	objCptr->type=NIL;
	objCptr->value=NULL;
	return status;
}

errorStatus N_define(cptr* objCptr,env* curEnv)
{
	errorStatus status={0,NULL};
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,2);
	status=eval(args[1],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	if(args[0]->type!=ATM)
	{
		status.status=SYNTAXERROR;
		status.place=createCptr(NULL);
		replace(status.place,args[0]);
		deleteArg(args,2);
		return status;
	}
	atom* tmpAtm=args[0]->value;
	if(tmpAtm->type!=SYM)
	{
		status.status=SYNTAXERROR;
		status.place=createCptr(NULL);
		replace(status.place,args[0]);
		deleteArg(args,2);
		return status;
	}
	addDefine(tmpAtm->value.str,args[1],curEnv);
	replace(objCptr,args[1]);
	deleteArg(args,2);
	return status;
}

errorStatus N_lambda(cptr* objCptr,env* curEnv)
{
	errorStatus status={0,NULL};
	deleteCptr(objCptr);
	if(objCptr->outer->prev==NULL)
	{
		status.status=SYNTAXERROR;
		status.place=createCptr(NULL);
		return status;
	}
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
			errorStatus status=eval(realArg[i],curEnv);
			if(status.status!=0)
			{
				deleteArg(fakeArg,fakeArgNum);
				deleteArg(realArg,fakeArgNum);
				return status;
			}
		}
		for(i=0;i<fakeArgNum;i++)
		{
			atom* tmpAtm=NULL;
			if(fakeArg[i]->type==ATM&&((atom*)fakeArg[i]->value)->type==SYM)
				tmpAtm=fakeArg[i]->value;
			else
			{
				status.status=SYNTAXERROR;
				status.place=createCptr(NULL);
				replace(status.place,fakeArg[i]);
				deleteArg(fakeArg,fakeArgNum);
				deleteArg(realArg,fakeArgNum);
				return status;
			}
			addDefine(tmpAtm->value.str,realArg[i],tmpEnv);
		}
		deleteArg(realArg,fakeArgNum);
		deleteArg(fakeArg,fakeArgNum);
	}
	for(i=1;i<expNum;i++)
	{
		status=eval(expression[i],tmpEnv);
		if(status.status!=0)
		{
			deleteArg(expression,expNum);
			return status;
		}
	}
	replace(objCptr,expression[expNum-1]);
	deleteArg(expression,expNum);
	destroyEnv(tmpEnv);
	return status;
}

errorStatus N_list(cptr* objCptr,env* curEnv)
{
	errorStatus status={0,NULL};
	deleteCptr(objCptr);
	int argNum=coutArg(&objCptr->outer->cdr);
	cptr** args=dealArg(&objCptr->outer->cdr,argNum);
	int i;
	cptr* result=createCptr(NULL);
	cptr* tmp=result;
	for(i=0;i<argNum;i++)
	{
		status=eval(args[i],curEnv);
		if(status.status!=0)
		{
			deleteArg(args,argNum);
			deleteCptr(result);
			return status;
		}
		tmp->type=PAR;
		tmp->value=createPair(tmp->outer);
		pair* tmpPair=tmp->value;
		replace(&tmpPair->car,args[i]);
		tmp=&tmpPair->cdr;
	}
	replace(objCptr,result);
	deleteCptr(result);
	deleteArg(args,argNum);
	return status;
}

errorStatus N_defmacro(cptr* objCptr,env* curEnv)
{
	errorStatus status={0,NULL};
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,2);
	if(args[0]->type!=PAR)
	{
		status.status=SYNTAXERROR;
		status.place=createCptr(NULL);
		replace(status.place,args[0]);
		deleteArg(args,2);
		return status;
	}
	cptr* pattern=args[0];
	cptr* express=args[1];
	addMacro(pattern,express);
	deleteArg(args,2);
	objCptr->type=NIL;
	objCptr->value=NULL;
	return status;
}

errorStatus N_undef(cptr* objCptr,env* curEnv)
{
	errorStatus status={0,NULL};
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,1);
	deleteMacro(args[0]);
	deleteArg(args,1);
	objCptr->type=NIL;
	objCptr->value=NULL;
	return status;
}

errorStatus N_add(cptr* objCptr,env* curEnv)
{
	errorStatus status={0,NULL};
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,2);
	status=eval(args[0],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	if(args[0]->type!=ATM)
	{
		status.status=SYNTAXERROR;
		status.place=createCptr(NULL);
		replace(status.place,args[0]);
		deleteArg(args,2);
		return status;
	}
	status=eval(args[1],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	if(args[1]->type!=ATM)
	{
		status.status=SYNTAXERROR;
		status.place=createCptr(NULL);
		replace(status.place,args[1]);
		deleteArg(args,2);
		return status;
	}
	atom* atom1=args[0]->value;
	atom* atom2=args[1]->value;
	if(atom1->type!=DOU&&atom1->type!=INT)
	{
		status.status=SYNTAXERROR;
		status.place=createCptr(NULL);
		replace(status.place,args[0]);
		deleteArg(args,2);
		return status;
	}
	if(atom2->type!=DOU&&atom2->type!=INT)
	{
		status.status=SYNTAXERROR;
		status.place=createCptr(NULL);
		replace(status.place,args[1]);
		deleteArg(args,2);
		return status;
	}
	atom* tmpAtm=createAtom(0,NULL,objCptr->outer);
	if(atom1->type==DOU||atom2->type==DOU)
	{
		double result=((atom1->type==DOU)?atom1->value.dou:atom1->value.num)+((atom2->type==DOU)?atom2->value.dou:atom2->value.num);
		tmpAtm->type=DOU;
		tmpAtm->value.dou=result;
	}
	else if(atom1->type==INT&&atom2->type==INT)
	{
		int64_t result=atom1->value.num+atom2->value.num;
		tmpAtm->type=INT;
		tmpAtm->value.num=result;
	}
	objCptr->type=ATM;
	objCptr->value=tmpAtm;
	deleteArg(args,2);
	return status;
}

errorStatus N_sub(cptr* objCptr,env* curEnv)
{
	errorStatus status={0,NULL};
	cptr** args=dealArg(&objCptr->outer->cdr,2);
	status=eval(args[0],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	if(args[0]->type!=ATM)
	{
		status.status=SYNTAXERROR;
		status.place=createCptr(NULL);
		replace(status.place,args[0]);
		deleteArg(args,2);
		return status;
	}
	status=eval(args[1],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	if(args[1]->type!=ATM)
	{
		status.status=SYNTAXERROR;
		status.place=createCptr(NULL);
		replace(status.place,args[1]);
		deleteArg(args,2);
		return status;
	}
	atom* atom1=args[0]->value;
	atom* atom2=args[1]->value;
	if(atom1->type!=DOU&&atom1->type!=INT)
	{
		status.status=SYNTAXERROR;
		status.place=createCptr(NULL);
		replace(status.place,args[0]);
		deleteArg(args,2);
		return status;
	}
	if(atom2->type!=DOU&&atom2->type!=INT)
	{
		status.status=SYNTAXERROR;
		status.place=createCptr(NULL);
		replace(status.place,args[1]);
		deleteArg(args,2);
		return status;
	}
	atom* tmpAtm=createAtom(0,NULL,objCptr->outer);
	if(atom1->type==DOU||atom2->type==DOU)
	{
		double result=((atom1->type==DOU)?atom1->value.dou:atom1->value.num)-((atom2->type==DOU)?atom2->value.dou:atom2->value.num);
		tmpAtm->type=DOU;
		tmpAtm->value.dou=result;
	}
	else if(atom1->type==INT&&atom2->type==INT)
	{
		long result=atom1->value.num-atom2->value.num;
		tmpAtm->type=INT;
		tmpAtm->value.num=result;
	}
	objCptr->type=ATM;
	objCptr->value=tmpAtm;
	deleteArg(args,2);
	return status;
}

errorStatus N_mul(cptr* objCptr,env* curEnv)
{
	errorStatus status={0,NULL};
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,2);
	status=eval(args[0],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	if(args[0]->type!=ATM)
	{
		status.status=SYNTAXERROR;
		status.place=createCptr(NULL);
		replace(status.place,args[0]);
		deleteArg(args,2);
		return status;
	}
	status=eval(args[1],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	if(args[1]->type!=ATM)
	{
		status.status=SYNTAXERROR;
		status.place=createCptr(NULL);
		replace(status.place,args[1]);
		deleteArg(args,2);
		return status;
	}
	atom* atom1=args[0]->value;
	atom* atom2=args[1]->value;
	if(atom1->type!=DOU&&atom1->type!=INT)
	{
		status.status=SYNTAXERROR;
		status.place=createCptr(NULL);
		replace(status.place,args[0]);
		deleteArg(args,2);
		return status;
	}
	if(atom2->type!=DOU&&atom2->type!=INT)
	{
		status.status=SYNTAXERROR;
		status.place=createCptr(NULL);
		replace(status.place,args[1]);
		deleteArg(args,2);
		return status;
	}
	atom* tmpAtm=createAtom(0,NULL,objCptr->outer);
	if(atom1->type==DOU||atom2->type==DOU)
	{
		double result=((atom1->type==DOU)?atom1->value.dou:atom1->value.num)*((atom2->type==DOU)?atom2->value.dou:atom2->value.num);
		tmpAtm->type=DOU;
		tmpAtm->value.dou=result;
	}
	else if(atom1->type==INT&&atom2->type==INT)
	{
		int64_t result=atom1->value.num*atom2->value.num;
		tmpAtm->type=INT;
		tmpAtm->value.num=result;
	}
	objCptr->type=ATM;
	objCptr->value=tmpAtm;
	deleteArg(args,2);
	return status;
}

errorStatus N_div(cptr* objCptr,env* curEnv)
{
	errorStatus status={0,NULL};
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,2);
	status=eval(args[0],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	if(args[0]->type!=ATM)
	{
		status.status=SYNTAXERROR;
		status.place=createCptr(NULL);
		replace(status.place,args[0]);
		deleteArg(args,2);
		return status;
	}
	status=eval(args[1],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	if(args[1]->type!=ATM)
	{
		status.status=SYNTAXERROR;
		status.place=createCptr(NULL);
		replace(status.place,args[0]);
		deleteArg(args,2);
		return status;
	}
	atom* atom1=args[0]->value;
	atom* atom2=args[1]->value;
	if(atom1->type!=DOU&&atom1->type!=INT)
	{
		status.status=SYNTAXERROR;
		status.place=createCptr(NULL);
		replace(status.place,args[0]);
		deleteArg(args,2);
		return status;
	}
	if(atom2->type!=DOU&&atom2->type!=INT)
	{
		status.status=SYNTAXERROR;
		status.place=createCptr(NULL);
		replace(status.place,args[1]);
		deleteArg(args,2);
		return status;
	}
	atom* tmpAtm=createAtom(DOU,NULL,objCptr->outer);
	if(atom1->type==DOU||atom2->type==DOU)
	{
		double result=((atom1->type==DOU)?atom1->value.dou:atom1->value.num)/((atom2->type==DOU)?atom2->value.dou:atom2->value.num);
		tmpAtm->type=DOU;
		tmpAtm->value.dou=result;
	}
	objCptr->type=ATM;
	objCptr->value=tmpAtm;
	deleteArg(args,2);
	return status;
}

errorStatus N_mod(cptr* objCptr,env* curEnv)
{
	errorStatus status={0,NULL};
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,2);
	status=eval(args[0],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	if(args[0]->type!=ATM)
	{
		status.status=SYNTAXERROR;
		status.place=createCptr(NULL);
		replace(status.place,args[0]);
		deleteArg(args,2);
		return status;
	}
	status=eval(args[1],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	if(args[1]->type!=ATM)
	{
		status.status=SYNTAXERROR;
		status.place=createCptr(NULL);
		replace(status.place,args[1]);
		deleteArg(args,2);
		return status;
	}
	atom* atom1=args[0]->value;
	atom* atom2=args[1]->value;
	if(atom1->type!=DOU&&atom1->type!=INT)
	{
		status.status=SYNTAXERROR;
		status.place=createCptr(NULL);
		replace(status.place,args[0]);
		deleteArg(args,2);
		return status;
	}
	if(atom2->type!=DOU&&atom2->type!=INT)
	{
		status.status=SYNTAXERROR;
		status.place=createCptr(NULL);
		replace(status.place,args[1]);
		deleteArg(args,2);
		return status;
	}
	atom* tmpAtm=createAtom(INT,NULL,objCptr->outer);
	int64_t result=((atom1->type==DOU)?(int)atom1->value.dou:atom1->value.num)%((atom2->type==DOU)?(int)atom2->value.dou:atom2->value.num);
	tmpAtm->value.num=result;
	deleteArg(args,2);
	return status;
}

errorStatus N_print(cptr* objCptr,env* curEnv)
{
	errorStatus status={0,NULL};
	deleteCptr(objCptr);
	cptr** args=dealArg(&objCptr->outer->cdr,1);
	status=eval(args[0],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,1);
		return status;
	}
	if(args[0]->type!=ATM)
	{
		status.status=SYNTAXERROR;
		status.place=createCptr(NULL);
		replace(status.place,args[0]);
		deleteArg(args,1);
		return status;
	}
	atom* tmpAtom=args[0]->value;
	fprintf(stdout,"%s",tmpAtom->value);
	replace(objCptr,args[0]);
	deleteArg(args,1);
	return status;
}
