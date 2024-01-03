#include"bdb.h"
#include<fakeLisp/builtin.h>

static inline void init_cmd_read_ctx(CmdReadCtx* ctx)
{
	ctx->replxx=replxx_init();
	fklInitStringBuffer(&ctx->buf);
	fklInitPtrStack(&ctx->symbolStack,16,16);
	fklInitPtrStack(&ctx->stateStack,16,16);
	fklInitUintStack(&ctx->lineStack,16,16);
	fklVMvaluePushState0ToStack(&ctx->stateStack);
}

static inline void replace_info_fid_with_realpath(FklCodegenInfo* info)
{
	FklSid_t rpsid=fklAddSymbolCstr(info->realpath,info->globalSymTable)->id;
	info->fid=rpsid;
}

static void info_work_cb(FklCodegenInfo* info,void* ctx)
{
	if(!info->macroMark)
	{
		DebugCtx* dctx=(DebugCtx*)ctx;
		replace_info_fid_with_realpath(info);
		fklPutSidToSidSet(&dctx->file_sid_set,info->fid);
	}
}

static void create_env_work_cb(FklCodegenInfo* info,FklCodegenEnv* env,void* ctx)
{
	if(!info->macroMark)
	{
		env->refcount++;
		DebugCtx* dctx=(DebugCtx*)ctx;
		fklPushPtrStack(env,&dctx->envs);
	}
}

struct Int3Arg
{
	BreakpointHashItem* bp;
};

static void B_int3(FKL_VM_INS_FUNC_ARGL);
static void B_int33(FKL_VM_INS_FUNC_ARGL);

static void int3_queue_work_cb(FklVM* vm,void* a)
{
	struct Int3Arg* arg=(struct Int3Arg*)a;
	BreakpointHashItem* bp=arg->bp;
	FklVMframe* frame=vm->top_frame;
	frame->c.pc--;
	DebugCtx* ctx=bp->ctx;
	ctx->cur_thread=vm;
	getCurLineStr(ctx,bp->key.fid,bp->key.line);
	ctx->is_reach_breakpoint=1;
	longjmp(arg->bp->ctx->jmpb,DBG_REACH_BREAKPOINT);
}

static void B_int3(FKL_VM_INS_FUNC_ARGL)
{
	struct Int3Arg arg=
	{
		.bp=ins->ptr,
	};
	exe->ins_table[0]=B_int33;
	fklQueueWorkInIdleThread(exe,int3_queue_work_cb,&arg);
}

static void B_int33(FKL_VM_INS_FUNC_ARGL)
{
	exe->ins_table[0]=B_int3;
	FklInstruction* oins=((BreakpointHashItem*)ins->ptr)->ins;
	exe->ins_table[oins->op](exe,oins);
}

static inline int init_debug_codegen_outer_ctx(DebugCtx* ctx,const char* filename)
{
	FILE* fp=fopen(filename,"r");
	char* rp=fklRealpath(filename);
	FklCodegenOuterCtx* outer_ctx=&ctx->outer_ctx;
	FklCodegenInfo codegen={.fid=0};
	fklInitCodegenOuterCtx(outer_ctx,fklGetDir(rp));
	FklSymbolTable* pst=&outer_ctx->public_symbol_table;
	fklAddSymbolCstr(filename,pst);
	fklInitGlobalCodegenInfo(&codegen
			,rp
			,pst
			,0
			,outer_ctx
			,info_work_cb
			,create_env_work_cb
			,ctx);
	free(rp);
	FklByteCodelnt* mainByteCode=fklGenExpressionCodeWithFp(fp,&codegen);
	if(mainByteCode==NULL)
	{
		fklUninitCodegenInfo(&codegen);
		fklUninitCodegenOuterCtx(outer_ctx);
		return 1;
	}
	fklUpdatePrototype(codegen.pts
			,codegen.globalEnv
			,codegen.globalSymTable
			,pst);
	fklPrintUndefinedRef(codegen.globalEnv,codegen.globalSymTable,pst);

	FklPtrStack* scriptLibStack=codegen.libStack;
	FklVM* anotherVM=fklCreateVM(mainByteCode,codegen.globalSymTable,codegen.pts,1);
	codegen.globalSymTable=NULL;
	codegen.pts=NULL;
	anotherVM->libNum=scriptLibStack->top;
	anotherVM->libs=(FklVMlib*)calloc((scriptLibStack->top+1),sizeof(FklVMlib));
	FKL_ASSERT(anotherVM->libs);
	FklVMframe* mainframe=anotherVM->top_frame;
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
		fklInitVMlibWithCodegenLib(cur,curVMlib,anotherVM,1,anotherVM->pts);
		if(type==FKL_CODEGEN_LIB_SCRIPT)
			fklInitMainProcRefs(FKL_VM_PROC(curVMlib->proc),lr->ref,lr->rcount);
	}
	fklUninitCodegenInfo(&codegen);
	fklChdir(outer_ctx->cwd);

    ctx->st=&outer_ctx->public_symbol_table;
	ctx->gc=gc;
	ctx->cur_thread=anotherVM;

	anotherVM->ins_table[0]=B_int3;
	gc->main_thread=anotherVM;
	fklVMthreadStart(anotherVM,&gc->q);
	return 0;
}

static inline void set_argv_with_list(FklVMgc* gc,FklVMvalue* argv_list)
{
	int argc=fklVMlistLength(argv_list);
	gc->argc=argc;
	char** argv=(char**)malloc(sizeof(char*)*argc);
	FKL_ASSERT(argv);
	for(int i=0;i<argc;i++)
	{
		argv[i]=fklStringToCstr(FKL_VM_STR(FKL_VM_CAR(argv_list)));
		argv_list=FKL_VM_CDR(argv_list);
	}
	gc->argv=argv;
}

static void source_hash_item_uninit(void* d0)
{
	SourceCodeHashItem* item=(SourceCodeHashItem*)d0;
	void** base=item->lines.base;
	void** const end=item->lines.base[item->lines.top];
	for(;base<end;base++)
		free(*base);
	fklUninitPtrStack(&item->lines);
}

static const FklHashTableMetaTable SourceCodeHashMetaTable=
{
	.size=sizeof(SourceCodeHashItem),
	.__setKey=fklSetSidKey,
	.__setVal=fklSetSidKey,
	.__hashFunc=fklSidHashFunc,
	.__keyEqual=fklSidKeyEqual,
	.__getKey=fklHashDefaultGetKey,
	.__uninitItem=source_hash_item_uninit,
};

static inline void load_source_code_to_source_code_hash_item(SourceCodeHashItem* item
		,const char* rp)
{
	FILE* fp=fopen(rp,"r");
	FKL_ASSERT(fp);
	FklPtrStack* lines=&item->lines;
	fklInitPtrStack(lines,16,16);
	FklStringBuffer buffer;
	fklInitStringBuffer(&buffer);
	while(!feof(fp))
	{
		fklGetDelim(fp,&buffer,'\n');
		if(!buffer.index||buffer.buf[buffer.index-1]!='\n')
			fklStringBufferPutc(&buffer,'\n');
		FklString* cur_line=fklCreateString(buffer.index,buffer.buf);
		fklPushPtrStack(cur_line,lines);
		buffer.index=0;
	}
}

static inline void init_source_codes(DebugCtx* ctx)
{
	fklInitHashTable(&ctx->source_code_table,&SourceCodeHashMetaTable);
	FklHashTable* source_code_table=&ctx->source_code_table;
	for(FklHashTableItem* sid_list=ctx->file_sid_set.first
			;sid_list
			;sid_list=sid_list->next)
	{
		FklSid_t fid=*((FklSid_t*)sid_list->data);
		const FklString* str=fklGetSymbolWithId(fid,ctx->st)->symbol;
		SourceCodeHashItem* item=fklPutHashItem(&fid,source_code_table);
		load_source_code_to_source_code_hash_item(item,str->str);
	}
}

const SourceCodeHashItem* get_source_with_fid(FklHashTable* t,FklSid_t id)
{
	return fklGetHashItem(&id,t);
}

const FklString* getCurLineStr(DebugCtx* ctx,FklSid_t fid,uint32_t line)
{
	if(fid==ctx->curline_file&&line==ctx->curline)
		return ctx->curline_str;
	else if(fid==ctx->curline_file)
	{
		if(line>ctx->curfile_lines->top)
			return NULL;
		ctx->curline=line;
		ctx->curline_str=ctx->curfile_lines->base[line-1];
		return ctx->curline_str;
	}
	else
	{
		const SourceCodeHashItem* item=get_source_with_fid(&ctx->source_code_table,fid);
		if(item&&line<=item->lines.top)
		{
			ctx->curline_file=fid;
			ctx->curline=line;
			ctx->curfile_lines=&item->lines;
			ctx->curline_str=item->lines.base[line-1];
			return ctx->curline_str;
		}
		return NULL;
	}
}

static inline void get_all_code_objs(DebugCtx* ctx)
{
	FklVMvalue** phead=&ctx->gc->head;
	for(;;)
	{
		FklVMvalue* cur=*phead;
		if(cur==NULL)
			break;
		if(cur->type==FKL_TYPE_CODE_OBJ)
		{
			*phead=cur->next;
			cur->next=ctx->code_objs;
			ctx->code_objs=cur;
		}
		else
			phead=&cur->next;
	}
	phead=&ctx->cur_thread->obj_head;
	for(;;)
	{
		FklVMvalue* cur=*phead;
		if(cur==NULL)
			break;
		if(cur->type==FKL_TYPE_CODE_OBJ)
		{
			*phead=cur->next;
			cur->next=ctx->code_objs;
			ctx->code_objs=cur;
		}
		else
			phead=&cur->next;
	}
}

DebugCtx* createDebugCtx(FklVM* exe,const char* filename,FklVMvalue* argv)
{
	DebugCtx* ctx=(DebugCtx*)calloc(1,sizeof(DebugCtx));
	FKL_ASSERT(ctx);
	fklInitPtrStack(&ctx->envs,16,16);
	fklInitSidSet(&ctx->file_sid_set);
	if(init_debug_codegen_outer_ctx(ctx,filename))
	{
		fklUninitPtrStack(&ctx->envs);
		fklUninitHashTable(&ctx->file_sid_set);
		free(ctx);
		return NULL;
	}

	init_source_codes(ctx);
	get_all_code_objs(ctx);
	const FklLineNumberTableItem* ln=getCurFrameLineNumber(ctx->cur_thread->top_frame);
	ctx->curline_str=getCurLineStr(ctx,ln->fid,ln->line);

	set_argv_with_list(ctx->gc,argv);
	init_cmd_read_ctx(&ctx->read_ctx);

	return ctx;
}

const FklLineNumberTableItem* getCurFrameLineNumber(const FklVMframe* frame)
{
	if(frame->type==FKL_FRAME_COMPOUND)
	{
		FklVMproc* proc=FKL_VM_PROC(frame->c.proc);
		FklByteCodelnt* code=FKL_VM_CO(proc->codeObj);
		return fklFindLineNumTabNode(
				fklGetCompoundFrameCode(frame)-code->bc->code
				,code->ls
				,code->l);
	}
	return NULL;
}

