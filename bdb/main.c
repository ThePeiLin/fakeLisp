#include"bdb.h"
#include<fakeLisp/builtin.h>

static uv_once_t debug_ctx_inited=UV_ONCE_INIT;
uv_mutex_t alive_debug_lock;
static FklPtrStack alive_debug_ctxs;

typedef struct
{
	DebugCtx* ctx;
}DebugUdCtx;

FKL_VM_USER_DATA_DEFAULT_PRINT(debug_ctx_print,debug-ctx);

static void uninit_cmd_read_ctx(CmdReadCtx* ctx)
{
	fklUninitPtrStack(&ctx->stateStack);
	fklUninitPtrStack(&ctx->symbolStack);
	fklUninitUintStack(&ctx->lineStack);
	fklUninitStringBuffer(&ctx->buf);
	replxx_end(ctx->replxx);
}

static void uninit_all_alive_debug_ctx(void)
{
	uv_mutex_destroy(&alive_debug_lock);
	while(!fklIsPtrStackEmpty(&alive_debug_ctxs))
	{
		DebugCtx* ctx=fklPopPtrStack(&alive_debug_ctxs);
		uninit_cmd_read_ctx(&ctx->read_ctx);
	}
	fklUninitPtrStack(&alive_debug_ctxs);
#warning INCOMPLETE
}

static void init_alive_debug_ctx(void)
{
	atexit(uninit_all_alive_debug_ctx);
	uv_mutex_init(&alive_debug_lock);
	fklInitPtrStack(&alive_debug_ctxs,16,16);
}

static inline char* get_valid_file_name(const char* filename)
{
	if(fklIsAccessibleRegFile(filename))
	{
		if(fklIsScriptFile(filename))
			return fklCopyCstr(filename);
		return NULL;
	}
	else
	{
		char* r=NULL;
		FklStringBuffer main_script_buf;
		fklInitStringBuffer(&main_script_buf);

		fklStringBufferConcatWithCstr(&main_script_buf,filename);
		fklStringBufferConcatWithCstr(&main_script_buf,FKL_PATH_SEPARATOR_STR);
		fklStringBufferConcatWithCstr(&main_script_buf,"main.fkl");

		if(fklIsAccessibleRegFile(main_script_buf.buf))
			r=fklCopyCstr(main_script_buf.buf);
		fklUninitStringBuffer(&main_script_buf);
		return r;
	}
}

static inline void atomic_cmd_read_ctx(const CmdReadCtx* ctx,FklVMgc* gc)
{
	FklAnalysisSymbol** base=(FklAnalysisSymbol**)ctx->symbolStack.base;
	FklAnalysisSymbol** end=&base[ctx->symbolStack.top];
	for(;base<end;base++)
		fklVMgcToGray((*base)->ast,gc);
}

static void debug_ctx_atomic(const FklVMudata* ud,FklVMgc* gc)
{
	FKL_DECL_UD_DATA(debug_ctx,DebugUdCtx,ud);
	atomic_cmd_read_ctx(&debug_ctx->ctx->read_ctx,gc);
	markBreakpointCondExpObj(debug_ctx->ctx,gc);
}

static FklVMudMetaTable DebugCtxUdMetaTable=
{
	.size=sizeof(DebugUdCtx),
	.__prin1=debug_ctx_print,
	.__princ=debug_ctx_print,
	.__atomic=debug_ctx_atomic,
};

#define IS_DEBUG_CTX_UD(V) (FKL_IS_USERDATA(V)&&FKL_VM_UD(V)->t==&DebugCtxUdMetaTable)

#define RAISE_DBG_ERROR(WHO,ERR,EXE) abort()

static inline void push_debug_ctx(DebugCtx* ctx)
{
	uv_mutex_lock(&alive_debug_lock);
	fklPushPtrStack(ctx,&alive_debug_ctxs);
	uv_mutex_unlock(&alive_debug_lock);
}

static int bdb_make_debug_ctx(FKL_CPROC_ARGL)
{
	static const char Pname[]="bdb.make-debug-ctx";
	FKL_DECL_AND_CHECK_ARG2(filename_obj,argv_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(filename_obj,FKL_IS_STR,Pname,exe);
	{
		FklVMvalue* l=argv_obj;
		for(;FKL_IS_PAIR(l);l=FKL_VM_CDR(l))
		{
			FklVMvalue* cur=FKL_VM_CAR(l);
			FKL_CHECK_TYPE(cur,FKL_IS_STR,Pname,exe);
		}
		FKL_CHECK_TYPE(l,fklIsList,Pname,exe);
	}
	FklString* filename_str=FKL_VM_STR(filename_obj);
	char* valid_filename=get_valid_file_name(filename_str->str);
	if(!valid_filename)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR(Pname,filename_str->str,0,FKL_ERR_FILEFAILURE,exe);
	uint32_t rtp=exe->tp;
	FKL_VM_PUSH_VALUE(exe,filename_obj);
	FKL_VM_PUSH_VALUE(exe,argv_obj);
	fklUnlockThread(exe);
	DebugCtx* debug_ctx=createDebugCtx(exe,valid_filename,argv_obj);
	push_debug_ctx(debug_ctx);
	fklLockThread(exe);
	free(valid_filename);
	if(!debug_ctx)
		RAISE_DBG_ERROR(Pname,DBG_ERR_CODEGEN_FAILED,exe);
	FklVMvalue* ud=fklCreateVMvalueUdata(exe,&DebugCtxUdMetaTable,ctx->proc);
	FKL_DECL_VM_UD_DATA(debug_ud,DebugUdCtx,ud);
	debug_ud->ctx=debug_ctx;
	FKL_VM_SET_TP_AND_PUSH_VALUE(exe,rtp,ud);
	return 0;
}

static int bdb_debug_ctx_p(FKL_CPROC_ARGL)
{
	static const char Pname[]="bdb.debug-ctx?";
	FKL_DECL_AND_CHECK_ARG(obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,IS_DEBUG_CTX_UD(obj)
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_end_p(FKL_CPROC_ARGL)
{
	static const char Pname[]="bdb.debug-ctx-end?";
	FKL_DECL_AND_CHECK_ARG(obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,IS_DEBUG_CTX_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(debug_ud,DebugUdCtx,obj);
	FKL_VM_PUSH_VALUE(exe,debug_ud->ctx->end
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_exit(FKL_CPROC_ARGL)
{
	static const char Pname[]="bdb.debug-ctx-exit";
	FKL_DECL_AND_CHECK_ARG(obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,IS_DEBUG_CTX_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(debug_ud,DebugUdCtx,obj);
	debug_ud->ctx->end=1;
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static inline void debug_repl_parse_ctx_and_buf_reset(CmdReadCtx* cc,FklStringBuffer* s)
{
	cc->offset=0;
	fklStringBufferClear(s);
	s->buf[0]='\0';
	FklPtrStack* ss=&cc->symbolStack;
	while(!fklIsPtrStackEmpty(ss))
		free(fklPopPtrStack(ss));
	cc->stateStack.top=0;
	cc->lineStack.top=0;
	fklVMvaluePushState0ToStack(&cc->stateStack);
}

static inline const char* debug_ctx_replxx_input_string_buffer(Replxx* replxx
		,FklStringBuffer* buf
		,const char* prompt)
{
	const char* next=replxx_input(replxx,buf->index?"":prompt);
	if(next)
		fklStringBufferConcatWithCstr(buf,next);
	return next;
}

static inline FklVMvalue* debug_ctx_replxx_input(FklVM* exe
		,DebugCtx* dctx
		,const char* prompt
		,const char* func_name)
{
	CmdReadCtx* ctx=&dctx->read_ctx;
	FklGrammerMatchOuterCtx outerCtx=FKL_VMVALUE_PARSE_OUTER_CTX_INIT(exe);
	FklStringBuffer* s=&ctx->buf;
	int err=0;
	int is_eof=0;
	size_t errLine=0;
	for(;;)
	{
		fklUnlockThread(exe);
		is_eof=debug_ctx_replxx_input_string_buffer(ctx->replxx,s,prompt)==NULL;
		fklLockThread(exe);
		size_t restLen=fklStringBufferLen(s)-ctx->offset;

		FklVMvalue* ast=fklDefaultParseForCharBuf(fklStringBufferBody(s)+ctx->offset
				,restLen
				,&restLen
				,&outerCtx
				,&err
				,&errLine
				,&ctx->symbolStack
				,&ctx->lineStack
				,&ctx->stateStack);

		ctx->offset=fklStringBufferLen(s)-restLen;

		if(!restLen&&ctx->symbolStack.top==0&&is_eof)
		{
			dctx->end=1;
			return fklCreateVMvalueEof(exe);
		}
		else if((err==FKL_PARSE_WAITING_FOR_MORE
					||(err==FKL_PARSE_TERMINAL_MATCH_FAILED
						&&!restLen))
				&&is_eof)
		{
			debug_repl_parse_ctx_and_buf_reset(ctx,s);
			FKL_RAISE_BUILTIN_ERROR_CSTR(func_name,FKL_ERR_UNEXPECTED_EOF,exe);
		}
		else if(err==FKL_PARSE_TERMINAL_MATCH_FAILED&&restLen)
		{
			debug_repl_parse_ctx_and_buf_reset(ctx,s);
			FKL_RAISE_BUILTIN_ERROR_CSTR(func_name,FKL_ERR_INVALIDEXPR,exe);
		}
		else if(err==FKL_PARSE_REDUCE_FAILED)
		{
			debug_repl_parse_ctx_and_buf_reset(ctx,s);
			FKL_RAISE_BUILTIN_ERROR_CSTR(func_name,FKL_ERR_INVALIDEXPR,exe);
		}
		else if(ast)
		{
			if(restLen)
			{
				size_t idx=fklStringBufferLen(s)-restLen;
				replxx_set_preload_buffer(ctx->replxx,&s->buf[idx]);
				s->buf[idx]='\0';
			}
			replxx_history_add(ctx->replxx,s->buf);
			debug_repl_parse_ctx_and_buf_reset(ctx,s);
			return ast;
		}
	}
}

static int bdb_debug_ctx_repl(FKL_CPROC_ARGL)
{
	static const char Pname[]="bdb.debug-ctx-repl";
	FKL_DECL_AND_CHECK_ARG2(debug_ctx_obj,prompt_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,Pname,exe);
	FKL_CHECK_TYPE(prompt_obj,FKL_IS_STR,Pname,exe);

	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);
	uint32_t rtp=exe->tp;
	FKL_VM_PUSH_VALUE(exe,debug_ctx_obj);
	FKL_VM_PUSH_VALUE(exe,prompt_obj);
	FklVMvalue* cmd=debug_ctx_replxx_input(exe,debug_ctx_ud->ctx,FKL_VM_STR(prompt_obj)->str,Pname);
	FKL_VM_SET_TP_AND_PUSH_VALUE(exe,rtp,cmd);
	return 0;
}

static int bdb_debug_ctx_get_curline(FKL_CPROC_ARGL)
{
	static const char Pname[]="bdb.debug-ctx-repl";
	FKL_DECL_AND_CHECK_ARG(debug_ctx_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,Pname,exe);

	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);
	DebugCtx* dctx=debug_ctx_ud->ctx;
	FklVM* cur_thread=dctx->reached_thread;
	if(cur_thread)
	{
		FklVMframe* frame=cur_thread->top_frame;
		for(;frame&&frame->type==FKL_FRAME_OTHEROBJ;frame=frame->prev);
		if(frame)
		{
			const FklLineNumberTableItem* ln=getCurFrameLineNumber(frame);
			const FklString* line_str=getCurLineStr(dctx,ln->fid,ln->line);
			FklVMvalue* line_str_value=fklCreateVMvalueStr(exe,fklCopyString(line_str));
			const FklString* file_str=fklGetSymbolWithId(ln->fid,dctx->st)->symbol;
			FklVMvalue* file_str_value=fklCreateVMvalueStr(exe,fklCopyString(file_str));

			FklVMvalue* line_num_value=FKL_MAKE_VM_FIX(ln->line);
			FklVMvalue* r=fklCreateVMvalueVec3(exe
					,file_str_value
					,line_num_value
					,line_str_value);

			FKL_VM_PUSH_VALUE(exe,r);
		}
		else
			FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);

	}
	else
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_continue(FKL_CPROC_ARGL)
{
	static const char Pname[]="bdb.debug-ctx-continue";
	FKL_DECL_AND_CHECK_ARG(debug_ctx_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,Pname,exe);

	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);
	DebugCtx* dctx=debug_ctx_ud->ctx;
	uint32_t rtp=exe->tp;
	FKL_VM_PUSH_VALUE(exe,debug_ctx_obj);
	fklUnlockThread(exe);
	dctx->reached_breakpoint=NULL;
	dctx->reached_thread=NULL;
	if(setjmp(dctx->jmpb)==DBG_INTERRUPTED)
	{
		dctx->interrupted_by_debugger=1;
		if(dctx->reached_breakpoint)
			unsetStepping(dctx);
	}
	else
	{
		if(dctx->interrupted_by_debugger)
		{
			dctx->reached_breakpoint=NULL;
			fklVMreleaseWq(dctx->gc);
			fklVMcontinueTheWorld(dctx->gc);
		}
		fklVMtrappingIdleLoop(dctx->gc);
		dctx->interrupted_by_debugger=0;
	}
	fklLockThread(exe);
	FKL_VM_SET_TP_AND_PUSH_VALUE(exe
			,rtp
			,dctx->interrupted_by_debugger
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_set_break(FKL_CPROC_ARGL)
{
	static const char Pname[]="bdb.debug-ctx-set-break";
	FKL_DECL_AND_CHECK_ARG(debug_ctx_obj,exe,Pname);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);
	DebugCtx* dctx=debug_ctx_ud->ctx;

	FklSid_t fid=0;
	uint32_t line=0;
	PutBreakpointErrorType err=0;
	BreakpointHashItem* item=NULL;

	FKL_DECL_AND_CHECK_ARG(file_line_sym_obj,exe,Pname);

	if(FKL_IS_SYM(file_line_sym_obj))
	{
		FklSid_t id=fklAddSymbol(fklVMgetSymbolWithId(exe->gc,FKL_GET_SYM(file_line_sym_obj))->symbol,dctx->st)->id;
		item=putBreakpointForProcedure(dctx,id);
		if(item)
			goto done;
		else
		{
			err=PUT_BP_NOT_A_PROC;
			goto error;
		}
	}
	else if(fklIsVMint(file_line_sym_obj))
	{
		if(fklIsVMnumberLt0(file_line_sym_obj))
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
		line=fklGetUint(file_line_sym_obj);
		fid=dctx->curline_file;
	}
	else if(FKL_IS_STR(file_line_sym_obj))
	{
		FKL_DECL_AND_CHECK_ARG(line_obj,exe,Pname);
		FKL_CHECK_TYPE(line_obj,fklIsVMint,Pname,exe);
		if(fklIsVMnumberLt0(line_obj))
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
		line=fklGetUint(line_obj);
		FklString* str=FKL_VM_STR(file_line_sym_obj);
		if(fklIsAccessibleRegFile(str->str))
		{
			char* rp=fklRealpath(str->str);
			fid=fklAddSymbolCstr(rp,dctx->st)->id;
			free(rp);
		}
		else
		{
			char* filename_with_dir=fklStrCat(fklCopyCstr(dctx->outer_ctx.main_file_real_path_dir),str->str);
			if(fklIsAccessibleRegFile(str->str))
				fid=fklAddSymbolCstr(filename_with_dir,dctx->st)->id;
			free(filename_with_dir);
		}
	}
	else 
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);

	item=putBreakpointWithFileAndLine(dctx,fid,line,&err);
	FklVMvalue* cond_exp_obj=NULL;

done:

	cond_exp_obj=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	if(item)
	{
		if(cond_exp_obj)
		{
			FklNastNode* expression=fklCreateNastNodeFromVMvalue(cond_exp_obj,dctx->curline,NULL,exe->gc);
			if(!expression)
				FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_CIR_REF,exe);
			fklVMacquireSt(exe->gc);
			fklRecomputeSidForNastNode(expression,exe->gc->st,dctx->st);
			fklVMreleaseSt(exe->gc);
			item->cond_exp=expression;
		}
		item->cond_exp_obj=cond_exp_obj;

		FklVMvalue* filename=fklCreateVMvalueStr(exe,fklCopyString(fklGetSymbolWithId(item->key.fid,dctx->st)->symbol));
		FklVMvalue* line=FKL_MAKE_VM_FIX(item->key.line);
		FklVMvalue* num=FKL_MAKE_VM_FIX(item->num);
		FklVMvalue* r=NULL;
		if(cond_exp_obj)
			r=fklCreateVMvalueVec4(exe
					,num
					,filename
					,line
					,cond_exp_obj);
		else
			r=fklCreateVMvalueVec3(exe
					,num
					,filename
					,line);


		FKL_VM_PUSH_VALUE(exe,r);
	}
	else
	{
error:
		FKL_VM_PUSH_VALUE(exe
				,fklCreateVMvalueStr(exe
					,fklCreateStringFromCstr(getPutBreakpointErrorInfo(err))));
	}
	return 0;
}

static int bdb_debug_ctx_list_break(FKL_CPROC_ARGL)
{
	static const char Pname[]="bdb.debug-ctx-list-break";
	FKL_DECL_AND_CHECK_ARG(debug_ctx_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,Pname,exe);

	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);
	DebugCtx* dctx=debug_ctx_ud->ctx;

	FklVMvalue* r=FKL_VM_NIL;
	FklVMvalue** pr=&r;
	for(FklHashTableItem* list=dctx->breakpoints.first
			;list
			;list=list->next)
	{
		BreakpointHashItem* item=(BreakpointHashItem*)list->data;
		FklVMvalue* filename=fklCreateVMvalueStr(exe,fklCopyString(fklGetSymbolWithId(item->key.fid,dctx->st)->symbol));
		FklVMvalue* line=FKL_MAKE_VM_FIX(item->key.line);
		FklVMvalue* num=FKL_MAKE_VM_FIX(item->num);

		FklVMvalue* vec_val=NULL;
		if(item->cond_exp_obj)
			vec_val=fklCreateVMvalueVec4(exe
					,num
					,filename
					,line
					,item->cond_exp_obj);
		else
			vec_val=fklCreateVMvalueVec3(exe
					,num
					,filename
					,line);

		*pr=fklCreateVMvaluePairWithCar(exe,vec_val);
		pr=&FKL_VM_CDR(*pr);
	}
	FKL_VM_PUSH_VALUE(exe,r);

	return 0;
}

static int bdb_debug_ctx_del_break(FKL_CPROC_ARGL)
{
	static const char Pname[]="bdb.debug-ctx-del-break";
	FKL_DECL_AND_CHECK_ARG2(debug_ctx_obj,bp_num_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,Pname,exe);
	FKL_CHECK_TYPE(bp_num_obj,FKL_IS_FIX,Pname,exe);

	if(fklIsVMnumberLt0(bp_num_obj))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);

	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);
	DebugCtx* dctx=debug_ctx_ud->ctx;

	uint64_t num=fklGetUint(bp_num_obj);
	FklVMvalue* r=NULL;
	BreakpointHashItem* item=delBreakpoint(dctx,num);
	if(item)
	{
		FklVMvalue* filename=fklCreateVMvalueStr(exe,fklCopyString(fklGetSymbolWithId(item->key.fid,dctx->st)->symbol));
		FklVMvalue* line=FKL_MAKE_VM_FIX(item->key.line);
		FklVMvalue* num=FKL_MAKE_VM_FIX(item->num);

		FklVMvalue* vec_val=NULL;
		if(item->cond_exp_obj)
			vec_val=fklCreateVMvalueVec4(exe
					,num
					,filename
					,line
					,item->cond_exp_obj);
		else
			vec_val=fklCreateVMvalueVec3(exe
					,num
					,filename
					,line);

		r=vec_val;
		FKL_VM_PUSH_VALUE(exe,r);
	}
	else
		FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(dctx->breakpoint_num));
	return 0;
}

static int bdb_debug_ctx_set_list_src(FKL_CPROC_ARGL)
{
	static const char Pname[]="bdb.debug-ctx-set-list-src";
	FKL_DECL_AND_CHECK_ARG2(debug_ctx_obj,line_num_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,Pname,exe);

	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);
	DebugCtx* dctx=debug_ctx_ud->ctx;

	FKL_CHECK_TYPE(line_num_obj,FKL_IS_FIX,Pname,exe);
	int64_t line_num=FKL_GET_FIX(line_num_obj);
	if(line_num>0&&line_num<=dctx->curfile_lines->top)
		dctx->curlist_line=line_num;
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_list_src(FKL_CPROC_ARGL)
{
	static const char Pname[]="bdb.debug-ctx-list-src";
	FKL_DECL_AND_CHECK_ARG(debug_ctx_obj,exe,Pname);
	FklVMvalue* line_num_obj=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,Pname,exe);

	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);
	DebugCtx* dctx=debug_ctx_ud->ctx;

	if(line_num_obj)
	{
		FKL_CHECK_TYPE(line_num_obj,FKL_IS_FIX,Pname,exe);
		int64_t line_num=FKL_GET_FIX(line_num_obj);
		if(line_num<0||line_num>=dctx->curfile_lines->top)
			FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
		else
		{
			uint32_t curline_num=line_num;
			const FklString* line_str=dctx->curfile_lines->base[curline_num-1];

			FklVMvalue* num_val=FKL_MAKE_VM_FIX(curline_num);
			FklVMvalue* is_cur_line=curline_num==dctx->curline?FKL_VM_TRUE:FKL_VM_NIL;
			FklVMvalue* str_val=fklCreateVMvalueStr(exe,fklCopyString(line_str));

			FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueVec3(exe
						,num_val
						,is_cur_line
						,str_val));
		}
	}
	else if(dctx->curlist_line<=dctx->curfile_lines->top)
	{
		uint32_t curline_num=dctx->curlist_line;
		const FklString* line_str=dctx->curfile_lines->base[curline_num-1];

		FklVMvalue* num_val=FKL_MAKE_VM_FIX(curline_num);
		FklVMvalue* is_cur_line=curline_num==dctx->curline?FKL_VM_TRUE:FKL_VM_NIL;
		FklVMvalue* str_val=fklCreateVMvalueStr(exe,fklCopyString(line_str));

		FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueVec3(exe
					,num_val
					,is_cur_line
					,str_val));
	}
	else
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_set_step_into(FKL_CPROC_ARGL)
{
	static const char Pname[]="bdb.debug-ctx-set-step-into";
	FKL_DECL_AND_CHECK_ARG(debug_ctx_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);

	setStepInto(debug_ctx_ud->ctx);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_set_step_over(FKL_CPROC_ARGL)
{
	static const char Pname[]="bdb.debug-ctx-set-step-over";
	FKL_DECL_AND_CHECK_ARG(debug_ctx_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);

	setStepOver(debug_ctx_ud->ctx);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_set_step_out(FKL_CPROC_ARGL)
{
	static const char Pname[]="bdb.debug-ctx-set-step-out";
	FKL_DECL_AND_CHECK_ARG(debug_ctx_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);

	setStepOut(debug_ctx_ud->ctx);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_set_until(FKL_CPROC_ARGL)
{
	static const char Pname[]="bdb.debug-ctx-set-until";
	FKL_DECL_AND_CHECK_ARG(debug_ctx_obj,exe,Pname);
	FklVMvalue* lineno_obj=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);

	DebugCtx* dctx=debug_ctx_ud->ctx;
	if(lineno_obj==NULL)
		setStepOver(dctx);
	else
	{
		FKL_CHECK_TYPE(lineno_obj,FKL_IS_FIX,Pname,exe);
		int64_t line=FKL_GET_FIX(lineno_obj);
		if(line<((int64_t)dctx->curline))
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALID_VALUE,exe);
		setStepUntil(dctx,line);
	}
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_eval(FKL_CPROC_ARGL)
{
	static const char Pname[]="bdb.debug-ctx-eval";
	FKL_DECL_AND_CHECK_ARG2(debug_ctx_obj,expression_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);

	DebugCtx* dctx=debug_ctx_ud->ctx;
	if(dctx->reached_thread_frames.top)
	{
		FklVMframe* cur_frame=getCurrentFrame(dctx);
		for(;cur_frame&&cur_frame->type==FKL_FRAME_OTHEROBJ;cur_frame=cur_frame->prev);

		if(cur_frame)
		{
			FklNastNode* expression=fklCreateNastNodeFromVMvalue(expression_obj,dctx->curline,NULL,exe->gc);
			if(!expression)
				FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_CIR_REF,exe);
			fklVMacquireSt(exe->gc);
			fklRecomputeSidForNastNode(expression,exe->gc->st,dctx->st);
			fklVMreleaseSt(exe->gc);
			FklVMvalue* proc=compileEvalExpression(dctx
					,dctx->reached_thread
					,expression
					,cur_frame);
			if(proc)
			{
				FklVMvalue* value=callEvalProc(dctx
						,dctx->reached_thread
						,proc
						,cur_frame);
				if(value)
				{
					fputs(";=> ",stdout);
					fklPrin1VMvalue(value,stdout,dctx->gc);
					fputc('\n',stdout);
				}
			}
		}
		else
			printThreadCantEvaluate(dctx,stdout);
	}
	else
		printThreadAlreadyExited(dctx,stdout);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_back_trace(FKL_CPROC_ARGL)
{
	static const char Pname[]="bdb.debug-ctx-back-trace";
	FKL_DECL_AND_CHECK_ARG2(obj,prefix_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,IS_DEBUG_CTX_UD,Pname,exe);
	FKL_CHECK_TYPE(prefix_obj,FKL_IS_STR,Pname,exe);
	FKL_DECL_VM_UD_DATA(debug_ud,DebugUdCtx,obj);
	printBacktrace(debug_ud->ctx,FKL_VM_STR(prefix_obj),stdout);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_print_cur_frame(FKL_CPROC_ARGL)
{
	static const char Pname[]="bdb.debug-ctx-print-cur-frame";
	FKL_DECL_AND_CHECK_ARG2(obj,prefix_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,IS_DEBUG_CTX_UD,Pname,exe);
	FKL_CHECK_TYPE(prefix_obj,FKL_IS_STR,Pname,exe);
	FKL_DECL_VM_UD_DATA(debug_ud,DebugUdCtx,obj);
	printCurFrame(debug_ud->ctx,FKL_VM_STR(prefix_obj),stdout);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_up(FKL_CPROC_ARGL)
{
	static const char Pname[]="bdb.debug-ctx-up";
	FKL_DECL_AND_CHECK_ARG(obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,IS_DEBUG_CTX_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(debug_ud,DebugUdCtx,obj);

	DebugCtx* dctx=debug_ud->ctx;
	if(dctx->reached_thread
			&&dctx->curframe_idx<dctx->reached_thread_frames.top)
	{
		dctx->curframe_idx++;
		FKL_VM_PUSH_VALUE(exe,FKL_VM_TRUE);
	}
	else
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_down(FKL_CPROC_ARGL)
{
	static const char Pname[]="bdb.debug-ctx-down";
	FKL_DECL_AND_CHECK_ARG(obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,IS_DEBUG_CTX_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(debug_ud,DebugUdCtx,obj);

	DebugCtx* dctx=debug_ud->ctx;
	if(dctx->reached_thread
			&&dctx->curframe_idx>1)
	{
		dctx->curframe_idx--;
		FKL_VM_PUSH_VALUE(exe,FKL_VM_TRUE);
	}
	else
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_list_thread(FKL_CPROC_ARGL)
{
	static const char Pname[]="bdb.debug-ctx-list-thread";
	FKL_DECL_AND_CHECK_ARG2(obj,prefix_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,IS_DEBUG_CTX_UD,Pname,exe);
	FKL_CHECK_TYPE(prefix_obj,FKL_IS_STR,Pname,exe);
	FKL_DECL_VM_UD_DATA(debug_ud,DebugUdCtx,obj);

	DebugCtx* dctx=debug_ud->ctx;
	listThreads(dctx,FKL_VM_STR(prefix_obj),stdout);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_switch_thread(FKL_CPROC_ARGL)
{
	static const char Pname[]="bdb.debug-ctx-list-thread";
	FKL_DECL_AND_CHECK_ARG2(obj,id_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,IS_DEBUG_CTX_UD,Pname,exe);
	FKL_CHECK_TYPE(id_obj,FKL_IS_FIX,Pname,exe);
	FKL_DECL_VM_UD_DATA(debug_ud,DebugUdCtx,obj);

	DebugCtx* dctx=debug_ud->ctx;
	int64_t id=FKL_GET_FIX(id_obj);
	if(id>0&&id<=(int64_t)dctx->threads.top)
	{
		switchCurThread(dctx,FKL_GET_FIX(id_obj));
		FKL_VM_PUSH_VALUE(exe,FKL_VM_TRUE);
	}
	else
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

struct SymFunc
{
	const char* sym;
	FklVMcFunc f;
}exports_and_func[]=
{
	{"debug-ctx?",                bdb_debug_ctx_p,               },
	{"make-debug-ctx",            bdb_make_debug_ctx,            },
	{"debug-ctx-repl",            bdb_debug_ctx_repl,            },
	{"debug-ctx-get-curline",     bdb_debug_ctx_get_curline,     },
	{"debug-ctx-list-src",        bdb_debug_ctx_list_src,        },
	{"debug-ctx-set-list-src",    bdb_debug_ctx_set_list_src,    },

	{"debug-ctx-del-break",       bdb_debug_ctx_del_break,       },
	{"debug-ctx-set-break",       bdb_debug_ctx_set_break,       },
	{"debug-ctx-list-break",      bdb_debug_ctx_list_break,      },

	{"debug-ctx-set-step-over",   bdb_debug_ctx_set_step_over,   },
	{"debug-ctx-set-step-into",   bdb_debug_ctx_set_step_into,   },
	{"debug-ctx-set-step-out",    bdb_debug_ctx_set_step_out,    },
	{"debug-ctx-set-until",       bdb_debug_ctx_set_until,       },

	{"debug-ctx-continue",        bdb_debug_ctx_continue,        },
	{"debug-ctx-end?",            bdb_debug_ctx_end_p,           },
	{"debug-ctx-exit",            bdb_debug_ctx_exit,            },
	{"debug-ctx-eval",            bdb_debug_ctx_eval,            },

	{"debug-ctx-print-cur-frame", bdb_debug_ctx_print_cur_frame, },
	{"debug-ctx-back-trace",      bdb_debug_ctx_back_trace,      },
	{"debug-ctx-up",              bdb_debug_ctx_up,              },
	{"debug-ctx-down",            bdb_debug_ctx_down,            },

	{"debug-ctx-list-thread",     bdb_debug_ctx_list_thread,     },
	{"debug-ctx-switch-thread",   bdb_debug_ctx_switch_thread,   },
};

static const size_t EXPORT_NUM=sizeof(exports_and_func)/sizeof(struct SymFunc);

FKL_DLL_EXPORT void _fklExportSymbolInit(FKL_CODEGEN_DLL_LIB_INIT_EXPORT_FUNC_ARGS)
{
	*num=EXPORT_NUM;
	FklSid_t* symbols=(FklSid_t*)malloc(sizeof(FklSid_t)*EXPORT_NUM);
	FKL_ASSERT(symbols);
	for(size_t i=0;i<EXPORT_NUM;i++)
		symbols[i]=fklAddSymbolCstr(exports_and_func[i].sym,st)->id;
	*exports=symbols;
}

FKL_DLL_EXPORT FklVMvalue** _fklImportInit(FKL_IMPORT_DLL_INIT_FUNC_ARGS)
{
	uv_once(&debug_ctx_inited,init_alive_debug_ctx);
	*count=EXPORT_NUM;
	FklVMvalue** loc=(FklVMvalue**)malloc(sizeof(FklVMvalue*)*EXPORT_NUM);
	FKL_ASSERT(loc);
	for(size_t i=0;i<EXPORT_NUM;i++)
	{
		FklSid_t id=fklVMaddSymbolCstr(exe->gc,exports_and_func[i].sym)->id;
		FklVMcFunc func=exports_and_func[i].f;
		FklVMvalue* dlproc=fklCreateVMvalueCproc(exe,func,dll,NULL,id);
		loc[i]=dlproc;
	}
	return loc;
}
