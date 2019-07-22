#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include"fake.h"
extern sfc* specialfc;
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
			lor=l;
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
			while(isspace(*(tmpStr+j)))j++;
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
	callFunction(objTree);
	returnTree(objTree);
	return objTree;
}
int addSpecialFunction(char* name,void(*spFun)(listTreeNode*))
{
	sfc* current=specialfc;
	sfc* prev=NULL;
	if(current==NULL)
	{
		specialfc=(sfc*)malloc(sizeof(sfc));
		specialfc->functionName=name;
		specialfc->function=spFun;
		specialfc->next=NULL;
	}
	else
	{
		while(current!=NULL)
		{
			if(!strcmp(current->functionName,name))exit(0);
			prev=current;
			current=current->next;
		}
		current=(sfc*)malloc(sizeof(sfc));
		current->functionName=name;
		current->function=spFun;
		current->next=NULL;
		prev->next=current;
	}
}
void callFunction(listTreeNode* root)
{
	findSpecialFunction((char*)root->left)(root);
}
void (*(findSpecialFunction(const char* name)))(listTreeNode*)
{
	sfc* current=specialfc;
	while(current!=NULL&&strcmp(current->functionName,name))current=current->next;
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
