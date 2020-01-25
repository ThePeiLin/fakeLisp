#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include"fake.h"
#include"numAndString.h"
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
	consPair* objCons=NULL;
	cell* objCell;
	while(*(objStr+i)!='\0')
	{
		if(*(objStr+i)=='(')
		{
			i++;
			if(root==NULL)
			{
				root=createCell();
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
			consPair* prev=NULL;
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
			consPair* tmp=createCons(objCons);
			objCons->right.type=con;
			objCons->right.value=(void*)tmp;
			objCons=tmp;
			objCell=&objCons->left;
		}
		else if(*(objStr+i)=='\"')
		{
			if(root==NULL)objCell=root=createCell();
			rawString tmp=getStringBetweenMarks(objStr+i);
			objCell->type=atm;
			if(!(objCell->value=(atom*)malloc(sizeof(atom))))errors(OUTOFMEMORY);
			((atom*)objCell->value)->type=str;
			((atom*)objCell->value)->prev=objCons;
			((atom*)objCell->value)->value=(void*)tmp.str;
			i+=tmp.len;
		}
		else if(isdigit(*(objStr+i))||(*(objStr+i)=='-'&&isdigit(objStr+i+1)))
		{
			if(root==NULL)objCell=root=createCell();
			char* tmp=getStringFromList(objStr+i);
			objCell->type=atm;
			if(!(objCell->value=(atom*)malloc(sizeof(atom))))errors(OUTOFMEMORY);
			((atom*)objCell->value)->type=num;
			((atom*)objCell->value)->prev=objCons;
			((atom*)objCell->value)->value=(void*)tmp;
			i+=strlen(tmp);
		}
		else
		{
			if(root==NULL)objCell=root=createCell();
			char* tmp=getStringFromList(objStr+i);
			objCell->type=atm;
			if(!(objCell->value=(atom*)malloc(sizeof(atom))))errors(OUTOFMEMORY);
			((atom*)objCell->value)->prev=objCons;
			((atom*)objCell->value)->type=sym;
			((atom*)objCell->value)->value=tmp;
			i+=strlen(tmp);
		}
	}
	return root;
}


cell* eval(cell* objCell,env* curEnv)
{
	
}
int addFunction(char* name,void(*pFun)(cell*,env*))
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
void callFunction(cell* root,env* curEnv)
{
	
}
void (*(findFunction(const char* name)))(cell*,env*)
{
	nativeFunc* current=funAndForm;
	while(current!=NULL&&strcmp(current->functionName,name))current=current->next;
	if(current==NULL)
		return NULL;
	else
		return current->function;
}
void returnTree(cell* objCell)
{
	
}

cell* deleteCons(cell* objCell)
{
	cell* tmpCell=objCell;
	consPair* objCons=NULL;
	while(tmpCell!=NULL)
	{
		if(tmpCell->type==con)
		{
			objCons=tmpCell->value;
			tmpCell=&objCons->left;
		}
		if(tmpCell->type==atm)
		{
			objCons=((atom*)tmpCell->value)->prev;
			free(((atom*)tmpCell->value)->value);
			free(tmpCell->value);
			tmpCell->type=nil;
			tmpCell->value=NULL;
		}
		if(objCons!=NULL&&objCons->left.type==nil)tmpCell=&objCons->right;
		if(objCons!=NULL&&objCons->right.type==nil&&objCons->left.type==nil)
		{
			consPair* prev=objCons;
			objCons=objCons->prev;
			tmpCell=&objCons->left;
			if(tmpCell->value==prev)
			{
				tmpCell->type=nil;
				tmpCell->value=NULL;
			}
			else
			{
				objCons->right.type=nil;
				objCons->right.value=NULL;
			}
			free(prev);
		}
		if(objCons==NULL||objCons->prev==NULL)break;
	}
	if(objCell->type!=nil)free(objCell->value);
	objCell->type=nil;
	objCell->value=NULL;
	return objCell;
}

cell* destroyList(cell* objCell)
{
	consPair* objCons=NULL;
	if(objCell->type==con)objCons=((consPair*)objCell->value)->prev;
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
	consPair* tmpCons=(objCell->type==con)?objCell->value:NULL;
	consPair* objCons=tmpCons;
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
			consPair* prev=NULL;
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

consPair* createCons(consPair* prev)
{
	consPair* tmp;
	if((tmp=(consPair*)malloc(sizeof(consPair))))
	{
		tmp->left.type=nil;
		tmp->left.value=NULL;
		tmp->right.type=nil;
		tmp->right.value=NULL;
		tmp->prev=prev;
	}
	else errors(OUTOFMEMORY);
}

int copyTree(cell* objCell,const cell* copiedCell)
{
	if(copiedCell==NULL||objCell==NULL)return 0;
	consPair* objCons=NULL;
	consPair* copiedCons=NULL;
	consPair* tmpCons=(copiedCell->type==con)?copiedCell->value:NULL;
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
			consPair* objPrev=NULL;
			consPair* coPrev=NULL;
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

cell* createCell()
{
	cell* tmp=NULL;
	if(!(tmp=(cell*)malloc(sizeof(cell))))
		errors(OUTOFMEMORY);
	tmp->type=nil;
	tmp->value=NULL;
	return tmp;
}
