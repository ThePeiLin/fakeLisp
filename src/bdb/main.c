#include "bdb.h"

#include <fakeLisp/builtin.h>
#include <fakeLisp/common.h>
#include <fakeLisp/readline.h>
#include <fakeLisp/str_buf.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>
#include <string.h>

FKL_VM_DEF_UD_STRUCT(FklVMvalueBdbPd, {
    FklVMvalue *done_sym;
    FklVMvalue *err_sym;
});

static FklVMudMetaTable const BdbPublicDataMetaTable;

static FKL_ALWAYS_INLINE FklVMvalueBdbPd *as_bdb_pd(const FklVMvalue *v) {
    FKL_ASSERT(FKL_VM_UD(v)->mt_ == &BdbPublicDataMetaTable);
    return FKL_TYPE_CAST(FklVMvalueBdbPd *, v);
}

static void bdb_public_ud_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueBdbPd *pd = as_bdb_pd(ud);
    fklVMgcToGray(pd->done_sym, gc);
    fklVMgcToGray(pd->err_sym, gc);
}

static void init_bdb_public_data(FklVMvalueBdbPd *pd, FklVM *exe) {
    pd->done_sym = fklVMaddSymbolCstr(exe, "done");
    pd->err_sym = fklVMaddSymbolCstr(exe, "error");
}

static FklVMudMetaTable const BdbPublicDataMetaTable = {
    .size = sizeof(FklVMvalueBdbPd),
    .atomic = bdb_public_ud_atomic,
};

FKL_VM_USER_DATA_DEFAULT_PRINT(debug_ctx_print, "debug-ctx");

static inline char *get_valid_file_name(const char *filename) {
    if (fklIsAccessibleRegFile(filename)) {
        if (fklIsScriptFile(filename))
            return fklZstrdup(filename);
        return NULL;
    }

    char *r = NULL;
    FklStrBuf main_script_buf;
    fklInitStrBuf(&main_script_buf);

    fklStrBufConcatWithCstr(&main_script_buf, filename);
    fklStrBufConcatWithCstr(&main_script_buf, FKL_PATH_SEPARATOR_STR);
    fklStrBufConcatWithCstr(&main_script_buf, "main.fkl");

    if (fklIsAccessibleRegFile(main_script_buf.buf))
        r = fklZstrdup(main_script_buf.buf);
    fklUninitStrBuf(&main_script_buf);
    return r;
}

static inline void atomic_cmd_read_ctx(const BdbCmdReadCtx *ctx, FklVMgc *gc) {
    const FklAnalysisSymbol *base = ctx->symbols.base;
    const FklAnalysisSymbol *end = &base[ctx->symbols.size];
    for (; base < end; base++)
        fklVMgcToGray(base->ast, gc);
}

static const FklVMudMetaTable DebugCtxUdMetaTable;

static inline int is_debug_ctx(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->mt_ == &DebugCtxUdMetaTable;
}

static inline DebugCtx *as_dbg_ctx(const FklVMvalue *v) {
    FKL_ASSERT(is_debug_ctx(v));
    return (DebugCtx *)v;
}

static void debug_ctx_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    DebugCtx *dctx = as_dbg_ctx(ud);
    atomic_cmd_read_ctx(&dctx->read_ctx, gc);
    fklVMgcToGray(dctx->backtrace_list, gc);
}

static int debug_ctx_finalize(FklVMvalue *data, FklVMgc *gc) {
    DebugCtx *ctx = as_dbg_ctx(data);
    if (ctx->exit == 0) {
        fprintf(stderr,
                "[%s: %d] debug ctx should be exit manually before it be finalized\n",
                __REL_FILE__,
                __LINE__);
        abort();
    }
    bdbUninitDbgCtx(ctx);
    return FKL_VM_UD_FINALIZE_NOW;
}

static const FklVMudMetaTable DebugCtxUdMetaTable = {
    .size = sizeof(DebugCtx),
    .prin1 = debug_ctx_print,
    .princ = debug_ctx_print,
    .atomic = debug_ctx_atomic,
    .finalize = debug_ctx_finalize,
};

static inline int is_string_list(const FklVMvalue *p) {
    for (; p != FKL_VM_NIL; p = FKL_VM_CDR(p))
        if (!FKL_IS_PAIR(p) || !FKL_IS_STR(FKL_VM_CAR(p)))
            return 0;
    return 1;
}

static int bdb_make_debug_ctx(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *filename_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *argv_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(filename_obj, FKL_IS_STR, exe);
    FKL_CHECK_TYPE(argv_obj, is_string_list, exe);
    char *valid_filename = get_valid_file_name(FKL_VM_STR(filename_obj)->str);
    if (!valid_filename)
        FKL_RAISE_BUILTIN_ERROR_FMT(FKL_ERR_FILEFAILURE,
                exe,
                "Failed for file: %s",
                filename_obj);
    FklVMvalue *dll = FKL_VM_CPROC(ctx->proc)->dll;
    FklVMvalue *ud = fklCreateVMvalueUd(exe, &DebugCtxUdMetaTable, dll);
    DebugCtx *dctx = as_dbg_ctx(ud);

    int r = 0;
    FKL_VM_UNLOCK_BLOCK(exe, flag) {
        r = bdbInitDbgCtx(dctx, exe, valid_filename, argv_obj);
    }

    fklZfree(valid_filename);
    if (r)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE, exe);
    FKL_CPROC_RETURN(exe, ctx, ud);
    return 0;
}

static int bdb_debug_ctx_p(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CPROC_RETURN(exe, ctx, is_debug_ctx(obj) ? FKL_VM_TRUE : FKL_VM_NIL);
    return 0;
}

static int bdb_debug_ctx_exit_p(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, is_debug_ctx, exe);
    DebugCtx *dctx = as_dbg_ctx(obj);
    FKL_CPROC_RETURN(exe, ctx, dctx->exit ? FKL_VM_TRUE : FKL_VM_NIL);
    return 0;
}

static int bdb_debug_ctx_done_p(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, is_debug_ctx, exe);
    DebugCtx *dctx = as_dbg_ctx(obj);
    FKL_CPROC_RETURN(exe, ctx, dctx->done ? FKL_VM_TRUE : FKL_VM_NIL);
    return 0;
}

static int bdb_debug_ctx_exit(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, is_debug_ctx, exe);
    DebugCtx *dctx = as_dbg_ctx(obj);
    bdbExitDbgCtx(dctx);
    FKL_CPROC_RETURN(exe, ctx, obj);
    return 0;
}

static inline void debug_repl_parse_ctx_and_buf_reset(BdbCmdReadCtx *cc,
        FklStrBuf *s) {
    if (s) {
        fklStrBufClear(s);
        s->buf[0] = '\0';
    }
    cc->states.size = 0;
    fklVMvaluePushState0ToStack(&cc->states);
}

typedef struct {
    FklVM *exe;
    BdbCmdReadCtx *read_ctx;
    FklGrammerMatchCtx *ctx;
    size_t offset;
    FklVMvalue *ast;

    int err;
} ReadExpressionEndArgs;

#define READ_ERROR_UNEXPETED_EOF (1)
#define READ_ERROR_INVALIDEXPR (2)

static int read_expression_end_cb(const char *str,
        int str_len,
        const uint32_t *u32_buf,
        int u32_len,
        int pos,
        void *vargs) {
    if (pos != u32_len)
        return 0;

    ReadExpressionEndArgs *args = FKL_TYPE_CAST(ReadExpressionEndArgs *, vargs);
    FklVM *exe = args->exe;
    BdbCmdReadCtx *read_ctx = args->read_ctx;
    FklGrammerMatchCtx *ctx = args->ctx;

    size_t restLen = str_len - args->offset;
    size_t errLine = 0;
    FklParseError err = 0;

    int end = 0;
    FKL_VM_LOCK_BLOCK(exe, flag) {
        FklVMvalue *ast = fklDefaultParseForCharBuf(str + args->offset,
                restLen,
                &restLen,
                ctx,
                &err,
                &errLine,
                &read_ctx->symbols,
                &read_ctx->states);

        args->offset = str_len - restLen;

        if (err == FKL_PARSE_TERMINAL_MATCH_FAILED && restLen) {
            args->err = READ_ERROR_INVALIDEXPR;
            end = 1;
        } else if (err == FKL_PARSE_REDUCE_FAILED) {
            args->err = READ_ERROR_INVALIDEXPR;
            end = 1;
        } else if (ast) {
            args->ast = ast;
            args->err = 0;
            FKL_VM_PUSH_VALUE(exe, ast);
            end = 1;
        } else {
            end = 0;
        }
    }
    return end;
}

static inline int debug_ctx_read_expression_in_string_buffer(FklStrBuf *buf,
        const char *prompt,
        ReadExpressionEndArgs *args) {
    char *next = fklReadline3(prompt,
            buf->buf,
            read_expression_end_cb,
            FKL_TYPE_CAST(void *, args));
    fklStrBufClear(buf);
    int is_eof = next == NULL;
    if (next) {
        fklStrBufConcatWithCstr(buf, next);
        fklZfree(next);
    }
    return is_eof;
}

static inline FklVMvalue *
debug_ctx_read_expression(FklVM *exe, DebugCtx *dctx, const char *prompt) {
    BdbCmdReadCtx *read_ctx = &dctx->read_ctx;
    FklGrammerMatchCtx ctx = FKL_VMVALUE_PARSE_CTX_INIT(exe, NULL);
    FklStrBuf *const s = &read_ctx->buf;
    ReadExpressionEndArgs args = {
        .exe = exe,
        .read_ctx = read_ctx,
        .ctx = &ctx,
    };

    FKL_VM_UNLOCK_BLOCK(exe, flag) {
        debug_ctx_read_expression_in_string_buffer(s, prompt, &args);
    }

    if (args.ast == NULL && args.err) {
        debug_repl_parse_ctx_and_buf_reset(read_ctx, s);
        if (args.err == READ_ERROR_UNEXPETED_EOF)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_UNEXPECTED_EOF, exe);
        else if (args.err == READ_ERROR_INVALIDEXPR)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDEXPR, exe);
    } else if (args.ast) {
        fklStoreHistoryInStrBuf(s, args.offset);
        debug_repl_parse_ctx_and_buf_reset(read_ctx, NULL);
        return args.ast;
    }

    return FKL_VM_EOF;
}

static int bdb_debug_ctx_readline(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *dbg_ctx_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *prompt_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(dbg_ctx_obj, is_debug_ctx, exe);
    FKL_CHECK_TYPE(prompt_obj, FKL_IS_STR, exe);

    DebugCtx *dctx = as_dbg_ctx(dbg_ctx_obj);
    const char *prompt = FKL_VM_STR(prompt_obj)->str;
    FklVMvalue *cmd = debug_ctx_read_expression(exe, dctx, prompt);
    FKL_CPROC_RETURN(exe, ctx, cmd);
    return 0;
}

static int bdb_debug_ctx_get_curline(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *debug_ctx_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(debug_ctx_obj, is_debug_ctx, exe);

    DebugCtx *dctx = as_dbg_ctx(debug_ctx_obj);

    BdbPos curline = { 0 };
    if (!bdbGetCurLine(dctx, &curline)) {
        FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
        return 0;
    }

    FklVMvalue *line_str_v = fklCreateVMvalueStr(exe, curline.str);
    FklVMvalue *file_str_v = fklCreateVMvalueStr(exe, curline.filename);
    FklVMvalue *line_num_value = FKL_MAKE_VM_FIX(curline.line);

    FklVMvalue *r = fklCreateVMvalueVecExt(exe,
            3,
            file_str_v,
            line_num_value,
            line_str_v);

    FKL_CPROC_RETURN(exe, ctx, r);

    return 0;
}

static inline int debug_restart(DebugCtx *dctx, FklVM *exe) {
    if (!dctx->running)
        return 0;

    if (dctx->reached_thread) {
        FKL_VM_UNLOCK_BLOCK(exe, flag) {
            bdbSetAllThreadReadyToExit(dctx->reached_thread);
            bdbWaitAllThreadExit(dctx->reached_thread);
        }
    } else if (dctx->gc.main_thread) {
        fklDestroyAllVMs(dctx->gc.main_thread);
        dctx->gc.main_thread = NULL;
    }
    bdbRestartDebugging(dctx);
    return 1;
}

static int bdb_debug_ctx_continue(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *dctx_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(dctx_obj, is_debug_ctx, exe);

    DebugCtx *dctx = as_dbg_ctx(dctx_obj);
    bdbClearDeletedBp(dctx);
    if (dctx->done) {
        debug_restart(dctx, exe);
        FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
        return 0;
    }
    FklVMvalue *retval = FKL_VM_NIL;
    FKL_VM_UNLOCK_BLOCK(exe, flag) {
        dctx->reached_breakpoint = NULL;
        dctx->reached_thread = NULL;
        jmp_buf buf;
        dctx->jmpb = &buf;
        int r = setjmp(buf);
        if (r == DBG_INTERRUPTED) {
            dctx->done = 0;
            if (dctx->reached_breakpoint) {
                bdbUnsetStepping(dctx);
                const BdbBp *reached_bp = dctx->reached_breakpoint;
                retval = bdbCreateBpVec(exe, dctx, reached_bp);
            }
        } else if (r == DBG_ERROR_OCCUR) {
            dctx->done = 1;
            retval = as_bdb_pd(ctx->pd)->err_sym;
        } else {
            if (dctx->running) {
                dctx->reached_breakpoint = NULL;
                dctx->reached_thread_frames.size = 0;
                fklVMreleaseWq(&dctx->gc);
                fklVMcontinueTheWorld(&dctx->gc);
            }
            dctx->running = 1;
            fklVMidleLoop(&dctx->gc);
            dctx->done = 1;
            retval = as_bdb_pd(ctx->pd)->done_sym;
        }
        dctx->jmpb = NULL;
    }
    FKL_CPROC_RETURN(exe, ctx, retval);
    return 0;
}

static int bdb_debug_ctx_restart(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *dctx_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(dctx_obj, is_debug_ctx, exe);

    DebugCtx *dctx = as_dbg_ctx(dctx_obj);
    if (debug_restart(dctx, exe))
        FKL_CPROC_RETURN(exe, ctx, FKL_VM_TRUE);
    else
        FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    return 0;
}

static int bdb_debug_ctx_set_break(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 3, argc);

    FklVMvalue *dctx_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *is_temporary = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *name_obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FKL_CHECK_TYPE(dctx_obj, is_debug_ctx, exe);
    DebugCtx *dctx = as_dbg_ctx(dctx_obj);

    FklVMvalue *filename = NULL;
    uint32_t line = 0;
    BdbPutBpErrorType err = 0;
    BdbBp *item = NULL;

    uint32_t cond_exp_obj_idx = 3 + FKL_IS_STR(name_obj);

    if (argc > cond_exp_obj_idx + 1)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOMANYARG, exe);

    if (FKL_IS_SYM(name_obj)) {
        const FklString *function_name = FKL_VM_SYM(name_obj);
        item = bdbPutBp1(dctx, function_name);
        if (item != NULL)
            goto done;
        FKL_RAISE_BUILTIN_ERROR_FMT(FKL_ERR_INVALID_VALUE,
                exe,
                "Variable %S is not a procedure",
                name_obj);

    } else if (fklIsVMint(name_obj)) {
        if (fklIsVMnumberLt0(name_obj))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
        line = fklVMgetUint(name_obj);
        filename = fklCreateVMvalueStr(exe, bdbSymbol(dctx->curfile_lines->k));
    } else if (FKL_IS_STR(name_obj)) {
        if (argc < 4) {
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG, exe);
        }

        FklVMvalue *line_obj = FKL_CPROC_GET_ARG(exe, ctx, 3);
        FKL_CHECK_TYPE(line_obj, fklIsVMint, exe);
        if (fklIsVMnumberLt0(line_obj))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
        line = fklVMgetUint(line_obj);
        filename = name_obj;
    } else {
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    }

    item = bdbPutBp(dctx, FKL_VM_STR(filename), line, &err);
    FklVMvalue *cond_exp_obj = NULL;

done:
    cond_exp_obj = argc > cond_exp_obj_idx
                         ? FKL_CPROC_GET_ARG(exe, ctx, cond_exp_obj_idx)
                         : NULL;
    if (item) {
        if (cond_exp_obj == NULL)
            goto return_bp_vec;
        FKL_CHECK_TYPE(cond_exp_obj, FKL_IS_STR, exe);

        BdbWrapper cond_exp = bdbParse(dctx, FKL_VM_STR(cond_exp_obj));
        if (!bdbHas(cond_exp))
            FKL_RAISE_BUILTIN_ERROR_FMT(FKL_ERR_SYNTAXERROR,
                    exe,
                    "%s is not a valid expression",
                    cond_exp_obj)
        item->is_temporary = is_temporary != FKL_VM_NIL;
        item->cond_exp = cond_exp;
    return_bp_vec:
        FKL_CPROC_RETURN(exe, ctx, bdbCreateBpVec(exe, dctx, item));
    } else {
        const char *err_msg = bdbGetPutBpErrorMsg(err);
        FklVMvalue *str_v = fklCreateVMvalueStr1(exe, err_msg);
        FKL_CPROC_RETURN(exe, ctx, str_v);
    }
    return 0;
}

static int bdb_debug_ctx_list_break(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *dctx_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(dctx_obj, is_debug_ctx, exe);

    DebugCtx *dctx = as_dbg_ctx(dctx_obj);

    FklVMvalue *r = FKL_VM_NIL;
    FklVMvalue **pr = &r;
    for (BdbBpIdxHashMapNode *list = dctx->bt.idx_ht.first; list;
            list = list->next) {
        BdbBp *item = list->v;

        FklVMvalue *car = bdbCreateBpVec(exe, dctx, item);
        *pr = fklCreateVMvaluePair1(exe, car);
        pr = &FKL_VM_CDR(*pr);
    }
    FKL_CPROC_RETURN(exe, ctx, r);

    return 0;
}

static int bdb_debug_ctx_delete_break(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *dctx_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *bp_num_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);

    FKL_CHECK_TYPE(dctx_obj, is_debug_ctx, exe);
    FKL_CHECK_TYPE(bp_num_obj, FKL_IS_FIX, exe);

    if (fklIsVMnumberLt0(bp_num_obj))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);

    DebugCtx *dctx = as_dbg_ctx(dctx_obj);

    uint64_t num = fklVMgetUint(bp_num_obj);
    BdbBp *item = bdbDeleteBp(dctx, num);
    if (item)
        FKL_CPROC_RETURN(exe, ctx, bdbCreateBpVec(exe, dctx, item));
    else
        FKL_CPROC_RETURN(exe, ctx, FKL_MAKE_VM_FIX(dctx->bt.idx_ht.count));
    return 0;
}

static int bdb_debug_ctx_disable_break(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *dctx_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *bp_num_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(dctx_obj, is_debug_ctx, exe);
    FKL_CHECK_TYPE(bp_num_obj, FKL_IS_FIX, exe);

    if (fklIsVMnumberLt0(bp_num_obj))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);

    DebugCtx *dctx = as_dbg_ctx(dctx_obj);

    uint64_t num = fklVMgetUint(bp_num_obj);
    BdbBp *item = bdbDisableBp(dctx, num);
    if (item)
        FKL_CPROC_RETURN(exe, ctx, bdbCreateBpVec(exe, dctx, item));
    else
        FKL_CPROC_RETURN(exe, ctx, FKL_MAKE_VM_FIX(dctx->bt.idx_ht.count));
    return 0;
}

static int bdb_debug_ctx_enable_break(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *dctx_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *bp_num_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(dctx_obj, is_debug_ctx, exe);
    FKL_CHECK_TYPE(bp_num_obj, FKL_IS_FIX, exe);

    if (fklIsVMnumberLt0(bp_num_obj))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);

    DebugCtx *dctx = as_dbg_ctx(dctx_obj);

    uint64_t num = fklVMgetUint(bp_num_obj);
    BdbBp *item = bdbEnableBp(dctx, num);
    if (item)
        FKL_CPROC_RETURN(exe, ctx, bdbCreateBpVec(exe, dctx, item));
    else
        FKL_CPROC_RETURN(exe, ctx, FKL_MAKE_VM_FIX(dctx->bt.idx_ht.count));
    return 0;
}

static int bdb_debug_ctx_set_list_src(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *dctx_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *line_num_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(dctx_obj, is_debug_ctx, exe);

    DebugCtx *dctx = as_dbg_ctx(dctx_obj);

    FKL_CHECK_TYPE(line_num_obj, FKL_IS_FIX, exe);
    uint64_t line_num = FKL_GET_FIX(line_num_obj);
    if (line_num > 0 && line_num < dctx->curfile_lines->v.size)
        dctx->curlist_line = line_num;
    FKL_CPROC_RETURN(exe, ctx, dctx_obj);
    return 0;
}

static int bdb_debug_ctx_list_src(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 1, 2);
    FklVMvalue *dctx_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *line_num_obj = argc > 1 ? FKL_CPROC_GET_ARG(exe, ctx, 1) : NULL;
    FKL_CHECK_TYPE(dctx_obj, is_debug_ctx, exe);

    DebugCtx *dctx = as_dbg_ctx(dctx_obj);

    if (line_num_obj) {
        FKL_CHECK_TYPE(line_num_obj, FKL_IS_FIX, exe);
        uint64_t line_num = FKL_GET_FIX(line_num_obj);
        if (line_num <= 0 || line_num >= dctx->curfile_lines->v.size)
            FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
        else {
            uint32_t curline_num = line_num;
            const FklString *line_str =
                    dctx->curfile_lines->v.base[curline_num - 1];

            FklVMvalue *num_val = FKL_MAKE_VM_FIX(curline_num);
            FklVMvalue *is_cur_line =
                    curline_num == dctx->curline ? FKL_VM_TRUE : FKL_VM_NIL;
            FklVMvalue *str_val = fklCreateVMvalueStr(exe, line_str);

            FklVMvalue *r = fklCreateVMvalueVecExt(exe,
                    3,
                    num_val,
                    is_cur_line,
                    str_val);
            FKL_CPROC_RETURN(exe, ctx, r);
        }
    } else if (dctx->curlist_line <= dctx->curfile_lines->v.size) {
        uint32_t curline_num = dctx->curlist_line;
        const FklString *line_str =
                dctx->curfile_lines->v.base[curline_num - 1];

        FklVMvalue *num_val = FKL_MAKE_VM_FIX(curline_num);
        FklVMvalue *is_cur_line =
                curline_num == dctx->curline ? FKL_VM_TRUE : FKL_VM_NIL;
        FklVMvalue *str_val = fklCreateVMvalueStr(exe, line_str);

        FklVMvalue *r = fklCreateVMvalueVecExt(exe, //
                3,
                num_val,
                is_cur_line,
                str_val);

        FKL_CPROC_RETURN(exe, ctx, r);
    } else
        FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    return 0;
}

static int bdb_debug_ctx_list_file_src(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 3);
    FklVMvalue *dctx_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *filename_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *line_num_obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FKL_CHECK_TYPE(dctx_obj, is_debug_ctx, exe);
    FKL_CHECK_TYPE(filename_obj, FKL_IS_STR, exe);
    FKL_CHECK_TYPE(line_num_obj, FKL_IS_FIX, exe);

    DebugCtx *dctx = as_dbg_ctx(dctx_obj);

    const FklString *filename = FKL_VM_STR(filename_obj);
    const FklStringVector *item = bdbGetSource(dctx, filename);
    int64_t line_num = FKL_GET_FIX(line_num_obj);
    if (line_num <= 0) {
        FKL_RAISE_BUILTIN_ERROR_FMT(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,
                exe,
                "Line number should not be less than 1")
    }

    if (item == NULL) {
        FKL_RAISE_BUILTIN_ERROR_FMT(FKL_ERR_FILEFAILURE,
                exe,
                "Failed for file %S",
                filename_obj);
    }

    if (FKL_TYPE_CAST(uint64_t, line_num) >= item->size) {
        FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    } else {
        uint32_t target_line = line_num;
        const FklString *line_str = item->base[target_line - 1];

        FklVMvalue *num_val = FKL_MAKE_VM_FIX(target_line);

        const FklString *cur_filename = bdbSymbol(dctx->curfile_lines->k);
        int file_name_eq = fklStringEqual(filename, cur_filename);

        FklVMvalue *is_cur_line = (file_name_eq && target_line == dctx->curline)
                                        ? FKL_VM_TRUE
                                        : FKL_VM_NIL;
        FklVMvalue *str_val = fklCreateVMvalueStr(exe, line_str);

        FklVMvalue *r = fklCreateVMvalueVecExt(exe, //
                3,
                num_val,
                is_cur_line,
                str_val);

        FKL_CPROC_RETURN(exe, ctx, r);
    }
    return 0;
}

static int bdb_debug_ctx_set_step_into(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *dctx_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(dctx_obj, is_debug_ctx, exe);
    DebugCtx *dctx = as_dbg_ctx(dctx_obj);

    int8_t done = dctx->done;
    if (done)
        debug_restart(dctx, exe);
    bdbSetStepIns(dctx,
            dctx->reached_thread,
            done ? BDB_STEP_INS_CUR : BDB_STEP_INS_NEXT,
            BDB_STEP_MODE_INTO,
            BDB_STEP_TYPE_LINE);
    FKL_CPROC_RETURN(exe, ctx, dctx_obj);
    return 0;
}

static int bdb_debug_ctx_set_step_over(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *dctx_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(dctx_obj, is_debug_ctx, exe);
    DebugCtx *dctx = as_dbg_ctx(dctx_obj);

    int8_t done = dctx->done;
    if (done)
        debug_restart(dctx, exe);
    bdbSetStepIns(dctx,
            dctx->reached_thread,
            done ? BDB_STEP_INS_CUR : BDB_STEP_INS_NEXT,
            BDB_STEP_MODE_OVER,
            BDB_STEP_TYPE_LINE);
    FKL_CPROC_RETURN(exe, ctx, dctx_obj);
    return 0;
}

static int bdb_debug_ctx_set_step_out(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *dctx_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(dctx_obj, is_debug_ctx, exe);
    DebugCtx *dctx = as_dbg_ctx(dctx_obj);

    if (dctx->done)
        debug_restart(dctx, exe);
    bdbSetStepOut(dctx);
    FKL_CPROC_RETURN(exe, ctx, dctx_obj);
    return 0;
}

static int bdb_debug_ctx_set_until(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 1, 2);
    FklVMvalue *dctx_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *lineno_obj = argc > 1 ? FKL_CPROC_GET_ARG(exe, ctx, 1) : NULL;
    FKL_CHECK_TYPE(dctx_obj, is_debug_ctx, exe);

    DebugCtx *dctx = as_dbg_ctx(dctx_obj);

    int8_t done = dctx->done;
    if (done)
        debug_restart(dctx, exe);
    if (lineno_obj == NULL)
        bdbSetStepIns(dctx,
                dctx->reached_thread,
                done ? BDB_STEP_INS_CUR : BDB_STEP_INS_NEXT,
                BDB_STEP_MODE_OVER,
                BDB_STEP_TYPE_LINE);
    else {
        FKL_CHECK_TYPE(lineno_obj, FKL_IS_FIX, exe);
        int64_t line = FKL_GET_FIX(lineno_obj);
        if (line < ((int64_t)dctx->curline))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE, exe);
        bdbSetStepUntil(dctx, line);
    }
    FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    return 0;
}

static int bdb_debug_ctx_list_ins(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 1, 2);
    FklVMvalue *dctx_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *pc_num_obj = argc > 1 ? FKL_CPROC_GET_ARG(exe, ctx, 1) : NULL;
    FKL_CHECK_TYPE(dctx_obj, is_debug_ctx, exe);

    DebugCtx *dctx = as_dbg_ctx(dctx_obj);

    uint64_t cur_pc = 0;
    BdbWrapper proc = bdbUpdateCurProc(dctx, &cur_pc);
    if (!bdbHas(proc)) {
        FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
        return 0;
    }

    const FklByteCode *const bc = &bdbBcl(proc)->bc;
    if (pc_num_obj) {
        FKL_CHECK_TYPE(pc_num_obj, fklIsVMint, exe);
        if (fklIsVMnumberLt0(pc_num_obj)) {
            FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
            return 0;
        }
        uint64_t ins_pc = fklVMgetUint(pc_num_obj);
        if (ins_pc >= bc->len) {
            FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
            return 0;
        }
        int is_cur_pc = cur_pc == ins_pc;
        FklVMvalue *vec = bdbCreateInsVec(exe, dctx, is_cur_pc, ins_pc, proc);

        FKL_CPROC_RETURN(exe, ctx, vec);
    } else if (dctx->cur_ins_pc < bc->len) {
        uint64_t ins_pc = dctx->cur_ins_pc;
        int is_cur_pc = cur_pc == ins_pc;
        FklVMvalue *vec = bdbCreateInsVec(exe, dctx, is_cur_pc, ins_pc, proc);
        FKL_CPROC_RETURN(exe, ctx, vec);
    } else {
        FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    }
    return 0;
}

static int bdb_debug_ctx_set_list_ins(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *dctx_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *pc_num_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(dctx_obj, is_debug_ctx, exe);

    DebugCtx *dctx = as_dbg_ctx(dctx_obj);

    FKL_CHECK_TYPE(pc_num_obj, fklIsVMint, exe);

    BdbWrapper proc = bdbUpdateCurProc(dctx, NULL);

    if (bdbHas(proc) && !fklIsVMnumberLt0(pc_num_obj)) {
        uint64_t ins_pc = fklVMgetUint(pc_num_obj);
        dctx->cur_ins_pc = ins_pc;
    }
    FKL_CPROC_RETURN(exe, ctx, dctx_obj);
    return 0;
}

static int bdb_debug_ctx_get_cur_ins(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *dctx_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(dctx_obj, is_debug_ctx, exe);

    DebugCtx *dctx = as_dbg_ctx(dctx_obj);

    uint64_t cur_pc = 0;
    BdbWrapper proc = bdbUpdateCurProc(dctx, &cur_pc);

    if (bdbHas(proc)) {
        const FklByteCode *const bc = &bdbBcl(proc)->bc;
        if (cur_pc >= bc->len) {
            FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
            return 0;
        }

        FklVMvalue *vec = bdbCreateInsVec(exe, dctx, 1, cur_pc, proc);
        FKL_CPROC_RETURN(exe, ctx, vec);
    } else
        FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    return 0;
}

static int bdb_debug_ctx_set_step_ins(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *dctx_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(dctx_obj, is_debug_ctx, exe);
    DebugCtx *dctx = as_dbg_ctx(dctx_obj);

    bdbSetStepIns(dctx,
            dctx->reached_thread,
            BDB_STEP_INS_NEXT,
            BDB_STEP_MODE_INTO,
            BDB_STEP_TYPE_INS);
    FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    return 0;
}

static int bdb_debug_ctx_set_next_ins(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *dctx_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(dctx_obj, is_debug_ctx, exe);
    DebugCtx *dctx = as_dbg_ctx(dctx_obj);

    bdbSetStepIns(dctx,
            dctx->reached_thread,
            BDB_STEP_INS_NEXT,
            BDB_STEP_MODE_OVER,
            BDB_STEP_TYPE_INS);
    FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    return 0;
}

static int bdb_debug_ctx_eval(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *dctx_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *expression_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(dctx_obj, is_debug_ctx, exe);
    FKL_CHECK_TYPE(expression_obj, FKL_IS_STR, exe);
    DebugCtx *dctx = as_dbg_ctx(dctx_obj);

    if (dctx->done && dctx->reached_thread == NULL) {
        FKL_RAISE_BUILTIN_ERROR_FMT(FKL_ERR_THREADERROR,
                exe,
                "Program is already finishied");
    } else {
        FklVMframe *cur_frame = bdbGetCurrentFrame(dctx);
        for (; cur_frame && cur_frame->type == FKL_FRAME_OTHEROBJ;
                cur_frame = cur_frame->prev)
            ;

        if (cur_frame) {
            BdbEvalErr err = 0;
            BdbWrapper p = bdbCompileEvalExpression(dctx,
                    dctx->reached_thread,
                    FKL_VM_STR(expression_obj),
                    cur_frame,
                    &err);
            switch (err) {
            case BDB_EVAL_ERR_UNABLE:
                FKL_RAISE_BUILTIN_ERROR_FMT(FKL_ERR_SYNTAXERROR,
                        exe,
                        "Unable to evaluate in this context");
                break;
                break;
            case BDB_EVAL_ERR_IMPORT:
                FKL_RAISE_BUILTIN_ERROR_FMT(FKL_ERR_SYNTAXERROR,
                        exe,
                        "Import module in evaluation is not allow");
                break;
            case BDB_EVAL_ERR_INVALID_EXP:
                FKL_RAISE_BUILTIN_ERROR_FMT(FKL_ERR_INVALIDEXPR,
                        exe,
                        "Invalid evaluation expression: %s",
                        expression_obj);
                break;
            }

            FKL_ASSERT(bdbHas(p));
            FklVM *reached_thread = dctx->reached_thread;

            BdbWrapper ret;
            ret = bdbCallEvalProc(dctx, exe, reached_thread, p, cur_frame);

            FklVMvalue *retval = NULL;
            if (bdbIsError(ret)) {
                retval = as_bdb_pd(ctx->pd)->err_sym;
            } else {
                retval = bdbBoxStringify(exe, ret);
            }
            FKL_CPROC_RETURN(exe, ctx, retval);
            return 0;
        } else {
            FKL_RAISE_BUILTIN_ERROR_FMT(FKL_ERR_THREADERROR,
                    exe,
                    "Evaluating in current thread is not allowed");
        }
    }
    FKL_CPROC_RETURN(exe, ctx, dctx_obj);
    return 0;
}

static int bdb_debug_ctx_backtrace(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, is_debug_ctx, exe);
    DebugCtx *dctx = as_dbg_ctx(obj);

    FklVMvalue *l = bdbGetCurBacktrace(dctx, exe);
    if (l == NULL) {
        FKL_RAISE_BUILTIN_ERROR_FMT(FKL_ERR_INVALIDACCESS,
                exe,
                "Thread %s already exited",
                FKL_MAKE_VM_FIX(dctx->curthread_idx));
    }

    FKL_CPROC_RETURN(exe, ctx, l);
    return 0;
}

static int bdb_debug_ctx_error_info(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, is_debug_ctx, exe);
    DebugCtx *dctx = as_dbg_ctx(obj);

    FklVMvalue *retval = bdbErrInfo(dctx, exe);

    FKL_CPROC_RETURN(exe, ctx, retval);
    return 0;
}

static int bdb_debug_ctx_eval_backtrace(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, is_debug_ctx, exe);
    DebugCtx *dctx = as_dbg_ctx(obj);
    FklVMvalue *ret = dctx->backtrace_list;
    FKL_CPROC_RETURN(exe, ctx, ret);
    return 0;
}

static int bdb_debug_ctx_up(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, is_debug_ctx, exe);
    DebugCtx *dctx = as_dbg_ctx(obj);

    if (dctx->reached_thread
            && dctx->curframe_idx < dctx->reached_thread_frames.size) {
        dctx->curframe_idx++;
        FKL_CPROC_RETURN(exe, ctx, FKL_VM_TRUE);
    } else
        FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    return 0;
}

static int bdb_debug_ctx_down(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, is_debug_ctx, exe);
    DebugCtx *dctx = as_dbg_ctx(obj);

    if (dctx->reached_thread && dctx->curframe_idx > 1) {
        dctx->curframe_idx--;
        FKL_CPROC_RETURN(exe, ctx, FKL_VM_TRUE);
    } else
        FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    return 0;
}

static int bdb_debug_ctx_list_thread(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *prefix_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(obj, is_debug_ctx, exe);
    FKL_CHECK_TYPE(prefix_obj, FKL_IS_STR, exe);
    DebugCtx *dctx = as_dbg_ctx(obj);

    FklVMvalue *l = bdbListThreads(dctx, exe);
    FKL_CPROC_RETURN(exe, ctx, l);
    return 0;
}

static int bdb_debug_ctx_switch_thread(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *id_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(obj, is_debug_ctx, exe);
    FKL_CHECK_TYPE(id_obj, FKL_IS_FIX, exe);
    DebugCtx *dctx = as_dbg_ctx(obj);

    int64_t id = FKL_GET_FIX(id_obj);
    if (id > 0 && id <= (int64_t)dctx->threads.size) {
        bdbSwitchCurThread(dctx, FKL_GET_FIX(id_obj));
        FKL_CPROC_RETURN(exe, ctx, FKL_VM_TRUE);
    } else {
        FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    }
    return 0;
}

struct SymFunc {
    const char *sym;
    const FklVMvalue *v;
} exports_and_func[] = {
    // clang-format off
    {"debug-ctx?",                (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx?",                bdb_debug_ctx_p              )},
    {"make-debug-ctx",            (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("make-debug-ctx",            bdb_make_debug_ctx           )},
    {"debug-ctx-readline",        (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx-readline",        bdb_debug_ctx_readline       )},
    {"debug-ctx-get-curline",     (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx-get-curline",     bdb_debug_ctx_get_curline    )},
    {"debug-ctx-list-src",        (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx-list-src",        bdb_debug_ctx_list_src       )},
    {"debug-ctx-list-file-src",   (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx-list-file-src",   bdb_debug_ctx_list_file_src  )},
    {"debug-ctx-set-list-src",    (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx-set-list-src",    bdb_debug_ctx_set_list_src   )},

    {"debug-ctx-list-ins",        (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx-list-ins",        bdb_debug_ctx_list_ins       )},
    {"debug-ctx-set-list-ins",    (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx-set-list-ins",    bdb_debug_ctx_set_list_ins   )},
    {"debug-ctx-get-cur-ins",     (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx-get-cur-ins",     bdb_debug_ctx_get_cur_ins    )},

    {"debug-ctx-delete-break",    (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx-delete-break",    bdb_debug_ctx_delete_break   )},
    {"debug-ctx-enable-break",    (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx-enable-break",    bdb_debug_ctx_enable_break   )},
    {"debug-ctx-disable-break",   (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx-disable-break",   bdb_debug_ctx_disable_break  )},
    {"debug-ctx-set-break",       (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx-set-break",       bdb_debug_ctx_set_break      )},
    {"debug-ctx-list-break",      (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx-list-break",      bdb_debug_ctx_list_break     )},

    {"debug-ctx-set-step-over",   (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx-set-step-over",   bdb_debug_ctx_set_step_over  )},
    {"debug-ctx-set-step-into",   (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx-set-step-into",   bdb_debug_ctx_set_step_into  )},
    {"debug-ctx-set-step-out",    (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx-set-step-out",    bdb_debug_ctx_set_step_out   )},
    {"debug-ctx-set-step-ins",    (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx-set-step-ins",    bdb_debug_ctx_set_step_ins   )},
    {"debug-ctx-set-next-ins",    (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx-set-next-ins",    bdb_debug_ctx_set_next_ins   )},

    {"debug-ctx-set-until",       (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx-set-until",       bdb_debug_ctx_set_until      )},

    {"debug-ctx-continue",        (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx-continue",        bdb_debug_ctx_continue       )},
    {"debug-ctx-exit?",           (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx-exit?",           bdb_debug_ctx_exit_p         )},
    {"debug-ctx-done?",           (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx-done?",           bdb_debug_ctx_done_p         )},
    {"debug-ctx-exit",            (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx-exit",            bdb_debug_ctx_exit           )},
    {"debug-ctx-eval",            (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx-eval",            bdb_debug_ctx_eval           )},
    {"debug-ctx-restart",         (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx-restart",         bdb_debug_ctx_restart        )},

    {"debug-ctx-backtrace",       (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx-backtrace",       bdb_debug_ctx_backtrace      )},
    {"debug-ctx-error-info",      (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx-error-info",      bdb_debug_ctx_error_info     )},
    {"debug-ctx-eval-backtrace",  (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx-eval-backtrace",  bdb_debug_ctx_eval_backtrace )},

    {"debug-ctx-up",              (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx-up",              bdb_debug_ctx_up             )},
    {"debug-ctx-down",            (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx-down",            bdb_debug_ctx_down           )},

    {"debug-ctx-list-thread",     (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx-list-thread",     bdb_debug_ctx_list_thread    )},
    {"debug-ctx-switch-thread",   (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("debug-ctx-switch-thread",   bdb_debug_ctx_switch_thread  )},
    // clang-format on
};

static const size_t EXPORT_NUM =
        sizeof(exports_and_func) / sizeof(struct SymFunc);

FKL_DLL_EXPORT FklVMvalue **_fklExportSymbolInit(FklVM *vm, uint32_t *num) {
    *num = EXPORT_NUM;
    FklVMvalue **symbols =
            (FklVMvalue **)fklZmalloc(EXPORT_NUM * sizeof(FklVMvalue *));
    FKL_ASSERT(symbols);
    for (size_t i = 0; i < EXPORT_NUM; i++)
        symbols[i] = fklVMaddSymbolCstr(vm, exports_and_func[i].sym);
    return symbols;
}

FKL_DLL_EXPORT int _fklImportInit(FKL_IMPORT_DLL_INIT_FUNC_ARGS) {
    FKL_ASSERT(count == EXPORT_NUM);

    FklVMvalue *pd = fklCreateVMvalueUd(exe, &BdbPublicDataMetaTable, dll);
    init_bdb_public_data(as_bdb_pd(pd), exe);

    for (size_t i = 0; i < EXPORT_NUM; i++) {
        const FklVMvalue *v = exports_and_func[i].v;

        FklVMvalue *r = NULL;
        if (FKL_IS_CPROC(v)) {
            const FklVMvalueCproc *from = FKL_VM_CPROC(v);
            r = fklCreateVMvalueCproc(exe, from->func, dll, pd, from->name);
        }

        FKL_ASSERT(r);
        values[i] = r;
    }
    return 0;
}

FKL_CHECK_EXPORT_DLL_INIT_FUNC();
FKL_CHECK_IMPORT_DLL_INIT_FUNC();
