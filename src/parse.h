#ifndef PARSE_INCLUDED

#include <fakeLisp/grammer.h>
#include <fakeLisp/utils.h>

#include "grammer.c"

#endif

#ifndef PARSE_ACCEPT
#define PARSE_ACCEPT()                                                         \
    return fklAnalysisSymbolVectorPopBackNonNull(symbolStack)->ast;
#endif

#ifndef PARSE_REDUCE
#define PARSE_REDUCE()                                                         \
    if (do_reduce_action(stateStack, symbolStack, lineStack, action->prod,     \
                         action->actual_len, outerCtx, st, errLine)) {         \
        *err = FKL_PARSE_REDUCE_FAILED;                                        \
        return NULL;                                                           \
    }
#endif

#ifndef PARSE_BODY
void *fklParseWithTableForCharBuf2(
    const FklGrammer *g, const char *cstr, size_t len, size_t *restLen,
    FklGrammerMatchOuterCtx *outerCtx, FklSymbolTable *st, int *err,
    size_t *errLine, FklAnalysisSymbolVector *symbolStack,
    FklUintVector *lineStack, FklParseStateVector *stateStack) {
#endif
    *restLen = len;
    const char *start = cstr;

    int waiting_for_more_err = 0;

    for (;;) {
        FklStateActionMatchArgs match_args = FKL_STATE_ACTION_MATCH_ARGS_INIT;
        int is_waiting_for_more = 0;
        const FklAnalysisState *state =
            fklParseStateVectorBackNonNull(stateStack)->state;
        const FklAnalysisStateAction *action = state->state.action;
        for (; action; action = action->next)
            if (fklIsStateActionMatch(&action->match, g, outerCtx, start, cstr,
                                      *restLen, &is_waiting_for_more,
                                      &match_args))
                break;
        waiting_for_more_err |= is_waiting_for_more;
        if (action) {
            FKL_ASSERT(action->match.t != FKL_TERM_EOF || action->next == NULL);
            switch (action->action) {
            case FKL_ANALYSIS_IGNORE:
                outerCtx->line += fklCountCharInBuf(
                    cstr, match_args.matchLen + match_args.skip_ignore_len,
                    '\n');
                cstr += match_args.matchLen + match_args.skip_ignore_len;
                (*restLen) -= match_args.matchLen + match_args.skip_ignore_len;
                continue;
                break;
            case FKL_ANALYSIS_SHIFT:
                fklParseStateVectorPushBack2(
                    stateStack, (FklParseState){.state = action->state});
                fklInitTerminalAnalysisSymbol(
                    fklAnalysisSymbolVectorPushBack(symbolStack, NULL),
                    cstr + match_args.skip_ignore_len, match_args.matchLen,
                    outerCtx);
                fklUintVectorPushBack2(lineStack, outerCtx->line);
                outerCtx->line += fklCountCharInBuf(
                    cstr, match_args.matchLen + match_args.skip_ignore_len,
                    '\n');
                cstr += match_args.matchLen + match_args.skip_ignore_len;
                (*restLen) -= match_args.matchLen + match_args.skip_ignore_len;
                break;
            case FKL_ANALYSIS_ACCEPT:
                PARSE_ACCEPT();
                break;
            case FKL_ANALYSIS_REDUCE:
                PARSE_REDUCE();
                break;
            }
        } else {
            *err = (waiting_for_more_err
                    || (*restLen && *restLen == match_args.skip_ignore_len))
                     ? FKL_PARSE_WAITING_FOR_MORE
                     : FKL_PARSE_TERMINAL_MATCH_FAILED;
            break;
        }
    }
#ifndef PARSE_BODY
    return NULL;
}
#endif

#undef PARSE_ACCEPT
#undef PARSE_REDUCE
