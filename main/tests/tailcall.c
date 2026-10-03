#include <fakeLisp/builtin.h>
#include <fakeLisp/code.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>
#include <fakeLisp/zmalloc.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        ++checks;                                                              \
        if (cond) {                                                            \
            printf("ok: %s\n", msg);                                           \
        } else {                                                               \
            ++failures;                                                        \
            printf("FAILED: %s (line %d)\n", msg, __LINE__);                   \
        }                                                                      \
    } while (0)

/* ------------------------------------------------------------------ */
/* a native builtin `vm-monitor` injected through FklBuiltinDesc       */
/* ------------------------------------------------------------------ */

typedef struct {
    size_t calls;
    size_t max_depth;
    size_t limit;
} MonitorState;

static MonitorState g_monitor;

typedef struct {
    FklVMvalueCproc cproc;
    FklVMvalueVarRef ref;
} MonitorBuiltin;

static MonitorBuiltin g_monitor_builtin;

static size_t monitor_depth(FklVM *exe) {
    size_t depth = 0;
    for (FklVMframe *f = exe->top_frame; f; f = f->prev)
        ++depth;
    return depth;
}

static int monitor_fn(FklVM *exe, FklCprocFrameContext *ctx, uint32_t argc) {
    (void)argc;
    ++g_monitor.calls;

    size_t const depth = monitor_depth(exe);
    if (depth > g_monitor.max_depth)
        g_monitor.max_depth = depth;

    if (g_monitor.calls >= g_monitor.limit)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);

    FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    return 0;
}

static size_t monitor_desc_count(FklBuiltinDescCtx *ctx) {
    (void)ctx;
    const FklBuiltinDesc *base = fklDefaultBuiltinDesc();
    return base->count(base->ctx) + 1;
}

static const char *monitor_desc_name_get(FklBuiltinDescCtx *ctx, size_t idx) {
    (void)ctx;
    const FklBuiltinDesc *base = fklDefaultBuiltinDesc();
    size_t const n = base->count(base->ctx);
    if (idx < n)
        return base->name_get(base->ctx, idx);
    if (idx == n)
        return "vm-monitor";
    return NULL;
}

static FklBuiltinInliner
monitor_desc_inliner_get(FklBuiltinDescCtx *ctx, size_t idx, size_t arg_count) {
    (void)ctx;
    const FklBuiltinDesc *base = fklDefaultBuiltinDesc();
    size_t const n = base->count(base->ctx);
    if (idx < n)
        return base->inliner_get(base->ctx, idx, arg_count);
    return NULL;
}

static FklVMvalue **g_combined_refs = NULL;
static size_t g_refs_dtor_calls = 0;

static void monitor_desc_refs_dtor(FklBuiltinDescCtx *ctx,
        FklVM *vm,
        FklVMvalue **refs) {
    (void)ctx;
    (void)vm;
    ++g_refs_dtor_calls;
    free(refs);
    g_combined_refs = NULL;
}

static FklVMvalue **monitor_desc_refs(FklBuiltinDescCtx *ctx, FklVM *vm) {
    (void)ctx;
    const FklBuiltinDesc *base = fklDefaultBuiltinDesc();

    if (g_combined_refs == NULL) {
        size_t const n = base->count(base->ctx);
        g_combined_refs = (FklVMvalue **)malloc((n + 1) * sizeof(FklVMvalue *));
        FKL_ASSERT(g_combined_refs);

        FklVMvalue **base_refs = base->refs(base->ctx, vm);
        for (size_t i = 0; i < n; ++i)
            g_combined_refs[i] = base_refs[i];

        g_monitor_builtin.cproc =
                (FklVMvalueCproc)FKL_VM_CPROC_STATIC_INIT("vm-monitor",
                        monitor_fn);
        fklInitClosedVMvalueVarRef(&g_monitor_builtin.ref,
                FKL_VM_VAL(&g_monitor_builtin.cproc));
        g_combined_refs[n] = FKL_VM_VAL(&g_monitor_builtin.ref);
    }

    return g_combined_refs;
}

static FklVMvalue *monitor_desc_stdin_get(FklBuiltinDescCtx *ctx, FklVM *vm) {
    (void)ctx;
    const FklBuiltinDesc *base = fklDefaultBuiltinDesc();
    return base->stdin_get(base->ctx, vm);
}

static const FklBuiltinDesc monitor_desc = {
    .ctx = NULL,
    .count = monitor_desc_count,
    .inliner_get = monitor_desc_inliner_get,
    .name_get = monitor_desc_name_get,
    .refs = monitor_desc_refs,
    .refs_dtor = monitor_desc_refs_dtor,
    .stdin_get = monitor_desc_stdin_get,
};

/* ------------------------------------------------------------------ */

static const char *const test_source = //
        "(let loop [(n 1000000)]\n"
        "  (vm-monitor)\n"
        "  (if (> n 0)\n"
        "    (loop (- n 1))\n"
        "    0))\n";

static FklVMvalue *parse_source(FklVM *vm, const char *src) {
    FklParseError err = 0;
    size_t errorLine = 0;
    FklAnalysisSymbolVector symbolStack;
    FklParseStateVector stateStack;
    fklAnalysisSymbolVectorInit(&symbolStack, 16);
    fklParseStateVectorInit(&stateStack, 16);
    fklVMvaluePushState0ToStack(&stateStack);

    size_t restLen = strlen(src);
    FklGrammerMatchCtx gctx = FKL_VMVALUE_PARSE_CTX_INIT(vm, NULL);
    FklVMvalue *node = fklDefaultParseForCharBuf(src,
            restLen,
            &restLen,
            &gctx,
            &err,
            &errorLine,
            &symbolStack,
            &stateStack);

    fklAnalysisSymbolVectorUninit(&symbolStack);
    fklParseStateVectorUninit(&stateStack);
    return node;
}

int main(void) {
    const FklBuiltinDesc *desc = &monitor_desc;
    FklVMgc *gc = fklCreateVMgc(desc);
    CHECK(gc != NULL, "create a vm gc with the monitor builtin");
    if (gc == NULL)
        return 1;

    FklCgCtx cg;
    fklInitCgCtx(&cg, desc, fklDupDir("."), &gc->gcvm);

    FklVMvalue *node = parse_source(&gc->gcvm, test_source);
    CHECK(node != NULL, "parse the tail-call test source");
    if (node == NULL) {
        fklUninitCgCtx(&cg);
        fklDestroyVMgc(gc);
        return 1;
    }

    FklVMvalueCgInfo *info = fklCreateVMvalueCgInfo(&cg,
            NULL,
            NULL,
            &(FklCgInfoArgs){
                .is_lib = 1,
                .is_main = 1,
            });

    FklVMvalue *bc = fklGenExpressionCode(&cg, node, cg.main_env, info);
    CHECK(bc != NULL, "compile the tail-call test program");
    if (bc == NULL) {
        fklUninitCgCtx(&cg);
        fklDestroyVMgc(gc);
        return 1;
    }

    FklVMvalueProto *proto = fklCreateVMvalueProto2(&gc->gcvm, cg.main_env);
    FklVM *vm = fklCreateVMwithByteCode(bc, gc, proto, 0);
    fklUninitCgCtx(&cg);

    CHECK(vm != NULL, "create the vm");

    g_monitor.calls = 0;
    g_monitor.max_depth = 0;
    g_monitor.limit = 20000;

    int r = fklRunVM(vm, NULL);

    printf("monitor: calls=%zu, max_frame_depth=%zu, limit=%zu\n",
            g_monitor.calls,
            g_monitor.max_depth,
            g_monitor.limit);

    CHECK(r != 0, "the monitor stopped the program by raising");
    CHECK(g_monitor.calls == g_monitor.limit,
            "the monitor was called the expected number of times");
    CHECK(g_monitor.max_depth >= 2,
            "the monitor actually observed live frames");
    CHECK(g_monitor.max_depth <= 6,
            "self tail calls reuse the frame (bounded frame depth)");

    fklDestroyAllVMs(vm);
    fklDestroyVMgc(gc);

    CHECK(g_refs_dtor_calls == 1,
            "the desc refs dtor was called once at gc teardown");
    CHECK(g_combined_refs == NULL, "the desc refs dtor released the refs");

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
