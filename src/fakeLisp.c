#include"fakeLisp.h"
#include<fakeLisp/reader.h>
#include<fakeLisp/syntax.h>
#include<fakeLisp/compiler.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/vmutils.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/ast.h>
#include"utils.h"
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
#ifdef WFKL_I32
	InterpreterPath=_fullpath(NULL,argv[0],0);
#else
	InterpreterPath=realpath(argv[0],0);
#endif
	char* t=fklGetDir(InterpreterPath);
	free(InterpreterPath);
	InterpreterPath=t;
	if(argc==1||fklIsscript(filename))
	{
		FILE* fp=(argc>1)?fopen(argv[1],"r"):stdin;
		if(fp==NULL)
		{
			perror(filename);
			free(InterpreterPath);
			return EXIT_FAILURE;
		}
		FklIntpr* inter=fklNewIntpr(((fp==stdin)?"stdin":argv[1]),fp,NULL,NULL,NULL);
		fklAddSymbolToGlob(filename);
		fklInitGlobKeyWord(inter->glob);
		fklInitNativeDefTypes(inter->deftypes);
		fklInitBuiltInStringPattern();
		if(fp==stdin)
			runIntpr(inter);
		else
		{
#ifdef _WIN32
			char* rp=_fullpath(NULL,filename,0);
#else
			char* rp=realpath(filename,0);
#endif
			char* workpath=fklGetDir(rp);
			free(rp);
			int state;
			FklByteCodelnt* mainByteCode=fklCompileFile(inter,1,&state);
			fklFreeAllSharedObj();
			if(mainByteCode==NULL)
			{
				free(workpath);
				fklFreeIntpr(inter);
				fklUnInitPreprocess();
				free(InterpreterPath);
				fklFreeGlobSymbolTable();
				fklFreeGlobTypeList();
				return state;
			}
			inter->lnt->num=mainByteCode->ls;
			inter->lnt->list=mainByteCode->l;
			FklVMenv* globEnv=fklNewVMenv(NULL);
			FklVM* anotherVM=fklNewVM(mainByteCode->bc);
			fklFreeByteCode(mainByteCode->bc);
			free(mainByteCode);
			FklVMrunnable* mainrunnable=anotherVM->rstack->data[0];
			anotherVM->argc=argc-1;
			anotherVM->argv=argv+1;
			anotherVM->tid=pthread_self();
			mainrunnable->localenv=globEnv;
			anotherVM->callback=errorCallBack;
			anotherVM->lnt=inter->lnt;
			fklInitGlobEnv(globEnv,anotherVM->heap);
			chdir(workpath);
			free(workpath);
			if(setjmp(buf)==0)
			{
				fklRunVM(anotherVM);
				fklJoinAllThread();
				fklFreeIntpr(inter);
				fklUnInitPreprocess();
				fklFreeVMheap(anotherVM->heap);
				fklFreeGlobSymbolTable();
				fklFreeGlobTypeList();
				fklFreeAllVMs();
				fklFreeAllSharedObj();
			}
			else
			{
				fklDeleteCallChain(anotherVM);
				fklCancelAllThread();
				fklFreeIntpr(inter);
				fklUnInitPreprocess();
				fklFreeVMheap(anotherVM->heap);
				fklFreeAllVMs();
				fklFreeAllSharedObj();
				free(InterpreterPath);
				fklFreeGlobSymbolTable();
				fklFreeGlobTypeList();
				return exitState;
			}
		}
	}
	else if(fklIscode(filename))
	{
		FILE* fp=fopen(argv[1],"rb");
		if(fp==NULL)
		{
			perror(filename);
			free(InterpreterPath);
			return EXIT_FAILURE;
		}
		fklChangeWorkPath(filename);
		loadSymbolTable(fp);
		LineNumberTable* lnt=loadLineNumberTable(fp);
		fklLoadTypeList(fp);
		FklByteCode* mainCode=loadByteCode(fp);
		FklVM* anotherVM=fklNewVM(mainCode);
		VMheap* heap=anotherVM->heap;
		fklFreeByteCode(mainCode);
		fclose(fp);
		FklVMrunnable* mainrunnable=anotherVM->rstack->data[0];
		FklVMenv* globEnv=fklNewVMenv(NULL);
		anotherVM->argc=argc-1;
		anotherVM->argv=argv+1;
		mainrunnable->localenv=globEnv;
		anotherVM->callback=errorCallBack;
		anotherVM->lnt=lnt;
		fklInitGlobEnv(globEnv,anotherVM->heap);
		if(!setjmp(buf))
		{
			fklRunVM(anotherVM);
			fklJoinAllThread();
			fklFreeVMheap(heap);
			fklFreeGlobSymbolTable();
			fklFreeGlobTypeList();
			fklFreeAllVMs();
			fklFreeAllSharedObj();
			fklFreeLineNumberTable(lnt);
		}
		else
		{
			fklDeleteCallChain(anotherVM);
			fklCancelAllThread();
			fklFreeAllVMs();
			fklFreeAllSharedObj();
			fklFreeVMheap(heap);
			fklFreeGlobSymbolTable();
			fklFreeGlobTypeList();
			fklFreeLineNumberTable(lnt);
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

void runIntpr(FklIntpr* inter)
{
	int e=0;
	FklVM* anotherVM=fklNewVM(NULL);
	FklVMenv* globEnv=fklNewVMenv(NULL);
	anotherVM->tid=pthread_self();
	anotherVM->callback=errorCallBack;
	anotherVM->lnt=inter->lnt;
	fklInitGlobEnv(globEnv,anotherVM->heap);
	FklByteCode* rawProcList=NULL;
	char* prev=NULL;
	int32_t bs=0;
	for(;e<2;)
	{
		FklAstCptr* begin=NULL;
		if(inter->file==stdin)printf(">>>");
		int unexpectEOF=0;
		char* list=fklReadInPattern(inter->file,&prev,&unexpectEOF);
		FklErrorState state={0,NULL};
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
		begin=fklCreateTree(list,inter,NULL);
		if(fklIsAllSpace(list))
		{
			free(list);
			break;
		}
		if(begin!=NULL)
		{
			FklByteCodelnt* tmpByteCode=fklCompile(begin,inter->glob,inter,&state,!fklIsLambdaExpression(begin));
			if(state.state!=0)
			{
				fklExError(state.place,state.state,inter);
				fklDeleteCptr(state.place);
				if(inter->file!=stdin)
				{
					fklDeleteCptr(begin);
					fklFreeGlobSymbolTable();
					fklFreeGlobTypeList();
					exit(0);
				}
			}
			else if(tmpByteCode)
			{
				fklLntCat(inter->lnt,bs,tmpByteCode->l,tmpByteCode->ls);
				FklByteCode byteCodeOfVM={anotherVM->size,anotherVM->code};
				fklCodeCat(&byteCodeOfVM,tmpByteCode->bc);
				anotherVM->code=byteCodeOfVM.code;
				anotherVM->size=byteCodeOfVM.size;
				FklVMproc* tmp=fklNewVMproc(bs,tmpByteCode->bc->size);
				bs+=tmpByteCode->bc->size;
				fklFreeByteCodelnt(tmpByteCode);
				tmp->prevEnv=NULL;
				FklVMrunnable* mainrunnable=fklNewVMrunnable(tmp);
				mainrunnable->localenv=globEnv;
				fklPushPtrStack(mainrunnable,anotherVM->rstack);
				globEnv->refcount+=1;
				if(!(e=setjmp(buf)))
				{
					fklRunVM(anotherVM);
					FklVMstack* stack=anotherVM->stack;
					if(inter->file==stdin&&stack->tp!=0)
					{
						printf(";=>");
						fklDBG_printVMstack(stack,stdout,0);
					}
					stack->tp=0;
					fklFreeVMproc(tmp);
				}
				else
				{
					if(e>=2&&prev)
						free(prev);
					FklVMstack* stack=anotherVM->stack;
					stack->tp=0;
					stack->bp=0;
					tmp->prevEnv=NULL;
					fklFreeVMproc(tmp);
					fklDeleteCallChain(anotherVM);
				}
			}
			free(list);
			list=NULL;
			fklDeleteCptr(begin);
			free(begin);
		}
		else
		{
			if(list!=NULL)
				free(list);
		}
	}
	fklFreeVMenv(globEnv);
	fklJoinAllThread();
	free(rawProcList);
	fklFreeIntpr(inter);
	fklUnInitPreprocess();
	fklFreeVMheap(anotherVM->heap);
	fklFreeAllVMs();
	fklFreeAllSharedObj();
	fklFreeGlobSymbolTable();
	fklFreeGlobTypeList();
}

FklByteCode* loadByteCode(FILE* fp)
{
	int32_t size=0;
	int32_t i=0;
	fread(&size,sizeof(int32_t),1,fp);
	FklByteCode* tmp=(FklByteCode*)malloc(sizeof(FklByteCode));
	FKL_ASSERT(tmp,"loadByteCode",__FILE__,__LINE__);
	tmp->size=size;
	tmp->code=(uint8_t*)malloc(sizeof(uint8_t)*size);
	FKL_ASSERT(tmp->code,"loadByteCode",__FILE__,__LINE__);
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
		char* symbol=fklGetStringFromFile(fp);
		fklAddSymbolToGlob(symbol);
		free(symbol);
	}
}

LineNumberTable* loadLineNumberTable(FILE* fp)
{
	int32_t size=0;
	int32_t i=0;
	fread(&size,sizeof(int32_t),1,fp);
	LineNumTabNode** list=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*size);
	FKL_ASSERT(list,"loadLineNumberTable",__FILE__,__LINE__);
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
		LineNumTabNode* n=fklNewLineNumTabNode(fid,scp,cpc,line);
		list[i]=n;
	}
	LineNumberTable* lnt=fklNewLineNumTable();
	lnt->list=list;
	lnt->num=size;
	return lnt;
}
