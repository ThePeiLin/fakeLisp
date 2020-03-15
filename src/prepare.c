#include"prepare.h"
#include"fake.h"
static macro* Head=NULL;
static masym* First=NULL;
macro* macroMatch(cptr* objCptr)
{
	macro* current=Head;
	while(current!=NULL&&!fmatcmp(objCptr,current->format))current=current->next;
	return current;
}

int addMacro(cptr* format,cptr* express)
{
	if(format->type!=cel)return SYNTAXERROR;
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
	while(current!=NULL&&!strcmp(current->symName,name))current=current->next;
	return current;
}

int fmatcmp(cptr* origin,cptr* format)
{
	cell* tmpCell=(format->type==cel)?format->value:NULL;
	cell* forCell=tmpCell;
	cell* oriCell=(origin->type==cell)?origin:NULL;
	while(origin!=NULL)
	{
		if(format->type==cel&&origin->type==cel)
		{
			forCell=format->value;
			oriCell=origin->value;
			format=&forCell->car;
			origin=&oriCell->car;
			continue;
		}
		else if(format->type==atm)
		{
			atom* tmpAtm=format->value;
			masym* tmpSym=findMasym(tmpAtm->value);
			if(tmpSym==NULL)
				if(!cptrcmp(origin,format))return 0;
			if(!tmpSym->Func(origin))return 0;
			if(oriCell!=NULL&&origin==&oriCell->car)
			{
				origin=&oriCell->cdr;
				format=&forCell->cdr;
				continue;
			}
		}
		else if(origin->type!=format->type)return 0;
		else if(oriCell!=NULL&&origin==&origin->cdr)
		{
			cell* oriPrev=NULL;
			cell* forPrev=NULL;
			if(oriCell->prev==NULL)break;
			while(oriCell->prev!=NULL)
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
				continue;
			}
			if(forCell==tmpCell&&oriCell=&oriCell-cdr)break;
		}
		if(oriCell==NULL&&forCell==NULL)break;
	}
	return 1;
}
