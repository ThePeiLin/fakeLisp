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




branch* becomeTree(const char* objStr)
{
	int i=0;
	branch* root=NULL;
	consPair* objNode=NULL;
	branch* objBra;
	while(*(objStr+i)!='\0')
	{
		if(*(objStr+i)=='(')
		{
			i++;
			if(root==NULL)
			{
				root=createBranch();
				root->type=con;
				root->twig=createCons(NULL);
				objNode=root->twig;
				objBra=objNode->left;
			}
			else
			{
				objNode=createCons(objNode);
				objBra->type=con;
				objBra->twig=(void*)objNode;
				objBra=objNode->left;
			}
		}
		else if(*(objStr+i)==')')
		{
			i++;
			consPair* prev=NULL;
			if(objNode==NULL)break;
			do
			{
				prev=objNode;
				objNode=objNode->prev;
			}
			while(objNode==NULL||objNode->left->twig==prev);
			if(objNode->left->twig==prev)
				objBra=objNode->left;
			else if(objNode->right->twig==prev)
				objBra=objNode->right;
		}
		else if(*(objStr+i)==',')
		{
			i++;
			objBra=objNode->right;
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
			consPair* tmp=createCons(objNode);
			objNode->right->type=con;
			objNode->right->twig=(void*)tmp;
			objNode=tmp;
			objBra=objNode->left;
		}
		else if(isalpha(*(objStr+i)))
		{
			char* tmp=getStringFromList(objStr+i);
			objBra->type=sym;
			objBra->twig=(void*)tmp;
			i+=strlen(tmp);
		}
		else if(*(objStr+i)=='\"')
		{
			rawString tmp=getStringBetweenMarks(objStr+i);
			objBra->type=val;
			if(!(objBra->twig=(leaf*)malloc(sizeof(leaf))))errors(OUTOFMEMORY);
			((leaf*)objBra->twig)->type=str;
			((leaf*)objBra->twig)->prev=objNode;
			((leaf*)objBra->twig)->value=(void*)tmp.str;
			i+=tmp.len;
		}
		else if(isdigit(*(objStr+i))||*(objStr+i)=='-')
		{
			char* tmp=getStringFromList(objStr+i);
			objBra->type=val;
			if(!(objBra->twig=(leaf*)malloc(sizeof(leaf))))errors(OUTOFMEMORY);
			((leaf*)objBra->twig)->type=num;
			((leaf*)objBra->twig)->prev=objNode;
			((leaf*)objBra->twig)->value=(void*)tmp;
			i+=strlen(tmp);
		}
	}
	return root;
}


consPair* eval(branch* objBra,env* curEnv)
{
	
}
int addFunction(char* name,void(*pFun)(branch*,env*))
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
void callFunction(branch* root,env* curEnv)
{
	
}
void (*(findFunction(const char* name)))(branch*)
{
	nativeFunc* current=funAndForm;
	while(current!=NULL&&strcmp(current->functionName,name))current=current->next;
	if(current==NULL)
		return NULL;
	else
		return current->function;
}
void returnTree(branch* objBra)
{
	
}


branch* destroyList(branch* objBra)
{
	while((consPair*)(objBra->twig)->prev!=NULL)
	{
		if(objTree->left->type==con)objTree=objTree->left->twig;
		if(objTree->left->type==nil&&objTree->right->type==con)objTree=objTree->right->twig;
		if(objTree->left->type==nil&&objTree->right->type==nil)
		{
			consPair* prev=objTree;
			objTree=objTree->prev;
			if(objTree==NULL)
			{
				free(prev);
				break;
			}
			if(objTree->left->twig==prev)objTree->left->type=nil;
			else if(objTree->right->twig==prev)objTree->right->type=nil;
			free(prev);
		}
		if(objTree->left->type==sym)
		{
			free(objTree->left->twig);
			objTree->left->twig=NULL;
			objTree->left->type=nil;
		}
		if(objTree->left->type==nil&&objTree->right->type==sym)
		{
			free(objTree->right->twig);
			objTree->right->twig=NULL;
			objTree->right->type=nil;
		}
		if(objTree->left->type==val)
		{
			free(((leaf*)objTree->left->twig)->value);
			free(objTree->left->twig);
			objTree->left->twig=NULL;
			objTree->left->type=nil;
		}
		if(objTree->left->type==nil&&objTree->right->type==val)
		{
			free(((leaf*)objTree->right->twig)->value);
			free(objTree->right->twig);
			objTree->right->twig=NULL;
			objTree->right->type=nil;
		}
	}
}


branch* deleteNode(branch* objBra)
{
	if(objBra->type==sym)
		free(objBra->twig);
	else if(objBra->type==val)
	{
		free(((leaf*)objBra->twig)->val);
		free(objBra->twig);
	}
	else if(objBra->type==con)
	{
		consPair* objTree=objBra->twig;
		consPair* tmp=objTree;
		while(1)
		{
			if(objTree->left->type==con)objTree=objTree->left->twig;
			if(objTree->left->type==nil&&objTree->right->type==con)objTree=objTree->right->twig;
			if(objTree->left->type==nil&&objTree->right->type==nil)
			{
				consPair* prev=objTree;
				objTree=objTree->prev;
				if(objTree==NULL)
				{
					free(prev);
					break;
				}
				if(objTree->left->twig==prev)objTree->left->type=nil;
				else if(objTree->right->twig==prev)objTree->right->type=nil;
				free(prev);
			}
			if(objTree->left->type==sym)
			{
				free(objTree->left->twig);
				objTree->left->twig=NULL;
				objTree->left->type=nil;
			}
			if(objTree->left->type==nil&&objTree->right->type==sym)
			{
				free(objTree->right->twig);
				objTree->right->twig=NULL;
				objTree->right->type=nil;
			}
			if(objTree->left->type==val)
			{
				free(((leaf*)objTree->left->twig)->value);
				free(objTree->left->twig);
				objTree->left->twig=NULL;
				objTree->left->type=nil;
			}
			if(objTree->left->type==nil&&objTree->right->type==val)
			{
				free(((leaf*)objTree->right->twig)->value);
				free(objTree->right->twig);
				objTree->right->twig=NULL;
				objTree->right->type=nil;
			}
			if(objTree==tmp)
			{
				tmp=objTree->prev;
				free(objTree);
				if(tmp->left->twig==objTree)tmp->left->type=nil;
				else if(tmp->right->twig==objTree)tmp->right->type=nil;
				break;
			}
		}
	}
	objBra->type=nil;
	objBra->twig=NULL;
	return objBra;
}



void printList(branch* objBra,FILE* out)
{
	if(objTree->prev==NULL||objTree->prev->left->twig==objTree)putc('(',out);
	if(objTree->left->type==sym)fputs(objTree->left->twig,out);
	else if(objTree->left->type=val)printRawString(((leaf*)objTree->left->twig)->value,out);
	else if(objTree->left->type==con)printList(objTree->left->twig,out);
	if(objTree->right->type==sym)
	{
		putc(',',out);
		printRawString(objTree->right->twig,out);
	}
	else if(objTree->right->type==val)
	{
		putc(',',out);
		printRawString(((leaf*)objTree->right->twig)->value,out);
	}
	else if(objTree->right->type==con)
	{
		putc(' ',out);
		printList(objTree->right->twig,out);
	}
	if(objTree->prev==NULL||(consPair*)(objTree->prev)->left->twig==objTree)putc(')',out);
}
consPair* createCons(consPair* const prev)
{
	consPair* tmp;
	if((tmp=(consPair*)malloc(sizeof(consPair))))
	{
		tmp->left.type=nil;
		tmp->left.twig=NULL;
		tmp->right.type=nil;
		tmp->right.twig=NULL;
		tmp->prev=prev;
	}
	else errors(OUTOFMEMORY);
}
branch* copyTree(branch* objBra)
{
	consPair* tmp=createCons(NULL);
	if(objTree->left->type==con)
	{
		tmp->left->type=con;
		tmp->left->twig=copyTree(objTree->left->twig);
		consPair* tmp1=tmp->left->twig;
		tmp1->prev=tmp;
	}
	if(objTree->right->type==con)
	{
		tmp->right->type=con;
		tmp->right->twig=copyTree(objTree->right->twig);
		consPair* tmp1=tmp->right->twig;
		tmp1->prev=tmp;
	}
	if(objTree->left->type==sym)
	{
		int len=strlen(objTree->left->twig)+1;
		tmp->left->type=objTree->left->type;
		if(!(tmp->left->twig=(char*)malloc(sizeof(char)*len)))errors(OUTOFMEMORY);
		memcpy(tmp->left->twig,objTree->left->twig,len);
	}
	if(objTree->left->type==sym)
	{
		int len=strlen(objTree->right->twig)+1;
		tmp->right->type=objTree->right->type;
		if(!(tmp->right->twig=(char*)malloc(sizeof(char)*len)))errors(OUTOFMEMORY);
		memcpy(tmp->right->twig,objTree->right->twig,len);
	}
	if(objTree->left->type==val)
	{
		tmp->left->type=objTree->left->type;
		int len=strlen(((leaf*)objTree->left->twig)->value)+1;
		if(!(tmp->left->twig=(leaf*)malloc(sizeof(leaf))))errors(OUTOFMEMORY);
		if(!(((leaf*)tmp->left->twig)->value=(char*)malloc(sizeof(char)*len)))errors(OUTOFMEMORY);
		memcpy(((leaf*)tmp->left->twig)->value,((leaf*)objTree->left->twig)->value,len);
	}
	if(objTree->right->type==val)
	{
		tmp->right->type=objTree->right->type;
		int len=strlen(((leaf*)objTree->right->twig)->value)+1;
		if(!(tmp->right->twig=(leaf*)malloc(sizeof(leaf))))errors(OUTOFMEMORY);
		if(!(((leaf*)tmp->right->twig)->value=(char*)malloc(sizeof(char)*len)))errors(OUTOFMEMORY);
		memcpy(((leaf*)tmp->right->twig)->value,((leaf*)objTree->right->twig)->value,len);
	}
	return tmp;
}



defines* addDefine(const char* symName,const branch* objBra,env* curEnv)
{
	env* current=(curEnv==NULL)?glob:curEnv;
	if(current==NULL)
	{
		current=newEnv();
		if(current->symbols==NULL)
		{
			current->symbols=(defines*)malloc(sizeof(defines));
			current->symbols->symName=symName;
			current->obj->type=objBra->type;
			current->obj->twig=objBra->twig;
			current->symbols->next=NULL;
			return current->symbols;
		}
		else
		{
			defines* curSym=findDefines(symName,curEnv);
			if(curSym==NULL)
			{
				curSym* prev=NULL;
				curSym=curEnv->symbols;
				while(curSym->next!=NULL)
					curSym=curSym->next;
				prev=curSym;
				if(!(curSym=(defines*)malloc(sizeof(defines))))errors(OUTOFMEMORY);
				prev->next=curSym;
				curSym->obj->type=objBra->type;
				curSym->obj->twig=objBra->type;
				curSym->next=NULL;
			}
			else
			{
				deleteNode(&curSym->obj);
				curSym->obj->type=objBra->type;
				curSym->obj->twig=objBra->twig;
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
		while(currnet->next)current->next;
		prev=current;
		if(!(current=(env*)malloc(sizeof(env))))
			errors(OUTOFMEMORY);
		current->prev=prev;
		current->symbols=NULL;
		current->next=NULL;
		return current;
	}
}

defines* findDefines(char* name,env* curEnv)
{
	current=(curEnv==NULL)?glob:curEnv;
	if(current->symbols==NULL)return NULL;
	else
	{
		defines* curSym=current->symbols;
		while(curSym&&strcmp(name,curSym->symName))
			curSym=curSym->next;
		return curSym;
	}
}

branch* createBranch()
{
	branch* tmp=NULL;
	if(!(branch==(branch*)malloc(sizeof(branch))))
		errors(OUTOFMEMORY);
	tmp->type=nil;
	tmp->twig=NULL;
	return tmp;
}
