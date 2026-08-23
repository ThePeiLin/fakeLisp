#ifndef FKL_GRAMMER_BUILTIN_H
#define FKL_GRAMMER_BUILTIN_H

#include "fakeLisp/base.h"
#include <fakeLisp/grammer.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef INCLUDED_BUILTIN_H
#pragma clang diagnostic ignored "-Wunused-function"
size_t get_max_non_term_length(const FklGrammer *g,
        FklGrammerMatchCtx *ctx,
        const char *start,
        const char *cur,
        size_t restLen);
#endif

static int builtin_match_symbol_func(const FklBuiltinTerminalMatchArgs *args,
        const struct FklGrammer *g,
        const char *cstrStart,
        const char *cstr,
        size_t restLen,
        size_t *pmatchLen,
        FklGrammerMatchCtx *ctx,
        int *is_waiting_for_more) {
    const FklString *start_s = args->len > 0 ? args->args[0] : NULL;
    const FklString *end_s = args->len > 1 ? args->args[1] : start_s;

    const char *start = start_s == NULL ? NULL : start_s->str;
    size_t start_size = start_s == NULL ? 0 : start_s->size;
    const char *end = end_s == NULL ? NULL : end_s->str;
    size_t end_size = end_s == NULL ? 0 : end_s->size;

    FKL_BUILTIN_TERMINAL_START(builtin_match_symbol)
    FKL_BUILTIN_TERMINAL_ARG(start)
    FKL_BUILTIN_TERMINAL_ARG(end)

    if (start == NULL) {
        size_t maxLen =
                get_max_non_term_length(g, ctx, cstrStart, cstr, restLen);
        if (!maxLen                            //
                || fklIsDecInt(cstr, maxLen)   //
                || fklIsOctInt(cstr, maxLen)   //
                || fklIsHexInt(cstr, maxLen)   //
                || fklIsDecFloat(cstr, maxLen) //
                || fklIsHexFloat(cstr, maxLen) //
                || fklIsAllDigit(cstr, maxLen))
            return 0;
        *pmatchLen = maxLen;
        return 1;
    }

    size_t matchLen = 0;
    for (;;) {
        if (fklCharBufMatch(start, start_size, cstr, restLen - matchLen) >= 0) {
            matchLen += start_size;
            cstr += start_size;
            size_t len = fklQuotedCharBufMatch(cstr,
                    restLen - matchLen,
                    end,
                    end_size);
            if (!len) {
                *is_waiting_for_more |= 1;
                return 0;
            }
            matchLen += len;
            cstr += len;
            continue;
        }
        size_t maxLen = get_max_non_term_length(g,
                ctx,
                cstrStart,
                cstr,
                restLen - matchLen);
        if ((!matchLen && !maxLen)
                || (fklCharBufMatch(start,
                            start_size,
                            cstr + maxLen,
                            restLen - maxLen - matchLen)
                                < 0                            //
                        && maxLen                              //
                        && (fklIsDecInt(cstr, maxLen)          //
                                || fklIsOctInt(cstr, maxLen)   //
                                || fklIsHexInt(cstr, maxLen)   //
                                || fklIsDecFloat(cstr, maxLen) //
                                || fklIsHexFloat(cstr, maxLen) //
                                || fklIsAllDigit(cstr, maxLen))))
            return 0;
        matchLen += maxLen;
        cstr += maxLen;
        if (fklCharBufMatch(start, start_size, cstr, restLen - matchLen) < 0)
            break;
    }
    *pmatchLen = matchLen;
    return matchLen != 0;

    FKL_BUILTIN_TERMINAL_END()
}

static int builtin_match_identifier_func(
        const FklBuiltinTerminalMatchArgs *args,
        const FklGrammer *g,
        const char *cstrStart,
        const char *cstr,
        size_t restLen,
        size_t *pmatchLen,
        FklGrammerMatchCtx *ctx,
        int *is_waiting_for_more) {
    FKL_BUILTIN_TERMINAL_START(builtin_match_identifier)

    size_t maxLen = get_max_non_term_length(g, ctx, cstrStart, cstr, restLen);
    if (!maxLen                            //
            || fklIsDecInt(cstr, maxLen)   //
            || fklIsOctInt(cstr, maxLen)   //
            || fklIsHexInt(cstr, maxLen)   //
            || fklIsDecFloat(cstr, maxLen) //
            || fklIsHexFloat(cstr, maxLen) //
            || fklIsAllDigit(cstr, maxLen))
        return 0;
    *pmatchLen = maxLen;
    return 1;

    FKL_BUILTIN_TERMINAL_END()
}

static int builtin_match_nodelimiter_func(
        const FklBuiltinTerminalMatchArgs *args,
        const FklGrammer *g,
        const char *cstrStart,
        const char *cstr,
        size_t restLen,
        size_t *pmatchLen,
        FklGrammerMatchCtx *ctx,
        int *is_waiting_for_more) {
    FKL_BUILTIN_TERMINAL_START(builtin_match_nodelimiter)

    size_t maxLen = get_max_non_term_length(g, ctx, cstrStart, cstr, restLen);
    if (!maxLen)
        return 0;
    *pmatchLen = maxLen;
    return 1;

    FKL_BUILTIN_TERMINAL_END()
}

static int builtin_match_dec_int_func(const FklBuiltinTerminalMatchArgs *args,
        const FklGrammer *g,
        const char *cstrStart,
        const char *cstr,
        size_t restLen,
        size_t *pmatchLen,
        FklGrammerMatchCtx *ctx,
        int *is_waiting_for_more) {
    FKL_BUILTIN_TERMINAL_START(builtin_match_dec_int)

    if (restLen == 0)
        return 0;
    size_t maxLen = get_max_non_term_length(g, ctx, cstrStart, cstr, restLen);
    if (maxLen && fklIsDecInt(cstr, maxLen)) {
        *pmatchLen = maxLen;
        return 1;
    }
    return 0;

    FKL_BUILTIN_TERMINAL_END()
}

static int builtin_match_hex_int_func(const FklBuiltinTerminalMatchArgs *args,
        const FklGrammer *g,
        const char *cstrStart,
        const char *cstr,
        size_t restLen,
        size_t *pmatchLen,
        FklGrammerMatchCtx *ctx,
        int *is_waiting_for_more) {
    FKL_BUILTIN_TERMINAL_START(builtin_match_hex_int)

    if (restLen == 0)
        return 0;

    size_t maxLen = get_max_non_term_length(g, ctx, cstrStart, cstr, restLen);
    if (maxLen && fklIsHexInt(cstr, maxLen)) {
        *pmatchLen = maxLen;
        return 1;
    }
    return 0;

    FKL_BUILTIN_TERMINAL_END()
}

static int builtin_match_oct_int_func(const FklBuiltinTerminalMatchArgs *args,
        const FklGrammer *g,
        const char *cstrStart,
        const char *cstr,
        size_t restLen,
        size_t *pmatchLen,
        FklGrammerMatchCtx *ctx,
        int *is_waiting_for_more) {
    FKL_BUILTIN_TERMINAL_START(builtin_match_oct_int)

    if (restLen == 0)
        return 0;

    size_t maxLen = get_max_non_term_length(g, ctx, cstrStart, cstr, restLen);
    if (maxLen && fklIsOctInt(cstr, maxLen)) {
        *pmatchLen = maxLen;
        return 1;
    }

    return 0;

    FKL_BUILTIN_TERMINAL_END()
}

static int builtin_match_dec_float_func(const FklBuiltinTerminalMatchArgs *args,
        const FklGrammer *g,
        const char *cstrStart,
        const char *cstr,
        size_t restLen,
        size_t *pmatchLen,
        FklGrammerMatchCtx *ctx,
        int *is_waiting_for_more) {
    FKL_BUILTIN_TERMINAL_START(builtin_match_dec_float)
    if (restLen == 0)
        return 0;
    size_t maxLen = get_max_non_term_length(g, ctx, cstrStart, cstr, restLen);
    if (maxLen && fklIsDecFloat(cstr, maxLen)) {
        *pmatchLen = maxLen;
        return 1;
    }
    return 0;
    FKL_BUILTIN_TERMINAL_END()
}

static int builtin_match_hex_float_func(const FklBuiltinTerminalMatchArgs *args,
        const FklGrammer *g,
        const char *cstrStart,
        const char *cstr,
        size_t restLen,
        size_t *pmatchLen,
        FklGrammerMatchCtx *ctx,
        int *is_waiting_for_more) {
    FKL_BUILTIN_TERMINAL_START(builtin_match_hex_float)

    if (restLen == 0)
        return 0;
    size_t maxLen = get_max_non_term_length(g, ctx, cstrStart, cstr, restLen);
    if (maxLen && fklIsHexFloat(cstr, maxLen)) {
        *pmatchLen = maxLen;
        return 1;
    }

    return 0;
    FKL_BUILTIN_TERMINAL_END()
}

static int builtin_match_s_dint_func(const FklBuiltinTerminalMatchArgs *args,
        const FklGrammer *g,
        const char *cstrStart,
        const char *cstr,
        size_t restLen,
        size_t *pmatchLen,
        FklGrammerMatchCtx *ctx,
        int *is_waiting_for_more) {
    const FklString *start_s = args->len > 0 ? args->args[0] : NULL;
    const char *start = start_s == NULL ? NULL : start_s->str;
    size_t start_size = start_s == NULL ? 0 : start_s->size;

    FKL_BUILTIN_TERMINAL_START(builtin_match_s_dint)
    FKL_BUILTIN_TERMINAL_ARG(start)

    if (restLen == 0)
        return 0;

    size_t maxLen = get_max_non_term_length(g, ctx, cstrStart, cstr, restLen);
    if (maxLen != 0 //
            && (start == NULL
                    || fklCharBufMatch(start,
                               start_size,
                               cstr + maxLen,
                               restLen - maxLen)
                               < 0)
            && fklIsDecInt(cstr, maxLen)) {
        *pmatchLen = maxLen;
        return 1;
    }

    return 0;
    FKL_BUILTIN_TERMINAL_END()
}

static int builtin_match_s_xint_func(const FklBuiltinTerminalMatchArgs *args,
        const FklGrammer *g,
        const char *cstrStart,
        const char *cstr,
        size_t restLen,
        size_t *pmatchLen,
        FklGrammerMatchCtx *ctx,
        int *is_waiting_for_more) {
    const FklString *start_s = args->len > 0 ? args->args[0] : NULL;
    const char *start = start_s == NULL ? NULL : start_s->str;
    size_t start_size = start_s == NULL ? 0 : start_s->size;

    FKL_BUILTIN_TERMINAL_START(builtin_match_s_xint)
    FKL_BUILTIN_TERMINAL_ARG(start)

    if (restLen == 0)
        return 0;

    size_t maxLen = get_max_non_term_length(g, ctx, cstrStart, cstr, restLen);
    if (maxLen != 0 //
            && (start == NULL
                    || fklCharBufMatch(start,
                               start_size,
                               cstr + maxLen,
                               restLen - maxLen)
                               < 0)
            && fklIsHexInt(cstr, maxLen)) {
        *pmatchLen = maxLen;
        return 1;
    }

    return 0;
    FKL_BUILTIN_TERMINAL_END()
}

static int builtin_match_s_oint_func(const FklBuiltinTerminalMatchArgs *args,
        const FklGrammer *g,
        const char *cstrStart,
        const char *cstr,
        size_t restLen,
        size_t *pmatchLen,
        FklGrammerMatchCtx *ctx,
        int *is_waiting_for_more) {
    const FklString *start_s = args->len > 0 ? args->args[0] : NULL;
    const char *start = start_s == NULL ? NULL : start_s->str;
    size_t start_size = start_s == NULL ? 0 : start_s->size;

    FKL_BUILTIN_TERMINAL_START(builtin_match_s_oint)
    FKL_BUILTIN_TERMINAL_ARG(start)

    if (restLen == 0)
        return 0;

    size_t maxLen = get_max_non_term_length(g, ctx, cstrStart, cstr, restLen);
    if (maxLen != 0 //
            && (start == NULL
                    || fklCharBufMatch(start,
                               start_size,
                               cstr + maxLen,
                               restLen - maxLen)
                               < 0)
            && fklIsOctInt(cstr, maxLen)) {
        *pmatchLen = maxLen;
        return 1;
    }

    return 0;
    FKL_BUILTIN_TERMINAL_END()
}

static int builtin_match_s_dfloat_func(const FklBuiltinTerminalMatchArgs *args,
        const FklGrammer *g,
        const char *cstrStart,
        const char *cstr,
        size_t restLen,
        size_t *pmatchLen,
        FklGrammerMatchCtx *ctx,
        int *is_waiting_for_more) {
    const FklString *start_s = args->len > 0 ? args->args[0] : NULL;
    const char *start = start_s == NULL ? NULL : start_s->str;
    size_t start_size = start_s == NULL ? 0 : start_s->size;

    FKL_BUILTIN_TERMINAL_START(builtin_match_s_dfloat)
    FKL_BUILTIN_TERMINAL_ARG(start)

    if (restLen == 0)
        return 0;

    size_t maxLen = get_max_non_term_length(g, ctx, cstrStart, cstr, restLen);
    if (maxLen != 0 //
            && (start == NULL
                    || fklCharBufMatch(start,
                               start_size,
                               cstr + maxLen,
                               restLen - maxLen)
                               < 0)
            && fklIsDecFloat(cstr, maxLen)) {
        *pmatchLen = maxLen;
        return 1;
    }

    return 0;
    FKL_BUILTIN_TERMINAL_END()
}

static int builtin_match_s_xfloat_func(const FklBuiltinTerminalMatchArgs *args,
        const FklGrammer *g,
        const char *cstrStart,
        const char *cstr,
        size_t restLen,
        size_t *pmatchLen,
        FklGrammerMatchCtx *ctx,
        int *is_waiting_for_more) {
    const FklString *start_s = args->len > 0 ? args->args[0] : NULL;
    const char *start = start_s == NULL ? NULL : start_s->str;
    size_t start_size = start_s == NULL ? 0 : start_s->size;

    FKL_BUILTIN_TERMINAL_START(builtin_match_s_xfloat)
    FKL_BUILTIN_TERMINAL_ARG(start)

    if (restLen == 0)
        return 0;

    size_t maxLen = get_max_non_term_length(g, ctx, cstrStart, cstr, restLen);
    if (maxLen != 0 //
            && (start == NULL
                    || fklCharBufMatch(start,
                               start_size,
                               cstr + maxLen,
                               restLen - maxLen)
                               < 0)
            && fklIsHexFloat(cstr, maxLen)) {
        *pmatchLen = maxLen;
        return 1;
    }

    return 0;
    FKL_BUILTIN_TERMINAL_END()
}

static int builtin_match_s_char_func(const FklBuiltinTerminalMatchArgs *args,
        const FklGrammer *g,
        const char *cstrStart,
        const char *cstr,
        size_t restLen,
        size_t *pmatchLen,
        FklGrammerMatchCtx *ctx,
        int *is_waiting_for_more) {
    const FklString *prefix_s = args->args[0];
    FKL_ASSERT(prefix_s != NULL);

    const char *prefix = prefix_s->str;
    size_t prefix_size = prefix_s->size;

    FKL_BUILTIN_TERMINAL_START(builtin_match_s_char)
    FKL_BUILTIN_TERMINAL_ARG(prefix)

    FKL_ASSERT(prefix != NULL);

    size_t minLen = prefix_size + 1;
    if (restLen < minLen)
        return 0;
    if (fklCharBufMatch(prefix, prefix_size, cstr, restLen) < 0)
        return 0;
    restLen -= prefix_size;
    cstr += prefix_size;
    size_t maxLen = get_max_non_term_length(g, ctx, cstrStart, cstr, restLen);
    if (!maxLen)
        *pmatchLen = prefix_size + 1;
    else
        *pmatchLen = prefix_size + maxLen;
    return 1;
    FKL_BUILTIN_TERMINAL_END()
}

static int builtin_match_never_func(const FklBuiltinTerminalMatchArgs *args,
        const FklGrammer *g,
        const char *cstrStart,
        const char *cstr,
        size_t restLen,
        size_t *pmatchLen,
        FklGrammerMatchCtx *ctx,
        int *is_waiting_for_more) {
    FKL_BUILTIN_TERMINAL_START(builtin_match_never)
    return 0;
    FKL_BUILTIN_TERMINAL_END()
}

// AI 编写的第一版
static int builtin_match_raw_string_func_with_llm(
        const FklBuiltinTerminalMatchArgs *args,
        const char *cstrStart,
        const char *cstr,
        size_t restLen,
        size_t *pmatchLen,
        FklGrammerMatchCtx *ctx,
        int *is_waiting_for_more) {
    const FklString *d = args->args[0];
    // 1. 起始定界符
    if (restLen < d->size || fklStringCharBufMatch(d, cstr, restLen) < 0)
        return 0;
    // 2. 标签：扫描到下一个定界符（可为空）
    size_t i = d->size;
    size_t delim_end = restLen;
    for (; i < restLen; i++)
        if (fklStringCharBufMatch(d, cstr + i, restLen - i) >= 0) {
            delim_end = i;
            break;
        }
    if (delim_end == restLen) {
        *is_waiting_for_more |= 1;
        return 0;
    }
    size_t delim_len = delim_end - d->size;
    const char *delim = cstr + d->size;
    // 3. 内容：扫描到 <delim> <标签> <delim> 闭合序列
    size_t j = delim_end + d->size;
    for (; j < restLen; j++) {
        if (fklStringCharBufMatch(d, cstr + j, restLen - j) < 0)
            continue;
        if ((!delim_len
                    || fklCharBufMatch(delim,
                               delim_len,
                               cstr + j + d->size,
                               restLen - j - d->size)
                               >= 0)
                && fklStringCharBufMatch(d,
                           cstr + j + d->size + delim_len,
                           restLen - j - d->size - delim_len)
                           >= 0) {
            *pmatchLen = j + d->size + delim_len + d->size;
            return 1;
        }
    }
    *is_waiting_for_more |= 1;
    return 0;
}

static int builtin_match_raw_string_func(
        const FklBuiltinTerminalMatchArgs *args,
        const FklGrammer *g,
        const char *cstrStart,
        const char *cstr,
        size_t restLen,
        size_t *pmatchLen,
        FklGrammerMatchCtx *ctx,
        int *is_waiting_for_more) {
    const FklString *start_s = args->len > 0 ? args->args[0] : NULL;
    const FklString *end_s = args->len > 1 ? args->args[1] : start_s;

    const char *start = start_s == NULL ? NULL : start_s->str;
    size_t start_size = start_s == NULL ? 0 : start_s->size;
    const char *end = end_s == NULL ? NULL : end_s->str;
    size_t end_size = end_s == NULL ? 0 : end_s->size;

    FKL_BUILTIN_TERMINAL_START(builtin_match_raw_string)
    FKL_BUILTIN_TERMINAL_ARG(start)
    FKL_BUILTIN_TERMINAL_ARG(end)

    if (restLen == 0)
        return 0;

    size_t matchLen = 0;
    if (fklCharBufMatch(start, start_size, cstr, restLen) < 0)
        return 0;

    matchLen += start_size;
    cstr += start_size;
    restLen -= start_size;

    if (restLen == 0) {
        *is_waiting_for_more |= 1;
        return 0;
    }

    size_t delim_len = 0;
    const char *delim_start = cstr;
    while (restLen > 0) {
        if (fklCharBufMatch(start, start_size, cstr, restLen) >= 0)
            break;
        ++matchLen;
        ++delim_len;
        ++cstr;
        --restLen;
    }

    if (restLen == 0) {
        *is_waiting_for_more |= 1;
        return 0;
    }

    matchLen += start_size;
    cstr += start_size;
    restLen -= start_size;

    size_t const total_end_size = end_size + delim_len + end_size;

expect_end:
    if (restLen < total_end_size) {
        *is_waiting_for_more |= 1;
        return 0;
    }

    const char *cstr1 = cstr + end_size;
    size_t restLen1 = restLen - end_size;

    const char *cstr2 = cstr + end_size + delim_len;
    size_t restLen2 = restLen - end_size - delim_len;

    if (fklCharBufMatch(end, end_size, cstr, restLen) >= 0
            && fklCharBufMatch(delim_start, delim_len, cstr1, restLen1) >= 0
            && fklCharBufMatch(end, end_size, cstr2, restLen2) >= 0) {
        matchLen += total_end_size;
        *pmatchLen = matchLen;
        return 1;
    }

    ++matchLen;
    ++cstr;
    --restLen;

    goto expect_end;

    return 0;

    FKL_BUILTIN_TERMINAL_END()
}

#ifdef __cplusplus
}
#endif
#endif
