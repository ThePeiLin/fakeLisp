#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include"fake.h"
#include"tool.h"
#include"pretreatment.h"

static nativeFunc* funAndForm=NULL;
static env* Glob=NULL;

cptr* createTree(const char* objStr)
{
	int i=0;
	int braketsNum=0;
	cptr* root=NULL;
	cell* objCell=NULL;
	cptr* objCptr;
	while(*(objStr+i)!='\0')
	{
		if(*(objStr+i)=='(')
		{
			i++;
			braketsNum++;
			if(root==NULL)
			{
				root=createCptr(objCell);
				root->type=cel;
				root->value=createCell(NULL);
				objCell=root->value;
				objCptr=&objCell->car;
			}
			else
			{
				objCell=createCell(objCell);
				objCptr->type=cel;
				objCptr->value=(void*)objCell;
				objCptr=&objCell->car;
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
			cell* prev=NULL;
			if(objCell==NULL)break;
			while(objCell->prev!=NULL)
			{
				prev=objCell;
				objCell=objCell->prev;
				if(objCell->car.value==prev)break;
			}
			if(objCell->car.value==prev)
				objCptr=&objCell->car;
			else if(objCell->cdr.value==prev)
				objCptr=&objCell->cdr;
		}
		else if(*(objStr+i)==',')
		{
			i++;
			objCptr=&objCell->cdr;
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
			cell* tmp=createCell(objCell);
			objCell->cdr.type=cel;
			objCell->cdr.value=(void*)tmp;
			objCell=tmp;
			objCptr=&objCell->car;
		}
		else if(*(objStr+i)=='\"')
		{
			if(root==NULL)objCptr=root=createCptr(objCell);
			rawString tmp=getStringBetweenMarks(objStr+i);
			objCptr->type=atm;
			objCptr->value=(void*)createAtom(str,tmp.str,objCell);
			i+=tmp.len;
			free(tmp.str);
		}
		else if(isdigit(*(objStr+i))||(*(objStr+i)=='-'&&isdigit(*(objStr+i+1))))
		{
			if(root==NULL)objCptr=root=createCptr(objCell);
			char* tmp=getStringFromList(objStr+i);
			objCptr->type=atm;
			objCptr->value=(void*)createAtom(num,tmp,objCell);
			i+=strlen(tmp);
			free(tmp);
		}
		else
		{
			if(root==NULL)objCptr=root=createCptr(objCell);
			char* tmp=getStringFromList(objStr+i);
			objCptr->type=atm;
			objCptr->value=(void*)createAtom(sym,tmp,objCell);
			i+=strlen(tmp);
			free(tmp);
			continue;
		}
		if(braketsNum==0)break;
	}
	return root;
}

int addFunc(char* name,int (*pFun)(cptr*,env*))
{
	nativeFunc* current=funAndForm;
	nativeFunc* prev=NULL;
	if(current==NULL)
	{
		if(!(funAndForm=(nativeFunc*)malloc(sizeof(nativeFunc))))errors(OUTOFMEMORY);
		funAndForm->functionName=name;
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
		current->functionName=name;
		current->function=pFun;
		current->next=NULL;
		prev->next=current;
	}
}
void callFunc(cptr* objCptr,env* curEnv)
{
	findFunc(((atom*)objCptr->value)->value)(objCptr,curEnv);
}
int (*(findFunc(const char* name)))(cptr*,env*)
{
	nativeFunc* current=funAndForm;
	while(current!=NULL&&strcmp(current->functionName,name))current=current->next;
	if(current==NULL)
		return NULL;
	else
		return current->function;
}

int cptrcmp(cptr* first,cptr* second)
{
	if(first==NULL&&second==NULL)return 0;
	cell* firCell=NULL;
	cell* secCell=NULL;
	cell* tmpCell=(first->type==cel)?first->value:NULL;
	while(1)
	{
		if(first->type!=second->type)return 0;
		else if(first->type==cel)
		{
			firCell=first->value;
			secCell=second->value;
			first=&firCell->car;
			second=&secCell->car;
			continue;
		}
		else if(first->type==atm||first->type==nil)
		{
			if(first->type==atm)
			{
				atom* firAtm=first->value;
				atom* secAtm=second->value;
				if(firAtm->type!=secAtm->type)return 0;
				if(strcmp(firAtm->value,secAtm->value)!=0)return 0;
			}
			if(firCell!=NULL&&first==&firCell->car)
			{
				first=&firCell->cdr;
				second=&secCell->cdr;
				continue;
			}
		}
		if(firCell!=NULL&&first==&firCell->car)
		{
			first=&firCell->cdr;
			second=&secCell->cdr;
			continue;
		}
		else if(firCell!=NULL&&first==&firCell->cdr)
		{
			cell* firPrev=NULL;
			cell* secPrev=NULL;
			if(firCell->prev==NULL)break;
			while(firCell->prev!=NULL)
			{
				firPrev=firCell;
				secPrev=secCell;
				firCell=firCell->prev;
				secCell=secCell->prev;
				if(firPrev==firCell->car.value)break;
			}
			if(firCell!=NULL)
			{
				first=&firCell->cdr;
				second=&secCell->cdr;
			}
			if(firCell==tmpCell&&first==&firCell->cdr)break;
		}
		if(firCell==NULL&&secCell==NULL)break;
	}
	return 1;
}

int retree(cptr* fir,cptr* sec)
{
	if(fir->value==sec->value)return 0;
	else if(((cell*)sec->value)->cdr.type==nil)
	{
		cptr* beDel=createCptr(NULL);
		beDel->type=sec->type;
		beDel->value=sec->value;
		sec->type=fir->type;
		sec->value=fir->value;
		if(fir->type==cel)
			((cell*)fir->value)->prev=sec->outer;
		else if(fir->type==atm)
			((atom*)fir->value)->prev=sec->outer;
		fir->outer->car.type=nil;
		fir->outer->car.value=NULL;
		deleteCptr(beDel);
		free(beDel);
		return 0;
	}
	else
		return TOOMUCHARGS;
}

int deleteCptr(cptr* objCptr)
{
	if(objCptr==NULL)return 0;
	cell* tmpCell=(objCptr->type==cel)?objCptr->value:NULL;
	cell* objCell=tmpCell;
	cptr* tmpCptr=objCptr;
	while(tmpCptr!=NULL)
	{
		if(tmpCptr->type==cel)
		{
			if(objCell!=NULL&&tmpCptr==&objCell->cdr)
			{
				objCell=objCell->cdr.value;
				tmpCptr=&objCell->car;
			}
			else
			{
				objCell=tmpCptr->value;
				tmpCptr=&objCell->car;
				continue;
			}
		}
		else if(tmpCptr->type==atm)
		{
			if(tmpCptr->type!=nil)
			{
				atom* tmpAtm=(atom*)tmpCptr->value;
				free(tmpAtm->value);
				free(tmpAtm);
				tmpCptr->type=nil;
				tmpCptr->value=NULL;
			}
		}
		else if(tmpCptr->type==nil)
		{
			if(tmpCptr==&objCell->car)
			{
				tmpCptr=&objCell->cdr;
				continue;
			}
			else if(tmpCptr==&objCell->cdr)
			{
				cell* prev=objCell;
				objCell=objCell->prev;
				tmpCptr=&objCell->cdr;
				free(prev);
				if(tmpCell==objCell)break;
			}
		}
		if(objCell==NULL)break;
		{
			objCptr->type=nil;
			objCptr->value=NULL;
			break;
		}
	}
	return 0;
}

cptr* destroyList(cptr* objCptr)
{
	cell* objCell=NULL;
	if(objCptr->type==cel)objCell=((cell*)objCptr->value)->prev;
	if(objCptr->type==atm)objCell=((atom*)objCptr->value)->prev;
	if(objCptr->type==nil)return objCptr;
	while(objCell!=NULL&&objCell->prev!=NULL)objCell=objCell->prev;
	if(objCell!=NULL)
	{
		deleteCptr(&objCell->car);
		deleteCptr(&objCell->cdr);
	}
	free(objCell);
}

void printList(const cptr* objCptr,FILE* out)
{
	if(objCptr==NULL)return;
	cell* tmpCell=(objCptr->type==cel)?objCptr->value:NULL;
	cell* objCell=tmpCell;
	const cptr* tmp=objCptr;
	while(tmp!=NULL)
	{
		if(tmp->type==cel)
		{
			if(objCell!=NULL&&tmp==&objCell->cdr)
			{
				putc(' ',out);
				objCell=objCell->cdr.value;
				tmp=&objCell->car;
			}
			else
			{
				putc('(',out);
				objCell=tmp->value;
				tmp=&objCell->car;
				continue;
			}
		}
		else if(tmp->type==atm||tmp->type==nil)
		{
			if(objCell!=NULL&&tmp==&objCell->cdr&&tmp->type==atm)putc(',',out);
			if((objCell!=NULL&&tmp==&objCell->car&&tmp->type==nil)
			||(tmp->outer==NULL&&tmp->type==nil))fputs("nil",out);
			if(tmp->type!=nil&&(((atom*)tmp->value)->type==sym||((atom*)tmp->value)->type==num))
				fprintf(out,"%s",((atom*)tmp->value)->value);
			else if (tmp->type!=nil)printRawString(((atom*)tmp->value)->value,out);
			if(objCell!=NULL&&tmp==&objCell->car)
			{
				tmp=&objCell->cdr;
				continue;
			}
		}
		if(objCell!=NULL&&tmp==&objCell->cdr)
		{
			putc(')',out);
			cell* prev=NULL;
			if(objCell->prev==NULL)break;
			while(objCell->prev!=NULL)
			{
				prev=objCell;
				objCell=objCell->prev;
				if(prev==objCell->car.value)break;
			}
			if(objCell!=NULL)tmp=&objCell->cdr;
			if(objCell==tmpCell&&prev==objCell->cdr.value)break;
		}
		if(objCell==NULL)break;
	}
}

cell* createCell(cell* prev)
{
	cell* tmp;
	if((tmp=(cell*)malloc(sizeof(cell))))
	{
		tmp->car.outer=tmp;
		tmp->car.type=nil;
		tmp->car.value=NULL;
		tmp->cdr.outer=tmp;
		tmp->cdr.type=nil;
		tmp->cdr.value=NULL;
		tmp->prev=prev;
	}
	else errors(OUTOFMEMORY);
}

int copyCptr(cptr* objCptr,const cptr* copiedCptr)
{
	if(copiedCptr==NULL||objCptr==NULL)return 0;
	cell* objCell=NULL;
	cell* copiedCell=NULL;
	cell* tmpCell=(copiedCptr->type==cel)?copiedCptr->value:NULL;
	copiedCell=tmpCell;
	while(1)
	{
		objCptr->type=copiedCptr->type;
		if(copiedCptr->type==cel)
		{
			objCell=createCell(objCell);
			objCptr->value=objCell;
			copiedCell=copiedCptr->value;
			copiedCptr=&copiedCell->car;
			copiedCptr=&copiedCell->car;
			objCptr=&objCell->car;
			continue;
		}
		else if(copiedCptr->type==atm)
		{
			atom* coAtm=copiedCptr->value;
			atom* objAtm=NULL;
			if(!(objAtm=(atom*)malloc(sizeof(atom))))errors(OUTOFMEMORY);
			objCptr->value=objAtm;
			if(!(objAtm->value=(char*)malloc(strlen(coAtm->value)+1)))errors(OUTOFMEMORY);
			objAtm->type=coAtm->type;
			memcpy(objAtm->value,coAtm->value,strlen(coAtm->value)+1);
			if(copiedCptr==&copiedCell->car)
			{
				copiedCptr=&copiedCell->cdr;
				objCptr=&objCell->cdr;
				continue;
			}
			
		}
		else if(copiedCptr->type==nil)
		{
			objCptr->value=NULL;
			if(copiedCptr==&copiedCell->car)
			{
				objCptr=&objCell->cdr;
				copiedCptr=&copiedCell->cdr;
				continue;
			}
		}
		if(copiedCell!=NULL&&copiedCptr==&copiedCell->cdr)
		{
			cell* objPrev=NULL;
			cell* coPrev=NULL;
			if(copiedCell->prev==NULL)break;
			while(objCell->prev!=NULL&&copiedCell!=NULL)
			{
				coPrev=copiedCell;
				copiedCell=copiedCell->prev;
				objPrev=objCell;
				objCell=objCell->prev;
				if(coPrev==copiedCell->car.value)break;
			}
			if(copiedCell!=NULL)
			{
				copiedCptr=&copiedCell->cdr;
				objCptr=&objCell->cdr;
			}
			if(copiedCell==tmpCell&&copiedCptr->type==objCptr->type)break;
		}
		if(copiedCell==NULL)break;
	}
	return 1;
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

defines* findDefine(const char* name,env* curEnv)
{
	env* current=(curEnv==NULL)?Glob:curEnv;
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

cptr* createCptr(cell* outer)
{
	cptr* tmp=NULL;
	if(!(tmp=(cptr*)malloc(sizeof(cptr))))
		errors(OUTOFMEMORY);
	tmp->outer=outer;
	tmp->type=nil;
	tmp->value=NULL;
	return tmp;
}

atom* createAtom(int type,const char* value,cell* prev)
{
	atom* tmp=NULL;
	if(!(tmp=(atom*)malloc(sizeof(atom))))errors(OUTOFMEMORY);
	if(!(tmp->value=(char*)malloc(strlen(value)+1)))errors(OUTOFMEMORY);
	tmp->prev=prev;
	tmp->type=type;
	memcpy(tmp->value,value,strlen(value)+1);
	return tmp;
}

void replace(cptr* fir,const cptr* sec)
{
	cell* tmp=fir->outer;
	deleteCptr(fir);
	copyCptr(fir,sec);
	if(fir->type==cel)((cell*)fir->value)->prev=tmp;
	else if(fir->type==atm)((atom*)fir->value)->prev=tmp;
}

int eval(cptr* objCptr,env* curEnv)
{
	curEnv=(curEnv==NULL)?(Glob=(Glob==NULL)?newEnv(NULL):Glob):curEnv;
	if(objCptr==NULL)return SYNTAXERROR;
	curEnv=(curEnv==NULL)?Glob:curEnv;
	cptr* tmpCptr=objCptr;
	while(objCptr->type!=nil)
	{
		if(objCptr->type==atm)
		{
			atom* objAtm=objCptr->value;
			if(objCptr->outer==NULL||((objCptr->outer->prev!=NULL)&&(objCptr->outer->prev->cdr.value==objCptr->outer)))
			{
				if(objAtm->type!=sym)break;
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
					exError(objCptr,SYMUNDEFINE);
					return SYMUNDEFINE;
				}
			}
			else
			{
				int before=objCptr->outer->cdr.type;
				int (*pfun)(cptr*,env*)=NULL;
				pfun=findFunc(objAtm->value);
				if(pfun!=NULL)
				{
					int status=pfun(objCptr,curEnv);
					if(status!=0)exError(objCptr,status);
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
					}
					else 
					{
						exError(objCptr,SYMUNDEFINE);
						return SYMUNDEFINE;
					}
				}
				if(before!=nil&&objCptr->outer->cdr.type!=cel)break;
			}
		}
		else if(objCptr->type==cel)
		{
			macroExpand(objCptr);
			objCptr=&(((cell*)objCptr->value)->car);
		}
		else if(objCptr->type==nil)break;
	}
	return retree(objCptr,tmpCptr);
}

void exError(const cptr* obj,int type)
{
	printList(obj,stdout);
	switch(type)
	{
		case SYMUNDEFINE:printf(":Symbol is undefined.\n");break;
		case SYNTAXERROR:printf(":Syntax error.\n");break;
		case TOOMUCHARGS:printf(":Too much args.\n");break;
	}
}
