#ifndef FKL_OPCODE_H
#define FKL_OPCODE_H

#ifdef __cplusplus
extern "C"{
#endif

typedef enum
{
	FKL_OP_DUMMY=0,
	FKL_OP_PUSH_NIL,
	FKL_OP_PUSH_PAIR,
	FKL_OP_PUSH_I24,
	FKL_OP_PUSH_I64F,
	FKL_OP_PUSH_I64F_C,
	FKL_OP_PUSH_I64F_X,
	FKL_OP_PUSH_CHAR,
	FKL_OP_PUSH_F64,
	FKL_OP_PUSH_F64_C,
	FKL_OP_PUSH_F64_X,
	FKL_OP_PUSH_STR,
	FKL_OP_PUSH_STR_C,
	FKL_OP_PUSH_STR_X,
	FKL_OP_PUSH_SYM,
	FKL_OP_PUSH_SYM_C,
	FKL_OP_PUSH_SYM_X,
	FKL_OP_DUP,
	FKL_OP_PUSH_PROC,
	FKL_OP_PUSH_PROC_X,
	FKL_OP_PUSH_PROC_XX,
	FKL_OP_PUSH_PROC_XXX,
	FKL_OP_DROP,
	FKL_OP_POP_ARG,
	FKL_OP_POP_ARG_C,
	FKL_OP_POP_ARG_X,
	FKL_OP_POP_REST_ARG,
	FKL_OP_POP_REST_ARG_C,
	FKL_OP_POP_REST_ARG_X,
	FKL_OP_SET_BP,
	FKL_OP_CALL,
	FKL_OP_RES_BP,
	FKL_OP_JMP_IF_TRUE,
	FKL_OP_JMP_IF_TRUE_C,
	FKL_OP_JMP_IF_TRUE_X,
	FKL_OP_JMP_IF_TRUE_XX,
	FKL_OP_JMP_IF_FALSE,
	FKL_OP_JMP_IF_FALSE_C,
	FKL_OP_JMP_IF_FALSE_X,
	FKL_OP_JMP_IF_FALSE_XX,
	FKL_OP_JMP,
	FKL_OP_JMP_C,
	FKL_OP_JMP_X,
	FKL_OP_JMP_XX,
	FKL_OP_LIST_APPEND,
	FKL_OP_PUSH_VEC,
	FKL_OP_PUSH_VEC_C,
	FKL_OP_PUSH_VEC_X,
	FKL_OP_PUSH_VEC_XX,
	FKL_OP_TAIL_CALL,
	FKL_OP_PUSH_BI,
	FKL_OP_PUSH_BI_C,
	FKL_OP_PUSH_BI_X,
	FKL_OP_PUSH_BOX,
	FKL_OP_PUSH_BVEC,
	FKL_OP_PUSH_BVEC_C,
	FKL_OP_PUSH_BVEC_X,
	FKL_OP_PUSH_HASHEQ,
	FKL_OP_PUSH_HASHEQ_C,
	FKL_OP_PUSH_HASHEQ_X,
	FKL_OP_PUSH_HASHEQ_XX,
	FKL_OP_PUSH_HASHEQV,
	FKL_OP_PUSH_HASHEQV_C,
	FKL_OP_PUSH_HASHEQV_X,
	FKL_OP_PUSH_HASHEQV_XX,
	FKL_OP_PUSH_HASHEQUAL,
	FKL_OP_PUSH_HASHEQUAL_C,
	FKL_OP_PUSH_HASHEQUAL_X,
	FKL_OP_PUSH_HASHEQUAL_XX,
	FKL_OP_PUSH_LIST_0,
	FKL_OP_PUSH_LIST,
	FKL_OP_PUSH_LIST_C,
	FKL_OP_PUSH_LIST_X,
	FKL_OP_PUSH_LIST_XX,
	FKL_OP_PUSH_VEC_0,
	FKL_OP_LIST_PUSH,
	FKL_OP_IMPORT,
	FKL_OP_IMPORT_X,
	FKL_OP_IMPORT_XX,
	FKL_OP_PUSH_0,
	FKL_OP_PUSH_1,
	FKL_OP_PUSH_I8,
	FKL_OP_PUSH_I16,
	FKL_OP_PUSH_I64B,
	FKL_OP_PUSH_I64B_C,
	FKL_OP_PUSH_I64B_X,
	FKL_OP_GET_LOC,
	FKL_OP_GET_LOC_C,
	FKL_OP_GET_LOC_X,
	FKL_OP_PUT_LOC,
	FKL_OP_PUT_LOC_C,
	FKL_OP_PUT_LOC_X,
	FKL_OP_GET_VAR_REF,
	FKL_OP_GET_VAR_REF_C,
	FKL_OP_GET_VAR_REF_X,
	FKL_OP_PUT_VAR_REF,
	FKL_OP_PUT_VAR_REF_C,
	FKL_OP_PUT_VAR_REF_X,
	FKL_OP_EXPORT,
	FKL_OP_EXPORT_C,
	FKL_OP_EXPORT_X,
	FKL_OP_LOAD_LIB,
	FKL_OP_LOAD_LIB_C,
	FKL_OP_LOAD_LIB_X,
	FKL_OP_LOAD_DLL,
	FKL_OP_LOAD_DLL_C,
	FKL_OP_LOAD_DLL_X,
	FKL_OP_TRUE,
	FKL_OP_NOT,
	FKL_OP_EQ,
	FKL_OP_EQV,
	FKL_OP_EQUAL,

	FKL_OP_EQN,
	FKL_OP_EQN3,

	FKL_OP_GT,
	FKL_OP_GT3,

	FKL_OP_LT,
	FKL_OP_LT3,

	FKL_OP_GE,
	FKL_OP_GE3,

	FKL_OP_LE,
	FKL_OP_LE3,

	FKL_OP_INC,
	FKL_OP_DEC,
	FKL_OP_ADD,
	FKL_OP_SUB,
	FKL_OP_MUL,
	FKL_OP_DIV,
	FKL_OP_IDIV,
	FKL_OP_MOD,
	FKL_OP_ADD1,
	FKL_OP_MUL1,
	FKL_OP_NEG,
	FKL_OP_REC,
	FKL_OP_ADD3,
	FKL_OP_SUB3,
	FKL_OP_MUL3,
	FKL_OP_DIV3,
	FKL_OP_IDIV3,
	FKL_OP_PUSH_CAR,
	FKL_OP_PUSH_CDR,
	FKL_OP_CONS,
	FKL_OP_NTH,
	FKL_OP_VEC_REF,
	FKL_OP_STR_REF,
	FKL_OP_BOX,
	FKL_OP_UNBOX,
	FKL_OP_BOX0,

	FKL_OP_CLOSE_REF,
	FKL_OP_CLOSE_REF_X,
	FKL_OP_CLOSE_REF_XX,

	FKL_OP_RET,
	FKL_OP_EXPORT_TO,
	FKL_OP_EXPORT_TO_X,
	FKL_OP_EXPORT_TO_XX,
	FKL_OP_RES_BP_TP,

	FKL_OP_ATOM,

	FKL_OP_EXTRA_ARG,

	FKL_OP_PUSH_DVEC,
	FKL_OP_PUSH_DVEC_C,
	FKL_OP_PUSH_DVEC_X,
	FKL_OP_PUSH_DVEC_XX,
	FKL_OP_PUSH_DVEC_0,
	FKL_OP_DVEC_REF,
	FKL_OP_DVEC_FIRST,
	FKL_OP_DVEC_LAST,
	FKL_OP_VEC_FIRST,
	FKL_OP_VEC_LAST,

	FKL_OP_POP_LOC,
	FKL_OP_POP_LOC_C,
	FKL_OP_POP_LOC_X,

	FKL_OP_CAR_SET,
	FKL_OP_CDR_SET,
	FKL_OP_BOX_SET,

	FKL_OP_VEC_SET,
	FKL_OP_DVEC_SET,

	FKL_OP_HASH_REF_2,
	FKL_OP_HASH_REF_3,
	FKL_OP_HASH_SET,

	FKL_OP_INC_LOC,
	FKL_OP_INC_LOC_C,
	FKL_OP_INC_LOC_X,

	FKL_OP_DEC_LOC,
	FKL_OP_DEC_LOC_C,
	FKL_OP_DEC_LOC_X,

	FKL_OP_LAST_OPCODE,
}FklOpcode;

// Op(8)  |  A(8)  |  B(16)
// Op(8)  |      C(24)

#define FKL_MAX_INS_LEN (4)
typedef enum
{
	FKL_OP_MODE_I=0,
	FKL_OP_MODE_IsA,
	FKL_OP_MODE_IuB,
	FKL_OP_MODE_IsB,
	FKL_OP_MODE_IuC,
	FKL_OP_MODE_IsC,
	FKL_OP_MODE_IuBB,
	FKL_OP_MODE_IsBB,
	FKL_OP_MODE_IuCCB,
	FKL_OP_MODE_IsCCB,

	FKL_OP_MODE_IuAuB,
	FKL_OP_MODE_IuCuC,
	FKL_OP_MODE_IuCAuBB,
	FKL_OP_MODE_IuCAuBCC,

	FKL_OP_MODE_IxAxB,
}FklOpcodeMode;

#define FKL_GET_INS_UC(ins) (((((uint32_t)(ins)->bu)<<FKL_BYTE_WIDTH))|(ins)->au)
#define FKL_GET_INS_IC(ins) (((int32_t)FKL_GET_INS_UC(ins))-FKL_I24_OFFSET)
#define FKL_GET_INS_UX(ins) ((ins)->bu|(((uint32_t)(ins)[1].bu)<<FKL_I16_WIDTH))
#define FKL_GET_INS_IX(ins) ((int32_t)FKL_GET_INS_UX(ins))
#define FKL_GET_INS_UXX(ins) (FKL_GET_INS_UC(ins)\
		|(((uint64_t)FKL_GET_INS_UC((ins)+1))<<FKL_I24_WIDTH)\
		|(((uint64_t)ins[2].bu)<<(FKL_I24_WIDTH*2)))

#define FKL_GET_INS_IXX(ins) ((int64_t)FKL_GET_INS_UXX(ins))

const char* fklGetOpcodeName(FklOpcode);
FklOpcodeMode fklGetOpcodeMode(FklOpcode);
FklOpcode fklFindOpcode(const char*);
int fklGetOpcodeModeLen(FklOpcode);

#ifdef __cplusplus
}
#endif

#endif
