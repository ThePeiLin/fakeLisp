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
	if(format->type!=CEL)return SYNTAXERROR;
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
	cell* tmpCell=(format->type==CEL)?format->value:NULL;
	cell* forCell=tmpCell;
	cell* oriCell=(origin->type==CEL)?origin->value:NULL;
	while(origin!=NULL)
	{
		if(format->type==CEL&&origin->type==CEL)
		{
			forCell=format->value;
			oriCell=origin->value;
			format=&forCell->car;
			origin=&oriCell->car;
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
				forCell=format->outer;
				if(tmpSym!=NULL&&tmpSym->Func==M_VAREPT)
				{
					format=&forCell->cdr;
					origin=&oriCell->cdr;
				}
			}
			if(forCell!=NULL&&format==&forCell->car)
			{
				origin=&oriCell->cdr;
				format=&forCell->cdr;
				continue;
			}
		}
		else if(origin->type==NIL&&
				format->type==CEL&&
				((cell*)format->value)->car.type==ATM)
		{
			atom* tmpAtm=((cell*)format->value)->car.value;
			masym* tmpSym=findMasym(tmpAtm->value);
			if(tmpSym->Func!=M_VAREPT)
			{
				destroyEnv(MacroEnv);
				MacroEnv=NULL;
				return 0;
			}
			else
			{
				M_VAREPT(&origin->outer->car,&((cell*)format->value)->car,NULL,MacroEnv);
			}
		}
		else if(origin->type!=format->type)
		{
			destroyEnv(MacroEnv);
			MacroEnv=NULL;
			return 0;
		}


		if(forCell!=NULL&&format==&forCell->car)
		{
			origin=&oriCell->cdr;
			format=&forCell->cdr;
			continue;
		}
		else if(forCell!=NULL&&format==&forCell->cdr)
		{
			cell* oriPrev=NULL;
			cell* forPrev=NULL;
			if(oriCell->prev==NULL)break;
			while(oriCell->prev!=NULL&&forCell!=tmpCell)
			{
				oriPrev=oriCell;
				forPrev=forCell;
				oriCell=oriCell->prev;
				forCell=forCell->prev;
				if(oriPrev==oriCell->car.value)break;
			}
			if(oriCell!=NULL)
			{
				origin=&oriCell->cdr;
				format=&forCell->cdr;
			}
			if(forCell==tmpCell&&format==&forCell->cdr)break;
		}
		if(oriCell==NULL&&forCell==NULL)break;
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

void initPretreatment()
{
	addRule("ATOM",M_ATOM);
	addRule("CELL",M_CELL);
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

int M_CELL(const cptr* oriCptr,const cptr* fmtCptr,const char* name,env* curEnv)
{
	if(oriCptr==NULL)return 0;
	else if(oriCptr->type==CEL)
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
	cell* forCell=(format->type==CEL)?format->value:NULL;
	cell* tmpCell=forCell;
	env* forValRept=newEnv(NULL);
	defines* valreptdef=addDefine(name,&tmp,MacroEnv);
	while(oriCptr!=NULL&&fmtCptr!=NULL)
	{
		const cptr* origin=oriCptr;
		const cptr* format=fmtCptr;
		cell* forCell=(format->type==CEL)?format->value:NULL;
		cell* oriCell=(origin->type==CEL)?origin->value:NULL;
		while(origin!=NULL&&format!=NULL)
		{
			if(format->type==CEL&&origin->type==CEL)
			{
				forCell=format->value;
				oriCell=origin->value;
				format=&forCell->car;
				origin=&oriCell->car;
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
						forCell=(format==&fmtCptr->outer->prev->car)?NULL:format->outer;
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
				if(forCell!=NULL&&format==&forCell->car)
				{
					origin=&oriCell->cdr;
					format=&forCell->cdr;
					continue;
				}
			}
			else if(origin->type!=format->type)break;
			if(forCell!=NULL&&format==&forCell->car)
			{
				origin=&oriCell->cdr;
				format=&forCell->cdr;
				continue;
			}
			else if(forCell!=NULL&&format==&forCell->cdr)
			{
				cell* oriPrev=NULL;
				cell* forPrev=NULL;
				if(oriCell->prev==NULL)break;
				while(oriCell->prev!=NULL&&forCell!=tmpCell)
				{
					oriPrev=oriCell;
					forPrev=forCell;
					oriCell=oriCell->prev;
					forCell=forCell->prev;
					if(oriPrev==oriCell->car.value)break;
				}
				if(oriCell!=NULL)
				{
					origin=&oriCell->cdr;
					format=&forCell->cdr;
				}
				if(forCell==tmpCell&&format==&forCell->cdr)break;
			}
			if(forCell==NULL)break;
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
	while(fir->type!=NIL)fir=&((cell*)fir->value)->cdr;
	fir->type=CEL;
	fir->value=createCell(fir->outer);
	replace(&((cell*)fir->value)->car,sec);
}
