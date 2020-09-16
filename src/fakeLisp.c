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
#include"opcode.h"

char* builtInSymbolList[]=
{
	"else",
	"nil",
	"cons",
	"car",
	"cdr",
	"atom",
	"null",
	"ischr",
	"isint",
	"isdbl",
	"isstr",
	"issym",
	"isprc",
	"eq",
	"gt",
	"ge",
	"lt",
	"le",
	"not",
	"dbl",
	"str",
	"sym",
	"chr",
	"int",
	"add",
	"sub",
	"mul",
	"div",
	"mod",
	"gchstr",
	"strlen",
	"strcat",
	"open",
	"close",
	"getc",
	"ungetc",
	"write",
	"tell",
	"seek",
	"rewind",
};

byteCode P_cons=
{
	25,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_PAIR,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_POP_CAR,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_POP_CDR,
		FAKE_END_PROC
	}
};

byteCode P_car=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_CAR,
		FAKE_END_PROC
	}
};

byteCode P_cdr=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_PUSH_CDR,
		FAKE_END_PROC
	}
};

byteCode P_atom=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_ATOM,
		FAKE_END_PROC
	}
};

byteCode P_null=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_NULL,
		FAKE_END_PROC
	}
};

byteCode P_isint=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_IS_INT,
		FAKE_END_PROC
	}
};

byteCode P_ischr=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_IS_CHR,
		FAKE_END_PROC
	}
};

byteCode P_isdbl=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_IS_DBL,
		FAKE_END_PROC
	}
};

byteCode P_isstr=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_IS_STR,
		FAKE_END_PROC
	}
};

byteCode P_issym=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_IS_SYM,
		FAKE_END_PROC
	}
};

byteCode P_isprc=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_IS_PRC,
		FAKE_END_PROC
	}
};

byteCode P_eq=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_EQ,
		FAKE_END_PROC
	}
};

byteCode P_gt=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_GT,
		FAKE_END_PROC
	}
};

byteCode P_ge=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_GE,
		FAKE_END_PROC
	}
};

byteCode P_lt=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_LT,
		FAKE_END_PROC
	}
};

byteCode P_le=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_LE,
		FAKE_END_PROC
	}
};

byteCode P_not=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_NOT,
		FAKE_END_PROC
	}
};

byteCode P_dbl=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_CAST_TO_DBL,
		FAKE_END_PROC
	}
};

byteCode P_str=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_CAST_TO_STR,
		FAKE_END_PROC
	}
};

byteCode P_sym=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_CAST_TO_SYM,
		FAKE_END_PROC
	}
};

byteCode P_chr=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_CAST_TO_CHR,
		FAKE_END_PROC
	}
};

byteCode P_int=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_CAST_TO_INT,
		FAKE_END_PROC
	}
};

byteCode P_add=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_ADD,
		FAKE_END_PROC
	}
};

byteCode P_sub=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_SUB,
		FAKE_END_PROC
	}
};

byteCode P_mul=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_MUL,
		FAKE_END_PROC
	}
};

byteCode P_div=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_DIV,
		FAKE_END_PROC
	}
};

byteCode P_mod=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_MOD,
		FAKE_END_PROC
	}
};

byteCode P_gchstr=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_GET_CHR_STR,
		FAKE_END_PROC
	}
};

byteCode P_strlen=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_STR_LEN,
		FAKE_END_PROC
	}
};

byteCode P_strcat=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_STR_CAT,
		FAKE_END_PROC
	}
};

byteCode P_open=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_OPEN,
		FAKE_END_PROC
	}
};

byteCode P_close=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_CLOSE,
		FAKE_END_PROC
	}
};

byteCode P_getc=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_GETC,
		FAKE_END_PROC
	}
};

byteCode P_ungetc=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_UNGETC,
		FAKE_END_PROC
	}
};

byteCode P_write=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_WRITE,
		FAKE_END_PROC
	}
};

byteCode P_tell=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_TELL,
		FAKE_END_PROC
	}
};

byteCode P_seek=
{
	23,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_POP_VAR,1,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,1,0,0,0,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_SEEK,
		FAKE_END_PROC
	}
};

byteCode P_rewind=
{
	13,
	(char[])
	{
		FAKE_POP_VAR,0,0,0,0,
		FAKE_RES_BP,
		FAKE_PUSH_VAR,0,0,0,0,
		FAKE_REWIND,
		FAKE_END_PROC
	}
};

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
