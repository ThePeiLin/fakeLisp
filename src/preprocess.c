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
	//printAllKeyWord();
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

void initPreprocess()
{
	addRule("ATOM",M_ATOM);
	addRule("PAIR",M_PAIR);
	addRule("ANY",M_ANY);
	addRule("VAREPT",M_VAREPT);
	addRule("COLT",M_COLT);
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
	const cptr tmp={NULL,0,NIL,NULL};
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
		oriCptr=nextCptr(oriCptr);
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
