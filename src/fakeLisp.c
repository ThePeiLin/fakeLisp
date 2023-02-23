#include<fakeLisp/reader.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/lexer.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/vm.h>
#include<fakeLisp/parser.h>
#include<fakeLisp/codegen.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include<setjmp.h>
#ifdef _WIN32
#include<io.h>
#include<process.h>
#else
#include<unistd.h>
#endif

static void runRepl(FklCodegen*,const FklSid_t*);
static FklByteCode* loadByteCode(FILE*);
static void loadSymbolTable(FILE*,FklSymbolTable* table);
static void loadLib(FILE*,size_t*,FklVMlib**,FklVMvalue* globEnv,FklVMgc*);
static int exitState=0;

int main(int argc,char** argv)
{
	char* filename=(argc>1)?argv[1]:NULL;
	char* cwd=getcwd(NULL,0);
	fklSetCwd(cwd);
	free(cwd);
	fklInitVMargs(argc,argv);
	if(!filename)
	{
		fklSetMainFileRealPathWithCwd();
		FklCodegen codegen={.fid=0,};
		FklSymbolTable* globalSymTable=fklCreateSymbolTable();
		const FklSid_t* builtInHeadSymbolTable=fklInitCodegen(globalSymTable);
		fklInitGlobalCodegener(&codegen,NULL,NULL,globalSymTable,globalSymTable,0);
		runRepl(&codegen,builtInHeadSymbolTable);
		codegen.globalSymTable=NULL;
		FklPtrStack* loadedLibStack=codegen.loadedLibStack;
		while(!fklIsPtrStackEmpty(loadedLibStack))
		{
			FklCodegenLib* lib=fklPopPtrStack(loadedLibStack);
			fklDestroyCodegenLibMacroScope(lib);
			if(lib->type==FKL_CODEGEN_LIB_DLL)
				fklDestroyDll(lib->u.dll);
			free(lib->rp);
			free(lib);
		}
		fklUninitCodegener(&codegen);
		fklUninitCodegen();
	}
	else if(fklIsscript(filename))
	{
		if(!fklIsAccessableRegFile(filename))
		{
			perror(filename);
			fklDestroyCwd();
			return EXIT_FAILURE;
		}
		FILE* fp=fopen(filename,"r");
		if(fp==NULL)
		{
			perror(filename);
			fklDestroyCwd();
			return EXIT_FAILURE;
		}
		FklSymbolTable* publicSymbolTable=fklCreateSymbolTable();
		fklAddSymbolCstr(filename,publicSymbolTable);
		fklInitCodegen(publicSymbolTable);
		FklCodegen codegen={.fid=0,};
		char* rp=fklRealpath(filename);
		fklSetMainFileRealPath(rp);
		fklInitGlobalCodegener(&codegen,rp,NULL,fklCreateSymbolTable(),publicSymbolTable,0);
		free(rp);
		FklByteCodelnt* mainByteCode=fklGenExpressionCodeWithFp(fp,&codegen);
		if(mainByteCode==NULL)
		{
			fklUninitCodegener(&codegen);
			fklUninitCodegen();
			fklDestroyMainFileRealPath();
			fklDestroyCwd();
			return 1;
		}
		chdir(fklGetCwd());
		FklPtrStack* loadedLibStack=codegen.loadedLibStack;
		for(size_t i=0;i<loadedLibStack->top;i++)
		{
			FklCodegenLib* cur=loadedLibStack->base[i];
			if(cur->type==FKL_CODEGEN_LIB_SCRIPT)
				fklCodegenPrintUndefinedSymbol(cur->u.bcl,(FklCodegenLib**)loadedLibStack->base,codegen.globalSymTable,cur->exportNum,cur->exports);
		}
		fklCodegenPrintUndefinedSymbol(mainByteCode,(FklCodegenLib**)codegen.loadedLibStack->base,codegen.globalSymTable,0,NULL);
		FklVM* anotherVM=fklCreateVM(mainByteCode,codegen.globalSymTable,NULL,NULL);
		anotherVM->cpool=codegen.cpool;
		FklVMvalue* globEnv=fklCreateVMvalueNoGC(FKL_TYPE_ENV,fklCreateGlobVMenv(FKL_VM_NIL,anotherVM->gc,anotherVM->symbolTable),anotherVM->gc);
		anotherVM->libNum=codegen.loadedLibStack->top;
		anotherVM->libs=(FklVMlib*)malloc(sizeof(FklVMlib)*loadedLibStack->top);
		FKL_ASSERT(anotherVM->libs);
		while(!fklIsPtrStackEmpty(loadedLibStack))
		{
			FklCodegenLib* cur=fklPopPtrStack(loadedLibStack);
			FklVMlib* curVMlib=&anotherVM->libs[loadedLibStack->top];
			fklInitVMlibWithCodgenLibAndDestroy(cur,curVMlib,globEnv,anotherVM->gc);
		}

		FklVMvalue* mainEnv=fklCreateVMvalueNoGC(FKL_TYPE_ENV,fklCreateVMenv(globEnv,anotherVM->gc),anotherVM->gc);
		FklVMframe* mainframe=anotherVM->frames;
		mainframe->u.c.localenv=mainEnv;
		fklInitGlobalVMclosure(mainframe,anotherVM);
		FklVMvalue** ref=mainframe->u.c.lr.ref;

		FklVMproc* tmp=fklCreateVMproc(mainByteCode->bc->code,mainByteCode->bc->size,anotherVM->codeObj,anotherVM->gc);
		tmp->protoId=1;
		FklVMvalue* proc=fklCreateVMvalueNoGC(FKL_TYPE_PROC,tmp,anotherVM->gc);
		tmp->prevEnv=NULL;
		fklInitMainVMframeWithProc(mainframe,tmp,NULL);
		mainframe->u.c.proc=proc;

		fklUpdatePrototype(codegen.cpool
						,codegen.globalEnv
						,codegen.globalSymTable
						,codegen.publicSymbolTable);
		int r=fklRunVM(anotherVM);
		if(r)
		{
			exitState=r;
			fklDeleteCallChain(anotherVM);
		}
		else
			fklWaitGC(anotherVM->gc);
		fklJoinAllThread(anotherVM);
		free(ref);
		fklDestroyVMgc(anotherVM->gc);
		fklDestroyAllVMs(anotherVM);
		fklUninitCodegener(&codegen);
		fklUninitCodegen();
	}
	else if(fklIscode(filename))
	{
		if(!fklIsAccessableRegFile(filename))
		{
			perror(filename);
			fklDestroyCwd();
			return EXIT_FAILURE;
		}
		FILE* fp=fopen(argv[1],"rb");
		if(fp==NULL)
		{
			perror(filename);
			fklDestroyCwd();
			return EXIT_FAILURE;
		}
		FklSymbolTable* table=fklCreateSymbolTable();
		loadSymbolTable(fp,table);
		char* rp=fklRealpath(filename);
		fklSetMainFileRealPath(rp);
		free(rp);
		FklByteCodelnt* mainCodelnt=fklCreateByteCodelnt(NULL);
		fklLoadLineNumberTable(fp,&mainCodelnt->l,&mainCodelnt->ls);
		FklByteCode* mainCode=loadByteCode(fp);
		mainCodelnt->bc=mainCode;
		FklVM* anotherVM=fklCreateVM(mainCodelnt,table,NULL,NULL);
		FklVMgc* gc=anotherVM->gc;
		FklVMvalue* globEnv=fklCreateVMvalueNoGC(FKL_TYPE_ENV,fklCreateGlobVMenv(FKL_VM_NIL,anotherVM->gc,table),anotherVM->gc);
		loadLib(fp,&anotherVM->libNum,&anotherVM->libs,globEnv,anotherVM->gc);
		fclose(fp);
		FklVMframe* mainframe=anotherVM->frames;
		FklVMvalue* mainEnv=fklCreateVMvalueNoGC(FKL_TYPE_ENV,fklCreateVMenv(globEnv,anotherVM->gc),anotherVM->gc);
		mainframe->u.c.localenv=mainEnv;
		fklInitGlobalVMclosure(mainframe,anotherVM);
		FklVMvalue** ref=mainframe->u.c.lr.ref;
		int r=fklRunVM(anotherVM);
		if(r)
		{
			exitState=r;
			fklDeleteCallChain(anotherVM);
		}
		else
			fklWaitGC(anotherVM->gc);
		fklJoinAllThread(anotherVM);
		free(ref);
		fklDestroySymbolTable(table);
		fklDestroyAllVMs(anotherVM);
		fklDestroyVMgc(gc);
	}
	else
	{
		fprintf(stderr,"%s: It is not a correct file.\n",filename);
		fklDestroyCwd();
		return EXIT_FAILURE;
	}
	fklDestroyMainFileRealPath();
	fklDestroyCwd();
	return exitState;
}

static inline void delete_another_frame(FklVM* exe,FklVMframe* main)
{
	FklVMframe* sf=&exe->sf;
	while(exe->frames)
	{
		FklVMframe* cur=exe->frames;
		exe->frames=cur->prev;
		if(cur!=main)
			fklDestroyVMframe(cur,sf);
	}
}

static void runRepl(FklCodegen* codegen,const FklSid_t* builtInHeadSymbolTable)
{
	FklVM* anotherVM=fklCreateVM(NULL,codegen->globalSymTable,NULL,NULL);

	anotherVM->cpool=codegen->cpool;
	FklVMframe mainframe={FKL_FRAME_COMPOUND,};
	fklInitGlobalVMclosure(&mainframe,anotherVM);

	FklPtrStack tokenStack=FKL_STACK_INIT;
	fklInitPtrStack(&tokenStack,32,16);
	FklLineNumberTable* globalLnt=fklCreateLineNumTable();
	anotherVM->codeObj=fklCreateVMvalueNoGC(FKL_TYPE_CODE_OBJ,fklCreateByteCodelnt(fklCreateByteCode(0)),anotherVM->gc);
	char* prev=NULL;
	size_t prevSize=0;
	size_t libNum=codegen->loadedLibStack->top;
	for(;;)
	{
		FklNastNode* begin=NULL;
		FklStringMatchRouteNode* route=NULL;
		printf(">>>");
		int unexpectEOF=0;
		size_t size=0;
		char* list=fklReadInStringPattern(stdin
				,&prev
				,&size
				,&prevSize
				,codegen->curline
				,&codegen->curline
				,&unexpectEOF
				,&tokenStack
				,NULL
				,*(codegen->phead)
				,&route);
		if(unexpectEOF)
		{
			switch(unexpectEOF)
			{
				case 1:
					exitState=1;
					fprintf(stderr,"error of reader:Unexpect EOF at line %lu\n",codegen->curline);
					break;
				case 2:
					exitState=2;
					fprintf(stderr,"error of reader:Invalid expression at line %lu\n",codegen->curline);
					break;
			}
			fklDestroyStringMatchRoute(route);
			continue;
		}
		size_t errorLine=0;
		fklTcMutexAcquire(anotherVM->gc);
		begin=fklCreateNastNodeFromTokenStackAndMatchRoute(&tokenStack
				,route
				,&errorLine
				,builtInHeadSymbolTable
				,codegen
				,codegen->publicSymbolTable);
		fklTcMutexRelease(anotherVM->gc);
		fklDestroyStringMatchRoute(route);
		free(list);
		if(fklIsPtrStackEmpty(&tokenStack))
			break;
		while(!fklIsPtrStackEmpty(&tokenStack))
			fklDestroyToken(fklPopPtrStack(&tokenStack));
		if(!begin)
			fprintf(stderr,"error of reader:Invalid expression at line %lu\n",errorLine);
		else
		{
			fklMakeNastNodeRef(begin);
			fklTcMutexAcquire(anotherVM->gc);
			FklByteCodelnt* tmpByteCode=fklGenExpressionCode(begin,codegen->globalEnv,codegen);
			if(tmpByteCode)
			{
				fklUpdatePrototype(codegen->cpool
						,codegen->globalEnv
						,codegen->globalSymTable
						,codegen->publicSymbolTable);
				size_t unloadlibNum=codegen->loadedLibStack->top-libNum;
				if(unloadlibNum)
				{
					libNum+=unloadlibNum;
					FklVMlib* nlibs=(FklVMlib*)malloc(sizeof(FklVMlib)*libNum);
					FKL_ASSERT(nlibs);
					memcpy(nlibs,anotherVM->libs,sizeof(FklVMlib)*anotherVM->libNum);
					for(size_t i=anotherVM->libNum;i<libNum;i++)
					{
						FklVMlib* curVMlib=&nlibs[i];
						FklCodegenLib* curCGlib=codegen->loadedLibStack->base[i];
						fklInitVMlibWithCodegenLibRefs(curCGlib
								,curVMlib
								,anotherVM
								,&mainframe.u.c.lr
								,0);
					}
					FklVMlib* prev=anotherVM->libs;
					anotherVM->libs=nlibs;
					anotherVM->libNum=libNum;
					free(prev);
				}
				FklVMvalue* anotherCodeObj=fklCreateVMvalueNoGC(FKL_TYPE_CODE_OBJ,tmpByteCode,anotherVM->gc);
				FklVMproc* tmp=fklCreateVMproc(tmpByteCode->bc->code,tmpByteCode->bc->size,anotherCodeObj,anotherVM->gc);
				FklVMvalue* proc=fklCreateVMvalueNoGC(FKL_TYPE_PROC,tmp,anotherVM->gc);
				tmp->protoId=1;
				tmp->prevEnv=NULL;
				fklInitMainVMframeWithProc(&mainframe,tmp,anotherVM->frames);
				mainframe.u.c.proc=proc;
				anotherVM->frames=&mainframe;
				fklTcMutexRelease(anotherVM->gc);
				int r=fklRunReplVM(anotherVM);
				fklTcMutexAcquire(anotherVM->gc);
				if(r)
				{
					FklVMstack* stack=anotherVM->stack;
					stack->tps.top=0;
					stack->tp=0;
					stack->bp=0;
					stack->bps.top=0;
					tmp->prevEnv=NULL;
					delete_another_frame(anotherVM,&mainframe);
				}
				else
				{
					FklVMstack* stack=anotherVM->stack;
					if(stack->tp!=0)
					{
						printf(";=>");
						fklDBG_printVMstack(stack,stdout,0,anotherVM->symbolTable);
					}
					fklWaitGC(anotherVM->gc);
					anotherVM->frames=NULL;
					stack->tp=0;
				}
			}
			fklTcMutexRelease(anotherVM->gc);
			fklDestroyNastNode(begin);
			begin=NULL;
		}
	}
	fklDestroyLineNumberTable(globalLnt);
	fklJoinAllThread(anotherVM);
	free(mainframe.u.c.lr.loc);
	free(mainframe.u.c.lr.ref);
	fklUninitPtrStack(&tokenStack);
	fklDestroyVMgc(anotherVM->gc);
	fklDestroyAllVMs(anotherVM);
}

static void loadLib(FILE* fp,size_t* plibNum,FklVMlib** plibs,FklVMvalue* globEnv,FklVMgc* gc)
{
	fread(plibNum,sizeof(uint64_t),1,fp);
	size_t libNum=*plibNum;
	FklVMlib* libs=(FklVMlib*)malloc(sizeof(FklVMlib)*libNum);
	FKL_ASSERT(libs);
	*plibs=libs;
	for(size_t i=0;i<libNum;i++)
	{
		uint64_t exportNum=0;
		fread(&exportNum,sizeof(uint64_t),1,fp);
		FklSid_t* exports=(FklSid_t*)malloc(sizeof(FklSid_t)*exportNum);
		FKL_ASSERT(exports);
		fread(exports,sizeof(FklSid_t),exportNum,fp);
		FklCodegenLibType libType=FKL_CODEGEN_LIB_SCRIPT;
		fread(&libType,sizeof(char),1,fp);
		if(libType==FKL_CODEGEN_LIB_SCRIPT)
		{
			FklByteCodelnt* bcl=fklCreateByteCodelnt(NULL);
			fklLoadLineNumberTable(fp,&bcl->l,&bcl->ls);
			FklByteCode* bc=loadByteCode(fp);
			bcl->bc=bc;
			FklVMvalue* codeObj=fklCreateVMvalueNoGC(FKL_TYPE_CODE_OBJ,bcl,gc);
			fklInitVMlibWithCodeObj(&libs[i],exportNum,exports,globEnv,codeObj,gc);
		}
		else
		{
			uint64_t len=0;
			fread(&len,sizeof(uint64_t),1,fp);
			FklString* s=fklCreateString(len,NULL);
			fread(s->str,len,1,fp);
			FklVMvalue* stringValue=fklCreateVMvalueNoGC(FKL_TYPE_STR,s,gc);
			fklInitVMlib(&libs[i],exportNum,exports,stringValue);
		}
	}
}

static FklByteCode* loadByteCode(FILE* fp)
{
	uint64_t size=0;
	fread(&size,sizeof(uint64_t),1,fp);
	FklByteCode* tmp=(FklByteCode*)malloc(sizeof(FklByteCode));
	FKL_ASSERT(tmp);
	tmp->size=size;
	tmp->code=(uint8_t*)malloc(sizeof(uint8_t)*size);
	FKL_ASSERT(tmp->code);
	fread(tmp->code,size,1,fp);
	return tmp;
}

static void loadSymbolTable(FILE* fp,FklSymbolTable* table)
{
	uint64_t size=0;
	fread(&size,sizeof(size),1,fp);
	for(uint64_t i=0;i<size;i++)
	{
		uint64_t len=0;
		fread(&len,sizeof(len),1,fp);
		FklString* buf=fklCreateString(len,NULL);
		fread(buf->str,len,1,fp);
		fklAddSymbol(buf,table);
		free(buf);
	}
}
