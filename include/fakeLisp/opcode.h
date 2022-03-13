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
	FKL_PUSH_CHR,
	FKL_PUSH_DBL,
	FKL_PUSH_STR,
	FKL_PUSH_SYM,
	FKL_PUSH_BYTE,
	FKL_PUSH_VAR,
//	FKL_PUSH_ENV_VAR,
	FKL_PUSH_TOP,
	FKL_PUSH_PROC,
	FKL_PUSH_FPROC,
	FKL_PUSH_PTR_REF,
	FKL_PUSH_DEF_REF,
	FKL_PUSH_IND_REF,
	FKL_PUSH_REF,
	FKL_POP,
	FKL_POP_VAR,
	FKL_POP_ARG,
	FKL_POP_REST_ARG,
	FKL_POP_CAR,
	FKL_POP_CDR,
	FKL_POP_REF,
//	FKL_POP_ENV,
//	FKL_SWAP,
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
}FklOpcode;

#define FKL_LAST FKL_APPEND
const char* fklGetOpcodeName(FklOpcode);
int fklGetOpcodeArgLen(FklOpcode);
FklOpcode fklFindOpcode(const char*);

#ifdef __cplusplus
}
#endif

#endif
