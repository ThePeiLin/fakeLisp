#ifndef FKL_OPCODE_H
#define FKL_OPCODE_H

#ifdef __cplusplus
extern "C"{
#endif

typedef enum
{
	FKL_OP_PUSH_NIL=1,
	FKL_OP_PUSH_PAIR,
	FKL_OP_PUSH_I32,
	FKL_OP_PUSH_I64,
	FKL_OP_PUSH_CHAR,
	FKL_OP_PUSH_F64,
	FKL_OP_PUSH_STR,
	FKL_OP_PUSH_SYM,
	FKL_OP_DUP,
	FKL_OP_PUSH_PROC,
	FKL_OP_DROP,
	FKL_OP_POP_ARG,
	FKL_OP_POP_REST_ARG,
	FKL_OP_SET_BP,
	FKL_OP_CALL,
	FKL_OP_RES_BP,
	FKL_OP_JMP_IF_TRUE,
	FKL_OP_JMP_IF_FALSE,
	FKL_OP_JMP,
	FKL_OP_LIST_APPEND,
	FKL_OP_PUSH_VECTOR,
	FKL_OP_TAIL_CALL,
	FKL_OP_PUSH_BIG_INT,
	FKL_OP_PUSH_BOX,
	FKL_OP_PUSH_BYTEVECTOR,
	FKL_OP_PUSH_HASHTABLE_EQ,
	FKL_OP_PUSH_HASHTABLE_EQV,
	FKL_OP_PUSH_HASHTABLE_EQUAL,
	FKL_OP_PUSH_LIST_0,
	FKL_OP_PUSH_LIST,
	FKL_OP_PUSH_VECTOR_0,
	FKL_OP_LIST_PUSH,
	FKL_OP_IMPORT,
	FKL_OP_PUSH_0,
	FKL_OP_PUSH_1,
	FKL_OP_PUSH_I8,
	FKL_OP_PUSH_I16,
	FKL_OP_PUSH_I64_BIG,
	FKL_OP_GET_LOC,
	FKL_OP_PUT_LOC,
	FKL_OP_GET_VAR_REF,
	FKL_OP_PUT_VAR_REF,
	FKL_OP_EXPORT,
	FKL_OP_LOAD_LIB,
	FKL_OP_LOAD_DLL,
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
	FKL_OP_LAST_OPCODE,
}FklOpcode;

const char* fklGetOpcodeName(FklOpcode);
int fklGetOpcodeArgLen(FklOpcode);
FklOpcode fklFindOpcode(const char*);

#ifdef __cplusplus
}
#endif

#endif
