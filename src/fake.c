#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include"fake.h"
static faf* funAndForm=NULL;
static defines* def=NULL;
char* getListFromFile(FILE* file)
{
	char* tmp=NULL;
	char* before;
	char ch;
	int i=0;
	int j;
	int k;
	int braketsNum=0;
	while((ch=getc(file))!=EOF)
	{
		i++;
		j=i-1;
		before=tmp;
		if(!(tmp=(char*)malloc(sizeof(char)*i)))errors(OUTOFMEMORY);
		for(k=0;k<j;k++)*(tmp+k)=*(before+k);
		*(tmp+j)=ch;
		if(before!=NULL)free(before);
		if(ch=='(')braketsNum++;
		if(ch==')')braketsNum--;
		if(braketsNum==0)break;
	}
	if(tmp!=NULL)*(tmp+i)='\0';
	return tmp;
}
listTreeNode* becomeTree(const char* str)
{
	enum{l,r}lor=l;
	int i=0;
	listTreeNode* root=NULL;
	listTreeNode* objNode=NULL;
	while(1)
	{
		if(*(str+i)=='(')
		{
			i++;
			if(root==NULL)
			{
				root=createNode(NULL);
				objNode=root;
			}
			else
			{
				listTreeNode* prev;
				if(lor==l)
				{
					prev=objNode;
					objNode=createNode(prev);
					prev->leftType=node;
					prev->left=(void*)objNode;
					lor=l;
				}
				else if(lor==r)
				{
					prev=objNode;
					objNode=createNode(prev);
					prev->rightType=node;
					prev->right=(void*)objNode;
					lor=l;
				}
			}
		}
		else if(*(str+i)==')')
		{
			i++;
			listTreeNode* prev=NULL;
			while(1)
			{
				prev=objNode;
				objNode=objNode->prev;
				if(objNode==NULL||objNode->left==prev)break;
			}
			if(objNode==NULL)break;
			if((listTreeNode*)(objNode->left)==prev)lor=l;
			else if((listTreeNode*)(objNode->right)==prev)lor=r;
		}
		else if(*(str+i)==',')
		{
			i++;
			lor=r;
		}
		else if(isspace(*(str+i)))
		{
			int j=0;
			char* tmpStr=(char*)str+i;
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
			listTreeNode* tmp=createNode(objNode);
			objNode->rightType=node;
			objNode->right=(void*)tmp;
			objNode=tmp;
		}
		else if(isalpha(*(str+i)))
		{
			char* tmp=getStringFromList(str+i);
			if(lor==l)
			{
				objNode->leftType=sym;
				objNode->left=(void*)tmp;
			}
			else if(lor==r)
			{
				objNode->rightType=sym;
				objNode->right=(void*)tmp;
			}
			i+=strlen(tmp);
		}
		else if(isdigit(*(str+i))||*(str+i)=='-')
		{
			char* tmp=getStringFromList(str+i);
			if(lor==l)
			{
				objNode->leftType=chars;
				objNode->left=(void*)tmp;
			}
			else if(lor==r)
			{
				objNode->rightType=chars;
				objNode->right=(void*)tmp;
			}
			i+=strlen(tmp);
		}
	}
	return root;
}
char* getStringFromList(const char* str)
{
	char* tmp=NULL;
	char* before;
	char ch;
	int i=0;
	int j;
	int k;
	while((*(str+i)!='(')
			&&(*(str+i)!=')')
			&&!isspace(*(str+i))
			&&(*(str+i)!=',')
			&&(*(str+i)!=0))
	{
		ch=*(str+i);
		i++;
		j=i-1;
		before=tmp;
		if(!(tmp=(char*)malloc(sizeof(char)*i)))errors(OUTOFMEMORY);
		for(k=0;k<j;k++)*(tmp+k)=*(before+k);
		*(tmp+j)=ch;
		if(before!=NULL)free(before);
	}
	if(tmp!=NULL)*(tmp+i)='\0';
	return tmp;
}
listTreeNode* eval(listTreeNode* objTree)
{
	if(objTree->leftType==node)eval(objTree->left);
	if(objTree->prev->left==objTree)callFunction(objTree);
	returnTree(objTree);
	return objTree;
}
int addFunction(char* name,void(*pFun)(listTreeNode*))
{
	faf* current=funAndForm;
	faf* prev=NULL;
	if(current==NULL)
	{
		if(!(funAndForm=(faf*)malloc(sizeof(faf))))errors(OUTOFMEMORY);
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
		if(!(current=(faf*)malloc(sizeof(faf))))errors(OUTOFMEMORY);
		current->functionName=name;
		current->function=pFun;
		current->next=NULL;
		prev->next=current;
	}
}
void callFunction(listTreeNode* root)
{
	void (*pf)(listTreeNode*)=findFunction((char*)root->left);
	if(pf==NULL)
		printf("NO FUNCTION!\n");
	else pf(root);
}
void (*(findFunction(const char* name)))(listTreeNode*)
{
	faf* current=funAndForm;
	while(current!=NULL&&strcmp(current->functionName,name))current=current->next;
	if(current==NULL)
		return NULL;
	else
		return current->function;
}
void returnTree(listTreeNode* objTree)
{
	if(objTree->prev!=NULL&&objTree->prev!=objTree->prev->right)
	{
		listTreeNode* prev=objTree;
		objTree=objTree->prev;
		objTree->leftType=prev->leftType;
		objTree->left=prev->left;
		free(prev);
	}
}
listTreeNode* deleteTree(listTreeNode* objTree)
{
	while(objTree!=NULL)
	{
		if(objTree->leftType==node)objTree=objTree->left;
		if(objTree->leftType==nil&&objTree->rightType==node)objTree=objTree->right;
		if(objTree->leftType==nil&&objTree->rightType==nil)
		{
			listTreeNode* prev=objTree;
			objTree=objTree->prev;
			if(objTree==NULL)
			{
				free(prev);
				break;
			}
			if(objTree->left==prev)objTree->leftType=nil;
			else if(objTree->right==prev)objTree->rightType=nil;
			free(prev);
		}
		if(objTree->leftType==sym||objTree->leftType==chars)
		{
			free(objTree->left);
			objTree->leftType=nil;
		}
		if(objTree->leftType==nil&&(objTree->rightType==sym||objTree->rightType==chars))
		{
			free(objTree->right);
			objTree->rightType=nil;
		}
	}
}
listTreeNode* deleteNode(listTreeNode* objTree)
{
	listTreeNode* tmp=objTree;
	while(1)
	{
		if(objTree->leftType==node)objTree=objTree->left;
		if(objTree->leftType==nil&&objTree->rightType==node)objTree=objTree->right;
		if(objTree->leftType==nil&&objTree->rightType==nil)
		{
			listTreeNode* prev=objTree;
			objTree=objTree->prev;
			if(objTree==NULL)
			{
				free(prev);
				break;
			}
			if(objTree->left==prev)objTree->leftType=nil;
			else if(objTree->right==prev)objTree->rightType=nil;
			free(prev);
		}
		if(objTree->leftType==sym||objTree->leftType==chars)
		{
			free(objTree->left);
			objTree->leftType=nil;
		}
		if(objTree->leftType==nil&&(objTree->rightType==sym||objTree->rightType==chars))
		{
			free(objTree->right);
			objTree->rightType=nil;
		}
		if(objTree==tmp)
		{
			tmp=objTree->prev;
			free(objTree);
			if(tmp->left==objTree)tmp->leftType=nil;
			else if(tmp->right==objTree)tmp->rightType=nil;
			break;
		}
	}
}
void printList(listTreeNode* objTree)
{
	if(objTree->prev==NULL||objTree->prev->left==objTree)putchar('(');
	if(objTree->leftType==chars||objTree->leftType==sym)printf("%s",(char*)objTree->left);
	if(objTree->leftType==node)printList(objTree->left);
	if(objTree->rightType==chars||objTree->leftType==sym)printf(",%s",(char*)objTree->right);
	if(objTree->rightType==node)
	{
		putchar(' ');
		printList(objTree->right);
	}
	if(objTree->prev==NULL||objTree->prev->left==objTree)putchar(')');
}
listTreeNode* createNode(listTreeNode* const prev)
{
	listTreeNode* tmp;
	if((tmp=(listTreeNode*)malloc(sizeof(listTreeNode))))
	{
		tmp->prev=prev;
		tmp->leftType=nil;
		tmp->left=NULL;
		tmp->rightType=nil;
		tmp->right=NULL;
	}
	else errors(OUTOFMEMORY);
}
listTreeNode* copyTree(listTreeNode* objTree)
{
	listTreeNode* tmp=createNode(NULL);
	if(objTree->leftType==node)
	{
		tmp->leftType=node;
		tmp->left=copyTree(objTree->left);
		listTreeNode* tmp1=tmp->left;
		tmp1->prev=tmp;
	}
	if(objTree->rightType==node)
	{
		tmp->rightType=node;
		tmp->right=copyTree(objTree->right);
		listTreeNode* tmp1=tmp->right;
		tmp1->prev=tmp;
	}
	if(objTree->leftType==chars||objTree->leftType==sym)
	{
		int i;
		int len=strlen(objTree->left)+1;
		tmp->leftType=objTree->leftType;
		if(!(tmp->left=(char*)malloc(sizeof(char)*len)))errors(OUTOFMEMORY);
		for(i=0;i<len;i++)*((char*)(tmp->left)+i)=*((char*)(objTree->left)+i);
	}
	if(objTree->rightType==chars||objTree->leftType==sym)
	{
		int i;
		int len=strlen(objTree->right)+1;
		tmp->rightType=objTree->rightType;
		if(!(tmp->right=(char*)malloc(sizeof(char)*len)))errors(OUTOFMEMORY);
		for(i=0;i<len;i++)*((char*)(tmp->right)+i)=*((char*)(objTree->left)+i);
	}
	return tmp;
}
int addDefine(const char* symName,void* obj,enum TYPES types)
{
	defines* current==NULL;
	defines* prev==NULL;
	int len=strlen(symName)+1;
	if(def==NULL)
	{
		int i;
		if(!(def=(defines*)malloc(sizeof(defines))))errors(OUTOFMEMORY);
		if(!(def->symName=(char*)malloc(sizeof(char)*len)))errors(OUTOFMEMORY);
		for(i=0;i<len;i++)*(def->symName+i)=*(symName+i);
		def->next=NULL;
		if(types==node)def->NSC=copyTree(obj);
		else if(types==chars||types==sym)
		{
			int len=strlen((char*)obj)+1;
			if(!(def->NSC=(char*)malloc(sizeof(char)*len)))errors(OUTOFMEMORY);
			for(i=0;i<len;i++)*((char*)(def->NSC)+i)=*((char*)obj+i);
		}
		else if(types==nil)def->NSC=NULL;
		def->type=types;
	}
	else
	{
		int i;
		current=def;
		while(current->next)
		{
			prev=current;
			current=current->next;
		}
		if(!(current=(defines*)malloc(sizeof(defines))))errors(OUTOFMEMORY);
		if(!(current->symName=(char*)malloc(sizeof(char)*len)))errors(OUTOFMEMORY);
		for(i=0;i<len;i++)*(current->symName+i)=*(symName+i);
		current->next=NULL;
		prev->next=current;
		if(types==node)current->NSC=copyTree(obj);
		else if(types==chars||types==sym)
		{
			int len=strlen((char*)obj)+1;
			if(!(current->NSC=(char*)malloc(sizeof(char)*len)))errors(OUTOFMEMORY);
			for(i=0;i<len;i++)*((char*)(current->NSC)+i)=*((char*)obj+i);
		}
		else if(types==nil)current->NSC=NULL;
		current->type=types;
	}
}
void errors(int types)
{
	switch(types)
	{
		case OUTOFMEMORY:
			fprintf(stderr,"error:Out of memory!\n");
			exit(1);
			break;
	}
}
