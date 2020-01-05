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
				objBra=&objNode->left;
			}
			else
			{
				objNode=createCons(objNode);
				objBra->type=con;
				objBra->twig=(void*)objNode;
				objBra=&objNode->left;
			}
		}
		else if(*(objStr+i)==')')
		{
			i++;
			consPair* prev=NULL;
			if(objNode==NULL)break;
			while(objNode->prev!=NULL&&objNode->left.twig!=prev)
			{
				prev=objNode;
				objNode=objNode->prev;
			}
			if(objNode->left.twig==prev)
				objBra=&objNode->left;
			else if(objNode->right.twig==prev)
				objBra=&objNode->right;
		}
		else if(*(objStr+i)==',')
		{
			i++;
			objBra=&objNode->right;
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
			objNode->right.type=con;
			objNode->right.twig=(void*)tmp;
			objNode=tmp;
			objBra=&objNode->left;
		}
		else if(isalpha(*(objStr+i)))
		{
			if(root==NULL)objBra=root=createBranch();
			char* tmp=getStringFromList(objStr+i);
			objBra->type=atm;
			if(!(objBra->twig=(atom*)malloc(sizeof(atom))))errors(OUTOFMEMORY);
			((atom*)objBra->twig)->prev=objNode;
			((atom*)objBra->twig)->type=sym;
			((atom*)objBra->twig)->value=tmp;
			i+=strlen(tmp);
		}
		else if(*(objStr+i)=='\"')
		{
			if(root==NULL)objBra=root=createBranch();
			rawString tmp=getStringBetweenMarks(objStr+i);
			objBra->type=atm;
			if(!(objBra->twig=(atom*)malloc(sizeof(atom))))errors(OUTOFMEMORY);
			((atom*)objBra->twig)->type=str;
			((atom*)objBra->twig)->prev=objNode;
			((atom*)objBra->twig)->value=(void*)tmp.str;
			i+=tmp.len;
		}
		else if(isdigit(*(objStr+i))||*(objStr+i)=='-')
		{
			if(root==NULL)objBra=root=createBranch();
			char* tmp=getStringFromList(objStr+i);
			objBra->type=atm;
			if(!(objBra->twig=(atom*)malloc(sizeof(atom))))errors(OUTOFMEMORY);
			((atom*)objBra->twig)->type=num;
			((atom*)objBra->twig)->prev=objNode;
			((atom*)objBra->twig)->value=(void*)tmp;
			i+=strlen(tmp);
		}
	}
	return root;
}


branch* eval(branch* objBra,env* curEnv)
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
void (*(findFunction(const char* name)))(branch*,env*)
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

branch* deleteNode(branch* objBra)
{
	branch* tmpBra=objBra;
	consPair* objNode=NULL;
	while(tmpBra!=NULL)
	{
		if(tmpBra->type==con)
		{
			objNode=tmpBra->twig;
			tmpBra=&objNode->left;
		}
		if(tmpBra->type==atm)
		{
			objNode=((atom*)tmpBra->twig)->prev;
			free(((atom*)tmpBra->twig)->value);
			free(tmpBra->twig);
			tmpBra->type=nil;
			tmpBra->twig=NULL;
		}
		if(objNode!=NULL&&objNode->left.type==nil)tmpBra=&objNode->right;
		if(objNode!=NULL&&objNode->right.type==nil&&objNode->left.type==nil)
		{
			consPair* prev=objNode;
			objNode=objNode->prev;
			tmpBra=&objNode->left;
			if(tmpBra->twig==prev)
			{
				tmpBra->type=nil;
				tmpBra->twig=NULL;
			}
			else
			{
				objNode->right.type=nil;
				objNode->right.twig=NULL;
			}
			free(prev);
		}
		if(objNode==NULL||objNode->prev==NULL)break;
	}
	if(objBra->type!=nil)free(objBra->twig);
	objBra->type=nil;
	objBra->twig=NULL;
	return objBra;
}

branch* destroyList(branch* objBra)
{
	consPair* objNode=NULL;
	if(objBra->type==con)objNode=((consPair*)objBra->twig)->prev;
	if(objBra->type==atm)objNode=((atom*)objBra->twig)->prev;
	if(objBra->type==nil)return objBra;
	while(objNode!=NULL&&objNode->prev!=NULL)objNode=objNode->prev;
	if(objNode!=NULL)
	{
		deleteNode(&objNode->left);
		deleteNode(&objNode->right);
	}
	free(objNode);
}

void printList(branch* objBra,FILE* out)
{
	consPair* tmpNode=(objBra->type==con)?objBra->twig:NULL;
	consPair* objNode=tmpNode;
	branch* tmp=objBra;
	while(tmp!=NULL)
	{
		if(tmp->type==con)
		{
			if(objNode!=NULL&&tmp==&objNode->right)
			{
				putc(' ',out);
				objNode=objNode->right.twig;
				tmp=&objNode->left;
			}
			else
			{
				putc('(',out);
				objNode=tmp->twig;
				tmp=&objNode->left;
				continue;
			}
		}
		else if(tmp->type==atm||tmp->type==nil)
		{
			if(objNode!=NULL&&tmp==&objNode->right&&tmp->type==atm)putc(',',out);
			if(tmp->type!=nil&&(((atom*)tmp->twig)->type==sym||((atom*)tmp->twig)->type==num))
				fprintf(out,"%s",((atom*)tmp->twig)->value);
			else if (tmp->type!=nil)printRawString(((atom*)tmp->twig)->value,out);
			if(objNode!=NULL&&tmp==&objNode->left)
			{
				tmp=&objNode->right;
				continue;
			}
		}
		if(objNode!=NULL&&tmp==&objNode->right)
		{
			putc(')',out);
			consPair* prev=NULL;
			if(objNode->prev==NULL)break;
			while(objNode->prev!=NULL)
			{
				prev=objNode;
				objNode=objNode->prev;
				if(prev==objNode->left.twig)break;
			}
			if(objNode!=NULL)tmp=&objNode->right;
			if(objNode==tmpNode&&prev==objNode->right.twig)break;
		}
	}
}

consPair* createCons(consPair* prev)
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

int copyTree(branch* objBra,const branch* copiedBra)
{
	consPair* objNode=NULL;
	consPair* copiedNode=NULL;
	while(copiedBra!=NULL)
	{
		if(copiedBra->type==con)
		{
			consPair* tmp=objNode;
			copiedNode=copiedBra->twig;
			objNode=createCons(tmp);
			objBra=&objNode->left;
			copiedBra=&copiedNode->left;
		}
		if(copiedBra->type==atm)
		{
			atom* tmp1=NULL;
			atom* tmp2=(atom*)copiedBra->twig;
			char* tmpStr=NULL;
			char* objStr=tmp2->value;
			if(!(tmpStr=(char*)malloc(strlen(objStr)+1)))errors(OUTOFMEMORY);
			objBra->type=atm;
			if(!(tmp1=(atom*)malloc(sizeof(atom))))errors(OUTOFMEMORY);
			objBra->twig=tmp1;
			tmp1->prev=objNode;
			tmp1->type=tmp2->type;
			memcpy(tmpStr,objStr,strlen(objStr)+1);
			tmp1->value=tmpStr;
			if(tmp2->prev==NULL)break;
			if(&copiedNode->left==copiedBra)
			{
				objBra=&objNode->right;
				copiedBra=&copiedNode->right;
			}
		}
		if(copiedBra->type==nil)
		{
			objBra->type=nil;
			objBra->twig=NULL;
		}
		if(objNode!=NULL&&copiedNode!=NULL&&copiedNode->left.type==objNode->left.type&&copiedNode->right.type==objNode->right.type)
		{
			objNode=objNode->prev;
			copiedNode=copiedNode->prev;
			if(&objNode->left==objBra)
			{
				objBra=&objNode->right;
				copiedBra=&copiedNode->right;
			}
		}
		if(copiedNode==NULL)break;
	}
	return 1;
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
			memcpy(current->symbols->symName,symName,strlen(symName)+1);
			current->symbols->obj.type=objBra->type;
			current->symbols->obj.twig=objBra->twig;
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
				curSym->obj.type=objBra->type;
				curSym->obj.twig=objBra->twig;
				curSym->next=NULL;
			}
			else
			{
				deleteNode(&curSym->obj);
				curSym->obj.type=objBra->type;
				curSym->obj.twig=objBra->twig;
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

branch* createBranch()
{
	branch* tmp=NULL;
	if(!(tmp=(branch*)malloc(sizeof(branch))))
		errors(OUTOFMEMORY);
	tmp->type=nil;
	tmp->twig=NULL;
	return tmp;
}
