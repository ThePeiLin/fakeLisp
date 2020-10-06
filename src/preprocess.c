#include<string.h>
#include"preprocess.h"
#include"tool.h"
static macro* FirstMacro=NULL;
static masym* FirstMasym=NULL;
static env* MacroEnv=NULL;
static nativeFunc* funAndForm=NULL;
static keyWord* KeyWords=NULL;



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

defines* addDefine(const char* symName,const cptr* objCptr,env* curEnv)
{
	if(curEnv->symbols==NULL)
	{
		curEnv->symbols=newDefines(symName);
		replace(&curEnv->symbols->obj,objCptr);
		curEnv->symbols->next=NULL;
		return curEnv->symbols;
	}
	else
	{
		defines* curDef=findDefine(symName,curEnv);
		if(curDef==NULL)
		{
			curDef=newDefines(symName);
			curDef->next=curEnv->symbols;
			curEnv->symbols=curDef;
			replace(&curDef->obj,objCptr);
		}
		else
			replace(&curDef->obj,objCptr);
		return curDef;
	}
}


defines* findDefine(const char* name,const env* curEnv)
{
	if(curEnv->symbols==NULL)return NULL;
	else
	{
		defines* curDef=curEnv->symbols;
		defines* prev=NULL;
		while(curDef&&strcmp(name,curDef->symName))
			curDef=curDef->next;
		return curDef;
	}
}

errorStatus eval(cptr* objCptr,env* curEnv)
{
	errorStatus status={0,NULL};
	if(objCptr==NULL)
		return status;
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
				while(tmpEnv!=NULL&&!(objDef=findDefine(objAtm->value.str,tmpEnv)))
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
				if(objAtm->type!=SYM)
				{
					status.status=SYNTAXERROR;
					status.place=objCptr;
					return status;
				}
				int before=objCptr->outer->cdr.type;
				errorStatus (*pfun)(cptr*,env*)=NULL;
				pfun=findFunc(objAtm->value.str);
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
					while(tmpEnv!=NULL&&!(objDef=findDefine(objAtm->value.str,tmpEnv)))
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
			if(objCptr->type!=PAR)continue;
			objCptr=&(((pair*)objCptr->value)->car);
			continue;
		}
		if(retree(&objCptr,tmpCptr))break;
	}
	return status;
}

void addKeyWord(const char* objStr)
{
	if(objStr!=NULL)
	{
		keyWord* current=KeyWords;
		keyWord* prev=NULL;
		while(current!=NULL&&strcmp(current->word,objStr)){prev=current;current=current->next;}
		if(current==NULL)
		{
			current=(keyWord*)malloc(sizeof(keyWord));
			current->word=(char*)malloc(sizeof(char)*strlen(objStr));
			if(current==NULL||current->word==NULL)errors(OUTOFMEMORY);
			strcpy(current->word,objStr);
			if(prev!=NULL)prev->next=current;
			else KeyWords=current;
			current->next=NULL;
		}
	}
}

keyWord* hasKeyWord(const cptr* objCptr)
{
	atom* tmpAtm=NULL;
	if(objCptr->type==ATM&&(tmpAtm=objCptr->value)->type==SYM)
	{
		keyWord* tmp=KeyWords;
		while(tmp!=NULL&&strcmp(tmpAtm->value.str,tmp->word))
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
			if((tmpAtm==NULL||tmpAtm->type!=SYM)&&(cdrAtm==NULL||cdrAtm->type!=SYM))
			{
				tmp=NULL;
				continue;
			}
			while(tmp!=NULL&&tmpAtm!=NULL&&strcmp(tmpAtm->value.str,tmp->word)&&(cdrAtm==NULL||cdrAtm->type!=SYM||(cdrAtm!=NULL&&strcmp(cdrAtm->value.str,tmp->word))))
				tmp=tmp->next;
			if(tmp!=NULL)break;
		}
		return tmp;
	}
	return NULL;
}

void printAllKeyWord()
{
	keyWord* tmp=KeyWords;
	while(tmp!=NULL)
	{
		puts(tmp->word);
		tmp=tmp->next;
	}
}

macro* macroMatch(const cptr* objCptr)
{
	macro* current=FirstMacro;
	while(current!=NULL&&!fmatcmp(objCptr,current->format))
		current=current->next;
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
			if(carAtm->type==SYM)
			{
				if(!hasAnotherName(carAtm->value.str))addKeyWord(carAtm->value.str);
				else if(hasAnotherName(carAtm->value.str)&&!memcmp(carAtm->value.str,"COLT",4))
				{
					const char* name=hasAnotherName(carAtm->value.str);
					int i;
					int num=getWordNum(name);
					for(i=0;i<num;i++)
					{
						char* word=getWord(name,i+1);
						addKeyWord(word);
						free(word);
					}
				}
			}
			if(cdrAtm!=NULL&&cdrAtm->type==SYM)
			{
				if(!hasAnotherName(cdrAtm->value.str))addKeyWord(cdrAtm->value.str);
				else if(hasAnotherName(cdrAtm->value.str)&&!memcmp(cdrAtm->value.str,"COLT",4))
				{
					const char* name=hasAnotherName(cdrAtm->value.str);
					int i;
					int num=getWordNum(name);
					for(i=0;i<num;i++)
					{
						char* word=getWord(name,i+1);
						addKeyWord(word);
						free(word);
					}
				}
			}
		}
	}
	macro* current=FirstMacro;
	while(current!=NULL&&!cptrcmp(format,current->format))
		current=current->next;
	if(current==NULL)
	{
		if(!(current=(macro*)malloc(sizeof(macro))))errors(OUTOFMEMORY);
		current->format=format;
		current->express=express;
		current->next=FirstMacro;
		FirstMacro=current;
	}
	else
		current->express=express;
	//printAllKeyWord();
	return 0;
}

masym* findMasym(const char* name)
{
	masym* current=FirstMasym;
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
			atom* tmpAtm=format->value;
			masym* tmpSym=NULL;
			if(tmpAtm->type==SYM)
			{
				tmpSym=findMasym(tmpAtm->value.str);
				if(tmpSym==NULL||!hasAnotherName(tmpAtm->value.str))
				{
					if(!cptrcmp(origin,format))
					{
						destroyEnv(MacroEnv);
						MacroEnv=NULL;
						return 0;
					}
				}
				else if(!tmpSym->Func(origin,format,hasAnotherName(tmpAtm->value.str),MacroEnv))
				{
					destroyEnv(MacroEnv);
					MacroEnv=NULL;
					return 0;
				}
			}
			forPair=format->outer;
			oriPair=origin->outer;
			if(tmpSym!=NULL&&tmpSym->Func==M_VAREPT)
			{
				format=&forPair->cdr;
				origin=&oriPair->cdr;
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
			masym* tmpSym=findMasym(tmpAtm->value.str);
			if(tmpSym!=NULL&&tmpSym->Func!=M_VAREPT)
			{
				destroyEnv(MacroEnv);
				MacroEnv=NULL;
				return 0;
			}
			else if(tmpSym!=NULL)
			{
				cptr* tmp=&((pair*)format->value)->car;
				atom* tmpAtm=tmp->value;
				M_VAREPT(NULL,tmp,hasAnotherName(tmpAtm->value.str),MacroEnv);
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
		addDefine("RAWEXPRESS",objCptr,MacroEnv);
		cptr* tmpCptr=newCptr(0,NULL);
		replace(tmpCptr,tmp->express);
		status=eval(tmp->express,MacroEnv);
		if(status.status!=0)exError(status.place,status.status,NULL);
		replace(objCptr,tmp->express);
		deleteCptr(tmp->express);
		free(tmp->express);
		tmp->express=tmpCptr;
		return 1;
	}
	return 0;
}

void initPreprocess(intpr* inter)
{
	addRule("ATOM",M_ATOM);
	addRule("PAIR",M_PAIR);
	addRule("ANY",M_ANY);
	addRule("VAREPT",M_VAREPT);
	addRule("COLT",M_COLT);
	MacroEnv=newEnv(NULL);
	addKeyWord("define");
	addKeyWord("setq");
	addKeyWord("quote");
	addKeyWord("cond");
	addKeyWord("and");
	addKeyWord("or");
	addKeyWord("lambda");
	addMacro(createTree("(define ATOM#NAME ANY#VALUE)",inter),
			createTree("RAWEXPRESS",inter));
	addMacro(createTree("(setq ATOM#NAME ANY#VALUE)",inter),
			createTree("RAWEXPRESS",inter));
	addMacro(createTree("(quote ANY#VALUE)",inter),
			createTree("RAWEXPRESS",inter));
	addMacro(createTree("(cond (ANY#COND,ANY#BODY) VAREPT#ANOTHER)",inter),
			createTree("RAWEXPRESS",inter));
	addMacro(createTree("(and,ANY#OTHER)",inter),
			createTree("RAWEXPRESS",inter));
	addMacro(createTree("(or,ANY#OTHER)",inter),
			createTree("RAWEXPRESS",inter));
	addMacro(createTree("(lambda ANY#ARGS,PAIR#BODY)",inter),
			createTree("RAWEXPRESS",inter));
}
void addRule(const char* name,int (*obj)(const cptr*,const cptr*,const char*,env*))
{
	masym* current=NULL;
	if(!(current=(masym*)malloc(sizeof(masym))))errors(OUTOFMEMORY);
	if(!(current->symName=(char*)malloc(sizeof(char)*(strlen(name)+1))))errors(OUTOFMEMORY);
	current->next=FirstMasym;
	FirstMasym=current;
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
	const cptr tmp={NULL,0,NIL,NULL};
	pair* forPair=(format->type==PAR)?format->value:NULL;
	pair* tmpPair=forPair;
	defines* valreptdef=addDefine(name,&tmp,MacroEnv);
	if(oriCptr==NULL)
	{
		const cptr* format=fmtCptr;
		pair* forPair=(format->type==PAR)?format->value:NULL;
		env* forValRept=newEnv(NULL);
		while(format!=NULL)
		{
			if(format->type==PAR)
			{
				forPair=format->value;
				format=&forPair->car;
				continue;
			}
			else if(format->type==ATM)
			{
				atom* tmpAtm=format->value;
				if(tmpAtm->type==SYM)
				{
					const char* anotherName=hasAnotherName(tmpAtm->value.str);
					if(anotherName)
					{
						forPair=(format==&fmtCptr->outer->prev->car)?NULL:format->outer;
						int len=strlen(name)+strlen(anotherName)+2;
						char symName[len];
						strcpy(symName,name);
						strcat(symName,"#");
						strcat(symName,anotherName);
						defines* tmpDef=findDefine(symName,MacroEnv);
						if(tmpDef==NULL)tmpDef=addDefine(symName,&tmp,MacroEnv);
					}
				}
				if(forPair!=NULL&&format==&forPair->car)
				{
					format=&forPair->cdr;
					continue;
				}
			}
			if(forPair!=NULL&&format==&forPair->car)
			{
				format=&forPair->cdr;
				continue;
			}
			else if(forPair!=NULL&&format==&forPair->cdr)
			{
				pair* forPrev=NULL;
				while(forPair!=tmpPair)
				{
					forPrev=forPair;
					forPair=forPair->prev;
					if(forPair==forPair->car.value)break;
				}
				if(forPair!=NULL)
					format=&forPair->cdr;
				if(forPair==tmpPair&&format==&forPair->cdr)break;
			}
			if(forPair==NULL)break;
		}
		destroyEnv(forValRept);
	}
	while(oriCptr!=NULL&&fmtCptr!=NULL)
	{
		const cptr* origin=oriCptr;
		const cptr* format=fmtCptr;
		pair* forPair=(format->type==PAR)?format->value:NULL;
		pair* oriPair=(origin->type==PAR)?origin->value:NULL;
		env* forValRept=newEnv(NULL);
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
				if(tmpAtm->type==SYM)
				{
					masym* tmpSym=findMasym(tmpAtm->value.str);
					const char* anotherName=hasAnotherName(tmpAtm->value.str);
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
		const cptr* tmp=oriCptr;
		oriCptr=nextCptr(tmp);
	}
	return 1;
}

int M_COLT(const cptr* oriCptr,const cptr* fmtCptr,const char* name,env* curEnv)
{
	int i;
	int num=getWordNum(name);
	atom* tmpAtm=(oriCptr->type==ATM)?oriCptr->value:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type==SYM)
		for(i=0;i<num;i++)
		{
			char* word=getWord(name,i+1);
			if(!strcmp(word,tmpAtm->value.str))
			{
				char* tmpName=getWord(name,0);
				addDefine(tmpName,oriCptr,curEnv);
				free(word);
				free(tmpName);
				return 1;
			}
		}
	return 0;
}

void deleteMacro(cptr* objCptr)
{
	macro* current=FirstMacro;
	macro* prev=NULL;
	while(current!=NULL&&!cptrcmp(objCptr,current->format))
	{
		prev=current;
		current=current->next;
	}
	if(current!=NULL)
	{
		if(current==FirstMacro)FirstMacro=current->next;
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
	fir->value=newPair(0,fir->outer);
	replace(&((pair*)fir->value)->car,sec);
}

int getWordNum(const char* name)
{
	int i;
	int num=0;
	int len=strlen(name);
	for(i=0;i<len-1;i++)
		if(name[i]=='#')num++;
	return num;
}

char* getWord(const char* name,int num)
{
	int len=strlen(name);
	int tmpNum=0;
	int tmplen=0;
	char* tmpStr=NULL;
	int i;
	int j;
	for(i=0;i<len&&tmpNum<num;i++)
		if(name[i]=='#')tmpNum++;
	for(j=i;j<len;j++)
	{
		if(name[j]!='#')tmplen++;
		else break;
	}
	if(!(tmpStr=(char*)malloc(sizeof(char)*(tmplen+1))))errors(OUTOFMEMORY);
	strncpy(tmpStr,name+i,tmplen);
	tmpStr[tmplen]='\0';
	return tmpStr;
}

defines* newDefines(const char* name)
{
	defines* tmp=(defines*)malloc(sizeof(defines));
	if(tmp==NULL)errors(OUTOFMEMORY);
	tmp->symName=(char*)malloc(sizeof(char)*(strlen(name)+1));
	if(tmp->symName==NULL)errors(OUTOFMEMORY);
	strcpy(tmp->symName,name);
	tmp->obj=(cptr){NULL,0,NIL,NULL};
	tmp->next=NULL;
	return tmp;
}
