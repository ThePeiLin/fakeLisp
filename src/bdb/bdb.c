#include "bdb.h"

#include <fakeLisp/base.h>
#include <fakeLisp/builtin.h>
#include <fakeLisp/codegen.h>
#include <fakeLisp/common.h>
#include <fakeLisp/str_buf.h>
#include <fakeLisp/symbol.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>

#include <fakeLisp/ins_helper.h>

#include <string.h>

static void bdb_step_break_userdata_print(const FklVMvalue *ud,
        FklCodeBuilder *buf,
        FklVM *exe) {
    fklCodeBuilderPuts(buf, "#<break>");
}

static FklVMudMetaTable BdbStepBreakUserDataMetaTable = {
    .size = sizeof(FklVMvalueUd),
    .princ = bdb_step_break_userdata_print,
    .prin1 = bdb_step_break_userdata_print,
};

static const alignas(8) FklVMvalueUd BdbStepBreak = {
    .next_ = NULL,
    .gray_next_ = NULL,
    .mark_ = FKL_MARK_B,
    .type_ = FKL_TYPE_USERDATA,
    .mt_ = &BdbStepBreakUserDataMetaTable,
    .dll_ = NULL,
};

#define BDB_STEP_BREAK ((FklVMptr) & BdbStepBreak)

static inline void init_cmd_read_ctx(BdbCmdReadCtx *ctx) {
    fklInitStrBuf(&ctx->buf);
    fklAnalysisSymbolVectorInit(&ctx->symbols, 16);
    fklParseStateVectorInit(&ctx->states, 16);
    fklVMvaluePushState0ToStack(&ctx->states);
}

static void B_int3(FklVM *exe, const FklIns *ins);
static void B_int33(FklVM *exe, const FklIns *ins);

static inline FklIns get_step_target_ins(DebugCtx *debug_ctx,
        const FklIns *target) {
    FklIns r = { 0 };
    for (size_t i = 0; i < BDB_STEPPING_TARGET_INS_COUNT; ++i) {
        if (target == debug_ctx->stepping_ctx.target_ins[i]) {
            r = debug_ctx->stepping_ctx.ins[i];
        }
    }
    return r;
}

static void B_int3(FklVM *exe, const FklIns *ins) {
    DebugCtx *debug_ctx = FKL_CONTAINER_OF(exe->gc, DebugCtx, gc);

    BdbCodepoint *item = NULL;

    BdbInt3Flags flags = FKL_INS_uA(*ins);
    FklIns oins = { 0 };

    if ((flags & BDB_INT3_STEPPING)) {
        uint8_t is_at_breakpoint = (flags & BDB_INT3_STEP_AT_BP) != 0;
        BdbSteppingType stepping_type = (flags & BDB_INT3_STEP_LINE) != 0
                                              ? BDB_STEP_TYPE_LINE
                                              : BDB_STEP_TYPE_INS;

        oins = is_at_breakpoint
                     ? (item = bdbGetCodepoint(debug_ctx, ins))->origin_ins
                     : get_step_target_ins(debug_ctx, ins);

        if (debug_ctx->stepping_ctx.vm != exe || exe->is_single_thread) {
            goto reached_breakpoint;
        }

        const FklLntItem *ln = debug_ctx->stepping_ctx.ln;
        bdbUnsetStepping(debug_ctx);

        const FklIns *pc = ins;
        if (stepping_type == BDB_STEP_TYPE_INS) {
            const FklIns *cur = exe->top_frame->pc;
            exe->top_frame->pc = pc;

            // interrupt for stepping
            fklVMinterrupt(exe, BDB_STEP_BREAK, NULL);

            exe->top_frame->pc = cur;
        }

        fklVMexecuteInstruction(exe, OP(oins), &oins, exe->top_frame);

        if (flags & BDB_INT3_GET_NEXT_INS
                || (OP(*pc) == FKL_OP_DUMMY
                        && (FKL_INS_uA(*pc) & BDB_INT3_GET_NEXT_INS))) {
            bdbUnsetStepping(debug_ctx);
            debug_ctx->stepping_ctx.ln = ln;
            bdbSetStepIns(debug_ctx,
                    exe,
                    BDB_STEP_INS_CUR,
                    BDB_STEP_MODE_OVER,
                    stepping_type);
        }

        return;
    }

    item = bdbGetCodepoint(debug_ctx, ins);
    oins = item->origin_ins;

reached_breakpoint:
    if (exe->is_single_thread) {
        fklVMexecuteInstruction(exe, OP(oins), &oins, exe->top_frame);
        return;
    }

    exe->dummy_ins_func = B_int33;
    exe->top_frame->pc--;
    atomic_fetch_add(&item->bp->reached_count, 1);
    fklVMinterrupt(exe, FKL_VM_VAL(bdbCreateBpWrapper(exe, item->bp)), NULL);
}

static void B_int33(FklVM *exe, const FklIns *ins) {
    DebugCtx *debug_ctx = FKL_CONTAINER_OF(exe->gc, DebugCtx, gc);
    exe->dummy_ins_func = B_int3;

    const FklIns *oins = &bdbGetCodepoint(debug_ctx, ins)->origin_ins;
    fklVMexecuteInstruction(exe, OP(*oins), oins, exe->top_frame);
}

static void dbg_codegen_ctx_extra_mark(FklVMgc *gc, FklVMextraMarkArgs *arg) {
    DebugCtx *ctx = (DebugCtx *)arg;
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, ctx->glob_env), gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, ctx->eval_env), gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, ctx->eval_info), gc);

    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, ctx->cur_proc), gc);

    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, ctx->main_proc), gc);

    FklVMvalue **base = (FklVMvalue **)ctx->code_objs.base;
    FklVMvalue **last = &base[ctx->code_objs.size];
    for (; base < last; base++)
        fklVMgcToGray(*base, gc);

    for (BdbBpIdxHashMapNode *l = ctx->bt.idx_ht.first; l; l = l->next) {
        BdbBp *i = l->v;
        fklVMgcToGray(bdbUnwrap(i->cond_proc), gc);
        fklVMgcToGray(bdbUnwrap(i->cond_exp), gc);
        fklVMgcToGray(bdbUnwrap(i->filename), gc);
    }

    for (BdbBp *bp = ctx->bt.deleted_breakpoints; bp; bp = bp->next) {
        fklVMgcToGray(bdbUnwrap(bp->cond_exp), gc);
        fklVMgcToGray(bdbUnwrap(bp->cond_proc), gc);
        fklVMgcToGray(bdbUnwrap(bp->filename), gc);
    }

    base = gc->builtin_refs;
    last = &base[FKL_BUILTIN_SYMBOL_NUM];
    for (; base < last; base++)
        fklVMgcToGray(*base, gc);

    fklVMgcToGray(bdbUnwrap(ctx->error), gc);
}

static inline int load_source_code(FklStringVector *lines, const char *rp) {
    FILE *fp = fopen(rp, "r");
    if (fp == NULL)
        return 1;
    FKL_ASSERT(fp);
    fklStringVectorInit(lines, 16);
    FklStrBuf buffer;
    fklInitStrBuf(&buffer);
    while (!feof(fp)) {
        fklGetDelim(fp, &buffer, '\n');
        if (!buffer.index || buffer.buf[buffer.index - 1] != '\n')
            fklStrBufPutc(&buffer, '\n');
        FklString *cur_line = fklCreateString(buffer.index, buffer.buf);
        fklStringVectorPushBack2(lines, cur_line);
        buffer.index = 0;
    }
    fklUninitStrBuf(&buffer);
    fclose(fp);
    return 0;
}

static void src_vec_move(FklStringVector *to, const FklStringVector *from) {
    *to = *from;
    FklStringVector *f = FKL_TYPE_CAST(FklStringVector *, from);
    f->capacity = 0;
    f->base = NULL;
    f->size = 0;
}

static inline const FklString *
get_line(DebugCtx *ctx, const FklString *filename, uint32_t line) {
    if (!bdbHasSymbol(ctx, filename))
        return NULL;
    FklVMvalue *file_sym = fklVMaddSymbol(&ctx->gc.gcvm, filename);
    const BdbSourceCodeHashMapElm *item = ctx->curfile_lines;
    if (item == NULL)
        goto source_not_loaded;
    if (file_sym == bdbUnwrap(item->k) && line == ctx->curline)
        return ctx->curline_str;
    else if (file_sym == bdbUnwrap(item->k)) {
        if (line > item->v.size)
            return NULL;
        ctx->curline = line;
        ctx->curline_str = item->v.base[line - 1];
        return ctx->curline_str;
    }

source_not_loaded:
    item = bdbSourceCodeHashMapAt2(&ctx->source_code_table, bdbWrap(file_sym));
    if (item == NULL) {
        FklStrBuf full_path_buf = { 0 };
        fklInitStrBuf(&full_path_buf);
        fklStrBufConcatWithCstr(&full_path_buf,
                ctx->cg_ctx.main_file_real_path_dir);
        fklStrBufPuts(&full_path_buf, FKL_PATH_SEPARATOR_STR);
        fklStrBufConcatWithString(&full_path_buf, filename);

        // lines will be inited in `load_source_code`
        FklStringVector lines = { 0 };

        if (load_source_code(&lines, full_path_buf.buf)) {
            fklUninitStrBuf(&full_path_buf);
            return NULL;
        }

        // lines will be moved
        BdbWrapper key = bdbWrap(file_sym);

        {
            FklStringVector to = { 0 };
            src_vec_move(&to, &lines);
            item = bdbSourceCodeHashMapInsert(&ctx->source_code_table,
                    &key,
                    &to);
        }

        fklUninitStrBuf(&full_path_buf);
    }

    if (item && line <= item->v.size) {
        ctx->curlist_line = 1;
        ctx->curline = line;
        ctx->curfile_lines = item;
        ctx->curline_str = item->v.base[line - 1];
        return ctx->curline_str;
    }
    return NULL;
}

// 清理除对内建变量引用外的其他引用
static inline void clear_var_ref(const DebugCtx *dctx, BdbWrapper const v) {
    const FklVMvalueCgEnv *glob_env = dctx->glob_env;
    FklVMvalueProc *proc = FKL_VM_PROC(bdbUnwrap(v));
    FklVMvalueProto *pt = proc->proto;
    const FklVarRefDef *const refs = fklVMvalueProtoVarRefs(pt);
    for (uint32_t i = 0; i < proc->ref_count; ++i) {
        FklVMvalue *v = proc->closure[i];
        if (v == NULL)
            continue;

        FklSidScope key = { .sid = refs[i].sid, .scope = 1 };
        const FklSymDef *ref = fklSymDefHashMapGet(&glob_env->refs, &key);

        uint64_t cidx = FKL_GET_FIX(refs[i].cidx);

        // 当前的引用是对内建变量的引用
        if (ref != NULL && cidx == FKL_VAR_REF_INVALID_CIDX)
            continue;

        proc->closure[i] = NULL;
    }
}

static inline int compile_and_call_cond_exp(DebugCtx *ctx, //
        FklVM *vm,
        BdbBp *bp) {
    if (!bdbHas(bp->cond_proc)) {
        BdbEvalErr err;
        bp->is_errored = 0;
        BdbWrapper proc = bdbCompileEvalExpression1(ctx,
                vm,
                bp->cond_exp,
                vm->top_frame,
                &err);
        // XXX: pdb 不会打印条件断点的错误
        (void)err;
        if (bdbHas(proc)) {
            bp->cond_exp = BDB_NONE;
            bp->cond_proc = proc;
            goto compile_done;
        }

        bp->is_errored = 1;
    }

compile_done:
    if (bdbHas(bp->cond_proc)) {
        BdbWrapper cond_proc = bp->cond_proc;
        FklVMframe *btm_frame = vm->top_frame;

        fklVMfetchVarRef(vm, FKL_VM_PROC(bdbUnwrap(cond_proc)), btm_frame);
        BdbWrapper ret = bdbCallEvalProc(ctx, NULL, vm, cond_proc, btm_frame);

        clear_var_ref(ctx, cond_proc);

        if (bdbIsError(ret)) {
            return 1;
        }

        FklVMvalue *value = FKL_VM_BOX(bdbUnwrap(ret));

        if (value == FKL_VM_NIL)
            return 1;
    }

    return 0;
}

static void interrupt_queue_work_cb(FklVM *vm, void *a) {
    const BdbIntArg *arg = (const BdbIntArg *)a;
    DebugCtx *ctx = arg->ctx;
    if (ctx->reached_thread)
        return;
    if (arg->bp) {
        BdbBp *bp = arg->bp;
        atomic_fetch_sub(&bp->reached_count, 1);
        for (; bp; bp = bp->next) {
            if (bp->is_deleted || bp->is_disabled)
                continue;
            if (!bdbHas(bp->cond_exp))
                goto reach_breakpoint;

            if (bp->is_errored)
                goto reach_breakpoint;

            if (compile_and_call_cond_exp(ctx, vm, bp))
                continue;

        reach_breakpoint:
            ctx->reached_breakpoint = bp;
            bdbSetReachedThread(ctx, vm);
            get_line(ctx, FKL_VM_STR(bdbUnwrap(bp->filename)), bp->line);
            bp->count++;
            if (bp->is_temporary)
                bdbDeleteBp(ctx, bp->idx);
            longjmp(*ctx->jmpb, DBG_INTERRUPTED);
        }
    } else if (arg->ln) {
        const FklLntItem *ln = arg->ln;
        bdbSetReachedThread(ctx, vm);
        get_line(ctx, FKL_VM_SYM(ln->fid), ln->line);
        longjmp(*ctx->jmpb, DBG_INTERRUPTED);
    } else {
        ctx->error = arg->error;
        bdbSetReachedThread(ctx, vm);
        if (bdbHas(arg->error)) {
            longjmp(*ctx->jmpb, DBG_ERROR_OCCUR);
        } else {
            longjmp(*ctx->jmpb, DBG_INTERRUPTED);
        }
    }
}

static inline void dbg_interrupt(FklVM *exe, BdbIntArg *arg) {
    fklQueueWorkInIdleThread(exe, interrupt_queue_work_cb, arg);
}

static FklVMinterruptResult dbg_interrupt_handler(FklVM *exe,
        FklVMvalue *int_val,
        FklVMvalue **pv,
        void *arg) {
    FKL_ASSERT(int_val);

    DebugCtx *ctx = (DebugCtx *)arg;
    if (bdbIsBpWrapper(int_val)) {
        BdbBp *bp = bdbGetBp(int_val);
        BdbIntArg arg = {
            .bp = bp,
            .ctx = ctx,
        };
        dbg_interrupt(exe, &arg);
    } else if (int_val == BDB_STEP_BREAK) {
        BdbIntArg arg = {
            .ctx = ctx,
        };
        bdbUnsetStepping(ctx);
        dbg_interrupt(exe, &arg);
    } else if (fklIsVMvalueError(int_val)) {
        if (exe->is_single_thread) {
            return FKL_INT_NEXT;
        } else {
            BdbIntArg arg = {
                .error = bdbWrap(int_val),
                .ctx = ctx,
            };
            bdbUnsetStepping(ctx);
            dbg_interrupt(exe, &arg);
        }
    } else {
        return FKL_INT_NEXT;
    }
    return FKL_INT_DONE;
}

static inline int init_debug_compile_and_init_vm(DebugCtx *dctx,
        const char *filename) {
    FILE *fp = fopen(filename, "r");
    char *rp = fklRealpath(filename);

    FklVMgc *gc = &dctx->gc;
    FklCgCtx *ctx = &dctx->cg_ctx;
    fklInitCgCtx(ctx, fklGetDir(rp), &gc->gcvm);

    fklChdir(ctx->main_file_real_path_dir);
    FklVMvalueCgInfo *info = fklCreateVMvalueCgInfo(ctx,
            NULL,
            rp,
            &(FklCgInfoArgs){
                .is_lib = 1,
                .is_main = 1,
                .is_debugging = 1,
            });

    FklVMvalueCgEnv *main_env = ctx->main_env;
    fklZfree(rp);
    FklVMvalue *bc = fklGenExpressionCodeWithFp(ctx, fp, info, main_env);

    fklChdir(ctx->cwd);

    if (bc == NULL) {
        fklUninitCgCtx(ctx);
        fklUninitVMgc(gc);
        return 1;
    }

    FklVMvalueProto *proto = fklCreateVMvalueProto2(&gc->gcvm, ctx->main_env);
    fklPrintUndefinedRef(ctx->main_env->prev, &gc->err_out);

    FklVM *vm = fklCreateVMwithByteCode2(bc, &dctx->gc, proto, 0);
    bc = NULL;

    dctx->reached_thread = vm;

    vm->dummy_ins_func = B_int3;

    gc->main_thread = vm;
    fklVMpushInterruptHandler(gc,
            dbg_interrupt_handler,
            NULL,
            NULL,
            (FklVMextraMarkArgs *)dctx);
    fklVMthreadStart(vm, &gc->q);
    return 0;
}

static inline void set_argv_with_list(FklVMgc *gc, FklVMvalue *argv_list) {
    int argc = fklVMlistLength(argv_list);
    gc->argc = argc;
    if (argc == 0) {
        gc->argv = NULL;
    } else {
        char **argv = (char **)fklZmalloc(argc * sizeof(char *));
        FKL_ASSERT(argv);
        for (int i = 0; i < argc; i++) {
            argv[i] = fklStringToCstr(FKL_VM_STR(FKL_VM_CAR(argv_list)));
            argv_list = FKL_VM_CDR(argv_list);
        }
        gc->argv = argv;
    }
}

static inline void get_all_code_objs(DebugCtx *ctx) {
    fklValueVectorInit(&ctx->code_objs, 16);
    FklVMvalue *head = ctx->gc.head;
    for (; head; head = head->next_) {
        if (fklIsVMvalueCodeObj(head))
            fklValueVectorPushBack2(&ctx->code_objs, head);
    }
    head = ctx->reached_thread->obj_head;
    for (; head; head = head->next_) {
        if (fklIsVMvalueCodeObj(head))
            fklValueVectorPushBack2(&ctx->code_objs, head);
    }
}

int bdbInitDbgCtx(DebugCtx *ctx,
        FklVM *exe,
        const char *filename,
        FklVMvalue *argv) {
    ctx->backtrace_list = NULL;
    fklValueHashSetInit(&ctx->file_sid_set);
    FklVMgc *gc = &ctx->gc;
    fklInitVMgc(gc);
    if (init_debug_compile_and_init_vm(ctx, filename)) {
        fklUninitVMgc(gc);
        fklValueHashSetUninit(&ctx->file_sid_set);
        ctx->inited = 0;
        return -1;
    }

    bdbFrameVectorInit(&ctx->reached_thread_frames, 16);
    bdbThreadVectorInit(&ctx->threads, 16);

    bdbSetReachedThread(ctx, ctx->reached_thread);

    bdbSourceCodeHashMapInit(&ctx->source_code_table);
    get_all_code_objs(ctx);
    bdbInitBpTable(&ctx->bt);
    BdbPos pos = { 0 };
    int r = bdbGetCurLine(ctx, &pos);
    FKL_ASSERT(r);
    (void)r;
    ctx->curline_str = get_line(ctx, pos.filename, pos.line);

    ctx->cur_ins_pc = 0;
    ctx->cur_proc = FKL_VM_PROC(ctx->reached_thread->top_frame->proc);

    fklVMregisterExtraMarkFunc(gc,
            (FklVMextraMarkArgs *)ctx,
            dbg_codegen_ctx_extra_mark,
            NULL);

    ctx->glob_env = fklCreateVMvalueCgEnv(&ctx->cg_ctx,
            &(FklCgEnvCreateArgs){
                .prev_env = NULL,
                .prev_ms = NULL,
                .parent_scope = 1,
            });
    fklInitGlobCgEnv(ctx->glob_env, &ctx->gc.gcvm, 0);

    set_argv_with_list(&ctx->gc, argv);
    init_cmd_read_ctx(&ctx->read_ctx);

    memset(&ctx->stepping_ctx.ins[0], 0xff, sizeof(ctx->stepping_ctx.ins));
    ctx->main_proc = ctx->cur_proc;

    ctx->error = BDB_NONE;
    ctx->inited = 1;
    return 0;
}

static inline void uninit_cmd_read_ctx(BdbCmdReadCtx *ctx) {
    fklParseStateVectorUninit(&ctx->states);
    fklAnalysisSymbolVectorUninit(&ctx->symbols);
    fklUninitStrBuf(&ctx->buf);
}

void bdbExitDbgCtx(DebugCtx *ctx) {
    if (ctx->exit == 1)
        return;
    bdbUninitBpTable(&ctx->bt);
    FklVMgc *gc = &ctx->gc;
    if (ctx->running && ctx->reached_thread) {
        bdbSetAllThreadReadyToExit(ctx->reached_thread);
        bdbWaitAllThreadExit(ctx->reached_thread);
        ctx->running = 0;
        ctx->reached_thread = NULL;
    } else {
        fklDestroyAllVMs(gc->main_thread);
    }
    ctx->error = BDB_NONE;
    ctx->exit = 1;
}

void bdbUninitDbgCtx(DebugCtx *ctx) {
    if (!ctx->inited)
        return;
    FklVMgc *gc = &ctx->gc;
    fklVMunregisterExtraMarkFunc(gc, (FklVMextraMarkArgs *)ctx);

    ctx->backtrace_list = NULL;
    fklValueHashSetUninit(&ctx->file_sid_set);

    bdbUninitBpTable(&ctx->bt);
    bdbSourceCodeHashMapUninit(&ctx->source_code_table);
    fklValueVectorUninit(&ctx->code_objs);
    bdbThreadVectorUninit(&ctx->threads);
    bdbFrameVectorUninit(&ctx->reached_thread_frames);

    ctx->main_proc = NULL;
    ctx->glob_env = NULL;
    ctx->eval_env = NULL;
    ctx->eval_info = NULL;

    uninit_cmd_read_ctx(&ctx->read_ctx);
    fklUninitCgCtx(&ctx->cg_ctx);
    fklUninitVMgc(&ctx->gc);

    ctx->error = BDB_NONE;
    ctx->inited = 0;
}

static inline const FklLntItem *get_cur_frame_lnt(const FklVMframe *frame) {
    if (frame->type == FKL_FRAME_COMPOUND) {
        FklVMvalueProc *proc = FKL_VM_PROC(frame->proc);
        FklByteCodelnt *code = FKL_VM_CO(proc->bcl);
        return fklGetLntItem(code, frame->pc);
    }
    return NULL;
}

const FklStringVector *bdbGetSource(DebugCtx *dctx, const FklString *filename) {
    if (!bdbHasSymbol(dctx, filename))
        return NULL;
    FklVMvalue *file_sym = fklVMaddSymbol(&dctx->gc.gcvm, filename);
    return bdbSourceCodeHashMapGet2(&dctx->source_code_table,
            bdbWrap(file_sym));
}

const FklIns *bdbGetIns(DebugCtx *ctx,
        const FklString *filename,
        uint32_t line,
        BdbPutBpErrorType *err) {
    const FklStringVector *sc_item = bdbGetSource(ctx, filename);
    if (!sc_item) {
        *err = BDB_PUT_BP_FILE_INVALID;
        return NULL;
    }
    if (!line || line > sc_item->size) {
        *err = BDB_PUT_BP_AT_END_OF_FILE;
        return NULL;
    }

    FklIns *ins = NULL;
    for (; line <= sc_item->size; line++) {
        FklVMvalue **base = (FklVMvalue **)ctx->code_objs.base;
        FklVMvalue **const end = &base[ctx->code_objs.size];

        for (; base < end; base++) {
            FklVMvalue *cur = *base;
            FklByteCodelnt *bclnt = FKL_VM_CO(cur);
            FklLntItem *item = bclnt->l;
            FklLntItem *const end = &item[bclnt->ls];
            for (; item < end; item++) {
                if (fklStringEqual(FKL_VM_SYM(item->fid), filename)
                        && item->line == line) {
                    ins = &bclnt->bc.code[item->scp];
                    goto break_loop;
                }
            }
        }
    }
break_loop:
    if (ins)
        return ins;
    *err = BDB_PUT_BP_AT_END_OF_FILE;
    return NULL;
}

const char *bdbGetPutBpErrorMsg(BdbPutBpErrorType t) {
    static const char *msgs[] = {
        NULL,
        "end of file",
        "file is invalid",
        "the specified symbol is undefined or not a procedure",
    };
    return msgs[t];
}

int bdbHasSymbol(DebugCtx *ctx, const FklString *s) {
    return fklVMhasSymbol(&ctx->gc.gcvm, s);
}

FklVMvalue *bdbGetCurBacktrace(DebugCtx *ctx, FklVM *host_vm) {
    if (ctx->reached_thread_frames.size == 0)
        return NULL;
    FklVM *reached_thread = ctx->reached_thread;
    uint32_t top = ctx->reached_thread_frames.size;
    FklVMframe **base = ctx->reached_thread_frames.base;

    FklStrBuf buf = { 0 };
    fklInitStrBuf(&buf);

    FklCodeBuilder builder = { 0 };
    fklInitCodeBuilderStrBuf(&builder, &buf, NULL);

    FklVMvalue *retval = FKL_VM_NIL;
    FklVMvalue **ppcdr = &retval;
    for (uint32_t i = 0; i < top; i++) {
        fklStrBufClear(&buf);

        FklVMframe *cur = base[i];

        int is_cur = i + 1 == ctx->curframe_idx;

        fklPrintFrame(cur, reached_thread, &builder);

        FklVMvalue *frame_info =
                fklCreateVMvalueStr2(host_vm, buf.index, buf.buf);
        FklVMvalue *r = fklCreateVMvaluePair(host_vm,
                is_cur ? FKL_VM_TRUE : FKL_VM_NIL,
                frame_info);
        FklVMvalue *cur_pair = fklCreateVMvaluePair1(host_vm, r);
        *ppcdr = cur_pair;
        ppcdr = &FKL_VM_CDR(cur_pair);
    }

    fklUninitStrBuf(&buf);

    return retval;
}

FklVMvalue *bdbErrInfo(DebugCtx *dctx, FklVM *host_vm) {
    FKL_ASSERT(dctx->reached_thread);
    if (!bdbHas(dctx->error))
        return FKL_VM_NIL;

    FklStrBuf buf = { 0 };
    fklInitStrBuf(&buf);

    FklCodeBuilder builder = { 0 };
    fklInitCodeBuilderStrBuf(&builder, &buf, NULL);

    fklPrintIntruptInfo(bdbUnwrap(dctx->error), dctx->reached_thread, &builder);

    FklVMvalue *r = fklCreateVMvalueStr2(host_vm, buf.index, buf.buf);

    fklUninitStrBuf(&buf);

    return r;
}

FklVMframe *bdbGetCurrentFrame(DebugCtx *ctx) {
    if (ctx->reached_thread_frames.size) {
        FklVMframe **base = ctx->reached_thread_frames.base;
        FklVMframe *cur = base[ctx->curframe_idx - 1];
        return cur;
    }
    return NULL;
}

void bdbSetReachedThread(DebugCtx *ctx, FklVM *vm) {
    ctx->reached_thread = vm;
    ctx->curframe_idx = 1;
    ctx->reached_thread_frames.size = 0;
    for (FklVMframe *f = vm->top_frame; f; f = f->prev)
        bdbFrameVectorPushBack2(&ctx->reached_thread_frames, f);
    for (FklVMframe *f = vm->top_frame; f; f = f->prev) {
        if (f->type == FKL_FRAME_COMPOUND) {
            FklVMvalueProc *proc = FKL_VM_PROC(f->proc);
            if (proc != ctx->cur_proc) {
                ctx->cur_proc = proc;
                ctx->cur_ins_pc = 0;
            }
            break;
        }
    }
    ctx->curthread_idx = 1;
    ctx->threads.size = 0;
    bdbThreadVectorPushBack2(&ctx->threads, vm);
    for (FklVM *cur = vm->next; cur != vm; cur = cur->next)
        bdbThreadVectorPushBack2(&ctx->threads, cur);
}

void bdbSwitchCurThread(DebugCtx *ctx, uint32_t idx) {
    ctx->curthread_idx = idx;
    ctx->curframe_idx = 1;
    ctx->reached_thread_frames.size = 0;
    FklVM *vm = bdbGetCurThread(ctx);
    if (vm == NULL)
        return;
    ctx->reached_thread = vm;
    for (FklVMframe *f = vm->top_frame; f; f = f->prev)
        bdbFrameVectorPushBack2(&ctx->reached_thread_frames, f);
}

FklVM *bdbGetCurThread(DebugCtx *ctx) {
    if (ctx->threads.size)
        return (FklVM *)ctx->threads.base[ctx->curthread_idx - 1];
    return NULL;
}

void bdbSetAllThreadReadyToExit(FklVM *head) {
    fklSetThreadReadyToExit(head);
    for (FklVM *cur = head->next; cur != head; cur = cur->next)
        fklSetThreadReadyToExit(cur);
}

void bdbWaitAllThreadExit(FklVM *head) {
    FklVMgc *gc = head->gc;
    fklVMreleaseWq(gc);
    fklVMcontinueTheWorld(gc);
    fklVMidleLoop(gc);
    fklDestroyAllVMs(gc->main_thread);
}

void bdbRestartDebugging(DebugCtx *ctx) {
    FklVMgc *gc = &ctx->gc;
    ctx->error = BDB_NONE;
    dbg_codegen_ctx_extra_mark(gc, (FklVMextraMarkArgs *)ctx);

    fklVMgcCheck(&gc->gcvm, 1);

    // XXX: 卸载所有已加载的模块
    for (FklVMvalue *v = gc->head; v; v = v->next_) {
        if (!fklIsVMvalueLib(v))
            continue;
        FklVMvalueLib *l = fklVMvalueLib(v);
        atomic_store(&l->import_state, FKL_VM_LIB_NONE);
    }

    FklVMvalueProc *main_proc = ctx->main_proc;

    FklVM *main_thread = fklCreateVM(FKL_VM_VAL(main_proc), gc);
    ctx->reached_thread = main_thread;
    ctx->running = 0;
    ctx->done = 0;
    main_thread->dummy_ins_func = B_int3;
    gc->main_thread = main_thread;
    fklVMthreadStart(main_thread, &gc->q);
    bdbSetReachedThread(ctx, main_thread);
}

void bdbStringify(DebugCtx *ctx, BdbWrapper const v, FklCodeBuilder *build) {
    fklPrin1VMvalue2(bdbUnwrap(v), build, &ctx->gc.gcvm);
}

BdbWrapper bdbUpdateCurProc(DebugCtx *ctx, uint64_t *ppc) {
    FklVMvalueProc *proc = NULL;
    FklVMframe *curframe = bdbGetCurrentFrame(ctx);
    for (; curframe; curframe = curframe->prev) {
        if (curframe->type == FKL_FRAME_COMPOUND) {
            FklVMvalueProc *v = FKL_VM_PROC(curframe->proc);
            if (ctx->cur_proc != v) {
                ctx->cur_proc = v;
                ctx->cur_ins_pc = 0;
            }
            if (ppc) {
                const FklByteCode *const bc = &FKL_VM_CO(v->bcl)->bc;
                *ppc = curframe->pc - bc->code;
            }
            proc = v;
            break;
        }
    }
    return proc == NULL ? BDB_NONE : bdbWrap(FKL_VM_VAL(proc));
}

FklVMvalue *bdbCreateInsVec(FklVM *exe,
        DebugCtx *dctx,
        int is_cur_pc,
        uint64_t ins_pc,
        BdbWrapper proc) {
    const FklByteCode *const bc = &bdbBcl(proc)->bc;
    const FklIns *ins = &bc->code[ins_pc];
    FklVMvalue *num_val = fklMakeVMuint(ins_pc, exe);
    FklVMvalue *is_cur_ins = is_cur_pc ? FKL_VM_TRUE : FKL_VM_NIL;
    if (OP(*ins) == FKL_OP_DUMMY) {
        ins = &bdbGetCodepoint(dctx, ins)->origin_ins;
    }
    FklVMvalue *opcode_str = NULL;
    FklVMvalue *imm1 = NULL;
    FklVMvalue *imm2 = NULL;
    FklOpcode op = OP(*ins);
    FklOpcodeMode mode = fklGetOpcodeMode(op);
    FklInsArg arg;
    fklGetInsOpArgWithOp(op, ins, &arg);

    FklVMvalueProto *proto = FKL_VM_PROC(bdbUnwrap(proc))->proto;
    FklVMvalue *const *const konsts = fklVMvalueProtoConsts(proto);

    switch (op) {
    case FKL_OP_PUSH_CHAR:
        imm1 = FKL_MAKE_VM_CHR(arg.ux);
        break;
    case FKL_OP_PUSH_CONST: {
        FklStrBuf buf = { 0 };
        fklInitStrBuf(&buf);
        FklCodeBuilder builder = { 0 };
        fklInitCodeBuilderStrBuf(&builder, &buf, NULL);
        fklPrin1VMvalue2(konsts[arg.ux], &builder, &dctx->gc.gcvm);
        imm1 = fklCreateVMvalueStr2(exe, buf.index, buf.buf);
        fklUninitStrBuf(&buf);
    } break;
    case FKL_OP_DROP:
    case FKL_OP_PAIR:
    case FKL_OP_VEC:
    case FKL_OP_STR:
    case FKL_OP_BVEC:
    case FKL_OP_BOX:
    case FKL_OP_HASH: {
        FklStrBuf buf = { 0 };
        fklInitStrBuf(&buf);
        fklStrBufPuts(&buf, fklGetOpcodeName(op));
        fklStrBufPuts(&buf, "::");
        fklStrBufPuts(&buf, fklGetSubOpcodeName(op, arg.ix));
        opcode_str = fklCreateVMvalueStr2(exe, buf.index, buf.buf);
        fklUninitStrBuf(&buf);
        goto op_with_subop;
    } break;
    default:
        switch (mode) {
        case FKL_OP_MODE_I:
            break;
        case FKL_OP_MODE_IsA:
        case FKL_OP_MODE_IsB:
        case FKL_OP_MODE_IsC:
        case FKL_OP_MODE_IsBB:
        case FKL_OP_MODE_IsCCB:
            imm1 = fklMakeVMint(arg.ix, exe);
            break;

        case FKL_OP_MODE_IuB:
        case FKL_OP_MODE_IuC:
        case FKL_OP_MODE_IuBB:
        case FKL_OP_MODE_IuCCB:
            imm1 = fklMakeVMuint(arg.ux, exe);
            break;

        case FKL_OP_MODE_IsAuB:
            imm1 = fklMakeVMint(arg.ix, exe);
            imm2 = fklMakeVMuint(arg.uy, exe);
            break;
        case FKL_OP_MODE_IxAxB:
        case FKL_OP_MODE_IuAuB:
        case FKL_OP_MODE_IuCuC:
        case FKL_OP_MODE_IuCAuBB:
            imm1 = fklMakeVMuint(arg.ux, exe);
            imm2 = fklMakeVMuint(arg.uy, exe);
            break;
        }
        break;
    }
    FklVMvalue *retval = NULL;
    opcode_str = fklCreateVMvalueStr1(exe, fklGetOpcodeName(op));
    switch (mode) {
    case FKL_OP_MODE_I:
    op_with_subop:
        retval = fklCreateVMvalueVecExt(exe, //
                3,
                num_val,
                is_cur_ins,
                opcode_str);
        break;
    case FKL_OP_MODE_IsA:
    case FKL_OP_MODE_IuB:
    case FKL_OP_MODE_IsB:
    case FKL_OP_MODE_IuC:
    case FKL_OP_MODE_IsC:
    case FKL_OP_MODE_IuBB:
    case FKL_OP_MODE_IsBB:
    case FKL_OP_MODE_IuCCB:
    case FKL_OP_MODE_IsCCB:
        retval = fklCreateVMvalueVecExt(exe,
                4,
                num_val,
                is_cur_ins,
                opcode_str,
                imm1);
        break;
    case FKL_OP_MODE_IsAuB:
    case FKL_OP_MODE_IuAuB:
    case FKL_OP_MODE_IuCuC:
    case FKL_OP_MODE_IuCAuBB:

    case FKL_OP_MODE_IxAxB:
        retval = fklCreateVMvalueVecExt(exe,
                5,
                num_val,
                is_cur_ins,
                opcode_str,
                imm1,
                imm2);
        break;
    }
    return retval;
}

FklVMvalue *bdbBoxStringify(FklVM *vm, BdbWrapper const v) {
    return fklVMstringify(FKL_VM_BOX(bdbUnwrap(v)), vm, '1');
}

BdbWrapper bdbParse(DebugCtx *dctx, const FklString *s) {
    FklVMvalue *v = fklCreateAst(&dctx->gc.gcvm, s, NULL);
    return bdbWrap(v);
}

int bdbGetCurLine(DebugCtx *dctx, BdbPos *line) {
    FklVM *cur_thread = dctx->reached_thread;
    if (cur_thread == NULL)
        return 0;

    FklVMframe *frame = cur_thread->top_frame;
    for (; frame && frame->type == FKL_FRAME_OTHEROBJ; frame = frame->prev)
        ;
    if (frame == NULL) {
        return 0;
    }

    const FklLntItem *ln = get_cur_frame_lnt(frame);
    const FklString *line_str = get_line(dctx, FKL_VM_SYM(ln->fid), ln->line);

    line->str = line_str;
    line->filename = FKL_VM_SYM(ln->fid);
    line->line = ln->line;
    return 1;
}

static FKL_ALWAYS_INLINE const FklIns *assign_ins(const FklIns *to,
        const FklIns from) {
    *(FklIns *)to = from;
    return to;
}

void bdbInitBpTable(BdbBpTable *bt) {
    bdbBpInsHashMapInit(&bt->ins_ht);
    bdbBpIdxHashMapInit(&bt->idx_ht);
    bt->next_idx = 1;
    bt->deleted_breakpoints = NULL;
}

static inline void mark_breakpoint_deleted(BdbBp *bp, BdbBpTable *bt) {
    bp->cond_exp = BDB_NONE;
    bp->cond_proc = BDB_NONE;
    bp->filename = BDB_NONE;

    bp->is_errored = 0;
    bp->is_deleted = 1;

    bp->next_del = bt->deleted_breakpoints;
    bt->deleted_breakpoints = bp;
}

static inline void remove_breakpoint(BdbBp *bp, BdbBpTable *bt) {
    BdbBpInsHashMapElm *ins_item = bp->item;
    *(bp->pnext) = bp->next;
    if (bp->next)
        bp->next->pnext = bp->pnext;
    bp->pnext = NULL;

    if (ins_item->v.bp == NULL) {
        const FklIns *ins = ins_item->k;
        assign_ins(ins, ins_item->v.origin_ins);
        bdbBpInsHashMapDel2(&bt->ins_ht, ins);
    }
}

static inline void clear_breakpoint(BdbBpTable *bt) {
    for (BdbBpIdxHashMapNode *l = bt->idx_ht.first; l; l = l->next) {
        BdbBp *bp = l->v;
        mark_breakpoint_deleted(bp, bt);
        remove_breakpoint(bp, bt);
    }
}

static inline void destroy_all_deleted_breakpoint(BdbBpTable *bt) {
    BdbBp *bp = bt->deleted_breakpoints;
    while (bp) {
        BdbBp *cur = bp;
        bp = bp->next_del;
        cur->cond_exp = BDB_NONE;
        cur->cond_proc = BDB_NONE;
        cur->filename = BDB_NONE;
        fklZfree(cur);
    }
    bt->deleted_breakpoints = NULL;
}

void bdbUninitBpTable(BdbBpTable *bt) {
    clear_breakpoint(bt);
    bdbBpInsHashMapUninit(&bt->ins_ht);
    bdbBpIdxHashMapUninit(&bt->idx_ht);
    destroy_all_deleted_breakpoint(bt);
}

#include <fakeLisp/common.h>

FKL_VM_USER_DATA_DEFAULT_PRINT(bp_wrapper_print, "breakpoint-wrapper");

static FklVMudMetaTable BreakpointWrapperMetaTable = {
    .size = sizeof(FklVMvalueBpWrapper),
    .prin1 = bp_wrapper_print,
    .princ = bp_wrapper_print,
};

FklVMvalueBpWrapper *bdbCreateBpWrapper(FklVM *vm, BdbBp *bp) {
    FklVMvalueBpWrapper *r = (FklVMvalueBpWrapper *)fklCreateVMvalueUd(vm,
            &BreakpointWrapperMetaTable,
            NULL);

    r->bp = bp;
    return r;
}

int bdbIsBpWrapper(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v)
        && FKL_VM_UD(v)->mt_ == &BreakpointWrapperMetaTable;
}

static FKL_ALWAYS_INLINE FklVMvalueBpWrapper *as_bp_wrapper(
        const FklVMvalue *v) {
    FKL_ASSERT(bdbIsBpWrapper(v));
    return (FklVMvalueBpWrapper *)v;
}

BdbBp *bdbGetBp(const FklVMvalue *v) { return as_bp_wrapper(v)->bp; }

static FKL_ALWAYS_INLINE BdbBp *get_bp(DebugCtx *dctx, uint32_t idx) {
    BdbBp **i = bdbBpIdxHashMapGet2(&dctx->bt.idx_ht, idx);
    if (i)
        return *i;
    return NULL;
}

BdbBp *bdbDisableBp(DebugCtx *dctx, uint32_t idx) {
    BdbBp *bp = get_bp(dctx, idx);
    if (bp) {
        bp->is_disabled = 1;
        return bp;
    }
    return NULL;
}

BdbBp *bdbEnableBp(DebugCtx *dctx, uint32_t idx) {
    BdbBp *bp = get_bp(dctx, idx);
    if (bp) {
        bp->is_disabled = 0;
        return bp;
    }
    return NULL;
}

void bdbClearDeletedBp(DebugCtx *dctx) {
    BdbBpTable *bt = &dctx->bt;
    BdbBp **phead = &bt->deleted_breakpoints;
    while (*phead) {
        BdbBp *cur = *phead;
        if (atomic_load(&cur->reached_count))
            phead = &cur->next_del;
        else {
            remove_breakpoint(cur, bt);
            *phead = cur->next_del;
            cur->cond_exp = BDB_NONE;
            cur->cond_proc = BDB_NONE;
            cur->filename = BDB_NONE;
            fklZfree(cur);
        }
    }
}

BdbBp *bdbDeleteBp(DebugCtx *dctx, uint32_t idx) {
    BdbBp *bp = NULL;
    BdbBpTable *bt = &dctx->bt;
    if (bdbBpIdxHashMapErase(&bt->idx_ht, &idx, &bp, NULL)) {
        mark_breakpoint_deleted(bp, bt);
        return bp;
    }
    return NULL;
}

BdbCodepoint *bdbGetCodepoint(DebugCtx *dctx, const FklIns *ins) {
    return bdbBpInsHashMapGet2(&dctx->bt.ins_ht, FKL_TYPE_CAST(FklIns *, ins));
}

FklVMvalue *bdbCreateBpVec(FklVM *exe, DebugCtx *dctx, const BdbBp *bp) {
    BdbPos pos = bdbBpPos(bp);
    FklVMvalue *filename = fklCreateVMvalueStr(exe, pos.filename);
    FklVMvalue *line = FKL_MAKE_VM_FIX(pos.line);
    FklVMvalue *num = FKL_MAKE_VM_FIX(bp->idx);

    FklVMvalue *exp_str = FKL_VM_NIL;
    if (bdbHas(bp->cond_exp)) {
        FklStrBuf buf = { 0 };
        fklInitStrBuf(&buf);
        FklCodeBuilder builder = { 0 };
        fklInitCodeBuilderStrBuf(&builder, &buf, NULL);
        fklPrin1VMvalue2(bdbUnwrap(bp->cond_exp), &builder, &dctx->gc.gcvm);
        exp_str = fklCreateVMvalueStr2(exe, buf.index, buf.buf);
        fklUninitStrBuf(&buf);
    }

    FklVMvalue *r = fklCreateVMvalueVecExt(exe,
            6,
            num,
            filename,
            line,
            exp_str,
            fklMakeVMuint(bp->count, exe),
            fklCreateVMvaluePair(exe,
                    bp->is_disabled ? FKL_VM_NIL : FKL_VM_TRUE,
                    bp->is_temporary ? FKL_VM_TRUE : FKL_VM_NIL));
    return r;
}

BdbPos bdbBpPos(const BdbBp *bp) {
    BdbPos pos = { 0 };
    pos.filename = FKL_VM_SYM(bdbUnwrap(bp->filename));
    pos.line = bp->line;
    pos.str = NULL;

    return pos;
}

static inline BdbBp *make_breakpoint(BdbBpInsHashMapElm *item,
        BdbBpTable *bt,
        FklVMvalue *fid,
        uint32_t line) {
    BdbBp *bp = (BdbBp *)fklZcalloc(1, sizeof(BdbBp));
    FKL_ASSERT(bp);
    bp->filename = bdbWrap(fid);
    bp->line = line;
    bp->idx = bt->next_idx++;
    bp->item = item;
    if (item->v.bp)
        (item->v.bp)->pnext = &bp->next;
    bp->pnext = &item->v.bp;
    bp->next = item->v.bp;
    item->v.bp = bp;
    bdbBpIdxHashMapAdd2(&bt->idx_ht, bp->idx, bp);
    return bp;
}

static inline BdbBp *create_bp(DebugCtx *ctx,
        const FklString *filename,
        uint32_t line,
        const FklIns *ins) {
    FKL_ASSERT(bdbHasSymbol(ctx, filename));
    FklVMvalue *s = fklVMaddSymbol(&ctx->gc.gcvm, filename);
    BdbBpTable *bt = &ctx->bt;
    BdbBpInsHashMapElm *item = bdbBpInsHashMapInsert2(&bt->ins_ht,
            ins,
            (BdbCodepoint){ .origin_ins = *ins, .bp = NULL });
    item->v.bp = make_breakpoint(item, bt, s, line);
    assign_ins(ins, FKL_MAKE_INS_I(FKL_OP_DUMMY));
    return item->v.bp;
}

BdbBp *bdbPutBp(DebugCtx *ctx,
        const FklString *filename,
        uint32_t line,
        BdbPutBpErrorType *err) {
    const FklIns *ins = bdbGetIns(ctx, filename, line, err);
    if (ins == NULL)
        return NULL;
    return create_bp(ctx, filename, line, ins);
}

static inline BdbBp *put_breakpoint_with_pc(DebugCtx *ctx,
        uint64_t pc,
        const FklIns *ins,
        const FklLntItem *ln) {
    return create_bp(ctx,
            FKL_VM_SYM(ln->fid),
            ln->line,
            FKL_TYPE_CAST(FklIns *, ins));
}

static inline const FklLntItem *get_proc_start_line_number(
        const FklVMvalueProc *proc) {
    FklByteCodelnt *code = FKL_VM_CO(proc->bcl);
    return fklFindLntItem(proc->spc - code->bc.code, code->ls, code->l);
}

static inline FklVMvalue *find_local_var(DebugCtx *ctx,
        const FklString *func_name) {
    FKL_ASSERT(bdbHasSymbol(ctx, func_name));
    FklVM *cur_thread = ctx->reached_thread;
    if (cur_thread == NULL)
        cur_thread = ctx->gc.main_thread;
    FklVMframe *frame = cur_thread->top_frame;
    for (; frame && frame->type == FKL_FRAME_OTHEROBJ; frame = frame->prev)
        ;
    if (!frame)
        return NULL;

    const FklLntItem *ln = get_cur_frame_lnt(frame);
    FKL_ASSERT(ln);
    uint32_t scope = ln->scope;
    if (scope == 0)
        return NULL;
    FklVMvalueProto *proto = FKL_VM_PROC(frame->proc)->proto;
    FklVMvalueCgEnvWeakMap *weak_map = ctx->cg_ctx.proto_env_map;
    FklVMvalueCgEnv *env = fklVMvalueCgEnvWeakMapGet(weak_map, proto);
    if (env == NULL)
        return NULL;
    FklVMvalue *id = fklVMaddSymbol(&ctx->gc.gcvm, func_name);
    const FklSymDefHashMapElm *def;
    def = fklFindSymbolDef(id, scope, env);
    if (def)
        return FKL_VM_GET_ARG(cur_thread, frame, def->v.idx);
    return NULL;
}

static inline FklVMvalue *find_closure_var(DebugCtx *ctx,
        const FklString *func_name) {
    FKL_ASSERT(bdbHasSymbol(ctx, func_name));
    FklVM *cur_thread = ctx->reached_thread;
    if (cur_thread == NULL)
        cur_thread = ctx->gc.main_thread;
    FklVMframe *frame = cur_thread->top_frame;
    for (; frame && frame->type == FKL_FRAME_OTHEROBJ; frame = frame->prev)
        ;
    if (!frame)
        return NULL;
    FklVMvalueProc *proc = FKL_VM_PROC(frame->proc);
    FklVMvalueProto *pt = proc->proto;
    const FklVarRefDef *cur = fklVMvalueProtoVarRefs(pt);
    const FklVarRefDef *end = cur + pt->ref_count;
    size_t idx = 0;
    FklVMvalue *func_id = fklVMaddSymbol(&ctx->gc.gcvm, func_name);
    for (; cur < end; ++cur) {
        if (cur->sid == func_id)
            return *(FKL_VM_VAR_REF_GET(proc->closure[idx]));
        ++idx;
    }
    return NULL;
}

BdbBp *bdbPutBp1(DebugCtx *ctx, const FklString *func_name) {
    if (!bdbHasSymbol(ctx, func_name))
        return NULL;

    FklVMvalue *var_value = find_local_var(ctx, func_name);
    if (var_value == NULL)
        var_value = find_closure_var(ctx, func_name);
    if (var_value == NULL || !FKL_IS_PROC(var_value))
        return NULL;

    FklVMvalueProc *proc = FKL_VM_PROC(var_value);
    FklByteCodelnt *code = FKL_VM_CO(proc->bcl);
    uint64_t pc = proc->spc - code->bc.code;
    const FklLntItem *ln = get_proc_start_line_number(proc);
    return put_breakpoint_with_pc(ctx, pc, proc->spc, ln);
    return NULL;
}

typedef struct {
    size_t i;
    uint8_t flags;
} SetSteppingArgs;

static inline void set_stepping_target(struct SteppingCtx *stepping_ctx,
        const FklIns *target,
        const SetSteppingArgs *args) {
    stepping_ctx->ins[args->i] = *target;
    stepping_ctx->target_ins[args->i] = target;
    FklOpcode op = OP(*target);
    uint8_t flags = args->flags;
    if (op == FKL_OP_DUMMY)
        flags |= BDB_INT3_STEP_AT_BP;

    assign_ins(target, FKL_MAKE_INS_IuA(FKL_OP_DUMMY, flags));
}

static inline void set_step_ins(DebugCtx *ctx,
        FklVM *exe,
        BdbStepTargetPlacingType placing_type,
        BdbSteppingMode mode) {
    if (exe == NULL || exe->top_frame == NULL)
        return;
    uint8_t int3_flags = BDB_INT3_STEPPING;
    ctx->stepping_ctx.vm = exe;

    FklVMframe *f = exe->top_frame;
    for (; f && f->type == FKL_FRAME_OTHEROBJ; f = f->prev)
        ;
    if (f == NULL)
        return;
    int r = 0;
    const FklIns *ins[2] = { NULL, NULL };
    if (placing_type == BDB_STEP_INS_CUR) {
        ins[0] = f->pc;
        r = 1;
        goto set_target;
    }

    r = fklGetNextIns(f->pc, ins);
    if (r != 0)
        goto set_target;

    r = 1;
    if (fklIsCallIns(*(f->pc))) {
        FklVMvalue *proc = exe->base[exe->bp];
        if (mode == BDB_STEP_MODE_INTO && FKL_IS_PROC(proc)) {
            FklVMvalueProc *p = FKL_VM_PROC(proc);
            ins[0] = p->spc;
        } else {
            FklInsArg arg = { 0 };
            int8_t l = fklGetInsOpArg(f->pc, &arg);
            ins[0] = f->pc + l;
        }

    } else if (fklIsLoadLibIns(*(f->pc))) {
        FklInsArg arg = { 0 };
        int8_t l = fklGetInsOpArg(f->pc, &arg);
        FklVMvalueProto *proto = FKL_VM_PROC(f->proc)->proto;
        FklVMvalueLib *lib = fklVMvalueProtoUsedLibs(proto)[arg.ux];

        int state = atomic_load(&lib->import_state);
        if (mode == BDB_STEP_MODE_INTO && state == FKL_VM_LIB_NONE) {
            FklVMvalueProc *p = FKL_VM_PROC(lib->proc);
            ins[0] = p->spc;
        } else {
            ins[0] = f->pc + l;
        }
    } else if (f->ret_cb == NULL) {
        switch (f->mark) {
        case FKL_VM_COMPOUND_FRAME_MARK_RET:
            for (f = f->prev; f; f = f->prev) {
                if (f->type == FKL_FRAME_COMPOUND) {
                    ins[0] = f->pc;
                    break;
                }
            }
            if (ins[0] == NULL)
                r = 0;
            break;
        case FKL_VM_COMPOUND_FRAME_MARK_CALL:
            ins[0] = f->spc;
            break;
        }
    } else {
        int3_flags |= BDB_INT3_GET_NEXT_INS;
        ins[0] = f->pc;
    }

set_target:
    for (int i = 0; i < r; ++i) {
        uint8_t flags = int3_flags;

        set_stepping_target(&ctx->stepping_ctx,
                ins[i],
                &(SetSteppingArgs){
                    .i = i,
                    .flags = flags,
                });
    }
}

static inline void set_step_line(DebugCtx *ctx,
        FklVM *exe,
        BdbStepTargetPlacingType placing_type,
        BdbSteppingMode mode) {
    struct SteppingCtx *sctx = &ctx->stepping_ctx;
    if (exe == NULL || exe->top_frame == NULL)
        return;
    uint8_t int3_flags = BDB_INT3_STEPPING;
    ctx->stepping_ctx.vm = exe;

    FklVMframe *f = exe->top_frame;
    for (; f && f->type == FKL_FRAME_OTHEROBJ; f = f->prev)
        ;
    if (f == NULL)
        return;

    int r = 0;
    const FklIns *ins[2] = { NULL, NULL };
    const FklIns *cur_ins = f->pc;

    FklByteCodelnt *bcl = FKL_VM_CO(FKL_VM_PROC(f->proc)->bcl);
    const FklLntItem *ln = fklGetLntItem(bcl, cur_ins);

    if (placing_type == BDB_STEP_INS_CUR) {
        FKL_ASSERT(sctx->ln);
        if (sctx->ln->line == ln->line && sctx->ln->fid == ln->fid) {
            int3_flags |= BDB_INT3_STEP_LINE;
            int3_flags |= BDB_INT3_GET_NEXT_INS;
        }
        ins[0] = cur_ins;
        r = 1;
        goto set_target;
    }

    sctx->ln = ln;

    for (;;) {
        r = fklGetNextIns(cur_ins, ins);
        if (r != 1)
            break;
    get_next_line_ins:
        ln = fklGetLntItem(FKL_VM_CO(FKL_VM_PROC(f->proc)->bcl), ins[0]);
        if (sctx->ln->line != ln->line || sctx->ln->fid != ln->fid) {
            goto set_target;
        }
        cur_ins = ins[0];
    }

    FklIns tmp_ins = *cur_ins;

    if (OP(*cur_ins) == FKL_OP_DUMMY) {
        const FklIns oins = bdbGetCodepoint(ctx, cur_ins)->origin_ins;
        assign_ins(cur_ins, oins);

        r = fklGetNextIns(cur_ins, ins);
        assign_ins(cur_ins, tmp_ins);

        if (r == 1) {
            cur_ins = ins[0];
            goto get_next_line_ins;
        }
        tmp_ins = oins;
    }

    if (r == 2) {
        r = 1;
        ins[0] = cur_ins;
        int3_flags |= BDB_INT3_GET_NEXT_INS;
        int3_flags |= BDB_INT3_STEP_LINE;
        goto set_target;
    }
    r = 1;
    if (fklIsCallIns(tmp_ins)) {
        FklVMvalue *proc = exe->base[exe->bp];
        if (cur_ins != f->pc) {
            ins[0] = cur_ins;
            int3_flags |= BDB_INT3_GET_NEXT_INS;
            int3_flags |= BDB_INT3_STEP_LINE;
            goto set_target;
        }
        if (mode == BDB_STEP_MODE_INTO && FKL_IS_PROC(proc)) {
            FklVMvalueProc *p = FKL_VM_PROC(proc);
            ins[0] = p->spc;
            goto set_target;
        }

        FklInsArg arg = { 0 };
        int8_t l = fklGetInsOpArg(cur_ins, &arg);
        ins[0] = cur_ins + l;
        cur_ins = ins[0];
        goto get_next_line_ins;

    } else if (fklIsLoadLibIns(tmp_ins)) {
        FklInsArg arg = { 0 };
        int8_t l = fklGetInsOpArg(cur_ins, &arg);
        FklVMvalueProto *proto = FKL_VM_PROC(f->proc)->proto;
        FklVMvalueLib *lib = fklVMvalueProtoUsedLibs(proto)[arg.ux];

        int state = atomic_load(&lib->import_state);
        if (cur_ins != f->pc) {
            ins[0] = cur_ins;
            int3_flags |= BDB_INT3_GET_NEXT_INS;
            int3_flags |= BDB_INT3_STEP_LINE;
            goto set_target;
        }
        if (mode == BDB_STEP_MODE_INTO && state == FKL_VM_LIB_NONE) {
            FklVMvalueProc *p = FKL_VM_PROC(lib->proc);
            ins[0] = p->spc;
            goto set_target;
        }
        ins[0] = cur_ins + l;
        cur_ins = ins[0];
        goto get_next_line_ins;

    } else if (f->ret_cb == NULL) {
        switch (f->mark) {
        case FKL_VM_COMPOUND_FRAME_MARK_RET:
            for (f = f->prev; f; f = f->prev) {
                if (f->type == FKL_FRAME_COMPOUND) {
                    ins[0] = f->pc;
                    break;
                }
            }
            if (ins[0] == NULL)
                r = 0;
            break;
        case FKL_VM_COMPOUND_FRAME_MARK_CALL:
            ins[0] = f->spc;
            break;
        }
    } else {
        int3_flags |= BDB_INT3_STEP_LINE;
        int3_flags |= BDB_INT3_GET_NEXT_INS;
        ins[0] = cur_ins;
    }

set_target:
    for (int i = 0; i < r; ++i) {
        uint8_t flags = int3_flags;

        set_stepping_target(&ctx->stepping_ctx,
                ins[i],
                &(SetSteppingArgs){
                    .i = i,
                    .flags = flags,
                });
    }
}

void bdbSetStepIns(DebugCtx *ctx,
        FklVM *exe,
        BdbStepTargetPlacingType placing_type,
        BdbSteppingMode mode,
        BdbSteppingType type) {
    switch (type) {
    case BDB_STEP_TYPE_INS:
        set_step_ins(ctx, exe, placing_type, mode);
        break;
    case BDB_STEP_TYPE_LINE:
        set_step_line(ctx, exe, placing_type, mode);
        break;
    }
}

void bdbSetStepOut(DebugCtx *ctx) {
    FklVM *exe = ctx->reached_thread;

    if (exe == NULL || exe->top_frame == NULL)
        return;
    ctx->stepping_ctx.vm = exe;
    FklVMframe *f = exe->top_frame;
    for (; f && f->type == FKL_FRAME_OTHEROBJ; f = f->prev)
        ;
    if (f == NULL)
        return;
    for (f = f->prev; f; f = f->prev) {
        if (f->type == FKL_FRAME_COMPOUND)
            break;
    }

    if (f == NULL)
        return;

    const FklIns *ins = f->pc;

    set_stepping_target(&ctx->stepping_ctx,
            ins,
            &(SetSteppingArgs){ .i = 0, .flags = BDB_INT3_STEPPING });
}

void bdbSetStepUntil(DebugCtx *ctx, uint32_t target_line) {
    FklVM *exe = ctx->reached_thread;

    if (exe == NULL || exe->top_frame == NULL)
        return;

    ctx->stepping_ctx.vm = exe;
    BdbPutBpErrorType err = 0;
    const FklString *s = bdbSymbol(ctx->curfile_lines->k);
    const FklIns *target = bdbGetIns(ctx, s, target_line, &err);
    if (target == NULL)
        return;

    set_stepping_target(&ctx->stepping_ctx,
            target,
            &(SetSteppingArgs){ .i = 0, .flags = BDB_INT3_STEPPING });
}

void bdbUnsetStepping(DebugCtx *ctx) {
    struct SteppingCtx *sctx = &ctx->stepping_ctx;
    sctx->ln = NULL;
    for (size_t i = 0; i < BDB_STEPPING_TARGET_INS_COUNT; ++i) {
        if (sctx->target_ins[i]) {
            assign_ins(sctx->target_ins[i], sctx->ins[i]);
            sctx->target_ins[i] = NULL;
        }
    }
    memset(&sctx->ins[0], 0xff, sizeof(sctx->ins));

    if (sctx->vm) {
        sctx->vm = NULL;
    }
}
