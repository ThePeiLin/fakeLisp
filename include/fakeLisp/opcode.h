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
	FKL_OP_PUSH_VAR,
	FKL_OP_PUSH_TOP,
	FKL_OP_PUSH_PROC,
	FKL_OP_POP,
	FKL_OP_POP_VAR,
	FKL_OP_POP_ARG,
	FKL_OP_POP_REST_ARG,
	FKL_OP_SET_TP,
	FKL_OP_SET_BP,
	FKL_OP_CALL,
	FKL_OP_RES_TP,
	FKL_OP_POP_TP,
	FKL_OP_RES_BP,
	FKL_OP_JMP_IF_TRUE,
	FKL_OP_JMP_IF_FALSE,
	FKL_OP_JMP,
	FKL_OP_PUSH_TRY,
	FKL_OP_POP_TRY,
	FKL_OP_LIST_APPEND,
	FKL_OP_PUSH_VECTOR,
	FKL_OP_PUSH_R_ENV,
	FKL_OP_POP_R_ENV,
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
	FKL_OP_IMPORT_WITH_SYMBOLS,
	FKL_OP_LAST_OPCODE,
}FklOpcode;

const char* fklGetOpcodeName(FklOpcode);
int fklGetOpcodeArgLen(FklOpcode);
FklOpcode fklFindOpcode(const char*);

#ifdef __cplusplus
}
#endif

#endif
