#include<fakeLisp/reader.h>
#include<fakeLisp/syntax.h>
#include<fakeLisp/compiler.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/parser.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/ast.h>
#include<fakeLisp/vm.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<ctype.h>
#include<pthread.h>
#include<setjmp.h>

void runRepl(FklInterpreter*);
FklByteCode* loadByteCode(FILE*);
void loadSymbolTable(FILE*);
FklLineNumberTable* loadLineNumberTable(FILE*);
static jmp_buf buf;
static int exitState=0;

void errorCallBack(void* a)
{
	int* i=(int*)a;
	exitState=255;
	longjmp(buf,i[(sizeof(void*)*2)/sizeof(int)]);
}

int main(int argc,char** argv)
{
	char* filename=(argc>1)?argv[1]:"stdin";
#ifdef WFKL_I32
	char* intprPath=_fullpath(NULL,argv[0],0);
#else
	char* intprPath=realpath(argv[0],0);
#endif
	char* t=fklGetDir(intprPath);
	free(intprPath);
	fklSetInterpreterPath(t);
	free(t);
	if(argc==1||fklIsscript(filename))
	{
		FILE* fp=(argc>1)?fopen(argv[1],"r"):stdin;
		if(fp==NULL)
		{
			perror(filename);
			fklFreeInterpeterPath();
			return EXIT_FAILURE;
		}
		FklInterpreter* inter=fklNewIntpr(((fp==stdin)?"stdin":argv[1]),fp,NULL,NULL,NULL);
		fklAddSymbolToGlob(filename);
		fklInitGlobKeyWord(inter->glob);
		fklInitNativeDefTypes(inter->deftypes);
		//fklInitBuiltInStringPattern();
		if(fp==stdin)
			runRepl(inter);
		else
		{
#ifdef _WIN32
			char* rp=_fullpath(NULL,filename,0);
#else
			char* rp=realpath(filename,0);
#endif
			char* pWorkPath=getcwd(NULL,0);
			char* workpath=fklGetDir(rp);
			free(rp);
			int state;
			FklByteCodelnt* mainByteCode=fklCompileFile(inter,1,&state);
			fklFreeAllSharedObj();
			if(mainByteCode==NULL)
			{
				free(workpath);
				fklFreeIntpr(inter);
				fklUninitPreprocess();
				fklFreeGlobSymbolTable();
				fklFreeGlobTypeList();
				fklFreeInterpeterPath();
				free(pWorkPath);
				return state;
			}
			inter->lnt->num=mainByteCode->ls;
			inter->lnt->list=mainByteCode->l;
			FklVMenv* globEnv=fklNewVMenv(NULL);
			FklVM* anotherVM=fklNewVM(mainByteCode->bc);
			fklFreeByteCode(mainByteCode->bc);
			free(mainByteCode);
			FklVMrunnable* mainrunnable=anotherVM->rstack->base[0];
			anotherVM->argc=argc-1;
			anotherVM->argv=argv+1;
			anotherVM->tid=pthread_self();
			mainrunnable->localenv=globEnv;
			anotherVM->callback=errorCallBack;
			anotherVM->lnt=inter->lnt;
			fklInitGlobEnv(globEnv,anotherVM->heap);
			chdir(pWorkPath);
			free(workpath);
			free(pWorkPath);
			if(setjmp(buf)==0)
			{
				fklRunVM(anotherVM);
				fklJoinAllThread();
				fklFreeIntpr(inter);
				fklUninitPreprocess();
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
				fklUninitPreprocess();
				fklFreeVMheap(anotherVM->heap);
				fklFreeAllVMs();
				fklFreeAllSharedObj();
				fklFreeInterpeterPath();
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
			fklFreeInterpeterPath();
			return EXIT_FAILURE;
		}
		loadSymbolTable(fp);
		FklLineNumberTable* lnt=loadLineNumberTable(fp);
		fklLoadTypeList(fp);
		FklByteCode* mainCode=loadByteCode(fp);
		FklVM* anotherVM=fklNewVM(mainCode);
		FklVMheap* heap=anotherVM->heap;
		fklFreeByteCode(mainCode);
		fclose(fp);
		FklVMrunnable* mainrunnable=anotherVM->rstack->base[0];
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
			fklFreeInterpeterPath();
			return exitState;
		}
	}
	else
	{
		fprintf(stderr,"%s: It is not a correct file.\n",filename);
		fklFreeInterpeterPath();
		return EXIT_FAILURE;
	}
	fklFreeInterpeterPath();
	return exitState;
}

void runRepl(FklInterpreter* inter)
{
	int e=0;
	FklVM* anotherVM=fklNewVM(NULL);
	FklVMenv* globEnv=fklNewVMenv(NULL);
	anotherVM->tid=pthread_self();
	anotherVM->callback=errorCallBack;
	anotherVM->lnt=inter->lnt;
	fklInitGlobEnv(globEnv,anotherVM->heap);
	FklByteCode* rawProcList=NULL;
	FklPtrStack* tokenStack=fklNewPtrStack(32,16);
	char* prev=NULL;
	size_t prevSize=0;
	int32_t bs=0;
	for(;e<2;)
	{
		FklAstCptr* begin=NULL;
		if(inter->file==stdin)printf(">>>");
		if(prev)
			fwrite(prev,sizeof(char),prevSize,stdout);
		int unexpectEOF=0;
		size_t size=0;
		char* list=fklReadInStringPattern(inter->file,&prev,&size,&prevSize,inter->curline,&unexpectEOF,tokenStack,NULL);
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
		begin=fklCreateAstWithTokens(tokenStack,inter);
		inter->curline+=fklCountChar(list,'\n',size);
		while(!fklIsPtrStackEmpty(tokenStack))
			fklFreeToken(fklPopPtrStack(tokenStack));
		//begin=fklCreateTree(list,inter,NULL);
		if(fklIsAllSpaceBufSize(list,size))
		{
			free(list);
			break;
		}
		if(begin!=NULL)
		{
			FklByteCodelnt* tmpByteCode=fklCompile(begin,inter->glob,inter,&state,!fklIsLambdaExpression(begin));
			if(state.state!=0)
			{
				fklPrintCompileError(state.place,state.state,inter);
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
	fklFreePtrStack(tokenStack);
	free(rawProcList);
	fklFreeIntpr(inter);
	fklUninitPreprocess();
	fklFreeVMheap(anotherVM->heap);
	fklFreeAllVMs();
	fklFreeAllSharedObj();
	fklFreeGlobSymbolTable();
	fklFreeGlobTypeList();
}

FklByteCode* loadByteCode(FILE* fp)
{
	uint64_t size=0;
	fread(&size,sizeof(uint64_t),1,fp);
	FklByteCode* tmp=(FklByteCode*)malloc(sizeof(FklByteCode));
	FKL_ASSERT(tmp,"loadByteCode");
	tmp->size=size;
	tmp->code=(uint8_t*)malloc(sizeof(uint8_t)*size);
	FKL_ASSERT(tmp->code,"loadByteCode");
	fread(tmp->code,size,1,fp);
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

FklLineNumberTable* loadLineNumberTable(FILE* fp)
{
	uint32_t size=0;
	uint32_t i=0;
	fread(&size,sizeof(uint32_t),1,fp);
	FklLineNumTabNode** list=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*size);
	FKL_ASSERT(list,"loadLineNumberTable");
	for(;i<size;i++)
	{
		FklSid_t fid=0;
		uint64_t scp=0;
		uint64_t cpc=0;
		uint32_t line=0;
		fread(&fid,sizeof(fid),1,fp);
		fread(&scp,sizeof(scp),1,fp);
		fread(&cpc,sizeof(cpc),1,fp);
		fread(&line,sizeof(line),1,fp);
		FklLineNumTabNode* n=fklNewLineNumTabNode(fid,scp,cpc,line);
		list[i]=n;
	}
	FklLineNumberTable* lnt=fklNewLineNumTable();
	lnt->list=list;
	lnt->num=size;
	return lnt;
}
