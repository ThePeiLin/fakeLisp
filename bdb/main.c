#include"bdb.h"
#include<fakeLisp/builtin.h>

typedef struct
{
	DebugCtx* ctx;
}DebugUdCtx;

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(debug_ctx_as_print,debug-ctx);

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

static void debug_ctx_atomic(const FklVMud* ud,FklVMgc* gc)
{
	FKL_DECL_UD_DATA(debug_ctx,DebugUdCtx,ud);
	atomic_cmd_read_ctx(&debug_ctx->ctx->read_ctx,gc);
	markBreakpointCondExpObj(debug_ctx->ctx,gc);
}

static void debug_ctx_finalizer(FklVMud* data)
{
	FKL_DECL_UD_DATA(ud_ctx,DebugUdCtx,data);
	DebugCtx* dctx=ud_ctx->ctx;
	destroyDebugCtx(dctx);
}

static FklVMudMetaTable DebugCtxUdMetaTable=
{
	.size=sizeof(DebugUdCtx),
	.__as_prin1=debug_ctx_as_print,
	.__as_princ=debug_ctx_as_print,
	.__atomic=debug_ctx_atomic,
	.__finalizer=debug_ctx_finalizer,
};

#define IS_DEBUG_CTX_UD(V) (FKL_IS_USERDATA(V)&&FKL_VM_UD(V)->t==&DebugCtxUdMetaTable)

static void debug_ctx_atexit_mark(void* arg,FklVMgc* gc)
{
	fklVMgcToGray(arg,gc);
}

static void debug_ctx_atexit_func(FklVM* vm,void* arg)
{
	FklVMvalue* v=arg;
	FKL_DECL_VM_UD_DATA(debug_ud,DebugUdCtx,v);
	exitDebugCtx(debug_ud->ctx);
}

static void insert_debug_ctx_atexit(FklVM* vm,FklVMvalue* v)
{
	fklVMatExit(vm
			,debug_ctx_atexit_func
			,debug_ctx_atexit_mark
			,NULL
			,v);
}

static int bdb_make_debug_ctx(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(filename_obj,argv_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(filename_obj,FKL_IS_STR,exe);
	{
		FklVMvalue* l=argv_obj;
		for(;FKL_IS_PAIR(l);l=FKL_VM_CDR(l))
		{
			FklVMvalue* cur=FKL_VM_CAR(l);
			FKL_CHECK_TYPE(cur,FKL_IS_STR,exe);
		}
		FKL_CHECK_TYPE(l,fklIsList,exe);
	}
	FklString* filename_str=FKL_VM_STR(filename_obj);
	char* valid_filename=get_valid_file_name(filename_str->str);
	if(!valid_filename)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR(filename_str->str,0,FKL_ERR_FILEFAILURE,exe);
	uint32_t rtp=exe->tp;
	FKL_VM_PUSH_VALUE(exe,filename_obj);
	FKL_VM_PUSH_VALUE(exe,argv_obj);
	fklUnlockThread(exe);
	DebugCtx* debug_ctx=createDebugCtx(exe,valid_filename,argv_obj);
	fklLockThread(exe);
	free(valid_filename);
	if(!debug_ctx)
		FKL_RAISE_BUILTIN_ERROR_CSTR(FKL_ERR_INVALID_VALUE,exe);
	FklVMvalue* ud=fklCreateVMvalueUd(exe,&DebugCtxUdMetaTable,ctx->proc);
	FKL_DECL_VM_UD_DATA(debug_ud,DebugUdCtx,ud);
	debug_ud->ctx=debug_ctx;
	insert_debug_ctx_atexit(exe,ud);
	FKL_VM_SET_TP_AND_PUSH_VALUE(exe,rtp,ud);
	return 0;
}

static int bdb_debug_ctx_p(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_VM_PUSH_VALUE(exe,IS_DEBUG_CTX_UD(obj)
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_exit_p(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,IS_DEBUG_CTX_UD,exe);
	FKL_DECL_VM_UD_DATA(debug_ud,DebugUdCtx,obj);
	FKL_VM_PUSH_VALUE(exe,debug_ud->ctx->exit
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_done_p(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,IS_DEBUG_CTX_UD,exe);
	FKL_DECL_VM_UD_DATA(debug_ud,DebugUdCtx,obj);
	FKL_VM_PUSH_VALUE(exe,debug_ud->ctx->done
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_exit(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,IS_DEBUG_CTX_UD,exe);
	FKL_DECL_VM_UD_DATA(debug_ud,DebugUdCtx,obj);
	debug_ud->ctx->exit=1;
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
	else
		fputc('\n',stdout);
	return next;
}

static inline FklVMvalue* debug_ctx_replxx_input(FklVM* exe
		,DebugCtx* dctx
		,const char* prompt)
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
			dctx->exit=1;
			return fklCreateVMvalueEof(exe);
		}
		else if((err==FKL_PARSE_WAITING_FOR_MORE
					||(err==FKL_PARSE_TERMINAL_MATCH_FAILED
						&&!restLen))
				&&is_eof)
		{
			debug_repl_parse_ctx_and_buf_reset(ctx,s);
			FKL_RAISE_BUILTIN_ERROR_CSTR(FKL_ERR_UNEXPECTED_EOF,exe);
		}
		else if(err==FKL_PARSE_TERMINAL_MATCH_FAILED&&restLen)
		{
			debug_repl_parse_ctx_and_buf_reset(ctx,s);
			FKL_RAISE_BUILTIN_ERROR_CSTR(FKL_ERR_INVALIDEXPR,exe);
		}
		else if(err==FKL_PARSE_REDUCE_FAILED)
		{
			debug_repl_parse_ctx_and_buf_reset(ctx,s);
			FKL_RAISE_BUILTIN_ERROR_CSTR(FKL_ERR_INVALIDEXPR,exe);
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
	FKL_DECL_AND_CHECK_ARG2(debug_ctx_obj,prompt_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,exe);
	FKL_CHECK_TYPE(prompt_obj,FKL_IS_STR,exe);

	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);
	uint32_t rtp=exe->tp;
	FKL_VM_PUSH_VALUE(exe,debug_ctx_obj);
	FKL_VM_PUSH_VALUE(exe,prompt_obj);
	FklVMvalue* cmd=debug_ctx_replxx_input(exe,debug_ctx_ud->ctx,FKL_VM_STR(prompt_obj)->str);
	FKL_VM_SET_TP_AND_PUSH_VALUE(exe,rtp,cmd);
	return 0;
}

static int bdb_debug_ctx_get_curline(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(debug_ctx_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,exe);

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
			if(line_str)
			{
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

	}
	else
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static inline FklVMvalue* create_breakpoint_vec(FklVM* exe
		,DebugCtx* dctx
		,const Breakpoint* item)
{
	FklVMvalue* filename=fklCreateVMvalueStr(exe,fklCopyString(fklGetSymbolWithId(item->fid,dctx->st)->symbol));
	FklVMvalue* line=FKL_MAKE_VM_FIX(item->line);
	FklVMvalue* num=FKL_MAKE_VM_FIX(item->num);

	FklVMvalue* r=fklCreateVMvalueVec6(exe
			,num
			,filename
			,line
			,item->cond_exp_obj?fklCreateVMvalueBox(exe,item->cond_exp_obj):FKL_VM_NIL
			,fklMakeVMuint(item->count,exe)
			,fklCreateVMvaluePair(exe,item->is_disabled?FKL_VM_NIL:FKL_VM_TRUE,item->is_temporary?FKL_VM_TRUE:FKL_VM_NIL));
	return r;
}

static inline int debug_restart(DebugCtx* dctx,FklVM* exe)
{
	if(dctx->running)
	{
		if(dctx->reached_thread)
		{
			fklUnlockThread(exe);
			setAllThreadReadyToExit(dctx->reached_thread);
			waitAllThreadExit(dctx->reached_thread);
			fklLockThread(exe);
		}
		restartDebugging(dctx);
		return 1;
	}
	return 0;
}

static int bdb_debug_ctx_continue(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(debug_ctx_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,exe);

	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);
	DebugCtx* dctx=debug_ctx_ud->ctx;
	uint32_t rtp=exe->tp;
	FKL_VM_PUSH_VALUE(exe,debug_ctx_obj);
	if(dctx->done)
	{
		debug_restart(dctx,exe);
		FKL_VM_SET_TP_AND_PUSH_VALUE(exe,rtp,FKL_VM_NIL);
		return 0;
	}
	fklUnlockThread(exe);
	dctx->reached_breakpoint=NULL;
	dctx->reached_thread=NULL;
	int r=setjmp(dctx->jmpb);
	if(r==DBG_INTERRUPTED)
	{
		dctx->done=0;
		if(dctx->reached_breakpoint)
			unsetStepping(dctx);
	}
	else if(r==DBG_ERROR_OCCUR)
		dctx->done=1;
	else
	{
		if(dctx->running)
		{
			dctx->reached_breakpoint=NULL;
			fklVMreleaseWq(dctx->gc);
			fklVMcontinueTheWorld(dctx->gc);
		}
		dctx->running=1;
		fklVMtrappingIdleLoop(dctx->gc);
		dctx->done=1;
	}
	fklLockThread(exe);
	FKL_VM_SET_TP_AND_PUSH_VALUE(exe
			,rtp
			,dctx->reached_breakpoint
			?create_breakpoint_vec(exe,dctx,dctx->reached_breakpoint)
			:FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_restart(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(debug_ctx_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,exe);

	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);
	DebugCtx* dctx=debug_ctx_ud->ctx;
	uint32_t rtp=exe->tp;
	FKL_VM_PUSH_VALUE(exe,debug_ctx_obj);
	if(debug_restart(dctx,exe))
		FKL_VM_SET_TP_AND_PUSH_VALUE(exe,rtp,FKL_VM_TRUE);
	else
		FKL_VM_SET_TP_AND_PUSH_VALUE(exe,rtp,FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_set_break(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(debug_ctx_obj,exe);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,exe);
	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);
	DebugCtx* dctx=debug_ctx_ud->ctx;

	FklSid_t fid=0;
	uint32_t line=0;
	PutBreakpointErrorType err=0;
	Breakpoint* item=NULL;

	FKL_DECL_AND_CHECK_ARG(file_line_sym_obj,exe);

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
			FKL_RAISE_BUILTIN_ERROR_CSTR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
		line=fklGetUint(file_line_sym_obj);
		fid=dctx->curline_file;
	}
	else if(FKL_IS_STR(file_line_sym_obj))
	{
		FKL_DECL_AND_CHECK_ARG(line_obj,exe);
		FKL_CHECK_TYPE(line_obj,fklIsVMint,exe);
		if(fklIsVMnumberLt0(line_obj))
			FKL_RAISE_BUILTIN_ERROR_CSTR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);

	item=putBreakpointWithFileAndLine(dctx,fid,line,&err);
	FklVMvalue* cond_exp_obj=NULL;

done:

	cond_exp_obj=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	if(item)
	{
		if(cond_exp_obj)
		{
			FklNastNode* expression=fklCreateNastNodeFromVMvalue(cond_exp_obj,dctx->curline,NULL,exe->gc);
			if(!expression)
				FKL_RAISE_BUILTIN_ERROR_CSTR(FKL_ERR_CIR_REF,exe);
			fklVMacquireSt(exe->gc);
			fklRecomputeSidForNastNode(expression,exe->gc->st,dctx->st);
			fklVMreleaseSt(exe->gc);
			item->cond_exp=expression;
		}
		item->cond_exp_obj=cond_exp_obj;

		FKL_VM_PUSH_VALUE(exe,create_breakpoint_vec(exe,dctx,item));
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

static int bdb_debug_ctx_set_tbreak(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(debug_ctx_obj,exe);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,exe);
	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);
	DebugCtx* dctx=debug_ctx_ud->ctx;

	FklSid_t fid=0;
	uint32_t line=0;
	PutBreakpointErrorType err=0;
	Breakpoint* item=NULL;

	FKL_DECL_AND_CHECK_ARG(file_line_sym_obj,exe);

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
			FKL_RAISE_BUILTIN_ERROR_CSTR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
		line=fklGetUint(file_line_sym_obj);
		fid=dctx->curline_file;
	}
	else if(FKL_IS_STR(file_line_sym_obj))
	{
		FKL_DECL_AND_CHECK_ARG(line_obj,exe);
		FKL_CHECK_TYPE(line_obj,fklIsVMint,exe);
		if(fklIsVMnumberLt0(line_obj))
			FKL_RAISE_BUILTIN_ERROR_CSTR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);

	item=putBreakpointWithFileAndLine(dctx,fid,line,&err);
	FklVMvalue* cond_exp_obj=NULL;

done:

	cond_exp_obj=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	if(item)
	{
		if(cond_exp_obj)
		{
			FklNastNode* expression=fklCreateNastNodeFromVMvalue(cond_exp_obj,dctx->curline,NULL,exe->gc);
			if(!expression)
				FKL_RAISE_BUILTIN_ERROR_CSTR(FKL_ERR_CIR_REF,exe);
			fklVMacquireSt(exe->gc);
			fklRecomputeSidForNastNode(expression,exe->gc->st,dctx->st);
			fklVMreleaseSt(exe->gc);
			item->cond_exp=expression;
		}
		item->is_temporary=1;
		item->cond_exp_obj=cond_exp_obj;

		FKL_VM_PUSH_VALUE(exe,create_breakpoint_vec(exe,dctx,item));
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
	FKL_DECL_AND_CHECK_ARG(debug_ctx_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,exe);

	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);
	DebugCtx* dctx=debug_ctx_ud->ctx;

	FklVMvalue* r=FKL_VM_NIL;
	FklVMvalue** pr=&r;
	for(FklHashTableItem* list=dctx->breakpoints.first
			;list
			;list=list->next)
	{
		Breakpoint* item=((BreakpointHashItem*)list->data)->bp;

		*pr=fklCreateVMvaluePairWithCar(exe,create_breakpoint_vec(exe,dctx,item));
		pr=&FKL_VM_CDR(*pr);
	}
	FKL_VM_PUSH_VALUE(exe,r);

	return 0;
}

static int bdb_debug_ctx_del_break(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(debug_ctx_obj,bp_num_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,exe);
	FKL_CHECK_TYPE(bp_num_obj,FKL_IS_FIX,exe);

	if(fklIsVMnumberLt0(bp_num_obj))
		FKL_RAISE_BUILTIN_ERROR_CSTR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);

	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);
	DebugCtx* dctx=debug_ctx_ud->ctx;

	uint64_t num=fklGetUint(bp_num_obj);
	Breakpoint* item=delBreakpoint(dctx,num);
	if(item)
		FKL_VM_PUSH_VALUE(exe,create_breakpoint_vec(exe,dctx,item));
	else
		FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(dctx->breakpoint_num));
	return 0;
}

static int bdb_debug_ctx_dis_break(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(debug_ctx_obj,bp_num_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,exe);
	FKL_CHECK_TYPE(bp_num_obj,FKL_IS_FIX,exe);

	if(fklIsVMnumberLt0(bp_num_obj))
		FKL_RAISE_BUILTIN_ERROR_CSTR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);

	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);
	DebugCtx* dctx=debug_ctx_ud->ctx;

	uint64_t num=fklGetUint(bp_num_obj);
	Breakpoint* item=disBreakpoint(dctx,num);
	if(item)
		FKL_VM_PUSH_VALUE(exe,create_breakpoint_vec(exe,dctx,item));
	else
		FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(dctx->breakpoint_num));
	return 0;
}

static int bdb_debug_ctx_enable_break(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(debug_ctx_obj,bp_num_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,exe);
	FKL_CHECK_TYPE(bp_num_obj,FKL_IS_FIX,exe);

	if(fklIsVMnumberLt0(bp_num_obj))
		FKL_RAISE_BUILTIN_ERROR_CSTR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);

	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);
	DebugCtx* dctx=debug_ctx_ud->ctx;

	uint64_t num=fklGetUint(bp_num_obj);
	Breakpoint* item=enableBreakpoint(dctx,num);
	if(item)
		FKL_VM_PUSH_VALUE(exe,create_breakpoint_vec(exe,dctx,item));
	else
		FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(dctx->breakpoint_num));
	return 0;
}

static int bdb_debug_ctx_set_list_src(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(debug_ctx_obj,line_num_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,exe);

	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);
	DebugCtx* dctx=debug_ctx_ud->ctx;

	FKL_CHECK_TYPE(line_num_obj,FKL_IS_FIX,exe);
	int64_t line_num=FKL_GET_FIX(line_num_obj);
	if(line_num>0&&line_num<dctx->curfile_lines->top)
		dctx->curlist_line=line_num;
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_list_src(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(debug_ctx_obj,exe);
	FklVMvalue* line_num_obj=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,exe);

	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);
	DebugCtx* dctx=debug_ctx_ud->ctx;

	if(line_num_obj)
	{
		FKL_CHECK_TYPE(line_num_obj,FKL_IS_FIX,exe);
		int64_t line_num=FKL_GET_FIX(line_num_obj);
		if(line_num<=0||line_num>=dctx->curfile_lines->top)
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

static int bdb_debug_ctx_list_file_src(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG3(debug_ctx_obj,filename_obj,line_num_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,exe);
	FKL_CHECK_TYPE(filename_obj,FKL_IS_STR,exe);
	FKL_CHECK_TYPE(line_num_obj,FKL_IS_FIX,exe);

	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);
	DebugCtx* dctx=debug_ctx_ud->ctx;

	FklSid_t fid=0;
	FklString* str=FKL_VM_STR(filename_obj);
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

	if(fid==0)
	{
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
		return 0;
	}

	const SourceCodeHashItem* item=getSourceWithFid(dctx,fid);
	int64_t line_num=FKL_GET_FIX(line_num_obj);
	if(item==NULL||line_num<=0||line_num>=item->lines.top)
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	else
	{
		uint32_t target_line=line_num;
		const FklString* line_str=item->lines.base[target_line-1];

		FklVMvalue* num_val=FKL_MAKE_VM_FIX(target_line);
		FklVMvalue* is_cur_line=(fid==dctx->curline_file&&target_line==dctx->curline)
			?FKL_VM_TRUE
			:FKL_VM_NIL;
		FklVMvalue* str_val=fklCreateVMvalueStr(exe,fklCopyString(line_str));

		FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueVec3(exe
					,num_val
					,is_cur_line
					,str_val));
	}
	return 0;
}


static int bdb_debug_ctx_set_step_into(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(debug_ctx_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,exe);
	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);

	setStepInto(debug_ctx_ud->ctx);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_set_step_over(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(debug_ctx_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,exe);
	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);

	setStepOver(debug_ctx_ud->ctx);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_set_step_out(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(debug_ctx_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,exe);
	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);

	setStepOut(debug_ctx_ud->ctx);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_set_until(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(debug_ctx_obj,exe);
	FklVMvalue* lineno_obj=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,exe);
	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);

	DebugCtx* dctx=debug_ctx_ud->ctx;
	if(lineno_obj==NULL)
		setStepOver(dctx);
	else
	{
		FKL_CHECK_TYPE(lineno_obj,FKL_IS_FIX,exe);
		int64_t line=FKL_GET_FIX(lineno_obj);
		if(line<((int64_t)dctx->curline))
			FKL_RAISE_BUILTIN_ERROR_CSTR(FKL_ERR_INVALID_VALUE,exe);
		setStepUntil(dctx,line);
	}
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static inline FklVMvalue* get_byte_code_frame_and_reset(DebugCtx* ctx,FklInstruction const** ppc)
{
	FklVMvalue* bc=NULL;
	FklVMframe* curframe=getCurrentFrame(ctx);
	for(;curframe;curframe=curframe->prev)
	{
		if(curframe->type==FKL_FRAME_COMPOUND)
		{
			FklVMvalue* v=FKL_VM_PROC(curframe->c.proc)->codeObj;
			if(ctx->curlist_bytecode!=v)
			{
				ctx->curlist_bytecode=v;
				ctx->curlist_ins_pc=0;
			}
			if(ppc)
				*ppc=curframe->c.pc;
			bc=v;
			break;
		}
	}
	return bc;
}

static inline FklVMvalue* create_ins_vec(FklVM* exe
		,DebugCtx* dctx
		,FklVMvalue* num_val
		,FklVMvalue* is_cur_ins
		,const FklInstruction* ins)
{
	FklVMvalue* opcode_str=fklCreateVMvalueStr(exe,fklCreateStringFromCstr(fklGetOpcodeName(ins->op)));
	FklVMvalue* imm1=NULL;
	FklVMvalue* imm2=NULL;
	int vec_len=3;
	FklOpcode op=ins->op;
	int open_arg_len=fklGetOpcodeArgLen(op);
	switch(open_arg_len)
	{
		case -1:
			switch(op)
			{
				case FKL_OP_PUSH_PROC:
					vec_len=5;
					imm1=FKL_MAKE_VM_FIX(ins->imm);
					imm2=fklMakeVMuint(ins->imm_u64,exe);
					break;
				case FKL_OP_PUSH_STR:
					vec_len=4;
					imm1=fklCreateVMvalueStr(exe,fklCopyString(ins->str));
					break;
				case FKL_OP_PUSH_BYTEVECTOR:
					vec_len=4;
					imm1=fklCreateVMvalueBvec(exe,fklCopyBytevector(ins->bvec));
					break;
				case FKL_OP_PUSH_BIG_INT:
					vec_len=4;
					imm1=fklCreateVMvalueBigInt(exe,ins->bi);
					break;
				default:
					abort();
					break;
			}
			break;
		case 0:
			break;
		case 1:
			vec_len=4;
			if(op==FKL_OP_PUSH_CHAR)
				imm1=FKL_MAKE_VM_CHR(ins->chr);
			else
				imm1=FKL_MAKE_VM_FIX(ins->imm_i8);
			break;
		case 2:
			vec_len=4;
			imm1=FKL_MAKE_VM_FIX(ins->imm_i16);
			break;
		case 4:
			vec_len=4;
			imm1=FKL_MAKE_VM_FIX(ins->imm_i32);
			break;
		case 8:
			switch(op)
			{
				case FKL_OP_PUSH_F64:
					vec_len=4;
					imm1=fklCreateVMvalueF64(exe,ins->f64);
					break;
				case FKL_OP_PUSH_VECTOR:
				case FKL_OP_PUSH_HASHTABLE_EQ:
				case FKL_OP_PUSH_HASHTABLE_EQV:
				case FKL_OP_PUSH_HASHTABLE_EQUAL:
				case FKL_OP_PUSH_LIST:
					vec_len=4;
					imm1=fklMakeVMuint(ins->imm_u64,exe);
					break;
				case FKL_OP_PUSH_I64:
				case FKL_OP_PUSH_I64_BIG:
				case FKL_OP_JMP:
				case FKL_OP_JMP_IF_FALSE:
				case FKL_OP_JMP_IF_TRUE:
					vec_len=4;
					imm1=fklMakeVMint(ins->imm_i64,exe);
					break;
				case FKL_OP_PUSH_SYM:
					vec_len=4;
					{
						FklSid_t id=fklVMaddSymbol(exe->gc
								,fklGetSymbolWithId(ins->sid,dctx->st)->symbol)->id;
						imm1=FKL_MAKE_VM_SYM(id);
					}
					break;
				case FKL_OP_IMPORT:
				case FKL_OP_CLOSE_REF:
				case FKL_OP_EXPORT_TO:
					vec_len=5;
					imm1=FKL_MAKE_VM_FIX(ins->imm);
					imm2=FKL_MAKE_VM_FIX(ins->imm_u32);
					break;
				default:
					abort();
					break;
			}
			break;
	}
	FklVMvalue* retval=NULL;
	switch(vec_len)
	{
		case 3:
			retval=fklCreateVMvalueVec3(exe,num_val,is_cur_ins,opcode_str);
			break;
		case 4:
			retval=fklCreateVMvalueVec4(exe,num_val,is_cur_ins,opcode_str,imm1);
			break;
		case 5:
			retval=fklCreateVMvalueVec5(exe,num_val,is_cur_ins,opcode_str,imm1,imm2);
			break;
	}
	return retval;
}

static int bdb_debug_ctx_list_ins(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(debug_ctx_obj,exe);
	FklVMvalue* pc_num_obj=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,exe);

	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);
	DebugCtx* dctx=debug_ctx_ud->ctx;

	const FklInstruction* cur_ins=NULL;
	FklVMvalue* byte_code=get_byte_code_frame_and_reset(dctx,&cur_ins);
	if(byte_code==NULL)
	{
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
		return 0;
	}

	FklByteCode* bc=FKL_VM_CO(byte_code)->bc;
	uint64_t cur_pc=cur_ins-bc->code;
	if(pc_num_obj)
	{
		FKL_CHECK_TYPE(pc_num_obj,fklIsVMint,exe);
		if(fklIsVMnumberLt0(pc_num_obj))
			FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
		else
		{
			uint64_t ins_pc=fklGetUint(pc_num_obj);
			if(ins_pc>=bc->len)
				FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
			else
			{
				const FklInstruction* ins=&bc->code[ins_pc];
				if(ins->op==0)
					ins=&((Breakpoint*)ins->ptr)->origin_ins;

				FklVMvalue* num_val=fklMakeVMuint(ins_pc,exe);
				FklVMvalue* is_cur_ins=cur_pc==ins_pc?FKL_VM_TRUE:FKL_VM_NIL;

				FKL_VM_PUSH_VALUE(exe,create_ins_vec(exe
							,dctx
							,num_val
							,is_cur_ins
							,ins));
			}
		}
	}
	else if(dctx->curlist_ins_pc<bc->len)
	{
		uint64_t ins_pc=dctx->curlist_ins_pc;
		const FklInstruction* ins=&bc->code[ins_pc];

		FklVMvalue* num_val=fklMakeVMuint(ins_pc,exe);
		FklVMvalue* is_cur_ins=cur_pc==ins_pc?FKL_VM_TRUE:FKL_VM_NIL;

		FKL_VM_PUSH_VALUE(exe,create_ins_vec(exe
					,dctx
					,num_val
					,is_cur_ins
					,ins));
	}
	else
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_set_list_ins(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(debug_ctx_obj,pc_num_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,exe);

	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);
	DebugCtx* dctx=debug_ctx_ud->ctx;

	FKL_CHECK_TYPE(pc_num_obj,fklIsVMint,exe);

	FklVMvalue* byte_code=get_byte_code_frame_and_reset(dctx,NULL);


	if(byte_code&&!fklIsVMnumberLt0(pc_num_obj))
	{
		uint64_t ins_pc=fklGetUint(pc_num_obj);
		dctx->curlist_ins_pc=ins_pc;
	}
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_get_cur_ins(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(debug_ctx_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,exe);

	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);
	DebugCtx* dctx=debug_ctx_ud->ctx;

	const FklInstruction* ins=NULL;
	FklVMvalue* byte_code=get_byte_code_frame_and_reset(dctx,&ins);


	if(byte_code)
	{
		FklByteCode* bc=FKL_VM_CO(byte_code)->bc;
		if(ins<&bc->code[bc->len])
		{
			if(ins->op==0)
				ins=&((Breakpoint*)ins->ptr)->origin_ins;
			uint64_t ins_pc=ins-bc->code;
			FklVMvalue* num_val=fklMakeVMuint(ins_pc,exe);
			FklVMvalue* is_cur_ins=FKL_VM_TRUE;

			FKL_VM_PUSH_VALUE(exe,create_ins_vec(exe
						,dctx
						,num_val
						,is_cur_ins
						,ins));
		}
		else
			FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	}
	else
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_set_single_ins(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(debug_ctx_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,exe);
	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);

	setSingleStep(debug_ctx_ud->ctx);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_eval(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(debug_ctx_obj,expression_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,exe);
	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);

	DebugCtx* dctx=debug_ctx_ud->ctx;
	if(dctx->done&&dctx->reached_thread==NULL)
		printThreadAlreadyExited(dctx,stdout);
	else
	{
		FklVMframe* cur_frame=getCurrentFrame(dctx);
		for(;cur_frame&&cur_frame->type==FKL_FRAME_OTHEROBJ;cur_frame=cur_frame->prev);

		if(cur_frame)
		{
			int compile_unabled;
			FklNastNode* expression=fklCreateNastNodeFromVMvalue(expression_obj,dctx->curline,NULL,exe->gc);
			if(!expression)
				FKL_RAISE_BUILTIN_ERROR_CSTR(FKL_ERR_CIR_REF,exe);
			fklVMacquireSt(exe->gc);
			fklRecomputeSidForNastNode(expression,exe->gc->st,dctx->st);
			fklVMreleaseSt(exe->gc);
			FklVMvalue* proc=compileEvalExpression(dctx
					,dctx->reached_thread
					,expression
					,cur_frame
					,&compile_unabled);
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
			if(compile_unabled)
				printUnableToCompile(stdout);
		}
		else
			printThreadCantEvaluate(dctx,stdout);
	}
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_back_trace(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(obj,prefix_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,IS_DEBUG_CTX_UD,exe);
	FKL_CHECK_TYPE(prefix_obj,FKL_IS_STR,exe);
	FKL_DECL_VM_UD_DATA(debug_ud,DebugUdCtx,obj);
	printBacktrace(debug_ud->ctx,FKL_VM_STR(prefix_obj),stdout);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_print_cur_frame(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(obj,prefix_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,IS_DEBUG_CTX_UD,exe);
	FKL_CHECK_TYPE(prefix_obj,FKL_IS_STR,exe);
	FKL_DECL_VM_UD_DATA(debug_ud,DebugUdCtx,obj);
	printCurFrame(debug_ud->ctx,FKL_VM_STR(prefix_obj),stdout);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_up(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,IS_DEBUG_CTX_UD,exe);
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
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,IS_DEBUG_CTX_UD,exe);
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
	FKL_DECL_AND_CHECK_ARG2(obj,prefix_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,IS_DEBUG_CTX_UD,exe);
	FKL_CHECK_TYPE(prefix_obj,FKL_IS_STR,exe);
	FKL_DECL_VM_UD_DATA(debug_ud,DebugUdCtx,obj);

	DebugCtx* dctx=debug_ud->ctx;
	listThreads(dctx,FKL_VM_STR(prefix_obj),stdout);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_switch_thread(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(obj,id_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,IS_DEBUG_CTX_UD,exe);
	FKL_CHECK_TYPE(id_obj,FKL_IS_FIX,exe);
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
	{"debug-ctx-list-file-src",   bdb_debug_ctx_list_file_src,   },
	{"debug-ctx-set-list-src",    bdb_debug_ctx_set_list_src,    },

	{"debug-ctx-list-ins",        bdb_debug_ctx_list_ins,        },
	{"debug-ctx-set-list-ins",    bdb_debug_ctx_set_list_ins,    },
	{"debug-ctx-get-cur-ins",     bdb_debug_ctx_get_cur_ins,     },

	{"debug-ctx-del-break",       bdb_debug_ctx_del_break,       },
	{"debug-ctx-dis-break",       bdb_debug_ctx_dis_break,       },
	{"debug-ctx-enable-break",    bdb_debug_ctx_enable_break,    },
	{"debug-ctx-set-break",       bdb_debug_ctx_set_break,       },
	{"debug-ctx-set-tbreak",      bdb_debug_ctx_set_tbreak,      },
	{"debug-ctx-list-break",      bdb_debug_ctx_list_break,      },

	{"debug-ctx-set-step-over",   bdb_debug_ctx_set_step_over,   },
	{"debug-ctx-set-step-into",   bdb_debug_ctx_set_step_into,   },
	{"debug-ctx-set-step-out",    bdb_debug_ctx_set_step_out,    },
	{"debug-ctx-set-single-ins",  bdb_debug_ctx_set_single_ins,  },
	{"debug-ctx-set-until",       bdb_debug_ctx_set_until,       },

	{"debug-ctx-continue",        bdb_debug_ctx_continue,        },
	{"debug-ctx-exit?",           bdb_debug_ctx_exit_p,          },
	{"debug-ctx-done?",           bdb_debug_ctx_done_p,          },
	{"debug-ctx-exit",            bdb_debug_ctx_exit,            },
	{"debug-ctx-eval",            bdb_debug_ctx_eval,            },
	{"debug-ctx-restart",         bdb_debug_ctx_restart,         },

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
	*count=EXPORT_NUM;
	FklVMvalue** loc=(FklVMvalue**)malloc(sizeof(FklVMvalue*)*EXPORT_NUM);
	FKL_ASSERT(loc);
	fklVMacquireSt(exe->gc);
	FklSymbolTable* st=exe->gc->st;
	for(size_t i=0;i<EXPORT_NUM;i++)
	{
		FklSid_t id=fklAddSymbolCstr(exports_and_func[i].sym,st)->id;
		FklVMcFunc func=exports_and_func[i].f;
		FklVMvalue* dlproc=fklCreateVMvalueCproc(exe,func,dll,NULL,id);
		loc[i]=dlproc;
	}
	fklVMreleaseSt(exe->gc);
	return loc;
}
