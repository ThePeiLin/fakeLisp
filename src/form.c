#include<stdlib.h>
#include<string.h>
#ifdef _WIN32
#include<tchar.h>
#include<direct.h>
#else
#include<unistd.h>
#endif
#include"tool.h"
#include"form.h"
#include"preprocess.h"

AST_cptr** dealArg(AST_cptr* argCptr,int num)
{
	AST_cptr* tmpCptr=(argCptr->type==PAIR)?argCptr:NULL;
	AST_cptr** args=NULL;
	if(!(args=(AST_cptr**)malloc(num*sizeof(AST_cptr*))))errors("dealArg",__FILE__,__LINE__);
	int i;
	for(i=0;i<num;i++)
	{
		if(argCptr->type==NIL||argCptr->type==ATM)
			args[i]=newCptr(0,NULL);
		else
		{
			args[i]=newCptr(0,NULL);
			replace(args[i],&((AST_pair*)argCptr->value)->car);
			deleteCptr(&((AST_pair*)argCptr->value)->car);
			argCptr=&((AST_pair*)argCptr->value)->cdr;
		}
	}
	if(tmpCptr!=NULL)
	{
		AST_cptr* beDel=newCptr(0,NULL);
		beDel->type=tmpCptr->type;
		beDel->value=tmpCptr->value;
		((AST_pair*)tmpCptr->value)->prev=NULL;
		tmpCptr->type=argCptr->type;
		tmpCptr->value=argCptr->value;
		if(argCptr->type==PAIR)
			((AST_pair*)argCptr->value)->prev=tmpCptr->outer;
		else if(argCptr->type==ATM)
			((AST_atom*)argCptr->value)->prev=tmpCptr->outer;
		argCptr->type=NIL;
		argCptr->value=NULL;
		deleteCptr(beDel);
		free(beDel);
	}
	return args;
}

int coutArg(AST_cptr* argCptr)
{
	int num=0;
	while(argCptr->type==PAIR)
	{
		AST_cptr* tmp=&((AST_pair*)argCptr->value)->car;
		if(tmp->type!=NIL)num++;
		argCptr=&((AST_pair*)argCptr->value)->cdr;
	}
	return num;
}

void deleteArg(AST_cptr** args,int num)
{
	int i;
	for(i=0;i<num;i++)
	{
		deleteCptr(args[i]);
		free(args[i]);
	}
	free(args);
}

int isFalse(const AST_cptr* objCptr)
{
	if(objCptr->type==NIL)return 1;
	else if(objCptr->type==PAIR)
	{
		AST_pair* tmpPair=objCptr->value;
		if(tmpPair->car.type==NIL&&tmpPair->cdr.type==NIL)return 1;
	}
	return 0;
}

ErrorStatus N_quote(AST_cptr* objCptr,PreEnv* curEnv,intpr* inter)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	AST_cptr** args=dealArg(&objCptr->outer->cdr,1);
	replace(objCptr,args[0]);
	deleteArg(args,1);
	return status;
}

ErrorStatus N_car(AST_cptr* objCptr,PreEnv* curEnv,intpr* inter)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	AST_cptr** args=dealArg(&objCptr->outer->cdr,1);
	status=eval(args[0],curEnv,inter);
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
	replace(objCptr,&((AST_pair*)args[0]->value)->car);
	deleteArg(args,1);
	return status;
}

ErrorStatus N_cdr(AST_cptr* objCptr,PreEnv* curEnv,intpr* inter)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	AST_cptr** args=dealArg(&objCptr->outer->cdr,1);
	status=eval(args[0],curEnv,inter);
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
	replace(objCptr,&((AST_pair*)args[0]->value)->cdr);
	deleteArg(args,1);
	return status;
}

ErrorStatus N_cons(AST_cptr* objCptr,PreEnv* curEnv,intpr* inter)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	AST_cptr** args=dealArg(&objCptr->outer->cdr,2);
	status=eval(args[0],curEnv,inter);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	status=eval(args[1],curEnv,inter);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	objCptr->type=PAIR;
	AST_pair* tmpPair=objCptr->value=newPair(0,objCptr->outer);
	replace(&tmpPair->car,args[0]);
	replace(&tmpPair->cdr,args[1]);
	deleteArg(args,2);
	return status;
}

ErrorStatus N_eq(AST_cptr* objCptr,PreEnv* curEnv,intpr* inter)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	AST_cptr** args=dealArg(&objCptr->outer->cdr,2);
	status=eval(args[0],curEnv,inter);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	status=eval(args[1],curEnv,inter);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	if(AST_cptrcmp(args[0],args[1])==0)
	{
		objCptr->type=NIL;
		objCptr->value=NULL;
	}
	else
	{
		AST_atom* tmpAtm=newAtom(IN32,NULL,objCptr->outer);
		objCptr->type=ATM;
		objCptr->value=tmpAtm;
		tmpAtm->value.num=1;
	}
	deleteArg(args,2);
	return status;
}

ErrorStatus N_atom(AST_cptr* objCptr,PreEnv* curEnv,intpr* inter)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	AST_cptr** args=dealArg(&objCptr->outer->cdr,1);
	status=eval(args[0],curEnv,inter);
	if(status.status!=0)
	{
		deleteArg(args,1);
		return status;
	}
	if(args[0]->type!=PAIR)
	{
		AST_atom* tmpAtm=newAtom(IN32,NULL,objCptr->outer);
		objCptr->type=ATM;
		objCptr->value=tmpAtm;
		tmpAtm->value.num=1;
	}
	else
	{
		AST_pair* tmpPair=args[0]->value;
		if(tmpPair->car.type==NIL&&tmpPair->cdr.type==NIL)
		{
			AST_atom* tmpAtm=newAtom(IN32,NULL,objCptr->outer);
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

ErrorStatus N_null(AST_cptr* objCptr,PreEnv* curEnv,intpr* inter)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	AST_cptr** args=dealArg(&objCptr->outer->cdr,1);
	status=eval(args[0],curEnv,inter);
	if(status.status!=0)
	{
		deleteArg(args,1);
		return status;
	}
	if(args[0]->type==NIL)
	{
		AST_atom* tmpAtm=newAtom(IN32,NULL,objCptr->outer);
		objCptr->type=ATM;
		objCptr->value=tmpAtm;
		tmpAtm->value.num=1;
	}
	else if(args[0]->type==PAIR)
	{
		AST_pair* tmpPair=args[0]->value;
		if(tmpPair->car.type==NIL&&tmpPair->cdr.type==NIL)
		{
			AST_atom* tmpAtm=newAtom(IN32,NULL,objCptr->outer);
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

ErrorStatus N_and(AST_cptr* objCptr,PreEnv* curEnv,intpr* inter)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	AST_cptr** args=dealArg(&objCptr->outer->cdr,2);
	status=eval(args[0],curEnv,inter);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	status=eval(args[1],curEnv,inter);
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
		AST_atom* tmpAtm=newAtom(IN32,NULL,objCptr->outer);
		objCptr->type=ATM;
		objCptr->value=tmpAtm;
		tmpAtm->value.num=1;
	}
	deleteArg(args,1);
	return status;
}

ErrorStatus N_or(AST_cptr* objCptr,PreEnv* curEnv,intpr* inter)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	AST_cptr** args=dealArg(&objCptr->outer->cdr,2);
	status=eval(args[0],curEnv,inter);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	status=eval(args[1],curEnv,inter);
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
		AST_atom* tmpAtm=newAtom(IN32,NULL,objCptr->outer);
		objCptr->type=ATM;
		objCptr->value=tmpAtm;
		tmpAtm->value.num=1;
	}
	deleteArg(args,1);
	return status;
}

ErrorStatus N_not(AST_cptr* objCptr,PreEnv* curEnv,intpr* inter)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	AST_cptr** args=dealArg(&objCptr->outer->cdr,1);
	status=eval(args[0],curEnv,inter);
	if(status.status!=0)
	{
		deleteArg(args,1);
		return status;
	}
	if(isFalse(args[0]))
	{
		AST_atom* tmpAtm=newAtom(IN32,NULL,objCptr->outer);
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

ErrorStatus N_cond(AST_cptr* objCptr,PreEnv* curEnv,intpr* inter)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	int condNum=coutArg(&objCptr->outer->cdr);
	AST_cptr** condition=dealArg(&objCptr->outer->cdr,condNum);
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
		AST_cptr** expression=dealArg(condition[i],expNum);
		status=eval(expression[0],curEnv,inter);
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
			status=eval(expression[j],curEnv,inter);
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

ErrorStatus N_define(AST_cptr* objCptr,PreEnv* curEnv,intpr* inter)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	AST_cptr** args=dealArg(&objCptr->outer->cdr,2);
	status=eval(args[1],curEnv,inter);
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
	AST_atom* tmpAtm=args[0]->value;
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

ErrorStatus N_lambda(AST_cptr* objCptr,PreEnv* curEnv,intpr* inter)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	if(objCptr->outer->prev==NULL)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		return status;
	}
	PreEnv* tmpEnv=newEnv(curEnv);
	int i;
	int expNum=coutArg(&objCptr->outer->cdr);
	AST_cptr** expression=dealArg(&objCptr->outer->cdr,expNum);
	int fakeArgNum=coutArg(expression[0]);
	if(fakeArgNum!=0)
	{
		AST_cptr** fakeArg=dealArg(expression[0],fakeArgNum);
		AST_cptr** realArg=dealArg(&objCptr->outer->prev->cdr,fakeArgNum);
		for(i=0;i<fakeArgNum;i++)
		{
			ErrorStatus status=eval(realArg[i],curEnv,inter);
			if(status.status!=0)
			{
				deleteArg(fakeArg,fakeArgNum);
				deleteArg(realArg,fakeArgNum);
				return status;
			}
		}
		for(i=0;i<fakeArgNum;i++)
		{
			AST_atom* tmpAtm=NULL;
			if(fakeArg[i]->type==ATM&&((AST_atom*)fakeArg[i]->value)->type==SYM)
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
		status=eval(expression[i],tmpEnv,inter);
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

ErrorStatus N_list(AST_cptr* objCptr,PreEnv* curEnv,intpr* inter)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	int argNum=coutArg(&objCptr->outer->cdr);
	AST_cptr** args=dealArg(&objCptr->outer->cdr,argNum);
	int i;
	AST_cptr* result=newCptr(0,NULL);
	AST_cptr* tmp=result;
	for(i=0;i<argNum;i++)
	{
		status=eval(args[i],curEnv,inter);
		if(status.status!=0)
		{
			deleteArg(args,argNum);
			deleteCptr(result);
			return status;
		}
		tmp->type=PAIR;
		tmp->value=newPair(0,tmp->outer);
		AST_pair* tmpPair=tmp->value;
		replace(&tmpPair->car,args[i]);
		tmp=&tmpPair->cdr;
	}
	replace(objCptr,result);
	deleteCptr(result);
	free(result);
	deleteArg(args,argNum);
	return status;
}

ErrorStatus N_defmacro(AST_cptr* objCptr,PreEnv* curEnv,intpr* inter)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	AST_cptr** args=dealArg(&objCptr->outer->cdr,2);
	if(args[0]->type!=PAIR)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		replace(status.place,args[0]);
		deleteArg(args,2);
		return status;
	}
	AST_cptr* pattern=args[0];
	AST_cptr* express=args[1];
	addMacro(pattern,express);
	free(args);
	objCptr->type=NIL;
	objCptr->value=NULL;
	return status;
}

ErrorStatus N_append(AST_cptr* objCptr,PreEnv* curEnv,intpr* inter)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	AST_cptr** args=dealArg(&objCptr->outer->cdr,2);
	status=eval(args[0],curEnv,inter);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	status=eval(args[1],curEnv,inter);
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
	AST_cptr* tmp=&((AST_pair*)args[0]->value)->car;
	while(nextCptr(tmp)!=NULL)tmp=nextCptr(tmp);
	AST_pair* tmpPair=newPair(0,tmp->outer);
	tmp->outer->cdr.type=PAIR;
	tmp->outer->cdr.value=tmpPair;
	replace(&tmpPair->car,args[1]);
	replace(objCptr,args[0]);
	deleteArg(args,2);
	return status;
}

ErrorStatus N_extend(AST_cptr* objCptr,PreEnv* curEnv,intpr* inter)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	AST_cptr** args=dealArg(&objCptr->outer->cdr,2);
	status=eval(args[0],curEnv,inter);
	if(status.status!=0)
	{
		deleteArg(args,2);
		return status;
	}
	status=eval(args[1],curEnv,inter);
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
	AST_cptr* tmp=&((AST_pair*)args[0]->value)->car;
	while(nextCptr(tmp)!=NULL)tmp=nextCptr(tmp);
	replace(&tmp->outer->cdr,args[1]);
	replace(objCptr,args[0]);
	deleteArg(args,2);
	return status;
}

/*ErrorStatus N_undef(AST_cptr* objCptr,PreEnv* curEnv)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	AST_cptr** args=dealArg(&objCptr->outer->cdr,1);
	deleteMacro(args[0]);
	deleteArg(args,1);
	objCptr->type=NIL;
	objCptr->value=NULL;
	return status;
}*/

ErrorStatus N_add(AST_cptr* objCptr,PreEnv* curEnv,intpr* inter)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	AST_cptr** args=dealArg(&objCptr->outer->cdr,2);
	status=eval(args[0],curEnv,inter);
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
	status=eval(args[1],curEnv,inter);
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
	AST_atom* AST_atom1=args[0]->value;
	AST_atom* AST_atom2=args[1]->value;
	if(AST_atom1->type!=DBL&&AST_atom1->type!=IN32)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		replace(status.place,args[0]);
		deleteArg(args,2);
		return status;
	}
	if(AST_atom2->type!=DBL&&AST_atom2->type!=IN32)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		replace(status.place,args[1]);
		deleteArg(args,2);
		return status;
	}
	AST_atom* tmpAtm=newAtom(0,NULL,objCptr->outer);
	if(AST_atom1->type==DBL||AST_atom2->type==DBL)
	{
		double result=((AST_atom1->type==DBL)?AST_atom1->value.dbl:AST_atom1->value.num)+((AST_atom2->type==DBL)?AST_atom2->value.dbl:AST_atom2->value.num);
		tmpAtm->type=DBL;
		tmpAtm->value.dbl=result;
	}
	else if(AST_atom1->type==IN32&&AST_atom2->type==IN32)
	{
		int32_t result=AST_atom1->value.num+AST_atom2->value.num;
		tmpAtm->type=IN32;
		tmpAtm->value.num=result;
	}
	objCptr->type=ATM;
	objCptr->value=tmpAtm;
	deleteArg(args,2);
	return status;
}

ErrorStatus N_sub(AST_cptr* objCptr,PreEnv* curEnv,intpr* inter)
{
	ErrorStatus status={0,NULL};
	AST_cptr** args=dealArg(&objCptr->outer->cdr,2);
	status=eval(args[0],curEnv,inter);
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
	status=eval(args[1],curEnv,inter);
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
	AST_atom* AST_atom1=args[0]->value;
	AST_atom* AST_atom2=args[1]->value;
	if(AST_atom1->type!=DBL&&AST_atom1->type!=IN32)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		replace(status.place,args[0]);
		deleteArg(args,2);
		return status;
	}
	if(AST_atom2->type!=DBL&&AST_atom2->type!=IN32)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		replace(status.place,args[1]);
		deleteArg(args,2);
		return status;
	}
	AST_atom* tmpAtm=newAtom(0,NULL,objCptr->outer);
	if(AST_atom1->type==DBL||AST_atom2->type==DBL)
	{
		double result=((AST_atom1->type==DBL)?AST_atom1->value.dbl:AST_atom1->value.num)-((AST_atom2->type==DBL)?AST_atom2->value.dbl:AST_atom2->value.num);
		tmpAtm->type=DBL;
		tmpAtm->value.dbl=result;
	}
	else if(AST_atom1->type==IN32&&AST_atom2->type==IN32)
	{
		long result=AST_atom1->value.num-AST_atom2->value.num;
		tmpAtm->type=IN32;
		tmpAtm->value.num=result;
	}
	objCptr->type=ATM;
	objCptr->value=tmpAtm;
	deleteArg(args,2);
	return status;
}

ErrorStatus N_mul(AST_cptr* objCptr,PreEnv* curEnv,intpr* inter)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	AST_cptr** args=dealArg(&objCptr->outer->cdr,2);
	status=eval(args[0],curEnv,inter);
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
	status=eval(args[1],curEnv,inter);
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
	AST_atom* AST_atom1=args[0]->value;
	AST_atom* AST_atom2=args[1]->value;
	if(AST_atom1->type!=DBL&&AST_atom1->type!=IN32)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		replace(status.place,args[0]);
		deleteArg(args,2);
		return status;
	}
	if(AST_atom2->type!=DBL&&AST_atom2->type!=IN32)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		replace(status.place,args[1]);
		deleteArg(args,2);
		return status;
	}
	AST_atom* tmpAtm=newAtom(0,NULL,objCptr->outer);
	if(AST_atom1->type==DBL||AST_atom2->type==DBL)
	{
		double result=((AST_atom1->type==DBL)?AST_atom1->value.dbl:AST_atom1->value.num)*((AST_atom2->type==DBL)?AST_atom2->value.dbl:AST_atom2->value.num);
		tmpAtm->type=DBL;
		tmpAtm->value.dbl=result;
	}
	else if(AST_atom1->type==IN32&&AST_atom2->type==IN32)
	{
		int32_t result=AST_atom1->value.num*AST_atom2->value.num;
		tmpAtm->type=IN32;
		tmpAtm->value.num=result;
	}
	objCptr->type=ATM;
	objCptr->value=tmpAtm;
	deleteArg(args,2);
	return status;
}

ErrorStatus N_div(AST_cptr* objCptr,PreEnv* curEnv,intpr* inter)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	AST_cptr** args=dealArg(&objCptr->outer->cdr,2);
	status=eval(args[0],curEnv,inter);
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
	status=eval(args[1],curEnv,inter);
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
	AST_atom* AST_atom1=args[0]->value;
	AST_atom* AST_atom2=args[1]->value;
	if(AST_atom1->type!=DBL&&AST_atom1->type!=IN32)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		replace(status.place,args[0]);
		deleteArg(args,2);
		return status;
	}
	if(AST_atom2->type!=DBL&&AST_atom2->type!=IN32)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		replace(status.place,args[1]);
		deleteArg(args,2);
		return status;
	}
	AST_atom* tmpAtm=newAtom(DBL,NULL,objCptr->outer);
	if(AST_atom1->type==DBL||AST_atom2->type==DBL)
	{
		double result=((AST_atom1->type==DBL)?AST_atom1->value.dbl:AST_atom1->value.num)/((AST_atom2->type==DBL)?AST_atom2->value.dbl:AST_atom2->value.num);
		tmpAtm->type=DBL;
		tmpAtm->value.dbl=result;
	}
	objCptr->type=ATM;
	objCptr->value=tmpAtm;
	deleteArg(args,2);
	return status;
}

ErrorStatus N_mod(AST_cptr* objCptr,PreEnv* curEnv,intpr* inter)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	AST_cptr** args=dealArg(&objCptr->outer->cdr,2);
	status=eval(args[0],curEnv,inter);
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
	status=eval(args[1],curEnv,inter);
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
	AST_atom* AST_atom1=args[0]->value;
	AST_atom* AST_atom2=args[1]->value;
	if(AST_atom1->type!=DBL&&AST_atom1->type!=IN32)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		replace(status.place,args[0]);
		deleteArg(args,2);
		return status;
	}
	if(AST_atom2->type!=DBL&&AST_atom2->type!=IN32)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		replace(status.place,args[1]);
		deleteArg(args,2);
		return status;
	}
	AST_atom* tmpAtm=newAtom(IN32,NULL,objCptr->outer);
	int32_t result=((AST_atom1->type==DBL)?(int)AST_atom1->value.dbl:AST_atom1->value.num)%((AST_atom2->type==DBL)?(int)AST_atom2->value.dbl:AST_atom2->value.num);
	tmpAtm->value.num=result;
	deleteArg(args,2);
	return status;
}

ErrorStatus N_print(AST_cptr* objCptr,PreEnv* curEnv,intpr* inter)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	AST_cptr** args=dealArg(&objCptr->outer->cdr,1);
	status=eval(args[0],curEnv,inter);
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
	AST_atom* tmpAtom=args[0]->value;
	fprintf(stdout,"%s",tmpAtom->value.str);
	replace(objCptr,args[0]);
	deleteArg(args,1);
	return status;
}

ErrorStatus N_import(AST_cptr* objCptr,PreEnv* curEnv,intpr* inter)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	AST_cptr** args=dealArg(&objCptr->outer->cdr,1);
	if(args[0]->type!=ATM)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		replace(status.place,args[0]);
		deleteArg(args,1);
		return status;
	}
#ifdef _WIN32
	char* filetype=".dll";
#else
	char* filetype=".so";
#endif
	AST_atom* tmpAtom=args[0]->value;
	char* modname=(char*)malloc(sizeof(char)*(strlen(filetype)+strlen(tmpAtom->value.str)+1));
	strcpy(modname,tmpAtom->value.str);
	strcat(modname,filetype);
#ifdef _WIN32
	char* rp=_fullpath(NULL,modname,0);
#else
	char* rp=realpath(modname,0);
#endif
	if(rp==NULL)
	{
		perror(modname);
		exit(EXIT_FAILURE);
	}
	char* rep=relpath(getLastWorkDir(inter),rp);
	char* rmodname=(char*)malloc(sizeof(char)*(strlen(rep)-strlen(filetype)+1));
	if(!rmodname)errors("N_import",__FILE__,__LINE__);
	memcpy(rmodname,rep,strlen(rep)-strlen(filetype));
	rmodname[strlen(rep)-strlen(filetype)]='\0';
	if(!ModHasLoad(rmodname,*getpHead(inter)))
	{

		if(rp==NULL)
		{
			perror(rep);
			exit(EXIT_FAILURE);
		}
		Dlls** pDlls=getpDlls(inter);
		Modlist** pTail=getpTail(inter);
		Modlist** pHead=getpHead(inter);
		loadDll(rp,pDlls,rmodname,pTail);
		if(*pHead==NULL)*pHead=*pTail;
	}
	free(rmodname);
	free(modname);
	free(rp);
	free(rep);
	replace(objCptr,args[0]);
	deleteArg(args,1);
	return status;
}
