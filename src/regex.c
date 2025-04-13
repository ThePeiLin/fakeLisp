#include "fakeLisp/base.h"
#include <fakeLisp/common.h>
#include <fakeLisp/regex.h>
#include <fakeLisp/utils.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

struct ReInst {
    uint32_t p;
    uint32_t trueoff;
    uint32_t failoff;
};

struct ReCompileCtx {
    struct ReInst *inst;
    uint32_t objs;
    uint32_t strln;
    uint32_t ecnt;
    uint32_t stcnt;
};

static inline uint32_t count_number_esc_char(const char *pat, uint32_t len) {
    if (toupper(*pat) == 'X') {
        uint32_t i = 1;
        if (i < len && isxdigit(pat[i]))
            i++;
        if (i < len && isxdigit(pat[i]))
            i++;
        return i;
    } else if (*pat >= '0' && *pat < '9') {
        uint32_t i = 1;
        if (i < len && pat[i] >= '0' && pat[i] < '9')
            i++;
        if (i < len && pat[i] >= '0' && pat[i] < '9')
            i++;
        return i;
    } else
        return 1;
}

static inline int esc_char_to_char(const char *pat, uint32_t len) {
    char tmp[4] = {0};
    strncpy(tmp, pat, len > 3 ? 3 : len);
    if (toupper(*pat) == 'X' && len > 1)
        return strtol(&tmp[1], NULL, 16);
    else if (*pat >= '0' && *pat < '9')
        return strtol(tmp, NULL, 8);
    else
        return *pat;
}

static inline uint32_t compute_char_class_len(const char *pat, uint32_t len) {
    uint32_t retval = 1;
    const char *end = &pat[len];
    while (pat < end) {
        if (&pat[2] < end && pat[1] == '-') {
            retval += 3;
            pat += 3;
        } else {
            uint32_t len = 0;
            uint8_t num = 0;
            while (num < UINT8_MAX) {
                if (&pat[len] >= end
                    || (&pat[len + 2] >= end && pat[len + 1] == '-'))
                    break;
                if (pat[len] == '\\')
                    len += count_number_esc_char(&pat[len + 1],
                                                 end - &pat[len + 1]);
                num++;
                len++;
            }
            retval += 2 + num;
            pat += len;
        }
    }
    return retval;
}

static inline uint32_t char_buf_to_char_class(const char *pat, size_t len,
                                              uint8_t *patrns) {
    const char *end = &pat[len];
    uint32_t idx = 0;
    while (pat < end) {
        if (&pat[2] < end && pat[1] == '-') {
            patrns[idx++] = FKL_REGEX_CHAR_CLASS_RANGE;
            if (*pat > pat[2])
                return 0;
            patrns[idx++] = *pat;
            patrns[idx++] = pat[2];
            pat += 3;
        } else {
            uint32_t len = 0;
            uint8_t num = 0;
            while (num < UINT8_MAX) {
                if (&pat[len] >= end
                    || (&pat[len + 2] >= end && pat[len + 1] == '-'))
                    break;
                if (pat[len] == '\\')
                    len += count_number_esc_char(&pat[len + 1],
                                                 end - &pat[len + 1]);
                num++;
                len++;
            }
            patrns[idx++] = FKL_REGEX_CHAR_CLASS_CHAR;
            patrns[idx++] = num;
            for (uint32_t i = 0; i < len; i++) {
                if (pat[i] == '\\') {
                    uint32_t esc_len =
                        count_number_esc_char(&pat[i + 1], len - i - 1);
                    int ch = esc_char_to_char(&pat[i + 1], esc_len);
                    if (ch > UINT8_MAX)
                        return 0;
                    patrns[idx++] = ch;
                    i += len;
                } else
                    patrns[idx++] = pat[i];
            }
            pat += len;
        }
    }
    patrns[idx++] = FKL_REGEX_CHAR_CLASS_END;
    uint32_t retval = idx;
    return retval;
}

static inline int compile_count(struct ReCompileCtx *ctx, const char *pat,
                                size_t len) {
    size_t i = 0;
    char c = 0;
    uint32_t brcnt = 0;
    ctx->ecnt++;
    ctx->stcnt = 1;
    ctx->inst = (struct ReInst *)calloc(ctx->ecnt, sizeof(struct ReInst));
    FKL_ASSERT(ctx->inst);
    uint32_t resel = 0;
    while (i < len) {
        c = pat[i];
        if (c == '\\') {
            if (i + 1 >= len)
                return 1;
            i += count_number_esc_char(&pat[i + 1], len - i - 1);
        } else if (c == '*' || c == '+')
            ctx->stcnt++;
        else if (c == '(') {
            if (i + 1 >= len)
                return 1;
            brcnt++;
            struct ReInst *inst = (struct ReInst *)fklRealloc(
                ctx->inst, (ctx->ecnt + 1) * sizeof(struct ReInst));
            FKL_ASSERT(inst);
            ctx->inst = inst;
            inst[ctx->ecnt].p = resel;
            inst[ctx->ecnt].trueoff = ctx->objs;
            inst[ctx->ecnt].failoff = 0;
            resel = ctx->ecnt;
            ctx->ecnt++;
        } else if (c == ')') {
            if (brcnt == 0)
                return 1;
            ctx->inst[resel].failoff = ctx->objs;
            resel = ctx->inst[resel].p;
            brcnt--;
        } else if (c == '[') {
            if (i + 1 >= len)
                return 1;
            if (pat[i + 1] == '^')
                i++;
            uint32_t pat_begin = i + 1;
            while (i < len) {
                i++;
                if (pat[i] == ']')
                    break;
                else if (pat[i] == '\\') {
                    if (i + 1 >= len)
                        return 1;
                    i += count_number_esc_char(&pat[i + 1], len - i - 1);
                    ;
                }
            }
            if (i == len)
                return 1;
            uint32_t pat_len = (i - pat_begin);
            ctx->strln += compute_char_class_len(&pat[pat_begin], pat_len);
        }
        i++;
        ctx->objs++;
    }
    if (brcnt)
        return 1;
    ctx->objs++;
    ctx->inst[0].failoff = ctx->objs - 1;
    return 0;
}

FklRegexCode *fklRegexCompileCharBuf(const char *pattern, size_t len) {
    FklRegexCode *retval = NULL;
    struct ReCompileCtx ctx = {NULL, 0, 0, 0, 0};
    if (compile_count(&ctx, pattern, len))
        goto error;
    uint32_t patoff = ctx.objs * sizeof(FklRegexObj);
    size_t totallen = patoff + ctx.strln + sizeof(FklRegexCode);
    retval = (FklRegexCode *)calloc(1, totallen);
    FKL_ASSERT(retval);
    retval->totalsize = totallen - sizeof(FklRegexCode);
    retval->pstsize = ctx.stcnt;
    FklRegexObj *objs = retval->data;
    uint8_t *patrns = (uint8_t *)objs;
    char c = '\0';
    uint32_t i = 0;
    uint32_t j = 0;
    uint32_t ecnt = 0;
    uint32_t resel = 0;

    struct ReInst *inst = ctx.inst;
    while (i < len) {
        c = pattern[i];
        FklRegexObj *cur_obj = &objs[j];
        switch (c) {
        case '^':
            cur_obj->type = FKL_REGEX_BEGIN;
            cur_obj->falseoffset = inst[resel].failoff;
            cur_obj->trueoffset = j + 1;
            break;
        case '$':
            cur_obj->type = FKL_REGEX_END;
            cur_obj->falseoffset = 0;
            cur_obj->trueoffset = j + 1;
            break;
        case '.':
            cur_obj->type = FKL_REGEX_DOT;
            cur_obj->falseoffset = 0;
            cur_obj->trueoffset = j + 1;
            break;
        case '*':
            if (j == 0)
                goto error;
            else {
                FklRegexObj *prev = &cur_obj[-1];
                cur_obj->type = FKL_REGEX_STAR;
                cur_obj->falseoffset = inst[resel].failoff;
                if (prev->type == FKL_REGEX_GROUPEND)
                    cur_obj->trueoffset = inst[resel + 1].trueoff;
                else
                    cur_obj->trueoffset = j - 1;
                prev->falseoffset = j;
            }
            break;
        case '+':
            if (j == 0)
                goto error;
            else {
                FklRegexObj *prev = &cur_obj[-1];
                cur_obj->type = FKL_REGEX_PLUS;
                cur_obj->falseoffset = inst[resel].failoff;
                if (prev->type == FKL_REGEX_GROUPEND)
                    cur_obj->trueoffset = inst[resel + 1].trueoff;
                else
                    cur_obj->trueoffset = j - 1;
                prev->falseoffset = j;
            }
            break;
        case '?':
            if (j == 0)
                goto error;
            else {
                cur_obj->type = FKL_REGEX_QUESTIONMARK;
                cur_obj->trueoffset = j + 1;
                cur_obj->falseoffset = inst[resel].failoff;
                cur_obj[-1].falseoffset = j;
            }
            break;
        case '|':
            if (j == 0)
                goto error;
            else {
                uint32_t cur_failoff = inst[resel].failoff;
                cur_obj->type = FKL_REGEX_BRANCH;
                cur_obj->falseoffset = j + 1;
                cur_obj->trueoffset = cur_failoff;
                for (uint32_t i = j - 1; i > inst[resel].trueoff; i--) {
                    if (objs[i].type == FKL_REGEX_BRANCH
                        && objs[i].trueoffset == cur_failoff)
                        break;
                    if (objs[i].falseoffset == cur_failoff)
                        objs[i].falseoffset = j;
                }
            }
            break;
        case '(':
            ecnt++;
            resel = ecnt;
            FKL_ASSERT(ctx.ecnt > ecnt);
            cur_obj->type = FKL_REGEX_GROUPSTART;
            cur_obj->falseoffset = inst[resel].failoff;
            cur_obj->trueoffset = j + 1;
            break;
        case ')':
            resel = inst[resel].p;
            cur_obj->type = FKL_REGEX_GROUPEND;
            cur_obj->falseoffset = inst[resel].failoff;
            cur_obj->trueoffset = j + 1;
            break;
        case '\\':
            if (i + 1 >= len)
                goto error;
            else {
                cur_obj->trueoffset = j + 1;
                cur_obj->falseoffset = inst[resel].failoff;
                uint32_t esc_len =
                    count_number_esc_char(&pattern[i + 1], len - i - 1);
                switch (pattern[i + 1]) {
                case 'd':
                    cur_obj->type = FKL_REGEX_DIGITS;
                    break;
                case 'D':
                    cur_obj->type = FKL_REGEX_NOT_DIGITS;
                    break;
                case 'w':
                    cur_obj->type = FKL_REGEX_ALPHA;
                    break;
                case 'W':
                    cur_obj->type = FKL_REGEX_NOT_ALPHA;
                    break;
                case 's':
                    cur_obj->type = FKL_REGEX_WHITESPACE;
                    break;
                case 'S':
                    cur_obj->type = FKL_REGEX_NOT_WHITESPACE;
                    break;
                case 'n':
                    cur_obj->type = FKL_REGEX_CHAR;
                    cur_obj->ch = '\n';
                    break;
                default: {
                    cur_obj->type = FKL_REGEX_CHAR;
                    int ch = esc_char_to_char(&pattern[i + 1], esc_len);
                    if (ch > UINT8_MAX)
                        goto error;
                    cur_obj->ch = ch;
                } break;
                }
                i += esc_len;
            }
            break;
        case '[': {
            if (pattern[i + 1] == '^') {
                cur_obj->type = FKL_REGEX_INV_CHAR_CLASS;
                i++;
            } else
                cur_obj->type = FKL_REGEX_CHAR_CLASS;
            uint32_t pat_begin = i + 1;
            while (i < len) {
                i++;
                if (pattern[i] == ']')
                    break;
                else if (pattern[i] == '\\') {
                    if (i + 1 >= len)
                        goto error;
                    i += count_number_esc_char(&pattern[i + 1], len - i - 1);
                    ;
                }
            }
            uint32_t pat_len = (i - pat_begin);
            cur_obj->ccl = patoff;
            uint32_t len = char_buf_to_char_class(&pattern[pat_begin], pat_len,
                                                  &patrns[patoff]);
            if (!len)
                goto error;
            patoff += len;
            cur_obj->trueoffset = j + 1;
            cur_obj->falseoffset = inst[resel].failoff;
        } break;
        default:
            cur_obj->type = FKL_REGEX_CHAR;
            cur_obj->ch = c;
            cur_obj->trueoffset = j + 1;
            cur_obj->falseoffset = inst[resel].failoff;
            break;
        }
        i++;
        j++;
    }
    objs[j].type = FKL_REGEX_UNUSED;
    free(ctx.inst);
    return retval;
error:
    free(ctx.inst);
    free(retval);
    return NULL;
}

void fklRegexFree(FklRegexCode *re) { free(re); }

FklRegexCode *fklRegexCompileCstr(const char *cstr) {
    return fklRegexCompileCharBuf(cstr, strlen(cstr));
}

static inline void print_char(char ch, FILE *fp) {
    if (ch == '[')
        fputs("\\[", fp);
    else if (ch == ']')
        fputs("\\]", fp);
    else if (isgraph(ch))
        fprintf(fp, "%c", ch);
    else
        fprintf(fp, "\\x%X", (uint8_t)ch);
}

static inline void print_char_class(uint8_t *pat, FILE *fp) {
    while (*pat != FKL_REGEX_CHAR_CLASS_END) {
        switch (*(pat++)) {
        case FKL_REGEX_CHAR_CLASS_CHAR: {
            uint8_t num = *(pat++);
            for (uint8_t i = 0; i < num; i++)
                print_char(pat[i], fp);
            pat += num;
        } break;
        case FKL_REGEX_CHAR_CLASS_RANGE: {
            print_char(*(pat++), fp);
            fputc('-', fp);
            print_char(*(pat++), fp);
        } break;
        }
    }
}

static const char *regex_obj_type_name[] = {
    "UNUSED",
    "BEGIN",
    "END",
    "DOT",
    "CHAR",
    "CHAR_CLASS",
    "INV_CHAR_CLASS",
    "DIGITS",
    "NOT_DIGITS",
    "ALPHA",
    "NOT_ALPHA",
    "WHITESPACE",
    "NOT_WHITESPACE",
    "STAR",
    "PLUS",
    "QUESTIONMARK",
    "BRANCH",
    "GROUPSTART",
    "GROUPEND",
};

static const char *regex_obj_enum_type_name[] = {
    "FKL_REGEX_UNUSED",
    "FKL_REGEX_BEGIN",
    "FKL_REGEX_END",
    "FKL_REGEX_DOT",
    "FKL_REGEX_CHAR",
    "FKL_REGEX_CHAR_CLASS",
    "FKL_REGEX_INV_CHAR_CLASS",
    "FKL_REGEX_DIGITS",
    "FKL_REGEX_NOT_DIGITS",
    "FKL_REGEX_ALPHA",
    "FKL_REGEX_NOT_ALPHA",
    "FKL_REGEX_WHITESPACE",
    "FKL_REGEX_NOT_WHITESPACE",
    "FKL_REGEX_STAR",
    "FKL_REGEX_PLUS",
    "FKL_REGEX_QUESTIONMARK",
    "FKL_REGEX_BRANCH",
    "FKL_REGEX_GROUPSTART",
    "FKL_REGEX_GROUPEND",
};

void fklRegexPrint(const FklRegexCode *re, FILE *fp) {
    ;

    fprintf(fp, "total size:%u\n", re->totalsize);
    const FklRegexObj *obj = re->data;
    uint32_t idx = 0;
    uint8_t *patrns = (uint8_t *)re->data;
    for (;;) {
        fprintf(fp, "%-8X %-14s %8X %8X ", idx, regex_obj_type_name[obj->type],
                obj->trueoffset, obj->falseoffset);
        if (obj->type == FKL_REGEX_CHAR)
            print_char(obj->ch, fp);
        else if (obj->type == FKL_REGEX_CHAR_CLASS
                 || obj->type == FKL_REGEX_INV_CHAR_CLASS) {
            fputc('[', fp);
            print_char_class(&patrns[obj->ccl], fp);
            fputc(']', fp);
        }
        fputc('\n', fp);
        if (obj->type == FKL_REGEX_UNUSED)
            break;
        obj++;
        idx++;
    }
}

struct ReMatchState {
    uint32_t matchcnt;
    uint32_t offset;
    uint32_t st;
    uint32_t st0;
};

static inline int match_char_class(uint8_t c, const uint8_t *patrns) {
    while (*patrns != FKL_REGEX_CHAR_CLASS_END) {
        switch (*(patrns++)) {
        case FKL_REGEX_CHAR_CLASS_RANGE: {
            uint8_t start = *(patrns++);
            uint8_t end = *(patrns++);
            if (c >= start && c <= end)
                return 1;
        } break;
        case FKL_REGEX_CHAR_CLASS_CHAR: {
            uint8_t num = *(patrns++);
            for (uint8_t i = 0; i < num; i++) {
                if (c == patrns[i])
                    return 1;
            }
            patrns += num;
        } break;
        }
    }
    return 0;
}

static inline int match_inv_char_class(uint8_t c, const uint8_t *patrns) {
    if (*patrns != FKL_REGEX_CHAR_CLASS_END)
        return !match_char_class(c, patrns);
    return 0;
}

static inline int matchone(const FklRegexObj *cur, const uint8_t *patrns,
                           uint8_t c) {
    switch (cur->type) {
    case FKL_REGEX_DOT:
        return 1;
        break;
    case FKL_REGEX_CHAR_CLASS:
        return match_char_class(c, &patrns[cur->ccl]);
        break;
    case FKL_REGEX_INV_CHAR_CLASS:
        return match_inv_char_class(c, &patrns[cur->ccl]);
        break;
    case FKL_REGEX_DIGITS:
        return isdigit(c);
        break;
    case FKL_REGEX_NOT_DIGITS:
        return !isdigit(c);
        break;
    case FKL_REGEX_ALPHA:
        return isalnum(c);
        break;
    case FKL_REGEX_NOT_ALPHA:
        return !isalnum(c);
        break;
    case FKL_REGEX_WHITESPACE:
        return isspace(c);
        break;
    case FKL_REGEX_NOT_WHITESPACE:
        return !isspace(c);
        break;
    case FKL_REGEX_CHAR:
        return cur->ch == c;
        break;
    default:
        return -1;
    }
}

#define STP state[sp]
#define POP() sp--
#define PUSH()                                                                 \
    state[sp + 1].offset = state[sp].offset;                                   \
    state[sp + 1].st = state[sp].st;                                           \
    sp++

#define COMMON_PART
static inline uint32_t matchpattern(const FklRegexCode *re, const char *text,
                                    uint32_t len) {
    const uint32_t IMPOSSIBLE_IDX = len + 1;
    const FklRegexObj *objs = re->data;
    const FklRegexObj *cur_obj = NULL;
    const uint8_t *patrns = (const uint8_t *)re->data;
    struct ReMatchState *state =
        (struct ReMatchState *)calloc(re->pstsize, sizeof(struct ReMatchState));
    FKL_ASSERT(state);
    uint32_t sp = 0;
    int evalres = 0;
    uint32_t brtxcoff = 0;

    STP.st0 = IMPOSSIBLE_IDX;

    for (;;) {
        cur_obj = &objs[STP.offset];
        switch (cur_obj->type) {
        case FKL_REGEX_BEGIN:
            STP.offset = cur_obj->trueoffset;
            break;
        case FKL_REGEX_UNUSED:
        case FKL_REGEX_END:
            if (evalres == 0 && sp > 0)
                POP();
            else {
                uint32_t est = STP.st;
                free(state);
                if (cur_obj->type == FKL_REGEX_END)
                    return est == len && evalres ? est : IMPOSSIBLE_IDX;
                else
                    return evalres ? est : IMPOSSIBLE_IDX;
            }
            break;
        case FKL_REGEX_QUESTIONMARK:
            STP.offset = cur_obj->trueoffset;
            evalres = 1;
            STP.matchcnt = 0;
            break;
        case FKL_REGEX_STAR:
        case FKL_REGEX_PLUS: {
            if (evalres)
                STP.matchcnt++;
            if (evalres && STP.st < len) {
                uint32_t offset0 = STP.offset + 1;
                STP.offset = cur_obj->trueoffset;
                const FklRegexObj *cur_obj0 = &objs[offset0];
                if (cur_obj0->type != FKL_REGEX_END
                    && cur_obj0->type != FKL_REGEX_UNUSED
                    && cur_obj0->type != FKL_REGEX_GROUPEND
                    && cur_obj0->type != FKL_REGEX_BRANCH
                    && STP.st0 != STP.st) {
                    uint32_t tr = matchone(cur_obj0, patrns, text[STP.st]);
                    if (tr) {
                        STP.st0 = STP.st;
                        PUSH();
                        STP.st0 = IMPOSSIBLE_IDX;
                        STP.offset = cur_obj0->trueoffset;
                        STP.st++;
                    }
                }
            } else {
                if (cur_obj->type == FKL_REGEX_PLUS) {
                    if (STP.matchcnt > 0)
                        evalres = 1;
                    else
                        evalres = 0;
                } else if (cur_obj->type == FKL_REGEX_STAR)
                    evalres = 1;
                if (evalres == 1)
                    STP.offset++;
                else
                    STP.offset = cur_obj->falseoffset;
                STP.matchcnt = 0;
            }
        } break;
        case FKL_REGEX_GROUPSTART:
            STP.offset++;
            brtxcoff = STP.st;
            break;
        case FKL_REGEX_GROUPEND:
            if (evalres == 0) {
                STP.st = brtxcoff;
                STP.offset = cur_obj->falseoffset;
            } else {
                STP.offset = cur_obj->trueoffset;
                brtxcoff = 0;
            }
            break;
        case FKL_REGEX_BRANCH:
            if (evalres == 0) {
                STP.st = brtxcoff;
                STP.offset = cur_obj->falseoffset;
            } else {
                STP.offset = cur_obj->trueoffset;
                brtxcoff = 0;
            }
            break;
        default:
            if (STP.st == len) {
                STP.offset = cur_obj->falseoffset;
                evalres = 0;
                continue;
            }
            evalres = matchone(cur_obj, patrns, text[STP.st]);
            if (evalres) {
                STP.offset = cur_obj->trueoffset;
                STP.st++;
            } else
                STP.offset = cur_obj->falseoffset;
            break;
        }
    }
    free(state);
    return 0;
}

static inline uint32_t lex_matchpattern(const FklRegexCode *re,
                                        const char *text, uint32_t len,
                                        int *last_is_true) {
    const uint32_t IMPOSSIBLE_IDX = len + 1;
    const FklRegexObj *objs = re->data;
    const FklRegexObj *cur_obj = NULL;
    const uint8_t *patrns = (const uint8_t *)re->data;
    struct ReMatchState *state =
        (struct ReMatchState *)calloc(re->pstsize, sizeof(struct ReMatchState));
    FKL_ASSERT(state);
    uint32_t sp = 0;
    int evalres = 0;
    uint32_t brtxcoff = 0;

    STP.st0 = IMPOSSIBLE_IDX;

    for (;;) {
        cur_obj = &objs[STP.offset];
        switch (cur_obj->type) {
        case FKL_REGEX_BEGIN:
            STP.offset = cur_obj->trueoffset;
            break;
        case FKL_REGEX_UNUSED:
        case FKL_REGEX_END:
            if (evalres == 0 && sp > 0)
                POP();
            else {
                uint32_t est = STP.st;
                free(state);
                return evalres ? est : IMPOSSIBLE_IDX;
            }
            break;
        case FKL_REGEX_QUESTIONMARK:
            STP.offset = cur_obj->trueoffset;
            evalres = 1;
            STP.matchcnt = 0;
            break;
        case FKL_REGEX_STAR:
        case FKL_REGEX_PLUS: {
            if (evalres)
                STP.matchcnt++;
            if (evalres && STP.st < len) {
                uint32_t offset0 = STP.offset + 1;
                STP.offset = cur_obj->trueoffset;
                const FklRegexObj *cur_obj0 = &objs[offset0];
                if (cur_obj0->type != FKL_REGEX_END
                    && cur_obj0->type != FKL_REGEX_UNUSED
                    && cur_obj0->type != FKL_REGEX_GROUPEND
                    && cur_obj0->type != FKL_REGEX_BRANCH
                    && STP.st0 != STP.st) {
                    uint32_t tr = matchone(cur_obj0, patrns, text[STP.st]);
                    if (tr) {
                        STP.st0 = STP.st;
                        PUSH();
                        STP.st0 = IMPOSSIBLE_IDX;
                        STP.offset = cur_obj0->trueoffset;
                        STP.st++;
                    }
                }
            } else {
                if (cur_obj->type == FKL_REGEX_PLUS) {
                    if (STP.matchcnt > 0)
                        evalres = 1;
                    else
                        evalres = 0;
                } else if (cur_obj->type == FKL_REGEX_STAR)
                    evalres = 1;
                if (evalres == 1)
                    STP.offset++;
                else
                    STP.offset = cur_obj->falseoffset;
                STP.matchcnt = 0;
            }
        } break;
        case FKL_REGEX_GROUPSTART:
            STP.offset++;
            brtxcoff = STP.st;
            break;
        case FKL_REGEX_GROUPEND:
            if (evalres == 0) {
                STP.st = brtxcoff;
                STP.offset = cur_obj->falseoffset;
            } else {
                STP.offset = cur_obj->trueoffset;
                brtxcoff = 0;
            }
            break;
        case FKL_REGEX_BRANCH:
            if (evalres == 0) {
                STP.st = brtxcoff;
                STP.offset = cur_obj->falseoffset;
            } else {
                STP.offset = cur_obj->trueoffset;
                brtxcoff = 0;
            }
            break;
        default:
            if (STP.st == len) {
                STP.offset = cur_obj->falseoffset;
                evalres = 0;
                continue;
            }
            evalres = matchone(cur_obj, patrns, text[STP.st]);
            if (evalres) {
                STP.offset = cur_obj->trueoffset;
                STP.st++;
            } else
                STP.offset = cur_obj->falseoffset;
            break;
        }
        if (evalres)
            *last_is_true = 1;
    }
    free(state);
    return 0;
}

#undef STP
#undef POP
#undef PUSH

uint32_t fklRegexMatchpInCharBuf(const FklRegexCode *re, const char *text,
                                 uint32_t len, uint32_t *ppos) {
    if (re->data[0].type == FKL_REGEX_BEGIN) {
        *ppos = 0;
        return matchpattern(re, text, len);
    } else {
        uint32_t pos = 0;
        uint32_t match_len = 0;
        while (pos < len) {
            match_len = matchpattern(re, &text[pos], len - pos);
            if (match_len > len - pos)
                pos++;
            else {
                *ppos = pos;
                return match_len;
            }
        }
    }
    return len + 1;
}

uint32_t fklRegexMatchpInCstr(const FklRegexCode *re, const char *str,
                              uint32_t *ppos) {
    return fklRegexMatchpInCharBuf(re, str, strlen(str), ppos);
}

uint32_t fklRegexLexMatchp(const FklRegexCode *re, const char *text,
                           uint32_t len, int *last_is_true) {
    *last_is_true = 0;
    return lex_matchpattern(re, text, len, last_is_true);
}

static inline void print_objs(const FklRegexObj *obj, uint32_t objs_num,
                              FILE *fp) {
    for (const FklRegexObj *end = &obj[objs_num]; obj < end; obj++) {
        fprintf(fp, "\t\t{.type=%-24s,.trueoffset=%u,.falseoffset=%u,",
                regex_obj_enum_type_name[obj->type], obj->trueoffset,
                obj->falseoffset);
        if (obj->type == FKL_REGEX_CHAR)
            fprintf(fp, ".ch=%d,},\n", obj->ch);
        else if (obj->type == FKL_REGEX_CHAR_CLASS)
            fprintf(fp, ".ccl=%u,},\n", obj->ccl);
        else
            fputs("},\n", fp);
    }
}

static const char *char_class_enum_name[] = {
    "FKL_REGEX_CHAR_CLASS_END",
    "FKL_REGEX_CHAR_CLASS_CHAR",
    "FKL_REGEX_CHAR_CLASS_RANGE",
};

static inline void print_patrns(const uint8_t *pat, uint32_t len, FILE *fp) {
    for (const uint8_t *end = &pat[len]; pat < end;) {
        fprintf(fp, "\t\t%-26s,", char_class_enum_name[*pat]);
        switch (*(pat++)) {
        case FKL_REGEX_CHAR_CLASS_END:
            break;
        case FKL_REGEX_CHAR_CLASS_RANGE:
            fprintf(fp, "%u,", *(pat++));
            fprintf(fp, "%u,", *(pat++));
            break;
        case FKL_REGEX_CHAR_CLASS_CHAR: {
            uint32_t num = *(pat++);
            fprintf(fp, "%u,", num);
            for (uint8_t i = 0; i < num; i++)
                fprintf(fp, "%u,", pat[i]);
            pat += num;
        } break;
        }
        fputc('\n', fp);
    }
}

void fklRegexPrintAsC(const FklRegexCode *re, const char *prefix,
                      const char *pattern, uint32_t pattern_len, FILE *fp) {
    uint32_t objs_num = 1;
    uint32_t totalsize = re->totalsize;
    const FklRegexObj *objs = re->data;
    for (uint32_t i = 0; objs[i].type != FKL_REGEX_UNUSED; i++)
        objs_num++;
    uint32_t objs_size = objs_num * sizeof(FklRegexObj);
    uint32_t strln = totalsize - objs_size;
    if (strln)
        fprintf(fp,
                "struct{\n"
                "\tuint32_t totalsize;\n"
                "\tuint32_t pstsize;\n"
                "\tFklRegexObj objs[%u];\n"
                "\tuint8_t patrns[%u];\n"
                "}",
                objs_num, strln);
    else
        fprintf(fp,
                "struct{\n"
                "\tuint32_t totalsize;\n"
                "\tuint32_t pstsize;\n"
                "\tFklRegexObj objs[%u];\n"
                "}",
                objs_num);
    fputs(prefix ? prefix : "regex_", fp);
    fklPrintCharBufInHex(pattern, pattern_len, fp);
    fprintf(fp,
            "={\n"
            "\t.totalsize=%u,\n"
            "\t.pstsize=%u,\n"
            "\t.objs={\n",
            totalsize, re->pstsize);
    print_objs(objs, objs_num, fp);
    fputs("\t},\n", fp);
    if (strln) {
        const uint8_t *patrns = &((const uint8_t *)re->data)[objs_size];
        fputs("\t.patrns={\n", fp);
        print_patrns(patrns, strln, fp);
        fputs("\t},\n", fp);
    }
    fputs("};\n", fp);
}

#include <fakeLisp/common.h>

void fklRegexPrintAsCwithNum(const FklRegexCode *re, const char *prefix,
                             uint64_t num, FILE *fp) {
    uint32_t objs_num = 1;
    uint32_t totalsize = re->totalsize;
    const FklRegexObj *objs = re->data;
    for (uint32_t i = 0; objs[i].type != FKL_REGEX_UNUSED; i++)
        objs_num++;
    uint32_t objs_size = objs_num * sizeof(FklRegexObj);
    uint32_t strln = totalsize - objs_size;
    if (strln)
        fprintf(fp,
                "struct{\n"
                "\tuint32_t totalsize;\n"
                "\tuint32_t pstsize;\n"
                "\tFklRegexObj objs[%u];\n"
                "\tuint8_t patrns[%u];\n"
                "}",
                objs_num, strln);
    else
        fprintf(fp,
                "struct{\n"
                "\tuint32_t totalsize;\n"
                "\tuint32_t pstsize;\n"
                "\tFklRegexObj objs[%u];\n"
                "}",
                objs_num);
    fputs(prefix ? prefix : "regex_", fp);
    fprintf(fp,
            "%" FKL_PRT64X "={\n"
            "\t.totalsize=%u,\n"
            "\t.pstsize=%u,\n"
            "\t.objs={\n",
            num, totalsize, re->pstsize);
    print_objs(objs, objs_num, fp);
    fputs("\t},\n", fp);
    if (strln) {
        const uint8_t *patrns = &((const uint8_t *)re->data)[objs_size];
        fputs("\t.patrns={\n", fp);
        print_patrns(patrns, strln, fp);
        fputs("\t},\n", fp);
    }
    fputs("};\n", fp);
}

const FklRegexCode *fklAddRegexStr(FklRegexTable *t, const FklString *str) {
    return fklAddRegexCharBuf(t, str->str, str->size);
}

const FklRegexCode *fklAddRegexCstr(FklRegexTable *t, const char *cstr) {
    return fklAddRegexCharBuf(t, cstr, strlen(cstr));
}

const FklRegexCode *fklAddRegexCharBuf(FklRegexTable *t, const char *buf,
                                       size_t len) {
    uintptr_t hashv = fklCharBufHash(buf, len);
    FklStrRegexTable *ht = &t->str_re;
    FklStrRegexTableNode *const *bkt = fklStrRegexTableBucket(ht, hashv);
    for (; *bkt; bkt = &(*bkt)->bkt_next) {
        FklStrRegexTableNode *cur = *bkt;
        if (!fklStringCharBufCmp(cur->k, len, buf)) {
            return cur->v.re;
        }
    }

    FklRegexCode *re = fklRegexCompileCharBuf(buf, len);
    if (re) {
        FklStrRegexTableNode *node = fklStrRegexTableCreateNode();
        *FKL_REMOVE_CONST(FklString *, &node->k) = fklCreateString(len, buf);
        node->v.num = ++t->num;
        node->v.re = re;
        fklStrRegexTableInsertNode(&t->str_re, hashv, node);
        fklRegexStrTableAdd2(&t->re_str, re, node->k);
    } else
        return NULL;
    return re;
}

void fklInitRegexTable(FklRegexTable *t) {
    t->num = 0;
    fklStrRegexTableInit(&t->str_re);
    fklRegexStrTableInit(&t->re_str);
}

void fklUninitRegexTable(FklRegexTable *t) {
    t->num = 0;
    fklStrRegexTableUninit(&t->str_re);
    fklRegexStrTableUninit(&t->re_str);
}

FklRegexTable *fklCreateRegexTable(void) {
    FklRegexTable *r = (FklRegexTable *)malloc(sizeof(FklRegexTable));
    FKL_ASSERT(r);
    fklInitRegexTable(r);
    return r;
}

const FklString *fklGetStringWithRegex(const FklRegexTable *t,
                                       const FklRegexCode *re, uint64_t *pnum) {
    FklString **item = fklRegexStrTableGet2(&t->re_str, re);
    if (item) {
        if (pnum) {
            FklRegexItem *str_re = fklStrRegexTableGet2(&t->str_re, *item);
            *pnum = str_re->num;
        }
        return *item;
    }
    return NULL;
}
