#include "bdb.h"

#include <fakeLisp/builtin.h>
#include <fakeLisp/vm.h>
#include <string.h>

static void bdb_step_break_userdata_as_print(const FklVMud *ud,
        FklStringBuffer *buf,
        FklVM *exe) {
    fklStringBufferConcatWithCstr(buf, "#<break>");
}

static FklVMudMetaTable BdbStepBreakUserDataMetaTable = {
    .size = 0,
    .__as_princ = bdb_step_break_userdata_as_print,
    .__as_prin1 = bdb_step_break_userdata_as_print,
};

const alignas(8) FklVMvalueUd BdbStepBreak = {
    .gc = NULL,
    .next = NULL,
    .gray_next = NULL,
    .mark = FKL_MARK_B,
    .type = FKL_TYPE_USERDATA,
    .ud = { .t = &BdbStepBreakUserDataMetaTable, .dll = NULL },
};

static inline void init_cmd_read_ctx(CmdReadCtx *ctx) {
    fklInitStringBuffer(&ctx->buf);
    fklAnalysisSymbolVectorInit(&ctx->symbolStack, 16);
    fklParseStateVectorInit(&ctx->stateStack, 16);
    fklUintVectorInit(&ctx->lineStack, 16);
    fklVMvaluePushState0ToStack(&ctx->stateStack);
}

static inline void replace_info_fid_with_realpath(FklCodegenInfo *info) {
    FklSid_t rpsid =
            fklAddSymbolCstr(info->realpath, info->runtime_symbol_table);
    info->fid = rpsid;
}

static void info_work_cb(FklCodegenInfo *info, void *ctx) {
    if (!info->is_macro) {
        DebugCtx *dctx = (DebugCtx *)ctx;
        replace_info_fid_with_realpath(info);
        fklSidHashSetPut2(&dctx->file_sid_set, info->fid);
    }
}

static void
create_env_work_cb(FklCodegenInfo *info, FklCodegenEnv *env, void *ctx) {
    if (!info->is_macro) {
        DebugCtx *dctx = (DebugCtx *)ctx;
        env->is_debugging = 1;
        putEnv(dctx, env);
    }
}

static void B_int3(FklVM *exe, const FklInstruction *ins);
static void B_int33(FklVM *exe, const FklInstruction *ins);

static inline FklInstruction get_step_target_ins(DebugCtx *debug_ctx,
        const FklInstruction *target) {
    FklInstruction r = { 0 };
    for (size_t i = 0; i < BDB_STEPPING_TARGET_INS_COUNT; ++i) {
        if (target == debug_ctx->stepping_ctx.target_ins[i]) {
            r = debug_ctx->stepping_ctx.ins[i];
        }
    }
    return r;
}

static void B_int3(FklVM *exe, const FklInstruction *ins) {
    DebugCtx *debug_ctx = FKL_CONTAINER_OF(exe->gc, DebugCtx, gc);

    BdbCodepoint *item = NULL;

    Int3Flags flags = ins->au;
    FklInstruction oins = { 0 };

    if ((flags & INT3_STEPPING)) {
        uint8_t is_at_breakpoint = (flags & INT3_STEP_AT_BP) != 0;
        SteppingType stepping_type =
                (flags & INT3_STEP_LINE) != 0 ? STEP_LINE : STEP_INS;

        oins = is_at_breakpoint ? (item = getBreakpointHashItem(debug_ctx, ins))
                                          ->origin_ins
                                : get_step_target_ins(debug_ctx, ins);

        if (debug_ctx->stepping_ctx.vm != exe || exe->is_single_thread) {
            goto reached_breakpoint;
        }

        const FklLineNumberTableItem *ln = debug_ctx->stepping_ctx.ln;
        unsetStepping(debug_ctx);

        FklInstruction *pc = FKL_REMOVE_CONST(FklInstruction, ins);
        if (stepping_type == STEP_INS) {
            FklInstruction *cur = exe->top_frame->c.pc;
            exe->top_frame->c.pc = pc;

            // interrupt for stepping
            fklVMinterrupt(exe, BDB_STEP_BREAK, NULL);

            exe->top_frame->c.pc = cur;
        }

        fklVMexecuteInstruction(exe, oins.op, &oins, exe->top_frame);

        if (flags & INT3_GET_NEXT_INS
                || (pc->op == FKL_OP_DUMMY && (pc->au & INT3_GET_NEXT_INS))) {
            unsetStepping(debug_ctx);
            debug_ctx->stepping_ctx.ln = ln;
            setStepIns(debug_ctx, exe, STEP_INS_CUR, STEP_OVER, stepping_type);
        }

        return;
    }

    item = getBreakpointHashItem(debug_ctx, ins);
    oins = item->origin_ins;

reached_breakpoint:
    if (exe->is_single_thread) {
        fklVMexecuteInstruction(exe, oins.op, &oins, exe->top_frame);
        return;
    }

    exe->dummy_ins_func = B_int33;
    exe->top_frame->c.pc--;
    atomic_fetch_add(&item->bp->reached_count, 1);
    fklVMinterrupt(exe, createBreakpointWrapper(exe, item->bp), NULL);
}

static void B_int33(FklVM *exe, const FklInstruction *ins) {
    DebugCtx *debug_ctx = FKL_CONTAINER_OF(exe->gc, DebugCtx, gc);
    exe->dummy_ins_func = B_int3;

    const FklInstruction *oins =
            &getBreakpointHashItem(debug_ctx, ins)->origin_ins;
    fklVMexecuteInstruction(exe, oins->op, oins, exe->top_frame);
}

static inline int init_debug_compile_and_init_vm(DebugCtx *ctx,
        const char *filename) {
    FILE *fp = fopen(filename, "r");
    char *rp = fklRealpath(filename);
    FklCodegenOuterCtx *outer_ctx = &ctx->outer_ctx;
    FklCodegenInfo codegen = { .fid = 0 };
    fklInitCodegenOuterCtx(outer_ctx, fklGetDir(rp));
    FklSymbolTable *pst = &outer_ctx->public_symbol_table;
    FklConstTable *pkt = &outer_ctx->public_kt;
    fklAddSymbolCstr(filename, pst);
    FklCodegenEnv *main_env = fklInitGlobalCodegenInfo(&codegen,
            rp,
            pst,
            pkt,
            0,
            outer_ctx,
            info_work_cb,
            create_env_work_cb,
            ctx);
    fklZfree(rp);
    FklByteCodelnt *mainByteCode =
            fklGenExpressionCodeWithFp(fp, &codegen, main_env);
    if (mainByteCode == NULL) {
        fklDestroyCodegenEnv(main_env);
        fklUninitCodegenInfo(&codegen);
        fklUninitCodegenOuterCtx(outer_ctx);
        return 1;
    }
    fklUpdatePrototype(codegen.pts,
            main_env,
            codegen.runtime_symbol_table,
            pst);
    fklDestroyCodegenEnv(main_env);
    fklPrintUndefinedRef(codegen.global_env, codegen.runtime_symbol_table, pst);

    FklCodegenLibVector *script_libraries = codegen.libraries;
    fklInitVMgc(&ctx->gc,
            codegen.runtime_symbol_table,
            codegen.runtime_kt,
            codegen.pts,
            0,
            NULL);

    FklVM *anotherVM = fklCreateVMwithByteCode2(mainByteCode, &ctx->gc, 1, 0);
    mainByteCode = NULL;

    codegen.runtime_symbol_table = NULL;
    codegen.pts = NULL;

    FklVMgc *gc = anotherVM->gc;
    gc->lib_num = script_libraries->size;
    gc->libs = (FklVMlib *)fklZcalloc((script_libraries->size + 1),
            sizeof(FklVMlib));
    FKL_ASSERT(gc->libs);

    while (!fklCodegenLibVectorIsEmpty(script_libraries)) {
        FklVMlib *curVMlib = &gc->libs[script_libraries->size];
        FklCodegenLib *cur =
                fklCodegenLibVectorPopBackNonNull(script_libraries);
        FklCodegenLibType type = cur->type;
        fklInitVMlibWithCodegenLibMove(cur,
                curVMlib,
                anotherVM,
                anotherVM->pts);
        if (type == FKL_CODEGEN_LIB_SCRIPT)
            fklInitMainProcRefs(anotherVM, curVMlib->proc);
    }
    fklUninitCodegenInfo(&codegen);
    fklChdir(outer_ctx->cwd);

    ctx->st = &outer_ctx->public_symbol_table;
    ctx->kt = &outer_ctx->public_kt;
    ctx->reached_thread = anotherVM;

    anotherVM->dummy_ins_func = B_int3;

    gc->main_thread = anotherVM;
    fklVMpushInterruptHandler(gc, dbgInterruptHandler, NULL, NULL, ctx);
    fklVMthreadStart(anotherVM, &gc->q);
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

static inline void load_source_code_to_source_code_hash_item(
        BdbSourceCodeHashMapElm *item,
        const char *rp) {
    FILE *fp = fopen(rp, "r");
    FKL_ASSERT(fp);
    FklStringVector *lines = &item->v;
    fklStringVectorInit(lines, 16);
    FklStringBuffer buffer;
    fklInitStringBuffer(&buffer);
    while (!feof(fp)) {
        fklGetDelim(fp, &buffer, '\n');
        if (!buffer.index || buffer.buf[buffer.index - 1] != '\n')
            fklStringBufferPutc(&buffer, '\n');
        FklString *cur_line = fklCreateString(buffer.index, buffer.buf);
        fklStringVectorPushBack2(lines, cur_line);
        buffer.index = 0;
    }
    fklUninitStringBuffer(&buffer);
    fclose(fp);
}

static inline void init_source_codes(DebugCtx *ctx) {
    bdbSourceCodeHashMapInit(&ctx->source_code_table);
    BdbSourceCodeHashMap *source_code_table = &ctx->source_code_table;
    for (FklSidHashSetNode *sid_list = ctx->file_sid_set.first; sid_list;
            sid_list = sid_list->next) {
        FklSid_t fid = sid_list->k;
        const FklString *str = fklGetSymbolWithId(fid, ctx->st);
        BdbSourceCodeHashMapElm *item =
                bdbSourceCodeHashMapInsert(source_code_table, &fid, NULL);
        load_source_code_to_source_code_hash_item(item, str->str);
    }
}

const FklString *getCurLineStr(DebugCtx *ctx, FklSid_t fid, uint32_t line) {
    if (fid == ctx->curline_file && line == ctx->curline)
        return ctx->curline_str;
    else if (fid == ctx->curline_file) {
        if (line > ctx->curfile_lines->size)
            return NULL;
        ctx->curline = line;
        ctx->curline_str = ctx->curfile_lines->base[line - 1];
        return ctx->curline_str;
    } else {
        const FklStringVector *item =
                bdbSourceCodeHashMapGet2(&ctx->source_code_table, fid);
        if (item && line <= item->size) {
            ctx->curlist_line = 1;
            ctx->curline_file = fid;
            ctx->curline = line;
            ctx->curfile_lines = item;
            ctx->curline_str = item->base[line - 1];
            return ctx->curline_str;
        }
        return NULL;
    }
}

static inline void get_all_code_objs(DebugCtx *ctx) {
    fklVMvalueVectorInit(&ctx->code_objs, 16);
    FklVMvalue *head = ctx->gc.head;
    for (; head; head = head->next) {
        if (fklIsVMvalueCodeObj(head))
            fklVMvalueVectorPushBack2(&ctx->code_objs, head);
    }
    head = ctx->reached_thread->obj_head;
    for (; head; head = head->next) {
        if (fklIsVMvalueCodeObj(head))
            fklVMvalueVectorPushBack2(&ctx->code_objs, head);
    }
}

static inline void internal_dbg_extra_mark(DebugCtx *ctx, FklVMgc *gc) {
    fklVMgcToGray(ctx->main_proc, gc);

    FklVMvalue **base = (FklVMvalue **)ctx->code_objs.base;
    FklVMvalue **last = &base[ctx->code_objs.size];
    for (; base < last; base++)
        fklVMgcToGray(*base, gc);

    for (BdbBpIdxHashMapNode *l = ctx->bt.idx_ht.first; l; l = l->next) {
        Breakpoint *i = l->v;
        if (i->is_compiled)
            fklVMgcToGray(i->proc, gc);
    }

    base = gc->builtin_refs;
    last = &base[FKL_BUILTIN_SYMBOL_NUM];
    for (; base < last; base++)
        fklVMgcToGray(*base, gc);
}

static void dbg_extra_mark(FklVMgc *gc, void *arg) {
    DebugCtx *ctx = (DebugCtx *)arg;
    internal_dbg_extra_mark(ctx, gc);
}

static inline void push_extra_mark_value(DebugCtx *ctx) {
    FklVM *vm = ctx->reached_thread;
    ctx->main_proc = vm->top_frame->c.proc;
    fklVMpushExtraMarkFunc(&ctx->gc, dbg_extra_mark, NULL, ctx);
}

int initDebugCtx(DebugCtx *ctx,
        FklVM *exe,
        const char *filename,
        FklVMvalue *argv) {
    bdbEnvHashMapInit(&ctx->envs);
    fklSidHashSetInit(&ctx->file_sid_set);
    if (init_debug_compile_and_init_vm(ctx, filename)) {
        bdbEnvHashMapUninit(&ctx->envs);
        fklSidHashSetUninit(&ctx->file_sid_set);
        ctx->inited = 0;
        return -1;
    }

    bdbFrameVectorInit(&ctx->reached_thread_frames, 16);
    bdbThreadVectorInit(&ctx->threads, 16);

    setReachedThread(ctx, ctx->reached_thread);
    init_source_codes(ctx);
    ctx->temp_proc_prototype_id = fklInsertEmptyFuncPrototype(ctx->gc.pts);
    get_all_code_objs(ctx);
    push_extra_mark_value(ctx);
    initBreakpointTable(&ctx->bt);
    const FklLineNumberTableItem *ln =
            getCurFrameLineNumber(ctx->reached_thread->top_frame);
    FKL_ASSERT(ln);
    ctx->curline_str = getCurLineStr(ctx, ln->fid, ln->line);

    ctx->curlist_ins_pc = 0;
    ctx->curlist_bytecode = ctx->reached_thread->top_frame->c.proc;

    ctx->glob_env = fklCreateCodegenEnv(NULL, 1, NULL);
    fklInitGlobCodegenEnv(ctx->glob_env, ctx->st);
    ctx->glob_env->refcount++;

    set_argv_with_list(&ctx->gc, argv);
    init_cmd_read_ctx(&ctx->read_ctx);

    memset(&ctx->stepping_ctx.ins[0], 0xff, sizeof(ctx->stepping_ctx.ins));
    ctx->inited = 1;
    return 0;
}

static inline void uninit_cmd_read_ctx(CmdReadCtx *ctx) {
    fklParseStateVectorUninit(&ctx->stateStack);
    fklAnalysisSymbolVectorUninit(&ctx->symbolStack);
    fklUintVectorUninit(&ctx->lineStack);
    fklUninitStringBuffer(&ctx->buf);
}

void exitDebugCtx(DebugCtx *ctx) {
    if (ctx->exit == 1)
        return;
    uninitBreakpointTable(&ctx->bt);
    FklVMgc *gc = &ctx->gc;
    if (ctx->running && ctx->reached_thread) {
        setAllThreadReadyToExit(ctx->reached_thread);
        waitAllThreadExit(ctx->reached_thread);
        ctx->running = 0;
        ctx->reached_thread = NULL;
    } else
        fklDestroyAllVMs(gc->main_thread);
    ctx->exit = 1;
}

void uninitDebugCtx(DebugCtx *ctx) {
    if (!ctx->inited)
        return;
    bdbEnvHashMapUninit(&ctx->envs);
    fklSidHashSetUninit(&ctx->file_sid_set);

    uninitBreakpointTable(&ctx->bt);
    bdbSourceCodeHashMapUninit(&ctx->source_code_table);
    ctx->main_proc = NULL;
    fklVMvalueVectorUninit(&ctx->code_objs);
    bdbThreadVectorUninit(&ctx->threads);
    bdbFrameVectorUninit(&ctx->reached_thread_frames);

    fklUninitVMgc(&ctx->gc);
    uninit_cmd_read_ctx(&ctx->read_ctx);
    fklUninitCodegenOuterCtx(&ctx->outer_ctx);
    fklDestroyCodegenEnv(ctx->glob_env);
    ctx->inited = 0;
}

const FklLineNumberTableItem *
getCurLineNumberItemWithCp(const FklInstruction *cp, FklByteCodelnt *code) {
    return fklFindLineNumTabNode(cp - code->bc.code, code->ls, code->l);
}

const FklLineNumberTableItem *getCurFrameLineNumber(const FklVMframe *frame) {
    if (frame->type == FKL_FRAME_COMPOUND) {
        FklVMproc *proc = FKL_VM_PROC(frame->c.proc);
        FklByteCodelnt *code = FKL_VM_CO(proc->codeObj);
        return getCurLineNumberItemWithCp(fklGetCompoundFrameCode(frame), code);
    }
    return NULL;
}

const FklStringVector *getSourceWithFid(DebugCtx *dctx, FklSid_t fid) {
    return bdbSourceCodeHashMapGet2(&dctx->source_code_table, fid);
}

FklInstruction *getInsWithFileAndLine(DebugCtx *ctx,
        FklSid_t fid,
        uint32_t line,
        PutBreakpointErrorType *err) {
    const FklStringVector *sc_item =
            bdbSourceCodeHashMapGet2(&ctx->source_code_table, fid);
    if (!sc_item) {
        *err = PUT_BP_FILE_INVALID;
        return NULL;
    }
    if (!line || line > sc_item->size) {
        *err = PUT_BP_AT_END_OF_FILE;
        return NULL;
    }

    FklInstruction *ins = NULL;
    for (; line <= sc_item->size; line++) {
        FklVMvalue **base = (FklVMvalue **)ctx->code_objs.base;
        FklVMvalue **const end = &base[ctx->code_objs.size];

        for (; base < end; base++) {
            FklVMvalue *cur = *base;
            FklByteCodelnt *bclnt = FKL_VM_CO(cur);
            FklLineNumberTableItem *item = bclnt->l;
            FklLineNumberTableItem *const end = &item[bclnt->ls];
            for (; item < end; item++) {
                if (item->fid == fid && item->line == line) {
                    ins = &bclnt->bc.code[item->scp];
                    goto break_loop;
                }
            }
        }
    }
break_loop:
    if (ins)
        return ins;
    *err = PUT_BP_AT_END_OF_FILE;
    return NULL;
}

Breakpoint *putBreakpointWithFileAndLine(DebugCtx *ctx,
        FklSid_t fid,
        uint32_t line,
        PutBreakpointErrorType *err) {
    FklInstruction *ins = getInsWithFileAndLine(ctx, fid, line, err);
    if (ins)
        return createBreakpoint(fid, line, ins, ctx);
    return NULL;
}

const char *getPutBreakpointErrorInfo(PutBreakpointErrorType t) {
    static const char *msgs[] = {
        NULL,
        "end of file",
        "file is invalid",
        "the specified symbol is undefined or not a procedure",
    };
    return msgs[t];
}

static inline FklVMvalue *find_local_var(DebugCtx *ctx, FklSid_t id) {
    FklVM *cur_thread = ctx->reached_thread;
    if (cur_thread == NULL)
        cur_thread = ctx->gc.main_thread;
    FklVMframe *frame = cur_thread->top_frame;
    for (; frame && frame->type == FKL_FRAME_OTHEROBJ; frame = frame->prev)
        ;
    if (!frame)
        return NULL;

    const FklLineNumberTableItem *ln = getCurFrameLineNumber(frame);
    FKL_ASSERT(ln);
    uint32_t scope = ln->scope;
    if (scope == 0)
        return NULL;
    uint32_t prototype_id = FKL_VM_PROC(frame->c.proc)->protoId;
    FklCodegenEnv *env = getEnv(ctx, prototype_id);
    if (env == NULL)
        return NULL;
    const FklSymDefHashMapElm *def =
            fklFindSymbolDefByIdAndScope(id, scope, env);
    if (def)
        return FKL_VM_GET_ARG(cur_thread, frame, def->v.idx);
    return NULL;
}

static inline FklVMvalue *find_closure_var(DebugCtx *ctx, FklSid_t id) {
    FklVM *cur_thread = ctx->reached_thread;
    if (cur_thread == NULL)
        cur_thread = ctx->gc.main_thread;
    FklVMframe *frame = cur_thread->top_frame;
    for (; frame && frame->type == FKL_FRAME_OTHEROBJ; frame = frame->prev)
        ;
    if (!frame)
        return NULL;
    FklVMproc *proc = FKL_VM_PROC(frame->c.proc);
    uint32_t prototype_id = proc->protoId;
    FklFuncPrototype *pt = &ctx->reached_thread->pts->pa[prototype_id];
    FklSymDefHashMapMutElm *def = pt->refs;
    FklSymDefHashMapMutElm *const end = &def[pt->rcount];
    for (; def < end; def++)
        if (def->k.id == id)
            return *(FKL_VM_VAR_REF_GET(proc->closure[def->v.idx]));
    return NULL;
}

static inline const FklLineNumberTableItem *get_proc_start_line_number(
        const FklVMproc *proc) {
    FklByteCodelnt *code = FKL_VM_CO(proc->codeObj);
    return fklFindLineNumTabNode(proc->spc - code->bc.code, code->ls, code->l);
}

static inline Breakpoint *put_breakpoint_with_pc(DebugCtx *ctx,
        uint64_t pc,
        FklInstruction *ins,
        const FklLineNumberTableItem *ln) {
    return createBreakpoint(ln->fid, ln->line, ins, ctx);
}

Breakpoint *putBreakpointForProcedure(DebugCtx *ctx, FklSid_t name_sid) {
    FklVMvalue *var_value = find_local_var(ctx, name_sid);
    if (var_value == NULL)
        var_value = find_closure_var(ctx, name_sid);
    if (var_value && FKL_IS_PROC(var_value)) {
        FklVMproc *proc = FKL_VM_PROC(var_value);
        FklByteCodelnt *code = FKL_VM_CO(proc->codeObj);
        uint64_t pc = proc->spc - code->bc.code;
        const FklLineNumberTableItem *ln = get_proc_start_line_number(proc);
        return put_breakpoint_with_pc(ctx, pc, proc->spc, ln);
    }
    return NULL;
}

void printBacktrace(DebugCtx *ctx, const FklString *prefix, FILE *fp) {
    if (ctx->reached_thread_frames.size) {
        FklVM *vm = ctx->reached_thread;
        uint32_t top = ctx->reached_thread_frames.size;
        FklVMframe **base = ctx->reached_thread_frames.base;
        for (uint32_t i = 0; i < top; i++) {
            FklVMframe *cur = base[i];
            if (i + 1 == ctx->curframe_idx)
                fklPrintString(prefix, fp);
            else
                for (uint32_t i = 0; i < prefix->size; i++)
                    fputc(' ', fp);
            fklPrintFrame(cur, vm, fp);
        }
    } else
        printThreadAlreadyExited(ctx, fp);
}

FklVMframe *getCurrentFrame(DebugCtx *ctx) {
    if (ctx->reached_thread_frames.size) {
        FklVMframe **base = ctx->reached_thread_frames.base;
        FklVMframe *cur = base[ctx->curframe_idx - 1];
        return cur;
    }
    return NULL;
}

void printCurFrame(DebugCtx *ctx, const FklString *prefix, FILE *fp) {
    if (ctx->reached_thread_frames.size) {
        FklVM *vm = ctx->reached_thread;
        FklVMframe *cur = getCurrentFrame(ctx);
        fklPrintString(prefix, fp);
        fklPrintFrame(cur, vm, fp);
    } else
        printThreadAlreadyExited(ctx, fp);
}

void setReachedThread(DebugCtx *ctx, FklVM *vm) {
    ctx->reached_thread = vm;
    ctx->curframe_idx = 1;
    ctx->reached_thread_frames.size = 0;
    for (FklVMframe *f = vm->top_frame; f; f = f->prev)
        bdbFrameVectorPushBack2(&ctx->reached_thread_frames, f);
    for (FklVMframe *f = vm->top_frame; f; f = f->prev) {
        if (f->type == FKL_FRAME_COMPOUND) {
            FklVMvalue *bytecode = FKL_VM_PROC(f->c.proc)->codeObj;
            if (bytecode != ctx->curlist_bytecode) {
                ctx->curlist_bytecode = bytecode;
                ctx->curlist_ins_pc = 0;
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

void listThreads(DebugCtx *ctx, const FklString *prefix, FILE *fp) {
    FklVM **base = (FklVM **)ctx->threads.base;
    uint32_t top = ctx->threads.size;
    for (uint32_t i = 0; i < top; i++) {
        FklVM *cur = base[i];
        if (i + 1 == ctx->curthread_idx)
            fklPrintString(prefix, fp);
        else
            for (uint32_t i = 0; i < prefix->size; i++)
                fputc(' ', fp);
        fprintf(fp, "thread %u ", i + 1);
        if (cur->top_frame)
            fklPrintFrame(cur->top_frame, cur, fp);
        else
            fputs("exited\n", fp);
    }
}

void switchCurThread(DebugCtx *ctx, uint32_t idx) {
    ctx->curthread_idx = idx;
    ctx->curframe_idx = 1;
    ctx->reached_thread_frames.size = 0;
    FklVM *vm = getCurThread(ctx);
    if (vm == NULL)
        return;
    ctx->reached_thread = vm;
    for (FklVMframe *f = vm->top_frame; f; f = f->prev)
        bdbFrameVectorPushBack2(&ctx->reached_thread_frames, f);
}

FklVM *getCurThread(DebugCtx *ctx) {
    if (ctx->threads.size)
        return (FklVM *)ctx->threads.base[ctx->curthread_idx - 1];
    return NULL;
}

void printThreadAlreadyExited(DebugCtx *ctx, FILE *fp) {
    fprintf(fp, "*** thread %u already exited ***\n", ctx->curthread_idx);
}

void printThreadCantEvaluate(DebugCtx *ctx, FILE *fp) {
    fprintf(fp,
            "*** can't evaluate expression in thread %u ***\n",
            ctx->curthread_idx);
}

void printUnableToCompile(FILE *fp) {
    fputs("*** can't compile expression in current frame ***\n", fp);
}

void printNotAllowImport(FILE *fp) {
    fputs("*** not allow to import lib in debug evaluation ***\n", fp);
}

void setAllThreadReadyToExit(FklVM *head) {
    fklSetThreadReadyToExit(head);
    for (FklVM *cur = head->next; cur != head; cur = cur->next)
        fklSetThreadReadyToExit(cur);
}

void waitAllThreadExit(FklVM *head) {
    FklVMgc *gc = head->gc;
    fklVMreleaseWq(gc);
    fklVMcontinueTheWorld(gc);
    fklVMidleLoop(gc);
    fklDestroyAllVMs(gc->main_thread);
}

void restartDebugging(DebugCtx *ctx) {
    FklVMgc *gc = &ctx->gc;
    internal_dbg_extra_mark(ctx, gc);
    for (uint64_t i = 1; i <= gc->lib_num; ++i) {
        FklVMlib *cur = &gc->libs[i];
        FklVMvalue *v = cur->proc;
        uint64_t ipc = cur->epc;

        fklVMgcToGray(v, gc);
        fklUninitVMlib(cur);
        fklInitVMlib(cur, v, ipc);
    }

    while (!fklVMgcPropagate(gc))
        ;
    FklVMvalue *white = NULL;
    fklVMgcCollect(gc, &white);
    fklVMgcSweep(white);
    fklVMgcUpdateThreshold(gc);

    FklVMvalue *main_proc = ctx->main_proc;

    FklVM *main_thread = fklCreateVM(main_proc, gc);
    ctx->reached_thread = main_thread;
    ctx->running = 0;
    ctx->done = 0;
    main_thread->dummy_ins_func = B_int3;
    gc->main_thread = main_thread;
    fklVMthreadStart(main_thread, &gc->q);
    setReachedThread(ctx, main_thread);
}
