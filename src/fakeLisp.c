#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include"fakeLisp.h"
#include"fake.h"
#include"tool.h"
#include"form.h"
int main(int argc,char** argv)
{
	addFunc("quote",N_quote);
	addFunc("car",N_car);
	addFunc("cdr",N_cdr);
	addFunc("cons",N_cons);
	addFunc("eq",N_eq);
	addFunc("atom",N_atom);
	addFunc("null",N_null);
	addFunc("cond",N_cond);
	addFunc("define",N_define);
	addFunc("lambda",N_lambda);
	addFunc("list",N_list);
	cptr* begin=NULL;
	newEnv();
	while(1)
	{
		printf(">>>");
		char ch;
		while(isspace(ch=getc(stdin)));
		if(ch==EOF)break;
		ungetc(ch,stdin);
		char* list=getListFromFile(stdin);
		begin=createTree(list);
		free(list);
		list=NULL;
		int status=eval(begin,NULL);
		if(status!=0)return SYNTAXERROR;
		fputs(";=>",stdout);
		printList(begin,stdout);
		putchar('\n');
		deleteCptr(begin);
		free(begin);
		begin=NULL;
	}
	return 0;
}
