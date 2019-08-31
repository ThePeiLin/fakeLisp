#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include"fake.h"
static faf* funAndForm;
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
		tmp=(char*)malloc(sizeof(char)*i);
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
				root=(listTreeNode*)malloc(sizeof(listTreeNode));
				root->prev=NULL;
				root->leftType=nil;
				root->left=NULL;
				root->rightType=nil;
				root->right=NULL;
				objNode=root;
			}
			else
			{
				listTreeNode* prev;
				if(lor==l)
				{
					prev=objNode;
					objNode=(listTreeNode*)malloc(sizeof(listTreeNode));
					objNode->leftType=nil;
					objNode->left=NULL;
					objNode->rightType=nil;
					objNode->right=NULL;
					objNode->prev=prev;
					prev->leftType=node;
					prev->left=(void*)objNode;
					lor=l;
				}
				else if(lor==r)
				{
					prev=objNode;
					objNode=(listTreeNode*)malloc(sizeof(listTreeNode));
					objNode->leftType=nil;
					objNode->left=NULL;
					objNode->rightType=nil;
					objNode->right=NULL;
					objNode->prev=prev;
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
			listTreeNode* tmp=(listTreeNode*)malloc(sizeof(listTreeNode));
			tmp->leftType=nil;
			tmp->left=NULL;
			tmp->rightType=nil;
			tmp->right=NULL;
			tmp->prev=objNode;
			objNode->rightType=node;
			objNode->right=(void*)tmp;
			objNode=tmp;
		}
		else
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
		tmp=(char*)malloc(sizeof(char)*i);
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
	callFunction(objTree);
	returnTree(objTree);
	return objTree;
}
int addFunction(char* name,void(*pFun)(listTreeNode*))
{
	faf* current=funAndForm;
	faf* prev=NULL;
	if(current==NULL)
	{
		funAndForm=(faf*)malloc(sizeof(faf));
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
		current=(faf*)malloc(sizeof(faf));
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
int printList(listTreeNode* objTree)
{
}
