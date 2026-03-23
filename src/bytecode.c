#include <fakeLisp/bytecode.h>
#include <fakeLisp/common.h>
#include <fakeLisp/opcode.h>
#include <fakeLisp/symbol.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>
#include <fakeLisp/zmalloc.h>

#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void fklInitByteCode(FklByteCode *tmp, size_t len) {
    tmp->len = len;
    if (len == 0)
        tmp->code = NULL;
    else {
        tmp->code = (FklIns *)fklZcalloc(len, sizeof(FklIns));
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

    FklIns *ins = (FklIns *)fklZrealloc(bc->code, len * sizeof(FklIns));
    FKL_ASSERT(ins);

    bc->code = ins;
    if (bc->len < len) {
        memset(&ins[bc->len], 0, (len - bc->len) * sizeof(FklIns));
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

void fklInitSingleInsBcl(FklByteCodelnt *r,
        FklIns ins,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    FklByteCode *bc = &r->bc;
    bc->code[0] = ins;
    r->ls = 1;
    r->l = (FklLntItem *)fklZmalloc(sizeof(FklLntItem));
    FKL_ASSERT(r->l);
    fklInitLineNumTabNode(&r->l[0], fid, 0, line, scope);
}

FklByteCodelnt *fklCreateSingleInsBclnt(FklIns ins,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    FKL_ASSERT(fid == FKL_VM_NIL || FKL_IS_SYM(fid));
    FklByteCodelnt *r = fklCreateByteCodelnt(1);
    fklInitSingleInsBcl(r, ins, fid, line, scope);
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
        fir->code = (FklIns *)fklZrealloc(fir->code, fir->len * sizeof(FklIns));
        FKL_ASSERT(fir->code);
        memcpy(&fir->code[len], sec->code, sizeof(FklIns) * sec->len);
    }
}

void fklCodeReverseConcat(const FklByteCode *fir, FklByteCode *sec) {
    uint32_t len = fir->len;
    if (fir->len + sec->len == 0)
        sec->code = NULL;
    else {
        sec->code = (FklIns *)fklZrealloc(sec->code,
                (fir->len + sec->len) * sizeof(FklIns));
        FKL_ASSERT(sec->code);
        memmove(&sec->code[len], sec->code, sec->len * sizeof(FklIns));
        memcpy(sec->code, fir->code, len * sizeof(FklIns));
    }

    sec->len = len + sec->len;
}

FklByteCode *fklCopyByteCode(const FklByteCode *obj) {
    FklByteCode *tmp = fklCreateByteCode(obj->len);
    memcpy(tmp->code, obj->code, obj->len * sizeof(FklIns));
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
    memcpy(to->code, from->code, from->len * sizeof(FklIns));
}

void fklSetByteCodelnt(FklByteCodelnt *a, const FklByteCodelnt *b) {
    fklUninitByteCodelnt(a);
    fklSetByteCode(&a->bc, &b->bc);
    a->ls = b->ls;
    a->l = fklCopyMemory(b->l, b->ls * sizeof(FklLntItem));
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
    for (const FklIns *ins = NULL; index + r < bc->len;) {
        ins = &bc->code[index + r];
        if (FKL_INS_OP(*ins) == FKL_OP_CALL)
            break;
        if (fklIsMakeProcIns(*ins)) {
            FklInsArg arg = { 0 };
            int8_t l = fklGetInsOpArg(ins, &arg);
            r += arg.ux + l;
        } else {
            r++;
        }
    }

    return r;
}

static inline int64_t get_next(const FklIns *ins) {
    if (fklIsJmpIns(*ins)) {
        FklInsArg arg = { 0 };
        int8_t l = fklGetInsOpArg(ins, &arg);
        return arg.ix + l;
    }
    return 1;
}

int fklGetNextIns(const FklIns *c, const FklIns *ins[2]) {
    int r = 1;
    ins[0] = NULL;
    ins[1] = NULL;

    FklInsArg arg = { 0 };
    int8_t l = fklGetInsOpArg(c, &arg);
    if (fklIsCallIns(*c) || fklIsRetIns(*c)) {
        r = 0;
    } else if (fklIsMakeProcIns(*c)) {
        ins[0] = c + l + arg.ux;
    } else if (fklIsCondJmpIns(*c)) {
        ins[0] = c + l;
        ins[1] = ins[0] + arg.ix;
        if (ins[0] == ins[1])
            ins[1] = NULL;
        else
            r = 2;
    } else if (fklIsJmpIns(*c)) {
        ins[0] = c + l + arg.ix;
    } else {
        ins[0] = c + l;
    }
    return r;
}

#define I32_L8_MASK FKL_MASK1(uint32_t, 24, 0)
#define I32_L16_MASK FKL_MASK1(uint32_t, 16, 0)
#define I64_L24_MASK FKL_MASK1(uint64_t, 24, 0)

#define MAKE_EXTRA_uC(uC) (FKL_MAKE_INS_IuC(FKL_OP_EXTRA_ARG, uC))
#define MAKE_EXTRA_uB(uB) (FKL_MAKE_INS_IuB(FKL_OP_EXTRA_ARG, uB))
#define MAKE_EXTRA_uA(uA) (FKL_MAKE_INS_IuA(FKL_OP_EXTRA_ARG, uA))

static FKL_ALWAYS_INLINE void set_ins_uxx(FklIns *ins, uint64_t k) {
    ins[0] = FKL_INS_SET_uC(ins[0], k & I64_L24_MASK);
    ins[1] = MAKE_EXTRA_uC((k >> FKL_I24_WIDTH) & I64_L24_MASK);
    ins[2] = MAKE_EXTRA_uB((k >> (FKL_I24_WIDTH * 2)) & I32_L16_MASK);
}

static FKL_ALWAYS_INLINE void set_ins_ux(FklIns *ins, uint32_t k) {
    ins[0] = FKL_INS_SET_uB(ins[0], k & I32_L16_MASK);
    ins[1] = MAKE_EXTRA_uB((k >> FKL_I16_WIDTH) & I32_L16_MASK);
}

static FKL_ALWAYS_INLINE void
set_ins_2_uxx(FklIns *ins, uint64_t ux, uint64_t uy) {
    ins[0] = FKL_INS_SET_uC(ins[0], ux & I64_L24_MASK);
    ins[1] = MAKE_EXTRA_uA((ux >> FKL_I24_WIDTH) & I32_L8_MASK);
    ins[1] = FKL_INS_SET_uB(ins[1], uy & I32_L16_MASK);
    ins[2] = MAKE_EXTRA_uB((uy >> FKL_I16_WIDTH) & I32_L16_MASK);
}

int fklMakeIns(FklIns *ins, FklOpcode op, const FklInsArg *arg) {
    int64_t ix = 0;
    uint64_t ux = 0;

    uint64_t uy = 0;

    if (op == FKL_OP_DUMMY)
        return 0;
    FklOpcodeMode mode = fklGetOpcodeMode(op);
    switch (mode) {
    case FKL_OP_MODE_I:
        break;

    case FKL_OP_MODE_IsA:
    case FKL_OP_MODE_IsB:
    case FKL_OP_MODE_IsC:
    case FKL_OP_MODE_IsBB:
    case FKL_OP_MODE_IsCCB:
        ix = arg->ix;
        break;

    case FKL_OP_MODE_IuB:
    case FKL_OP_MODE_IuC:
    case FKL_OP_MODE_IuBB:
    case FKL_OP_MODE_IuCCB:
        ux = arg->ux;
        break;

    case FKL_OP_MODE_IsAuB:
        ix = arg->ix;
        uy = arg->uy;
        break;

    case FKL_OP_MODE_IuAuB:
    case FKL_OP_MODE_IuCuC:
    case FKL_OP_MODE_IuCAuBB:
        ux = arg->ux;
        uy = arg->uy;
        break;
    case FKL_OP_MODE_IxAxB:
        FKL_UNREACHABLE();
        break;
    }

    for (;;) {
        if (op == FKL_OP_DUMMY)
            return 0;
        mode = fklGetOpcodeMode(op);
        switch (mode) {
        case FKL_OP_MODE_I:
            goto emit_ins;
            break;

        case FKL_OP_MODE_IsA:
            if (ix > INT8_MAX || ix < INT8_MIN)
                op = fklGetOpcodeNext(op);
            else
                goto emit_ins;
            break;

        case FKL_OP_MODE_IsB:
            if (ix > INT16_MAX || ix < INT16_MIN)
                op = fklGetOpcodeNext(op);
            else
                goto emit_ins;
            break;

        case FKL_OP_MODE_IsC:
            if (ix > FKL_U24_MAX || ix < FKL_I24_MIN)
                op = fklGetOpcodeNext(op);
            else
                goto emit_ins;
            break;

        case FKL_OP_MODE_IsBB:
            if (ix > INT32_MAX || ix < INT32_MIN)
                op = fklGetOpcodeNext(op);
            else
                goto emit_ins;
            break;

        case FKL_OP_MODE_IsCCB:
            goto emit_ins;
            break;

        case FKL_OP_MODE_IuB:
            if (ux > UINT16_MAX)
                op = fklGetOpcodeNext(op);
            else
                goto emit_ins;
            break;

        case FKL_OP_MODE_IuC:
            if (ux > FKL_U24_MAX)
                op = fklGetOpcodeNext(op);
            else
                goto emit_ins;
            break;

        case FKL_OP_MODE_IuBB:
            if (ux > UINT32_MAX)
                op = fklGetOpcodeNext(op);
            else
                goto emit_ins;
            break;

        case FKL_OP_MODE_IuCCB:
            goto emit_ins;
            break;

        case FKL_OP_MODE_IsAuB:
            if (ix > INT8_MAX || ix < INT8_MIN || uy > UINT16_MAX)
                op = fklGetOpcodeNext(op);
            else
                goto emit_ins;
            break;

        case FKL_OP_MODE_IuAuB:
            if (ux > UINT8_MAX || uy > UINT16_MAX)
                op = fklGetOpcodeNext(op);
            else
                goto emit_ins;
            break;

        case FKL_OP_MODE_IuCuC:
            if (ux > FKL_U24_MAX || uy > FKL_U24_MAX)
                op = fklGetOpcodeNext(op);
            else
                goto emit_ins;
            break;

        case FKL_OP_MODE_IuCAuBB:
            if (ux > UINT32_MAX || uy > UINT32_MAX)
                op = fklGetOpcodeNext(op);
            else
                goto emit_ins;
            break;

        case FKL_OP_MODE_IxAxB:
            FKL_UNREACHABLE();
            break;
        }
    }

    int len = 0;
emit_ins:
    FKL_ASSERT(op != FKL_OP_DUMMY);
    ins[0] = FKL_INS_SET_OP(ins[0], op);
    len = fklGetOpcodeModeLen(op);

    switch (mode) {
    case FKL_OP_MODE_I:
        break;

    case FKL_OP_MODE_IsA:
        ins[0] = FKL_INS_SET_sA(ins[0], ix);
        break;

    case FKL_OP_MODE_IsB:
        ins[0] = FKL_INS_SET_sB(ins[0], ix);
        break;

    case FKL_OP_MODE_IsC:
        ins[0] = FKL_INS_SET_sC(ins[0], ix);
        break;

    case FKL_OP_MODE_IsBB:
        set_ins_ux(ins, ix);
        break;

    case FKL_OP_MODE_IsCCB:
        set_ins_uxx(ins, ix);
        break;

        // unsigned imm
    case FKL_OP_MODE_IuB:
        ins[0] = FKL_INS_SET_uB(ins[0], ux);
        break;

    case FKL_OP_MODE_IuC:
        ins[0] = FKL_INS_SET_uC(ins[0], ux);
        break;

    case FKL_OP_MODE_IuBB:
        set_ins_ux(ins, ux);
        break;

    case FKL_OP_MODE_IuCCB:
        set_ins_uxx(ins, ux);
        break;

        // two imm
    case FKL_OP_MODE_IsAuB:
        ins[0] = FKL_INS_SET_sA(ins[0], ix);
        ins[0] = FKL_INS_SET_uB(ins[0], uy);
        break;

    case FKL_OP_MODE_IuAuB:
        ins[0] = FKL_INS_SET_uA(ins[0], ux);
        ins[0] = FKL_INS_SET_uB(ins[0], uy);
        break;

    case FKL_OP_MODE_IuCuC:
        ins[0] = FKL_INS_SET_uC(ins[0], ux);
        ins[1] = FKL_INS_SET_uC(ins[1], uy);
        break;

    case FKL_OP_MODE_IuCAuBB:
        set_ins_2_uxx(ins, ux, uy);
        break;

    case FKL_OP_MODE_IxAxB:
        FKL_UNREACHABLE();
        break;
    }

    return len;
}

static inline int is_last_expression(uint64_t index, FklByteCode *bc) {
    uint64_t size = bc->len;
    FklIns *code = bc->code;
    for (uint64_t i = index; i < size && FKL_INS_OP(code[i]) != FKL_OP_RET;
            i += get_next(&code[i])) {
        FklOpcode op = FKL_INS_OP(code[i]);
        if (op != FKL_OP_JMP && op != FKL_OP_CLOSE_REF) {
            return 0;
        }
    }
    return 1;
}

static inline int
get_ins_op_with_op(FklOpcode op, const FklIns *ins, FklInsArg *a) {
    switch (fklGetOpcodeMode(op)) {
    case FKL_OP_MODE_I:
        return 1;
        break;
    case FKL_OP_MODE_IsA:
        a->ix = FKL_INS_sA(*ins);
        return 1;
        break;
    case FKL_OP_MODE_IuB:
        a->ux = FKL_INS_uB(*ins);
        return 1;
        break;
    case FKL_OP_MODE_IsB:
        a->ix = FKL_INS_sB(*ins);
        return 1;
        break;
    case FKL_OP_MODE_IuC:
        a->ux = FKL_INS_uC(*ins);
        return 1;
        break;
    case FKL_OP_MODE_IsC:
        a->ix = FKL_INS_sC(*ins);
        return 1;
        break;
    case FKL_OP_MODE_IuBB:
        a->ux = FKL_INS_uX(ins);
        return 2;
        break;
    case FKL_OP_MODE_IsBB:
        a->ix = FKL_INS_sX(ins);
        return 2;
        break;
    case FKL_OP_MODE_IuCCB:
        a->ux = FKL_INS_uXX(ins);
        return 3;
        break;
    case FKL_OP_MODE_IsCCB:
        a->ix = FKL_INS_sXX(ins);
        return 3;
        break;

    case FKL_OP_MODE_IsAuB:
        a->ix = FKL_INS_sA(*ins);
        a->uy = FKL_INS_uB(*ins);
        return 1;
        break;

    case FKL_OP_MODE_IuAuB:
        a->ux = FKL_INS_uA(*ins);
        a->uy = FKL_INS_uB(*ins);
        return 1;
        break;

    case FKL_OP_MODE_IuCuC:
        a->ux = FKL_INS_uC(ins[0]);
        a->uy = FKL_INS_uC(ins[1]);
        return 2;
        break;
    case FKL_OP_MODE_IuCAuBB:
        a->ux = FKL_INS_uC(ins[0])
              | (((uint32_t)FKL_INS_uA(ins[1])) << FKL_I24_WIDTH);
        a->uy = FKL_INS_uB(ins[1])
              | (((uint64_t)FKL_INS_uB(ins[2])) << FKL_I16_WIDTH);
        return 3;
        break;

    case FKL_OP_MODE_IxAxB:
        a->ux = FKL_INS_uA(*ins);
        a->uy = FKL_INS_uB(*ins);
        return 1;
        break;
    }
    return 1;
}

int fklGetInsOpArg(const FklIns *ins, FklInsArg *a) {
    return get_ins_op_with_op(FKL_INS_OP(*ins), ins, a);
}

int fklGetInsOpArgWithOp(FklOpcode op, const FklIns *ins, FklInsArg *a) {
    return get_ins_op_with_op(op, ins, a);
}

void fklScanAndSetTailCall(FklByteCode *bc) {
    for (uint64_t i = skipToCall(0, bc); i < bc->len; i += skipToCall(i, bc))
        if (is_last_expression(++i, bc)) {
            bc->code[i - 1] = FKL_INS_SET_OP(bc->code[i - 1], FKL_OP_TAIL_CALL);
        }
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
                f->l = (FklLntItem *)fklZmalloc(s->ls * sizeof(FklLntItem));
                FKL_ASSERT(f->l);
                FKL_INCREASE_ALL_SCP(s->l + 1, s->ls - 1, f->bc.len);
                memcpy(f->l, s->l, (s->ls) * sizeof(FklLntItem));
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
                    f->l = (FklLntItem *)fklZrealloc(f->l,
                            size * sizeof(FklLntItem));
                    FKL_ASSERT(f->l);
                    memcpy(&f->l[f->ls],
                            &s->l[1],
                            (s->ls - 1) * sizeof(FklLntItem));
                }
                f->ls += s->ls - 1;
            } else {
                uint32_t size = f->ls + s->ls;
                if (!size)
                    f->l = NULL;
                else {
                    f->l = (FklLntItem *)fklZrealloc(f->l,
                            size * sizeof(FklLntItem));
                    FKL_ASSERT(f->l);
                    memcpy(&f->l[f->ls], s->l, (s->ls) * sizeof(FklLntItem));
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
                s->l = (FklLntItem *)fklZmalloc(f->ls * sizeof(FklLntItem));
                FKL_ASSERT(s->l);
                memcpy(s->l, f->l, (f->ls) * sizeof(FklLntItem));
            }
        } else {
            FKL_INCREASE_ALL_SCP(s->l, s->ls, f->bc.len);
            if (f->l[f->ls - 1].line == s->l[0].line
                    && f->l[f->ls - 1].scope == s->l[0].scope
                    && f->l[f->ls - 1].fid == s->l[0].fid) {
                FklLntItem *l;
                const uint32_t len = f->ls + s->ls - 1;
                if (len == 0)
                    l = NULL;
                else {
                    l = (FklLntItem *)fklZmalloc(len * sizeof(FklLntItem));
                    FKL_ASSERT(l);
                    memcpy(l, f->l, (f->ls) * sizeof(FklLntItem));
                    memcpy(&l[f->ls],
                            &s->l[1],
                            (s->ls - 1) * sizeof(FklLntItem));
                }
                fklZfree(s->l);
                s->l = l;
                s->ls += f->ls - 1;
            } else {
                FklLntItem *l;
                const uint32_t len = f->ls + s->ls;
                if (len == 0)
                    l = NULL;
                else {
                    l = (FklLntItem *)fklZmalloc(len * sizeof(FklLntItem));
                    FKL_ASSERT(l);
                    memcpy(l, f->l, (f->ls) * sizeof(FklLntItem));
                    memcpy(&l[f->ls], s->l, (s->ls) * sizeof(FklLntItem));
                }
                fklZfree(s->l);
                s->l = l;
                s->ls += f->ls;
            }
        }
        fklCodeReverseConcat(&f->bc, &s->bc);
    }
}

void fklByteCodeInsertFront(FklIns ins, FklByteCode *bc) {
    FklIns *code = (FklIns *)fklZrealloc(bc->code, //
            (bc->len + 1) * sizeof(FklIns));
    FKL_ASSERT(code);
    for (uint64_t end = bc->len; end > 0; end--)
        code[end] = code[end - 1];
    code[0] = ins;
    bc->len++;
    bc->code = code;
}

void fklByteCodePushBack(FklByteCode *bc, FklIns ins) {
    bc->code = (FklIns *)fklZrealloc(bc->code, (bc->len + 1) * sizeof(FklIns));
    FKL_ASSERT(bc->code);
    bc->code[bc->len] = ins;
    bc->len++;
}

void fklByteCodeLntPushBackIns(FklByteCodelnt *bcl,
        const FklIns ins,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    FKL_ASSERT(fid == FKL_VM_NIL || FKL_IS_SYM(fid));
    if (!bcl->l) {
        bcl->ls = 1;
        bcl->l = (FklLntItem *)fklZmalloc(sizeof(FklLntItem));
        FKL_ASSERT(bcl->l);
        fklInitLineNumTabNode(&bcl->l[0], fid, 0, line, scope);
        fklByteCodePushBack(&bcl->bc, ins);
    } else {
        fklByteCodePushBack(&bcl->bc, ins);
    }
}

void fklByteCodeLntInsertFrontIns(const FklIns ins,
        FklByteCodelnt *bcl,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    FKL_ASSERT(fid == FKL_VM_NIL || FKL_IS_SYM(fid));
    if (!bcl->l) {
        bcl->ls = 1;
        bcl->l = (FklLntItem *)fklZmalloc(sizeof(FklLntItem));
        FKL_ASSERT(bcl->l);
        fklInitLineNumTabNode(&bcl->l[0], fid, 0, line, scope);
        fklByteCodePushBack(&bcl->bc, ins);
    } else {
        fklByteCodeInsertFront(ins, &bcl->bc);
        FKL_INCREASE_ALL_SCP(bcl->l + 1, bcl->ls - 1, 1);
    }
}

void fklByteCodeLntInsertInsAt(FklByteCodelnt *bcl, FklIns ins, uint64_t at) {
    FklByteCode *bc = &bcl->bc;
    bc->code = (FklIns *)fklZrealloc(bc->code, (++bc->len) * sizeof(FklIns));
    FKL_ASSERT(bc->code);
    FklLntItem *l = (FklLntItem *)fklFindLntItem(at, bcl->ls, bcl->l);
    FKL_ASSERT(l);
    const FklLntItem *end = &bcl->l[bcl->ls];
    for (l++; l < end; l++)
        l->scp++;
    for (uint64_t i = bc->len - 1; i > at; i++)
        bc->code[i] = bc->code[i - 1];
    bc->code[at] = ins;
}

FklIns fklByteCodeLntRemoveInsAt(FklByteCodelnt *bcl, uint64_t at) {
    FklByteCode *bc = &bcl->bc;
    FklLntItem *l = (FklLntItem *)fklFindLntItem(at, bcl->ls, bcl->l);
    FKL_ASSERT(l);
    const FklLntItem *end = &bcl->l[bcl->ls];
    for (l++; l < end; l++)
        l->scp--;
    FklIns ins = bc->code[at];
    for (uint64_t i = at; i < bc->len - 1; i++)
        bc->code[i] = bc->code[i + 1];
    if (!bc->len)
        bc->code = NULL;
    else {
        bc->code =
                (FklIns *)fklZrealloc(bc->code, (--bc->len) * sizeof(FklIns));
        FKL_ASSERT(bc->code);
    }
    return ins;
}

void fklInitLineNumTabNode(FklLntItem *n,
        FklVMvalue *fid,
        uint64_t scp,
        uint32_t line,
        uint32_t scope) {
    FKL_ASSERT(fid == FKL_VM_NIL || FKL_IS_SYM(fid));
    n->fid = fid;
    n->scp = scp;
    n->line = line;
    n->scope = scope;
}

const FklLntItem *
fklFindLntItem(uint64_t cp, size_t ls, const FklLntItem *line_numbers) {
    int64_t l = 0;
    int64_t h = ls - 1;
    int64_t mid = 0;
    int64_t end = ls - 1;
    while (l <= h) {
        mid = l + (h - l) / 2;
        const FklLntItem *cur = &line_numbers[mid];
        if (cp < cur->scp)
            h = mid - 1;
        else if (mid < end && cp >= cur[1].scp)
            l = mid + 1;
        else
            return cur;
    }
    return NULL;
}
