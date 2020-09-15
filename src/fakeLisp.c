#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include"fakeLisp.h"
#include"tool.h"
#include"form.h"
#include"preprocess.h"
#include"syntax.h"
#include"compiler.h"
#include"fakeVM.h"

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
//	addFunc("undef",N_undef);
	addFunc("add",N_add);
	addFunc("sub",N_sub);
	addFunc("mul",N_mul);
	addFunc("div",N_div);
	addFunc("mod",N_mod);
	addFunc("append",N_append);
	addFunc("extend",N_extend);
//	addFunc("print",N_print);
}

void runIntpr(intpr* inter)
{
	initPreprocess(inter);
	fakeVM* anotherVM=newFakeVM(NULL,NULL);
	varstack* globEnv=newVarStack(0,1,NULL);
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
		errorStatus status={0,NULL};
		begin=createTree(list,inter);
		if(begin!=NULL)
		{
			if(isPreprocess(begin))
			{
				status=eval(begin,NULL);
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
			}
			else
			{
				byteCode* tmpByteCode=compile(begin,inter->glob,inter,&status);
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
				{
					anotherVM->procs=castRawproc(anotherVM->procs,inter->procs);
					anotherVM->mainproc->code=tmpByteCode->code;
					anotherVM->mainproc->localenv=globEnv;
					anotherVM->mainproc->size=tmpByteCode->size;
					anotherVM->mainproc->cp=0;
					anotherVM->curproc=anotherVM->mainproc;
				//	printByteCode(tmpByteCode);
					runFakeVM(anotherVM);
					fakestack* stack=anotherVM->stack;
					printf("=> ");
					printStackValue(getTopValue(stack),stdout);
					freeStackValue(getTopValue(stack));
					stack->tp-=1;
					putchar('\n');
					freeByteCode(tmpByteCode);
				}
			}
			free(list);
			list=NULL;
			deleteCptr(begin);
			free(begin);
		}
	}
}

byteCode* castRawproc(byteCode* prev,rawproc* procs)
{
	if(procs==NULL)return NULL;
	else
	{
		byteCode* tmp=(byteCode*)realloc(prev,sizeof(byteCode)*(procs->count+1));
		if(tmp==NULL)errors(OUTOFMEMORY);
		rawproc* curRawproc=procs;
		while(curRawproc!=NULL)
		{
			tmp[curRawproc->count]=*curRawproc->proc;
			curRawproc=curRawproc->next;
		}
		return tmp;
	}
}
