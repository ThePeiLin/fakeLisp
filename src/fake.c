#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include"fake.h"
#include"tool.h"

static nativeFunc* funAndForm=NULL;
static env* glob=NULL;

char* getStringFromList(const char* str)
{
	char* tmp=NULL;
	int len=0;
	while((*(str+len)!='(')
			&&(*(str+len)!=')')
			&&!isspace(*(str+len))
			&&(*(str+len)!=',')
			&&(*(str+len)!=0))len++;
	if(!(tmp=(char*)malloc(sizeof(char)*len+1)))errors(OUTOFMEMORY);
	memcpy(tmp,str,len);
	if(tmp!=NULL)*(tmp+len)='\0';
	return tmp;
}

char* getListFromFile(FILE* file)
{
	char* tmp=NULL;
	char* before;
	char ch;
	int i=0;
	int j;
	int braketsNum=0;
	while((ch=getc(file))!=EOF)
	{
		i++;
		j=i-1;
		before=tmp;
		if(!(tmp=(char*)malloc(sizeof(char)*i)))
			errors(OUTOFMEMORY);
		memcpy(tmp,before,j);
		*(tmp+j)=ch;
		if(before!=NULL)free(before);
		if(ch=='(')braketsNum++;
		if(ch==')')braketsNum--;
		if(braketsNum==0)break;
	}
	if(tmp!=NULL)*(tmp+i)='\0';
	return tmp;
}




cell* createTree(const char* objStr)
{
	int i=0;
	cell* root=NULL;
	conspair* objCons=NULL;
	cell* objCell;
	while(*(objStr+i)!='\0')
	{
		if(*(objStr+i)=='(')
		{
			i++;
			if(root==NULL)
			{
				root=createCell(objCons);
				root->type=con;
				root->value=createCons(NULL);
				objCons=root->value;
				objCell=&objCons->left;
			}
			else
			{
				objCons=createCons(objCons);
				objCell->type=con;
				objCell->value=(void*)objCons;
				objCell=&objCons->left;
			}
		}
		else if(*(objStr+i)==')')
		{
			i++;
			conspair* prev=NULL;
			if(objCons==NULL)break;
			while(objCons->prev!=NULL&&objCons->left.value!=prev)
			{
				prev=objCons;
				objCons=objCons->prev;
			}
			if(objCons->left.value==prev)
				objCell=&objCons->left;
			else if(objCons->right.value==prev)
				objCell=&objCons->right;
		}
		else if(*(objStr+i)==',')
		{
			i++;
			objCell=&objCons->right;
		}
		else if(isspace(*(objStr+i)))
		{
			int j=0;
			char* tmpStr=(char*)objStr+i;
			while(isspace(*(tmpStr+j)))j--;
			if(*(tmpStr+j)==',')
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
			conspair* tmp=createCons(objCons);
			objCons->right.type=con;
			objCons->right.value=(void*)tmp;
			objCons=tmp;
			objCell=&objCons->left;
		}
		else if(*(objStr+i)=='\"')
		{
			if(root==NULL)objCell=root=createCell(objCons);
			rawString tmp=getStringBetweenMarks(objStr+i);
			objCell->type=atm;
			objCell->value=(void*)createAtom(str,tmp.str,objCons);
			i+=tmp.len;
			free(tmp.str);
		}
		else if(isdigit(*(objStr+i))||(*(objStr+i)=='-'&&isdigit(objStr+i+1)))
		{
			if(root==NULL)objCell=root=createCell(objCons);
			char* tmp=getStringFromList(objStr+i);
			objCell->type=atm;
			objCell->value=(void*)createAtom(num,tmp,objCons);
			i+=strlen(tmp);
			free(tmp);
		}
		else
		{
			if(root==NULL)objCell=root=createCell(objCons);
			char* tmp=getStringFromList(objStr+i);
			objCell->type=atm;
			objCell->value=(void*)createAtom(sym,tmp,objCons);
			i+=strlen(tmp);
			free(tmp);
		}
	}
	return root;
}

int addFunc(char* name,int (*pFun)(cell*,env*))
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
void callFunc(cell* objCell,env* curEnv)
{
	findFunc(((atom*)objCell->value)->value)(objCell,curEnv);
}
int (*(findFunc(const char* name)))(cell*,env*)
{
	nativeFunc* current=funAndForm;
	while(current!=NULL&&strcmp(current->functionName,name))current=current->next;
	if(current==NULL)
		return NULL;
	else
		return current->function;
}

int retree(cell* fir,cell* sec)
{
	if(fir->value==sec->value)return 0;
	else if((sec->outer==NULL||((conspair*)sec->outer)->prev->right.value==((conspair*)sec->outer))&&((conspair*)sec->value)->right.type==nil)
	{
		cell* beDel=createCell(NULL);
		beDel->type=sec->type;
		beDel->value=sec->value;
		sec->type=fir->type;
		sec->value=fir->value;
		if(fir->type==con)
			((conspair*)fir->value)->prev=sec->outer;
		else if(fir->type==atm)
			((atom*)fir->value)->prev=sec->outer;
		((conspair*)fir->outer)->left.type=nil;
		((conspair*)fir->outer)->left.value=NULL;
		deleteCons(beDel);
		free(beDel);
		return 0;
	}
	else
		return FAILRETURN;
}

int deleteCons(cell* objCell)
{
	conspair* tmpCons=(objCell->type==con)?objCell->value:NULL;
	conspair* objCons=tmpCons;
	cell* tmpCell=objCell;
	while(tmpCell!=NULL)
	{
		if(tmpCell->type==con)
		{
			if(objCons!=NULL&&tmpCell==&objCons->right)
			{
				objCons=objCons->right.value;
				tmpCell=&objCons->left;
			}
			else
			{
				objCons=tmpCell->value;
				tmpCell=&objCons->left;
				continue;
			}
		}
		else if(tmpCell->type==atm)
		{
			if(tmpCell->type!=nil)
			{
				atom* tmpAtm=(atom*)tmpCell->value;
				free(tmpAtm->value);
				free(tmpAtm);
				tmpCell->type=nil;
				tmpCell->value=NULL;
			}
		}
		else if(tmpCell->type==nil)
		{
			if(tmpCell==&objCons->left)
			{
				tmpCell=&objCons->right;
				continue;
			}
			else if(tmpCell==&objCons->right)
			{
				conspair* prev=objCons;
				objCons=objCons->prev;
				tmpCell=&objCons->right;
				free(prev);
				if(tmpCons==objCons)break;
			}
		}
		if(objCons==NULL)break;
		{
			objCell->type=nil;
			objCell->value=NULL;
			break;
		}
	}
	return 0;
}

cell* destroyList(cell* objCell)
{
	conspair* objCons=NULL;
	if(objCell->type==con)objCons=((conspair*)objCell->value)->prev;
	if(objCell->type==atm)objCons=((atom*)objCell->value)->prev;
	if(objCell->type==nil)return objCell;
	while(objCons!=NULL&&objCons->prev!=NULL)objCons=objCons->prev;
	if(objCons!=NULL)
	{
		deleteCons(&objCons->left);
		deleteCons(&objCons->right);
	}
	free(objCons);
}

void printList(cell* objCell,FILE* out)
{
	conspair* tmpCons=(objCell->type==con)?objCell->value:NULL;
	conspair* objCons=tmpCons;
	cell* tmp=objCell;
	while(tmp!=NULL)
	{
		if(tmp->type==con)
		{
			if(objCons!=NULL&&tmp==&objCons->right)
			{
				putc(' ',out);
				objCons=objCons->right.value;
				tmp=&objCons->left;
			}
			else
			{
				putc('(',out);
				objCons=tmp->value;
				tmp=&objCons->left;
				continue;
			}
		}
		else if(tmp->type==atm||tmp->type==nil)
		{
			if(objCons!=NULL&&tmp==&objCons->right&&tmp->type==atm)putc(',',out);
			if(objCons!=NULL&&tmp==&objCons->left&&tmp->type==nil)fputs("nil",out);
			if(tmp->type!=nil&&(((atom*)tmp->value)->type==sym||((atom*)tmp->value)->type==num))
				fprintf(out,"%s",((atom*)tmp->value)->value);
			else if (tmp->type!=nil)printRawString(((atom*)tmp->value)->value,out);
			if(objCons!=NULL&&tmp==&objCons->left)
			{
				tmp=&objCons->right;
				continue;
			}
		}
		if(objCons!=NULL&&tmp==&objCons->right)
		{
			putc(')',out);
			conspair* prev=NULL;
			if(objCons->prev==NULL)break;
			while(objCons->prev!=NULL)
			{
				prev=objCons;
				objCons=objCons->prev;
				if(prev==objCons->left.value)break;
			}
			if(objCons!=NULL)tmp=&objCons->right;
			if(objCons==tmpCons&&prev==objCons->right.value)break;
		}
		if(objCons==NULL)break;
	}
}

conspair* createCons(conspair* prev)
{
	conspair* tmp;
	if((tmp=(conspair*)malloc(sizeof(conspair))))
	{
		tmp->left.outer=tmp;
		tmp->left.type=nil;
		tmp->left.value=NULL;
		tmp->right.outer=tmp;
		tmp->right.type=nil;
		tmp->right.value=NULL;
		tmp->prev=prev;
	}
	else errors(OUTOFMEMORY);
}

int copyList(cell* objCell,const cell* copiedCell)
{
	if(copiedCell==NULL||objCell==NULL)return 0;
	conspair* objCons=NULL;
	conspair* copiedCons=NULL;
	conspair* tmpCons=(copiedCell->type==con)?copiedCell->value:NULL;
	copiedCons=tmpCons;
	while(1)
	{
		objCell->type=copiedCell->type;
		if(copiedCell->type==con)
		{
			objCons=createCons(objCons);
			objCell->value=objCons;
			copiedCons=copiedCell->value;
			copiedCell=&copiedCons->left;
			copiedCell=&copiedCons->left;
			objCell=&objCons->left;
			continue;
		}
		else if(copiedCell->type==atm)
		{
			atom* coAtm=copiedCell->value;
			atom* objAtm=NULL;
			if(!(objAtm=(atom*)malloc(sizeof(atom))))errors(OUTOFMEMORY);
			objCell->value=objAtm;
			if(!(objAtm->value=(char*)malloc(strlen(coAtm->value)+1)))errors(OUTOFMEMORY);
			objAtm->type=coAtm->type;
			memcpy(objAtm->value,coAtm->value,strlen(coAtm->value)+1);
			if(copiedCell==&copiedCons->left)
			{
				copiedCell=&copiedCons->right;
				objCell=&objCons->right;
				continue;
			}
			
		}
		else if(copiedCell->type==nil)
		{
			objCell->value=NULL;
			if(copiedCell==&copiedCons->left)
			{
				objCell=&objCons->right;
				copiedCell=&copiedCons->right;
				continue;
			}
		}
		if(copiedCons!=NULL&&copiedCell==&copiedCons->right)
		{
			conspair* objPrev=NULL;
			conspair* coPrev=NULL;
			if(copiedCons->prev==NULL)break;
			while(copiedCons->prev!=NULL)
			{
				coPrev=copiedCons;
				copiedCons=copiedCons->prev;
				objPrev=objCons;
				objCons=objCons->prev;
				if(coPrev==copiedCons->left.value)break;
			}
			if(copiedCons!=NULL)
			{
				copiedCell=&copiedCons->right;
				objCell=&objCons->right;
			}
			if(copiedCons==tmpCons&&copiedCell==&copiedCons->right)break;
		}
	}
	return 1;
}

defines* addDefine(const char* symName,const cell* objCell,env* curEnv)
{
	env* current=(curEnv==NULL)?glob:curEnv;
	if(current==NULL)
	{
		current=newEnv();
		if(current->symbols==NULL)
		{
			current->symbols=(defines*)malloc(sizeof(defines));
			memcpy(current->symbols->symName,symName,strlen(symName)+1);
			current->symbols->obj.type=objCell->type;
			current->symbols->obj.value=objCell->value;
			current->symbols->next=NULL;
			return current->symbols;
		}
		else
		{
			defines* curSym=findDefines(symName,curEnv);
			if(curSym==NULL)
			{
				defines* prev=NULL;
				curSym=curEnv->symbols;
				while(curSym->next!=NULL)
					curSym=curSym->next;
				prev=curSym;
				if(!(curSym=(defines*)malloc(sizeof(defines))))errors(OUTOFMEMORY);
				prev->next=curSym;
				curSym->obj.type=objCell->type;
				curSym->obj.value=objCell->value;
				curSym->next=NULL;
			}
			else
			{
				deleteCons(&curSym->obj);
				curSym->obj.type=objCell->type;
				curSym->obj.value=objCell->value;
			}
			return curSym;
		}
	}
}

env* newEnv()
{
	if(glob==NULL)
	{
		glob=(env*)malloc(sizeof(env));
		glob->prev=NULL;
		glob->symbols=NULL;
		glob->next=NULL;
		return glob;
	}
	else
	{
		env* current=glob;
		env* prev=NULL;
		while(current->next)current->next;
		prev=current;
		if(!(current=(env*)malloc(sizeof(env))))
			errors(OUTOFMEMORY);
		current->prev=prev;
		current->symbols=NULL;
		current->next=NULL;
		return current;
	}
}

defines* findDefines(const char* name,env* curEnv)
{
	env* current=(curEnv==NULL)?glob:curEnv;
	if(current->symbols==NULL)return NULL;
	else
	{
		defines* curSym=current->symbols;
		while(curSym&&strcmp(name,curSym->symName))
			curSym=curSym->next;
		return curSym;
	}
}

cell* createCell(conspair* outer)
{
	cell* tmp=NULL;
	if(!(tmp=(cell*)malloc(sizeof(cell))))
		errors(OUTOFMEMORY);
	tmp->outer=outer;
	tmp->type=nil;
	tmp->value=NULL;
	return tmp;
}

atom* createAtom(int type,const char* value,conspair* prev)
{
	atom* tmp=NULL;
	if(!(tmp=(atom*)malloc(sizeof(atom))))errors(OUTOFMEMORY);
	if(!(tmp->value=(char*)malloc(strlen(value)+1)))errors(OUTOFMEMORY);
	tmp->prev=prev;
	tmp->type=type;
	memcpy(tmp->value,value,strlen(value)+1);
	return tmp;
}

void replace(cell* fir,cell* sec)
{
	conspair* tmp=fir->outer;
	deleteCons(fir);
	copyList(fir,sec);
	if(fir->type==con)((conspair*)fir->value)->prev=tmp;
	else if(fir->type==atm)((atom*)fir->value)->prev=tmp;
}

int eval(cell* objCell,env* curEnv)
{
	curEnv=(curEnv==NULL)?glob:curEnv;
	cell* tmpCell=objCell;
	while(objCell->type!=nil||((atom*)objCell->value)->type!=sym)
	{
		if(objCell->type==atm)
		{
			atom* objAtm=objCell->value;
			if(objAtm->type==sym)
			{
				int (*pfun)(cell*,env*)=NULL;
				pfun=findFunc(objAtm->value);
				if(pfun!=NULL)
				{
					if(objCell->outer==NULL||(((conspair*)objCell->outer)->prev!=NULL
					&&objCell->outer==((conspair*)objCell->outer)->prev->right.value))
						return SYNTAXERROR;
					pfun(objCell,curEnv);
					if(((conspair*)objCell->outer)->right.type==nil)break;
				}
				else
				{
					cell* reCell=NULL;
					env* tmpEnv=curEnv;
					while(reCell=&(findDefines(objAtm->value,tmpEnv)->obj))
							tmpEnv=tmpEnv->prev;
					if(reCell!=NULL)
					{
						replace(objCell,reCell);
						if(objCell->outer==NULL
						||(((conspair*)objCell->outer)->prev!=NULL
						&&((conspair*)objCell->outer)->prev!=((conspair*)objCell->outer)->prev->right.value))
						break;
					}
						else return SYMUNDEFINE;
				}
			}
			else break;
		}
		else if(objCell->type==con)objCell=&(((conspair*)objCell->value)->left);
	}
	return retree(objCell,tmpCell);
}
