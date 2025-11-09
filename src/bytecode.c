#include <fakeLisp/bytecode.h>
#include <fakeLisp/common.h>
#include <fakeLisp/opcode.h>
#include <fakeLisp/symbol.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>
#include <fakeLisp/zmalloc.h>

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

void fklInitByteCode(FklByteCode *tmp, size_t len) {
    tmp->len = len;
    if (len == 0)
        tmp->code = NULL;
    else {
        tmp->code = (FklInstruction *)fklZcalloc(len, sizeof(FklInstruction));
        FKL_ASSERT(tmp->code);
    }
}

FklByteCode *fklCreateByteCode(size_t len) {
    FklByteCode *tmp = (FklByteCode *)fklZmalloc(sizeof(FklByteCode));
    FKL_ASSERT(tmp);
    fklInitByteCode(tmp, len);
    return tmp;
}

void fklByteCodeRealloc(FklByteCode *bc, size_t len) {
    if (bc->len == len)
        return;

    if (len == 0) {
        bc->len = 0;
        fklZfree(bc->code);
        bc->code = NULL;
        return;
    }

    FklInstruction *ins = (FklInstruction *)fklZrealloc(bc->code,
            len * sizeof(FklInstruction));
    FKL_ASSERT(ins);

    bc->code = ins;
    if (bc->len < len) {
        memset(&ins[bc->len], 0, (len - bc->len) * sizeof(FklInstruction));
    }

    bc->len = len;
}

void fklInitByteCodelnt(FklByteCodelnt *t, size_t len) {
    t->ls = 0;
    t->l = NULL;
    fklInitByteCode(&t->bc, len);
}

FklByteCodelnt *fklCreateByteCodelnt(size_t len) {
    FklByteCodelnt *t = (FklByteCodelnt *)fklZmalloc(sizeof(FklByteCodelnt));
    FKL_ASSERT(t);
    fklInitByteCodelnt(t, len);
    return t;
}

FklByteCodelnt *fklCreateSingleInsBclnt(FklInstruction ins,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    FKL_ASSERT(fid == NULL || FKL_IS_SYM(fid));
    FklByteCodelnt *r = fklCreateByteCodelnt(1);
    FklByteCode *bc = &r->bc;
    bc->code[0] = ins;
    r->ls = 1;
    r->l = (FklLineNumberTableItem *)fklZmalloc(sizeof(FklLineNumberTableItem));
    FKL_ASSERT(r->l);
    fklInitLineNumTabNode(&r->l[0], fid, 0, line, scope);
    return r;
}

void fklUninitByteCodelnt(FklByteCodelnt *t) {
    fklUninitByteCode(&t->bc);
    if (t->l) {
        t->ls = 0;
        fklZfree(t->l);
        t->l = NULL;
    }
}

void fklDestroyByteCodelnt(FklByteCodelnt *t) {
    fklUninitByteCodelnt(t);
    fklZfree(t);
}

void fklUninitByteCode(FklByteCode *obj) {
    obj->len = 0;
    fklZfree(obj->code);
    obj->code = NULL;
}

void fklDestroyByteCode(FklByteCode *obj) {
    fklUninitByteCode(obj);
    fklZfree(obj);
}

void fklCodeConcat(FklByteCode *fir, const FklByteCode *sec) {
    uint32_t len = fir->len;
    fir->len = sec->len + fir->len;
    if (!fir->len)
        fir->code = NULL;
    else {
        fir->code = (FklInstruction *)fklZrealloc(fir->code,
                fir->len * sizeof(FklInstruction));
        FKL_ASSERT(fir->code);
        memcpy(&fir->code[len], sec->code, sizeof(FklInstruction) * sec->len);
    }
}

void fklCodeReverseConcat(const FklByteCode *fir, FklByteCode *sec) {
    uint32_t len = fir->len;
    if (fir->len + sec->len == 0)
        sec->code = NULL;
    else {
        sec->code = (FklInstruction *)fklZrealloc(sec->code,
                (fir->len + sec->len) * sizeof(FklInstruction));
        FKL_ASSERT(sec->code);
        memmove(&sec->code[len], sec->code, sec->len * sizeof(FklInstruction));
        memcpy(sec->code, fir->code, len * sizeof(FklInstruction));
    }

    sec->len = len + sec->len;
}

FklByteCode *fklCopyByteCode(const FklByteCode *obj) {
    FklByteCode *tmp = fklCreateByteCode(obj->len);
    memcpy(tmp->code, obj->code, obj->len * sizeof(FklInstruction));
    return tmp;
}

void fklMoveByteCode(FklByteCode *to, FklByteCode *from) {
    fklUninitByteCode(to);
    *to = *from;
    from->code = NULL;
    from->len = 0;
}

void fklSetByteCode(FklByteCode *to, const FklByteCode *from) {
    fklByteCodeRealloc(to, from->len);
    memcpy(to->code, from->code, from->len * sizeof(FklInstruction));
}

void fklSetByteCodelnt(FklByteCodelnt *a, const FklByteCodelnt *b) {
    fklUninitByteCodelnt(a);
    fklSetByteCode(&a->bc, &b->bc);
    a->ls = b->ls;
    a->l = fklCopyMemory(b->l, b->ls * sizeof(FklLineNumberTableItem));
}

FklByteCodelnt *fklCopyByteCodelnt(const FklByteCodelnt *obj) {
    FklByteCodelnt *tmp = fklCreateByteCodelnt(obj->bc.len);
    fklSetByteCodelnt(tmp, obj);
    return tmp;
}

void fklMoveByteCodelnt(FklByteCodelnt *to, FklByteCodelnt *from) {
    fklUninitByteCodelnt(to);
    *to = *from;
    from->bc.code = NULL;
    from->bc.len = 0;
    from->l = NULL;
    from->ls = 0;
}

typedef struct ByteCodePrintState {
    uint32_t tc;
    uint32_t prototype_id;
    uint64_t cp;
    uint64_t cpc;
} ByteCodePrintState;

#define FKL_VECTOR_TYPE_PREFIX Bc
#define FKL_VECTOR_METHOD_PREFIX bc
#define FKL_VECTOR_ELM_TYPE ByteCodePrintState
#define FKL_VECTOR_ELM_TYPE_NAME BcPrintState
#include <fakeLisp/cont/vector.h>

static uint64_t skipToCall(uint64_t index, const FklByteCode *bc) {
    uint64_t r = 0;
    const FklInstruction *ins = NULL;
    while (index + r < bc->len
            && (ins = &bc->code[index + r])->op != FKL_OP_CALL) {
        if (fklIsPushProcIns(ins)) {
            FklInstructionArg arg = { 0 };
            int8_t l = fklGetInsOpArg(ins, &arg);
            r += arg.uy + l;
        } else
            r++;
    }
    return r;
}

static inline int64_t get_next(const FklInstruction *ins) {
    if (fklIsJmpIns(ins)) {
        FklInstructionArg arg = { 0 };
        int8_t l = fklGetInsOpArg(ins, &arg);
        return arg.ix + l;
    }
    return 1;
}

int fklGetNextIns(const FklInstruction *c, const FklInstruction *ins[2]) {
    int r = 1;
    ins[0] = NULL;
    ins[1] = NULL;

    FklInstructionArg arg = { 0 };
    int8_t l = fklGetInsOpArg(c, &arg);
    if (fklIsCallIns(c) || fklIsRetIns(c)) {
        r = 0;
    } else if (fklIsPushProcIns(c)) {
        ins[0] = c + l + arg.uy;
    } else if (fklIsCondJmpIns(c)) {
        ins[0] = c + l;
        ins[1] = ins[0] + arg.ix;
        if (ins[0] == ins[1])
            ins[1] = NULL;
        else
            r = 2;
    } else if (fklIsJmpIns(c)) {
        ins[0] = c + l + arg.ix;
    } else {
        ins[0] = c + l;
    }
    return r;
}

int fklGetNextIns2(FklInstruction *c, FklInstruction *ins[2]) {
    int r = 1;
    ins[0] = NULL;
    ins[1] = NULL;

    FklInstructionArg arg = { 0 };
    int8_t l = fklGetInsOpArg(c, &arg);
    if (c->op == FKL_OP_DUMMY || fklIsCallIns(c) || fklIsRetIns(c)
            || fklIsLoadLibIns(c)) {
        r = 0;
    } else if (fklIsPushProcIns(c)) {
        ins[0] = c + l + arg.uy;
    } else if (fklIsCondJmpIns(c)) {
        ins[0] = c + l;
        ins[1] = ins[0] + arg.ix;
        r = 2;
    } else if (fklIsJmpIns(c)) {
        ins[0] = c + l + arg.ix;
    } else {
        ins[0] = c + l;
    }
    return r;
}

static inline int is_last_expression(uint64_t index, FklByteCode *bc) {
    uint64_t size = bc->len;
    FklInstruction *code = bc->code;
    for (uint64_t i = index; i < size && code[i].op != FKL_OP_RET;
            i += get_next(&code[i]))
        if (code[i].op != FKL_OP_JMP && code[i].op != FKL_OP_CLOSE_REF)
            return 0;
    return 1;
}

static inline int get_ins_op_with_op(FklOpcode op,
        const FklInstruction *ins,
        FklInstructionArg *a) {
    switch (fklGetOpcodeMode(op)) {
    case FKL_OP_MODE_I:
        return 1;
        break;
    case FKL_OP_MODE_IsA:
        a->ix = ins->ai;
        return 1;
        break;
    case FKL_OP_MODE_IuB:
        a->ux = ins->bu;
        return 1;
        break;
    case FKL_OP_MODE_IsB:
        a->ix = ins->bi;
        return 1;
        break;
    case FKL_OP_MODE_IuC:
        a->ux = FKL_GET_INS_UC(ins);
        return 1;
        break;
    case FKL_OP_MODE_IsC:
        a->ix = FKL_GET_INS_IC(ins);
        return 1;
        break;
    case FKL_OP_MODE_IuBB:
        a->ux = FKL_GET_INS_UX(ins);
        return 2;
        break;
    case FKL_OP_MODE_IsBB:
        a->ix = FKL_GET_INS_IX(ins);
        return 2;
        break;
    case FKL_OP_MODE_IuCCB:
        a->ux = FKL_GET_INS_UXX(ins);
        return 3;
        break;
    case FKL_OP_MODE_IsCCB:
        a->ix = FKL_GET_INS_IXX(ins);
        return 3;
        break;

    case FKL_OP_MODE_IsAuB:
        a->ix = ins->ai;
        a->uy = ins->bu;
        return 1;
        break;

    case FKL_OP_MODE_IuAuB:
        a->ux = ins->au;
        a->uy = ins->bu;
        return 1;
        break;

    case FKL_OP_MODE_IuCuC:
        a->ux = FKL_GET_INS_UC(ins);
        a->uy = FKL_GET_INS_UC(ins + 1);
        return 2;
        break;
    case FKL_OP_MODE_IuCAuBB:
        a->ux = FKL_GET_INS_UC(ins) | (((uint32_t)ins[1].au) << FKL_I24_WIDTH);
        a->uy = ins[1].bu | (((uint64_t)ins[2].bu) << FKL_I16_WIDTH);
        return 3;
        break;
    case FKL_OP_MODE_IuCAuBCC:
        a->ux = FKL_GET_INS_UC(ins) | (((uint32_t)ins[1].au) << FKL_I24_WIDTH);
        a->uy = ins[1].bu | (((uint64_t)ins[2].au) << FKL_I16_WIDTH)
              | (((uint64_t)ins[2].bu) << FKL_I24_WIDTH)
              | (((uint64_t)ins[3].au) << (FKL_I16_WIDTH * 2 + FKL_BYTE_WIDTH))
              | (((uint64_t)ins[3].bu)
                      << (FKL_I16_WIDTH * 2 + FKL_BYTE_WIDTH * 2));
        return 4;
        break;

    case FKL_OP_MODE_IxAxB:
        a->ux = ins->au;
        a->uy = ins->bu;
        return 1;
        break;
    }
    return 1;
}

int fklGetInsOpArg(const FklInstruction *ins, FklInstructionArg *a) {
    return get_ins_op_with_op(ins->op, ins, a);
}

int fklGetInsOpArgWithOp(FklOpcode op,
        const FklInstruction *ins,
        FklInstructionArg *a) {
    return get_ins_op_with_op(op, ins, a);
}

void fklScanAndSetTailCall(FklByteCode *bc) {
    for (uint64_t i = skipToCall(0, bc); i < bc->len; i += skipToCall(i, bc))
        if (is_last_expression(++i, bc))
            bc->code[i - 1].op = FKL_OP_TAIL_CALL;
}

void fklIncreaseScpOfByteCodelnt(FklByteCodelnt *o, uint64_t size) {
    uint32_t i = 0;
    for (; i < o->ls; i++)
        o->l[i].scp += size;
}

#define FKL_INCREASE_ALL_SCP(l, ls, s)                                         \
    for (size_t i = 0; i < (ls); i++)                                          \
    (l)[i].scp += (s)

void fklCodeLntConcat(FklByteCodelnt *f, const FklByteCodelnt *s) {
    if (s->bc.len) {
        if (!f->l) {
            f->ls = s->ls;
            if (s->ls == 0)
                f->l = NULL;
            else {
                f->l = (FklLineNumberTableItem *)fklZmalloc(
                        s->ls * sizeof(FklLineNumberTableItem));
                FKL_ASSERT(f->l);
                FKL_INCREASE_ALL_SCP(s->l + 1, s->ls - 1, f->bc.len);
                memcpy(f->l, s->l, (s->ls) * sizeof(FklLineNumberTableItem));
            }
        } else {
            FKL_INCREASE_ALL_SCP(s->l, s->ls, f->bc.len);
            if (f->l[f->ls - 1].line == s->l[0].line
                    && f->l[f->ls - 1].scope == s->l[0].scope
                    && f->l[f->ls - 1].fid == s->l[0].fid) {
                uint32_t size = f->ls + s->ls - 1;
                if (!size)
                    f->l = NULL;
                else {
                    f->l = (FklLineNumberTableItem *)fklZrealloc(f->l,
                            size * sizeof(FklLineNumberTableItem));
                    FKL_ASSERT(f->l);
                    memcpy(&f->l[f->ls],
                            &s->l[1],
                            (s->ls - 1) * sizeof(FklLineNumberTableItem));
                }
                f->ls += s->ls - 1;
            } else {
                uint32_t size = f->ls + s->ls;
                if (!size)
                    f->l = NULL;
                else {
                    f->l = (FklLineNumberTableItem *)fklZrealloc(f->l,
                            size * sizeof(FklLineNumberTableItem));
                    FKL_ASSERT(f->l);
                    memcpy(&f->l[f->ls],
                            s->l,
                            (s->ls) * sizeof(FklLineNumberTableItem));
                }
                f->ls += s->ls;
            }
        }
        fklCodeConcat(&f->bc, &s->bc);
    }
}

void fklCodeLntReverseConcat(const FklByteCodelnt *f, FklByteCodelnt *s) {
    if (f->bc.len) {
        if (!s->l) {
            s->ls = f->ls;
            if (f->ls == 0)
                s->l = NULL;
            else {
                s->l = (FklLineNumberTableItem *)fklZmalloc(
                        f->ls * sizeof(FklLineNumberTableItem));
                FKL_ASSERT(s->l);
                memcpy(s->l, f->l, (f->ls) * sizeof(FklLineNumberTableItem));
            }
        } else {
            FKL_INCREASE_ALL_SCP(s->l, s->ls, f->bc.len);
            if (f->l[f->ls - 1].line == s->l[0].line
                    && f->l[f->ls - 1].scope == s->l[0].scope
                    && f->l[f->ls - 1].fid == s->l[0].fid) {
                FklLineNumberTableItem *l;
                const uint32_t len = f->ls + s->ls - 1;
                if (len == 0)
                    l = NULL;
                else {
                    l = (FklLineNumberTableItem *)fklZmalloc(
                            len * sizeof(FklLineNumberTableItem));
                    FKL_ASSERT(l);
                    memcpy(l, f->l, (f->ls) * sizeof(FklLineNumberTableItem));
                    memcpy(&l[f->ls],
                            &s->l[1],
                            (s->ls - 1) * sizeof(FklLineNumberTableItem));
                }
                fklZfree(s->l);
                s->l = l;
                s->ls += f->ls - 1;
            } else {
                FklLineNumberTableItem *l;
                const uint32_t len = f->ls + s->ls;
                if (len == 0)
                    l = NULL;
                else {
                    l = (FklLineNumberTableItem *)fklZmalloc(
                            len * sizeof(FklLineNumberTableItem));
                    FKL_ASSERT(l);
                    memcpy(l, f->l, (f->ls) * sizeof(FklLineNumberTableItem));
                    memcpy(&l[f->ls],
                            s->l,
                            (s->ls) * sizeof(FklLineNumberTableItem));
                }
                fklZfree(s->l);
                s->l = l;
                s->ls += f->ls;
            }
        }
        fklCodeReverseConcat(&f->bc, &s->bc);
    }
}

void fklByteCodeInsertFront(FklInstruction ins, FklByteCode *bc) {
    FklInstruction *code = (FklInstruction *)fklZrealloc(bc->code,
            (bc->len + 1) * sizeof(FklInstruction));
    FKL_ASSERT(code);
    for (uint64_t end = bc->len; end > 0; end--)
        code[end] = code[end - 1];
    code[0] = ins;
    bc->len++;
    bc->code = code;
}

void fklByteCodePushBack(FklByteCode *bc, FklInstruction ins) {
    bc->code = (FklInstruction *)fklZrealloc(bc->code,
            (bc->len + 1) * sizeof(FklInstruction));
    FKL_ASSERT(bc->code);
    bc->code[bc->len] = ins;
    bc->len++;
}

void fklByteCodeLntPushBackIns(FklByteCodelnt *bcl,
        const FklInstruction *ins,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    FKL_ASSERT(fid == NULL || FKL_IS_SYM(fid));
    if (!bcl->l) {
        bcl->ls = 1;
        bcl->l = (FklLineNumberTableItem *)fklZmalloc(
                sizeof(FklLineNumberTableItem));
        FKL_ASSERT(bcl->l);
        fklInitLineNumTabNode(&bcl->l[0], fid, 0, line, scope);
        fklByteCodePushBack(&bcl->bc, *ins);
    } else {
        fklByteCodePushBack(&bcl->bc, *ins);
    }
}

void fklByteCodeLntInsertFrontIns(const FklInstruction *ins,
        FklByteCodelnt *bcl,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    FKL_ASSERT(fid == NULL || FKL_IS_SYM(fid));
    if (!bcl->l) {
        bcl->ls = 1;
        bcl->l = (FklLineNumberTableItem *)fklZmalloc(
                sizeof(FklLineNumberTableItem));
        FKL_ASSERT(bcl->l);
        fklInitLineNumTabNode(&bcl->l[0], fid, 0, line, scope);
        fklByteCodePushBack(&bcl->bc, *ins);
    } else {
        fklByteCodeInsertFront(*ins, &bcl->bc);
        FKL_INCREASE_ALL_SCP(bcl->l + 1, bcl->ls - 1, 1);
    }
}

void fklByteCodeLntInsertInsAt(FklByteCodelnt *bcl,
        FklInstruction ins,
        uint64_t at) {
    FklByteCode *bc = &bcl->bc;
    bc->code = (FklInstruction *)fklZrealloc(bc->code,
            (++bc->len) * sizeof(FklInstruction));
    FKL_ASSERT(bc->code);
    FklLineNumberTableItem *l =
            (FklLineNumberTableItem *)fklFindLineNumTabNode(at,
                    bcl->ls,
                    bcl->l);
    FKL_ASSERT(l);
    const FklLineNumberTableItem *end = &bcl->l[bcl->ls];
    for (l++; l < end; l++)
        l->scp++;
    for (uint64_t i = bc->len - 1; i > at; i++)
        bc->code[i] = bc->code[i - 1];
    bc->code[at] = ins;
}

FklInstruction fklByteCodeLntRemoveInsAt(FklByteCodelnt *bcl, uint64_t at) {
    FklByteCode *bc = &bcl->bc;
    FklLineNumberTableItem *l =
            (FklLineNumberTableItem *)fklFindLineNumTabNode(at,
                    bcl->ls,
                    bcl->l);
    FKL_ASSERT(l);
    const FklLineNumberTableItem *end = &bcl->l[bcl->ls];
    for (l++; l < end; l++)
        l->scp--;
    FklInstruction ins = bc->code[at];
    for (uint64_t i = at; i < bc->len - 1; i++)
        bc->code[i] = bc->code[i + 1];
    if (!bc->len)
        bc->code = NULL;
    else {
        bc->code = (FklInstruction *)fklZrealloc(bc->code,
                (--bc->len) * sizeof(FklInstruction));
        FKL_ASSERT(bc->code);
    }
    return ins;
}

void fklInitLineNumTabNode(FklLineNumberTableItem *n,
        FklVMvalue *fid,
        uint64_t scp,
        uint32_t line,
        uint32_t scope) {
    FKL_ASSERT(fid == NULL || FKL_IS_SYM(fid));
    n->fid = fid;
    n->scp = scp;
    n->line = line;
    n->scope = scope;
}

const FklLineNumberTableItem *fklFindLineNumTabNode(uint64_t cp,
        size_t ls,
        const FklLineNumberTableItem *line_numbers) {
    int64_t l = 0;
    int64_t h = ls - 1;
    int64_t mid = 0;
    int64_t end = ls - 1;
    while (l <= h) {
        mid = l + (h - l) / 2;
        const FklLineNumberTableItem *cur = &line_numbers[mid];
        if (cp < cur->scp)
            h = mid - 1;
        else if (mid < end && cp >= cur[1].scp)
            l = mid + 1;
        else
            return cur;
    }
    return NULL;
}
