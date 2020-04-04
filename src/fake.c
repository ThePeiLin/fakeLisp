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
				root->type=CEL;
				root->value=createCell(NULL);
				objCell=root->value;
				objCptr=&objCell->car;
			}
			else
			{
				objCell=createCell(objCell);
				objCptr->type=CEL;
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
			objCell->cdr.type=CEL;
			objCell->cdr.value=(void*)tmp;
			objCell=tmp;
			objCptr=&objCell->car;
		}
		else if(*(objStr+i)=='\"')
		{
			if(root==NULL)objCptr=root=createCptr(objCell);
			rawString tmp=getStringBetweenMarks(objStr+i);
			objCptr->type=ATM;
			objCptr->value=(void*)createAtom(STR,tmp.str,objCell);
			i+=tmp.len;
			free(tmp.str);
		}
		else if(isdigit(*(objStr+i))||(*(objStr+i)=='-'&&isdigit(*(objStr+i+1))))
		{
			if(root==NULL)objCptr=root=createCptr(objCell);
			char* tmp=getStringFromList(objStr+i);
			objCptr->type=ATM;
			objCptr->value=(void*)createAtom(NUM,tmp,objCell);
			i+=strlen(tmp);
			free(tmp);
		}
		else
		{
			if(root==NULL)objCptr=root=createCptr(objCell);
			char* tmp=getStringFromList(objStr+i);
			objCptr->type=ATM;
			objCptr->value=(void*)createAtom(SYM,tmp,objCell);
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



int retree(cptr* fir,cptr* sec)
{
	if(fir->value==sec->value)return 0;
	else if(((cell*)sec->value)->cdr.type==NIL)
	{
		cptr* beDel=createCptr(NULL);
		beDel->type=sec->type;
		beDel->value=sec->value;
		sec->type=fir->type;
		sec->value=fir->value;
		if(fir->type==CEL)
			((cell*)fir->value)->prev=sec->outer;
		else if(fir->type==ATM)
			((atom*)fir->value)->prev=sec->outer;
		fir->outer->car.type=NIL;
		fir->outer->car.value=NULL;
		deleteCptr(beDel);
		free(beDel);
		return 0;
	}
	else
		return TOOMUCHARGS;
}

void printList(const cptr* objCptr,FILE* out)
{
	if(objCptr==NULL)return;
	cell* tmpCell=(objCptr->type==CEL)?objCptr->value:NULL;
	cell* objCell=tmpCell;
	const cptr* tmp=objCptr;
	while(tmp!=NULL)
	{
		if(tmp->type==CEL)
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
		else if(tmp->type==ATM||tmp->type==NIL)
		{
			if(objCell!=NULL&&tmp==&objCell->cdr&&tmp->type==ATM)putc(',',out);
			if((objCell!=NULL&&tmp==&objCell->car&&tmp->type==NIL&&objCell->cdr.type!=NIL)
			||(tmp->outer==NULL&&tmp->type==NIL))fputs("nil",out);
			if(tmp->type!=NIL&&(((atom*)tmp->value)->type==SYM||((atom*)tmp->value)->type==NUM))
				fprintf(out,"%s",((atom*)tmp->value)->value);
			else if (tmp->type!=NIL)printRawString(((atom*)tmp->value)->value,out);
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
			while(objCell->prev!=NULL&&objCell!=tmpCell)
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

int eval(cptr* objCptr,env* curEnv)
{
	curEnv=(curEnv==NULL)?(Glob=(Glob==NULL)?newEnv(NULL):Glob):curEnv;
	if(objCptr==NULL)return SYNTAXERROR;
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
					if(objCptr->outer->cdr.type!=CEL)break;
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
			}
		}
		else if(objCptr->type==CEL)
		{
			macroExpand(objCptr);
			if(objCptr->type!=CEL)continue;
			objCptr=&(((cell*)objCptr->value)->car);
		}
		else if(objCptr->type==NIL)break;
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
