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

int main(int argc,char** argv)
{
	char* filename=argv[1];
	FILE* fp=(argc>1)?fopen(argv[1],"r"):stdin;
	if(fp==NULL)
	{
		perror(filename);
		return EXIT_FAILURE;
	}
	if(argc==1||isscript(filename))
	{
		if(fp!=stdin)
			changeWorkPath(filename);
		intpr* inter=newIntpr(((fp==stdin)?"stdin":argv[1]),fp,NULL);
		initEvalution();
		runIntpr(inter);
		deleteAllDll(inter->modules);
	}
	else if(iscode(filename))
	{
		changeWorkPath(filename);
		ByteCode* RawProcess=loadRawproc(fp);
		ByteCode* mainprocess=loadByteCode(fp);
		//printByteCode(mainprocess,stdout);
		fakeVM* anotherVM=newFakeVM(mainprocess,RawProcess);
		loadAllModules(fp,&anotherVM->modules);
		VMenv* globEnv=newVMenv(0,NULL);
		anotherVM->mainproc->localenv=globEnv;
		anotherVM->mainproc->code->localenv=globEnv;
		initGlobEnv(globEnv,anotherVM->heap);
		runFakeVM(anotherVM);
		GC_sweep(anotherVM->heap);
		deleteAllDll(anotherVM->modules);
	}
	else
	{
		fprintf(stderr,"error: It is not a correct file.\n");
		return EXIT_FAILURE;
	}
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
	addFunc("lambda",N_lambda);
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
	addFunc("print",N_print);
	addFunc("import",N_import);
}

void runIntpr(intpr* inter)
{
	initPreprocess();
	fakeVM* anotherVM=newFakeVM(NULL,NULL);
	VMenv* globEnv=newVMenv(0,NULL);
	anotherVM->mainproc->localenv=globEnv;
	anotherVM->tid=pthread_self();
	initGlobEnv(globEnv,anotherVM->heap);
	for(;;)
	{
		ANS_cptr* begin=NULL;
		if(inter->file==stdin)printf(">>>");
		int ch=getc(inter->file);
		if(ch==EOF)break;
		else ungetc(ch,inter->file);
		char* list=getListFromFile(inter->file);
	//	printf("%s\n==================\n",list);
		if(list==NULL)continue;
		ErrorStatus status={0,NULL};
		begin=createTree(list,inter);
	//	printList(begin,stderr);
	//	printf("\n==============\n");
		if(begin!=NULL)
		{

			if(isPreprocess(begin))
			{
				status=eval(begin,NULL,inter);
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
				anotherVM->modules=inter->modules;
			}
			else
			{
			//	printList(begin,stdout);
			//	putchar('\n');
				ByteCode* tmpByteCode=compile(begin,inter->glob,inter,&status);
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
					//printByteCode(tmpByteCode,stderr);
					anotherVM->procs=castRawproc(anotherVM->procs,inter->procs);
					anotherVM->mainproc->code=newVMcode(tmpByteCode);
					anotherVM->mainproc->code->localenv=globEnv;
					anotherVM->mainproc->localenv=globEnv;
					anotherVM->mainproc->cp=0;
					anotherVM->curproc=anotherVM->mainproc;
					runFakeVM(anotherVM);
					VMstack* stack=anotherVM->stack;
					if(inter->file==stdin)
					{
						printf("=> ");
						VMvalue* tmp=getTopValue(stack);
						VMpair* tmpPair=(tmp->type==PAIR)?tmp->u.pair:NULL;
						printVMvalue(tmp,tmpPair,stdout,1);
						putchar('\n');
					}
					//fprintf(stderr,"======\n");
					//fprintf(stderr,"stack->tp=%d\n",stack->tp);
					//printAllStack(stack,stderr);
					stack->tp-=1;
					freeByteCode(tmpByteCode);
				}
			}
			free(list);
			list=NULL;
			deleteCptr(begin);
			free(begin);
		}
	}
	GC_sweep(anotherVM->heap);
}

ByteCode* castRawproc(ByteCode* prev,RawProc* procs)
{
	if(procs==NULL)return NULL;
	else
	{
		ByteCode* tmp=(ByteCode*)realloc(prev,sizeof(ByteCode)*(procs->count+1));
		if(tmp==NULL)
		{
			fprintf(stderr,"In file \"%s\",line %d\n",__FILE__,__LINE__);
			errors(OUTOFMEMORY,__FILE__,__LINE__);
		}
		RawProc* curRawproc=procs;
		while(curRawproc!=NULL)
		{
			tmp[curRawproc->count]=*curRawproc->proc;
			curRawproc=curRawproc->next;
		}
		return tmp;
	}
}


ByteCode* loadRawproc(FILE* fp)
{
	int32_t num=0;
	int i=0;
	fread(&num,sizeof(int32_t),1,fp);
	ByteCode* tmp=(ByteCode*)malloc(sizeof(ByteCode)*num);
	if(tmp==NULL)
	{
		fprintf(stderr,"In file \"%s\",line %d\n",__FILE__,__LINE__);
		errors(OUTOFMEMORY,__FILE__,__LINE__);
	}
	for(;i<num;i++)
	{
		int32_t size=0;
		fread(&size,sizeof(int32_t),1,fp);
		tmp[i].size=size;
		tmp[i].code=(char*)malloc(sizeof(char)*size);
		if(tmp[i].code==NULL)
		{
			fprintf(stderr,"In file \"%s\",line %d\n",__FILE__,__LINE__);
			errors(OUTOFMEMORY,__FILE__,__LINE__);
		}
		int32_t j=0;
		for(;j<size;j++)
			tmp[i].code[j]=getc(fp);
	}
	return tmp;
}

ByteCode* loadByteCode(FILE* fp)
{
	int32_t size=0;
	int32_t i=0;
	fread(&size,sizeof(int32_t),1,fp);
	ByteCode* tmp=(ByteCode*)malloc(sizeof(ByteCode));
	if(tmp==NULL)
		errors(OUTOFMEMORY,__FILE__,__LINE__);
	tmp->size=size;
	tmp->code=(char*)malloc(sizeof(char)*size);
	if(tmp->code==NULL)
		errors(OUTOFMEMORY,__FILE__,__LINE__);
	for(;i<size;i++)
		tmp->code[i]=getc(fp);
	return tmp;
}
