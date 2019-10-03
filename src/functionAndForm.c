#include<stdlib.h>
#include<ctype.h>
#include"numAndString.h"
#include"functionAndForm.h"
void T_add(listTreeNode* objTree)
{
	long result=0;
	listTreeNode* current=objTree;
	while(current->right!=NULL)current=current->right;
	while(current!=objTree)
	{
		if(current->leftType==node)eval(current->left);
		else if(current->leftType==val&&(isdigit(*(char*)(current->left))||*(char*)(current->left)=='-'))
		{
			result+=stringToInt((char*)current->left);
			current=current->prev;
		}
	}
	free(objTree->left);
	deleteNode(objTree->right);
	objTree->leftType=val;
	objTree->left=intToString(result);
	objTree->rightType=nil;
	objTree->right=NULL;
}
void T_sub(listTreeNode* objTree)
{
	long beSub;
	long subNum=0;
	listTreeNode* pBeSub=objTree->right;
	listTreeNode* pSubNum=pBeSub->right;
	if(pBeSub->leftType==node)eval(pBeSub->left);
	if(pBeSub->leftType==val&&(isdigit(*(char*)(pBeSub->left))||*(char*)(pBeSub->left)=='-'))
		beSub=stringToInt(pBeSub->left);
	while(pSubNum->rightType!=nil&&pSubNum->right!=NULL)pSubNum=pSubNum->right;
	while(pSubNum!=pBeSub)
	{
		if(pSubNum->leftType==node)eval(pSubNum->left);
		if(pSubNum->leftType==val&&(isdigit(*(char*)(pSubNum->left))||*(char*)(pSubNum->left)=='-'))
		{
			subNum+=stringToInt((char*)pSubNum->left);
			pSubNum=pSubNum->prev;
		}
	}
	free(objTree->left);
	deleteNode(objTree->right);
	objTree->left=intToString(beSub-subNum);
	objTree->leftType=val;
	objTree->rightType=nil;
	objTree->right=NULL;
}
void T_mul(listTreeNode* objTree)
{
	long result=1;
	listTreeNode* current=objTree;
	while(current->right!=NULL)current=current->right;
	while(current!=objTree)
	{
		if(current->leftType==node)eval(current->left);
		else if(current->leftType==val&&(isdigit(*(char*)(current->left))||*(char*)(current->left)=='-'))
		{
			result*=stringToInt((char*)current->left);
			current=current->prev;
		}
	}
	free(objTree->left);
	objTree->leftType=val;
	objTree->left=intToString(result);
	deleteNode(objTree->right);
	objTree->rightType=nil;
	objTree->right=NULL;
}
void T_div(listTreeNode* objTree)
{
	long quotient=0;
	long remainder=0;
	long beDiv=0;
	long divNum=0;
	listTreeNode* pBeDiv=objTree->right;
	if(pBeDiv!=NULL)
	{
		if(pBeDiv->leftType==node)eval(pBeDiv->left);
		else beDiv=stringToInt(pBeDiv->left);
	}
	else exit(0);
	listTreeNode* pDivNum=pBeDiv->right;
	if(pDivNum!=NULL)
	{
		if(pDivNum->leftType==node)eval(pDivNum->left);
		else divNum=stringToInt(pDivNum->left);
	}
	else exit(0);
	remainder=beDiv%divNum;
	quotient=(beDiv-remainder)/divNum;
	free(objTree->left);
	listTreeNode* tmp=createNode(objTree);
	tmp->left=intToString(quotient);
	tmp->leftType=val;
	tmp->rightType=nil;
	deleteNode(objTree->right);
	objTree->leftType=node;
	objTree->left=tmp;
	objTree->rightType=nil;
	objTree->right=NULL;
}
void T_mod(listTreeNode* objTree)
{
	long result=0;
	long beMod=0;
	long modNum=0;
	listTreeNode* pBeMod=objTree->right;
	if(pBeMod!=NULL)
	{
		if(pBeMod->leftType==node)eval(pBeMod->left);
		else beMod=stringToInt(pBeMod->left);
	}
	else exit(0);
	listTreeNode* pModNum=pBeMod->right;
	if(pModNum!=NULL)
	{
		if(pModNum->leftType==node)eval(pModNum->left);
		else modNum=stringToInt(pModNum->left);
	}
	else exit(0);
	result=beMod%modNum;
	free(objTree->left);
	objTree->left=intToString(result);
	objTree->leftType=val;
	deleteNode(objTree->right);
	objTree->rightType=nil;
	objTree->right=NULL;
}
void T_exit(listTreeNode* objTree)
{
	deleteTree(objTree);
	exit(0);
}
