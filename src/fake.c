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




cptr* createTree(const char* objStr)
{
	int i=0;
	cptr* root=NULL;
	cell* objCell=NULL;
	cptr* objCptr;
	while(*(objStr+i)!='\0')
	{
		if(*(objStr+i)=='(')
		{
			i++;
			if(root==NULL)
			{
				root=createCptr(objCell);
				root->type=cel;
				root->value=createCell(NULL);
				objCell=root->value;
				objCptr=&objCell->left;
			}
			else
			{
				objCell=createCell(objCell);
				objCptr->type=cel;
				objCptr->value=(void*)objCell;
				objCptr=&objCell->left;
			}
		}
		else if(*(objStr+i)==')')
		{
			i++;
			cell* prev=NULL;
			if(objCell==NULL)break;
			while(objCell->prev!=NULL&&objCell->left.value!=prev)
			{
				prev=objCell;
				objCell=objCell->prev;
			}
			if(objCell->left.value==prev)
				objCptr=&objCell->left;
			else if(objCell->right.value==prev)
				objCptr=&objCell->right;
		}
		else if(*(objStr+i)==',')
		{
			i++;
			objCptr=&objCell->right;
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
			cell* tmp=createCell(objCell);
			objCell->right.type=cel;
			objCell->right.value=(void*)tmp;
			objCell=tmp;
			objCptr=&objCell->left;
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
		else if(isdigit(*(objStr+i))||(*(objStr+i)=='-'&&isdigit(objStr+i+1)))
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
		}
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
	else if((sec->outer==NULL||((cell*)sec->outer)->prev->right.value==((cell*)sec->outer))&&((cell*)sec->value)->right.type==nil)
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
		((cell*)fir->outer)->left.type=nil;
		((cell*)fir->outer)->left.value=NULL;
		deleteCell(beDel);
		free(beDel);
		return 0;
	}
	else
		return FAILRETURN;
}

int deleteCell(cptr* objCptr)
{
	cell* tmpCell=(objCptr->type==cel)?objCptr->value:NULL;
	cell* objCell=tmpCell;
	cptr* tmpCptr=objCptr;
	while(tmpCptr!=NULL)
	{
		if(tmpCptr->type==cel)
		{
			if(objCell!=NULL&&tmpCptr==&objCell->right)
			{
				objCell=objCell->right.value;
				tmpCptr=&objCell->left;
			}
			else
			{
				objCell=tmpCptr->value;
				tmpCptr=&objCell->left;
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
			if(tmpCptr==&objCell->left)
			{
				tmpCptr=&objCell->right;
				continue;
			}
			else if(tmpCptr==&objCell->right)
			{
				cell* prev=objCell;
				objCell=objCell->prev;
				tmpCptr=&objCell->right;
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
		deleteCell(&objCell->left);
		deleteCell(&objCell->right);
	}
	free(objCell);
}

void printList(cptr* objCptr,FILE* out)
{
	cell* tmpCell=(objCptr->type==cel)?objCptr->value:NULL;
	cell* objCell=tmpCell;
	cptr* tmp=objCptr;
	while(tmp!=NULL)
	{
		if(tmp->type==cel)
		{
			if(objCell!=NULL&&tmp==&objCell->right)
			{
				putc(' ',out);
				objCell=objCell->right.value;
				tmp=&objCell->left;
			}
			else
			{
				putc('(',out);
				objCell=tmp->value;
				tmp=&objCell->left;
				continue;
			}
		}
		else if(tmp->type==atm||tmp->type==nil)
		{
			if(objCell!=NULL&&tmp==&objCell->right&&tmp->type==atm)putc(',',out);
			if((objCell!=NULL&&tmp==&objCell->left&&tmp->type==nil)
			||(tmp->outer==NULL&&tmp->type==nil))fputs("nil",out);
			if(tmp->type!=nil&&(((atom*)tmp->value)->type==sym||((atom*)tmp->value)->type==num))
				fprintf(out,"%s",((atom*)tmp->value)->value);
			else if (tmp->type!=nil)printRawString(((atom*)tmp->value)->value,out);
			if(objCell!=NULL&&tmp==&objCell->left)
			{
				tmp=&objCell->right;
				continue;
			}
		}
		if(objCell!=NULL&&tmp==&objCell->right)
		{
			putc(')',out);
			cell* prev=NULL;
			if(objCell->prev==NULL)break;
			while(objCell->prev!=NULL)
			{
				prev=objCell;
				objCell=objCell->prev;
				if(prev==objCell->left.value)break;
			}
			if(objCell!=NULL)tmp=&objCell->right;
			if(objCell==tmpCell&&prev==objCell->right.value)break;
		}
		if(objCell==NULL)break;
	}
}

cell* createCell(cell* prev)
{
	cell* tmp;
	if((tmp=(cell*)malloc(sizeof(cell))))
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

int copyList(cptr* objCptr,const cptr* copiedCptr)
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
			copiedCptr=&copiedCell->left;
			copiedCptr=&copiedCell->left;
			objCptr=&objCell->left;
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
			if(copiedCptr==&copiedCell->left)
			{
				copiedCptr=&copiedCell->right;
				objCptr=&objCell->right;
				continue;
			}
			
		}
		else if(copiedCptr->type==nil)
		{
			objCptr->value=NULL;
			if(copiedCptr==&copiedCell->left)
			{
				objCptr=&objCell->right;
				copiedCptr=&copiedCell->right;
				continue;
			}
		}
		if(copiedCell!=NULL&&copiedCptr==&copiedCell->right)
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
				if(coPrev==copiedCell->left.value)break;
			}
			if(copiedCell!=NULL)
			{
				copiedCptr=&copiedCell->right;
				objCptr=&objCell->right;
			}
			if(copiedCell==tmpCell&&copiedCptr==&copiedCell->right)break;
		}
		if(copiedCell==NULL)break;
	}
	return 1;
}

defines* addDefine(const char* symName,const cptr* objCptr,env* curEnv)
{
	env* current=(curEnv==NULL)?glob:curEnv;
	if(current==NULL)
	{
		current=newEnv();
		if(current->symbols==NULL)
		{
			current->symbols=(defines*)malloc(sizeof(defines));
			memcpy(current->symbols->symName,symName,strlen(symName)+1);
			current->symbols->obj.type=objCptr->type;
			current->symbols->obj.value=objCptr->value;
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
				curSym->obj.type=objCptr->type;
				curSym->obj.value=objCptr->value;
				curSym->next=NULL;
			}
			else
			{
				deleteCell(&curSym->obj);
				curSym->obj.type=objCptr->type;
				curSym->obj.value=objCptr->value;
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

void replace(cptr* fir,cptr* sec)
{
	cell* tmp=fir->outer;
	deleteCell(fir);
	copyList(fir,sec);
	if(fir->type==con)((cell*)fir->value)->prev=tmp;
	else if(fir->type==atm)((atom*)fir->value)->prev=tmp;
}

int eval(cptr* objCptr,env* curEnv)
{
	curEnv=(curEnv==NULL)?glob:curEnv;
	cptr* tmpCptr=objCptr;
	while(objCptr->type!=nil)
	{
		if(objCptr->type==atm)
		{
			atom* objAtm=objCptr->value;
			if(objAtm->type==sym)
			{
				int (*pfun)(cptr*,env*)=NULL;
				pfun=findFunc(objAtm->value);
				if(pfun!=NULL)
				{
					if(objCptr->outer==NULL||(((cell*)objCptr->outer)->prev!=NULL
					&&objCptr->outer==((cell*)objCptr->outer)->prev->right.value)
					)
						return SYNTAXERROR;
					pfun(objCptr,curEnv);
					if(((cell*)objCptr->outer)->right.type==nil)break;
				}
				else
				{
					cptr* reCptr=NULL;
					env* tmpEnv=curEnv;
					while(reCptr=&(findDefines(objAtm->value,tmpEnv)->obj))
							tmpEnv=tmpEnv->prev;
					if(reCptr!=NULL)
					{
						replace(objCptr,reCptr);
						if(objCptr->outer==NULL
						||(((cell*)objCptr->outer)->prev!=NULL
						&&((cell*)objCptr->outer)->prev!=((cell*)objCptr->outer)->prev->right.value))
						break;
					}
						else return SYMUNDEFINE;
				}
			}
			else break;
		}
		else if(objCptr->type==con)objCptr=&(((cell*)objCptr->value)->left);
	}
	return retree(objCptr,tmpCptr);
}
