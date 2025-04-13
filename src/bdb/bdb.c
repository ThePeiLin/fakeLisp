#include "bdb.h"

#include <fakeLisp/builtin.h>
#include <string.h>

static inline void init_cmd_read_ctx(CmdReadCtx *ctx) {
    ctx->replxx = replxx_init();
    fklInitStringBuffer(&ctx->buf);
    fklAnalysisSymbolVectorInit(&ctx->symbolStack, 16);
    fklParseStateVectorInit(&ctx->stateStack, 16);
    fklUintVectorInit(&ctx->lineStack, 16);
    fklVMvaluePushState0ToStack(&ctx->stateStack);
}

static inline void replace_info_fid_with_realpath(FklCodegenInfo *info) {
    FklSid_t rpsid =
        fklAddSymbolCstr(info->realpath, info->runtime_symbol_table)->v;
    info->fid = rpsid;
}

static void info_work_cb(FklCodegenInfo *info, void *ctx) {
    if (!info->is_macro) {
        DebugCtx *dctx = (DebugCtx *)ctx;
        replace_info_fid_with_realpath(info);
        fklSidTablePut2(&dctx->file_sid_set, info->fid);
    }
}

static void create_env_work_cb(FklCodegenInfo *info, FklCodegenEnv *env,
                               void *ctx) {
    if (!info->is_macro) {
        DebugCtx *dctx = (DebugCtx *)ctx;
        putEnv(dctx, env);
    }
}

static void B_int3(FKL_VM_INS_FUNC_ARGL);
static void B_int33(FKL_VM_INS_FUNC_ARGL);

static void B_int3(FKL_VM_INS_FUNC_ARGL) {
    BdbCodepoint *item = getBreakpointHashItem(exe->debug_ctx, ins);
    if (exe->is_single_thread) {
        FklOpcode origin_op = item->origin_op;
        fklVMexecuteInstruction(exe, origin_op, ins, exe->top_frame);
    } else {
        exe->dummy_ins_func = B_int33;
        exe->top_frame->c.pc--;
        atomic_fetch_add(&item->bp->reached_count, 1);
        fklVMinterrupt(exe, createBreakpointWrapper(exe, item->bp), NULL);
    }
}

static void B_int33(FKL_VM_INS_FUNC_ARGL) {
    exe->dummy_ins_func = B_int3;
    fklVMexecuteInstruction(
        exe, getBreakpointHashItem(exe->debug_ctx, ins)->origin_op, ins,
        exe->top_frame);
}

static inline int init_debug_codegen_outer_ctx(DebugCtx *ctx,
                                               const char *filename) {
    FILE *fp = fopen(filename, "r");
    char *rp = fklRealpath(filename);
    FklCodegenOuterCtx *outer_ctx = &ctx->outer_ctx;
    FklCodegenInfo codegen = {.fid = 0};
    fklInitCodegenOuterCtx(outer_ctx, fklGetDir(rp));
    FklSymbolTable *pst = &outer_ctx->public_symbol_table;
    FklConstTable *pkt = &outer_ctx->public_kt;
    fklAddSymbolCstr(filename, pst);
    FklCodegenEnv *main_env =
        fklInitGlobalCodegenInfo(&codegen, rp, pst, pkt, 0, outer_ctx,
                                 info_work_cb, create_env_work_cb, ctx);
    free(rp);
    FklByteCodelnt *mainByteCode =
        fklGenExpressionCodeWithFp(fp, &codegen, main_env);
    if (mainByteCode == NULL) {
        fklDestroyCodegenEnv(main_env);
        fklUninitCodegenInfo(&codegen);
        fklUninitCodegenOuterCtx(outer_ctx);
        return 1;
    }
    fklUpdatePrototype(codegen.pts, main_env, codegen.runtime_symbol_table,
                       pst);
    fklDestroyCodegenEnv(main_env);
    fklPrintUndefinedRef(codegen.global_env, codegen.runtime_symbol_table, pst);

    FklCodegenLibVector *scriptLibStack = codegen.libStack;
    FklVM *anotherVM =
        fklCreateVMwithByteCode(mainByteCode, codegen.runtime_symbol_table,
                                codegen.runtime_kt, codegen.pts, 1);
    codegen.runtime_symbol_table = NULL;
    codegen.pts = NULL;
    anotherVM->libNum = scriptLibStack->size;
    anotherVM->libs =
        (FklVMlib *)calloc((scriptLibStack->size + 1), sizeof(FklVMlib));
    FKL_ASSERT(anotherVM->libs);

    FklVMgc *gc = anotherVM->gc;
    while (!fklCodegenLibVectorIsEmpty(scriptLibStack)) {
        FklVMlib *curVMlib = &anotherVM->libs[scriptLibStack->size];
        FklCodegenLib *cur = fklCodegenLibVectorPopBack(scriptLibStack);
        FklCodegenLibType type = cur->type;
        fklInitVMlibWithCodegenLibAndDestroy(cur, curVMlib, anotherVM,
                                             anotherVM->pts);
        if (type == FKL_CODEGEN_LIB_SCRIPT)
            fklInitMainProcRefs(anotherVM, curVMlib->proc);
    }
    fklUninitCodegenInfo(&codegen);
    fklChdir(outer_ctx->cwd);

    ctx->st = &outer_ctx->public_symbol_table;
    ctx->kt = &outer_ctx->public_kt;
    ctx->gc = gc;
    ctx->reached_thread = anotherVM;

    anotherVM->dummy_ins_func = B_int3;
    anotherVM->debug_ctx = ctx;

    gc->main_thread = anotherVM;
    fklVMpushInterruptHandler(gc, dbgInterruptHandler, NULL, NULL, ctx);
    fklVMthreadStart(anotherVM, &gc->q);
    return 0;
}

static inline void set_argv_with_list(FklVMgc *gc, FklVMvalue *argv_list) {
    int argc = fklVMlistLength(argv_list);
    gc->argc = argc;
    char **argv = (char **)malloc(sizeof(char *) * argc);
    FKL_ASSERT(argv);
    for (int i = 0; i < argc; i++) {
        argv[i] = fklStringToCstr(FKL_VM_STR(FKL_VM_CAR(argv_list)));
        argv_list = FKL_VM_CDR(argv_list);
    }
    gc->argv = argv;
}

static inline void
load_source_code_to_source_code_hash_item(BdbSourceCodeTableElm *item,
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
    bdbSourceCodeTableInit(&ctx->source_code_table);
    BdbSourceCodeTable *source_code_table = &ctx->source_code_table;
    for (FklSidTableNode *sid_list = ctx->file_sid_set.first; sid_list;
         sid_list = sid_list->next) {
        FklSid_t fid = sid_list->k;
        const FklString *str = fklGetSymbolWithId(fid, ctx->st)->k;
        BdbSourceCodeTableElm *item =
            bdbSourceCodeTableInsert(source_code_table, &fid, NULL);
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
            bdbSourceCodeTableGet2(&ctx->source_code_table, fid);
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
    FklVMvalue *head = ctx->gc->head;
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
    FklVMvalue **base = (FklVMvalue **)ctx->extra_mark_value.base;
    FklVMvalue **last = &base[ctx->extra_mark_value.size];
    for (; base < last; base++)
        fklVMgcToGray(*base, gc);

    base = (FklVMvalue **)ctx->code_objs.base;
    last = &base[ctx->code_objs.size];
    for (; base < last; base++)
        fklVMgcToGray(*base, gc);

    for (BdbBpIdxTableNode *l = ctx->bt.idx_ht.first; l; l = l->next) {
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
    fklVMvalueVectorInit(&ctx->extra_mark_value, vm->libNum + 16);
    fklVMvalueVectorPushBack2(&ctx->extra_mark_value, vm->top_frame->c.proc);
    const uint64_t last = vm->libNum + 1;
    for (uint64_t i = 1; i < last; i++) {
        FklVMlib *cur = &vm->libs[i];
        fklVMvalueVectorPushBack2(&ctx->extra_mark_value, cur->proc);
    }
    fklVMpushExtraMarkFunc(ctx->gc, dbg_extra_mark, NULL, ctx);
}

DebugCtx *createDebugCtx(FklVM *exe, const char *filename, FklVMvalue *argv) {
    DebugCtx *ctx = (DebugCtx *)calloc(1, sizeof(DebugCtx));
    FKL_ASSERT(ctx);
    bdbEnvTableInit(&ctx->envs);
    fklSidTableInit(&ctx->file_sid_set);
    if (init_debug_codegen_outer_ctx(ctx, filename)) {
        bdbEnvTableUninit(&ctx->envs);
        fklSidTableUninit(&ctx->file_sid_set);
        free(ctx);
        return NULL;
    }

    bdbFrameVectorInit(&ctx->reached_thread_frames, 16);
    bdbThreadVectorInit(&ctx->threads, 16);

    setReachedThread(ctx, ctx->reached_thread);
    init_source_codes(ctx);
    ctx->temp_proc_prototype_id = fklInsertEmptyFuncPrototype(ctx->gc->pts);
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

    set_argv_with_list(ctx->gc, argv);
    init_cmd_read_ctx(&ctx->read_ctx);

    return ctx;
}

static inline void uninit_cmd_read_ctx(CmdReadCtx *ctx) {
    fklParseStateVectorUninit(&ctx->stateStack);
    fklAnalysisSymbolVectorUninit(&ctx->symbolStack);
    fklUintVectorUninit(&ctx->lineStack);
    fklUninitStringBuffer(&ctx->buf);
    replxx_end(ctx->replxx);
}

void exitDebugCtx(DebugCtx *ctx) {
    FklVMgc *gc = ctx->gc;
    if (ctx->running && ctx->reached_thread) {
        setAllThreadReadyToExit(ctx->reached_thread);
        waitAllThreadExit(ctx->reached_thread);
        ctx->running = 0;
        ctx->reached_thread = NULL;
    } else
        fklDestroyAllVMs(gc->main_thread);
}

void destroyDebugCtx(DebugCtx *ctx) {
    bdbEnvTableUninit(&ctx->envs);
    fklSidTableUninit(&ctx->file_sid_set);

    uninitBreakpointTable(&ctx->bt);
    bdbSourceCodeTableUninit(&ctx->source_code_table);
    fklVMvalueVectorUninit(&ctx->extra_mark_value);
    fklVMvalueVectorUninit(&ctx->code_objs);
    bdbThreadVectorUninit(&ctx->threads);
    bdbFrameVectorUninit(&ctx->reached_thread_frames);

    fklDestroyVMgc(ctx->gc);
    uninit_cmd_read_ctx(&ctx->read_ctx);
    fklUninitCodegenOuterCtx(&ctx->outer_ctx);
    fklDestroyCodegenEnv(ctx->glob_env);
    free(ctx);
}

const FklLineNumberTableItem *
getCurLineNumberItemWithCp(const FklInstruction *cp, FklByteCodelnt *code) {
    return fklFindLineNumTabNode(cp - code->bc->code, code->ls, code->l);
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
    return bdbSourceCodeTableGet2(&dctx->source_code_table, fid);
}

Breakpoint *putBreakpointWithFileAndLine(DebugCtx *ctx, FklSid_t fid,
                                         uint32_t line,
                                         PutBreakpointErrorType *err) {
    const FklStringVector *sc_item =
        bdbSourceCodeTableGet2(&ctx->source_code_table, fid);
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
                    ins = &bclnt->bc->code[item->scp];
                    goto break_loop;
                }
            }
        }
    }
break_loop:
    if (ins)
        return createBreakpoint(fid, line, ins, ctx);
    *err = PUT_BP_AT_END_OF_FILE;
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
    FklVMframe *frame = cur_thread->top_frame;
    for (; frame->type == FKL_FRAME_OTHEROBJ; frame = frame->prev)
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
    const FklSymDefTableElm *def = fklFindSymbolDefByIdAndScope(id, scope, env);
    if (def)
        return cur_thread->locv[def->v.idx];
    return NULL;
}

static inline FklVMvalue *find_closure_var(DebugCtx *ctx, FklSid_t id) {
    FklVM *cur_thread = ctx->reached_thread;
    FklVMframe *frame = cur_thread->top_frame;
    for (; frame->type == FKL_FRAME_OTHEROBJ; frame = frame->prev)
        ;
    if (!frame)
        return NULL;
    FklVMproc *proc = FKL_VM_PROC(frame->c.proc);
    uint32_t prototype_id = proc->protoId;
    FklFuncPrototype *pt = &ctx->reached_thread->pts->pa[prototype_id];
    FklSymDefTableMutElm *def = pt->refs;
    FklSymDefTableMutElm *const end = &def[pt->rcount];
    for (; def < end; def++)
        if (def->k.id == id)
            return *(FKL_VM_VAR_REF_GET(proc->closure[def->v.idx]));
    return NULL;
}

static inline const FklLineNumberTableItem *
get_proc_start_line_number(const FklVMproc *proc) {
    FklByteCodelnt *code = FKL_VM_CO(proc->codeObj);
    return fklFindLineNumTabNode(proc->spc - code->bc->code, code->ls, code->l);
}

static inline Breakpoint *
put_breakpoint_with_pc(DebugCtx *ctx, uint64_t pc, FklInstruction *ins,
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
        uint64_t pc = proc->spc - code->bc->code;
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
    fprintf(fp, "*** can't evaluate expression in thread %u ***\n",
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
    FklVMgc *gc = ctx->gc;
    internal_dbg_extra_mark(ctx, gc);
    while (!fklVMgcPropagate(gc))
        ;
    FklVMvalue *white = NULL;
    fklVMgcCollect(gc, &white);
    fklVMgcSweep(white);
    fklVMgcRemoveUnusedGrayCache(gc);
    fklVMgcUpdateThreshold(gc);

    FklVMvalue **base = (FklVMvalue **)ctx->extra_mark_value.base;
    FklVMvalue **const end = &base[ctx->extra_mark_value.size];
    FklVMvalue *main_proc = base[0];

    base++;
    uint64_t lib_num = ctx->extra_mark_value.size - 1;
    FklVMlib *libs = (FklVMlib *)calloc(lib_num + 1, sizeof(FklVMlib));
    FKL_ASSERT(libs);
    for (uint64_t i = 1; base < end; base++, i++)
        fklInitVMlib(&libs[i], *base);

    FklVM *main_thread = fklCreateVM(main_proc, gc, lib_num, libs);
    ctx->reached_thread = main_thread;
    ctx->running = 0;
    ctx->done = 0;
    main_thread->dummy_ins_func = B_int3;
    main_thread->debug_ctx = ctx;
    gc->main_thread = main_thread;
    fklVMthreadStart(main_thread, &gc->q);
    setReachedThread(ctx, main_thread);
}
