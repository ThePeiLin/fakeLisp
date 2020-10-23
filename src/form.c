#include<stdlib.h>
#include"tool.h"
#include"form.h"
#include"preprocess.h"

Cptr** dealArg(Cptr* argCptr,int num)
{
	Cptr* tmpCptr=(argCptr->type==PAIR)?argCptr:NULL;
	Cptr** args=NULL;
	if(!(args=(Cptr**)malloc(num*sizeof(Cptr*))))errors(OUTOFMEMORY,__FILE__,__LINE__);
	int i;
	for(i=0;i<num;i++)
	{
		if(argCptr->type==NIL||argCptr->type==ATM)
			args[i]=newCptr(0,NULL);
		else
		{
			args[i]=newCptr(0,NULL);
			replace(args[i],&((ANS_pair*)argCptr->value)->car);
			deleteCptr(&((ANS_pair*)argCptr->value)->car);
			argCptr=&((ANS_pair*)argCptr->value)->cdr;
		}
	}
	if(tmpCptr!=NULL)
	{
		Cptr* beDel=newCptr(0,NULL);
		beDel->type=tmpCptr->type;
		beDel->value=tmpCptr->value;
		((ANS_pair*)tmpCptr->value)->prev=NULL;
		tmpCptr->type=argCptr->type;
		tmpCptr->value=argCptr->value;
		if(argCptr->type==PAIR)
			((ANS_pair*)argCptr->value)->prev=tmpCptr->outer;
		else if(argCptr->type==ATM)
			((ANS_atom*)argCptr->value)->prev=tmpCptr->outer;
		argCptr->type=NIL;
		argCptr->value=NULL;
		deleteCptr(beDel);
		free(beDel);
	}
	return args;
}

int coutArg(Cptr* argCptr)
{
	int num=0;
	while(argCptr->type==PAIR)
	{
		Cptr* tmp=&((ANS_pair*)argCptr->value)->car;
		if(tmp->type!=NIL)num++;
		argCptr=&((ANS_pair*)argCptr->value)->cdr;
	}
	return num;
}

void deleteArg(Cptr** args,int num)
{
	int i;
	for(i=0;i++;i<num)
	{
		deleteCptr(args[i]);
		free(args[i]);
	}
	free(args);
}

int isFalse(const Cptr* objCptr)
{
	if(objCptr->type==NIL)return 1;
	else if(objCptr->type==PAIR)
	{
		ANS_pair* tmpPair=objCptr->value;
		if(tmpPair->car.type==NIL&&tmpPair->cdr.type==NIL)return 1;
	}
	return 0;
}

ErrorStatus N_quote(Cptr* objCptr,env* curEnv)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	Cptr** args=dealArg(&objCptr->outer->cdr,1);
	replace(objCptr,args[0]);
	deleteArg(args,1);
	return status;
}

ErrorStatus N_car(Cptr* objCptr,env* curEnv)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	Cptr** args=dealArg(&objCptr->outer->cdr,1);
	status=eval(args[0],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,1);
		return status;
	}
	if(args[0]->type==ATM||args[0]->type==NIL)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		replace(status.place,args[0]);
		deleteArg(args,1);
		return status;
	}
	replace(objCptr,&((ANS_pair*)args[0]->value)->car);
	deleteArg(args,1);
	return status;
}

ErrorStatus N_cdr(Cptr* objCptr,env* curEnv)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	Cptr** args=dealArg(&objCptr->outer->cdr,1);
	status=eval(args[0],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,1);
		return status;
	}
	if(args[0]->type==ATM||args[0]->type==NIL)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		replace(status.place,args[0]);
		deleteArg(args,1);
		return status;
	}
	replace(objCptr,&((ANS_pair*)args[0]->value)->cdr);
	deleteArg(args,1);
	return status;
}

ErrorStatus N_cons(Cptr* objCptr,env* curEnv)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	Cptr** args=dealArg(&objCptr->outer->cdr,2);
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
	objCptr->type=PAIR;
	ANS_pair* tmpPair=objCptr->value=newPair(0,objCptr->outer);
	replace(&tmpPair->car,args[0]);
	replace(&tmpPair->cdr,args[1]);
	deleteArg(args,2);
	return status;
}

ErrorStatus N_eq(Cptr* objCptr,env* curEnv)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	Cptr** args=dealArg(&objCptr->outer->cdr,2);
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
	if(Cptrcmp(args[0],args[1])==0)
	{
		objCptr->type=NIL;
		objCptr->value=NULL;
	}
	else
	{
		ANS_atom* tmpAtm=newAtom(INT,NULL,objCptr->outer);
		objCptr->type=ATM;
		objCptr->value=tmpAtm;
		tmpAtm->value.num=1;
	}
	deleteArg(args,2);
	return status;
}

ErrorStatus N_ANS_atom(Cptr* objCptr,env* curEnv)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	Cptr** args=dealArg(&objCptr->outer->cdr,1);
	status=eval(args[0],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,1);
		return status;
	}
	if(args[0]->type!=PAIR)
	{
		ANS_atom* tmpAtm=newAtom(INT,NULL,objCptr->outer);
		objCptr->type=ATM;
		objCptr->value=tmpAtm;
		tmpAtm->value.num=1;
	}
	else
	{
		ANS_pair* tmpPair=args[0]->value;
		if(tmpPair->car.type==NIL&&tmpPair->cdr.type==NIL)
		{
			ANS_atom* tmpAtm=newAtom(INT,NULL,objCptr->outer);
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

ErrorStatus N_null(Cptr* objCptr,env* curEnv)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	Cptr** args=dealArg(&objCptr->outer->cdr,1);
	status=eval(args[0],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,1);
		return status;
	}
	if(args[0]->type==NIL)
	{
		ANS_atom* tmpAtm=newAtom(INT,NULL,objCptr->outer);
		objCptr->type=ATM;
		objCptr->value=tmpAtm;
		tmpAtm->value.num=1;
	}
	else if(args[0]->type==PAIR)
	{
		ANS_pair* tmpPair=args[0]->value;
		if(tmpPair->car.type==NIL&&tmpPair->cdr.type==NIL)
		{
			ANS_atom* tmpAtm=newAtom(INT,NULL,objCptr->outer);
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

ErrorStatus N_and(Cptr* objCptr,env* curEnv)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	Cptr** args=dealArg(&objCptr->outer->cdr,2);
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
	if(isFalse(args[0])||isFalse(args[1]))
	{
		objCptr->type=NIL;
		objCptr->value=NULL;
	}
	else
	{
		ANS_atom* tmpAtm=newAtom(INT,NULL,objCptr->outer);
		objCptr->type=ATM;
		objCptr->value=tmpAtm;
		tmpAtm->value.num=1;
	}
	deleteArg(args,1);
	return status;
}

ErrorStatus N_or(Cptr* objCptr,env* curEnv)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	Cptr** args=dealArg(&objCptr->outer->cdr,2);
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
	if(isFalse(args[0])&&isFalse(args[1]))
	{
		objCptr->type=NIL;
		objCptr->value=NULL;
	}
	else
	{
		ANS_atom* tmpAtm=newAtom(INT,NULL,objCptr->outer);
		objCptr->type=ATM;
		objCptr->value=tmpAtm;
		tmpAtm->value.num=1;
	}
	deleteArg(args,1);
	return status;
}

ErrorStatus N_not(Cptr* objCptr,env* curEnv)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	Cptr** args=dealArg(&objCptr->outer->cdr,1);
	status=eval(args[0],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,1);
		return status;
	}
	if(isFalse(args[0]))
	{
		ANS_atom* tmpAtm=newAtom(INT,NULL,objCptr->outer);
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

ErrorStatus N_cond(Cptr* objCptr,env* curEnv)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	int condNum=coutArg(&objCptr->outer->cdr);
	Cptr** condition=dealArg(&objCptr->outer->cdr,condNum);
	int i;
	for(i=0;i<condNum;i++)
	{
		if(condition[i]->type!=PAIR)
		{
			status.status=SYNTAXERROR;
			status.place=newCptr(0,NULL);
			replace(status.place,condition[i]);
			deleteArg(condition,condNum);
			return status;
		}
		int expNum=coutArg(condition[i]);
		Cptr** expression=dealArg(condition[i],expNum);
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

ErrorStatus N_define(Cptr* objCptr,env* curEnv)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	Cptr** args=dealArg(&objCptr->outer->cdr,2);
	status=eval(args[1],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	if(args[0]->type!=ATM)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		replace(status.place,args[0]);
		deleteArg(args,2);
		return status;
	}
	ANS_atom* tmpAtm=args[0]->value;
	if(tmpAtm->type!=SYM)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		replace(status.place,args[0]);
		deleteArg(args,2);
		return status;
	}
	addDefine(tmpAtm->value.str,args[1],curEnv);
	replace(objCptr,args[1]);
	deleteArg(args,2);
	return status;
}

ErrorStatus N_lambda(Cptr* objCptr,env* curEnv)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	if(objCptr->outer->prev==NULL)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		return status;
	}
	env* tmpEnv=newEnv(curEnv);
	int i;
	int expNum=coutArg(&objCptr->outer->cdr);
	Cptr** expression=dealArg(&objCptr->outer->cdr,expNum);
	int fakeArgNum=coutArg(expression[0]);
	if(fakeArgNum!=0)
	{
		Cptr** fakeArg=dealArg(expression[0],fakeArgNum);
		Cptr** realArg=dealArg(&objCptr->outer->prev->cdr,fakeArgNum);
		for(i=0;i<fakeArgNum;i++)
		{
			ErrorStatus status=eval(realArg[i],curEnv);
			if(status.status!=0)
			{
				deleteArg(fakeArg,fakeArgNum);
				deleteArg(realArg,fakeArgNum);
				return status;
			}
		}
		for(i=0;i<fakeArgNum;i++)
		{
			ANS_atom* tmpAtm=NULL;
			if(fakeArg[i]->type==ATM&&((ANS_atom*)fakeArg[i]->value)->type==SYM)
				tmpAtm=fakeArg[i]->value;
			else
			{
				status.status=SYNTAXERROR;
				status.place=newCptr(0,NULL);
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

ErrorStatus N_list(Cptr* objCptr,env* curEnv)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	int argNum=coutArg(&objCptr->outer->cdr);
	Cptr** args=dealArg(&objCptr->outer->cdr,argNum);
	int i;
	Cptr* result=newCptr(0,NULL);
	Cptr* tmp=result;
	for(i=0;i<argNum;i++)
	{
		status=eval(args[i],curEnv);
		if(status.status!=0)
		{
			deleteArg(args,argNum);
			deleteCptr(result);
			return status;
		}
		tmp->type=PAIR;
		tmp->value=newPair(0,tmp->outer);
		ANS_pair* tmpPair=tmp->value;
		replace(&tmpPair->car,args[i]);
		tmp=&tmpPair->cdr;
	}
	replace(objCptr,result);
	deleteCptr(result);
	deleteArg(args,argNum);
	return status;
}

ErrorStatus N_defmacro(Cptr* objCptr,env* curEnv)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	Cptr** args=dealArg(&objCptr->outer->cdr,2);
	if(args[0]->type!=PAIR)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		replace(status.place,args[0]);
		deleteArg(args,2);
		return status;
	}
	Cptr* pattern=args[0];
	Cptr* express=args[1];
	addMacro(pattern,express);
	deleteArg(args,2);
	objCptr->type=NIL;
	objCptr->value=NULL;
	return status;
}

ErrorStatus N_append(Cptr* objCptr,env* curEnv)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	Cptr** args=dealArg(&objCptr->outer->cdr,2);
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
	if(args[0]->type!=PAIR)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		replace(status.place,args[0]);
		deleteArg(args,2);
		return status;
	}
	Cptr* tmp=&((ANS_pair*)args[0]->value)->car;
	while(nextCptr(tmp)!=NULL)tmp=nextCptr(tmp);
	ANS_pair* tmpPair=newPair(0,tmp->outer);
	tmp->outer->cdr.type=PAIR;
	tmp->outer->cdr.value=tmpPair;
	replace(&tmpPair->car,args[1]);
	replace(objCptr,args[0]);
	deleteArg(args,2);
	return status;
}

ErrorStatus N_extend(Cptr* objCptr,env* curEnv)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	Cptr** args=dealArg(&objCptr->outer->cdr,2);
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
	if(args[0]->type!=PAIR)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		replace(status.place,args[0]);
		deleteArg(args,2);
		return status;
	}
	Cptr* tmp=&((ANS_pair*)args[0]->value)->car;
	while(nextCptr(tmp)!=NULL)tmp=nextCptr(tmp);
	replace(&tmp->outer->cdr,args[1]);
	replace(objCptr,args[0]);
	deleteArg(args,2);
	return status;
}

/*ErrorStatus N_undef(Cptr* objCptr,env* curEnv)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	Cptr** args=dealArg(&objCptr->outer->cdr,1);
	deleteMacro(args[0]);
	deleteArg(args,1);
	objCptr->type=NIL;
	objCptr->value=NULL;
	return status;
}*/

ErrorStatus N_add(Cptr* objCptr,env* curEnv)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	Cptr** args=dealArg(&objCptr->outer->cdr,2);
	status=eval(args[0],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	if(args[0]->type!=ATM)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
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
		status.place=newCptr(0,NULL);
		replace(status.place,args[1]);
		deleteArg(args,2);
		return status;
	}
	ANS_atom* ANS_atom1=args[0]->value;
	ANS_atom* ANS_atom2=args[1]->value;
	if(ANS_atom1->type!=DBL&&ANS_atom1->type!=INT)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		replace(status.place,args[0]);
		deleteArg(args,2);
		return status;
	}
	if(ANS_atom2->type!=DBL&&ANS_atom2->type!=INT)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		replace(status.place,args[1]);
		deleteArg(args,2);
		return status;
	}
	ANS_atom* tmpAtm=newAtom(0,NULL,objCptr->outer);
	if(ANS_atom1->type==DBL||ANS_atom2->type==DBL)
	{
		double result=((ANS_atom1->type==DBL)?ANS_atom1->value.dbl:ANS_atom1->value.num)+((ANS_atom2->type==DBL)?ANS_atom2->value.dbl:ANS_atom2->value.num);
		tmpAtm->type=DBL;
		tmpAtm->value.dbl=result;
	}
	else if(ANS_atom1->type==INT&&ANS_atom2->type==INT)
	{
		int32_t result=ANS_atom1->value.num+ANS_atom2->value.num;
		tmpAtm->type=INT;
		tmpAtm->value.num=result;
	}
	objCptr->type=ATM;
	objCptr->value=tmpAtm;
	deleteArg(args,2);
	return status;
}

ErrorStatus N_sub(Cptr* objCptr,env* curEnv)
{
	ErrorStatus status={0,NULL};
	Cptr** args=dealArg(&objCptr->outer->cdr,2);
	status=eval(args[0],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	if(args[0]->type!=ATM)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
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
		status.place=newCptr(0,NULL);
		replace(status.place,args[1]);
		deleteArg(args,2);
		return status;
	}
	ANS_atom* ANS_atom1=args[0]->value;
	ANS_atom* ANS_atom2=args[1]->value;
	if(ANS_atom1->type!=DBL&&ANS_atom1->type!=INT)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		replace(status.place,args[0]);
		deleteArg(args,2);
		return status;
	}
	if(ANS_atom2->type!=DBL&&ANS_atom2->type!=INT)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		replace(status.place,args[1]);
		deleteArg(args,2);
		return status;
	}
	ANS_atom* tmpAtm=newAtom(0,NULL,objCptr->outer);
	if(ANS_atom1->type==DBL||ANS_atom2->type==DBL)
	{
		double result=((ANS_atom1->type==DBL)?ANS_atom1->value.dbl:ANS_atom1->value.num)-((ANS_atom2->type==DBL)?ANS_atom2->value.dbl:ANS_atom2->value.num);
		tmpAtm->type=DBL;
		tmpAtm->value.dbl=result;
	}
	else if(ANS_atom1->type==INT&&ANS_atom2->type==INT)
	{
		long result=ANS_atom1->value.num-ANS_atom2->value.num;
		tmpAtm->type=INT;
		tmpAtm->value.num=result;
	}
	objCptr->type=ATM;
	objCptr->value=tmpAtm;
	deleteArg(args,2);
	return status;
}

ErrorStatus N_mul(Cptr* objCptr,env* curEnv)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	Cptr** args=dealArg(&objCptr->outer->cdr,2);
	status=eval(args[0],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	if(args[0]->type!=ATM)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
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
		status.place=newCptr(0,NULL);
		replace(status.place,args[1]);
		deleteArg(args,2);
		return status;
	}
	ANS_atom* ANS_atom1=args[0]->value;
	ANS_atom* ANS_atom2=args[1]->value;
	if(ANS_atom1->type!=DBL&&ANS_atom1->type!=INT)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		replace(status.place,args[0]);
		deleteArg(args,2);
		return status;
	}
	if(ANS_atom2->type!=DBL&&ANS_atom2->type!=INT)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		replace(status.place,args[1]);
		deleteArg(args,2);
		return status;
	}
	ANS_atom* tmpAtm=newAtom(0,NULL,objCptr->outer);
	if(ANS_atom1->type==DBL||ANS_atom2->type==DBL)
	{
		double result=((ANS_atom1->type==DBL)?ANS_atom1->value.dbl:ANS_atom1->value.num)*((ANS_atom2->type==DBL)?ANS_atom2->value.dbl:ANS_atom2->value.num);
		tmpAtm->type=DBL;
		tmpAtm->value.dbl=result;
	}
	else if(ANS_atom1->type==INT&&ANS_atom2->type==INT)
	{
		int32_t result=ANS_atom1->value.num*ANS_atom2->value.num;
		tmpAtm->type=INT;
		tmpAtm->value.num=result;
	}
	objCptr->type=ATM;
	objCptr->value=tmpAtm;
	deleteArg(args,2);
	return status;
}

ErrorStatus N_div(Cptr* objCptr,env* curEnv)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	Cptr** args=dealArg(&objCptr->outer->cdr,2);
	status=eval(args[0],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	if(args[0]->type!=ATM)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
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
		status.place=newCptr(0,NULL);
		replace(status.place,args[0]);
		deleteArg(args,2);
		return status;
	}
	ANS_atom* ANS_atom1=args[0]->value;
	ANS_atom* ANS_atom2=args[1]->value;
	if(ANS_atom1->type!=DBL&&ANS_atom1->type!=INT)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		replace(status.place,args[0]);
		deleteArg(args,2);
		return status;
	}
	if(ANS_atom2->type!=DBL&&ANS_atom2->type!=INT)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		replace(status.place,args[1]);
		deleteArg(args,2);
		return status;
	}
	ANS_atom* tmpAtm=newAtom(DBL,NULL,objCptr->outer);
	if(ANS_atom1->type==DBL||ANS_atom2->type==DBL)
	{
		double result=((ANS_atom1->type==DBL)?ANS_atom1->value.dbl:ANS_atom1->value.num)/((ANS_atom2->type==DBL)?ANS_atom2->value.dbl:ANS_atom2->value.num);
		tmpAtm->type=DBL;
		tmpAtm->value.dbl=result;
	}
	objCptr->type=ATM;
	objCptr->value=tmpAtm;
	deleteArg(args,2);
	return status;
}

ErrorStatus N_mod(Cptr* objCptr,env* curEnv)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	Cptr** args=dealArg(&objCptr->outer->cdr,2);
	status=eval(args[0],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	if(args[0]->type!=ATM)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
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
		status.place=newCptr(0,NULL);
		replace(status.place,args[1]);
		deleteArg(args,2);
		return status;
	}
	ANS_atom* ANS_atom1=args[0]->value;
	ANS_atom* ANS_atom2=args[1]->value;
	if(ANS_atom1->type!=DBL&&ANS_atom1->type!=INT)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		replace(status.place,args[0]);
		deleteArg(args,2);
		return status;
	}
	if(ANS_atom2->type!=DBL&&ANS_atom2->type!=INT)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		replace(status.place,args[1]);
		deleteArg(args,2);
		return status;
	}
	ANS_atom* tmpAtm=newAtom(INT,NULL,objCptr->outer);
	int32_t result=((ANS_atom1->type==DBL)?(int)ANS_atom1->value.dbl:ANS_atom1->value.num)%((ANS_atom2->type==DBL)?(int)ANS_atom2->value.dbl:ANS_atom2->value.num);
	tmpAtm->value.num=result;
	deleteArg(args,2);
	return status;
}

ErrorStatus N_print(Cptr* objCptr,env* curEnv)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	Cptr** args=dealArg(&objCptr->outer->cdr,1);
	status=eval(args[0],curEnv);
	if(status.status!=0)
	{
		deleteArg(args,1);
		return status;
	}
	if(args[0]->type!=ATM)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		replace(status.place,args[0]);
		deleteArg(args,1);
		return status;
	}
	ANS_atom* tmpAtom=args[0]->value;
	fprintf(stdout,"%s",tmpAtom->value);
	replace(objCptr,args[0]);
	deleteArg(args,1);
	return status;
}
