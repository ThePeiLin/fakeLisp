#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
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
	cptr* begin=NULL;
	newEnv();
	while(1)
	{
		printf(">>>");
		char* list=getListFromFile(stdin);
		begin=createTree(list);
		eval(begin,NULL);
		printList(begin,stdout);
		putchar('\n');
		deleteCptr(begin);
		begin=NULL;
		char ch=getchar();
		if(ch!='\n')ungetc(ch,stdin);
	}
	return 0;
}
