#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include"fakeLisp.h"
#include"fake.h"
#include"tool.h"
#include"form.h"
#include"preprocess.h"

int main(int argc,char** argv)
{
	FILE* fp=(argc>1)?fopen(argv[1],"r"):stdin;
	if(fp==NULL)
	{
		perror(argv[1]);
		return EXIT_FAILURE;
	}
	intpr* inter=newIntpr(((fp==stdin)?"stdin":argv[1]),fp);
	initEvalution();
	runIntpr(inter);
	return 0;
}

cptr* evalution(const char* objStr,intpr* inter)
{
	errorStatus status={0,NULL};
	cptr* begin=createTree(objStr,inter);
	status=eval(begin,inter->glob);
	if(status.status!=0)
	{
		exError(status.place,status.status,inter);
		deleteCptr(status.place);
		if(inter->file!=stdin)
		{
			deleteCptr(begin);
			exit(0);
		}
	}
	else 
		if(begin!=NULL&&inter->file==stdin)
		{
			fputs(";=>",stdout);
			printList(begin,stdout);
			putchar('\n');
		}
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
//	addFunc("lambda",N_lambda);
	addFunc("list",N_list);
	addFunc("defmacro",N_defmacro);
	addFunc("undef",N_undef);
	addFunc("add",N_add);
	addFunc("sub",N_sub);
	addFunc("mul",N_mul);
	addFunc("div",N_div);
	addFunc("mod",N_mod);
//	addFunc("print",N_print);
	initPreprocess();
}

void runIntpr(intpr* inter)
{
	for(;;)
	{
		cptr* begin=NULL;
		if(inter->file==stdin)printf(">>>");
		char ch;
		while(isspace(ch=getc(inter->file)))if(ch=='\n')inter->curline+=1;;
		if(ch==EOF)break;
		ungetc(ch,inter->file);
		char* list=getListFromFile(inter->file);
		if(list==NULL)continue;
		begin=evalution(list,inter);
		free(list);
		list=NULL;
		deleteCptr(begin);
		free(begin);
		begin=NULL;
	}
}
