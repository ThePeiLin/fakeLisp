#include"pretreatment.h"
#include"fake.h"
#include"tool.h"
#include<string.h>
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
				if(tmpSym==NULL)
				{
					if(!cptrcmp(origin,format))
					{
						destroyEnv(MacroEnv);
						MacroEnv=NULL;
						clearCount();
						return 0;
					}
				}
				else if(!tmpSym->Func(origin,hasAnotherName(tmpAtm->value)))
				{
					destroyEnv(MacroEnv);
					MacroEnv=NULL;
					clearCount();
					return 0;
				}
			}
			if(oriCell!=NULL&&origin==&oriCell->car)
			{
				origin=&oriCell->cdr;
				format=&forCell->cdr;
				continue;
			}
		}
		else if(origin->type!=format->type)
		{
			destroyEnv(MacroEnv);
			MacroEnv=NULL;
			clearCount();
			return 0;
		}
		if(oriCell!=NULL&&origin==&oriCell->car)
		{
			origin=&oriCell->cdr;
			format=&forCell->cdr;
			continue;
		}
		else if(oriCell!=NULL&&origin==&oriCell->cdr)
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
	clearCount();
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
	}
	return 0;
}

void initPretreatment()
{
	addRule("ATOM",M_ATOM);
	addRule("CELL",M_CELL);
	addRule("ANY",M_ANY);
	MacroEnv=newEnv(NULL);
	
}
void addRule(const char* name,int (*obj)(const cptr*,const char*))
{
	masym* current=NULL;
	if(!(current=(masym*)malloc(sizeof(masym))))errors(OUTOFMEMORY);
	if(!(current->symName=(char*)malloc(sizeof(char)*(strlen(name)+1))))errors(OUTOFMEMORY);
	current->next=First;
	First=current;
	strcpy(current->symName,name);
	current->Func=obj;
}

int M_ATOM(const cptr* objCptr,const char* name)
{
	static int count;
	if(objCptr==NULL)count=0;
	else if(objCptr->type==ATM)
	{
		if(name==NULL)
		{
			char* num;
			char* symName;
			if(!(num=(char*)malloc((sizeof(int)*2)+1)))errors(OUTOFMEMORY);
			sprintf(num,"%x",count);
			if(!(symName=(char*)malloc(strlen("ATOM#")+strlen(num)+1)))errors(OUTOFMEMORY);
			strcpy(symName,"ATOM#");
			strcat(symName,num);
			addDefine(symName,objCptr,MacroEnv);
			free(symName);
			free(num);
			count++;
			return 1;
		}
		else
		{
			addDefine(name,objCptr,MacroEnv);
			return 1;
		}
	}
	return 0;
}

int M_CELL(const cptr* objCptr,const char* name)
{
	static int count;
	if(objCptr==NULL)count=0;
	else if(objCptr->type==CEL)
	{
		if(name==NULL)
		{
			char* num;
			char* symName;
			if(!(num=(char*)malloc((sizeof(int)*2)+1)))errors(OUTOFMEMORY);
			sprintf(num,"%x",count);
			if(!(symName=(char*)malloc(strlen("CELL#")+strlen(num)+1)))errors(OUTOFMEMORY);
			strcpy(symName,"CELL#");
			strcat(symName,num);
			addDefine(symName,objCptr,MacroEnv);
			free(symName);
			free(num);
			count++;
			return 1;
		}
		else
		{
			addDefine(name,objCptr,MacroEnv);
			return 1;
		}
	}
	return 0;
}

int M_ANY(const cptr* objCptr,const char* name)
{
	static int count;
	if(objCptr==NULL)count=0;
	else
	{
		if(name==NULL)
		{
			char* num;
			char* symName;
			if(!(num=(char*)malloc((sizeof(int)*2)+1)))errors(OUTOFMEMORY);
			sprintf(num,"%x",count);
			if(!(symName=(char*)malloc(strlen("ANY#")+strlen(num)+1)))errors(OUTOFMEMORY);
			strcpy(symName,"ANY#");
			strcat(symName,num);
			addDefine(symName,objCptr,MacroEnv);
			free(symName);
			free(num);
			count++;
			return 1;
		}
		else
		{
			addDefine(name,objCptr,MacroEnv);
			return 1;
		}
	}
	return 0;
}

void clearCount()
{
	masym* current=First;
	while(current!=NULL)
	{
		current->Func(NULL,NULL);
		current=current->next;
	}
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
