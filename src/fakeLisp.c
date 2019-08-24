#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include"fake.h"
#include"specialFunction.h"
#include"floatAndString.h"
sfc* specialfc=0;
int isScript(const char*);
int main(int argc,char** argv)
{
	addSpecialFunction("add",T_add);
	addSpecialFunction("sub",T_sub);
	addSpecialFunction("mul",T_mul);
	addSpecialFunction("div",T_div);
	addSpecialFunction("exit",T_exit);
	if(argc>1)
	{
		if(isScript(argv[1]))
		{
			FILE* file;
			if((file=fopen(argv[1],"r"))==NULL)
			{
				printf("Can't open %s\n",argv[1]);
				exit(EXIT_FAILURE);
			}
			else
			{
				while(1)
				{
					char ch;
					char* str=NULL;
					ch=getc(file);
					if(ch=='(')
					{
						listTreeNode* objTree;
						ungetc(ch,file);
						str=getListFromFile(file);
						if(str!=NULL)objTree=eval(becomeTree(str));
						free(str);
					}
					else if(ch==EOF)break;
				}
			}
		}
		else
		{
			printf("\"%s\" is not a script.\n",argv[1]);
			exit(EXIT_FAILURE);
		}
	}
	else if(argc<2)
	{
		puts("Ready");
		printf(">>>");
		while(1)
		{
			char ch;
			char* str=NULL;
			ch=getc(stdin);
			if(ch=='(')
			{
				listTreeNode* objTree;
				ungetc(ch,stdin);
				str=getListFromFile(stdin);
				if(str!=NULL)objTree=eval(becomeTree(str));
				free(str);
				printf("%s\n",(char*)objTree->left);
			}
			if(ch=='\n')printf(">>>");
		}
	}
	return 0;
}
int isScript(const char* filename)
{
	char back[4];
	int i;
	int len=strlen(filename);
	for(i=0;i<3;i++)back[i]=filename[len-3+i];
	back[3]=0;
	return !strcmp(back,".fl");
}
