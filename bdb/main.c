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
		,CmdReadCtx* ctx
		,const char* prompt
		,const char* func_name)
{
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
			exit(0);
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
	FklVMvalue* cmd=debug_ctx_replxx_input(exe,&debug_ctx_ud->ctx->read_ctx,FKL_VM_STR(prompt_obj)->str,Pname);
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
	FklVM* cur_thread=dctx->cur_thread;
	FklVMframe* frame=cur_thread->top_frame;
	const FklLineNumberTableItem* ln=getCurFrameLineNumber(frame);
	const FklString* line_str=getCurLineStr(dctx,ln->fid,ln->line);
	FklVMvalue* line_str_value=fklCreateVMvalueStr(exe,fklCopyString(line_str));
	const FklString* file_str=fklGetSymbolWithId(ln->fid,dctx->st)->symbol;
	FklVMvalue* file_str_value=fklCreateVMvalueStr(exe,fklCopyString(file_str));
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvaluePair(exe,file_str_value,line_str_value));
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
	int reach_break_point=0;
	FKL_VM_PUSH_VALUE(exe,debug_ctx_obj);
	fklUnlockThread(exe);
	if(setjmp(dctx->jmpb)==DBG_REACH_BREAKPOINT)
		reach_break_point=1;
	else
		fklVMidleLoop(dctx->gc);
	fklLockThread(exe);
	FKL_VM_SET_TP_AND_PUSH_VALUE(exe
			,rtp
			,reach_break_point
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int bdb_debug_ctx_set_break(FKL_CPROC_ARGL)
{
	static const char Pname[]="bdb.debug-ctx-continue";
	FKL_DECL_AND_CHECK_ARG(debug_ctx_obj,exe,Pname);
	FKL_CHECK_TYPE(debug_ctx_obj,IS_DEBUG_CTX_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(debug_ctx_ud,DebugUdCtx,debug_ctx_obj);
	DebugCtx* dctx=debug_ctx_ud->ctx;
	uint32_t arg_num=FKL_VM_GET_ARG_NUM(exe);

	FklSid_t fid=0;
	uint32_t line=0;
	switch(arg_num)
	{
		case 1:
			{
				FklVMvalue* line_obj=FKL_VM_POP_ARG(exe);
				FKL_CHECK_TYPE(line_obj,fklIsVMint,Pname,exe);
				if(fklIsVMnumberLt0(line_obj))
					FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
				line=fklGetUint(line_obj);
				fid=dctx->curline_file;
			}
			break;
		case 2:
			{
				FklVMvalue* filename_obj=FKL_VM_POP_ARG(exe);
				FklVMvalue* line_obj=FKL_VM_POP_ARG(exe);
				FKL_CHECK_TYPE(line_obj,fklIsVMint,Pname,exe);
				FKL_CHECK_TYPE(filename_obj,FKL_IS_STR,Pname,exe);
				if(fklIsVMnumberLt0(line_obj))
					FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
				line=fklGetUint(line_obj);
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
			}
			break;
		default:
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_TOOMANYARG,exe);
			break;
	}

	PutBreakpointErrorType err=0;
	BreakpointHashItem* item=putBreakpoint(dctx,fid,line,&err);
	if(item)
	{
		FklVMvalue* filename=fklCreateVMvalueStr(exe,fklCopyString(fklGetSymbolWithId(item->key.fid,dctx->st)->symbol));
		FklVMvalue* line=FKL_MAKE_VM_FIX(item->key.line);
		FklVMvalue* num=FKL_MAKE_VM_FIX(item->num);
		FklVMvalue* r=fklCreateVMvalueVec(exe,3);
		FklVMvec* vec=FKL_VM_VEC(r);
		vec->base[0]=num;
		vec->base[1]=filename;
		vec->base[2]=line;

		FKL_VM_PUSH_VALUE(exe,r);
	}
	else
		FKL_VM_PUSH_VALUE(exe
				,fklCreateVMvalueStr(exe
					,fklCreateStringFromCstr(getPutBreakpointErrorInfo(err))));
	return 0;
}

static int bdb_debug_incomplete(FKL_CPROC_ARGL)
{
	abort();
}

struct SymFunc
{
	const char* sym;
	FklVMcFunc f;
}exports_and_func[]=
{
	{"debug-ctx?",            bdb_debug_ctx_p,           },
	{"make-debug-ctx",        bdb_make_debug_ctx,        },
	{"debug-ctx-repl",        bdb_debug_ctx_repl,        },
	{"debug-ctx-get-curline", bdb_debug_ctx_get_curline, },
	{"debug-ctx-end?",        bdb_debug_ctx_end_p,       },
	{"debug-ctx-step",        bdb_debug_incomplete,      },
	{"debug-ctx-next",        bdb_debug_incomplete,      },
	{"debug-ctx-del-break",   bdb_debug_incomplete,      },
	{"debug-ctx-set-break",   bdb_debug_ctx_set_break,   },
	{"debug-ctx-continue",    bdb_debug_ctx_continue,    },
	{"debug-ctx-exit",        bdb_debug_incomplete,      },
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
