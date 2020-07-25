#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include"fake.h"
#include"tool.h"
#include"preprocess.h"

static nativeFunc* funAndForm=NULL;
static env* Glob=NULL;
static keyWord* KeyWords=NULL;

cptr* createTree(const char* objStr)
{
	if(objStr==NULL)return NULL;
	int i=0;
	int braketsNum=0;
	cptr* root=NULL;
	pair* objPair=NULL;
	cptr* objCptr;
	while(*(objStr+i)!='\0')
	{
		if(*(objStr+i)=='(')
		{
			i++;
			braketsNum++;
			if(root==NULL)
			{
				root=createCptr(objPair);
				root->type=PAR;
				root->value=createPair(NULL);
				objPair=root->value;
				objCptr=&objPair->car;
			}
			else
			{
				objPair=createPair(objPair);
				objCptr->type=PAR;
				objCptr->value=(void*)objPair;
				objCptr=&objPair->car;
			}
		}
		else if(*(objStr+i)==')')
		{
			i++;
			braketsNum--;
			if(braketsNum<0)
			{
				printf("%s:Syntax error.\n",objStr);
				if(root!=NULL)deleteCptr(root);
				return NULL;
			}
			pair* prev=NULL;
			if(objPair==NULL)break;
			while(objPair->prev!=NULL)
			{
				prev=objPair;
				objPair=objPair->prev;
				if(objPair->car.value==prev)break;
			}
			if(objPair->car.value==prev)
				objCptr=&objPair->car;
			else if(objPair->cdr.value==prev)
				objCptr=&objPair->cdr;
		}
		else if(*(objStr+i)==',')
		{
			i++;
			objCptr=&objPair->cdr;
		}
		else if(isspace(*(objStr+i)))
		{
			int j=0;
			char* tmpStr=(char*)objStr+i;
			while(isspace(*(tmpStr+j)))j--;
			if(*(tmpStr+j)==','||*(tmpStr+j)=='(')
			{
				j=1;
				while(isspace(*(tmpStr+j)))j++;
				i+=j;
				continue;
			}
			j=0;
			while(isspace(*(tmpStr+j)))j++;
			if(*(tmpStr+j)==',')
			{
				i+=j;
				continue;
			}
			i+=j;
			pair* tmp=createPair(objPair);
			objPair->cdr.type=PAR;
			objPair->cdr.value=(void*)tmp;
			objPair=tmp;
			objCptr=&objPair->car;
		}
		else if(*(objStr+i)=='\"')
		{
			if(root==NULL)objCptr=root=createCptr(objPair);
			rawString tmp=getStringBetweenMarks(objStr+i);
			objCptr->type=ATM;
			objCptr->value=(void*)createAtom(STR,tmp.str,objPair);
			i+=tmp.len;
			free(tmp.str);
		}
		else if(isdigit(*(objStr+i))||(*(objStr+i)=='-'&&isdigit(*(objStr+i+1))))
		{
			if(root==NULL)objCptr=root=createCptr(objPair);
			char* tmp=getStringFromList(objStr+i);
			objCptr->type=ATM;
			objCptr->value=(void*)createAtom((hasAlpha(tmp))?SYM:NUM,tmp,objPair);
			i+=strlen(tmp);
			free(tmp);
		}
		else
		{
			if(root==NULL)objCptr=root=createCptr(objPair);
			char* tmp=getStringFromList(objStr+i);
			objCptr->type=ATM;
			objCptr->value=(void*)createAtom(SYM,tmp,objPair);
			i+=strlen(tmp);
			free(tmp);
			continue;
		}
		if(braketsNum==0)break;
	}
	return root;
}

void addFunc(const char* name,errorStatus (*pFun)(cptr*,env*))
{
	nativeFunc* current=funAndForm;
	nativeFunc* prev=NULL;
	if(current==NULL)
	{
		if(!(funAndForm=(nativeFunc*)malloc(sizeof(nativeFunc))))errors(OUTOFMEMORY);
		if(!(funAndForm->functionName=(char*)malloc(sizeof(char)*(strlen(name)+1))))errors(OUTOFMEMORY);
		strcpy(funAndForm->functionName,name);
		funAndForm->function=pFun;
		funAndForm->next=NULL;
	}
	else
	{
		while(current!=NULL)
		{
			if(!strcmp(current->functionName,name))exit(0);
			prev=current;
			current=current->next;
		}
		if(!(current=(nativeFunc*)malloc(sizeof(nativeFunc))))errors(OUTOFMEMORY);
		if(!(current->functionName=(char*)malloc(sizeof(char)*(strlen(name)+1))))errors(OUTOFMEMORY);
		strcpy(current->functionName,name);
		current->function=pFun;
		current->next=NULL;
		prev->next=current;
	}
}

errorStatus (*(findFunc(const char* name)))(cptr*,env*)
{
	nativeFunc* current=funAndForm;
	while(current!=NULL&&strcmp(current->functionName,name))current=current->next;
	if(current==NULL)
		return NULL;
	else
		return current->function;
}



int retree(cptr** fir,cptr* sec)
{
	while(*fir!=sec)
	{
		cptr* preCptr=((*fir)->outer->prev==NULL)?sec:&(*fir)->outer->prev->car;
		replace(preCptr,*fir);
		*fir=preCptr;
		if(preCptr->outer!=NULL&&preCptr->outer->cdr.type!=NIL)return 0;
	}
	return 1;
}

void printList(const cptr* objCptr,FILE* out)
{
	if(objCptr==NULL)return;
	pair* tmpPair=(objCptr->type==PAR)?objCptr->value:NULL;
	pair* objPair=tmpPair;
	const cptr* tmp=objCptr;
	while(tmp!=NULL)
	{
		if(tmp->type==PAR)
		{
			if(objPair!=NULL&&tmp==&objPair->cdr)
			{
				putc(' ',out);
				objPair=objPair->cdr.value;
				tmp=&objPair->car;
			}
			else
			{
				putc('(',out);
				objPair=tmp->value;
				tmp=&objPair->car;
				continue;
			}
		}
		else if(tmp->type==ATM||tmp->type==NIL)
		{
			if(objPair!=NULL&&tmp==&objPair->cdr&&tmp->type==ATM)putc(',',out);
			if((objPair!=NULL&&tmp==&objPair->car&&tmp->type==NIL&&objPair->cdr.type!=NIL)
			||(tmp->outer==NULL&&tmp->type==NIL))fputs("nil",out);
			if(tmp->type!=NIL&&(((atom*)tmp->value)->type==SYM||((atom*)tmp->value)->type==NUM))
				fprintf(out,"%s",((atom*)tmp->value)->value);
			else if (tmp->type!=NIL)printRawString(((atom*)tmp->value)->value,out);
			if(objPair!=NULL&&tmp==&objPair->car)
			{
				tmp=&objPair->cdr;
				continue;
			}
		}
		if(objPair!=NULL&&tmp==&objPair->cdr)
		{
			putc(')',out);
			pair* prev=NULL;
			if(objPair->prev==NULL)break;
			while(objPair->prev!=NULL&&objPair!=tmpPair)
			{
				prev=objPair;
				objPair=objPair->prev;
				if(prev==objPair->car.value)break;
			}
			if(objPair!=NULL)tmp=&objPair->cdr;
			if(objPair==tmpPair&&prev==objPair->cdr.value)break;
		}
		if(objPair==NULL)break;
	}
}

defines* addDefine(const char* symName,const cptr* objCptr,env* curEnv)
{
	if(curEnv==NULL)errors(WRONGENV);
	if(curEnv->symbols==NULL)
	{
		if(!(curEnv->symbols=(defines*)malloc(sizeof(defines))))errors(OUTOFMEMORY);
		if(!(curEnv->symbols->symName=(char*)malloc(sizeof(char)*(strlen(symName)+1))))errors(OUTOFMEMORY);
		memcpy(curEnv->symbols->symName,symName,strlen(symName)+1);
		replace(&curEnv->symbols->obj,objCptr);
		curEnv->symbols->next=NULL;
		return curEnv->symbols;
	}
	else
	{
		defines* curSym=findDefine(symName,curEnv);
		if(curSym==NULL)
		{
			if(!(curSym=(defines*)malloc(sizeof(defines))))errors(OUTOFMEMORY);
			if(!(curSym->symName=(char*)malloc(sizeof(char)*(strlen(symName)+1))))errors(OUTOFMEMORY);
			memcpy(curSym->symName,symName,strlen(symName)+1);
			curSym->next=curEnv->symbols;
			curEnv->symbols=curSym;
			replace(&curSym->obj,objCptr);
		}
		else
			replace(&curSym->obj,objCptr);
		return curSym;
	}
}

env* newEnv(env* prev)
{
	env* curEnv=NULL;
	if(!(curEnv=(env*)malloc(sizeof(env))))errors(OUTOFMEMORY);
	curEnv->prev=prev;
	curEnv->symbols=NULL;
	return curEnv;
}

void destroyEnv(env* objEnv)
{
	if(objEnv==NULL)errors(WRONGENV);
	defines* delsym=objEnv->symbols;
	while(delsym!=NULL)
	{
		free(delsym->symName);
		deleteCptr(&delsym->obj);
		defines* prev=delsym;
		delsym=delsym->next;
		free(prev);
	}
	free(objEnv);
}

defines* findDefine(const char* name,const env* curEnv)
{
	const env* current=(curEnv==NULL)?Glob:curEnv;
	if(current->symbols==NULL)return NULL;
	else
	{
		defines* curSym=current->symbols;
		defines* prev=NULL;
		while(curSym&&strcmp(name,curSym->symName))
			curSym=curSym->next;
		return curSym;
	}
}

errorStatus eval(cptr* objCptr,env* curEnv)
{
	errorStatus status={0,NULL};
	curEnv=(curEnv==NULL)?(Glob=(Glob==NULL)?newEnv(NULL):Glob):curEnv;
	if(objCptr==NULL)
	{
		status.status=SYNTAXERROR;
		status.place=objCptr;
		return status;
	}
	curEnv=(curEnv==NULL)?Glob:curEnv;
	cptr* tmpCptr=objCptr;
	while(objCptr->type!=NIL)
	{
		if(objCptr->type==ATM)
		{
			atom* objAtm=objCptr->value;
			if(objCptr->outer==NULL||((objCptr->outer->prev!=NULL)&&(objCptr->outer->prev->cdr.value==objCptr->outer)))
			{
				if(objAtm->type!=SYM)break;
				cptr* reCptr=NULL;
				defines* objDef=NULL;
				env* tmpEnv=curEnv;
				while(!(objDef=findDefine(objAtm->value,tmpEnv))&&tmpEnv!=NULL)
					tmpEnv=tmpEnv->prev;
				if(objDef!=NULL)
				{
					reCptr=&objDef->obj;
					replace(objCptr,reCptr);
					break;
				}
				else
				{
					status.status=SYMUNDEFINE;
					status.place=objCptr;
					return status;
				}
			}
			else
			{
				int before=objCptr->outer->cdr.type;
				errorStatus (*pfun)(cptr*,env*)=NULL;
				pfun=findFunc(objAtm->value);
				if(pfun!=NULL)
				{
					status=pfun(objCptr,curEnv);
					if(status.status!=0)return status;
				}
				else
				{
					cptr* reCptr=NULL;
					defines* objDef=NULL;
					env* tmpEnv=curEnv;
					while(!(objDef=findDefine(objAtm->value,tmpEnv))&&tmpEnv!=NULL)
						tmpEnv=tmpEnv->prev;
					if(objDef!=NULL)
					{
						reCptr=&objDef->obj;
						replace(objCptr,reCptr);
						continue;
					}
					else 
					{
						status.status=SYMUNDEFINE;
						status.place=objCptr;
						return status;
					}
				}
			}
		}
		else if(objCptr->type==PAR)
		{
			if(!macroExpand(objCptr)&&hasKeyWord(objCptr))
			{
				status.status=SYNTAXERROR;
				status.place=objCptr;
				return status;
			}
			if(objCptr->type!=PAR)continue;
			objCptr=&(((pair*)objCptr->value)->car);
			continue;
		}
		if(retree(&objCptr,tmpCptr))break;
	}
	return status;
}

void exError(const cptr* obj,int type)
{
	printList(obj,stdout);
	switch(type)
	{
		case SYMUNDEFINE:printf(":Symbol is undefined.\n");break;
		case SYNTAXERROR:printf(":Syntax error.\n");break;
	}
}
int hasAlpha(const char* objStr)
{
	if(!isdigit(*objStr)&&*objStr!='-')return 1;
	const char* tmp=objStr+1;
	int len=strlen(objStr);
	for(;tmp<objStr+len;tmp++)
		if(!isdigit(*tmp))return 1;
	return 0;
}

int addKeyWord(const char* objStr)
{
	if(objStr!=NULL)
	{
		int len=strlen(objStr)+1;
		keyWord* tmp=(keyWord*)malloc(sizeof(keyWord));
		char* tmpStr=(char*)malloc(sizeof(char)*len);
		if(tmp==NULL||tmpStr==NULL)errors(OUTOFMEMORY);
		memcpy(tmpStr,objStr,len);
		tmp->word=tmpStr;
		tmp->next=KeyWords;
		KeyWords=tmp;
		return 1;
	}
	return 0;
}

keyWord* hasKeyWord(const cptr* objCptr)
{
	atom* tmpAtm=NULL;
	if(objCptr->type==ATM&&(tmpAtm=objCptr->value)->type==SYM)
	{
		keyWord* tmp=KeyWords;
		while(tmp!=NULL&&strcmp(tmpAtm->value,tmp->word))
			tmp=tmp->next;
		return tmp;
	}
	else if(objCptr->type==PAR)
	{
		keyWord* tmp=NULL;
		for(objCptr=&((pair*)objCptr->value)->car;objCptr!=NULL;objCptr=nextCptr(objCptr))
		{
			tmp=KeyWords;
			tmpAtm=(objCptr->type==ATM)?objCptr->value:NULL;
			atom* cdrAtm=(objCptr->outer->cdr.type==ATM)?objCptr->outer->cdr.value:NULL;
			if(tmpAtm==NULL&&cdrAtm==NULL)
			{
				tmp=NULL;
				continue;
			}
			while(tmp!=NULL&&tmpAtm!=NULL&&strcmp(tmpAtm->value,tmp->word)&&(cdrAtm==NULL||(cdrAtm==NULL&&strcmp(cdrAtm->value,tmp->word))))
				tmp=tmp->next;
		}
		return tmp;
	}
	return NULL;
}
