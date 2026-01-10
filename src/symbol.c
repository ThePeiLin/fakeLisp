#include <fakeLisp/base.h>
#include <fakeLisp/common.h>
#include <fakeLisp/symbol.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FKL_VM_USER_DATA_DEFAULT_PRINT(proto_print, "proto");

static inline FklVMvalueProto *as_proto(const FklVMvalue *v) {
    FKL_ASSERT(fklIsVMvalueProto(v));
    return FKL_TYPE_CAST(FklVMvalueProto *, v);
}

static void proto_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    const FklVMvalueProto *proto = as_proto(ud);
    FklVMvalue *const *cur = proto->vals;
    FklVMvalue *const *const end = &cur[proto->total_val_count];

    fklVMgcToGray(proto->name, gc);
    fklVMgcToGray(proto->file, gc);

    for (; cur < end; ++cur)
        fklVMgcToGray(*cur, gc);
}

static FklVMudMetaTable const ProtoUserDataMetaTable = {
    .size = sizeof(FklVMvalueProto),
    .princ = proto_print,
    .prin1 = proto_print,
    .atomic = proto_atomic,
};

int fklIsVMvalueProto(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->mt_ == &ProtoUserDataMetaTable;
}

FklVMvalueProto *fklCreateVMvalueProto(FklVM *exe, uint32_t val_count) {
    size_t extra_size = val_count * sizeof(FklVMvalue *);

    FklVMvalueProto *proto = (FklVMvalueProto *)fklCreateVMvalueUd2(exe, //
            &ProtoUserDataMetaTable,
            extra_size,
            NULL);

    proto->total_val_count = val_count;

    return proto;
}
