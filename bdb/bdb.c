#include"bdb.h"
#include<fakeLisp/builtin.h>
#include<string.h>

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
		DebugCtx* dctx=(DebugCtx*)ctx;
		putEnv(dctx,env);
	}
}

static void B_int3(FKL_VM_INS_FUNC_ARGL);
static void B_int33(FKL_VM_INS_FUNC_ARGL);

static void B_int3(FKL_VM_INS_FUNC_ARGL)
{
	if(exe->is_single_thread)
	{
		FklInstruction* oins=&((Breakpoint*)ins->ptr)->origin_ins;
		fklVMexcuteInstruction(exe,oins,exe->top_frame);
	}
	else
	{
		exe->dummy_ins_func=B_int33;
		exe->top_frame->c.pc--;
		fklVMinterrupt(exe,createBreakpointWrapper(exe,ins->ptr),NULL);
	}
}

static void B_int33(FKL_VM_INS_FUNC_ARGL)
{
	exe->dummy_ins_func=B_int3;
	FklInstruction* oins=&((Breakpoint*)ins->ptr)->origin_ins;
	fklVMexcuteInstruction(exe,oins,exe->top_frame);
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
	FklCodegenEnv* main_env=fklInitGlobalCodegenInfo(&codegen
			,rp
			,pst
			,0
			,outer_ctx
			,info_work_cb
			,create_env_work_cb
			,ctx);
	free(rp);
	FklByteCodelnt* mainByteCode=fklGenExpressionCodeWithFp(fp,&codegen,main_env);
	if(mainByteCode==NULL)
	{
		fklDestroyCodegenEnv(main_env);
		fklUninitCodegenInfo(&codegen);
		fklUninitCodegenOuterCtx(outer_ctx);
		return 1;
	}
	fklUpdatePrototype(codegen.pts
			,main_env
			,codegen.globalSymTable
			,pst);
	fklDestroyCodegenEnv(main_env);
	fklPrintUndefinedRef(codegen.globalEnv,codegen.globalSymTable,pst);

	FklPtrStack* scriptLibStack=codegen.libStack;
	FklVM* anotherVM=fklCreateVMwithByteCode(mainByteCode,codegen.globalSymTable,codegen.pts,1);
	codegen.globalSymTable=NULL;
	codegen.pts=NULL;
	anotherVM->libNum=scriptLibStack->top;
	anotherVM->libs=(FklVMlib*)calloc((scriptLibStack->top+1),sizeof(FklVMlib));
	FKL_ASSERT(anotherVM->libs);

	FklVMgc* gc=anotherVM->gc;
	while(!fklIsPtrStackEmpty(scriptLibStack))
	{
		FklVMlib* curVMlib=&anotherVM->libs[scriptLibStack->top];
		FklCodegenLib* cur=fklPopPtrStack(scriptLibStack);
		FklCodegenLibType type=cur->type;
		fklInitVMlibWithCodegenLibAndDestroy(cur,curVMlib,anotherVM,anotherVM->pts);
		if(type==FKL_CODEGEN_LIB_SCRIPT)
			fklInitMainProcRefs(anotherVM,curVMlib->proc);
	}
	fklUninitCodegenInfo(&codegen);
	fklChdir(outer_ctx->cwd);

    ctx->st=&outer_ctx->public_symbol_table;
	ctx->gc=gc;
	ctx->reached_thread=anotherVM;

	anotherVM->dummy_ins_func=B_int3;

	gc->main_thread=anotherVM;
	fklVMpushInterruptHandler(gc,dbgInterruptHandler,NULL,NULL,ctx);
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
	void** const end=&item->lines.base[item->lines.top];
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
	fklUninitStringBuffer(&buffer);
	fclose(fp);
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
			ctx->curlist_line=1;
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
	fklInitPtrStack(&ctx->code_objs,16,16);
	FklVMvalue* head=ctx->gc->head;
	for(;head;head=head->next)
	{
		if(head->type==FKL_TYPE_CODE_OBJ)
			fklPushPtrStack(head,&ctx->code_objs);
	}
	head=ctx->reached_thread->obj_head;
	for(;head;head=head->next)
	{
		if(head->type==FKL_TYPE_CODE_OBJ)
			fklPushPtrStack(head,&ctx->code_objs);
	}
}

static inline void internal_dbg_extra_mark(DebugCtx* ctx,FklVMgc* gc)
{
	FklVMvalue** base=(FklVMvalue**)ctx->extra_mark_value.base;
	FklVMvalue** last=&base[ctx->extra_mark_value.top];
	for(;base<last;base++)
		fklVMgcToGray(*base,gc);

	base=(FklVMvalue**)ctx->code_objs.base;
	last=&base[ctx->code_objs.top];
	for(;base<last;base++)
		fklVMgcToGray(*base,gc);

	for(FklHashTableItem* l=ctx->breakpoints.first
			;l
			;l=l->next)
	{
		Breakpoint* i=((BreakpointHashItem*)l->data)->bp;
		if(i->compiled)
			fklVMgcToGray(i->proc,gc);
	}

	base=gc->builtin_refs;
	last=&base[FKL_BUILTIN_SYMBOL_NUM];
	for(;base<last;base++)
		fklVMgcToGray(*base,gc);
}

static void dbg_extra_mark(FklVMgc* gc,void* arg)
{
	DebugCtx* ctx=(DebugCtx*)arg;
	internal_dbg_extra_mark(ctx,gc);
}

static inline void push_extra_mark_value(DebugCtx* ctx)
{
	FklVM* vm=ctx->reached_thread;
	fklInitPtrStack(&ctx->extra_mark_value,vm->libNum+1,16);
	fklPushPtrStack(vm->top_frame->c.proc,&ctx->extra_mark_value);
	const uint64_t last=vm->libNum+1;
	for(uint64_t i=1;i<last;i++)
	{
		FklVMlib* cur=&vm->libs[i];
		fklPushPtrStack(cur->proc,&ctx->extra_mark_value);
	}
	fklVMpushExtraMarkFunc(ctx->gc,dbg_extra_mark,NULL,ctx);
}

DebugCtx* createDebugCtx(FklVM* exe,const char* filename,FklVMvalue* argv)
{
	DebugCtx* ctx=(DebugCtx*)calloc(1,sizeof(DebugCtx));
	FKL_ASSERT(ctx);
	initEnvTable(&ctx->envs);
	fklInitSidSet(&ctx->file_sid_set);
	if(init_debug_codegen_outer_ctx(ctx,filename))
	{
		fklUninitHashTable(&ctx->envs);
		fklUninitHashTable(&ctx->file_sid_set);
		free(ctx);
		return NULL;
	}

	fklInitPtrStack(&ctx->reached_thread_frames,16,16);
	fklInitPtrStack(&ctx->threads,16,16);
	fklInitUintStack(&ctx->unused_prototype_id_for_cond_bp,16,16);

	setReachedThread(ctx,ctx->reached_thread);
	init_source_codes(ctx);
	ctx->temp_proc_prototype_id=fklInsertEmptyFuncPrototype(ctx->gc->pts);
	get_all_code_objs(ctx);
	push_extra_mark_value(ctx);
	initBreakpointTable(&ctx->breakpoints);
	const FklLineNumberTableItem* ln=getCurFrameLineNumber(ctx->reached_thread->top_frame);
	ctx->curline_str=getCurLineStr(ctx,ln->fid,ln->line);

	ctx->curlist_ins_pc=0;
	ctx->curlist_bytecode=ctx->reached_thread->top_frame->c.proc;

	ctx->glob_env=fklCreateCodegenEnv(NULL,1,NULL);
	fklInitGlobCodegenEnv(ctx->glob_env,ctx->st);
	ctx->glob_env->refcount++;

	set_argv_with_list(ctx->gc,argv);
	init_cmd_read_ctx(&ctx->read_ctx);

	return ctx;
}

static inline void uninit_cmd_read_ctx(CmdReadCtx* ctx)
{
	fklUninitPtrStack(&ctx->stateStack);
	fklUninitPtrStack(&ctx->symbolStack);
	fklUninitUintStack(&ctx->lineStack);
	fklUninitStringBuffer(&ctx->buf);
	replxx_end(ctx->replxx);
}

static inline void destroy_all_deleted_breakpoint(DebugCtx* ctx)
{
	Breakpoint* bp=ctx->deleted_breakpoints;
	while(bp)
	{
		Breakpoint* cur=bp;
		bp=bp->next;
		if(cur->cond_exp)
			fklDestroyNastNode(cur->cond_exp);
		free(cur);
	}
}

void exitDebugCtx(DebugCtx* ctx)
{
	FklVMgc* gc=ctx->gc;
	if(ctx->running&&ctx->reached_thread)
	{
		setAllThreadReadyToExit(ctx->reached_thread);
		waitAllThreadExit(ctx->reached_thread);
		ctx->running=0;
		ctx->reached_thread=NULL;
	}
	else
		fklDestroyAllVMs(gc->main_thread);
}

void destroyDebugCtx(DebugCtx* ctx)
{
	fklUninitHashTable(&ctx->envs);
	fklUninitHashTable(&ctx->file_sid_set);

	clearBreakpoint(ctx);
	destroy_all_deleted_breakpoint(ctx);
	fklUninitHashTable(&ctx->breakpoints);
	fklUninitHashTable(&ctx->source_code_table);
	fklUninitPtrStack(&ctx->extra_mark_value);
	fklUninitPtrStack(&ctx->code_objs);
	fklUninitPtrStack(&ctx->threads);
	fklUninitPtrStack(&ctx->reached_thread_frames);
	fklUninitUintStack(&ctx->unused_prototype_id_for_cond_bp);

	fklDestroyVMgc(ctx->gc);
	uninit_cmd_read_ctx(&ctx->read_ctx);
	fklUninitCodegenOuterCtx(&ctx->outer_ctx);
	fklDestroyCodegenEnv(ctx->glob_env);
	free(ctx);
}

const FklLineNumberTableItem* getCurLineNumberItemWithCp(const FklInstruction* cp
		,FklByteCodelnt* code)
{
	return fklFindLineNumTabNode(cp-code->bc->code
			,code->ls
			,code->l);
}

const FklLineNumberTableItem* getCurFrameLineNumber(const FklVMframe* frame)
{
	if(frame->type==FKL_FRAME_COMPOUND)
	{
		FklVMproc* proc=FKL_VM_PROC(frame->c.proc);
		FklByteCodelnt* code=FKL_VM_CO(proc->codeObj);
		return getCurLineNumberItemWithCp(
				fklGetCompoundFrameCode(frame)
				,code);
	}
	return NULL;
}

void toggleVMint3(FklVM* exe)
{
	if(exe->dummy_ins_func==B_int33)
		exe->dummy_ins_func=B_int3;
	else
		exe->dummy_ins_func=B_int33;
}

const SourceCodeHashItem* getSourceWithFid(DebugCtx* dctx,FklSid_t fid)
{
	return get_source_with_fid(&dctx->source_code_table,fid);
}

Breakpoint* putBreakpointWithFileAndLine(DebugCtx* ctx
		,FklSid_t fid
		,uint32_t line
		,PutBreakpointErrorType* err)
{
	const SourceCodeHashItem* sc_item=get_source_with_fid(&ctx->source_code_table,fid);
	if(!sc_item)
	{
		*err=PUT_BP_FILE_INVALID;
		return NULL;
	}
	if(!line||line>sc_item->lines.top)
	{
		*err=PUT_BP_AT_END_OF_FILE;
		return NULL;
	}

	FklInstruction* ins=NULL;
	for(;line<=sc_item->lines.top;line++)
	{
		FklVMvalue** base=(FklVMvalue**)ctx->code_objs.base;
		FklVMvalue** const end=&base[ctx->code_objs.top];

		for(;base<end;base++)
		{
			FklVMvalue* cur=*base;
			FklByteCodelnt* bclnt=FKL_VM_CO(cur);
			FklLineNumberTableItem* item=bclnt->l;
			FklLineNumberTableItem* const end=&item[bclnt->ls];
			for(;item<end;item++)
			{
				if(item->fid==fid&&item->line==line)
				{
					ins=&bclnt->bc->code[item->scp];
					goto break_loop;
				}
			}
		}
	}
break_loop:
	if(ins)
	{
		ctx->breakpoint_num++;
		BreakpointHashItem* item=fklPutHashItem(&ctx->breakpoint_num,&ctx->breakpoints);
		item->bp=createBreakpoint(ctx->breakpoint_num,fid,line,ins,ctx);
		return item->bp;
	}
	*err=PUT_BP_AT_END_OF_FILE;
	return NULL;
}

const char* getPutBreakpointErrorInfo(PutBreakpointErrorType t)
{
	static const char* msgs[]=
	{
		NULL,
		"end of file",
		"file is invalid",
		"the specified symbol is undefined or not a procedure",
	};
	return msgs[t];
}

FklVMvalue* findLocalVar(DebugCtx* ctx,FklSid_t id)
{
	FklVM* cur_thread=ctx->reached_thread;
	FklVMframe* frame=cur_thread->top_frame;
	for(;frame->type==FKL_FRAME_OTHEROBJ;frame=frame->prev);
	if(!frame)
		return NULL;
	uint32_t scope=getCurFrameLineNumber(frame)->scope;
	if(scope==0)
		return NULL;
	uint32_t prototype_id=FKL_VM_PROC(frame->c.proc)->protoId; 
	FklCodegenEnv* env=getEnv(ctx,prototype_id);
	if(env==NULL)
		return NULL;
	const FklSymbolDef* def=fklFindSymbolDefByIdAndScope(id,scope,env);
	if(def)
		return cur_thread->locv[def->idx];
	return NULL;
}

FklVMvalue* findClosureVar(DebugCtx* ctx,FklSid_t id)
{
	FklVM* cur_thread=ctx->reached_thread;
	FklVMframe* frame=cur_thread->top_frame;
	for(;frame->type==FKL_FRAME_OTHEROBJ;frame=frame->prev);
	if(!frame)
		return NULL;
	FklVMproc* proc=FKL_VM_PROC(frame->c.proc);
	uint32_t prototype_id=proc->protoId;
	FklFuncPrototype* pt=&ctx->reached_thread->pts->pa[prototype_id];
	FklSymbolDef* def=pt->refs;
	FklSymbolDef* const end=&def[pt->rcount];
	for(;def<end;def++)
		if(def->k.id==id)
			return *(FKL_VM_VAR_REF_GET(proc->closure[def->idx]));
	return NULL;
}

static inline const FklLineNumberTableItem* get_proc_start_line_number(const FklVMproc* proc)
{
	FklByteCodelnt* code=FKL_VM_CO(proc->codeObj);
	return fklFindLineNumTabNode(
			proc->spc-code->bc->code
			,code->ls
			,code->l);
}

static inline Breakpoint* put_breakpoint_with_pc(DebugCtx* ctx
		,uint64_t pc
		,FklInstruction* ins
		,const FklLineNumberTableItem* ln)
{
	ctx->breakpoint_num++;
	BreakpointHashItem* item=fklPutHashItem(&ctx->breakpoint_num,&ctx->breakpoints);
	item->bp=createBreakpoint(ctx->breakpoint_num,ln->fid,ln->line,ins,ctx);
	return item->bp;
}

Breakpoint* putBreakpointForProcedure(DebugCtx* ctx,FklSid_t name_sid)
{
	FklVMvalue* var_value=findLocalVar(ctx,name_sid);
	if(var_value==NULL)
		var_value=findClosureVar(ctx,name_sid);
	if(var_value&&FKL_IS_PROC(var_value))
	{
		FklVMproc* proc=FKL_VM_PROC(var_value);
		FklByteCodelnt* code=FKL_VM_CO(proc->codeObj);
		uint64_t pc=proc->spc-code->bc->code;
		const FklLineNumberTableItem* ln=get_proc_start_line_number(proc);
		return put_breakpoint_with_pc(ctx,pc,proc->spc,ln);
	}
	return NULL;
}

void printBacktrace(DebugCtx* ctx,const FklString* prefix,FILE* fp)
{
	if(ctx->reached_thread_frames.top)
	{
		FklVM* vm=ctx->reached_thread;
		uint32_t top=ctx->reached_thread_frames.top;
		void** base=ctx->reached_thread_frames.base;
		for(uint32_t i=0;i<top;i++)
		{
			FklVMframe* cur=base[i];
			if(i+1==ctx->curframe_idx)
				fklPrintString(prefix,fp);
			else
				for(uint32_t i=0;i<prefix->size;i++)
					fputc(' ',fp);
			fklPrintFrame(cur,vm,fp);
		}
	}
	else
		printThreadAlreadyExited(ctx,fp);

}

FklVMframe* getCurrentFrame(DebugCtx* ctx)
{
	if(ctx->reached_thread_frames.top)
	{
		void** base=ctx->reached_thread_frames.base;
		FklVMframe* cur=base[ctx->curframe_idx-1];
		return cur;
	}
	return NULL;
}

void printCurFrame(DebugCtx* ctx,const FklString* prefix,FILE* fp)
{
	if(ctx->reached_thread_frames.top)
	{
		FklVM* vm=ctx->reached_thread;
		FklVMframe* cur=getCurrentFrame(ctx);
		fklPrintString(prefix,fp);
		fklPrintFrame(cur,vm,fp);
	}
	else
		printThreadAlreadyExited(ctx,fp);
}

void setReachedThread(DebugCtx* ctx,FklVM* vm)
{
	ctx->reached_thread=vm;
	ctx->curframe_idx=1;
	ctx->reached_thread_frames.top=0;
	for(FklVMframe* f=vm->top_frame;f;f=f->prev)
		fklPushPtrStack(f,&ctx->reached_thread_frames);
	for(FklVMframe* f=vm->top_frame;f;f=f->prev)
	{
		if(f->type==FKL_FRAME_COMPOUND)
		{
			FklVMvalue* bytecode=FKL_VM_PROC(f->c.proc)->codeObj;
			if(bytecode!=ctx->curlist_bytecode)
			{
				ctx->curlist_bytecode=bytecode;
				ctx->curlist_ins_pc=0;
			}
			break;
		}
	}
	ctx->curthread_idx=1;
	ctx->threads.top=0;
	fklPushPtrStack(vm,&ctx->threads);
	for(FklVM* cur=vm->next;cur!=vm;cur=cur->next)
		fklPushPtrStack(cur,&ctx->threads);
}

void listThreads(DebugCtx* ctx,const FklString* prefix,FILE* fp)
{
	FklVM** base=(FklVM**)ctx->threads.base;
	uint32_t top=ctx->threads.top;
	for(uint32_t i=0;i<top;i++)
	{
		FklVM* cur=base[i];
		if(i+1==ctx->curthread_idx)
			fklPrintString(prefix,fp);
		else
			for(uint32_t i=0;i<prefix->size;i++)
				fputc(' ',fp);
		fprintf(fp,"thread %u ",i+1);
		if(cur->top_frame)
			fklPrintFrame(cur->top_frame,cur,fp);
		else
			fputs("exited\n",fp);
	}
}

void switchCurThread(DebugCtx* ctx,uint32_t idx)
{
	ctx->curthread_idx=idx;
	ctx->curframe_idx=1;
	ctx->reached_thread_frames.top=0;
	FklVM* vm=getCurThread(ctx);
	ctx->reached_thread=vm;
	for(FklVMframe* f=vm->top_frame;f;f=f->prev)
		fklPushPtrStack(f,&ctx->reached_thread_frames);
}

FklVM* getCurThread(DebugCtx* ctx)
{
	if(ctx->threads.top)
		return (FklVM*)ctx->threads.base[ctx->curthread_idx-1];
	return NULL;
}

void printThreadAlreadyExited(DebugCtx* ctx,FILE* fp)
{
	fprintf(fp,"*** thread %u already exited ***\n",ctx->curthread_idx);
}

void printThreadCantEvaluate(DebugCtx* ctx,FILE* fp)
{
	fprintf(fp,"*** can't evaluate expression in thread %u ***\n",ctx->curthread_idx);
}

void printUnableToCompile(FILE* fp)
{
	fputs("*** can't compile expression in current frame ***\n",fp);
}

void printNotAllowImport(FILE* fp)
{
	fputs("*** not allow to import lib in debug evaluation ***\n",fp);
}

void setAllThreadReadyToExit(FklVM* head)
{
	fklSetThreadReadyToExit(head);
	for(FklVM* cur=head->next;cur!=head;cur=cur->next)
		fklSetThreadReadyToExit(cur);
}

void waitAllThreadExit(FklVM* head)
{
	FklVMgc* gc=head->gc;
	fklVMreleaseWq(gc);
	fklVMcontinueTheWorld(gc);
	fklVMidleLoop(gc);
	fklDestroyAllVMs(gc->main_thread);
}

void restartDebugging(DebugCtx* ctx)
{
	FklVMgc* gc=ctx->gc;
	internal_dbg_extra_mark(ctx,gc);
	while(!fklVMgcPropagate(gc));
	FklVMvalue* white=NULL;
	fklVMgcCollect(gc,&white);
	fklVMgcSweep(white);
	fklVMgcRemoveUnusedGrayCache(gc);
	fklVMgcUpdateThreshold(gc);

	FklVMvalue** base=(FklVMvalue**)ctx->extra_mark_value.base;
	FklVMvalue** const end=&base[ctx->extra_mark_value.top];
	FklVMvalue* main_proc=base[0];

	base++;
	uint64_t lib_num=ctx->extra_mark_value.top-1;
	FklVMlib* libs=(FklVMlib*)calloc(lib_num+1,sizeof(FklVMlib));
	FKL_ASSERT(libs);
	for(uint64_t i=1;base<end;base++,i++)
		fklInitVMlib(&libs[i],*base);

	FklVM* main_thread=fklCreateVM(main_proc,gc,lib_num,libs);
	ctx->reached_thread=main_thread;
	ctx->running=0;
	ctx->done=0;
	main_thread->dummy_ins_func=B_int3;
	gc->main_thread=main_thread;
	fklVMthreadStart(main_thread,&gc->q);
	setReachedThread(ctx,main_thread);
}

