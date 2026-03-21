#include <fakeLisp/bytecode.h>
#include <fakeLisp/common.h>
#include <fakeLisp/opcode.h>
#include <fakeLisp/optimizer.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>
#include <fakeLisp/zmalloc.h>

#include <stdlib.h>

static inline void push_ins_ln(FklByteCodeBuffer *buf, const FklInsLn *ins) {
    if (buf->size == buf->capacity) {
        buf->capacity <<= 1;
        FklInsLn *new_base = (FklInsLn *)fklZrealloc(buf->base,
                buf->capacity * sizeof(FklInsLn));
        FKL_ASSERT(new_base);
        buf->base = new_base;
    }

    buf->base[buf->size++] = *ins;
}

static inline int set_insln_to_ins(const FklInsLn *cur_ins_ln, FklIns *ins) {
    const FklIns cur_ins = cur_ins_ln->ins;
    FklOpcode op = FKL_INS_OP(cur_ins);
    int ol = fklGetOpcodeModeLen(op);

    for (int i = 0; i < ol; i++)
        ins[i] = cur_ins_ln[i].ins;
    return ol;
}

static inline int set_ins_to_insln(const FklIns *ins,
        const FklInsLn *peephole,
        FklInsLn *output) {
    FklOpcode op = FKL_INS_OP(ins[0]);
    int ol = fklGetOpcodeModeLen(op);
    for (int i = 0; i < ol; ++i) {
        output[i] = peephole[i];
        output[i].ins = ins[i];
    }

    return ol;
}

static inline int set_jmp_backward(FklOpcode op, int64_t len, FklIns *new_ins) {
    len = -len - 1;
    if (len >= FKL_I24_MIN)
        return FKL_MAKE_INS(new_ins, op, .ix = len);
    len -= 1;
    if (len >= INT32_MIN)
        return FKL_MAKE_INS(new_ins, op, .ix = len);
    len -= 1;
    return FKL_MAKE_INS(new_ins, op, .ix = len);
}

static inline FklByteCodeBuffer *recompute_jmp_target(FklByteCodeBuffer *a,
        FklByteCodeBuffer *b) {
    int change;
    FklInsArg arg;
    FklInsLn tmp_ins_ln;
    FklIns ins[4] = { FKL_INS_STATIC_INIT };
    FklIns new_ins[4] = { FKL_INS_STATIC_INIT };

    do {
        change = 0;
        b->size = 0;

        for (uint64_t i = 0; i < a->size;) {
            FklInsLn *cur = &a->base[i];
            tmp_ins_ln = *cur;
            if (cur->jmp_to) {
                int ol = set_insln_to_ins(cur, ins);
                fklGetInsOpArg(ins, &arg);

                if (fklIsMakeProcIns(*ins)) {
                    FKL_ASSERT(arg.ux);
                    FklInsLn *jmp_to = &cur[ol];
                    uint64_t end = a->size - i - ol;
                    uint64_t jmp_offset = 0;
                    for (; jmp_offset < end
                            && jmp_to[jmp_offset].block_id != cur->jmp_to;
                            jmp_offset++)
                        ;
                    FKL_ASSERT(jmp_offset < end);

                    int nl = FKL_MAKE_INS(new_ins,
                            FKL_OP_MAKE_PROC,
                            .ux = jmp_offset);

                    if (jmp_offset == (uint64_t)arg.ux && ol == nl)
                        goto no_change;
                    change = 1;

                    tmp_ins_ln.ins = new_ins[0];
                    push_ins_ln(b, &tmp_ins_ln);
                    for (int i = 1; i < nl; i++)
                        fklByteCodeBufferPush(b,
                                &new_ins[i],
                                cur->line,
                                cur->scope,
                                cur->fid);
                    goto done;
                }

                if (!fklIsJmpIns(*ins) && !fklIsCondJmpIns(*ins))
                    goto no_change;
                FKL_ASSERT(arg.ix);
                if (arg.ix > 0) {
                    FklInsLn *jmp_to = &cur[ol];
                    uint64_t end = a->size - i - ol;
                    uint64_t jmp_offset = 0;
                    for (; jmp_offset < end
                            && jmp_to[jmp_offset].block_id != cur->jmp_to;
                            jmp_offset++)
                        ;
                    FKL_ASSERT(jmp_offset < end);

                    FklOpcode op = FKL_INS_OP(cur->ins);
                    if (fklIsJmpIfTrueIns(cur->ins)) {
                        op = FKL_OP_JMP_IF_TRUE;
                    } else if (fklIsJmpIfFalseIns(cur->ins)) {
                        op = FKL_OP_JMP_IF_FALSE;
                    } else {
                        op = FKL_OP_JMP;
                    }

                    int nl = FKL_MAKE_INS(new_ins, op, .ix = jmp_offset);

                    if (jmp_offset == (uint64_t)arg.ix && ol == nl)
                        goto no_change;
                    change = 1;

                    tmp_ins_ln.ins = new_ins[0];
                    push_ins_ln(b, &tmp_ins_ln);
                    for (int i = 1; i < nl; i++)
                        fklByteCodeBufferPush(b,
                                &new_ins[i],
                                cur->line,
                                cur->scope,
                                cur->fid);
                } else {
                    int64_t offset = 1;
                    for (; (int64_t)i >= offset
                            && cur[-offset].block_id != cur->jmp_to;
                            offset++)
                        ;
                    FKL_ASSERT((int64_t)i >= offset);

                    FklOpcode op;

                    op = fklIsJmpIfTrueIns(cur->ins)  ? FKL_OP_JMP_IF_TRUE
                       : fklIsJmpIfFalseIns(cur->ins) ? FKL_OP_JMP_IF_FALSE
                                                      : FKL_OP_JMP;

                    int nl = set_jmp_backward(op, offset, new_ins);
                    FklInsArg new_arg;
                    fklGetInsOpArg(new_ins, &new_arg);

                    if (new_arg.ix == arg.ix && ol == nl)
                        goto no_change;
                    change = 1;

                    tmp_ins_ln.ins = new_ins[0];
                    push_ins_ln(b, &tmp_ins_ln);
                    for (int i = 1; i < nl; i++)
                        fklByteCodeBufferPush(b,
                                &new_ins[i],
                                cur->line,
                                cur->scope,
                                cur->fid);
                }
            done:
                i += ol;
            } else {
            no_change:
                push_ins_ln(b, cur);
                i++;
            }
        }

        FklByteCodeBuffer *tmp = a;
        a = b;
        b = tmp;
    } while (change);

    return a;
}

#if 0
#include <math.h>
static inline void print_basic_block(FklByteCodeBuffer* buf,FILE* fp)
{
	fputs("\n------\n",fp);
	FklInstructionArg ins_arg;
	uint64_t codeLen=buf->size;
	int numLen=codeLen?(int)(log10(codeLen)+1):1;
	for(uint64_t i=0;i<buf->size;i++)
	{
		FklInsLn* cur=&buf->base[i];
		if(cur->block_id)
			fprintf(fp,"\nblock_id: %u\n",cur->block_id);
		FklInstruction ins[4]={FKL_INS_STATIC_INIT};
		int l=fklGetOpcodeModeLen(cur->ins.op);
		for(int i=0;i<l;i++)
			ins[i]=cur[i].ins;
		fklGetInsOpArg(ins,&ins_arg);
		fprintf(fp,"%-*"PRId64":\t%-17s ",numLen,i,fklGetOpcodeName(ins->op));
		switch(fklGetOpcodeMode(ins->op))
		{
			case FKL_OP_MODE_IsA:
			case FKL_OP_MODE_IsB:
			case FKL_OP_MODE_IsC:
			case FKL_OP_MODE_IsBB:
			case FKL_OP_MODE_IsCCB:
				fprintf(fp,"%"PRId64,ins_arg.ix);
				break;
			case FKL_OP_MODE_IuB:
			case FKL_OP_MODE_IuC:
			case FKL_OP_MODE_IuBB:
			case FKL_OP_MODE_IuCCB:
				fprintf(fp,"%"PRIu64,ins_arg.ux);
				break;
			case FKL_OP_MODE_IuAuB:
			case FKL_OP_MODE_IuCuC:
			case FKL_OP_MODE_IuCAuBB:
			case FKL_OP_MODE_IuCAuBCC:
				fprintf(fp,"%"PRIu64"\t%"PRIu64,ins_arg.ux,ins_arg.uy);
				break;
			case FKL_OP_MODE_I:
				break;

			case FKL_OP_MODE_IxAxB:
				fprintf(fp,"%#"PRIx64"\t%#"PRIx64,ins_arg.ux,ins_arg.uy);
				break;
		}
		putc('\n',fp);
		if(cur->jmp_to)
			fprintf(fp,"jmp_to: %u\n",cur->jmp_to);
	}
}
#endif

void fklRecomputeInsImm(FklByteCodelnt *bcl,
        void *ctx,
        FklRecomputeInsImmPredicate predicate,
        FklRecomputeInsImmFunc func) {
    FklInsArg arg;

    FklIns cur[4] = { FKL_INS_STATIC_INIT };
    FklIns new_ins[4] = { FKL_INS_STATIC_INIT };

    FklByteCodeBuffer buf;
    fklInitByteCodeBufferWith(&buf, bcl);
    fklByteCodeBufferScanAndSetBasicBlock(&buf);

    FklByteCodeBuffer new_buf;
    fklInitByteCodeBuffer(&new_buf, bcl->bc.len);

    FklInsLn tmp_ins_ln;

    FklInsLn *cur_ins_ln = buf.base;
    const FklInsLn *end = &cur_ins_ln[buf.size];
    while (cur_ins_ln < end) {
        FklIns cur_ins = cur_ins_ln->ins;
        tmp_ins_ln = *cur_ins_ln;
        FklOpcode op = FKL_INS_OP(cur_ins);
        if (predicate(op)) {
            int ol = fklGetOpcodeModeLen(op);

            for (int i = 0; i < ol; i++)
                cur[i] = cur_ins_ln[i].ins;

            FKL_ASSERT(op != FKL_OP_EXTRA_ARG);

            fklGetInsOpArg(cur, &arg);
            FklOpcodeMode mode = FKL_OP_MODE_I;
            if (func(ctx, &op, &mode, &arg)) {
                fklUninitByteCodeBuffer(&buf);
                fklUninitByteCodeBuffer(&new_buf);
                return;
            }

            int nl = 0;
            switch (mode) {
            case FKL_OP_MODE_IsA:
            case FKL_OP_MODE_IsB:
            case FKL_OP_MODE_IsC:
            case FKL_OP_MODE_IsBB:
            case FKL_OP_MODE_IsCCB:
                nl = FKL_MAKE_INS(new_ins, op, .ix = arg.ix);
                break;
            case FKL_OP_MODE_IuB:
            case FKL_OP_MODE_IuC:
            case FKL_OP_MODE_IuBB:
            case FKL_OP_MODE_IuCCB:
                nl = FKL_MAKE_INS(new_ins, op, .ux = arg.ux);
                break;
            case FKL_OP_MODE_IsAuB:
                nl = FKL_MAKE_INS(new_ins, op, .ix = arg.ix, .uy = arg.uy);
                break;
            case FKL_OP_MODE_IuAuB:
            case FKL_OP_MODE_IuCuC:
            case FKL_OP_MODE_IuCAuBB:
                nl = FKL_MAKE_INS(new_ins, op, .ux = arg.ux, .uy = arg.uy);
                break;

            case FKL_OP_MODE_I:
            case FKL_OP_MODE_IxAxB:
                FKL_UNREACHABLE();
            }

            tmp_ins_ln.ins = new_ins[0];
            push_ins_ln(&new_buf, &tmp_ins_ln);
            for (int i = 1; i < nl; i++)
                fklByteCodeBufferPush(&new_buf,
                        &new_ins[i],
                        cur_ins_ln->line,
                        cur_ins_ln->scope,
                        cur_ins_ln->fid);

            cur_ins_ln += ol;
        } else {
            push_ins_ln(&new_buf, cur_ins_ln);
            cur_ins_ln++;
        }
    }

    FklByteCodeBuffer *fin = recompute_jmp_target(&new_buf, &buf);
    fklSetByteCodelntWithBuf(bcl, fin);
    fklUninitByteCodeBuffer(&buf);
    fklUninitByteCodeBuffer(&new_buf);
}

void fklInitByteCodeBuffer(FklByteCodeBuffer *buf, size_t capacity) {
    FKL_ASSERT(capacity);
    buf->capacity = capacity;
    buf->size = 0;
    buf->base = (FklInsLn *)fklZmalloc(capacity * sizeof(FklInsLn));
    FKL_ASSERT(buf->base);
}

FklByteCodeBuffer *fklCreateByteCodeBuffer(size_t capacity) {
    FklByteCodeBuffer *buf =
            (FklByteCodeBuffer *)fklZmalloc(sizeof(FklByteCodeBuffer));
    FKL_ASSERT(buf);
    fklInitByteCodeBuffer(buf, capacity);
    return buf;
}

static inline void bytecode_buffer_reserve(FklByteCodeBuffer *buf,
        size_t target) {
    if (buf->capacity >= target)
        return;
    size_t target_capacity = buf->capacity << 1;
    if (target_capacity < target)
        target_capacity = target;
    FklInsLn *new_base = (FklInsLn *)fklZrealloc(buf->base,
            target_capacity * sizeof(FklInsLn));
    FKL_ASSERT(new_base);
    buf->base = new_base;
    buf->capacity = target_capacity;
}

void fklSetByteCodeBuffer(FklByteCodeBuffer *buf, const FklByteCodelnt *bcl) {
    size_t len = bcl->bc.len;
    bytecode_buffer_reserve(buf, len);
    buf->size = len;

    uint64_t pc = 0;
    uint64_t ln_idx = 0;
    uint64_t ln_idx_end = bcl->ls - 1;
    const FklLntItem *ln_item_base = bcl->l;
    const FklIns *ins_base = bcl->bc.code;
    FklInsLn *ins_ln_base = buf->base;
    for (; pc < len; pc++) {
        if (ln_idx < ln_idx_end && pc >= ln_item_base[ln_idx + 1].scp)
            ln_idx++;

        ins_ln_base[pc] = (FklInsLn){
            .ins = ins_base[pc],
            .line = ln_item_base[ln_idx].line,
            .scope = ln_item_base[ln_idx].scope,
            .fid = ln_item_base[ln_idx].fid,
            .block_id = 0,
            .jmp_to = 0,
        };
    }
}

void fklInitByteCodeBufferWith(FklByteCodeBuffer *buf,
        const FklByteCodelnt *bcl) {
    size_t len = bcl->bc.len;
    fklInitByteCodeBuffer(buf, len);
    buf->size = len;

    uint64_t pc = 0;
    uint64_t ln_idx = 0;
    uint64_t ln_idx_end = bcl->ls - 1;
    const FklLntItem *ln_item_base = bcl->l;
    const FklIns *ins_base = bcl->bc.code;
    FklInsLn *ins_ln_base = buf->base;
    for (; pc < len; pc++) {
        if (ln_idx < ln_idx_end && pc >= ln_item_base[ln_idx + 1].scp)
            ln_idx++;

        ins_ln_base[pc] = (FklInsLn){
            .ins = ins_base[pc],
            .line = ln_item_base[ln_idx].line,
            .scope = ln_item_base[ln_idx].scope,
            .fid = ln_item_base[ln_idx].fid,
            .block_id = 0,
            .jmp_to = 0,
        };
    }
}

FklByteCodeBuffer *fklCreateByteCodeBufferWith(const FklByteCodelnt *bcl) {
    FklByteCodeBuffer *buf =
            (FklByteCodeBuffer *)fklZmalloc(sizeof(FklByteCodeBuffer));
    FKL_ASSERT(buf);
    fklInitByteCodeBufferWith(buf, bcl);
    return buf;
}

void fklByteCodeBufferPush(FklByteCodeBuffer *buf,
        const FklIns *ins,
        uint32_t line,
        uint32_t scope,
        FklVMvalue *fid) {
    if (buf->size == buf->capacity) {
        buf->capacity <<= 1;
        FklInsLn *new_base = (FklInsLn *)fklZrealloc(buf->base,
                buf->capacity * sizeof(FklInsLn));
        FKL_ASSERT(new_base);
        buf->base = new_base;
    }

    buf->base[buf->size++] = (FklInsLn){
        .ins = *ins,
        .line = line,
        .scope = scope,
        .fid = fid,
        .block_id = 0,
        .jmp_to = 0,
    };
}

void fklUninitByteCodeBuffer(FklByteCodeBuffer *buf) {
    buf->capacity = 0;
    buf->size = 0;
    fklZfree(buf->base);
    buf->base = NULL;
}

void fklDestroyByteCodeBuffer(FklByteCodeBuffer *buf) {
    fklUninitByteCodeBuffer(buf);
    fklZfree(buf);
}

void fklSetByteCodelntWithBuf(FklByteCodelnt *bcl,
        const FklByteCodeBuffer *buf) {
    FKL_ASSERT(buf->size);
    FklLntItem *ln =
            (FklLntItem *)fklZrealloc(bcl->l, buf->size * sizeof(FklLntItem));
    FKL_ASSERT(ln);

    FklByteCode *bc = &bcl->bc;
    bc->len = buf->size;
    FklIns *new_code =
            (FklIns *)fklZrealloc(bc->code, bc->len * sizeof(FklIns));
    FKL_ASSERT(new_code);

    bc->code = new_code;
    bcl->l = ln;

    const FklInsLn *ins_ln_base = buf->base;
    uint64_t ln_idx = 0;
    ln[ln_idx] = (FklLntItem){ .fid = ins_ln_base->fid,
        .scp = 0,
        .line = ins_ln_base->line,
        .scope = ins_ln_base->scope };
    for (size_t i = 0; i < buf->size; i++) {
        const FklInsLn *cur = &ins_ln_base[i];
        if (ln[ln_idx].scope != cur->scope || ln[ln_idx].line != cur->line
                || ln[ln_idx].fid != cur->fid) {
            ln_idx++;
            ln[ln_idx] = (FklLntItem){ .fid = cur->fid,
                .scp = i,
                .line = cur->line,
                .scope = cur->scope };
        }
        new_code[i] = cur->ins;
    }

    uint32_t ls = ln_idx + 1;

    bcl->ls = ls;
    FklLntItem *nnl = (FklLntItem *)fklZrealloc(ln, ls * sizeof(FklLntItem));
    FKL_ASSERT(nnl);
    bcl->l = nnl;
}

uint32_t fklByteCodeBufferScanAndSetBasicBlock(FklByteCodeBuffer *buf) {
    FKL_ASSERT(buf->size);
    FklInsArg arg;
    buf->base[0].block_id = 1;
    uint32_t next_block_id = 1;
    FklInsLn *base = buf->base;
    FklIns ins[4] = { FKL_INS_STATIC_INIT };

    for (uint64_t i = 0; i < buf->size;) {
        FklInsLn *cur = &base[i];
        if (fklIsJmpIns(cur->ins) || fklIsCondJmpIns(cur->ins)) {
            int l = fklGetOpcodeModeLen(FKL_INS_OP(cur->ins));
            for (int i = 0; i < l; i++)
                ins[i] = cur[i].ins;
            fklGetInsOpArg(ins, &arg);
            if (!cur[l].block_id)
                cur[l].block_id = ++next_block_id;
            uint64_t jmp_to = i + arg.ix + l;
            FklInsLn *jmp_to_ins = &base[jmp_to];
            if (!jmp_to_ins->block_id)
                jmp_to_ins->block_id = ++next_block_id;
            cur->jmp_to = jmp_to_ins->block_id;
            i += l;
        } else if (fklIsMakeProcIns(cur->ins)) {
            int l = fklGetOpcodeModeLen(FKL_INS_OP(cur->ins));
            for (int i = 0; i < l; i++)
                ins[i] = cur[i].ins;
            fklGetInsOpArg(ins, &arg);
            if (!cur[l].block_id)
                cur[l].block_id = ++next_block_id;
            uint64_t jmp_to = i + arg.ux + l;
            FklInsLn *jmp_to_ins = &base[jmp_to];
            if (!jmp_to_ins->block_id)
                jmp_to_ins->block_id = ++next_block_id;
            cur->jmp_to = jmp_to_ins->block_id;
            i += l;
        } else {
            i++;
        }
    }
    return next_block_id;
}

FklByteCodelnt *fklCreateByteCodelntFromBuf(const FklByteCodeBuffer *buf) {
    FklByteCodelnt *bcl = fklCreateByteCodelnt(0);
    fklSetByteCodelntWithBuf(bcl, buf);
    return bcl;
}

static inline void scan_and_set_block_start(FklByteCodeBuffer *buf,
        uint64_t *block_start) {
    for (uint64_t i = 0; i < buf->size; i++)
        if (buf->base[i].block_id)
            block_start[buf->base[i].block_id - 1] = i;
}

#define PEEPHOLE_SIZE (12)

typedef uint32_t (*PeepholeOptimizationPredicate)(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k);
typedef uint32_t (*PeepholeOptimizationOutput)(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k,
        FklInsLn *output);

struct PeepholeOptimizer {
    PeepholeOptimizationPredicate predicate;
    PeepholeOptimizationOutput output;
    enum {
        PEEPHOLE_SHOULD_IN_ONE_BLOCK = 0,
        PEEPHOLE_CAN_IN_BLOCKS,
    } block_state;
};

static uint32_t not3_predicate(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k) {
    if (k < 3)
        return 0;
    if (FKL_INS_OP(peephole[0].ins) == FKL_OP_NOT
            && FKL_INS_OP(peephole[1].ins) == FKL_OP_NOT
            && FKL_INS_OP(peephole[2].ins) == FKL_OP_NOT)
        return 3;
    return 0;
}

static uint32_t not3_output(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k,
        FklInsLn *output) {
    output[0] = peephole[0];
    return 1;
}

static FKL_ALWAYS_INLINE int is_in_u16_range(uint32_t i) {
    return i <= UINT16_MAX;
}

static uint32_t inc_or_dec_loc_predicate(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k) {
    if (k < 3)
        return 0;
    if (FKL_INS_OP(peephole[0].ins) == FKL_OP_GET_LOC
            && FKL_INS_OP(peephole[1].ins) == FKL_OP_ADDK
            && FKL_INS_OP(peephole[2].ins) == FKL_OP_PUT_LOC
            && FKL_INS_uC(peephole[0].ins) == FKL_INS_uC(peephole[2].ins)
            && is_in_u16_range(FKL_INS_uC(peephole[0].ins)))
        return 3;
    return 0;
}

static uint32_t inc_or_dec_loc_output(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k,
        FklInsLn *output) {
    uint32_t uB = FKL_INS_uC(peephole[0].ins);
    int8_t sA = FKL_INS_sA(peephole[1].ins);
    output[0] = peephole[0];
    output[0].ins = FKL_MAKE_INS_IsA(FKL_OP_ADDK_LOC, sA);
    output[0].ins = FKL_INS_SET_uB(output[0].ins, uB);
    return 1;
}

static FKL_ALWAYS_INLINE int is_drop1(const FklIns i) {
    return FKL_INS_OP(i) == FKL_OP_DROP && FKL_INS_uA(i) == FKL_SUBOP_DROP_1;
}

static uint32_t not_jmp_if_true_or_false_predicate(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k) {
    if (k < 3)
        return 0;
    uint32_t i = 0;
    if (FKL_INS_OP(peephole[i++].ins) != FKL_OP_NOT)
        return 0;
    FklIns ins[4] = { FKL_INS_STATIC_INIT };
    i += set_insln_to_ins(&peephole[i], ins);
    if (i >= k)
        return 0;
    if (!fklIsCondJmpIns(ins[0]))
        return 0;
    if (!is_drop1(peephole[i].ins))
        return 0;
    FklIns target_ins = buf->base[block_start[peephole[1].jmp_to - 1]].ins;
    if (is_drop1(target_ins))
        return i;
    return 0;
}

static uint32_t not_jmp_if_true_or_false_output(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k,
        FklInsLn *output) {
    output[0] = peephole[1];
    if (fklIsJmpIfTrueIns(output[0].ins)) {
        output[0].ins = FKL_INS_SET_OP(output[0].ins, FKL_OP_JMP_IF_FALSE);
    } else {
        output[0].ins = FKL_INS_SET_OP(output[0].ins, FKL_OP_JMP_IF_TRUE);
    }
    return 1;
}

static uint32_t put_loc_drop_predicate(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k) {
    if (k < 2)
        return 0;
    FklIns ins[4] = { FKL_INS_STATIC_INIT };
    uint32_t i = set_insln_to_ins(&peephole[0], ins);
    if (i < k && FKL_INS_OP(ins[0]) == FKL_OP_PUT_LOC
            && is_in_u16_range(FKL_INS_uC(ins[0]))
            && is_drop1(peephole[i].ins)) {
        return i + 1;
    }
    return 0;
}

static uint32_t put_loc_drop_output(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k,
        FklInsLn *output) {
    FklIns ins[4] = { FKL_INS_STATIC_INIT };
    FklInsArg arg;
    set_insln_to_ins(&peephole[0], ins);
    fklGetInsOpArg(ins, &arg);
    uint32_t loc_idx = arg.ux;
    FklOpcode op = FKL_OP_POP_LOC;

    uint32_t nl = FKL_MAKE_INS(ins, op, .ux = loc_idx);

    set_ins_to_insln(ins, peephole, output);

    return nl;
}

static uint32_t pop_and_get_loc_predicate(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k) {
    if (k < 2)
        return 0;
    FklIns ins[4] = { FKL_INS_STATIC_INIT };
    FklInsArg arg;
    uint32_t i = set_insln_to_ins(&peephole[0], ins);
    if (i >= k)
        return 0;
    if (FKL_INS_OP(ins[0]) != FKL_OP_POP_LOC)
        return 0;

    fklGetInsOpArg(ins, &arg);
    uint32_t loc_idx = arg.ux;
    i += set_insln_to_ins(&peephole[i], ins);
    if (FKL_INS_OP(ins[0]) != FKL_OP_GET_LOC)
        return 0;
    fklGetInsOpArg(ins, &arg);
    if (loc_idx != arg.ux)
        return 0;
    return i;
}

static uint32_t pop_and_get_loc_output(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k,
        FklInsLn *output) {
    FklIns ins[4] = { FKL_INS_STATIC_INIT };
    FklInsArg arg;
    set_insln_to_ins(&peephole[0], ins);
    fklGetInsOpArg(ins, &arg);
    uint32_t loc_idx = arg.ux;
    FklOpcode op = FKL_OP_PUT_LOC;
    uint32_t nl = FKL_MAKE_INS(ins, op, .ux = loc_idx);

    set_ins_to_insln(ins, peephole, output);

    return nl;
}

static uint32_t jmp_to_ret_predicate(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k) {
    if (peephole[0].jmp_to == 0)
        return 0;
    FklIns op = FKL_INS_OP(buf->base[block_start[peephole[0].jmp_to - 1]].ins);
    if (op != FKL_OP_RET)
        return 0;
    if (fklIsJmpIns(peephole[0].ins))
        return FKL_INS_OP(peephole[0].ins) - (FKL_OP_JMP - 1);
    if (fklIsJmpIfTrueIns(peephole[0].ins))
        return FKL_INS_OP(peephole[0].ins) - (FKL_OP_JMP_IF_TRUE - 1);
    if (fklIsJmpIfFalseIns(peephole[0].ins))
        return FKL_INS_OP(peephole[0].ins) - (FKL_OP_JMP_IF_FALSE - 1);
    return 0;
}

static uint32_t jmp_to_ret_output(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k,
        FklInsLn *output) {
    FklOpcode op = fklIsJmpIns(peephole[0].ins)       ? FKL_OP_RET
                 : fklIsJmpIfTrueIns(peephole[0].ins) ? FKL_OP_RET_IF_TRUE
                                                      : FKL_OP_RET_IF_FALSE;
    output[0] = peephole[0];
    output[0].ins = FKL_INS_SET_OP(output[0].ins, op);
    return 1;
}

static uint32_t jmp_to_jmp_predicate(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k) {
    if (peephole[0].jmp_to) {
        FklIns target_ins = buf->base[block_start[peephole[0].jmp_to - 1]].ins;
        FklOpcode target_opcode = FKL_INS_OP(target_ins);
        if (target_opcode >= FKL_OP_JMP && target_opcode <= FKL_OP_JMP_XX) {
            if (fklIsJmpIns(peephole[0].ins))
                return FKL_INS_OP(peephole[0].ins) - (FKL_OP_JMP - 1);
            if (fklIsJmpIfTrueIns(peephole[0].ins))
                return FKL_INS_OP(peephole[0].ins) - (FKL_OP_JMP_IF_TRUE - 1);
            if (fklIsJmpIfFalseIns(peephole[0].ins))
                return FKL_INS_OP(peephole[0].ins) - (FKL_OP_JMP_IF_FALSE - 1);
        } else
            return 0;
    }
    return 0;
}

static uint32_t jmp_to_jmp_output(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k,
        FklInsLn *output) {
    output[0] = peephole[0];
    uint32_t target_jmp_to =
            buf->base[block_start[peephole[0].jmp_to - 1]].jmp_to;
    output[0].jmp_to = target_jmp_to;
    return 1;
}

static inline int is_foldable_const(const FklIns in, int32_t *oprand) {
    switch (FKL_INS_OP(in)) {
    case FKL_OP_PUSH_0:
        *oprand = 0;
        return 1;
        break;
    case FKL_OP_PUSH_1:
        *oprand = 1;
        return 1;
        break;
    case FKL_OP_PUSH_I8:
        *oprand = FKL_INS_sA(in);
        return 1;
        break;
    case FKL_OP_PUSH_I16:
        *oprand = FKL_INS_sB(in);
        return 1;
        break;
    case FKL_OP_PUSH_I24:
        *oprand = FKL_INS_sC(in);
        return 1;
        break;
    default:
        return 0;
        break;
    }
    return 0;
}

union I24u {
    struct {
        int32_t i24 : 24;
        int8_t o : 8;
    };
    int32_t i;
};

static inline int is_i24_mul_overflow(int32_t a, int32_t b) {
    if (a == 0 || b == 0)
        return 0;
    if (a >= 0 && b >= 0)
        return (FKL_I24_MAX / a) < b;
    if (a < 0 && b < 0)
        return (FKL_I24_MAX / a) > b;
    if (a * b == FKL_I24_MIN)
        return 0;
    union I24u t = { .i24 = a * b };
    t.i24 /= b;
    return a != t.i24;
}

static inline int is_foldable_op2(const FklIns in, int32_t a, int32_t b) {
    int32_t res;
    switch (FKL_INS_OP(in)) {
    case FKL_OP_ADD:
        if (FKL_INS_sA(in) != 2)
            return 0;
        res = a + b;
        return res >= FKL_I24_MIN && res <= FKL_I24_MAX;
        break;
    case FKL_OP_SUB:
        if (FKL_INS_sA(in) != 2)
            return 0;
        res = a - b;
        return res >= FKL_I24_MIN && res <= FKL_I24_MAX;
        break;
    case FKL_OP_MUL:
        if (FKL_INS_sA(in) != 2)
            return 0;
        return !is_i24_mul_overflow(a, b);
        break;
    case FKL_OP_IDIV:
        if (FKL_INS_sA(in) != 2)
            return 0;
        if (a == 0)
            return 0;
        res = a / b;
        return res >= FKL_I24_MIN && res <= FKL_I24_MAX;
        break;
    case FKL_OP_DIV:
        if (FKL_INS_sA(in) != 2 && FKL_INS_sA(in) != -2)
            return 0;
        if (a == 0)
            return 0;
        if (FKL_INS_sA(in) == -2) {
            res = a % b;
            return res >= FKL_I24_MIN && res <= FKL_I24_MAX;
        } else {
            res = a / b;
            return a % b == 0 && res >= FKL_I24_MIN && res <= FKL_I24_MAX;
        }
        break;
    default:
        return 0;
        break;
    }
    return 0;
}

static uint32_t oprand2_const_fold_predicate(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k) {
    if (k < 3)
        return 0;
    int32_t oprand1;
    if (is_foldable_const(peephole[0].ins, &oprand1)) {
        int32_t oprand2;
        if (is_foldable_const(peephole[1].ins, &oprand2))
            if (is_foldable_op2(peephole[2].ins, oprand1, oprand2))
                return 3;
    }
    return 0;
}

static uint32_t oprand2_const_fold_output(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k,
        FklInsLn *output) {
    int32_t oprand1 = 0;
    int32_t oprand2 = 0;
    is_foldable_const(peephole[0].ins, &oprand1);
    is_foldable_const(peephole[1].ins, &oprand2);
    int32_t r;
    switch (FKL_INS_OP(peephole[2].ins)) {
    case FKL_OP_ADD:
        FKL_ASSERT(FKL_INS_sA(peephole[2].ins) == 2);
        r = oprand1 + oprand2;
        break;
    case FKL_OP_SUB:
        FKL_ASSERT(FKL_INS_sA(peephole[2].ins) == 2);
        r = oprand1 - oprand2;
        break;
    case FKL_OP_MUL:
        FKL_ASSERT(FKL_INS_sA(peephole[2].ins) == 2);
        r = oprand1 * oprand2;
        break;
    case FKL_OP_DIV:
        if (FKL_INS_sA(peephole[2].ins) == -2) {
            r = oprand1 % oprand2;
        } else
            goto div;
        break;
    case FKL_OP_IDIV:
    div:
        FKL_ASSERT(FKL_INS_sA(peephole[2].ins) == 2);
        r = oprand1 / oprand2;
        break;
    default:
        FKL_UNREACHABLE();
        break;
    }
    output[0] = peephole[0];
    if (r == 0 || r == 1) {
        output[0].ins = FKL_INS_SET_OP(output[0].ins, FKL_OP_PUSH_0 + r);
    } else if (r >= FKL_I24_MIN && r <= FKL_I24_MAX) {
        int l = FKL_MAKE_INS(&output[0].ins, FKL_OP_PUSH_I8, .ix = r);
        (void)l;
        FKL_ASSERT(l == 1);
    } else {
        FKL_UNREACHABLE();
    }
    return 1;
}

static inline int
is_foldable_op3(const FklIns in, int32_t a, int32_t b, int32_t c) {
    int32_t res;
    switch (FKL_INS_OP(in)) {
    case FKL_OP_ADD:
        if (FKL_INS_sA(in) != 3)
            return 0;
        res = a + b + c;
        return res >= FKL_I24_MIN && res <= FKL_I24_MAX;
        break;
    case FKL_OP_SUB:
        if (FKL_INS_sA(in) != 3)
            return 0;
        res = a - (b + c);
        return res >= FKL_I24_MIN && res <= FKL_I24_MAX;
        break;
    case FKL_OP_MUL:
        if (FKL_INS_sA(in) != 3)
            return 0;
        return !is_i24_mul_overflow(a, b) && !is_i24_mul_overflow(c, a * b);
        break;
    case FKL_OP_IDIV:
        if (FKL_INS_sA(in) != 3)
            return 0;
        if (is_i24_mul_overflow(a, b))
            return 0;
        if (a == 0 || b == 0)
            return 0;
        res = a / (b * c);
        return res >= FKL_I24_MIN && res <= FKL_I24_MAX;
        break;
    case FKL_OP_DIV:
        if (FKL_INS_sA(in) != 3)
            return 0;
        if (is_i24_mul_overflow(a, b))
            return 0;
        if (a == 0 || b == 0)
            return 0;
        res = a / (b * c);
        return a % (b * c) == 0 && res >= FKL_I24_MIN && res <= FKL_I24_MAX;
        break;
    default:
        return 0;
        break;
    }
    return 0;
}

static uint32_t oprand3_const_fold_predicate(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k) {
    if (k < 4)
        return 0;
    int32_t oprand1;
    if (is_foldable_const(peephole[0].ins, &oprand1)) {
        int32_t oprand2;
        if (is_foldable_const(peephole[1].ins, &oprand2)) {
            int32_t oprand3;
            if (is_foldable_const(peephole[2].ins, &oprand3))
                if (is_foldable_op3(peephole[3].ins, oprand1, oprand2, oprand3))
                    return 4;
        }
    }
    return 0;
}

static uint32_t oprand3_const_fold_output(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k,
        FklInsLn *output) {
    int32_t oprand1 = 0;
    int32_t oprand2 = 0;
    int32_t oprand3 = 0;
    is_foldable_const(peephole[0].ins, &oprand1);
    is_foldable_const(peephole[1].ins, &oprand2);
    is_foldable_const(peephole[2].ins, &oprand3);
    int32_t r;
    switch (FKL_INS_OP(peephole[3].ins)) {
    case FKL_OP_ADD:
        FKL_ASSERT(FKL_INS_sA(peephole[3].ins) == 3);
        r = oprand1 + oprand2 + oprand3;
        break;
    case FKL_OP_SUB:
        FKL_ASSERT(FKL_INS_sA(peephole[3].ins) == 3);
        r = oprand1 - (oprand2 + oprand3);
        break;
    case FKL_OP_MUL:
        FKL_ASSERT(FKL_INS_sA(peephole[3].ins) == 3);
        r = oprand1 * oprand2 * oprand3;
        break;
    case FKL_OP_DIV:
    case FKL_OP_IDIV:
        FKL_ASSERT(FKL_INS_sA(peephole[3].ins) == 3);
        r = oprand1 / (oprand2 * oprand3);
        break;
    default:
        FKL_UNREACHABLE();
        break;
    }
    output[0] = peephole[0];
    if (r == 0 || r == 1) {
        output[0].ins = FKL_INS_SET_OP(output[0].ins, FKL_OP_PUSH_0 + r);
    } else if (r >= FKL_I24_MIN && r <= FKL_I24_MAX) {
        int l = FKL_MAKE_INS(&output[0].ins, FKL_OP_PUSH_I8, .ix = r);
        (void)l;
        FKL_ASSERT(l == 1);
    } else {
        FKL_UNREACHABLE();
    }
    return 1;
}

static inline int is_foldable_op1(const FklIns in, int32_t a) {
    int32_t res;
    switch (FKL_INS_OP(in)) {
    case FKL_OP_ADDK:
        res = a + FKL_INS_sA(in);
        return res >= FKL_I24_MIN && res <= FKL_I24_MAX;
        break;
    case FKL_OP_ADD:
        return FKL_INS_sA(in) == 1;
        break;
    case FKL_OP_MUL:
        return FKL_INS_sA(in) == 1;
        break;
    case FKL_OP_SUB:
        if (FKL_INS_sA(in) != 1) {
            return 0;
        } else {
            res = -a;
            return res >= FKL_I24_MIN && res <= FKL_I24_MAX;
        }
        break;
    default:
        return 0;
        break;
    }
    return 0;
}

static uint32_t oprand1_const_fold_predicate(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k) {
    if (k < 2)
        return 0;
    int32_t oprand1;
    if (is_foldable_const(peephole[0].ins, &oprand1)) {
        if (is_foldable_op1(peephole[1].ins, oprand1))
            return 2;
    }
    return 0;
}

static uint32_t oprand1_const_fold_output(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k,
        FklInsLn *output) {
    int32_t oprand1 = 0;
    is_foldable_const(peephole[0].ins, &oprand1);
    int32_t r;
    switch (FKL_INS_OP(peephole[1].ins)) {
    case FKL_OP_ADDK:
        r = oprand1 + FKL_INS_sA(peephole[1].ins);
        break;
    case FKL_OP_ADD:
    case FKL_OP_MUL:
        FKL_ASSERT(FKL_INS_sA(peephole[1].ins) == 1);
        r = oprand1;
        break;
    case FKL_OP_SUB:
        FKL_ASSERT(FKL_INS_sA(peephole[1].ins) == 1);
        r = -oprand1;
        break;
    default:
        FKL_UNREACHABLE();
        break;
    }
    output[0] = peephole[0];
    if (r == 0 || r == 1) {
        output[0].ins = FKL_INS_SET_OP(output[0].ins, FKL_OP_PUSH_0 + r);
    } else if (r >= FKL_I24_MIN && r <= FKL_I24_MAX) {
        int l = FKL_MAKE_INS(&output[0].ins, FKL_OP_PUSH_I8, .ix = r);
        (void)l;
        FKL_ASSERT(l == 1);
    } else {
        FKL_UNREACHABLE();
    }
    return 1;
}

static uint32_t nil_cond_ret_or_jmp_drop_pred(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k) {
    if (k < 4)
        return 0;
    uint32_t block_count = 0;
    uint32_t i = 0;
    if (FKL_INS_OP(peephole[i++].ins) == FKL_OP_PUSH_NIL) {
        if (peephole[i].block_id)
            block_count++;
        if (fklIsJmpIfTrueIns(peephole[i].ins)
                || FKL_INS_OP(peephole[i].ins) == FKL_OP_RET_IF_TRUE) {
            i += fklGetOpcodeModeLen(FKL_INS_OP(peephole[i].ins));
            if (i >= k)
                return 0;
            if (peephole[i].block_id)
                block_count++;
            if (is_drop1(peephole[i++].ins)) {
                if (i >= k)
                    return 0;
                if (peephole[i].block_id)
                    block_count++;
                if (block_count <= 1) {
                    i += fklGetOpcodeModeLen(FKL_INS_OP(peephole[i].ins));
                    return i;
                }
            }
        }
    }
    return 0;
}

static uint32_t nil_cond_ret_or_jmp_drop_output(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k,
        FklInsLn *output) {
    uint32_t i = 1 + fklGetOpcodeModeLen(FKL_INS_OP(peephole[1].ins)) + 1;
    uint32_t l = fklGetOpcodeModeLen(FKL_INS_OP(peephole[i].ins));
    for (uint32_t j = 0; j < l; j++) {
        output[j] = peephole[i + j];
        output[j].block_id = peephole[j].block_id;
    }
    return l;
}

static uint32_t push_const_not_pred(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k) {
    if (k < 2)
        return 0;
    switch (FKL_INS_OP(peephole[0].ins)) {
    case FKL_OP_PUSH_NIL:
    case FKL_OP_PUSH_0:
    case FKL_OP_PUSH_1:
    case FKL_OP_PUSH_I8:
    case FKL_OP_PUSH_I16:
    case FKL_OP_PUSH_I24:
    case FKL_OP_PUSH_CONST: {
        uint32_t i = fklGetOpcodeModeLen(FKL_INS_OP(peephole[0].ins));
        if (k > i && FKL_INS_OP(peephole[i].ins) == FKL_OP_NOT)
            return i + 1;
    } break;
    default:
        return 0;
    }
    return 0;
}

static uint32_t push_const_not_output(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k,
        FklInsLn *output) {
    output[0] = peephole[0];
    FklOpcode op = FKL_INS_OP(output[0].ins) == FKL_OP_PUSH_NIL
                         ? FKL_OP_PUSH_1
                         : FKL_OP_PUSH_NIL;
    output[0].ins = FKL_INS_SET_OP(output[0].ins, op);
    return 1;
}

static uint32_t mov_loc_pred(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k) {
    if (k < 2)
        return 0;
    if (FKL_INS_OP(peephole[0].ins) != FKL_OP_GET_LOC)
        return 0;

    uint32_t a_idx = FKL_INS_uC(peephole[0].ins);
    if (a_idx > UINT8_MAX)
        return 0;
    if (FKL_INS_OP(peephole[1].ins) != FKL_OP_PUT_LOC)
        return 0;
    if (is_in_u16_range(FKL_INS_uC(peephole[1].ins)))
        return 2;
    return 0;
}

static uint32_t mov_loc_output(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k,
        FklInsLn *output) {
    output[0] = peephole[0];
    output[0].ins = FKL_INS_SET_OP(output[0].ins, FKL_OP_MOV_LOC);
    output[0].ins = FKL_INS_SET_uA(output[0].ins, FKL_INS_uC(peephole[0].ins));
    output[0].ins = FKL_INS_SET_uB(output[0].ins, FKL_INS_uC(peephole[1].ins));
    return 1;
}

static uint32_t mov_loc_drop_get_pred(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k) {
    if (k < 3)
        return 0;
    if (FKL_INS_OP(peephole[0].ins) != FKL_OP_MOV_LOC)
        return 0;

    uint32_t a_idx = FKL_INS_uA(peephole[0].ins);
    uint32_t b_idx = FKL_INS_uB(peephole[0].ins);
    if (!is_drop1(peephole[1].ins))
        return 0;

    if (FKL_INS_OP(peephole[2].ins) == FKL_OP_GET_LOC) {
        return FKL_INS_uC(peephole[2].ins) == a_idx ? 3 : 0;
    } else if (FKL_INS_OP(peephole[2].ins) == FKL_OP_MOV_LOC) {
        uint32_t a_idx1 = FKL_INS_uA(peephole[2].ins);
        uint32_t b_idx1 = FKL_INS_uB(peephole[2].ins);
        return a_idx1 == a_idx && b_idx1 == b_idx ? 3 : 0;
    }

    return 0;
}

static uint32_t mov_loc_drop_get_output(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k,
        FklInsLn *output) {
    output[0] = peephole[0];
    return 1;
}

static uint32_t mov_var_ref_pred(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k) {
    if (k < 2)
        return 0;
    if (FKL_INS_OP(peephole[0].ins) != FKL_OP_GET_VAR_REF)
        return 0;

    uint32_t a_idx = FKL_INS_uB(peephole[0].ins);
    if (a_idx > UINT8_MAX)
        return 0;
    if (FKL_INS_OP(peephole[1].ins) != FKL_OP_PUT_VAR_REF)
        return 0;
    if (is_in_u16_range(FKL_INS_uC(peephole[1].ins)))
        return 2;
    return 0;
}

static uint32_t mov_var_ref_output(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k,
        FklInsLn *output) {
    output[0] = peephole[0];
    output[0].ins = FKL_INS_SET_OP(output[0].ins, FKL_OP_MOV_VAR_REF);
    output[0].ins = FKL_INS_SET_uA(output[0].ins, FKL_INS_uC(peephole[0].ins));
    output[0].ins = FKL_INS_SET_uB(output[0].ins, FKL_INS_uC(peephole[1].ins));
    return 1;
}

static uint32_t mov_var_ref_drop_get_pred(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k) {
    if (k < 3)
        return 0;
    if (FKL_INS_OP(peephole[0].ins) != FKL_OP_MOV_VAR_REF)
        return 0;

    uint32_t a_idx = FKL_INS_uA(peephole[0].ins);
    uint32_t b_idx = FKL_INS_uB(peephole[0].ins);
    if (!is_drop1(peephole[1].ins))
        return 0;

    if (FKL_INS_OP(peephole[2].ins) == FKL_OP_GET_VAR_REF) {
        return FKL_INS_uB(peephole[2].ins) == a_idx ? 3 : 0;
    } else if (FKL_INS_OP(peephole[2].ins) == FKL_OP_MOV_VAR_REF) {
        uint32_t a_idx1 = FKL_INS_uA(peephole[2].ins);
        uint32_t b_idx1 = FKL_INS_uB(peephole[2].ins);
        return a_idx1 == a_idx && b_idx1 == b_idx ? 3 : 0;
    }
    return 0;
}

static uint32_t mov_to_get_pred(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k) {
    FklOpcode op = FKL_INS_OP(peephole[0].ins);
    if (op != FKL_OP_MOV_LOC && op != FKL_OP_MOV_VAR_REF)
        return 0;
    uint32_t a_idx = FKL_INS_uA(peephole[0].ins);
    uint32_t b_idx = FKL_INS_uB(peephole[0].ins);
    if (a_idx == b_idx)
        return 1;
    return 0;
}

static uint32_t mov_to_get_output(const FklByteCodeBuffer *buf,
        const uint64_t *block_start,
        const FklInsLn *peephole,
        uint32_t k,
        FklInsLn *output) {
    output[0] = peephole[0];
    uint32_t idx = FKL_INS_uA(peephole[0].ins);
    FklOpcode op = FKL_INS_OP(peephole[0].ins) == FKL_OP_MOV_LOC
                         ? FKL_OP_GET_LOC
                         : FKL_OP_GET_VAR_REF;
    output[0].ins = FKL_MAKE_INS_IuC(op, idx);
    return 1;
}

static const struct PeepholeOptimizer PeepholeOptimizers[] = {
    // clang-format off
    { not3_predicate,                     not3_output,                     PEEPHOLE_SHOULD_IN_ONE_BLOCK },
    { inc_or_dec_loc_predicate,           inc_or_dec_loc_output,           PEEPHOLE_SHOULD_IN_ONE_BLOCK },
    { not_jmp_if_true_or_false_predicate, not_jmp_if_true_or_false_output, PEEPHOLE_SHOULD_IN_ONE_BLOCK },
    { put_loc_drop_predicate,             put_loc_drop_output,             PEEPHOLE_SHOULD_IN_ONE_BLOCK },
    { pop_and_get_loc_predicate,          pop_and_get_loc_output,          PEEPHOLE_SHOULD_IN_ONE_BLOCK },
    { jmp_to_ret_predicate,               jmp_to_ret_output,               PEEPHOLE_SHOULD_IN_ONE_BLOCK },
    { jmp_to_jmp_predicate,               jmp_to_jmp_output,               PEEPHOLE_SHOULD_IN_ONE_BLOCK },
    { oprand1_const_fold_predicate,       oprand1_const_fold_output,       PEEPHOLE_SHOULD_IN_ONE_BLOCK },
    { oprand2_const_fold_predicate,       oprand2_const_fold_output,       PEEPHOLE_SHOULD_IN_ONE_BLOCK },
    { oprand3_const_fold_predicate,       oprand3_const_fold_output,       PEEPHOLE_SHOULD_IN_ONE_BLOCK },
    { nil_cond_ret_or_jmp_drop_pred,      nil_cond_ret_or_jmp_drop_output, PEEPHOLE_CAN_IN_BLOCKS       },
    { push_const_not_pred,                push_const_not_output,           PEEPHOLE_SHOULD_IN_ONE_BLOCK },
    { mov_loc_pred,                       mov_loc_output,                  PEEPHOLE_SHOULD_IN_ONE_BLOCK },
    { mov_loc_drop_get_pred,              mov_loc_drop_get_output,         PEEPHOLE_SHOULD_IN_ONE_BLOCK },
    { mov_var_ref_pred,                   mov_var_ref_output,              PEEPHOLE_SHOULD_IN_ONE_BLOCK },
    { mov_var_ref_drop_get_pred,          mov_loc_drop_get_output,         PEEPHOLE_SHOULD_IN_ONE_BLOCK },
    { mov_to_get_pred,                    mov_to_get_output,               PEEPHOLE_SHOULD_IN_ONE_BLOCK },
    { NULL,                               NULL,                            PEEPHOLE_SHOULD_IN_ONE_BLOCK },
    // clang-format on
};

static inline int
is_in_one_block(const FklInsLn *peephole, uint32_t offset, uint32_t *block_id) {
    if (peephole[0].block_id) {
        for (uint32_t i = 1; i < offset; i++)
            if (peephole[i].block_id
                    && peephole[i].block_id != peephole[0].block_id)
                return 0;
        *block_id = peephole[0].block_id;
    } else {
        for (uint32_t i = 0; i < offset; i++)
            if (peephole[i].block_id)
                return 0;
    }
    return 1;
}

static inline int do_peephole_optimize(FklByteCodeBuffer *buf,
        const uint64_t *block_start) {
    FklInsLn peephole[PEEPHOLE_SIZE];
    FklInsLn output[PEEPHOLE_SIZE];
    uint64_t valid_ins_idx[PEEPHOLE_SIZE];

    int change = 0;

    for (uint64_t i = 0; i < buf->size;) {
        uint64_t j = 0;
        uint64_t k = 0;

        for (; j + i < buf->size && k < PEEPHOLE_SIZE; j++)
            if (FKL_INS_OP(buf->base[i + j].ins)) {
                peephole[k] = buf->base[i + j];
                valid_ins_idx[k++] = j;
            }

        if (k == 0)
            break;

        uint32_t offset = 0;
        for (const struct PeepholeOptimizer *optimizer = &PeepholeOptimizers[0];
                optimizer->predicate;
                optimizer++) {
            offset = optimizer->predicate(buf, block_start, peephole, k);
            uint32_t block_id = 0;
            if (offset
                    && (optimizer->block_state
                            || is_in_one_block(peephole, offset, &block_id))) {
                FklInsLn *output_start = &buf->base[i];
                uint32_t output_len = optimizer->output(buf,
                        block_start,
                        peephole,
                        k,
                        output);
                uint32_t ii = 0;
                for (; ii < output_len; ii++)
                    output_start[ii] = output[ii];
                if (block_id)
                    output_start[0].block_id = block_id;
                if (offset < k) {
                    for (; ii < valid_ins_idx[offset]; ii++) {
                        FklIns ins = output_start[ii].ins;
                        ins = FKL_INS_SET_OP(ins, FKL_OP_DUMMY);
                        output_start[ii].ins = ins;
                    }
                    for (i += valid_ins_idx[offset];
                            i < buf->size
                            && FKL_INS_OP(buf->base[i].ins) == FKL_OP_EXTRA_ARG;
                            i++)
                        ;
                } else {
                    for (; i + ii < buf->size; ii++) {
                        FklIns ins = output_start[ii].ins;
                        ins = FKL_INS_SET_OP(ins, FKL_OP_DUMMY);
                        output_start[ii].ins = ins;
                    }
                    i = buf->size;
                }
                change = 1;
                goto done;
            }
        }

        for (i += (valid_ins_idx[0] + 1);
                i < buf->size
                && FKL_INS_OP(buf->base[i].ins) == FKL_OP_EXTRA_ARG;
                i++)
            ;
    done:
        continue;
    }
    return change;
}

static inline FklByteCodeBuffer *remove_dummy_code(FklByteCodeBuffer *a,
        FklByteCodeBuffer *b) {
    for (uint64_t i = 0; i < a->size; i++) {
        FklInsLn *cur = &a->base[i];
        if (FKL_INS_OP(cur->ins))
            push_ins_ln(b, cur);
    }

    return recompute_jmp_target(b, a);
}

void fklPeepholeOptimize(FklByteCodelnt *bcl) {
    FklByteCodeBuffer buf;
    fklInitByteCodeBufferWith(&buf, bcl);

    uint32_t block_num = fklByteCodeBufferScanAndSetBasicBlock(&buf);
    uint64_t *block_start =
            (uint64_t *)fklZmalloc(block_num * sizeof(uint64_t));
    FKL_ASSERT(block_start);
    scan_and_set_block_start(&buf, block_start);

    int change;
    do {
        change = do_peephole_optimize(&buf, block_start);
    } while (change);

    FklByteCodeBuffer new_buf;
    fklInitByteCodeBuffer(&new_buf, buf.size);

    FklByteCodeBuffer *fin = remove_dummy_code(&buf, &new_buf);

    fklSetByteCodelntWithBuf(bcl, fin);
    fklUninitByteCodeBuffer(&buf);
    fklUninitByteCodeBuffer(&new_buf);

    fklZfree(block_start);
}
