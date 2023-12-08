#include<fakeLisp/utils.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/bytecode.h>
#include<fakeLisp/vm.h>
#include<fakeLisp/parser.h>
#include<fakeLisp/codegen.h>
#include<fakeLisp/symbol.h>
#include<uv.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include<setjmp.h>
#ifdef _WIN32
#include<direct.h>
#include<io.h>
#include<process.h>
#else
#include<unistd.h>
#endif

static int runRepl(FklCodegenInfo*);
static void init_frame_to_repl_frame(FklVM*,FklCodegenInfo* codegen);
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
	fklSetMainFileRealDir(rp);
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
	if(codegen.globalEnv->uref.top)
	{
		fklUninitCodegenInfo(&codegen);
		fklUninitCodegenOuterCtx(&outer_ctx);
		fklDestroyByteCodelnt(mainByteCode);
		return FKL_EXIT_FAILURE;
	}

	FklPtrStack* scriptLibStack=codegen.libStack;
	FklVM* anotherVM=fklCreateVM(mainByteCode,codegen.globalSymTable,codegen.pts,1);
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

	fklChdir(fklGetCwd());
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
	fklSetMainFileRealDir(rp);
	free(rp);
	FklFuncPrototypes* prototypes=fklLoadFuncPrototypes(fklGetBuiltinSymbolNum(),fp);
	FklByteCodelnt* mainCodelnt=fklLoadByteCodelnt(fp);

	FklVM* anotherVM=fklCreateVM(mainCodelnt,table,prototypes,1);

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

	FklFuncPrototypes* pts=fklCreateFuncPrototypes(0);

	FklFuncPrototypes macro_pts;
	fklInitFuncPrototypes(&macro_pts,0);

	FklPtrStack scriptLibStack;
	fklInitPtrStack(&scriptLibStack,8,16);

	FklPtrStack macroScriptLibStack;
	fklInitPtrStack(&macroScriptLibStack,8,16);

	FklCodegenOuterCtx ctx;
	fklInitCodegenOuterCtxExceptPattern(&ctx);

	char* errorStr=NULL;
	char* rp=fklRealpath(filename);
	int load_result=fklLoadPreCompile(pts
			,&macro_pts
			,&scriptLibStack
			,&macroScriptLibStack
			,&gst
			,&ctx
			,rp
			,fp
			,&errorStr);
	fklUninitFuncPrototypes(&macro_pts);
	while(!fklIsPtrStackEmpty(&macroScriptLibStack))
		fklDestroyCodegenLib(fklPopPtrStack(&macroScriptLibStack));
	fklUninitPtrStack(&macroScriptLibStack);
	free(rp);
	fclose(fp);
	fklUninitSymbolTable(&ctx.public_symbol_table);
	if(load_result)
	{
		fklUninitSymbolTable(&gst);
		fklDestroyFuncPrototypes(pts);
		while(!fklIsPtrStackEmpty(&scriptLibStack))
			fklDestroyCodegenLib(fklPopPtrStack(&scriptLibStack));
		fklUninitPtrStack(&scriptLibStack);
		if(errorStr)
		{
			fprintf(stderr,"%s\n",errorStr);
			free(errorStr);
		}
		fprintf(stderr,"%s: Load pre-compile file failed.\n",filename);
		return 1;
	}

	FklCodegenLib* main_lib=fklPopPtrStack(&scriptLibStack);
	FklByteCodelnt* main_byte_code=main_lib->bcl;
	fklDestroyCodegenLibExceptBclAndDll(main_lib);

	FklVM* anotherVM=fklCreateVM(main_byte_code,&gst,pts,1);

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
	char* cwd=fklSysgetcwd();
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
		exitState=runRepl(&codegen);
		codegen.globalSymTable=NULL;
		FklPtrStack* loadedLibStack=codegen.libStack;
		while(!fklIsPtrStackEmpty(loadedLibStack))
		{
			FklCodegenLib* lib=fklPopPtrStack(loadedLibStack);
			fklDestroyCodegenLibMacroScope(lib);
			if(lib->type==FKL_CODEGEN_LIB_DLL)
				uv_dlclose(&lib->dll);
			fklUninitCodegenLibInfo(lib);
			free(lib);
		}
		fklUninitCodegenInfo(&codegen);
		fklUninitCodegenOuterCtx(&outer_ctx);
	}
	else
	{
		if(fklIsAccessibleRegFile(filename))
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

			if(fklIsAccessibleRegFile(main_script_buf.buf))
				exitState=compileAndRun(main_script_buf.buf);
			else if(fklIsAccessibleRegFile(main_code_file))
				exitState=runCode(main_code_file);
			else if(fklIsAccessibleRegFile(main_pre_file))
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
	fklDestroyMainFileRealDir();
	fklDestroyCwd();
	return exitState;
}

static int runRepl(FklCodegenInfo* codegen)
{
	FklVM* anotherVM=fklCreateVM(NULL,codegen->globalSymTable,codegen->pts,1);
	anotherVM->libs=(FklVMlib*)calloc(1,sizeof(FklVMlib));
	FKL_ASSERT(anotherVM->libs);

	init_frame_to_repl_frame(anotherVM,codegen);

	int err=fklRunVM(anotherVM);

	FklVMgc* gc=anotherVM->gc;
	fklDestroyAllVMs(anotherVM);
	fklDestroyVMgc(gc);
	codegen->pts=NULL;
	return err;
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

typedef struct
{
	size_t offset;
	FklPtrStack stateStack;
	FklPtrStack symbolStack;
	FklUintStack lineStack;
}NastCreatCtx;

#include<replxx.h>

// typedef struct
// {
// 	FklStringBuffer* buf;
// 	int eof;
// 	Replxx* replxx;
// }AsyncReplCtx;

typedef struct
{
	FklCodegenInfo* codegen;
	NastCreatCtx* cc;
	FklVM* exe;
	FklVMvalue* stdinVal;
	FklVMvalue* mainProc;
	FklStringBuffer buf;
	enum
	{
		READY,
		WAITING,
		READING,
		DONE,
	}state:8;
	uint32_t lcount;
	Replxx* replxx;
	int eof;
}ReplCtx;

FKL_CHECK_OTHER_OBJ_CONTEXT_SIZE(ReplCtx);

static inline void init_with_main_proc(FklVMproc* d,const FklVMproc* s)
{
	uint32_t count=s->rcount;
	FklVMvarRef** refs=fklCopyMemory(s->closure,count*sizeof(FklVMvarRef*));
	for(uint32_t i=0;i<count;i++)
		fklMakeVMvarRefRef(refs[i]);
	d->closure=refs;
	d->rcount=count;
	d->protoId=1;
	d->lcount=s->lcount;
}

static inline void inc_lref(FklVMCompoundFrameVarRef* lr,uint32_t lcount)
{
	if(!lr->lref)
	{
		lr->lref=(FklVMvarRef**)calloc(lcount,sizeof(FklVMvarRef*));
		FKL_ASSERT(lr->lref);
	}
}

static inline FklVMvarRef* insert_local_ref(FklVMCompoundFrameVarRef* lr
		,FklVMvarRef* ref
		,uint32_t cidx)
{
	FklVMvarRefList* rl=(FklVMvarRefList*)malloc(sizeof(FklVMvarRefList));
	FKL_ASSERT(rl);
	rl->ref=fklMakeVMvarRefRef(ref);
	rl->next=lr->lrefl;
	lr->lrefl=rl;
	lr->lref[cidx]=ref;
	return ref;
}

static inline void process_unresolve_ref_for_repl(FklCodegenEnv* env
		,FklFuncPrototypes* cp
		,FklVMgc* gc
		,FklVMvalue** loc
		,FklVMframe* mainframe)
{
	FklVMCompoundFrameVarRef* lr=fklGetCompoundFrameLocRef(mainframe);
	FklPtrStack* urefs=&env->uref;
	FklFuncPrototype* pts=cp->pts;
	FklPtrStack urefs1=FKL_STACK_INIT;
	fklInitPtrStack(&urefs1,16,8);
	uint32_t count=urefs->top;
	for(uint32_t i=0;i<count;i++)
	{
		FklUnReSymbolRef* uref=urefs->base[i];
		FklFuncPrototype* cpt=&pts[uref->prototypeId];
		FklSymbolDef* ref=&cpt->refs[uref->idx];
		FklSymbolDef* def=fklFindSymbolDefByIdAndScope(uref->id,uref->scope,env);
		if(def)
		{
			env->slotFlags[def->idx]=FKL_CODEGEN_ENV_SLOT_REF;
			ref->cidx=def->idx;
			ref->isLocal=1;
			uint32_t prototypeId=uref->prototypeId;
			uint32_t idx=ref->idx;
			inc_lref(lr,lr->lcount);
			for(FklVMvalue* v=gc->head;v;v=v->next)
				if(FKL_IS_PROC(v)&&FKL_VM_PROC(v)->protoId==prototypeId)
				{
					FklVMproc* proc=FKL_VM_PROC(v);
					FklVMvarRef* ref=proc->closure[idx];
					if(lr->lref[def->idx])
					{
						proc->closure[idx]=fklMakeVMvarRefRef(lr->lref[def->idx]);
						fklDestroyVMvarRef(ref);
					}
					else
					{
						ref->idx=def->idx;
						ref->ref=&loc[def->idx];
						insert_local_ref(lr,ref,def->idx);
					}
				}
			free(uref);
		}
		else if(env->prev)
		{
			ref->cidx=fklAddCodegenRefBySid(uref->id,env,uref->fid,uref->line);
			free(uref);
		}
		else
			fklPushPtrStack(uref,&urefs1);
	}
	urefs->top=0;
	while(!fklIsPtrStackEmpty(&urefs1))
		fklPushPtrStack(fklPopPtrStack(&urefs1),urefs);
	fklUninitPtrStack(&urefs1);
}

static inline void update_prototype_lcount(FklFuncPrototypes* cp
		,FklCodegenEnv* env)
{
	FklFuncPrototype* pts=&cp->pts[env->prototypeId];
	pts->lcount=env->lcount;
}

static inline void update_prototype_ref(FklFuncPrototypes* cp
		,FklCodegenEnv* env
		,FklSymbolTable* globalSymTable
		,FklSymbolTable* pst)
{
	FklFuncPrototype* pts=&cp->pts[env->prototypeId];
	FklHashTable* eht=&env->refs;
	uint32_t count=eht->num;
	FklSymbolDef* refs=(FklSymbolDef*)fklRealloc(pts->refs,sizeof(FklSymbolDef)*count);
	FKL_ASSERT(refs||!count);
	pts->refs=refs;
	pts->rcount=count;
	for(FklHashTableItem* list=eht->first;list;list=list->next)
	{
		FklSymbolDef* sd=(FklSymbolDef*)list->data;
		FklSid_t sid=fklAddSymbol(fklGetSymbolWithId(sd->k.id,pst)->symbol,globalSymTable)->id;
		FklSymbolDef ref={.k.id=sid,
			.k.scope=sd->k.scope,
			.idx=sd->idx,
			.cidx=sd->cidx,
			.isLocal=sd->isLocal};
		refs[sd->idx]=ref;
	}
}

static inline void alloc_more_space_for_var_ref(FklVMCompoundFrameVarRef* lr
		,uint32_t i
		,uint32_t n)
{
	if(lr->lref&&i<n)
	{
		FklVMvarRef** lref=(FklVMvarRef**)fklRealloc(lr->lref,sizeof(FklVMvarRef*)*n);
		FKL_ASSERT(lref);
		for(;i<n;i++)
			lref[i]=NULL;
		lr->lref=lref;
	}
}

#include<fakeLisp/grammer.h>

static inline void repl_nast_ctx_and_buf_reset(NastCreatCtx* cc
		,FklStringBuffer* s
		,FklGrammer* g)
{
	cc->offset=0;
	fklStringBufferClear(s);
	s->buf[0]='\0';
	FklPtrStack* ss=&cc->symbolStack;
	while(!fklIsPtrStackEmpty(ss))
	{
		FklAnalysisSymbol* s=fklPopPtrStack(ss);
		fklDestroyNastNode(s->ast);
		free(s);
	}
	cc->stateStack.top=0;
	cc->lineStack.top=0;
	if(g&&g->aTable.num)
		fklPushPtrStack(&g->aTable.states[0],&cc->stateStack);
	else
		fklNastPushState0ToStack(&cc->stateStack);
}

static inline const char* replxx_input_string_buffer(Replxx* replxx
		,FklStringBuffer* buf)
{
	const char* next=replxx_input(replxx,buf->index?"":">>>");
	if(next)
		fklStringBufferConcatWithCstr(buf,next);
	return next;
}

// static void async_repl_cb(uv_work_t* req)
// {
// 	AsyncReplCtx* repl_ctx=(AsyncReplCtx*)req;
// 	repl_ctx->eof=replxx_input_string_buffer(repl_ctx->replxx,repl_ctx->buf)==NULL;
// }
//
// static void async_after_repl_cb(uv_work_t* req,int status)
// {
// 	FklVM* exe=uv_req_get_data((uv_req_t*)req);
// 	fklResumeThread(exe);
// }

// static int async_repl_start(FklVM* exe,AsyncReplCtx* ctx)
// {
// 	fklSuspendThread(exe);
// 	return uv_queue_work(exe->loop,(uv_work_t*)ctx,async_repl_cb,async_after_repl_cb);
// }

static void repl_frame_step(void* data,FklVM* exe)
{
	ReplCtx* ctx=(ReplCtx*)data;
	NastCreatCtx* cc=ctx->cc;

	if(ctx->state==READY)
	{
		ctx->state=WAITING;
		return;
	}
	else if(ctx->state==WAITING)
	{
		exe->ltp=ctx->lcount;
		ctx->state=READING;
		if(exe->tp!=0)
		{
			printf(";=>");
			fklDBG_printVMstack(exe,stdout,0,exe->symbolTable);
		}
		exe->tp=0;

		fklSuspendThread(exe);
		ctx->eof=replxx_input_string_buffer(ctx->replxx,&ctx->buf)==NULL;
		fklResumeThread(exe);
		return;
		// printf(">>>");
	}
	else
		ctx->state=WAITING;

	FklCodegenInfo* codegen=ctx->codegen;
	FklSymbolTable* pst=&codegen->outer_ctx->public_symbol_table;
	FklStringBuffer* s=&ctx->buf;
	int is_eof=ctx->eof;
	FklNastNode* ast=NULL;
	FklGrammerMatchOuterCtx outerCtx=FKL_NAST_PARSE_OUTER_CTX_INIT;
	outerCtx.line=codegen->curline;
	size_t restLen=fklStringBufferLen(s)-cc->offset;
	int err=0;
	size_t errLine=0;
	FklGrammer* g=*(codegen->g);
	if(g&&g->aTable.num)
	{
		ast=fklParseWithTableForCharBuf(g
				,fklStringBufferBody(s)+cc->offset
				,restLen
				,&restLen
				,&outerCtx
				,exe->symbolTable
				,&err
				,&errLine
				,&cc->symbolStack
				,&cc->lineStack
				,&cc->stateStack);
	}
	else
	{
		ast=fklDefaultParseForCharBuf(fklStringBufferBody(s)+cc->offset
				,restLen
				,&restLen
				,&outerCtx
				,exe->symbolTable
				,&err
				,&errLine
				,&cc->symbolStack
				,&cc->lineStack
				,&cc->stateStack);
	}
	cc->offset=fklStringBufferLen(s)-restLen;
	codegen->curline=outerCtx.line;
	if(!restLen&&cc->symbolStack.top==0&&is_eof)
		ctx->state=DONE;
	else if((err==FKL_PARSE_WAITING_FOR_MORE
				||(err==FKL_PARSE_TERMINAL_MATCH_FAILED&&!restLen))
			&&is_eof)
	{
		repl_nast_ctx_and_buf_reset(cc,s,g);
		FKL_RAISE_BUILTIN_ERROR_CSTR("reading",FKL_ERR_UNEXPECTED_EOF,exe);
	}
	else if(err==FKL_PARSE_TERMINAL_MATCH_FAILED&&restLen)
	{
		repl_nast_ctx_and_buf_reset(cc,s,g);
		FKL_RAISE_BUILTIN_ERROR_CSTR("reading",FKL_ERR_INVALIDEXPR,exe);
	}
	else if(err==FKL_PARSE_REDUCE_FAILED)
	{
		repl_nast_ctx_and_buf_reset(cc,s,g);
		FKL_RAISE_BUILTIN_ERROR_CSTR("reading",FKL_ERR_INVALIDEXPR,exe);
	}
	else if(ast)
	{
		if(restLen)
		{
			size_t idx=fklStringBufferLen(s)-restLen;
			replxx_set_preload_buffer(ctx->replxx,&s->buf[idx]);
			s->buf[idx]='\0';
		}
		ctx->state=DONE;

		fklMakeNastNodeRef(ast);
		size_t libNum=codegen->libStack->top;
		FklByteCodelnt* mainCode=fklGenExpressionCode(ast,codegen->globalEnv,codegen);
		replxx_history_add(ctx->replxx,s->buf);
		g=*(codegen->g);
		repl_nast_ctx_and_buf_reset(cc,s,g);
		fklDestroyNastNode(ast);
		size_t unloadlibNum=codegen->libStack->top-libNum;
		if(unloadlibNum)
		{
			FklVMproc* proc=FKL_VM_PROC(ctx->mainProc);
			libNum+=unloadlibNum;
			FklVMlib* nlibs=(FklVMlib*)calloc((libNum+1),sizeof(FklVMlib));
			FKL_ASSERT(nlibs);
			memcpy(nlibs,exe->libs,sizeof(FklVMlib)*(exe->libNum+1));
			for(size_t i=exe->libNum;i<libNum;i++)
			{
				FklVMlib* curVMlib=&nlibs[i+1];
				FklCodegenLib* curCGlib=codegen->libStack->base[i];
				fklInitVMlibWithCodegenLibRefs(curCGlib
						,curVMlib
						,exe
						,proc->closure
						,proc->rcount
						,0
						,exe->pts);
			}
			FklVMlib* prev=exe->libs;
			exe->libs=nlibs;
			exe->libNum=libNum;
			free(prev);
		}
		if(mainCode)
		{
			uint32_t o_lcount=ctx->lcount;
			ctx->state=READY;

			update_prototype_lcount(codegen->pts,codegen->globalEnv);

			update_prototype_ref(codegen->pts
					,codegen->globalEnv
					,codegen->globalSymTable
					,pst);
			FklVMvalue* mainProc=fklCreateVMvalueProc(exe,NULL,0,FKL_VM_NIL,1);
			FklVMproc* proc=FKL_VM_PROC(mainProc);
			init_with_main_proc(proc,FKL_VM_PROC(ctx->mainProc));
			ctx->mainProc=mainProc;

			FklVMvalue* mainCodeObj=fklCreateVMvalueCodeObj(exe,mainCode);
			ctx->lcount=codegen->pts->pts[1].lcount;

			mainCode=FKL_VM_CO(mainCodeObj);
			proc->lcount=ctx->lcount;
			proc->codeObj=mainCodeObj;
			proc->spc=mainCode->bc->code;
			proc->end=proc->spc+mainCode->bc->len;

			FklVMframe* mainframe=fklCreateVMframeWithProcValue(ctx->mainProc,exe->frames);
			FklVMCompoundFrameVarRef* f=&mainframe->c.lr;
			f->base=0;
			f->loc=fklAllocMoreSpaceForMainFrame(exe,proc->lcount);
			f->lcount=proc->lcount;
			alloc_more_space_for_var_ref(f,o_lcount,f->lcount);

			process_unresolve_ref_for_repl(codegen->globalEnv,codegen->pts,exe->gc,exe->locv,mainframe);

			exe->frames=mainframe;
		}
		else
		{
			ctx->state=WAITING;
			return;
		}
	}
	else
		fklStringBufferPutc(&ctx->buf,'\n');
}

static int repl_frame_end(void* data)
{
	ReplCtx* ctx=(ReplCtx*)data;
	return ctx->state==DONE;
}

static void repl_frame_print_backtrace(void* data,FILE* fp,FklSymbolTable* table)
{
}

static void repl_frame_atomic(void* data,FklVMgc* gc)
{
	ReplCtx* ctx=(ReplCtx*)data;
	fklVMgcToGray(ctx->stdinVal,gc);
	fklVMgcToGray(ctx->mainProc,gc);
	FklVMvalue** locv=ctx->exe->locv;
	uint32_t lcount=ctx->lcount;
	for(uint32_t i=0;i<lcount;i++)
		fklVMgcToGray(locv[i],gc);
}

static inline void destroyNastCreatCtx(NastCreatCtx* cc)
{
	fklUninitPtrStack(&cc->stateStack);
	while(!fklIsPtrStackEmpty(&cc->symbolStack))
	{
		FklAnalysisSymbol* s=fklPopPtrStack(&cc->symbolStack);
		fklDestroyNastNode(s->ast);
		free(s);
	}
	fklUninitUintStack(&cc->lineStack);
	fklUninitPtrStack(&cc->symbolStack);
	free(cc);
}

static void repl_frame_finalizer(void* data)
{
	ReplCtx* ctx=(ReplCtx*)data;
	fklUninitStringBuffer(&ctx->buf);
	destroyNastCreatCtx(ctx->cc);
	replxx_end(ctx->replxx);
	// free(ctx->async_repl_ctx);
}

static const FklVMframeContextMethodTable ReplContextMethodTable=
{
	.atomic=repl_frame_atomic,
	.finalizer=repl_frame_finalizer,
	.copy=NULL,
	.print_backtrace=repl_frame_print_backtrace,
	.step=repl_frame_step,
	.end=repl_frame_end,
};

static inline NastCreatCtx* createNastCreatCtx(void)
{
	NastCreatCtx* cc=(NastCreatCtx*)malloc(sizeof(NastCreatCtx));
	FKL_ASSERT(cc);
	cc->offset=0;
	fklInitPtrStack(&cc->symbolStack,16,16);
	fklInitUintStack(&cc->lineStack,16,16);
	fklInitPtrStack(&cc->stateStack,16,16);
	fklNastPushState0ToStack(&cc->stateStack);
	return cc;
}

static int replErrorCallBack(FklVMframe* f,FklVMvalue* errValue,FklVM* exe)
{
	exe->tp=0;
	exe->bp=0;
	fklPrintErrBacktrace(errValue,exe,stderr);
	while(exe->frames->prev)
	{
		FklVMframe* cur=exe->frames;
		exe->frames=cur->prev;
		fklDestroyVMframe(cur,exe);
	}
	ReplCtx* ctx=(ReplCtx*)exe->frames->data;
	ctx->state=READY;
	return 1;
}

static inline void init_frame_to_repl_frame(FklVM* exe,FklCodegenInfo* codegen)
{
	FklVMframe* replFrame=(FklVMframe*)calloc(1,sizeof(FklVMframe));
	FKL_ASSERT(replFrame);
	replFrame->prev=NULL;
	replFrame->errorCallBack=replErrorCallBack;
	exe->frames=replFrame;
	replFrame->type=FKL_FRAME_OTHEROBJ;
	replFrame->t=&ReplContextMethodTable;
	ReplCtx* ctx=(ReplCtx*)replFrame->data;

	FklVMvalue* mainProc=fklCreateVMvalueProc(exe,NULL,0,FKL_VM_NIL,1);
	FklVMproc* proc=FKL_VM_PROC(mainProc);
	fklInitGlobalVMclosureForProc(proc,exe);
	ctx->replxx=replxx_init();
	ctx->exe=exe;
	ctx->mainProc=mainProc;
	ctx->lcount=0;

	ctx->stdinVal=proc->closure[FKL_VM_STDIN_IDX]->v;
	ctx->codegen=codegen;
	NastCreatCtx* cc=createNastCreatCtx();
	ctx->cc=cc;
	ctx->state=READY;
	fklInitStringBuffer(&ctx->buf);
	// AsyncReplCtx* repl_ctx=(AsyncReplCtx*)malloc(sizeof(AsyncReplCtx));
	// FKL_ASSERT(repl_ctx);
	// repl_ctx->buf=&ctx->buf;
	// repl_ctx->eof=0;
	// repl_ctx->replxx=ctx->replxx;
	// uv_req_set_data((uv_req_t*)&repl_ctx->req,exe);
	// ctx->async_repl_ctx=repl_ctx;
}


