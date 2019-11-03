#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include"fake.h"
#include"numAndString.h"
static nativeFunc* funAndForm=NULL;
static defines* GlobDef=NULL;

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
		if(!(tmp=(char*)malloc(sizeof(char)*i)))errors(OUTOFMEMORY);
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




listTreeNode* becomeTree(const char* objStr)
{
	int i=0;
	listTreeNode* root=NULL;
	listTreeNode* objNode=NULL;
	branch* objBra;
	while(1)
	{
		if(*(objStr+i)=='(')
		{
			i++;
			if(root==NULL)
			{
				root=createNode(NULL);
				objNode=root;
				objBra=&objNode->left;
			}
			else
			{
				objNode=createNode(objNode);
				objBra->type=node;
				objBra->twig=(void*)objNode;
				objBra=&objNode->left;
			}
		}
		else if(*(objStr+i)==')')
		{
			i++;
			listTreeNode* prev=NULL;
			if(objNode==NULL)break;
			while(1)
			{
				prev=objNode;
				objNode=objNode->prev;
				if(objNode==NULL||objNode->left.twig==prev)break;
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
			listTreeNode* tmp=createNode(objNode);
			objNode->right.type=node;
			objNode->right.twig=(void*)tmp;
			objNode=tmp;
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
		if(objTree->left.type==node)objTree=objTree->left.twig;
		if(objTree->left.type==nil&&objTree->right.type==node)objTree=objTree->right.twig;
		if(objTree->left.type==nil&&objTree->right.type==nil)
		{
			listTreeNode* prev=objTree;
			objTree=objTree->prev;
			if(objTree==NULL)
			{
				free(prev);
				break;
			}
			if(objTree->left.twig==prev)objTree->left.type=nil;
			else if(objTree->right.twig==prev)objTree->right.type=nil;
			free(prev);
		}
		if(objTree->left.type==sym)
		{
			free(objTree->left.twig);
			objTree->left.twig=NULL;
			objTree->left.type=nil;
		}
		if(objTree->left.type==nil&&objTree->right.type==sym)
		{
			free(objTree->right.twig);
			objTree->right.twig=NULL;
			objTree->right.type=nil;
		}
		if(objTree->left.type==val)
		{
			free(((leaf*)objTree->left.twig)->value);
			free(objTree->left.twig);
			objTree->left.twig=NULL;
			objTree->left.type=nil;
		}
		if(objTree->left.type==nil&&objTree->right.type==val)
		{
			free(((leaf*)objTree->right.twig)->value);
			free(objTree->right.twig);
			objTree->right.twig=NULL;
			objTree->right.type=nil;
		}
	}
}
listTreeNode* deleteNode(listTreeNode* objTree)
{
	listTreeNode* tmp=objTree;
	while(1)
	{
		if(objTree->left.type==node)objTree=objTree->left.twig;
		if(objTree->left.type==nil&&objTree->right.type==node)objTree=objTree->right.twig;
		if(objTree->left.type==nil&&objTree->right.type==nil)
		{
			listTreeNode* prev=objTree;
			objTree=objTree->prev;
			if(objTree==NULL)
			{
				free(prev);
				break;
			}
			if(objTree->left.twig==prev)objTree->left.type=nil;
			else if(objTree->right.twig==prev)objTree->right.type=nil;
			free(prev);
		}
		if(objTree->left.type==sym)
		{
			free(objTree->left.twig);
			objTree->left.twig=NULL;
			objTree->left.type=nil;
		}
		if(objTree->left.type==nil&&objTree->right.type==sym)
		{
			free(objTree->right.twig);
			objTree->right.twig=NULL;
			objTree->right.type=nil;
		}
		if(objTree->left.type==val)
		{
			free(((leaf*)objTree->left.twig)->value);
			free(objTree->left.twig);
			objTree->left.twig=NULL;
			objTree->left.type=nil;
		}
		if(objTree->left.type==nil&&objTree->right.type==val)
		{
			free(((leaf*)objTree->right.twig)->value);
			free(objTree->right.twig);
			objTree->right.twig=NULL;
			objTree->right.type=nil;
		}
		if(objTree==tmp)
		{
			tmp=objTree->prev;
			free(objTree);
			if(tmp->left.twig==objTree)tmp->left.type=nil;
			else if(tmp->right.twig==objTree)tmp->right.type=nil;
			break;
		}
	}
}
void printList(listTreeNode* objTree,FILE* out)
{
	if(objTree->prev==NULL||objTree->prev->left.twig==objTree)putc('(',out);
	if(objTree->left.type==sym)fputs(objTree->left.twig,out);
	else if(objTree->left.type=val)printRawString(((leaf*)objTree->left.twig)->value,out);
	else if(objTree->left.type==node)printList(objTree->left.twig,out);
	if(objTree->right.type==sym)
	{
		putc(',',out);
		printRawString(objTree->right.twig,out);
	}
	else if(objTree->right.type==val)
	{
		putc(',',out);
		printRawString(((leaf*)objTree->right.twig)->value,out);
	}
	else if(objTree->right.type==node)
	{
		putc(' ',out);
		printList(objTree->right.twig,out);
	}
	if(objTree->prev==NULL||(listTreeNode*)(objTree->prev)->left.twig==objTree)putc(')',out);
}
listTreeNode* createNode(listTreeNode* const prev)
{
	listTreeNode* tmp;
	if((tmp=(listTreeNode*)malloc(sizeof(listTreeNode))))
	{
		tmp->prev=prev;
		tmp->left.type=nil;
		tmp->left.twig=NULL;
		tmp->right.type=nil;
		tmp->right.twig=NULL;
	}
	else errors(OUTOFMEMORY);
}
listTreeNode* copyTree(listTreeNode* objTree)
{
	listTreeNode* tmp=createNode(NULL);
	if(objTree->left.type==node)
	{
		tmp->left.type=node;
		tmp->left.twig=copyTree(objTree->left.twig);
		listTreeNode* tmp1=tmp->left.twig;
		tmp1->prev=tmp;
	}
	if(objTree->right.type==node)
	{
		tmp->right.type=node;
		tmp->right.twig=copyTree(objTree->right.twig);
		listTreeNode* tmp1=tmp->right.twig;
		tmp1->prev=tmp;
	}
	if(objTree->left.type==sym)
	{
		int len=strlen(objTree->left.twig)+1;
		tmp->left.type=objTree->left.type;
		if(!(tmp->left.twig=(char*)malloc(sizeof(char)*len)))errors(OUTOFMEMORY);
		memcpy(tmp->left.twig,objTree->left.twig,len);
	}
	if(objTree->left.type==sym)
	{
		int len=strlen(objTree->right.twig)+1;
		tmp->right.type=objTree->right.type;
		if(!(tmp->right.twig=(char*)malloc(sizeof(char)*len)))errors(OUTOFMEMORY);
		memcpy(tmp->right.twig,objTree->right.twig,len);
	}
	if(objTree->left.type==val)
	{
		tmp->left.type=objTree->left.type;
		int len=strlen(((leaf*)objTree->left.twig)->value)+1;
		if(!(tmp->left.twig=(leaf*)malloc(sizeof(leaf))))errors(OUTOFMEMORY);
		if(!(((leaf*)tmp->left.twig)->value=(char*)malloc(sizeof(char)*len)))errors(OUTOFMEMORY);
		memcpy(((leaf*)tmp->left.twig)->value,((leaf*)objTree->left.twig)->value,len);
	}
	if(objTree->right.type==val)
	{
		tmp->right.type=objTree->right.type;
		int len=strlen(((leaf*)objTree->right.twig)->value)+1;
		if(!(tmp->right.twig=(leaf*)malloc(sizeof(leaf))))errors(OUTOFMEMORY);
		if(!(((leaf*)tmp->right.twig)->value=(char*)malloc(sizeof(char)*len)))errors(OUTOFMEMORY);
		memcpy(((leaf*)tmp->right.twig)->value,((leaf*)objTree->right.twig)->value,len);
	}
	return tmp;
}



/*int addDefine(const char* symName,void* obj,enum TYPES types)
{
	defines* current=NULL;
	defines* prev=NULL;
	int len=strlen(symName)+1;
	if(def==NULL)
	{
		int i;
		if(!(def=(defines*)malloc(sizeof(defines))))errors(OUTOFMEMORY);
		if(!(def->symName=(char*)malloc(sizeof(char)*len)))errors(OUTOFMEMORY);
		for(i=0;i<len;i++)*(def->symName+i)=*(symName+i);
		def->next=NULL;
		if(types==node)def->NSC=copyTree(obj);
		else if(types==val||types==sym)
		{
			int len=strlen((char*)obj->value)+1;
			if(!(def->NSC=(leaf*)malloc(sizeof(leaf))))errors(OUTOFMEMORY);
			if(!((leaf*)(def->NSC)->value=(char*)malloc(sizeof(char)*len)))errors(OUTOFMEMORY);
			for(i=0;i<len;i++)*((char*)((leaf*)(def->NSC)->value)+i)=*((char*)obj+i);
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
		else if(types==val||types==sym)
		{
			int len=strlen((char*)obj->value)+1;
			if(!(current->NSC=(leaf*)malloc(sizeof(leaf))))errors(OUTOFMEMORY);
			if(!((leaf*)(current->NSC)->value=(char*)malloc(sizeof(char)*len)))errors(OUTOFMEMORY);
			for(i=0;i<len;i++)*((char*)((leaf*)(current->NSC)->value)+i)=*((char*)obj+i);
		}
		else if(types==nil)current->NSC=NULL;
		current->type=types;
	}
}*/


void errors(int types)
{
	static char* inform[]=
	{
		"dummy",
		"Out of memory!\n"
	};
	fprintf(stderr,"error:%s",inform[types]);
	exit(1);
}
