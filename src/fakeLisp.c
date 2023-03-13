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
static void loadLib(FILE*,size_t*,FklVMlib**,FklVMgc*,FklVMCompoundFrameVarRef* lr);
static int exitState=0;

#define FKL_EXIT_FAILURE (255)

static inline int compileAndRun(char* filename)
{
	if(!fklIsAccessableRegFile(filename))
	{
		perror(filename);
		return FKL_EXIT_FAILURE;
	}
	FILE* fp=fopen(filename,"r");
	if(fp==NULL)
	{
		perror(filename);
		return FKL_EXIT_FAILURE;
	}
	FklSymbolTable* publicSymbolTable=fklCreateSymbolTable();
	fklAddSymbolCstr(filename,publicSymbolTable);
	fklInitCodegen(publicSymbolTable);
	FklCodegen codegen={.fid=0,};
	char* rp=fklRealpath(filename);
	fklSetMainFileRealPath(rp);
	fklInitGlobalCodegener(&codegen,rp,fklCreateSymbolTable(),publicSymbolTable,0);
	free(rp);
	FklByteCodelnt* mainByteCode=fklGenExpressionCodeWithFp(fp,&codegen);
	if(mainByteCode==NULL)
	{
		fklDestroyPrototypePool(codegen.ptpool);
		fklUninitCodegener(&codegen);
		fklUninitCodegen();
		return FKL_EXIT_FAILURE;
	}
	fklUpdatePrototype(codegen.ptpool
			,codegen.globalEnv
			,codegen.globalSymTable
			,codegen.publicSymbolTable);
	fklPrintUndefinedRef(codegen.globalEnv,codegen.globalSymTable,codegen.publicSymbolTable);

	chdir(fklGetCwd());
	FklPtrStack* loadedLibStack=codegen.loadedLibStack;
	FklVM* anotherVM=fklCreateVM(mainByteCode,codegen.globalSymTable,NULL,NULL);
	anotherVM->ptpool=codegen.ptpool;
	anotherVM->libNum=codegen.loadedLibStack->top;
	anotherVM->libs=(FklVMlib*)malloc(sizeof(FklVMlib)*loadedLibStack->top);
	FKL_ASSERT(anotherVM->libs);
	FklVMframe* mainframe=anotherVM->frames;
	fklInitGlobalVMclosure(mainframe,anotherVM);
	fklInitMainVMframeWithProc(anotherVM,mainframe
			,fklGetCompoundFrameProc(mainframe)->u.proc
			,NULL
			,anotherVM->ptpool);
	FklVMCompoundFrameVarRef* lr=&mainframe->u.c.lr;

	while(!fklIsPtrStackEmpty(loadedLibStack))
	{
		FklCodegenLib* cur=fklPopPtrStack(loadedLibStack);
		FklCodegenLibType type=cur->type;
		FklVMlib* curVMlib=&anotherVM->libs[loadedLibStack->top];
		fklInitVMlibWithCodgenLibAndDestroy(cur,curVMlib,anotherVM->gc,anotherVM->ptpool);
		if(type==FKL_CODEGEN_LIB_SCRIPT)
			fklInitMainProcRefs(curVMlib->proc->u.proc,lr->ref,lr->rcount);
	}

	int r=fklRunVM(anotherVM);
	if(r)
		fklDeleteCallChain(anotherVM);
	else
		fklWaitGC(anotherVM->gc);
	fklJoinAllThread(anotherVM);
	fklDestroyVMgc(anotherVM->gc);
	fklDestroyAllVMs(anotherVM);
	fklUninitCodegener(&codegen);
	fklUninitCodegen();
	return r;
}

static inline void initLibWithPrototyle(FklVMlib* lib,uint32_t num,FklPrototypePool* ptpool)
{
	FklPrototype* pts=ptpool->pts;
	for(uint32_t i=0;i<num;i++)
	{
		FklVMlib* cur=&lib[i];
		if(FKL_IS_PROC(cur->proc))
			cur->proc->u.proc->lcount=pts[cur->proc->u.proc->protoId-1].lcount;
	}
}

static inline int runCode(char* filename)
{
	if(!fklIsAccessableRegFile(filename))
	{
		perror(filename);
		return FKL_EXIT_FAILURE;
	}
	FILE* fp=fopen(filename,"rb");
	if(fp==NULL)
	{
		perror(filename);
		return FKL_EXIT_FAILURE;
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
	FklVMframe* mainframe=anotherVM->frames;

	fklInitGlobalVMclosure(mainframe,anotherVM);
	loadLib(fp
			,&anotherVM->libNum
			,&anotherVM->libs
			,anotherVM->gc
			,fklGetCompoundFrameLocRef(anotherVM->frames));

	anotherVM->ptpool=fklLoadPrototypePool(fp);
	fklInitMainVMframeWithProc(anotherVM
			,mainframe
			,fklGetCompoundFrameProc(mainframe)->u.proc
			,NULL
			,anotherVM->ptpool);

	fclose(fp);

	initLibWithPrototyle(anotherVM->libs,anotherVM->libNum,anotherVM->ptpool);
	int r=fklRunVM(anotherVM);
	if(r)
		fklDeleteCallChain(anotherVM);
	else
		fklWaitGC(anotherVM->gc);
	fklJoinAllThread(anotherVM);
	fklDestroySymbolTable(table);
	fklDestroyVMgc(gc);
	fklDestroyAllVMs(anotherVM);
	return r;
}

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
		fklInitGlobalCodegener(&codegen,NULL,globalSymTable,globalSymTable,0);
		runRepl(&codegen,builtInHeadSymbolTable);
		codegen.globalSymTable=NULL;
		FklPtrStack* loadedLibStack=codegen.loadedLibStack;
		while(!fklIsPtrStackEmpty(loadedLibStack))
		{
			FklCodegenLib* lib=fklPopPtrStack(loadedLibStack);
			fklDestroyCodegenLibMacroScope(lib);
			if(lib->type==FKL_CODEGEN_LIB_DLL)
				fklDestroyDll(lib->u.dll);
			fklUninitCodegenLibInfo(lib);
			free(lib);
		}
		fklUninitCodegener(&codegen);
		fklUninitCodegen();
	}
	else
	{
		if(fklIsscript(filename))
			exitState=compileAndRun(filename);
		else if(fklIscode(filename))
			exitState=runCode(filename);
		else
		{
			char* packageMainFileName=fklCopyCstr(filename);
			packageMainFileName=fklStrCat(packageMainFileName,FKL_PATH_SEPARATOR_STR);
			packageMainFileName=fklStrCat(packageMainFileName,"main.fkl");
			if(fklIsAccessableRegFile(packageMainFileName))
				exitState=compileAndRun(packageMainFileName);
			else
			{
				exitState=FKL_EXIT_FAILURE;
				fprintf(stderr,"%s: It is not a correct file.\n",filename);
			}
			free(packageMainFileName);
		}
	}
	fklDestroyMainFileRealPath();
	fklDestroyCwd();
	return exitState;
}

static inline void delete_another_frame(FklVM* exe,FklVMframe* main)
{
	while(exe->frames)
	{
		FklVMframe* cur=exe->frames;
		exe->frames=cur->prev;
		if(cur!=main)
			fklDestroyVMframe(cur,exe);
	}
}

static void runRepl(FklCodegen* codegen,const FklSid_t* builtInHeadSymbolTable)
{
	FklVM* anotherVM=fklCreateVM(NULL,codegen->globalSymTable,NULL,NULL);

	anotherVM->ptpool=codegen->ptpool;
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
				fklUpdatePrototype(codegen->ptpool
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
								,0
								,anotherVM->ptpool);
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
				fklInitMainVMframeWithProcForRepl(anotherVM,&mainframe,tmp,anotherVM->frames,anotherVM->ptpool);
				mainframe.u.c.proc=proc;
				anotherVM->frames=&mainframe;
				fklTcMutexRelease(anotherVM->gc);
				int r=fklRunReplVM(anotherVM);
				fklTcMutexAcquire(anotherVM->gc);
				tmp->closure=NULL;
				tmp->count=0;
				if(r)
				{
					FklVMstack* stack=anotherVM->stack;
					stack->tp=0;
					stack->bp=0;
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
	fklDoUninitCompoundFrame(&mainframe,anotherVM);
	uint32_t count=mainframe.u.c.lr.rcount;
	FklVMvarRef** ref=mainframe.u.c.lr.ref;
	for(uint32_t i=0;i<count;i++)
		fklDestroyVMvarRef(ref[i]);
	free(ref);
	fklUninitPtrStack(&tokenStack);
	fklDestroyVMgc(anotherVM->gc);
	fklDestroyAllVMs(anotherVM);
}

static void loadLib(FILE* fp
		,size_t* plibNum
		,FklVMlib** plibs
		,FklVMgc* gc
		,FklVMCompoundFrameVarRef* lr)
{
	fread(plibNum,sizeof(uint64_t),1,fp);
	size_t libNum=*plibNum;
	FklVMlib* libs=(FklVMlib*)malloc(sizeof(FklVMlib)*libNum);
	FKL_ASSERT(libs);
	*plibs=libs;
	for(size_t i=0;i<libNum;i++)
	{
		FklCodegenLibType libType=FKL_CODEGEN_LIB_SCRIPT;
		fread(&libType,sizeof(char),1,fp);
		if(libType==FKL_CODEGEN_LIB_SCRIPT)
		{
			uint32_t protoId=0;
			fread(&protoId,sizeof(protoId),1,fp);
			FklByteCodelnt* bcl=fklCreateByteCodelnt(NULL);
			fklLoadLineNumberTable(fp,&bcl->l,&bcl->ls);
			FklByteCode* bc=loadByteCode(fp);
			bcl->bc=bc;
			FklVMvalue* codeObj=fklCreateVMvalueNoGC(FKL_TYPE_CODE_OBJ,bcl,gc);
			fklInitVMlibWithCodeObj(&libs[i],codeObj,gc,protoId);
			fklInitMainProcRefs(libs[i].proc->u.proc,lr->ref,lr->rcount);
		}
		else
		{
			uint64_t len=0;
			fread(&len,sizeof(uint64_t),1,fp);
			FklString* s=fklCreateString(len,NULL);
			fread(s->str,len,1,fp);
			FklVMvalue* stringValue=fklCreateVMvalueNoGC(FKL_TYPE_STR,s,gc);
			fklInitVMlib(&libs[i],stringValue);
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
	FKL_ASSERT(tmp->code||!size);
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
