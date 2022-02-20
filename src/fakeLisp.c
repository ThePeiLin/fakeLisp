#include"fakeLisp.h"
#include<fakeLisp/reader.h>
#include<fakeLisp/common.h>
#include<fakeLisp/VMtool.h>
#include<fakeLisp/syntax.h>
#include<fakeLisp/compiler.h>
#include<fakeLisp/fakeVM.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/ast.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<ctype.h>
#include<pthread.h>
#include<setjmp.h>
static jmp_buf buf;
static int exitState=0;

void errorCallBack(void* a)
{
	int* i=(int*)a;
	exitState=255;
	longjmp(buf,i[(sizeof(void*)*2)/sizeof(int)]);
}

extern char* InterpreterPath;
extern struct GlobTypeUnionListElem GlobTypeUnionList;
int main(int argc,char** argv)
{
	char* filename=(argc>1)?argv[1]:"stdin";
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
		addSymbolToGlob(filename);
		initGlobKeyWord(inter->glob);
		initNativeDefTypes(inter->deftypes);
		initBuiltInStringPattern();
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
			int state;
			ByteCodelnt* mainByteCode=compileFile(inter,1,&state);
			if(mainByteCode==NULL)
			{
				free(workpath);
				freeIntpr(inter);
				unInitPreprocess();
				free(InterpreterPath);
				freeGlobSymbolTable();
				freeGlobTypeList();
				return state;
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
			anotherVM->callback=errorCallBack;
			anotherVM->lnt=inter->lnt;
			initGlobEnv(globEnv,anotherVM->heap);
			chdir(workpath);
			free(workpath);
			if(setjmp(buf)==0)
			{
				runFakeVM(anotherVM);
				joinAllThread();
				freeIntpr(inter);
				unInitPreprocess();
				freeVMheap(anotherVM->heap);
				freeGlobSymbolTable();
				freeGlobTypeList();
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
				freeGlobSymbolTable();
				freeGlobTypeList();
				return exitState;
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
		loadSymbolTable(fp);
		LineNumberTable* lnt=loadLineNumberTable(fp);
		loadTypeList(fp);
		ByteCode* mainCode=loadByteCode(fp);
		FakeVM* anotherVM=newFakeVM(mainCode);
		VMheap* heap=anotherVM->heap;
		freeByteCode(mainCode);
		fclose(fp);
		VMrunnable* mainrunnable=anotherVM->rstack->data[0];
		VMenv* globEnv=newVMenv(NULL);
		anotherVM->argc=argc-1;
		anotherVM->argv=argv+1;
		mainrunnable->localenv=globEnv;
		anotherVM->callback=errorCallBack;
		anotherVM->lnt=lnt;
		initGlobEnv(globEnv,anotherVM->heap);
		if(!setjmp(buf))
		{
			runFakeVM(anotherVM);
			joinAllThread();
			freeVMheap(heap);
			freeGlobSymbolTable();
			freeGlobTypeList();
			freeAllVMs();
			freeLineNumberTable(lnt);
		}
		else
		{
			deleteCallChain(anotherVM);
			cancelAllThread();
			freeAllVMs();
			freeVMheap(heap);
			freeGlobSymbolTable();
			freeGlobTypeList();
			freeLineNumberTable(lnt);
			free(InterpreterPath);
			return exitState;
		}
	}
	else
	{
		fprintf(stderr,"%s: It is not a correct file.\n",filename);
		free(InterpreterPath);
		return EXIT_FAILURE;
	}
	free(InterpreterPath);
	return exitState;
}

void runIntpr(Intpr* inter)
{
	int e=0;
	FakeVM* anotherVM=newFakeVM(NULL);
	VMenv* globEnv=newVMenv(NULL);
	anotherVM->tid=pthread_self();
	anotherVM->callback=errorCallBack;
	anotherVM->lnt=inter->lnt;
	initGlobEnv(globEnv,anotherVM->heap);
	ByteCode* rawProcList=NULL;
	char* prev=NULL;
	int32_t bs=0;
	for(;e<2;)
	{
		AST_cptr* begin=NULL;
		if(inter->file==stdin)printf(">>>");
		int unexpectEOF=0;
		char* list=readInPattern(inter->file,&prev,&unexpectEOF);
		ErrorState state={0,NULL};
		if(unexpectEOF)
		{
			switch(unexpectEOF)
			{
					case 1:
						fprintf(stderr,"error of reader:Unexpect EOF at line %d of %s\n",inter->curline,inter->filename);
						break;
					case 2:
						fprintf(stderr,"error of reader:Invalid expression at line %d of %s\n",inter->curline,inter->filename);
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
			ByteCodelnt* tmpByteCode=compile(begin,inter->glob,inter,&state,!isLambdaExpression(begin));
			if(state.state!=0)
			{
				exError(state.place,state.state,inter);
				deleteCptr(state.place);
				if(inter->file!=stdin)
				{
					deleteCptr(begin);
					freeGlobSymbolTable();
					freeGlobTypeList();
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
				VMrunnable* mainrunnable=newVMrunnable(tmp);
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
					if(e>=2&&prev)
						free(prev);
					VMstack* stack=anotherVM->stack;
					stack->tp=0;
					stack->bp=0;
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
	freeGlobSymbolTable();
	freeGlobTypeList();
}

ByteCode* loadByteCode(FILE* fp)
{
	int32_t size=0;
	int32_t i=0;
	fread(&size,sizeof(int32_t),1,fp);
	ByteCode* tmp=(ByteCode*)malloc(sizeof(ByteCode));
	FAKE_ASSERT(tmp,"loadByteCode",__FILE__,__LINE__);
	tmp->size=size;
	tmp->code=(uint8_t*)malloc(sizeof(uint8_t)*size);
	FAKE_ASSERT(tmp->code,"loadByteCode",__FILE__,__LINE__);
	for(;i<size;i++)
		tmp->code[i]=getc(fp);
	return tmp;
}

void loadSymbolTable(FILE* fp)
{
	int32_t size=0;
	int32_t i=0;
	fread(&size,sizeof(int32_t),1,fp);
	for(;i<size;i++)
	{
		char* symbol=getStringFromFile(fp);
		addSymbolToGlob(symbol);
		free(symbol);
	}
}

LineNumberTable* loadLineNumberTable(FILE* fp)
{
	int32_t size=0;
	int32_t i=0;
	fread(&size,sizeof(int32_t),1,fp);
	LineNumTabNode** list=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*size);
	FAKE_ASSERT(list,"loadLineNumberTable",__FILE__,__LINE__);
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
