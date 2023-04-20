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
static void loadLib(FILE*,uint64_t*,FklVMlib**,FklVMgc*,FklVMCompoundFrameVarRef* lr);
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
		fklDestroyFuncPrototypes(codegen.pts);
		fklUninitCodegener(&codegen);
		fklUninitCodegen();
		return FKL_EXIT_FAILURE;
	}
	fklUpdatePrototype(codegen.pts
			,codegen.globalEnv
			,codegen.globalSymTable
			,codegen.publicSymbolTable);
	fklPrintUndefinedRef(codegen.globalEnv,codegen.globalSymTable,codegen.publicSymbolTable);

	chdir(fklGetCwd());
	FklPtrStack* loadedLibStack=codegen.loadedLibStack;
	FklVM* anotherVM=fklCreateVM(mainByteCode,codegen.globalSymTable);
	anotherVM->pts=codegen.pts;
	anotherVM->libNum=codegen.loadedLibStack->top;
	anotherVM->libs=(FklVMlib*)malloc(sizeof(FklVMlib)*loadedLibStack->top);
	FKL_ASSERT(anotherVM->libs);
	FklVMframe* mainframe=anotherVM->frames;
	fklInitGlobalVMclosure(mainframe,anotherVM);
	fklInitMainVMframeWithProc(anotherVM,mainframe
			,fklGetCompoundFrameProc(mainframe)->u.proc
			,NULL
			,anotherVM->pts);
	FklVMCompoundFrameVarRef* lr=&mainframe->u.c.lr;

	FklVMgc* gc=anotherVM->gc;
	while(!fklIsPtrStackEmpty(loadedLibStack))
	{
		FklCodegenLib* cur=fklPopPtrStack(loadedLibStack);
		FklCodegenLibType type=cur->type;
		FklVMlib* curVMlib=&anotherVM->libs[loadedLibStack->top];
		fklInitVMlibWithCodgenLibAndDestroy(cur,curVMlib,anotherVM->gc,anotherVM->pts);
		if(type==FKL_CODEGEN_LIB_SCRIPT)
			fklInitMainProcRefs(curVMlib->proc->u.proc,lr->ref,lr->rcount);
	}

	int r=fklRunVM(anotherVM);
	fklDestroyAllVMs(anotherVM);
	fklDestroyVMgc(gc);
	fklUninitCodegener(&codegen);
	fklUninitCodegen();
	return r;
}

static inline void initLibWithPrototyle(FklVMlib* lib,uint32_t num,FklFuncPrototypes* pts)
{
	FklFuncPrototype* pta=pts->pts;
	for(uint32_t i=0;i<num;i++)
	{
		FklVMlib* cur=&lib[i];
		if(FKL_IS_PROC(cur->proc))
			cur->proc->u.proc->lcount=pta[cur->proc->u.proc->protoId].lcount;
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
	FklVM* anotherVM=fklCreateVM(mainCodelnt,table);

	FklVMgc* gc=anotherVM->gc;
	FklVMframe* mainframe=anotherVM->frames;

	fklInitGlobalVMclosure(mainframe,anotherVM);
	loadLib(fp
			,&anotherVM->libNum
			,&anotherVM->libs
			,gc
			,fklGetCompoundFrameLocRef(anotherVM->frames));

	anotherVM->pts=fklLoadFuncPrototypes(fp);
	fklInitMainVMframeWithProc(anotherVM
			,mainframe
			,fklGetCompoundFrameProc(mainframe)->u.proc
			,NULL
			,anotherVM->pts);

	fclose(fp);

	initLibWithPrototyle(anotherVM->libs,anotherVM->libNum,anotherVM->pts);
	int r=fklRunVM(anotherVM);
	fklDestroySymbolTable(table);
	fklDestroyAllVMs(anotherVM);
	fklDestroyVMgc(gc);
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

static void runRepl(FklCodegen* codegen,const FklSid_t* builtInHeadSymbolTable)
{
	FklVM* anotherVM=fklCreateVM(NULL,codegen->globalSymTable);
	anotherVM->pts=codegen->pts;

	fklInitFrameToReplFrame(anotherVM,codegen,builtInHeadSymbolTable);

	fklRunVM(anotherVM);

	FklVMgc* gc=anotherVM->gc;
	fklDestroyAllVMs(anotherVM);
	fklDestroyVMgc(gc);
}

static void loadLib(FILE* fp
		,uint64_t* plibNum
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
