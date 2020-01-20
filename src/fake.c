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
	consPair* objNode=NULL;
	cell* objBra;
	while(*(objStr+i)!='\0')
	{
		if(*(objStr+i)=='(')
		{
			i++;
			if(root==NULL)
			{
				root=createCell();
				root->type=con;
				root->pointer=createCons(NULL);
				objNode=root->pointer;
				objBra=&objNode->left;
			}
			else
			{
				objNode=createCons(objNode);
				objBra->type=con;
				objBra->pointer=(void*)objNode;
				objBra=&objNode->left;
			}
		}
		else if(*(objStr+i)==')')
		{
			i++;
			consPair* prev=NULL;
			if(objNode==NULL)break;
			while(objNode->prev!=NULL&&objNode->left.pointer!=prev)
			{
				prev=objNode;
				objNode=objNode->prev;
			}
			if(objNode->left.pointer==prev)
				objBra=&objNode->left;
			else if(objNode->right.pointer==prev)
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
			objNode->right.pointer=(void*)tmp;
			objNode=tmp;
			objBra=&objNode->left;
		}
		else if(isalpha(*(objStr+i)))
		{
			if(root==NULL)objBra=root=createCell();
			char* tmp=getStringFromList(objStr+i);
			objBra->type=atm;
			if(!(objBra->pointer=(atom*)malloc(sizeof(atom))))errors(OUTOFMEMORY);
			((atom*)objBra->pointer)->prev=objNode;
			((atom*)objBra->pointer)->type=sym;
			((atom*)objBra->pointer)->value=tmp;
			i+=strlen(tmp);
		}
		else if(*(objStr+i)=='\"')
		{
			if(root==NULL)objBra=root=createCell();
			rawString tmp=getStringBetweenMarks(objStr+i);
			objBra->type=atm;
			if(!(objBra->pointer=(atom*)malloc(sizeof(atom))))errors(OUTOFMEMORY);
			((atom*)objBra->pointer)->type=str;
			((atom*)objBra->pointer)->prev=objNode;
			((atom*)objBra->pointer)->value=(void*)tmp.str;
			i+=tmp.len;
		}
		else if(isdigit(*(objStr+i))||*(objStr+i)=='-')
		{
			if(root==NULL)objBra=root=createCell();
			char* tmp=getStringFromList(objStr+i);
			objBra->type=atm;
			if(!(objBra->pointer=(atom*)malloc(sizeof(atom))))errors(OUTOFMEMORY);
			((atom*)objBra->pointer)->type=num;
			((atom*)objBra->pointer)->prev=objNode;
			((atom*)objBra->pointer)->value=(void*)tmp;
			i+=strlen(tmp);
		}
	}
	return root;
}


cell* eval(cell* objBra,env* curEnv)
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
void returnTree(cell* objBra)
{
	
}

cell* deleteNode(cell* objBra)
{
	cell* tmpBra=objBra;
	consPair* objNode=NULL;
	while(tmpBra!=NULL)
	{
		if(tmpBra->type==con)
		{
			objNode=tmpBra->pointer;
			tmpBra=&objNode->left;
		}
		if(tmpBra->type==atm)
		{
			objNode=((atom*)tmpBra->pointer)->prev;
			free(((atom*)tmpBra->pointer)->value);
			free(tmpBra->pointer);
			tmpBra->type=nil;
			tmpBra->pointer=NULL;
		}
		if(objNode!=NULL&&objNode->left.type==nil)tmpBra=&objNode->right;
		if(objNode!=NULL&&objNode->right.type==nil&&objNode->left.type==nil)
		{
			consPair* prev=objNode;
			objNode=objNode->prev;
			tmpBra=&objNode->left;
			if(tmpBra->pointer==prev)
			{
				tmpBra->type=nil;
				tmpBra->pointer=NULL;
			}
			else
			{
				objNode->right.type=nil;
				objNode->right.pointer=NULL;
			}
			free(prev);
		}
		if(objNode==NULL||objNode->prev==NULL)break;
	}
	if(objBra->type!=nil)free(objBra->pointer);
	objBra->type=nil;
	objBra->pointer=NULL;
	return objBra;
}

cell* destroyList(cell* objBra)
{
	consPair* objNode=NULL;
	if(objBra->type==con)objNode=((consPair*)objBra->pointer)->prev;
	if(objBra->type==atm)objNode=((atom*)objBra->pointer)->prev;
	if(objBra->type==nil)return objBra;
	while(objNode!=NULL&&objNode->prev!=NULL)objNode=objNode->prev;
	if(objNode!=NULL)
	{
		deleteNode(&objNode->left);
		deleteNode(&objNode->right);
	}
	free(objNode);
}

void printList(cell* objBra,FILE* out)
{
	consPair* tmpNode=(objBra->type==con)?objBra->pointer:NULL;
	consPair* objNode=tmpNode;
	cell* tmp=objBra;
	while(tmp!=NULL)
	{
		if(tmp->type==con)
		{
			if(objNode!=NULL&&tmp==&objNode->right)
			{
				putc(' ',out);
				objNode=objNode->right.pointer;
				tmp=&objNode->left;
			}
			else
			{
				putc('(',out);
				objNode=tmp->pointer;
				tmp=&objNode->left;
				continue;
			}
		}
		else if(tmp->type==atm||tmp->type==nil)
		{
			if(objNode!=NULL&&tmp==&objNode->right&&tmp->type==atm)putc(',',out);
			if(tmp->type!=nil&&(((atom*)tmp->pointer)->type==sym||((atom*)tmp->pointer)->type==num))
				fprintf(out,"%s",((atom*)tmp->pointer)->value);
			else if (tmp->type!=nil)printRawString(((atom*)tmp->pointer)->value,out);
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
				if(prev==objNode->left.pointer)break;
			}
			if(objNode!=NULL)tmp=&objNode->right;
			if(objNode==tmpNode&&prev==objNode->right.pointer)break;
		}
	}
}

consPair* createCons(consPair* prev)
{
	consPair* tmp;
	if((tmp=(consPair*)malloc(sizeof(consPair))))
	{
		tmp->left.type=nil;
		tmp->left.pointer=NULL;
		tmp->right.type=nil;
		tmp->right.pointer=NULL;
		tmp->prev=prev;
	}
	else errors(OUTOFMEMORY);
}

int copyTree(cell* objBra,const cell* copiedBra)
{
	consPair* objNode=NULL;
	consPair* copiedNode=NULL;
	while(copiedBra!=NULL)
	{
		if(copiedBra->type==con)
		{
			consPair* tmp=objNode;
			copiedNode=copiedBra->pointer;
			objNode=createCons(tmp);
			objBra=&objNode->left;
			copiedBra=&copiedNode->left;
		}
		if(copiedBra->type==atm)
		{
			atom* tmp1=NULL;
			atom* tmp2=(atom*)copiedBra->pointer;
			char* tmpStr=NULL;
			char* objStr=tmp2->value;
			if(!(tmpStr=(char*)malloc(strlen(objStr)+1)))errors(OUTOFMEMORY);
			objBra->type=atm;
			if(!(tmp1=(atom*)malloc(sizeof(atom))))errors(OUTOFMEMORY);
			objBra->pointer=tmp1;
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
			objBra->pointer=NULL;
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

defines* addDefine(const char* symName,const cell* objBra,env* curEnv)
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
			current->symbols->obj.pointer=objBra->pointer;
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
				curSym->obj.pointer=objBra->pointer;
				curSym->next=NULL;
			}
			else
			{
				deleteNode(&curSym->obj);
				curSym->obj.type=objBra->type;
				curSym->obj.pointer=objBra->pointer;
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
	tmp->pointer=NULL;
	return tmp;
}
