#include<string.h>
#include"preprocess.h"
#include"fake.h"
#include"tool.h"
static macro* Head=NULL;
static masym* First=NULL;
static env* MacroEnv=NULL;
macro* macroMatch(const cptr* objCptr)
{
	macro* current=Head;
	while(current!=NULL&&!fmatcmp(objCptr,current->format))current=current->next;
	return current;
}

int addMacro(cptr* format,cptr* express)
{
	if(format->type!=PAR)return SYNTAXERROR;
	cptr* tmpCptr=NULL;
	for(tmpCptr=&((pair*)format->value)->car;tmpCptr!=NULL;tmpCptr=nextCptr(tmpCptr))
	{
		if(tmpCptr->type==ATM)
		{
			atom* carAtm=tmpCptr->value;
			atom* cdrAtm=(tmpCptr->outer->cdr.type==ATM)?tmpCptr->outer->cdr.value:NULL;
			if(carAtm->type==SYM&&!hasAnotherName(carAtm->value))addKeyWord(carAtm->value);
			if(cdrAtm!=NULL&&cdrAtm->type==SYM&&!hasAnotherName(cdrAtm->value))addKeyWord(carAtm->value);
		}
	}
	macro* current=Head;
	while(current!=NULL&&!cptrcmp(format,current->format))
		current=current->next;
	if(current==NULL)
	{
		if(!(current=(macro*)malloc(sizeof(macro))))errors(OUTOFMEMORY);
		current->format=format;
		current->express=express;
		current->next=Head;
		Head=current;
	}
	else
		current->express=express;
	return 0;
}

masym* findMasym(const char* name)
{
	masym* current=First;
	const char* anotherName=hasAnotherName(name);
	int len=(anotherName==NULL)?strlen(name):(anotherName-name-1);
	while(current!=NULL&&memcmp(current->symName,name,len))current=current->next;
	return current;
}

int fmatcmp(const cptr* origin,const cptr* format)
{
	MacroEnv=newEnv(NULL);
	pair* tmpPair=(format->type==PAR)?format->value:NULL;
	pair* forPair=tmpPair;
	pair* oriPair=(origin->type==PAR)?origin->value:NULL;
	while(origin!=NULL)
	{
		if(format->type==PAR&&origin->type==PAR)
		{
			forPair=format->value;
			oriPair=origin->value;
			format=&forPair->car;
			origin=&oriPair->car;
			continue;
		}
		else if(format->type==ATM)
		{
			if(format->type==ATM)
			{
				atom* tmpAtm=format->value;
				masym* tmpSym=findMasym(tmpAtm->value);
				if(tmpSym==NULL||!hasAnotherName(tmpAtm->value))
				{
					if(!cptrcmp(origin,format))
					{
						destroyEnv(MacroEnv);
						MacroEnv=NULL;
						return 0;
					}
				}
				else if(!tmpSym->Func(origin,format,hasAnotherName(tmpAtm->value),MacroEnv))
				{
					destroyEnv(MacroEnv);
					MacroEnv=NULL;
					return 0;
				}
				forPair=format->outer;
				if(tmpSym!=NULL&&tmpSym->Func==M_VAREPT)
				{
					format=&forPair->cdr;
					origin=&oriPair->cdr;
				}
			}
			if(forPair!=NULL&&format==&forPair->car)
			{
				origin=&oriPair->cdr;
				format=&forPair->cdr;
				continue;
			}
		}
		else if(origin->type==NIL&&
				format->type==PAR&&
				((pair*)format->value)->car.type==ATM)
		{
			atom* tmpAtm=((pair*)format->value)->car.value;
			masym* tmpSym=findMasym(tmpAtm->value);
			if(tmpSym->Func!=M_VAREPT)
			{
				destroyEnv(MacroEnv);
				MacroEnv=NULL;
				return 0;
			}
			else
			{
				M_VAREPT(&origin->outer->car,&((pair*)format->value)->car,NULL,MacroEnv);
			}
		}
		else if(origin->type!=format->type)
		{
			destroyEnv(MacroEnv);
			MacroEnv=NULL;
			return 0;
		}


		if(forPair!=NULL&&format==&forPair->car)
		{
			origin=&oriPair->cdr;
			format=&forPair->cdr;
			continue;
		}
		else if(forPair!=NULL&&format==&forPair->cdr)
		{
			pair* oriPrev=NULL;
			pair* forPrev=NULL;
			if(oriPair->prev==NULL)break;
			while(oriPair->prev!=NULL&&forPair!=tmpPair)
			{
				oriPrev=oriPair;
				forPrev=forPair;
				oriPair=oriPair->prev;
				forPair=forPair->prev;
				if(oriPrev==oriPair->car.value)break;
			}
			if(oriPair!=NULL)
			{
				origin=&oriPair->cdr;
				format=&forPair->cdr;
			}
			if(forPair==tmpPair&&format==&forPair->cdr)break;
		}
		if(oriPair==NULL&&forPair==NULL)break;
	}
	return 1;
}

int macroExpand(cptr* objCptr)
{
	errorStatus status={0,NULL};
	macro* tmp=macroMatch(objCptr);
	if(tmp!=NULL&&objCptr!=tmp->express)
	{
		cptr* tmpCptr=createCptr(NULL);
		replace(tmpCptr,tmp->express);
		status=eval(tmp->express,MacroEnv);
		if(status.status!=0)exError(status.place,status.status);
		replace(objCptr,tmp->express);
		deleteCptr(tmp->express);
		free(tmp->express);
		tmp->express=tmpCptr;
		return 1;
	}
	return 0;
}

void initPreprocess()
{
	addRule("ATOM",M_ATOM);
	addRule("PAIR",M_PAIR);
	addRule("ANY",M_ANY);
	addRule("VAREPT",M_VAREPT);
	MacroEnv=newEnv(NULL);
	
}
void addRule(const char* name,int (*obj)(const cptr*,const cptr*,const char*,env*))
{
	masym* current=NULL;
	if(!(current=(masym*)malloc(sizeof(masym))))errors(OUTOFMEMORY);
	if(!(current->symName=(char*)malloc(sizeof(char)*(strlen(name)+1))))errors(OUTOFMEMORY);
	current->next=First;
	First=current;
	strcpy(current->symName,name);
	current->Func=obj;
}

int M_ATOM(const cptr* oriCptr,const cptr* fmtCptr,const char* name,env* curEnv)
{
	if(oriCptr==NULL)return 0;
	else if(oriCptr->type==ATM)
	{
		addDefine(name,oriCptr,curEnv);
		return 1;
	}
	return 0;
}

int M_PAIR(const cptr* oriCptr,const cptr* fmtCptr,const char* name,env* curEnv)
{
	if(oriCptr==NULL)return 0;
	else if(oriCptr->type==PAR)
	{
		addDefine(name,oriCptr,curEnv);
		return 1;
	}
	return 0;
}

int M_ANY(const cptr* oriCptr,const cptr* fmtCptr,const char* name,env* curEnv)
{
	if(oriCptr==NULL)return 0;
	else
	{
		addDefine(name,oriCptr,curEnv);
		return 1;
	}
	return 0;
}

int M_VAREPT(const cptr* oriCptr,const cptr* fmtCptr,const char* name,env* curEnv)
{
	fmtCptr=&fmtCptr->outer->prev->car;
	const cptr* format=fmtCptr;
	const cptr tmp={NULL,NIL,NULL};
	pair* forPair=(format->type==PAR)?format->value:NULL;
	pair* tmpPair=forPair;
	env* forValRept=newEnv(NULL);
	defines* valreptdef=addDefine(name,&tmp,MacroEnv);
	while(oriCptr!=NULL&&fmtCptr!=NULL)
	{
		const cptr* origin=oriCptr;
		const cptr* format=fmtCptr;
		pair* forPair=(format->type==PAR)?format->value:NULL;
		pair* oriPair=(origin->type==PAR)?origin->value:NULL;
		while(origin!=NULL&&format!=NULL)
		{
			if(format->type==PAR&&origin->type==PAR)
			{
				forPair=format->value;
				oriPair=origin->value;
				format=&forPair->car;
				origin=&oriPair->car;
				continue;
			}
			else if(format->type==ATM)
			{
				atom* tmpAtm=format->value;
				masym* tmpSym=findMasym(tmpAtm->value);
				const char* anotherName=hasAnotherName(tmpAtm->value);
				if(tmpSym==NULL||tmpSym->Func==M_VAREPT||!anotherName)
				{
					if(!cptrcmp(origin,format))
					{
						destroyEnv(forValRept);
						return 0;
					}
				}
				else
				{
					if(tmpSym->Func(origin,format,anotherName,forValRept))
					{
						forPair=(format==&fmtCptr->outer->prev->car)?NULL:format->outer;
						int len=strlen(name)+strlen(anotherName)+2;
						char symName[len];
						strcpy(symName,name);
						strcat(symName,"#");
						strcat(symName,anotherName);
						defines* tmpDef=findDefine(symName,MacroEnv);
						if(tmpDef==NULL)tmpDef=addDefine(symName,&tmp,MacroEnv);
						addToList(&tmpDef->obj,&findDefine(anotherName,forValRept)->obj);
					}
					else break;
				}
				if(forPair!=NULL&&format==&forPair->car)
				{
					origin=&oriPair->cdr;
					format=&forPair->cdr;
					continue;
				}
			}
			else if(origin->type!=format->type)break;
			if(forPair!=NULL&&format==&forPair->car)
			{
				origin=&oriPair->cdr;
				format=&forPair->cdr;
				continue;
			}
			else if(forPair!=NULL&&format==&forPair->cdr)
			{
				pair* oriPrev=NULL;
				pair* forPrev=NULL;
				if(oriPair->prev==NULL)break;
				while(oriPair->prev!=NULL&&forPair!=tmpPair)
				{
					oriPrev=oriPair;
					forPrev=forPair;
					oriPair=oriPair->prev;
					forPair=forPair->prev;
					if(oriPrev==oriPair->car.value)break;
				}
				if(oriPair!=NULL)
				{
					origin=&oriPair->cdr;
					format=&forPair->cdr;
				}
				if(forPair==tmpPair&&format==&forPair->cdr)break;
			}
			if(forPair==NULL)break;
		}
		destroyEnv(forValRept);
		addToList(&valreptdef->obj,oriCptr);
		oriCptr=nextCptr(oriCptr);
	}
	return 1;
}

int M_COLT(const cptr* oriCptr,const cptr* fmtCptr,const char* name,env* curEnv)
{
}

void deleteMacro(cptr* objCptr)
{
	macro* current=Head;
	macro* prev=NULL;
	while(current!=NULL&&!cptrcmp(objCptr,current->format))
	{
		prev=current;
		current=current->next;
	}
	if(current!=NULL)
	{
		if(current==Head)Head=current->next;
		deleteCptr(current->format);
		deleteCptr(current->express);
		free(current->format);
		free(current->express);
		if(prev!=NULL)prev->next=current->next;
		free(current);
	}
}

const char* hasAnotherName(const char* name)
{
	int len=strlen(name);
	const char* tmp=name;
	for(;tmp<name+len;tmp++)
		if(*tmp=='#'&&(*(tmp-1)!='#'||*(tmp+1)!='#'))break;
	return (tmp==name+len)?NULL:tmp+1;
}

void addToList(cptr* fir,const cptr* sec)
{
	while(fir->type!=NIL)fir=&((pair*)fir->value)->cdr;
	fir->type=PAR;
	fir->value=createPair(fir->outer);
	replace(&((pair*)fir->value)->car,sec);
}
