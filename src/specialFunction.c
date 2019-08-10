#include<stdlib.h>
#include<ctype.h>
#include"floatAndString.h"
#include"specialFunction.h"
void T_add(listTreeNode* objTree)
{
	double result=0;
	listTreeNode* current=objTree;
	while(current->right!=NULL)current=current->right;
	while(current!=objTree)
	{
		if(current->leftType==node)eval(current->left);
		else if(current->leftType==chars&&(isdigit(*(char*)(current->left))||*(char*)(current->left)=='-'))
		{
			result+=stringToFloat((char*)current->left);
			free(current->left);
			current=current->prev;
			free(current->right);
			current->rightType=nil;
			current->right=NULL;
		}
	}
	free(objTree->left);
	objTree->leftType=chars;
	objTree->left=floatToString(result);
	objTree->rightType=nil;
	objTree->right=NULL;
}
void T_sub(listTreeNode* objTree)
{
	double beSub;
	double subNum=0;
	listTreeNode* pBeSub=objTree->right;
	listTreeNode* pSubNum=pBeSub->right;
	if(pBeSub->leftType==node)eval(pBeSub->left);
	if(pBeSub->leftType==chars&&(isdigit(*(char*)(pBeSub->left))||*(char*)(pBeSub->left)=='-'))
		beSub=stringToFloat(pBeSub->left);
	while(pSubNum->rightType!=nil&&pSubNum->right!=NULL)pSubNum=pSubNum->right;
	while(pSubNum!=pBeSub)
	{
		if(pSubNum->leftType==node)eval(pSubNum->left);
		if(pSubNum->leftType==chars&&(isdigit(*(char*)(pSubNum->left))||*(char*)(pSubNum->left)=='-'))
		{
			subNum+=stringToFloat((char*)pSubNum->left);
			free(pSubNum->left);
			pSubNum=pSubNum->prev;
			free(pSubNum->right);
			pSubNum->rightType=nil;
			pSubNum->right=NULL;
		}
	}
	free(pBeSub->left);
	free(pBeSub);
	free(objTree->left);
	objTree->left=floatToString(beSub-subNum);
	objTree->leftType=chars;
	objTree->rightType=nil;
	objTree->right=NULL;
}
void T_mul(listTreeNode* objTree)
{
	double result=1;
	listTreeNode* current=objTree;
	while(current->right!=NULL)current=current->right;
	while(current!=objTree)
	{
		if(current->leftType==node)eval(current->left);
		else if(current->leftType==chars&&(isdigit(*(char*)(current->left))||*(char*)(current->left)=='-'))
		{
			result*=stringToFloat((char*)current->left);
			free(current->left);
			current=current->prev;
			free(current->right);
			current->rightType=nil;
			current->right=NULL;
		}
	}
	free(objTree->left);
	objTree->leftType=chars;
	objTree->left=floatToString(result);
	objTree->rightType=nil;
	objTree->right=NULL;
}
void T_div(listTreeNode* objTree)
{
	double beDiv;
	double divNum=1;
	listTreeNode* pBeDiv=objTree->right;
	listTreeNode* pDivNum=pBeDiv->right;
	if(pBeDiv->leftType==node)eval(pBeDiv->left);
	if(pBeDiv->leftType==chars&&(isdigit(*(char*)(pBeDiv->left))||*(char*)(pBeDiv->left)=='-'))
		beDiv=stringToFloat(pBeDiv->left);
	while(pDivNum->rightType!=nil&&pDivNum->right!=NULL)pDivNum=pDivNum->right;
	while(pDivNum!=pBeDiv)
	{
		if(pDivNum->leftType==node)eval(pDivNum->left);
		if(pDivNum->leftType==chars&&(isdigit(*(char*)(pDivNum->left))||*(char*)(pDivNum->left)=='-'))
		{
			divNum*=stringToFloat((char*)pDivNum->left);
			free(pDivNum->left);
			pDivNum=pDivNum->prev;
			free(pDivNum->right);
			pDivNum->rightType=nil;
			pDivNum->right=NULL;
		}
	}
	free(pBeDiv->left);
	free(pBeDiv);
	free(objTree->left);
	objTree->left=floatToString(beDiv/divNum);
	objTree->leftType=chars;
	objTree->rightType=nil;
	objTree->right=NULL;
}
void T_exit(listTreeNode* objTree)
{
	free(objTree->left);
	free(objTree);//This function should delete the whole tree,so function "deleteTree" will replace "free"	
	exit(0);
}
