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
			do
			{
				prev=objNode;
				objNode=objNode->prev;
			}
			while(objNode==NULL||objNode->left->twig==prev);
			if(objNode->left->twig==prev)
				objBra=&objNode->left;
			else if(objNode->right->twig==prev)
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
			objNode->right->type=con;
			objNode->right->twig=(void*)tmp;
			objNode=tmp;
			objBra=&objNode->left;
		}
		else if(isalpha(*(objStr+i)))
		{
			if(root==NULL)objBra=root=createBranch();
			char* tmp=getStringFromList(objStr+i);
			objBra->type=atm;
			if(!(objBra->twig=(atom*)malloc(sizeof(atom))))errors(OUTOFMEMROY);
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
	consPair* objNode=(objBra->type==con)?objBra->twig:NULL;
	branch* tmp=objBra;
	while(tmp!=NULL&&objBra!=nil)
	{
		if(objNode!=NULL)
		{
			if(objNode->left.type=con)
			{
				putc('(',out);
				objNode=objNode->left.twig;
				tmp=&objNode->left;
			}
			else if(objNode->right.type=con)
			{
				putc(' ',out);
				objNode=objNode->right.twig;
				tmp=&objNode->right;
			}
			else if(objNode->right.type==atm)putc(',',out);
		}
		if(tmp->type==atm)
		{
			if(((atom*)tmp->twig)->type==sym||((atom*)tmp->twig)->type==num)fprintf(out,"%s",((atom*)tmp->twig)->value);
			if(((atom*)tmp->twig)->type==str)printRawString(((atom*)tmp->twig)->value,out);
			if(&objNode->left==tmp)tmp=&objNode->right;
		}
		if(&objNode->right==tmp)
		{
			putc(')',out);
			objNode=objNode->prev;
			objBra=&objNode->right;
		}
	}
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
	branch* tmpBra=objBra;
	branch* root=createBranch();
	branch copiedBra=root;
	consPair* objNode=NULL;
	consPair* copiedNode=NULL;
	while(tmpBra!=NULL&&tmpBra->type!=nil)
	{
		if(objBra->type==con)
		{
			consPair* tmp=copiedNode;
			objNode=objBra->twig;
			copiedNode=createCons(tmp);
			copiedBra=&copiedNode->left;
		}
		if(objBra->type==atm)
		{
			char* tmpStr=NULL;
			char* objStr=((atom*)objBra->twig)->value;
			if(!(tmpStr=(char*)malloc(strlen(objStr)+1)))errors(OUTOFMEMORY);
			copiedBra->type=atm;
			if(!(copiedBra->twig=(atom*)malloc(sizeof(atom))))errors(OUTOFMEMORY);
			((atom*)copiedBra->twig)->prev=copiedNode;
			((atom*)copiedBra->twig)->type=((atom*)objBra->twig)->type;
			memcpy(tmpStr,objStr,strlen(objStr)+1);
			((atom*)copiedBra->twig)->value=tmpStr;
			if(((atom*)objBra->twig)->prev==NULL)break;
			if(&copiedNode->left==objBra)
			{
				copiedBra=&copiedNode->right;
				objBra=&objNode->right;
			}
		}
		if(objNode!=NULL&&objNode->left.type==copiedNode.left&&objNode->right.type==copiedNode->right.type)
		{
			copiedNode=copiedNode->prev;
			objNode=objNode->prev;
			if(&copiedNode->left==copiedBra)
			{
				copiedBra=&copiedNode->right;
				objBra=&objNode->right;
			}
		}
		if(objNode==NULL)break;
	}
	return root;
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
