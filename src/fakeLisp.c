#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include"fakeLisp.h"
#include"fake.h"
#include"tool.h"
#include"form.h"
#include"pretreatment.h"

int main(int argc,char** argv)
{
	initEvalution();
	while(1)
	{
		cptr* begin=NULL;
		printf(">>>");
		char ch;
		while(isspace(ch=getc(stdin)));
		if(ch==EOF)break;
		ungetc(ch,stdin);
		char* list=getListFromFile(stdin);
		if(list==NULL)continue;
		begin=evalution(list);
		free(list);
		list=NULL;
		fputs(";=>",stdout);
		printList(begin,stdout);
		putchar('\n');
		deleteCptr(begin);
		free(begin);
		begin=NULL;
	}
	return 0;
}

cptr* evalution(const char* objStr)
{
	
	cptr* begin=createTree(objStr);
	eval(begin,NULL);
	return begin;
}

void initEvalution()
{
	addFunc("quote",N_quote);
	addFunc("car",N_car);
	addFunc("cdr",N_cdr);
	addFunc("cons",N_cons);
	addFunc("eq",N_eq);
	addFunc("atom",N_atom);
	addFunc("null",N_null);
	addFunc("cond",N_cond);
	addFunc("and",N_and);
	addFunc("or",N_or);
	addFunc("not",N_not);
	addFunc("define",N_define);
	addFunc("lambda",N_lambda);
	addFunc("list",N_list);
	addFunc("defmacro",N_defmacro);
	addFunc("undef",N_undef);
	addFunc("add",N_add);
	addFunc("sub",N_sub);
	addFunc("mul",N_mul);
	addFunc("div",N_div);
	addFunc("mod",N_mod);
	initPretreatment();
}
