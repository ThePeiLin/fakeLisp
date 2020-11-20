#include<string.h>
#include"preprocess.h"
#include"tool.h"
static PreMacro* FirstMacro=NULL;
static PreMasym* FirstMasym=NULL;
static PreEnv* MacroEnv=NULL;
static PreFunc* funAndForm=NULL;
static KeyWord* KeyWords=NULL;



void addFunc(const char* name,ErrorStatus (*pFun)(ANS_cptr*,PreEnv*))
{
	PreFunc* current=funAndForm;
	PreFunc* prev=NULL;
	if(current==NULL)
	{
		if(!(funAndForm=(PreFunc*)malloc(sizeof(PreFunc))))errors(OUTOFMEMORY,__FILE__,__LINE__);
		if(!(funAndForm->functionName=(char*)malloc(sizeof(char)*(strlen(name)+1))))errors(OUTOFMEMORY,__FILE__,__LINE__);
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
		if(!(current=(PreFunc*)malloc(sizeof(PreFunc))))errors(OUTOFMEMORY,__FILE__,__LINE__);
		if(!(current->functionName=(char*)malloc(sizeof(char)*(strlen(name)+1))))errors(OUTOFMEMORY,__FILE__,__LINE__);
		strcpy(current->functionName,name);
		current->function=pFun;
		current->next=NULL;
		prev->next=current;
	}
}

ErrorStatus (*(findFunc(const char* name)))(ANS_cptr*,PreEnv*)
{
	PreFunc* current=funAndForm;
	while(current!=NULL&&strcmp(current->functionName,name))current=current->next;
	if(current==NULL)
		return NULL;
	else
		return current->function;
}



int retree(ANS_cptr** fir,ANS_cptr* sec)
{
	while(*fir!=sec)
	{
		ANS_cptr* preCptr=((*fir)->outer->prev==NULL)?sec:&(*fir)->outer->prev->car;
		replace(preCptr,*fir);
		*fir=preCptr;
		if(preCptr->outer!=NULL&&preCptr->outer->cdr.type!=NIL)return 0;
	}
	return 1;
}

PreDef* addDefine(const char* symName,const ANS_cptr* objCptr,PreEnv* curEnv)
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
		PreDef* curDef=findDefine(symName,curEnv);
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


PreDef* findDefine(const char* name,const PreEnv* curEnv)
{
	if(curEnv->symbols==NULL)return NULL;
	else
	{
		PreDef* curDef=curEnv->symbols;
		PreDef* prev=NULL;
		while(curDef&&strcmp(name,curDef->symName))
			curDef=curDef->next;
		return curDef;
	}
}

ErrorStatus eval(ANS_cptr* objCptr,PreEnv* curEnv)
{
	ErrorStatus status={0,NULL};
	if(objCptr==NULL)
		return status;
	ANS_cptr* tmpCptr=objCptr;
	while(objCptr->type!=NIL)
	{
		if(objCptr->type==ATM)
		{
			ANS_atom* objAtm=objCptr->value;
			if(objCptr->outer==NULL||((objCptr->outer->prev!=NULL)&&(objCptr->outer->prev->cdr.value==objCptr->outer)))
			{
				if(objAtm->type!=SYM)break;
				ANS_cptr* reCptr=NULL;
				PreDef* objDef=NULL;
				PreEnv* tmpEnv=curEnv;
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
				ErrorStatus (*pfun)(ANS_cptr*,PreEnv*)=NULL;
				pfun=findFunc(objAtm->value.str);
				if(pfun!=NULL)
				{
					status=pfun(objCptr,curEnv);
					if(status.status!=0)return status;
				}
				else
				{
					ANS_cptr* reCptr=NULL;
					PreDef* objDef=NULL;
					PreEnv* tmpEnv=curEnv;
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
		else if(objCptr->type==PAIR)
		{
			if(objCptr->type!=PAIR)continue;
			objCptr=&(((ANS_pair*)objCptr->value)->car);
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
		KeyWord* current=KeyWords;
		KeyWord* prev=NULL;
		while(current!=NULL&&strcmp(current->word,objStr)){prev=current;current=current->next;}
		if(current==NULL)
		{
			current=(KeyWord*)malloc(sizeof(KeyWord));
			current->word=(char*)malloc(sizeof(char)*strlen(objStr));
			if(current==NULL||current->word==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
			strcpy(current->word,objStr);
			if(prev!=NULL)prev->next=current;
			else KeyWords=current;
			current->next=NULL;
		}
	}
}

KeyWord* hasKeyWord(const ANS_cptr* objCptr)
{
	ANS_atom* tmpAtm=NULL;
	if(objCptr->type==ATM&&(tmpAtm=objCptr->value)->type==SYM)
	{
		KeyWord* tmp=KeyWords;
		while(tmp!=NULL&&strcmp(tmpAtm->value.str,tmp->word))
			tmp=tmp->next;
		return tmp;
	}
	else if(objCptr->type==PAIR)
	{
		KeyWord* tmp=NULL;
		for(objCptr=&((ANS_pair*)objCptr->value)->car;objCptr!=NULL;objCptr=nextCptr(objCptr))
		{
			tmp=KeyWords;
			tmpAtm=(objCptr->type==ATM)?objCptr->value:NULL;
			ANS_atom* cdrAtm=(objCptr->outer->cdr.type==ATM)?objCptr->outer->cdr.value:NULL;
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
	KeyWord* tmp=KeyWords;
	while(tmp!=NULL)
	{
		puts(tmp->word);
		tmp=tmp->next;
	}
}

PreMacro* PreMacroMatch(const ANS_cptr* objCptr)
{
	PreMacro* current=FirstMacro;
	while(current!=NULL&&!fmatcmp(objCptr,current->format))
		current=current->next;
	return current;
}

int addMacro(ANS_cptr* format,ANS_cptr* express)
{
	if(format->type!=PAIR)return SYNTAXERROR;
	ANS_cptr* tmpCptr=NULL;
	for(tmpCptr=&((ANS_pair*)format->value)->car;tmpCptr!=NULL;tmpCptr=nextCptr(tmpCptr))
	{
		if(tmpCptr->type==ATM)
		{
			ANS_atom* carAtm=tmpCptr->value;
			ANS_atom* cdrAtm=(tmpCptr->outer->cdr.type==ATM)?tmpCptr->outer->cdr.value:NULL;
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
	PreMacro* current=FirstMacro;
	while(current!=NULL&&!ANS_cptrcmp(format,current->format))
		current=current->next;
	if(current==NULL)
	{
		if(!(current=(PreMacro*)malloc(sizeof(PreMacro))))errors(OUTOFMEMORY,__FILE__,__LINE__);
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

PreMasym* findMasym(const char* name)
{
	PreMasym* current=FirstMasym;
	const char* anotherName=hasAnotherName(name);
	int64_t len=0;if(anotherName==NULL)len=strlen(name);else len=(anotherName-name-1);
	while(current!=NULL&&memcmp(current->symName,name,len))current=current->next;
	return current;
}

int fmatcmp(const ANS_cptr* origin,const ANS_cptr* format)
{
	MacroEnv=newEnv(NULL);
	ANS_pair* tmpPair=(format->type==PAIR)?format->value:NULL;
	ANS_pair* forPair=tmpPair;
	ANS_pair* oriPair=(origin->type==PAIR)?origin->value:NULL;
	while(origin!=NULL)
	{
		if(format->type==PAIR&&origin->type==PAIR)
		{
			forPair=format->value;
			oriPair=origin->value;
			format=&forPair->car;
			origin=&oriPair->car;
			continue;
		}
		else if(format->type==ATM)
		{
			ANS_atom* tmpAtm=format->value;
			PreMasym* tmpSym=NULL;
			if(tmpAtm->type==SYM)
			{
				tmpSym=findMasym(tmpAtm->value.str);
				if(tmpSym==NULL||!hasAnotherName(tmpAtm->value.str))
				{
					if(!ANS_cptrcmp(origin,format))
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
				format->type==PAIR&&
				((ANS_pair*)format->value)->car.type==ATM)
		{
			ANS_atom* tmpAtm=((ANS_pair*)format->value)->car.value;
			PreMasym* tmpSym=findMasym(tmpAtm->value.str);
			if(tmpSym!=NULL&&tmpSym->Func!=M_VAREPT)
			{
				destroyEnv(MacroEnv);
				MacroEnv=NULL;
				return 0;
			}
			else if(tmpSym!=NULL)
			{
				ANS_cptr* tmp=&((ANS_pair*)format->value)->car;
				ANS_atom* tmpAtm=tmp->value;
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
			ANS_pair* oriPrev=NULL;
			ANS_pair* forPrev=NULL;
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

int PreMacroExpand(ANS_cptr* objCptr)
{
	ErrorStatus status={0,NULL};
	PreMacro* tmp=PreMacroMatch(objCptr);
	if(tmp!=NULL&&objCptr!=tmp->express)
	{
		ANS_cptr* tmpCptr=newCptr(0,NULL);
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
	addKeyWord("setf");
	addKeyWord("load");
}
void addRule(const char* name,int (*obj)(const ANS_cptr*,const ANS_cptr*,const char*,PreEnv*))
{
	PreMasym* current=NULL;
	if(!(current=(PreMasym*)malloc(sizeof(PreMasym))))errors(OUTOFMEMORY,__FILE__,__LINE__);
	if(!(current->symName=(char*)malloc(sizeof(char)*(strlen(name)+1))))errors(OUTOFMEMORY,__FILE__,__LINE__);
	current->next=FirstMasym;
	FirstMasym=current;
	strcpy(current->symName,name);
	current->Func=obj;
}

int M_ATOM(const ANS_cptr* oriCptr,const ANS_cptr* fmtCptr,const char* name,PreEnv* curEnv)
{
	if(oriCptr==NULL)return 0;
	else if(oriCptr->type==ATM)
	{
		addDefine(name,oriCptr,curEnv);
		return 1;
	}
	return 0;
}

int M_PAIR(const ANS_cptr* oriCptr,const ANS_cptr* fmtCptr,const char* name,PreEnv* curEnv)
{
	if(oriCptr==NULL)return 0;
	else if(oriCptr->type==PAIR)
	{
		addDefine(name,oriCptr,curEnv);
		return 1;
	}
	return 0;
}

int M_ANY(const ANS_cptr* oriCptr,const ANS_cptr* fmtCptr,const char* name,PreEnv* curEnv)
{
	if(oriCptr==NULL)return 0;
	else
	{
		addDefine(name,oriCptr,curEnv);
		return 1;
	}
	return 0;
}

int M_VAREPT(const ANS_cptr* oriCptr,const ANS_cptr* fmtCptr,const char* name,PreEnv* curEnv)
{
	fmtCptr=&fmtCptr->outer->prev->car;
	const ANS_cptr* format=fmtCptr;
	const ANS_cptr tmp={NULL,0,NIL,NULL};
	ANS_pair* forPair=(format->type==PAIR)?format->value:NULL;
	ANS_pair* tmpPair=forPair;
	PreDef* valreptdef=addDefine(name,&tmp,MacroEnv);
	if(oriCptr==NULL)
	{
		const ANS_cptr* format=fmtCptr;
		ANS_pair* forPair=(format->type==PAIR)?format->value:NULL;
		PreEnv* forValRept=newEnv(NULL);
		while(format!=NULL)
		{
			if(format->type==PAIR)
			{
				forPair=format->value;
				format=&forPair->car;
				continue;
			}
			else if(format->type==ATM)
			{
				ANS_atom* tmpAtm=format->value;
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
						PreDef* tmpDef=findDefine(symName,MacroEnv);
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
				ANS_pair* forPrev=NULL;
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
		const ANS_cptr* origin=oriCptr;
		const ANS_cptr* format=fmtCptr;
		ANS_pair* forPair=(format->type==PAIR)?format->value:NULL;
		ANS_pair* oriPair=(origin->type==PAIR)?origin->value:NULL;
		PreEnv* forValRept=newEnv(NULL);
		while(origin!=NULL&&format!=NULL)
		{
			if(format->type==PAIR&&origin->type==PAIR)
			{
				forPair=format->value;
				oriPair=origin->value;
				format=&forPair->car;
				origin=&oriPair->car;
				continue;
			}
			else if(format->type==ATM)
			{
				ANS_atom* tmpAtm=format->value;
				if(tmpAtm->type==SYM)
				{
					PreMasym* tmpSym=findMasym(tmpAtm->value.str);
					const char* anotherName=hasAnotherName(tmpAtm->value.str);
					if(tmpSym==NULL||tmpSym->Func==M_VAREPT||!anotherName)
					{
						if(!ANS_cptrcmp(origin,format))
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
							PreDef* tmpDef=findDefine(symName,MacroEnv);
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
				ANS_pair* oriPrev=NULL;
				ANS_pair* forPrev=NULL;
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
		const ANS_cptr* tmp=oriCptr;
		oriCptr=nextCptr(tmp);
	}
	return 1;
}

int M_COLT(const ANS_cptr* oriCptr,const ANS_cptr* fmtCptr,const char* name,PreEnv* curEnv)
{
	int i;
	int num=getWordNum(name);
	ANS_atom* tmpAtm=(oriCptr->type==ATM)?oriCptr->value:NULL;
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

void deleteMacro(ANS_cptr* objCptr)
{
	PreMacro* current=FirstMacro;
	PreMacro* prev=NULL;
	while(current!=NULL&&!ANS_cptrcmp(objCptr,current->format))
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

void addToList(ANS_cptr* fir,const ANS_cptr* sec)
{
	while(fir->type!=NIL)fir=&((ANS_pair*)fir->value)->cdr;
	fir->type=PAIR;
	fir->value=newPair(0,fir->outer);
	replace(&((ANS_pair*)fir->value)->car,sec);
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
	if(!(tmpStr=(char*)malloc(sizeof(char)*(tmplen+1))))errors(OUTOFMEMORY,__FILE__,__LINE__);
	strncpy(tmpStr,name+i,tmplen);
	tmpStr[tmplen]='\0';
	return tmpStr;
}

PreDef* newDefines(const char* name)
{
	PreDef* tmp=(PreDef*)malloc(sizeof(PreDef));
	if(tmp==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
	tmp->symName=(char*)malloc(sizeof(char)*(strlen(name)+1));
	if(tmp->symName==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
	strcpy(tmp->symName,name);
	tmp->obj=(ANS_cptr){NULL,0,NIL,NULL};
	tmp->next=NULL;
	return tmp;
}
