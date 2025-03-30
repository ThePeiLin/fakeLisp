#ifndef DISPATCH_INCLUDED
#include "vmrun.c"
#endif

#ifndef DISPATCH_SWITCH
void fklVMexecuteInstruction(FklVM *exe, FklOpcode op, FklInstruction *ins,
                             FklVMframe *frame) {
    FklVMlib *plib;
    uint32_t idx;
    uint32_t idx1;
    uint64_t size;
    uint64_t num;
    int64_t offset;
    switch (op) {
#endif
    case FKL_OP_DUMMY:
        exe->dummy_ins_func(exe, ins);
        return;
        break;
    case FKL_OP_PUSH_NIL:
        FKL_VM_PUSH_VALUE(exe, FKL_VM_NIL);
        break;
    case FKL_OP_PUSH_PAIR: {
        FklVMvalue *cdr = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *car = FKL_VM_POP_TOP_VALUE(exe);
        FKL_VM_PUSH_VALUE(exe, fklCreateVMvaluePair(exe, car, cdr));
    } break;
    case FKL_OP_PUSH_I24:
        FKL_VM_PUSH_VALUE(exe, FKL_MAKE_VM_FIX(FKL_GET_INS_IC(ins)));
        break;
    case FKL_OP_PUSH_I64F:
        FKL_VM_PUSH_VALUE(exe, FKL_MAKE_VM_FIX(exe->gc->ki64[ins->bu]));
        break;
    case FKL_OP_PUSH_I64F_C:
        FKL_VM_PUSH_VALUE(exe,
                          FKL_MAKE_VM_FIX(exe->gc->ki64[FKL_GET_INS_UC(ins)]));
        break;
    case FKL_OP_PUSH_I64F_X:
        FKL_VM_PUSH_VALUE(
            exe, FKL_MAKE_VM_FIX(exe->gc->ki64[GET_INS_UX(ins, frame)]));
        break;
    case FKL_OP_PUSH_CHAR:
        FKL_VM_PUSH_VALUE(exe, FKL_MAKE_VM_CHR(ins->bu));
        break;
    case FKL_OP_PUSH_F64:
        FKL_VM_PUSH_VALUE(exe,
                          fklCreateVMvalueF64(exe, exe->gc->kf64[ins->bu]));
        break;
    case FKL_OP_PUSH_F64_C:
        FKL_VM_PUSH_VALUE(
            exe, fklCreateVMvalueF64(exe, exe->gc->kf64[FKL_GET_INS_UC(ins)]));
        break;
    case FKL_OP_PUSH_F64_X:
        FKL_VM_PUSH_VALUE(exe, fklCreateVMvalueF64(
                                   exe, exe->gc->kf64[GET_INS_UX(ins, frame)]));
        break;
    case FKL_OP_PUSH_STR:
        FKL_VM_PUSH_VALUE(exe,
                          fklCreateVMvalueStr(exe, exe->gc->kstr[ins->bu]));
        break;
    case FKL_OP_PUSH_STR_C:
        FKL_VM_PUSH_VALUE(
            exe, fklCreateVMvalueStr(exe, exe->gc->kstr[FKL_GET_INS_UC(ins)]));
        break;
    case FKL_OP_PUSH_STR_X:
        FKL_VM_PUSH_VALUE(exe, fklCreateVMvalueStr(
                                   exe, exe->gc->kstr[GET_INS_UX(ins, frame)]));
        break;
    case FKL_OP_PUSH_SYM:
        FKL_VM_PUSH_VALUE(exe, FKL_MAKE_VM_SYM(ins->bu));
        break;
    case FKL_OP_PUSH_SYM_C:
        FKL_VM_PUSH_VALUE(exe, FKL_MAKE_VM_SYM(FKL_GET_INS_UC(ins)));
        break;
    case FKL_OP_PUSH_SYM_X:
        FKL_VM_PUSH_VALUE(exe, FKL_MAKE_VM_SYM(GET_INS_UX(ins, frame)));
        break;
    case FKL_OP_DUP: {
        if (exe->tp == exe->bp)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_STACKERROR, exe);
        FklVMvalue *val = FKL_VM_POP_TOP_VALUE(exe);
        FKL_VM_PUSH_VALUE(exe, val);
        FKL_VM_PUSH_VALUE(exe, val);
    } break;
    case FKL_OP_PUSH_PROC:
        idx = ins->au;
        size = ins->bu;
        goto push_proc;
    case FKL_OP_PUSH_PROC_X:
        idx = FKL_GET_INS_UC(ins);
        ins = frame->c.pc++;
        size = FKL_GET_INS_UC(ins);
        goto push_proc;
    case FKL_OP_PUSH_PROC_XX:
        idx = FKL_GET_INS_UC(ins) | (((uint32_t)ins[1].au) << FKL_I24_WIDTH);
        size = ins[1].bu | (((uint64_t)ins[2].bu) << FKL_I16_WIDTH);
        frame->c.pc += 2;
        goto push_proc;
    case FKL_OP_PUSH_PROC_XXX:
        idx = FKL_GET_INS_UC(ins) | (((uint32_t)ins[1].au) << FKL_I24_WIDTH);
        size = ins[1].bu | (((uint64_t)ins[2].au) << FKL_I16_WIDTH)
             | (((uint64_t)ins[2].bu) << FKL_I24_WIDTH)
             | (((uint64_t)ins[3].au) << (FKL_I16_WIDTH * 2 + FKL_BYTE_WIDTH))
             | (((uint64_t)ins[3].bu)
                << (FKL_I16_WIDTH * 2 + FKL_BYTE_WIDTH * 2));
        frame->c.pc += 3;
    push_proc: {
        FKL_VM_PUSH_VALUE(exe,
                          fklCreateVMvalueProcWithFrame(exe, frame, size, idx));
        fklAddCompoundFrameCp(frame, size);
    } break;
    case FKL_OP_DROP:
        DROP_TOP(exe);
        break;
    case FKL_OP_POP_ARG:
        idx = ins->bu;
        goto pop_arg;
    case FKL_OP_POP_ARG_C:
        idx = FKL_GET_INS_UC(ins);
        goto pop_arg;
    case FKL_OP_POP_ARG_X:
        idx = GET_INS_UX(ins, frame);
    pop_arg: {
        if (exe->tp <= exe->bp)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG, exe);
        FklVMvalue *v = FKL_VM_POP_TOP_VALUE(exe);
        GET_COMPOUND_FRAME_LOC(frame, idx) = v;
    } break;
    case FKL_OP_POP_REST_ARG:
        idx = ins->bu;
        goto pop_rest_arg;
    case FKL_OP_POP_REST_ARG_C:
        idx = FKL_GET_INS_UC(ins);
        goto pop_rest_arg;
    case FKL_OP_POP_REST_ARG_X:
        idx = GET_INS_UX(ins, frame);
    pop_rest_arg: {
        FklVMvalue *obj = FKL_VM_NIL;
        FklVMvalue *volatile *pValue = &obj;
        for (; exe->tp > exe->bp; pValue = &FKL_VM_CDR(*pValue))
            *pValue =
                fklCreateVMvaluePairWithCar(exe, FKL_VM_POP_TOP_VALUE(exe));
        GET_COMPOUND_FRAME_LOC(frame, idx) = obj;
    } break;
    case FKL_OP_SET_BP:
        fklSetBp(exe);
        break;
    case FKL_OP_CALL: {
        FklVMvalue *proc = FKL_VM_POP_TOP_VALUE(exe);
        if (!fklIsCallable(proc))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_CALL_ERROR, exe);
        switch (proc->type) {
        case FKL_TYPE_PROC:
            call_compound_procedure(exe, proc);
            break;
            CALL_CALLABLE_OBJ(exe, proc);
        }
        return;
    } break;
    case FKL_OP_RES_BP:
        if (fklResBp(exe))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOMANYARG, exe);
        frame->c.tp = exe->tp;
        frame->c.bp = exe->bp;
        break;
    case FKL_OP_JMP_IF_TRUE:
        offset = ins->bi;
        goto jmp_if_true;
        break;
    case FKL_OP_JMP_IF_TRUE_C:
        offset = FKL_GET_INS_IC(ins);
        goto jmp_if_true;
        break;
    case FKL_OP_JMP_IF_TRUE_X:
        offset = GET_INS_IX(ins, frame);
        goto jmp_if_true;
        break;
    case FKL_OP_JMP_IF_TRUE_XX:
        offset = GET_INS_IXX(ins, frame);
    jmp_if_true:
        if (FKL_VM_GET_TOP_VALUE(exe) != FKL_VM_NIL)
            frame->c.pc += offset;
        break;

    case FKL_OP_JMP_IF_FALSE:
        offset = ins->bi;
        goto jmp_if_false;
        break;
    case FKL_OP_JMP_IF_FALSE_C:
        offset = FKL_GET_INS_IC(ins);
        goto jmp_if_false;
        break;
    case FKL_OP_JMP_IF_FALSE_X:
        offset = GET_INS_IX(ins, frame);
        goto jmp_if_false;
        break;
    case FKL_OP_JMP_IF_FALSE_XX:
        offset = GET_INS_IXX(ins, frame);
    jmp_if_false:
        if (FKL_VM_GET_TOP_VALUE(exe) == FKL_VM_NIL)
            frame->c.pc += offset;
        break;
    case FKL_OP_JMP:
        offset = ins->bi;
        goto jmp;
        break;
    case FKL_OP_JMP_C:
        offset = FKL_GET_INS_IC(ins);
        goto jmp;
        break;
    case FKL_OP_JMP_X:
        offset = GET_INS_IX(ins, frame);
        goto jmp;
        break;
    case FKL_OP_JMP_XX:
        offset = GET_INS_IXX(ins, frame);
    jmp:
        frame->c.pc += offset;
        break;
    case FKL_OP_LIST_APPEND: {
        FklVMvalue *fir = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *sec = FKL_VM_POP_TOP_VALUE(exe);
        if (sec == FKL_VM_NIL)
            FKL_VM_PUSH_VALUE(exe, fir);
        else {
            FklVMvalue **lastcdr = &sec;
            while (FKL_IS_PAIR(*lastcdr))
                lastcdr = &FKL_VM_CDR(*lastcdr);
            if (*lastcdr != FKL_VM_NIL)
                FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
            *lastcdr = fir;
            FKL_VM_PUSH_VALUE(exe, sec);
        }
    } break;
    case FKL_OP_PUSH_VEC:
        size = ins->bu;
        goto push_vec;
    case FKL_OP_PUSH_VEC_C:
        size = FKL_GET_INS_UC(ins);
        goto push_vec;
    case FKL_OP_PUSH_VEC_X:
        size = GET_INS_UX(ins, frame);
        goto push_vec;
    case FKL_OP_PUSH_VEC_XX:
        size = GET_INS_UXX(ins, frame);
    push_vec: {
        FklVMvalue *vec = fklCreateVMvalueVec(exe, size);
        FklVMvec *v = FKL_VM_VEC(vec);
        for (uint64_t i = size; i > 0; i--)
            v->base[i - 1] = FKL_VM_POP_TOP_VALUE(exe);
        FKL_VM_PUSH_VALUE(exe, vec);
    } break;
    case FKL_OP_TAIL_CALL: {
        FklVMvalue *proc = FKL_VM_POP_TOP_VALUE(exe);
        if (!fklIsCallable(proc))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_CALL_ERROR, exe);
        switch (proc->type) {
        case FKL_TYPE_PROC:
            tail_call_proc(exe, proc);
            break;
            CALL_CALLABLE_OBJ(exe, proc);
        }
        return;
    } break;
    case FKL_OP_PUSH_BI:
        FKL_VM_PUSH_VALUE(exe,
                          fklCreateVMvalueBigInt2(exe, exe->gc->kbi[ins->bu]));
        break;
    case FKL_OP_PUSH_BI_C:
        FKL_VM_PUSH_VALUE(exe, fklCreateVMvalueBigInt2(
                                   exe, exe->gc->kbi[FKL_GET_INS_UC(ins)]));
        break;
    case FKL_OP_PUSH_BI_X:
        FKL_VM_PUSH_VALUE(exe, fklCreateVMvalueBigInt2(
                                   exe, exe->gc->kbi[GET_INS_UX(ins, frame)]));
        break;
    case FKL_OP_PUSH_BOX: {
        FklVMvalue *c = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *box = fklCreateVMvalueBox(exe, c);
        FKL_VM_PUSH_VALUE(exe, box);
    } break;
    case FKL_OP_PUSH_BVEC:
        FKL_VM_PUSH_VALUE(exe,
                          fklCreateVMvalueBvec(exe, exe->gc->kbvec[ins->bu]));
        break;
    case FKL_OP_PUSH_BVEC_C:
        FKL_VM_PUSH_VALUE(exe, fklCreateVMvalueBvec(
                                   exe, exe->gc->kbvec[FKL_GET_INS_UC(ins)]));
        break;
    case FKL_OP_PUSH_BVEC_X:
        FKL_VM_PUSH_VALUE(
            exe,
            fklCreateVMvalueBvec(exe, exe->gc->kbvec[GET_INS_UX(ins, frame)]));
        break;
    case FKL_OP_PUSH_HASHEQ:
        num = ins->bu;
        goto push_hasheq;
    case FKL_OP_PUSH_HASHEQ_C:
        num = FKL_GET_INS_UC(ins);
        goto push_hasheq;
    case FKL_OP_PUSH_HASHEQ_X:
        num = GET_INS_UX(ins, frame);
        goto push_hasheq;
    case FKL_OP_PUSH_HASHEQ_XX:
        num = GET_INS_UXX(ins, frame);
    push_hasheq: {
        FklVMvalue *hash = fklCreateVMvalueHashEq(exe);
        uint64_t kvnum = num * 2;
        FklVMvalue **base = &exe->base[exe->tp - kvnum];
        FklHashTable *ht = FKL_VM_HASH(hash);
        for (uint32_t i = 0; i < kvnum; i += 2) {
            FklVMvalue *key = base[i];
            FklVMvalue *value = base[i + 1];
            fklVMhashTableSet(key, value, ht);
        }
        exe->tp -= kvnum;
        FKL_VM_PUSH_VALUE(exe, hash);
    } break;
    case FKL_OP_PUSH_HASHEQV:
        num = ins->bu;
        goto push_hasheqv;
    case FKL_OP_PUSH_HASHEQV_C:
        num = FKL_GET_INS_UC(ins);
        goto push_hasheqv;
    case FKL_OP_PUSH_HASHEQV_X:
        num = GET_INS_UX(ins, frame);
        goto push_hasheqv;
    case FKL_OP_PUSH_HASHEQV_XX:
        num = GET_INS_UXX(ins, frame);
    push_hasheqv: {
        FklVMvalue *hash = fklCreateVMvalueHashEqv(exe);
        uint64_t kvnum = num * 2;
        FklVMvalue **base = &exe->base[exe->tp - kvnum];
        FklHashTable *ht = FKL_VM_HASH(hash);
        for (uint32_t i = 0; i < kvnum; i += 2) {
            FklVMvalue *key = base[i];
            FklVMvalue *value = base[i + 1];
            fklVMhashTableSet(key, value, ht);
        }
        exe->tp -= kvnum;
        FKL_VM_PUSH_VALUE(exe, hash);
    } break;
    case FKL_OP_PUSH_HASHEQUAL:
        num = ins->bu;
        goto push_hashequal;
    case FKL_OP_PUSH_HASHEQUAL_C:
        num = FKL_GET_INS_UC(ins);
        goto push_hashequal;
    case FKL_OP_PUSH_HASHEQUAL_X:
        num = GET_INS_UX(ins, frame);
        goto push_hashequal;
    case FKL_OP_PUSH_HASHEQUAL_XX:
        num = GET_INS_UXX(ins, frame);
    push_hashequal: {
        FklVMvalue *hash = fklCreateVMvalueHashEqual(exe);
        uint64_t kvnum = num * 2;
        FklVMvalue **base = &exe->base[exe->tp - kvnum];
        FklHashTable *ht = FKL_VM_HASH(hash);
        for (size_t i = 0; i < kvnum; i += 2) {
            FklVMvalue *key = base[i];
            FklVMvalue *value = base[i + 1];
            fklVMhashTableSet(key, value, ht);
        }
        exe->tp -= kvnum;
        FKL_VM_PUSH_VALUE(exe, hash);
    } break;
    case FKL_OP_PUSH_LIST_0: {
        FklVMvalue *pair = FKL_VM_NIL;
        FklVMvalue *last = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue **pcur = &pair;
        size_t bp = exe->bp;
        for (size_t i = bp; exe->tp > bp; exe->tp--, i++) {
            *pcur = fklCreateVMvaluePairWithCar(exe, exe->base[i]);
            pcur = &FKL_VM_CDR(*pcur);
        }
        *pcur = last;
        fklResBp(exe);
        FKL_VM_PUSH_VALUE(exe, pair);
    } break;
    case FKL_OP_PUSH_LIST:
        size = ins->bu;
        goto push_list;
    case FKL_OP_PUSH_LIST_C:
        size = FKL_GET_INS_UC(ins);
        goto push_list;
    case FKL_OP_PUSH_LIST_X:
        size = GET_INS_UX(ins, frame);
        goto push_list;
    case FKL_OP_PUSH_LIST_XX:
        size = GET_INS_UXX(ins, frame);
    push_list: {
        FklVMvalue *pair = FKL_VM_NIL;
        FklVMvalue *last = FKL_VM_POP_TOP_VALUE(exe);
        size--;
        FklVMvalue **pcur = &pair;
        for (size_t i = exe->tp - size; i < exe->tp; i++) {
            *pcur = fklCreateVMvaluePairWithCar(exe, exe->base[i]);
            pcur = &FKL_VM_CDR(*pcur);
        }
        exe->tp -= size;
        *pcur = last;
        FKL_VM_PUSH_VALUE(exe, pair);
    } break;
    case FKL_OP_PUSH_VEC_0: {
        size_t size = exe->tp - exe->bp;
        FklVMvalue *vec = fklCreateVMvalueVec(exe, size);
        FklVMvec *vv = FKL_VM_VEC(vec);
        for (size_t i = size; i > 0; i--)
            vv->base[i - 1] = FKL_VM_POP_TOP_VALUE(exe);
        fklResBp(exe);
        FKL_VM_PUSH_VALUE(exe, vec);
    } break;
    case FKL_OP_LIST_PUSH: {
        FklVMvalue *list = FKL_VM_POP_TOP_VALUE(exe);
        for (; FKL_IS_PAIR(list); list = FKL_VM_CDR(list))
            FKL_VM_PUSH_VALUE(exe, FKL_VM_CAR(list));
        if (list != FKL_VM_NIL)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    } break;
    case FKL_OP_IMPORT:
        idx = ins->au;
        idx1 = ins->bu;
        goto import_lib;
    case FKL_OP_IMPORT_X:
        idx = FKL_GET_INS_UC(ins);
        ins = frame->c.pc++;
        idx1 = FKL_GET_INS_UC(ins);
        goto import_lib;
    case FKL_OP_IMPORT_XX:
        idx = FKL_GET_INS_UC(ins) | (((uint32_t)ins[1].au) << FKL_I24_WIDTH);
        idx1 = ins[1].bu | (((uint64_t)ins[2].bu) << FKL_I16_WIDTH);
        frame->c.pc += 2;
    import_lib: {
        FklVMlib *plib = exe->importingLib;
        uint32_t count = plib->count;
        FklVMvalue *v = idx >= count ? NULL : plib->loc[idx];
        GET_COMPOUND_FRAME_LOC(frame, idx1) = v;
    } break;
    case FKL_OP_PUSH_0:
        FKL_VM_PUSH_VALUE(exe, FKL_MAKE_VM_FIX(0));
        break;
    case FKL_OP_PUSH_1:
        FKL_VM_PUSH_VALUE(exe, FKL_MAKE_VM_FIX(1));
        break;
    case FKL_OP_PUSH_I8:
        FKL_VM_PUSH_VALUE(exe, FKL_MAKE_VM_FIX(ins->ai));
        break;
    case FKL_OP_PUSH_I16:
        FKL_VM_PUSH_VALUE(exe, FKL_MAKE_VM_FIX(ins->bi));
        break;
    case FKL_OP_PUSH_I64B:
        FKL_VM_PUSH_VALUE(
            exe, fklCreateVMvalueBigIntWithI64(exe, exe->gc->ki64[ins->bu]));
        break;
    case FKL_OP_PUSH_I64B_C:
        FKL_VM_PUSH_VALUE(exe, fklCreateVMvalueBigIntWithI64(
                                   exe, exe->gc->ki64[FKL_GET_INS_UC(ins)]));
        break;
    case FKL_OP_PUSH_I64B_X:
        FKL_VM_PUSH_VALUE(exe, fklCreateVMvalueBigIntWithI64(
                                   exe, exe->gc->ki64[GET_INS_UX(ins, frame)]));
        break;
    case FKL_OP_GET_LOC:
        FKL_VM_PUSH_VALUE(exe, GET_COMPOUND_FRAME_LOC(frame, ins->bu));
        break;
    case FKL_OP_GET_LOC_C:
        FKL_VM_PUSH_VALUE(exe,
                          GET_COMPOUND_FRAME_LOC(frame, FKL_GET_INS_UC(ins)));
        break;
    case FKL_OP_GET_LOC_X:
        FKL_VM_PUSH_VALUE(
            exe, GET_COMPOUND_FRAME_LOC(frame, GET_INS_UX(ins, frame)));
        break;
    case FKL_OP_PUT_LOC:
        GET_COMPOUND_FRAME_LOC(frame, ins->bu) = FKL_VM_GET_TOP_VALUE(exe);
        break;
    case FKL_OP_PUT_LOC_C:
        GET_COMPOUND_FRAME_LOC(frame, FKL_GET_INS_UC(ins)) =
            FKL_VM_GET_TOP_VALUE(exe);
        break;
    case FKL_OP_PUT_LOC_X:
        GET_COMPOUND_FRAME_LOC(frame, GET_INS_UX(ins, frame)) =
            FKL_VM_GET_TOP_VALUE(exe);
        break;
    case FKL_OP_GET_VAR_REF:
        idx = ins->bu;
        goto get_var_ref;
    case FKL_OP_GET_VAR_REF_C:
        idx = FKL_GET_INS_UC(ins);
        goto get_var_ref;
    case FKL_OP_GET_VAR_REF_X:
        idx = GET_INS_UX(ins, frame);
    get_var_ref: {
        FklSid_t id = 0;
        FklVMvalue *v = get_var_val(frame, idx, exe->pts, &id);
        if (id)
            FKL_RAISE_BUILTIN_ERROR_FMT(FKL_ERR_SYMUNDEFINE, exe,
                                        "Symbol %S is undefined",
                                        FKL_MAKE_VM_SYM(id));
        FKL_VM_PUSH_VALUE(exe, v);
    } break;
    case FKL_OP_PUT_VAR_REF:
        idx = ins->bu;
        goto put_var_ref;
    case FKL_OP_PUT_VAR_REF_C:
        idx = FKL_GET_INS_UC(ins);
        goto put_var_ref;
    case FKL_OP_PUT_VAR_REF_X:
        idx = GET_INS_UX(ins, frame);
    put_var_ref: {
        FklSid_t id = 0;
        FklVMvalue *volatile *pv = get_var_ref(frame, idx, exe->pts, &id);
        if (!pv)
            FKL_RAISE_BUILTIN_ERROR_FMT(FKL_ERR_SYMUNDEFINE, exe,
                                        "Symbol %S is undefined",
                                        FKL_MAKE_VM_SYM(id));
        *pv = FKL_VM_GET_TOP_VALUE(exe);
    } break;
    case FKL_OP_EXPORT:
        idx = ins->bu;
        goto export;
    case FKL_OP_EXPORT_C:
        idx = FKL_GET_INS_UC(ins);
        goto export;
    case FKL_OP_EXPORT_X:
        idx = GET_INS_UX(ins, frame);
        export : {
            FklVMlib *lib = exe->importingLib;
            FklVMCompoundFrameVarRef *lr = fklGetCompoundFrameLocRef(frame);
            lib->loc[lib->count++] = lr->loc[idx];
        }
        break;
    case FKL_OP_LOAD_LIB:
        plib = &exe->libs[ins->bu];
        goto load_lib;
    case FKL_OP_LOAD_LIB_C:
        plib = &exe->libs[FKL_GET_INS_UC(ins)];
        goto load_lib;
    case FKL_OP_LOAD_LIB_X:
        plib = &exe->libs[GET_INS_UX(ins, frame)];
    load_lib: {
        if (plib->imported) {
            exe->importingLib = plib;
            FKL_VM_PUSH_VALUE(exe, FKL_VM_NIL);
        } else
            call_compound_procedure(exe, plib->proc);
        return;
    } break;
    case FKL_OP_LOAD_DLL:
        plib = &exe->libs[ins->bu];
        goto load_dll;
    case FKL_OP_LOAD_DLL_C:
        plib = &exe->libs[FKL_GET_INS_UC(ins)];
        goto load_dll;
    case FKL_OP_LOAD_DLL_X:
        plib = &exe->libs[GET_INS_UX(ins, frame)];
    load_dll: {
        if (!plib->imported) {
            FklVMvalue *errorStr = NULL;
            FklVMvalue *dll = fklCreateVMvalueDll(
                exe, FKL_VM_STR(plib->proc)->str, &errorStr);
            FklImportDllInitFunc initFunc = NULL;
            if (dll)
                initFunc = getImportInit(&(FKL_VM_DLL(dll)->dll));
            else
                FKL_RAISE_BUILTIN_ERROR_FMT(FKL_ERR_IMPORTFAILED, exe,
                                            FKL_VM_STR(errorStr)->str);
            if (!initFunc)
                FKL_RAISE_BUILTIN_ERROR_FMT(FKL_ERR_IMPORTFAILED, exe,
                                            "Failed to import dll: %s",
                                            plib->proc);
            uint32_t tp = exe->tp;
            fklInitVMdll(dll, exe);
            plib->loc = initFunc(exe, dll, &plib->count);
            plib->imported = 1;
            plib->belong = 1;
            plib->proc = FKL_VM_NIL;
            exe->tp = tp;
        }
        exe->importingLib = plib;
        FKL_VM_PUSH_VALUE(exe, FKL_VM_NIL);
    } break;
    case FKL_OP_TRUE:
        DROP_TOP(exe);
        FKL_VM_PUSH_VALUE(exe, FKL_MAKE_VM_FIX(1));
        break;
    case FKL_OP_NOT:
        if (FKL_VM_POP_TOP_VALUE(exe) == FKL_VM_NIL)
            FKL_VM_PUSH_VALUE(exe, FKL_VM_TRUE);
        else
            FKL_VM_PUSH_VALUE(exe, FKL_VM_NIL);
        break;
    case FKL_OP_EQ: {
        FklVMvalue *fir = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *sec = FKL_VM_POP_TOP_VALUE(exe);
        FKL_VM_PUSH_VALUE(exe,
                          fklVMvalueEq(fir, sec) ? FKL_VM_TRUE : FKL_VM_NIL);
    } break;
    case FKL_OP_EQV: {
        FklVMvalue *fir = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *sec = FKL_VM_POP_TOP_VALUE(exe);
        FKL_VM_PUSH_VALUE(exe,
                          (fklVMvalueEqv(fir, sec)) ? FKL_VM_TRUE : FKL_VM_NIL);
    } break;
    case FKL_OP_EQUAL: {
        FklVMvalue *fir = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *sec = FKL_VM_POP_TOP_VALUE(exe);
        FKL_VM_PUSH_VALUE(exe, (fklVMvalueEqual(fir, sec)) ? FKL_VM_TRUE
                                                           : FKL_VM_NIL);
    } break;
    case FKL_OP_EQN: {
        FklVMvalue *a = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *b = FKL_VM_POP_TOP_VALUE(exe);
        int err = 0;
        int r = fklVMvalueCmp(a, b, &err) == 0;
        if (err)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
        FKL_VM_PUSH_VALUE(exe, r ? FKL_VM_TRUE : FKL_VM_NIL);
    } break;
    case FKL_OP_EQN3: {
        FklVMvalue *a = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *b = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *c = FKL_VM_POP_TOP_VALUE(exe);
        int err = 0;
        int r = fklVMvalueCmp(a, b, &err) == 0 && !err
             && fklVMvalueCmp(b, c, &err) == 0;
        if (err)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
        FKL_VM_PUSH_VALUE(exe, r ? FKL_VM_TRUE : FKL_VM_NIL);
    } break;
    case FKL_OP_GT: {
        FklVMvalue *a = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *b = FKL_VM_POP_TOP_VALUE(exe);
        int err = 0;
        int r = fklVMvalueCmp(a, b, &err) > 0;
        if (err)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
        FKL_VM_PUSH_VALUE(exe, r ? FKL_VM_TRUE : FKL_VM_NIL);
    } break;
    case FKL_OP_GT3: {
        FklVMvalue *a = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *b = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *c = FKL_VM_POP_TOP_VALUE(exe);
        int err = 0;
        int r = fklVMvalueCmp(a, b, &err) > 0 && !err
             && fklVMvalueCmp(b, c, &err) > 0;
        if (err)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
        FKL_VM_PUSH_VALUE(exe, r ? FKL_VM_TRUE : FKL_VM_NIL);
    } break;
    case FKL_OP_LT: {
        FklVMvalue *a = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *b = FKL_VM_POP_TOP_VALUE(exe);
        int err = 0;
        int r = fklVMvalueCmp(a, b, &err) < 0;
        if (err)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
        FKL_VM_PUSH_VALUE(exe, r ? FKL_VM_TRUE : FKL_VM_NIL);
    } break;
    case FKL_OP_LT3: {
        FklVMvalue *a = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *b = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *c = FKL_VM_POP_TOP_VALUE(exe);
        int err = 0;
        int r = fklVMvalueCmp(a, b, &err) < 0 && !err
             && fklVMvalueCmp(b, c, &err) < 0;
        if (err)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
        FKL_VM_PUSH_VALUE(exe, r ? FKL_VM_TRUE : FKL_VM_NIL);
    } break;
    case FKL_OP_GE: {
        FklVMvalue *a = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *b = FKL_VM_POP_TOP_VALUE(exe);
        int err = 0;
        int r = fklVMvalueCmp(a, b, &err) >= 0;
        if (err)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
        FKL_VM_PUSH_VALUE(exe, r ? FKL_VM_TRUE : FKL_VM_NIL);
    } break;
    case FKL_OP_GE3: {
        FklVMvalue *a = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *b = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *c = FKL_VM_POP_TOP_VALUE(exe);
        int err = 0;
        int r = fklVMvalueCmp(a, b, &err) >= 0 && !err
             && fklVMvalueCmp(b, c, &err) >= 0;
        if (err)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
        FKL_VM_PUSH_VALUE(exe, r ? FKL_VM_TRUE : FKL_VM_NIL);
    } break;
    case FKL_OP_LE: {
        FklVMvalue *a = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *b = FKL_VM_POP_TOP_VALUE(exe);
        int err = 0;
        int r = fklVMvalueCmp(a, b, &err) <= 0;
        if (err)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
        FKL_VM_PUSH_VALUE(exe, r ? FKL_VM_TRUE : FKL_VM_NIL);
    } break;
    case FKL_OP_LE3: {
        FklVMvalue *a = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *b = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *c = FKL_VM_POP_TOP_VALUE(exe);
        int err = 0;
        int r = fklVMvalueCmp(a, b, &err) <= 0 && !err
             && fklVMvalueCmp(b, c, &err) <= 0;
        if (err)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
        FKL_VM_PUSH_VALUE(exe, r ? FKL_VM_TRUE : FKL_VM_NIL);
    } break;
    case FKL_OP_INC: {
        FklVMvalue *arg = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *r = fklProcessVMnumInc(exe, arg);
        if (r)
            FKL_VM_PUSH_VALUE(exe, r);
        else
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    } break;
    case FKL_OP_DEC: {
        FklVMvalue *arg = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *r = fklProcessVMnumDec(exe, arg);
        if (r)
            FKL_VM_PUSH_VALUE(exe, r);
        else
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    } break;
    case FKL_OP_ADD: {
        FklVMvalue *a = FKL_VM_POP_TOP_VALUE(exe);
        int64_t r64 = 0;
        double rd = 0.0;
        FklBigInt bi = FKL_BIGINT_0;
        FklVMvalue *b = FKL_VM_POP_TOP_VALUE(exe);
        PROCESS_ADD(a);
        PROCESS_ADD(b);
        PROCESS_ADD_RES();
    } break;
    case FKL_OP_SUB: {
        FklVMvalue *a = FKL_VM_POP_TOP_VALUE(exe);
        FKL_CHECK_TYPE(a, fklIsVMnumber, exe);

        int64_t r64 = 0;
        double rd = 0.0;
        FklBigInt bi = FKL_BIGINT_0;

        FklVMvalue *b = FKL_VM_POP_TOP_VALUE(exe);
        PROCESS_ADD(b);
        PROCESS_SUB_RES();
    } break;
    case FKL_OP_MUL: {
        FklVMvalue *a = FKL_VM_POP_TOP_VALUE(exe);
        int64_t r64 = 1;
        double rd = 1.0;
        FklBigInt bi = FKL_BIGINT_0;
        FklVMvalue *b = FKL_VM_POP_TOP_VALUE(exe);
        PROCESS_MUL(a);
        PROCESS_MUL(b);
        PROCESS_MUL_RES();
    } break;
    case FKL_OP_DIV: {
        FklVMvalue *a = FKL_VM_POP_TOP_VALUE(exe);
        FKL_CHECK_TYPE(a, fklIsVMnumber, exe);

        int64_t r64 = 1;
        double rd = 1.0;
        FklBigInt bi = FKL_BIGINT_0;

        FklVMvalue *b = FKL_VM_POP_TOP_VALUE(exe);
        PROCESS_MUL(b);
        PROCESS_DIV_RES();
    } break;
    case FKL_OP_IDIV: {
        FklVMvalue *a = FKL_VM_POP_TOP_VALUE(exe);
        FKL_CHECK_TYPE(a, fklIsVMint, exe);

        int64_t r64 = 1;
        FklBigInt bi = FKL_BIGINT_0;

        FklVMvalue *b = FKL_VM_POP_TOP_VALUE(exe);
        PROCESS_IMUL(b);
        PROCESS_IDIV_RES();
    } break;
    case FKL_OP_MOD: {
        FklVMvalue *fir = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *sec = FKL_VM_POP_TOP_VALUE(exe);
        if (!fklIsVMnumber(fir) || !fklIsVMnumber(sec))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
        FklVMvalue *r = fklProcessVMnumMod(exe, fir, sec);
        if (!r)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_DIVZEROERROR, exe);
        FKL_VM_PUSH_VALUE(exe, r);
    } break;
    case FKL_OP_ADD1: {
        FklVMvalue *a = FKL_VM_POP_TOP_VALUE(exe);
        if (FKL_IS_BIG_INT(a))
            FKL_VM_PUSH_VALUE(
                exe, fklCreateVMvalueBigIntWithOther(exe, FKL_VM_BI(a)));
        else if (FKL_IS_F64(a))
            FKL_VM_PUSH_VALUE(exe, fklCreateVMvalueF64(exe, FKL_VM_F64(a)));
        else if (FKL_IS_FIX(a))
            FKL_VM_PUSH_VALUE(exe, a);
        else
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    } break;
    case FKL_OP_MUL1: {
        FklVMvalue *a = FKL_VM_POP_TOP_VALUE(exe);
        if (FKL_IS_BIG_INT(a))
            FKL_VM_PUSH_VALUE(
                exe, fklCreateVMvalueBigIntWithOther(exe, FKL_VM_BI(a)));
        else if (FKL_IS_F64(a))
            FKL_VM_PUSH_VALUE(exe, fklCreateVMvalueF64(exe, FKL_VM_F64(a)));
        else if (FKL_IS_FIX(a))
            FKL_VM_PUSH_VALUE(exe, a);
        else
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    } break;
    case FKL_OP_NEG: {
        FklVMvalue *a = FKL_VM_POP_TOP_VALUE(exe);
        FKL_VM_PUSH_VALUE(exe, fklProcessVMnumNeg(exe, a));
    } break;
    case FKL_OP_REC: {
        FklVMvalue *a = FKL_VM_POP_TOP_VALUE(exe);
        FKL_CHECK_TYPE(a, fklIsVMnumber, exe);
        FklVMvalue *r = fklProcessVMnumRec(exe, a);
        if (!r)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_DIVZEROERROR, exe);
        FKL_VM_PUSH_VALUE(exe, r);
    } break;
    case FKL_OP_ADD3: {
        FklVMvalue *a = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *b = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *c = FKL_VM_POP_TOP_VALUE(exe);
        int64_t r64 = 0;
        double rd = 0.0;
        FklBigInt bi = FKL_BIGINT_0;
        PROCESS_ADD(a);
        PROCESS_ADD(b);
        PROCESS_ADD(c);
        PROCESS_ADD_RES();
    } break;
    case FKL_OP_SUB3: {
        FklVMvalue *a = FKL_VM_POP_TOP_VALUE(exe);
        FKL_CHECK_TYPE(a, fklIsVMnumber, exe);

        int64_t r64 = 0;
        double rd = 0.0;
        FklBigInt bi = FKL_BIGINT_0;

        FklVMvalue *b = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *c = FKL_VM_POP_TOP_VALUE(exe);
        PROCESS_ADD(b);
        PROCESS_ADD(c);
        PROCESS_SUB_RES();
    } break;
    case FKL_OP_MUL3: {
        FklVMvalue *a = FKL_VM_POP_TOP_VALUE(exe);
        int64_t r64 = 1;
        double rd = 1.0;
        FklBigInt bi = FKL_BIGINT_0;
        FklVMvalue *b = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *c = FKL_VM_POP_TOP_VALUE(exe);
        PROCESS_MUL(a);
        PROCESS_MUL(b);
        PROCESS_MUL(c);
        PROCESS_MUL_RES();
    } break;
    case FKL_OP_DIV3: {
        FklVMvalue *a = FKL_VM_POP_TOP_VALUE(exe);
        FKL_CHECK_TYPE(a, fklIsVMnumber, exe);

        int64_t r64 = 1;
        double rd = 1.0;
        FklBigInt bi = FKL_BIGINT_0;

        FklVMvalue *b = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *c = FKL_VM_POP_TOP_VALUE(exe);
        PROCESS_MUL(b);
        PROCESS_MUL(c);
        PROCESS_DIV_RES();
    } break;
    case FKL_OP_IDIV3: {
        FklVMvalue *a = FKL_VM_POP_TOP_VALUE(exe);
        FKL_CHECK_TYPE(a, fklIsVMint, exe);

        int64_t r64 = 1;
        FklBigInt bi = FKL_BIGINT_0;

        FklVMvalue *b = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *c = FKL_VM_POP_TOP_VALUE(exe);
        PROCESS_IMUL(b);
        PROCESS_IMUL(c);
        PROCESS_IDIV_RES();
    } break;
    case FKL_OP_PUSH_CAR: {
        FklVMvalue *obj = FKL_VM_POP_TOP_VALUE(exe);
        FKL_CHECK_TYPE(obj, FKL_IS_PAIR, exe);
        FKL_VM_PUSH_VALUE(exe, FKL_VM_CAR(obj));
    } break;
    case FKL_OP_PUSH_CDR: {
        FklVMvalue *obj = FKL_VM_POP_TOP_VALUE(exe);
        FKL_CHECK_TYPE(obj, FKL_IS_PAIR, exe);
        FKL_VM_PUSH_VALUE(exe, FKL_VM_CDR(obj));
    } break;
    case FKL_OP_CONS: {
        FklVMvalue *car = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *cdr = FKL_VM_POP_TOP_VALUE(exe);
        FKL_VM_PUSH_VALUE(exe, fklCreateVMvaluePair(exe, car, cdr));
    } break;
    case FKL_OP_NTH: {
        FklVMvalue *place = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *objlist = FKL_VM_POP_TOP_VALUE(exe);
        FKL_CHECK_TYPE(place, fklIsVMint, exe);
        if (fklIsVMnumberLt0(place))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
        size_t index = fklVMgetUint(place);
        if (objlist == FKL_VM_NIL || FKL_IS_PAIR(objlist)) {
            FklVMvalue *objPair = objlist;
            for (uint64_t i = 0; i < index && FKL_IS_PAIR(objPair);
                 i++, objPair = FKL_VM_CDR(objPair))
                ;
            if (FKL_IS_PAIR(objPair))
                FKL_VM_PUSH_VALUE(exe, FKL_VM_CAR(objPair));
            else
                FKL_VM_PUSH_VALUE(exe, FKL_VM_NIL);
        } else
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    } break;
    case FKL_OP_VEC_REF: {
        FklVMvalue *vec = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *place = FKL_VM_POP_TOP_VALUE(exe);
        if (!fklIsVMint(place) || !FKL_IS_VECTOR(vec))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
        if (fklIsVMnumberLt0(place))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
        size_t index = fklVMgetUint(place);
        FklVMvec *vv = FKL_VM_VEC(vec);
        if (index >= vv->size)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
        FKL_VM_PUSH_VALUE(exe, vv->base[index]);
    } break;
    case FKL_OP_STR_REF: {
        FklVMvalue *str = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *place = FKL_VM_POP_TOP_VALUE(exe);
        if (!fklIsVMint(place) || !FKL_IS_STR(str))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
        if (fklIsVMnumberLt0(place))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
        size_t index = fklVMgetUint(place);
        FklString *ss = FKL_VM_STR(str);
        size_t size = ss->size;
        if (index >= size)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
        FKL_VM_PUSH_VALUE(exe, FKL_MAKE_VM_CHR(ss->str[index]));
    } break;
    case FKL_OP_BVEC_REF: {
        FklVMvalue *bvec = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *place = FKL_VM_POP_TOP_VALUE(exe);
        if (!fklIsVMint(place) || !FKL_IS_BYTEVECTOR(bvec))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
        if (fklIsVMnumberLt0(place))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
        size_t index = fklVMgetUint(place);
        FklBytevector *ss = FKL_VM_BVEC(bvec);
        size_t size = ss->size;
        if (index >= size)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
        FKL_VM_PUSH_VALUE(exe, FKL_MAKE_VM_FIX(ss->ptr[index]));
    } break;
    case FKL_OP_BOX: {
        FklVMvalue *obj = FKL_VM_POP_TOP_VALUE(exe);
        FKL_VM_PUSH_VALUE(exe, fklCreateVMvalueBox(exe, obj));
    } break;
    case FKL_OP_UNBOX: {
        FklVMvalue *box = FKL_VM_POP_TOP_VALUE(exe);
        FKL_CHECK_TYPE(box, FKL_IS_BOX, exe);
        FKL_VM_PUSH_VALUE(exe, FKL_VM_BOX(box));
    } break;
    case FKL_OP_BOX0:
        FKL_VM_PUSH_VALUE(exe, fklCreateVMvalueBoxNil(exe));
        break;
    case FKL_OP_CLOSE_REF:
        idx = ins->au;
        idx1 = ins->bu;
        goto close_ref;
    case FKL_OP_CLOSE_REF_X:
        idx = FKL_GET_INS_UC(ins);
        ins = frame->c.pc++;
        idx1 = FKL_GET_INS_UC(ins);
        goto close_ref;
    case FKL_OP_CLOSE_REF_XX:
        idx = FKL_GET_INS_UC(ins) | (((uint32_t)ins[1].au) << FKL_I24_WIDTH);
        idx1 = ins[1].bu | (((uint64_t)ins[2].bu) << FKL_I16_WIDTH);
        frame->c.pc += 2;
    close_ref:
        close_var_ref_between(fklGetCompoundFrameLocRef(frame)->lref, idx,
                              idx1);
        break;
    case FKL_OP_RET: {
        if (frame->c.mark) {
            close_all_var_ref(&frame->c.lr);
            if (frame->c.lr.lrefl) {
                frame->c.lr.lrefl = NULL;
                memset(frame->c.lr.lref, 0,
                       sizeof(FklVMvalue *) * frame->c.lr.lcount);
            }
            frame->c.pc = frame->c.spc;
            frame->c.mark = 0;
            frame->c.tail = 0;
        } else {
            fklDoFinalizeCompoundFrame(exe, popFrame(exe));
            return;
        }
    } break;
    case FKL_OP_EXPORT_TO:
        idx = ins->au;
        idx1 = ins->bu;
        goto export_to;
    case FKL_OP_EXPORT_TO_X:
        idx = FKL_GET_INS_UC(ins);
        ins = frame->c.pc++;
        idx1 = FKL_GET_INS_UC(ins);
        goto export_to;
    case FKL_OP_EXPORT_TO_XX:
        idx = FKL_GET_INS_UC(ins) | (((uint32_t)ins[1].au) << FKL_I24_WIDTH);
        idx1 = ins[1].bu | (((uint64_t)ins[2].bu) << FKL_I16_WIDTH);
        frame->c.pc += 2;
    export_to: {
        FklVMlib *lib = &exe->libs[idx];
        DROP_TOP(exe);
        FklVMvalue **loc = NULL;
        if (idx1) {
            loc = (FklVMvalue **)malloc(idx1 * sizeof(FklVMvalue *));
            FKL_ASSERT(loc);
        }
        lib->loc = loc;
        lib->count = 0;
        lib->imported = 1;
        lib->belong = 1;
        lib->proc = FKL_VM_NIL;
        exe->importingLib = lib;
        FKL_VM_PUSH_VALUE(exe, FKL_VM_NIL);
    } break;
    case FKL_OP_RES_BP_TP:
        exe->tp = frame->c.tp;
        exe->bp = frame->c.bp;
        break;
    case FKL_OP_ATOM: {
        FklVMvalue *val = FKL_VM_POP_TOP_VALUE(exe);
        FKL_VM_PUSH_VALUE(exe, !FKL_IS_PAIR(val) ? FKL_VM_TRUE : FKL_VM_NIL);
    } break;
    case FKL_OP_VEC_FIRST: {
        FklVMvalue *vec = FKL_VM_POP_TOP_VALUE(exe);
        if (!FKL_IS_VECTOR(vec))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
        FklVMvec *vv = FKL_VM_VEC(vec);
        if (!vv->size)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
        FKL_VM_PUSH_VALUE(exe, vv->base[0]);
    } break;
    case FKL_OP_VEC_LAST: {
        FklVMvalue *vec = FKL_VM_POP_TOP_VALUE(exe);
        if (!FKL_IS_VECTOR(vec))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
        FklVMvec *vv = FKL_VM_VEC(vec);
        if (!vv->size)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
        FKL_VM_PUSH_VALUE(exe, vv->base[vv->size - 1]);
    } break;
    case FKL_OP_POP_LOC:
        idx = ins->bu;
        goto pop_loc;
    case FKL_OP_POP_LOC_C:
        idx = FKL_GET_INS_UC(ins);
        goto pop_loc;
    case FKL_OP_POP_LOC_X:
        idx = GET_INS_UX(ins, frame);
    pop_loc: {
        FklVMvalue *v = FKL_VM_POP_TOP_VALUE(exe);
        GET_COMPOUND_FRAME_LOC(frame, idx) = v;
    } break;
    case FKL_OP_CAR_SET: {
        FklVMvalue *pair = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *value = FKL_VM_GET_TOP_VALUE(exe);
        FKL_CHECK_TYPE(pair, FKL_IS_PAIR, exe);
        FKL_VM_CAR(pair) = value;
    } break;
    case FKL_OP_CDR_SET: {
        FklVMvalue *pair = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *value = FKL_VM_GET_TOP_VALUE(exe);
        FKL_CHECK_TYPE(pair, FKL_IS_PAIR, exe);
        FKL_VM_CDR(pair) = value;
    } break;
    case FKL_OP_BOX_SET: {
        FklVMvalue *box = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *value = FKL_VM_GET_TOP_VALUE(exe);
        FKL_CHECK_TYPE(box, FKL_IS_BOX, exe);
        FKL_VM_BOX(box) = value;
    } break;
    case FKL_OP_VEC_SET: {
        FklVMvalue *vec = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *place = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *value = FKL_VM_GET_TOP_VALUE(exe);
        if (!fklIsVMint(place) || !FKL_IS_VECTOR(vec))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
        if (fklIsVMnumberLt0(place))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
        size_t index = fklVMgetUint(place);
        FklVMvec *v = FKL_VM_VEC(vec);
        size_t size = v->size;
        if (index >= size)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
        v->base[index] = value;
    } break;
    case FKL_OP_STR_SET: {
        FklVMvalue *str = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *place = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *value = FKL_VM_GET_TOP_VALUE(exe);
        if (!fklIsVMint(place) || !FKL_IS_STR(str) || !FKL_IS_CHR(value))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
        if (fklIsVMnumberLt0(place))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
        size_t index = fklVMgetUint(place);
        FklString *s = FKL_VM_STR(str);
        size_t size = s->size;
        if (index >= size)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
        s->str[index] = FKL_GET_CHR(value);
    } break;
    case FKL_OP_BVEC_SET: {
        FklVMvalue *bvec = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *place = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *value = FKL_VM_GET_TOP_VALUE(exe);
        if (!fklIsVMint(place) || !FKL_IS_BYTEVECTOR(bvec)
            || !fklIsVMint(value))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
        if (fklIsVMnumberLt0(place))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
        size_t index = fklVMgetUint(place);
        FklBytevector *s = FKL_VM_BVEC(bvec);
        size_t size = s->size;
        if (index >= size)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
        s->ptr[index] = fklVMgetInt(value);
    } break;
    case FKL_OP_HASH_REF_2: {
        FklVMvalue *ht = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *key = FKL_VM_POP_TOP_VALUE(exe);
        FKL_CHECK_TYPE(ht, FKL_IS_HASHTABLE, exe);
        int ok;
        FklVMvalue *retval = fklVMhashTableGet(key, FKL_VM_HASH(ht), &ok);
        if (ok)
            FKL_VM_PUSH_VALUE(exe, retval);
        else
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NO_VALUE_FOR_KEY, exe);
    } break;
    case FKL_OP_HASH_REF_3: {
        FklVMvalue *ht = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *key = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue **defa = &FKL_VM_GET_TOP_VALUE(exe);
        FKL_CHECK_TYPE(ht, FKL_IS_HASHTABLE, exe);
        int ok;
        FklVMvalue *retval = fklVMhashTableGet(key, FKL_VM_HASH(ht), &ok);
        if (ok)
            *defa = retval;
    } break;
    case FKL_OP_HASH_SET: {
        FklVMvalue *ht = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *key = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *value = FKL_VM_GET_TOP_VALUE(exe);
        FKL_CHECK_TYPE(ht, FKL_IS_HASHTABLE, exe);
        fklVMhashTableSet(key, value, FKL_VM_HASH(ht));
    } break;
    case FKL_OP_INC_LOC: {
        FklVMvalue *v = GET_COMPOUND_FRAME_LOC(frame, ins->bu);
        FklVMvalue *r = fklProcessVMnumInc(exe, v);
        if (r)
            FKL_VM_PUSH_VALUE(exe, r);
        else
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
        GET_COMPOUND_FRAME_LOC(frame, ins->bu) = r;
    } break;

    case FKL_OP_DEC_LOC: {
        FklVMvalue *v = GET_COMPOUND_FRAME_LOC(frame, ins->bu);
        FklVMvalue *r = fklProcessVMnumDec(exe, v);
        if (r)
            FKL_VM_PUSH_VALUE(exe, r);
        else
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
        GET_COMPOUND_FRAME_LOC(frame, ins->bu) = r;
    } break;
    case FKL_OP_CALL_LOC: {
        FklVMvalue *proc = GET_COMPOUND_FRAME_LOC(frame, ins->bu);
        if (!fklIsCallable(proc))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_CALL_ERROR, exe);
        switch (proc->type) {
        case FKL_TYPE_PROC:
            call_compound_procedure(exe, proc);
            break;
            CALL_CALLABLE_OBJ(exe, proc);
        }
        return;
    } break;
    case FKL_OP_TAIL_CALL_LOC: {
        FklVMvalue *proc = GET_COMPOUND_FRAME_LOC(frame, ins->bu);
        if (!fklIsCallable(proc))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_CALL_ERROR, exe);
        switch (proc->type) {
        case FKL_TYPE_PROC:
            tail_call_proc(exe, proc);
            break;
            CALL_CALLABLE_OBJ(exe, proc);
        }
        return;
    } break;
    case FKL_OP_CALL_VAR_REF: {
        FklSid_t id = 0;
        FklVMvalue *proc = get_var_val(frame, ins->bu, exe->pts, &id);
        if (id)
            FKL_RAISE_BUILTIN_ERROR_FMT(FKL_ERR_SYMUNDEFINE, exe,
                                        "Symbol %S is undefined",
                                        FKL_MAKE_VM_SYM(id));
        if (!fklIsCallable(proc))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_CALL_ERROR, exe);
        switch (proc->type) {
        case FKL_TYPE_PROC:
            call_compound_procedure(exe, proc);
            break;
            CALL_CALLABLE_OBJ(exe, proc);
        }
        return;
    } break;
    case FKL_OP_TAIL_CALL_VAR_REF: {
        FklSid_t id = 0;
        FklVMvalue *proc = get_var_val(frame, ins->bu, exe->pts, &id);
        if (id)
            FKL_RAISE_BUILTIN_ERROR_FMT(FKL_ERR_SYMUNDEFINE, exe,
                                        "Symbol %S is undefined",
                                        FKL_MAKE_VM_SYM(id));
        if (!fklIsCallable(proc))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_CALL_ERROR, exe);
        switch (proc->type) {
        case FKL_TYPE_PROC:
            tail_call_proc(exe, proc);
            break;
            CALL_CALLABLE_OBJ(exe, proc);
        }
        return;
    } break;
    case FKL_OP_CALL_VEC: {
        FklVMvalue *vec = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *place = FKL_VM_POP_TOP_VALUE(exe);
        if (!fklIsVMint(place) || !FKL_IS_VECTOR(vec))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
        if (fklIsVMnumberLt0(place))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
        size_t index = fklVMgetUint(place);
        FklVMvec *vv = FKL_VM_VEC(vec);
        if (index >= vv->size)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
        FklVMvalue *proc = vv->base[index];
        if (!fklIsCallable(proc))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_CALL_ERROR, exe);
        switch (proc->type) {
        case FKL_TYPE_PROC:
            call_compound_procedure(exe, proc);
            break;
            CALL_CALLABLE_OBJ(exe, proc);
        }
        return;
    } break;
    case FKL_OP_TAIL_CALL_VEC: {
        FklVMvalue *vec = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *place = FKL_VM_POP_TOP_VALUE(exe);
        if (!fklIsVMint(place) || !FKL_IS_VECTOR(vec))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
        if (fklIsVMnumberLt0(place))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
        size_t index = fklVMgetUint(place);
        FklVMvec *vv = FKL_VM_VEC(vec);
        if (index >= vv->size)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
        FklVMvalue *proc = vv->base[index];
        if (!fklIsCallable(proc))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_CALL_ERROR, exe);
        switch (proc->type) {
        case FKL_TYPE_PROC:
            tail_call_proc(exe, proc);
            break;
            CALL_CALLABLE_OBJ(exe, proc);
        }
        return;
    } break;
    case FKL_OP_CALL_CAR: {
        FklVMvalue *obj = FKL_VM_POP_TOP_VALUE(exe);
        FKL_CHECK_TYPE(obj, FKL_IS_PAIR, exe);
        FklVMvalue *proc = FKL_VM_CAR(obj);
        if (!fklIsCallable(proc))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_CALL_ERROR, exe);
        switch (proc->type) {
        case FKL_TYPE_PROC:
            call_compound_procedure(exe, proc);
            break;
            CALL_CALLABLE_OBJ(exe, proc);
        }
        return;
    } break;
    case FKL_OP_TAIL_CALL_CAR: {
        FklVMvalue *obj = FKL_VM_POP_TOP_VALUE(exe);
        FKL_CHECK_TYPE(obj, FKL_IS_PAIR, exe);
        FklVMvalue *proc = FKL_VM_CAR(obj);
        if (!fklIsCallable(proc))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_CALL_ERROR, exe);
        switch (proc->type) {
        case FKL_TYPE_PROC:
            tail_call_proc(exe, proc);
            break;
            CALL_CALLABLE_OBJ(exe, proc);
        }
        return;
    } break;
    case FKL_OP_CALL_CDR: {
        FklVMvalue *obj = FKL_VM_POP_TOP_VALUE(exe);
        FKL_CHECK_TYPE(obj, FKL_IS_PAIR, exe);
        FklVMvalue *proc = FKL_VM_CDR(obj);
        if (!fklIsCallable(proc))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_CALL_ERROR, exe);
        switch (proc->type) {
        case FKL_TYPE_PROC:
            call_compound_procedure(exe, proc);
            break;
            CALL_CALLABLE_OBJ(exe, proc);
        }
        return;
    } break;
    case FKL_OP_TAIL_CALL_CDR: {
        FklVMvalue *obj = FKL_VM_POP_TOP_VALUE(exe);
        FKL_CHECK_TYPE(obj, FKL_IS_PAIR, exe);
        FklVMvalue *proc = FKL_VM_CDR(obj);
        if (!fklIsCallable(proc))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_CALL_ERROR, exe);
        switch (proc->type) {
        case FKL_TYPE_PROC:
            tail_call_proc(exe, proc);
            break;
            CALL_CALLABLE_OBJ(exe, proc);
        }
        return;
    } break;
    case FKL_OP_RET_IF_TRUE:
        if (FKL_VM_GET_TOP_VALUE(exe) != FKL_VM_NIL) {
            if (frame->c.mark) {
                close_all_var_ref(&frame->c.lr);
                if (frame->c.lr.lrefl) {
                    frame->c.lr.lrefl = NULL;
                    memset(frame->c.lr.lref, 0,
                           sizeof(FklVMvalue *) * frame->c.lr.lcount);
                }
                frame->c.pc = frame->c.spc;
                frame->c.mark = 0;
                frame->c.tail = 0;
            } else {
                fklDoFinalizeCompoundFrame(exe, popFrame(exe));
                return;
            }
        }
        break;
    case FKL_OP_RET_IF_FALSE:
        if (FKL_VM_GET_TOP_VALUE(exe) == FKL_VM_NIL) {
            if (frame->c.mark) {
                close_all_var_ref(&frame->c.lr);
                if (frame->c.lr.lrefl) {
                    frame->c.lr.lrefl = NULL;
                    memset(frame->c.lr.lref, 0,
                           sizeof(FklVMvalue *) * frame->c.lr.lcount);
                }
                frame->c.pc = frame->c.spc;
                frame->c.mark = 0;
                frame->c.tail = 0;
            } else {
                fklDoFinalizeCompoundFrame(exe, popFrame(exe));
                return;
            }
        }
        break;
    case FKL_OP_MOV_LOC:
        FKL_VM_PUSH_VALUE(exe, (GET_COMPOUND_FRAME_LOC(frame, ins->bu) =
                                    GET_COMPOUND_FRAME_LOC(frame, ins->au)));
        break;
    case FKL_OP_MOV_VAR_REF: {
        FklSid_t id = 0;
        FklVMvalue *v = get_var_val(frame, ins->au, exe->pts, &id);
        if (id)
            FKL_RAISE_BUILTIN_ERROR_FMT(FKL_ERR_SYMUNDEFINE, exe,
                                        "Symbol %S is undefined",
                                        FKL_MAKE_VM_SYM(id));
        FKL_VM_PUSH_VALUE(exe, v);
        FklVMvalue *volatile *pv = get_var_ref(frame, ins->bu, exe->pts, &id);
        if (!pv)
            FKL_RAISE_BUILTIN_ERROR_FMT(FKL_ERR_SYMUNDEFINE, exe,
                                        "Symbol %S is undefined",
                                        FKL_MAKE_VM_SYM(id));
        *pv = v;
    } break;
    case FKL_OP_EXTRA_ARG:
    case FKL_OP_LAST_OPCODE:
        abort();
        break;
#ifndef DISPATCH_SWITCH
    }
}
#endif
