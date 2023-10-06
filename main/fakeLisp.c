#include<fakeLisp/utils.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/bytecode.h>
#include<fakeLisp/vm.h>
#include<fakeLisp/parser.h>
#include<fakeLisp/codegen.h>
#include<fakeLisp/symbol.h>
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

static void runRepl(FklCodegenInfo*);
static void loadLib(FILE*,uint64_t*,FklVMlib**,FklVM*,FklVMCompoundFrameVarRef* lr);
static int exitState=0;

#define FKL_EXIT_FAILURE (255)

static inline int compileAndRun(char* filename)
{
	FILE* fp=fopen(filename,"r");
	if(fp==NULL)
	{
		perror(filename);
		return FKL_EXIT_FAILURE;
	}
	FklCodegenOuterCtx outer_ctx;
	fklInitCodegenOuterCtx(&outer_ctx);
	FklSymbolTable* pst=&outer_ctx.public_symbol_table;
	fklAddSymbolCstr(filename,pst);
	FklCodegenInfo codegen={.fid=0,};
	char* rp=fklRealpath(filename);
	fklSetMainFileRealPath(rp);
	fklInitGlobalCodegenInfo(&codegen,rp,fklCreateSymbolTable(),0,&outer_ctx);
	free(rp);
	FklByteCodelnt* mainByteCode=fklGenExpressionCodeWithFp(fp,&codegen);
	if(mainByteCode==NULL)
	{
		fklUninitCodegenInfo(&codegen);
		fklUninitCodegenOuterCtx(&outer_ctx);
		return FKL_EXIT_FAILURE;
	}
	fklUpdatePrototype(codegen.pts
			,codegen.globalEnv
			,codegen.globalSymTable
			,pst);
	fklPrintUndefinedRef(codegen.globalEnv,codegen.globalSymTable,pst);

	chdir(fklGetCwd());
	FklPtrStack* scriptLibStack=codegen.libStack;
	FklVM* anotherVM=fklCreateVM(mainByteCode,codegen.globalSymTable,codegen.pts);
	codegen.globalSymTable=NULL;
	codegen.pts=NULL;
	anotherVM->libNum=scriptLibStack->top;
	anotherVM->libs=(FklVMlib*)calloc((scriptLibStack->top+1),sizeof(FklVMlib));
	FKL_ASSERT(anotherVM->libs);
	FklVMframe* mainframe=anotherVM->frames;
	fklInitGlobalVMclosure(mainframe,anotherVM);
	fklInitMainVMframeWithProc(anotherVM,mainframe
			,FKL_VM_PROC(fklGetCompoundFrameProc(mainframe))
			,NULL
			,anotherVM->pts);
	FklVMCompoundFrameVarRef* lr=&mainframe->c.lr;

	FklVMgc* gc=anotherVM->gc;
	while(!fklIsPtrStackEmpty(scriptLibStack))
	{
		FklVMlib* curVMlib=&anotherVM->libs[scriptLibStack->top];
		FklCodegenLib* cur=fklPopPtrStack(scriptLibStack);
		FklCodegenLibType type=cur->type;
		fklInitVMlibWithCodgenLibAndDestroy(cur,curVMlib,anotherVM,anotherVM->pts);
		if(type==FKL_CODEGEN_LIB_SCRIPT)
			fklInitMainProcRefs(FKL_VM_PROC(curVMlib->proc),lr->ref,lr->rcount);
	}

	fklUninitCodegenInfo(&codegen);
	fklUninitCodegenOuterCtx(&outer_ctx);

	int r=fklRunVM(anotherVM);
	fklDestroySymbolTable(anotherVM->symbolTable);
	fklDestroyAllVMs(anotherVM);
	fklDestroyVMgc(gc);
	return r;
}

static inline void initLibWithPrototype(FklVMlib* lib,uint32_t num,FklFuncPrototypes* pts)
{
	FklFuncPrototype* pta=pts->pts;
	for(uint32_t i=1;i<=num;i++)
	{
		FklVMlib* cur=&lib[i];
		if(FKL_IS_PROC(cur->proc))
		{
			FklVMproc* proc=FKL_VM_PROC(cur->proc);
			proc->lcount=pta[proc->protoId].lcount;
		}
	}
}

static inline int runCode(char* filename)
{
	FILE* fp=fopen(filename,"rb");
	if(fp==NULL)
	{
		perror(filename);
		return FKL_EXIT_FAILURE;
	}
	FklSymbolTable* table=fklCreateSymbolTable();
	fklLoadSymbolTable(fp,table);
	char* rp=fklRealpath(filename);
	fklSetMainFileRealPath(rp);
	free(rp);
	FklFuncPrototypes* prototypes=fklLoadFuncPrototypes(fklGetBuiltinSymbolNum(),fp);
	FklByteCodelnt* mainCodelnt=fklLoadByteCodelnt(fp);

	FklVM* anotherVM=fklCreateVM(mainCodelnt,table,prototypes);

	FklVMgc* gc=anotherVM->gc;
	FklVMframe* mainframe=anotherVM->frames;

	fklInitGlobalVMclosure(mainframe,anotherVM);
	loadLib(fp
			,&anotherVM->libNum
			,&anotherVM->libs
			,anotherVM
			,fklGetCompoundFrameLocRef(anotherVM->frames));

	fklInitMainVMframeWithProc(anotherVM
			,mainframe
			,FKL_VM_PROC(fklGetCompoundFrameProc(mainframe))
			,NULL
			,anotherVM->pts);

	fclose(fp);

	initLibWithPrototype(anotherVM->libs,anotherVM->libNum,anotherVM->pts);
	int r=fklRunVM(anotherVM);
	fklDestroySymbolTable(table);
	fklDestroyAllVMs(anotherVM);
	fklDestroyVMgc(gc);
	return r;
}

static inline int runPreCompile(char* filename)
{
	FILE* fp=fopen(filename,"rb");
	if(fp==NULL)
	{
		perror(filename);
		return FKL_EXIT_FAILURE;
	}
	FklSymbolTable gst;
	fklInitSymbolTable(&gst);
	FklSymbolTable pst;
	fklInitSymbolTable(&pst);

	FklFuncPrototypes* pts=fklCreateFuncPrototypes(0);

	FklFuncPrototypes macro_pts;
	fklInitFuncPrototypes(&macro_pts,0);

	FklPtrStack scriptLibStack;
	fklInitPtrStack(&scriptLibStack,8,16);

	FklPtrStack macroScriptLibStack;
	fklInitPtrStack(&macroScriptLibStack,8,16);

	char* rp=fklRealpath(filename);
	int load_result=fklLoadPreCompile(pts
			,&macro_pts
			,&scriptLibStack
			,&macroScriptLibStack
			,&gst
			,&pst
			,rp
			,fp);
	fklUninitFuncPrototypes(&macro_pts);
	while(!fklIsPtrStackEmpty(&macroScriptLibStack))
		fklDestroyCodegenLib(fklPopPtrStack(&macroScriptLibStack));
	fklUninitPtrStack(&macroScriptLibStack);
	free(rp);
	fclose(fp);
	fklUninitSymbolTable(&pst);
	if(load_result)
	{
		while(!fklIsPtrStackEmpty(&scriptLibStack))
			fklDestroyCodegenLib(fklPopPtrStack(&scriptLibStack));
		fklUninitPtrStack(&scriptLibStack);
		fprintf(stderr,"%s: Load pre-compile file failed.\n",filename);
		return 1;
	}

	FklCodegenLib* main_lib=fklPopPtrStack(&scriptLibStack);
	FklByteCodelnt* main_byte_code=main_lib->bcl;
	fklDestroyCodegenLibExceptBclAndDll(main_lib);

	FklVM* anotherVM=fklCreateVM(main_byte_code,&gst,pts);

	anotherVM->libNum=scriptLibStack.top;
	anotherVM->libs=(FklVMlib*)calloc((scriptLibStack.top+1),sizeof(FklVMlib));
	FKL_ASSERT(anotherVM->libs);
	FklVMframe* mainframe=anotherVM->frames;
	fklInitGlobalVMclosure(mainframe,anotherVM);
	fklInitMainVMframeWithProc(anotherVM,mainframe
			,FKL_VM_PROC(fklGetCompoundFrameProc(mainframe))
			,NULL
			,anotherVM->pts);
	FklVMCompoundFrameVarRef* lr=&mainframe->c.lr;

	FklVMgc* gc=anotherVM->gc;
	while(!fklIsPtrStackEmpty(&scriptLibStack))
	{
		FklVMlib* curVMlib=&anotherVM->libs[scriptLibStack.top];
		FklCodegenLib* cur=fklPopPtrStack(&scriptLibStack);
		FklCodegenLibType type=cur->type;
		fklInitVMlibWithCodgenLibAndDestroy(cur,curVMlib,anotherVM,anotherVM->pts);
		if(type==FKL_CODEGEN_LIB_SCRIPT)
			fklInitMainProcRefs(FKL_VM_PROC(curVMlib->proc),lr->ref,lr->rcount);
	}
	fklUninitPtrStack(&scriptLibStack);

	int r=fklRunVM(anotherVM);
	fklDestroyAllVMs(anotherVM);
	fklDestroyVMgc(gc);
	fklUninitSymbolTable(&gst);
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
		FklCodegenOuterCtx outer_ctx;
		fklSetMainFileRealPathWithCwd();
		FklCodegenInfo codegen={.fid=0,};
		fklInitCodegenOuterCtx(&outer_ctx);
		FklSymbolTable* pst=&outer_ctx.public_symbol_table;
		fklInitGlobalCodegenInfo(&codegen,NULL,pst,0,&outer_ctx);
		runRepl(&codegen);
		codegen.globalSymTable=NULL;
		FklPtrStack* loadedLibStack=codegen.libStack;
		while(!fklIsPtrStackEmpty(loadedLibStack))
		{
			FklCodegenLib* lib=fklPopPtrStack(loadedLibStack);
			fklDestroyCodegenLibMacroScope(lib);
			if(lib->type==FKL_CODEGEN_LIB_DLL)
				fklDestroyDll(lib->dll);
			fklUninitCodegenLibInfo(lib);
			free(lib);
		}
		fklUninitCodegenInfo(&codegen);
		fklUninitCodegenOuterCtx(&outer_ctx);
	}
	else
	{
		if(fklIsAccessableRegFile(filename))
		{
			if(fklIsScriptFile(filename))
				exitState=compileAndRun(filename);
			else if(fklIsByteCodeFile(filename))
				exitState=runCode(filename);
			else if(fklIsPrecompileFile(filename))
				exitState=runPreCompile(filename);
			else
			{
				exitState=FKL_EXIT_FAILURE;
				fprintf(stderr,"%s: It is not a correct file.\n",filename);
			}
		}
		else
		{
			FklStringBuffer main_script_buf;
			fklInitStringBuffer(&main_script_buf);

			fklStringBufferConcatWithCstr(&main_script_buf,filename);
			fklStringBufferConcatWithCstr(&main_script_buf,FKL_PATH_SEPARATOR_STR);
			fklStringBufferConcatWithCstr(&main_script_buf,"main.fkl");

			char* main_code_file=fklStrCat(fklCopyCstr(main_script_buf.buf),FKL_BYTECODE_FKL_SUFFIX_STR);
			char* main_pre_file=fklStrCat(fklCopyCstr(main_script_buf.buf),FKL_PRE_COMPILE_FKL_SUFFIX_STR);

			if(fklIsAccessableRegFile(main_script_buf.buf))
				exitState=compileAndRun(main_script_buf.buf);
			else if(fklIsAccessableRegFile(main_code_file))
				exitState=runCode(main_code_file);
			else if(fklIsAccessableRegFile(main_pre_file))
				exitState=runPreCompile(main_pre_file);
			else
			{
				exitState=FKL_EXIT_FAILURE;
				fprintf(stderr,"%s: It is not a correct file.\n",filename);
			}
			fklUninitStringBuffer(&main_script_buf);
			free(main_code_file);
			free(main_pre_file);
		}
	}
	fklDestroyMainFileRealPath();
	fklDestroyCwd();
	return exitState;
}

static void runRepl(FklCodegenInfo* codegen)
{
	FklVM* anotherVM=fklCreateVM(NULL,codegen->globalSymTable,codegen->pts);
	anotherVM->libs=(FklVMlib*)calloc(1,sizeof(FklVMlib));
	FKL_ASSERT(anotherVM->libs);

	fklInitFrameToReplFrame(anotherVM,codegen);

	fklRunVM(anotherVM);

	FklVMgc* gc=anotherVM->gc;
	fklDestroyAllVMs(anotherVM);
	fklDestroyVMgc(gc);
	codegen->pts=NULL;
}

static void loadLib(FILE* fp
		,uint64_t* plibNum
		,FklVMlib** plibs
		,FklVM* exe
		,FklVMCompoundFrameVarRef* lr)
{
	fread(plibNum,sizeof(uint64_t),1,fp);
	size_t libNum=*plibNum;
	FklVMlib* libs=(FklVMlib*)calloc((libNum+1),sizeof(FklVMlib));
	FKL_ASSERT(libs);
	*plibs=libs;
	for(size_t i=1;i<=libNum;i++)
	{
		FklCodegenLibType libType=FKL_CODEGEN_LIB_SCRIPT;
		fread(&libType,sizeof(char),1,fp);
		if(libType==FKL_CODEGEN_LIB_SCRIPT)
		{
			uint32_t protoId=0;
			fread(&protoId,sizeof(protoId),1,fp);
			FklByteCodelnt* bcl=fklLoadByteCodelnt(fp);
			FklVMvalue* codeObj=fklCreateVMvalueCodeObj(exe,bcl);
			fklInitVMlibWithCodeObj(&libs[i],codeObj,exe,protoId);
			fklInitMainProcRefs(FKL_VM_PROC(libs[i].proc),lr->ref,lr->rcount);
		}
		else
		{
			uint64_t len=0;
			fread(&len,sizeof(uint64_t),1,fp);
			FklString* s=fklCreateString(len,NULL);
			fread(s->str,len,1,fp);
			FklVMvalue* stringValue=fklCreateVMvalueStr(exe,s);
			fklInitVMlib(&libs[i],stringValue);
		}
	}
}

