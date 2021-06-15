#include"fakeLisp.h"
#include"reader.h"
#include"common.h"
#include"VMtool.h"
#include"syntax.h"
#include"compiler.h"
#include"fakeVM.h"
#include"opcode.h"
#include"ast.h"
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<ctype.h>
#include<pthread.h>
#include<setjmp.h>

static jmp_buf buf;
static int exitStatus=0;

void errorCallBack(void* a)
{
	int* i=(int*)a;
	exitStatus=i[0];
	longjmp(buf,i[1]);
}

extern char* InterpreterPath;
int main(int argc,char** argv)
{
	char* filename=(argc>1)?argv[1]:NULL;
#ifdef WIN32
	InterpreterPath=_fullpath(NULL,argv[0],0);
#else
	InterpreterPath=realpath(argv[0],0);
#endif
	char* t=getDir(InterpreterPath);
	free(InterpreterPath);
	InterpreterPath=t;
	if(argc==1||isscript(filename))
	{
		FILE* fp=(argc>1)?fopen(argv[1],"r"):stdin;
		if(fp==NULL)
		{
			perror(filename);
			free(InterpreterPath);
			return EXIT_FAILURE;
		}
		Intpr* inter=newIntpr(((fp==stdin)?"stdin":argv[1]),fp,NULL,NULL,NULL);
		initGlobKeyWord(inter->glob);
		if(fp==stdin)
			runIntpr(inter);
		else
		{
#ifdef _WIN32
			char* rp=_fullpath(NULL,filename,0);
#else
			char* rp=realpath(filename,0);
#endif
			char* workpath=getDir(rp);
			free(rp);
			int status;
			SymTabNode* node=newSymTabNode(filename);
			addSymTabNode(node,inter->table);
			ByteCodelnt* mainByteCode=compileFile(inter,1,&status);
			if(mainByteCode==NULL)
			{
				free(workpath);
				freeIntpr(inter);
				unInitPreprocess();
				free(InterpreterPath);
				return status;
			}
			inter->lnt->num=mainByteCode->ls;
			inter->lnt->list=mainByteCode->l;
			VMenv* globEnv=newVMenv(NULL);
			FakeVM* anotherVM=newFakeVM(mainByteCode->bc);
			freeByteCode(mainByteCode->bc);
			free(mainByteCode);
			VMrunnable* mainrunnable=anotherVM->rstack->data[0];
			anotherVM->argc=argc-1;
			anotherVM->argv=argv+1;
			anotherVM->tid=pthread_self();
			mainrunnable->localenv=globEnv;
			mainrunnable->proc->prevEnv=NULL;
			anotherVM->callback=errorCallBack;
			anotherVM->table=inter->table;
			anotherVM->lnt=inter->lnt;
			initGlobEnv(globEnv,anotherVM->heap,inter->table);
			chdir(workpath);
			free(workpath);
			if(setjmp(buf)==0)
			{
				runFakeVM(anotherVM);
				joinAllThread();
				freeIntpr(inter);
				unInitPreprocess();
				freeVMheap(anotherVM->heap);
				freeAllVMs();
			}
			else
			{
				deleteCallChain(anotherVM);
				cancelAllThread();
				freeIntpr(inter);
				unInitPreprocess();
				freeVMheap(anotherVM->heap);
				freeAllVMs();
				free(InterpreterPath);
				return exitStatus;
			}
		}
	}
	else if(iscode(filename))
	{
		FILE* fp=fopen(argv[1],"rb");
		if(fp==NULL)
		{
			perror(filename);
			free(InterpreterPath);
			return EXIT_FAILURE;
		}
		changeWorkPath(filename);
		SymbolTable* table=loadSymbolTable(fp);
		LineNumberTable* lnt=loadLineNumberTable(fp);
		ByteCode* mainCode=loadByteCode(fp);
		FakeVM* anotherVM=newFakeVM(mainCode);
		VMheap* heap=anotherVM->heap;
		freeByteCode(mainCode);
		fclose(fp);
		VMrunnable* mainrunnable=anotherVM->rstack->data[0];
		VMenv* globEnv=newVMenv(NULL);
		anotherVM->argc=argc-1;
		anotherVM->argv=argv+1;
		anotherVM->table=table;
		mainrunnable->localenv=globEnv;
		mainrunnable->proc->prevEnv=NULL;
		anotherVM->callback=errorCallBack;
		anotherVM->lnt=lnt;
		initGlobEnv(globEnv,anotherVM->heap,table);
		if(!setjmp(buf))
		{
			runFakeVM(anotherVM);
			joinAllThread();
			freeVMheap(heap);
			freeSymbolTable(table);
			freeAllVMs();
			freeLineNumberTable(lnt);
		}
		else
		{
			deleteCallChain(anotherVM);
			cancelAllThread();
			freeAllVMs();
			freeVMheap(heap);
			freeSymbolTable(table);
			freeLineNumberTable(lnt);
			free(InterpreterPath);
			return exitStatus;
		}
	}
	else
	{
		fprintf(stderr,"%s: It is not a correct file.\n",filename);
		free(InterpreterPath);
		return EXIT_FAILURE;
	}
	free(InterpreterPath);
	return exitStatus;
}

void runIntpr(Intpr* inter)
{
	int e=0;
	FakeVM* anotherVM=newFakeVM(NULL);
	VMenv* globEnv=newVMenv(NULL);
	anotherVM->table=inter->table;
	anotherVM->tid=pthread_self();
	anotherVM->callback=errorCallBack;
	anotherVM->lnt=inter->lnt;
	initGlobEnv(globEnv,anotherVM->heap,inter->table);
	ByteCode* rawProcList=NULL;
	char* prev=NULL;
	int32_t bs=0;
	for(;e<2;)
	{
		AST_cptr* begin=NULL;
		if(inter->file==stdin)printf(">>>");
		int unexpectEOF=0;
		char* list=readInPattern(inter->file,&prev,&unexpectEOF);
		ErrorStatus status={0,NULL};
		if(unexpectEOF)
		{
			switch(unexpectEOF)
			{
				case 1:
					fprintf(stderr,"\nIn file \"%s\",line %d\nerror:Unexpect EOF.\n",inter->filename,inter->curline);
					break;
				case 2:
					fprintf(stderr,"\nIn file \"%s\",line %d\nerror:Invalid expression.\n",inter->filename,inter->curline);
					break;
			}
			free(list);
			list=NULL;
			continue;
		}
		begin=createTree(list,inter,NULL);
		if(isAllSpace(list))
		{
			free(list);
			break;
		}
		if(begin!=NULL)
		{
			ByteCodelnt* tmpByteCode=compile(begin,inter->glob,inter,&status,!isLambdaExpression(begin));
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
			else if(tmpByteCode)
			{
				lntCat(inter->lnt,bs,tmpByteCode->l,tmpByteCode->ls);
				ByteCode byteCodeOfVM={anotherVM->size,anotherVM->code};
				codeCat(&byteCodeOfVM,tmpByteCode->bc);
				anotherVM->code=byteCodeOfVM.code;
				anotherVM->size=byteCodeOfVM.size;
				VMproc* tmp=newVMproc(bs,tmpByteCode->bc->size);
				bs+=tmpByteCode->bc->size;
				freeByteCodelnt(tmpByteCode);
				tmp->prevEnv=NULL;
				VMrunnable* mainrunnable=newVMrunnable(tmp,NULL);
				mainrunnable->localenv=globEnv;
				pushComStack(mainrunnable,anotherVM->rstack);
				globEnv->refcount+=1;
				if(!(e=setjmp(buf)))
				{
					runFakeVM(anotherVM);
					VMstack* stack=anotherVM->stack;
					if(inter->file==stdin&&stack->tp!=0)
					{
						printf(";=>");
						DBG_printVMstack(stack,stdout,0);
					}
					stack->tp=0;
					freeVMproc(tmp);
				}
				else
				{
					VMstack* stack=anotherVM->stack;
					stack->tp=0;
					stack->bp=0;
					VMrunnable* mainrunnable=anotherVM->rstack->data[0];
					tmp=mainrunnable->proc;
					tmp->prevEnv=NULL;
					freeVMproc(tmp);
					deleteCallChain(anotherVM);
				}
			}
			free(list);
			list=NULL;
			deleteCptr(begin);
			free(begin);
		}
		else
		{
			if(list!=NULL)
				free(list);
		}
	}
	freeVMenv(globEnv);
	joinAllThread();
	free(rawProcList);
	freeIntpr(inter);
	unInitPreprocess();
	freeVMheap(anotherVM->heap);
	freeAllVMs();
}

ByteCode* loadByteCode(FILE* fp)
{
	int32_t size=0;
	int32_t i=0;
	fread(&size,sizeof(int32_t),1,fp);
	ByteCode* tmp=(ByteCode*)malloc(sizeof(ByteCode));
	if(tmp==NULL)
		errors("loadByteCode",__FILE__,__LINE__);
	tmp->size=size;
	tmp->code=(uint8_t*)malloc(sizeof(uint8_t)*size);
	if(tmp->code==NULL)
		errors("loadByteCode",__FILE__,__LINE__);
	for(;i<size;i++)
		tmp->code[i]=getc(fp);
	return tmp;
}

SymbolTable* loadSymbolTable(FILE* fp)
{
	int32_t size=0;
	int32_t i=0;
	SymbolTable* tmp=newSymbolTable();
	fread(&size,sizeof(int32_t),1,fp);
	for(;i<size;i++)
	{
		char* symbol=getStringFromFile(fp);
		SymTabNode* node=newSymTabNode(symbol);
		addSymTabNode(node,tmp);
		free(symbol);
	}
	return tmp;
}

LineNumberTable* loadLineNumberTable(FILE* fp)
{
	int32_t size=0;
	int32_t i=0;
	fread(&size,sizeof(int32_t),1,fp);
	LineNumTabNode** list=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*size);
	if(!list)
		errors("loadLineNumberTable",__FILE__,__LINE__);
	for(;i<size;i++)
	{
		int32_t fid=0;
		int32_t scp=0;
		int32_t cpc=0;
		int32_t line=0;
		fread(&fid,sizeof(fid),1,fp);
		fread(&scp,sizeof(scp),1,fp);
		fread(&cpc,sizeof(cpc),1,fp);
		fread(&line,sizeof(line),1,fp);
		LineNumTabNode* n=newLineNumTabNode(fid,scp,cpc,line);
		list[i]=n;
	}
	LineNumberTable* lnt=newLineNumTable();
	lnt->list=list;
	lnt->num=size;
	return lnt;
}
