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
		env->refcount++;
		DebugCtx* dctx=(DebugCtx*)ctx;
		fklPushPtrStack(env,&dctx->envs);
	}
}

static void B_int3(FKL_VM_INS_FUNC_ARGL);
static void B_int33(FKL_VM_INS_FUNC_ARGL);

static void B_int3(FKL_VM_INS_FUNC_ARGL)
{
	if(exe->is_single_thread)
	{
		FklInstruction* oins=&((BreakpointHashItem*)ins->ptr)->origin_ins;
		exe->ins_table[oins->op](exe,oins);
	}
	else
	{
		exe->ins_table[0]=B_int33;
		exe->top_frame->c.pc--;
		fklVMinterrupt(exe,createBreakpointWrapper(exe,ins->ptr));
	}
}

static void B_int33(FKL_VM_INS_FUNC_ARGL)
{
	exe->ins_table[0]=B_int3;
	FklInstruction* oins=&((BreakpointHashItem*)ins->ptr)->origin_ins;
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
	ctx->reached_thread=anotherVM;

	anotherVM->ins_table[0]=B_int3;

	gc->main_thread=anotherVM;
	fklVMpushInterruptHandler(gc,dbgInterruptHandler,ctx);
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
	phead=&ctx->reached_thread->obj_head;
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

static void dbg_extra_mark(FklVMgc* gc,void* arg)
{
	DebugCtx* ctx=(DebugCtx*)arg;
	FklVMvalue** base=(FklVMvalue**)ctx->extra_mark_value.base;
	FklVMvalue** const last=&base[ctx->extra_mark_value.top];
	for(;base<last;base++)
		fklVMgcToGray(*base,gc);
	for(FklVMvalue* head=ctx->code_objs;head;head=head->next)
	{
		head->mark=FKL_MARK_W;
		fklVMgcToGray(head,gc);
	}
}

FklVMvalue* getMainProc(DebugCtx* ctx)
{
	return ctx->extra_mark_value.base[0];
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
	fklVMpushExtraMarkFunc(ctx->gc,dbg_extra_mark,ctx);
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

	fklInitPtrStack(&ctx->reached_thread_frames,16,16);
	setReachedThread(ctx,ctx->reached_thread);
	init_source_codes(ctx);
	get_all_code_objs(ctx);
	push_extra_mark_value(ctx);
	initBreakpointTable(&ctx->breakpoints);
	const FklLineNumberTableItem* ln=getCurFrameLineNumber(ctx->reached_thread->top_frame);
	ctx->curline_str=getCurLineStr(ctx,ln->fid,ln->line);

	set_argv_with_list(ctx->gc,argv);
	init_cmd_read_ctx(&ctx->read_ctx);

	return ctx;
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
	if(exe->ins_table[0]==B_int33)
		exe->ins_table[0]=B_int3;
	else
		exe->ins_table[0]=B_int33;
}

BreakpointHashItem* putBreakpointWithFileAndLine(DebugCtx* ctx
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

	uint64_t scp=0;
	FklInstruction* ins=NULL;
	for(;line<=sc_item->lines.top;line++)
	{
		for(FklVMvalue* cur=ctx->code_objs
				;cur
				;cur=cur->next)
		{
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
		BreakpointHashKey key={.fid=fid,.line=line,.pc=scp};
		BreakpointHashItem t={.key=key,.num=0};
		BreakpointHashItem* item=fklGetOrPutHashItem(&t,&ctx->breakpoints);
		if(item->ctx)
			return item;
		else
		{
			ctx->breakpoint_num++;
			item->num=ctx->breakpoint_num;
			item->ctx=ctx;
			item->origin_ins=*ins;
			item->ins=ins;
			ins->op=0;
			ins->ptr=item;
			return item;
		}
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
	uint32_t prototype_id=FKL_VM_PROC(frame->c.proc)->protoId;
	uint32_t scope=getCurFrameLineNumber(frame)->scope;
	FklCodegenEnv* env=ctx->envs.base[prototype_id-1];
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
	FklFuncPrototype* pt=&ctx->reached_thread->pts->pts[prototype_id];
	FklSymbolDef* def=pt->refs;
	FklSymbolDef* const end=&def[pt->rcount];
	for(;def<end;def++)
		if(def->k.id==id)
			return *(FKL_VM_VAR_REF(proc->closure[def->idx])->ref);
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

static inline BreakpointHashItem* put_breakpoint_with_pc(DebugCtx* ctx
		,uint64_t pc
		,FklInstruction* ins
		,const FklLineNumberTableItem* ln)
{
	BreakpointHashKey key={.fid=ln->fid,.line=ln->line,.pc=pc};
	BreakpointHashItem t={.key=key,.num=0};
	BreakpointHashItem* item=fklGetOrPutHashItem(&t,&ctx->breakpoints);
	if(item->ctx)
		return item;
	else
	{
		ctx->breakpoint_num++;
		item->num=ctx->breakpoint_num;
		item->ctx=ctx;
		item->origin_ins=*ins;
		item->ins=ins;
		ins->op=0;
		ins->ptr=item;
		return item;
	}
}

BreakpointHashItem* putBreakpointForProcedure(DebugCtx* ctx,FklSid_t name_sid)
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
	if(ctx->reached_thread)
	{
		FklVM* vm=ctx->reached_thread;
		uint32_t top=ctx->reached_thread_frames.top;
		void** base=ctx->reached_thread_frames.base;
		for(uint32_t i=0;i<top;i++)
		{
			FklVMframe* cur=base[i];
			if(i==ctx->curframe_idx)
				fklPrintString(prefix,fp);
			else
				for(uint32_t i=0;i<prefix->size;i++)
					fputc(' ',fp);
			fklPrintFrame(cur,vm,fp);
		}
	}
}

FklVMframe* getCurrentFrame(DebugCtx* ctx)
{
	void** base=ctx->reached_thread_frames.base;
	FklVMframe* cur=base[ctx->curframe_idx];
	return cur;
}

void printCurFrame(DebugCtx* ctx,const FklString* prefix,FILE* fp)
{
	if(ctx->reached_thread)
	{
		FklVM* vm=ctx->reached_thread;
		FklVMframe* cur=getCurrentFrame(ctx);
		fklPrintString(prefix,fp);
		fklPrintFrame(cur,vm,fp);
	}
}

void setReachedThread(DebugCtx* ctx,FklVM* vm)
{
	ctx->reached_thread=vm;
	ctx->reached_thread_frames.top=0;
	ctx->curframe_idx=0;
	for(FklVMframe* f=vm->top_frame;f;f=f->prev)
		fklPushPtrStack(f,&ctx->reached_thread_frames);
}

