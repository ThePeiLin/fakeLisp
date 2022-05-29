#ifndef FKL_OPCODE_H
#define FKL_OPCODE_H

#ifdef __cplusplus
extern "C"{
#endif

typedef enum
{
	FKL_PUSH_NIL=1,
	FKL_PUSH_PAIR,//it push an empty pair,
	FKL_PUSH_I32,
	FKL_PUSH_I64,
	FKL_PUSH_CHAR,
	FKL_PUSH_F64,
	FKL_PUSH_STR,
	FKL_PUSH_SYM,
	FKL_PUSH_VAR,
	FKL_PUSH_TOP,
	FKL_PUSH_PROC,
	FKL_POP,
	FKL_POP_VAR,
	FKL_POP_ARG,
	FKL_POP_REST_ARG,
	FKL_POP_CAR,
	FKL_POP_CDR,
	//FKL_POP_REF,
	FKL_SET_TP,
	FKL_SET_BP,
	FKL_INVOKE,
	FKL_RES_TP,
	FKL_POP_TP,
	FKL_RES_BP,
	FKL_JMP_IF_TRUE,
	FKL_JMP_IF_FALSE,
	FKL_JMP,
	FKL_PUSH_TRY,
	FKL_POP_TRY,
	FKL_APPEND,
	FKL_PUSH_VECTOR,
	FKL_PUSH_R_ENV,
	FKL_POP_R_ENV,
	FKL_TAIL_INVOKE,
	FKL_PUSH_BIG_INT,
	FKL_PUSH_BOX,
}FklOpcode;

#define FKL_LAST_OPCODE FKL_PUSH_BOX
const char* fklGetOpcodeName(FklOpcode);
int fklGetOpcodeArgLen(FklOpcode);
FklOpcode fklFindOpcode(const char*);

#ifdef __cplusplus
}
#endif

#endif
