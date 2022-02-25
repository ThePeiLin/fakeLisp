#ifndef OPCODE_H
#define OPCODE_H
enum opcode
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
};

#ifdef USE_CODE_NAME

typedef struct
{
	char* codeName;
	int len;
}codeinfor;

static codeinfor codeName[]=
{
	{"dummy",0},
	{"push_nil",0},
	{"push_pair",0},
	{"push_i32",4},
	{"push_i64",8},
	{"push_chr",1},
	{"push_dbl",8},
	{"push_str",-1},
	{"push_sym",4},
	{"push_byte",-2},
	{"push_var",4},
//	{"push_env_var",0},
	{"push_top",0},
	{"push_proc",-2},
	{"push_fproc",4},
	{"push_ptr_ref",12},
	{"push_def_ref",12},
	{"push_ind_ref",8},
	{"push_ref",12},
	{"pop",0},
	{"pop_var",-3},
	{"pop_arg",4},
	{"pop_rest_arg",4},
	{"pop_car",0},
	{"pop_cdr",0},
	{"pop_ref",0},
//	{"pop_env",4},
//	{"swap",0},
	{"set_tp",0},
	{"set_bp",0},
	{"invoke",0},
	{"res_tp",0},
	{"pop_tp",0},
	{"res_bp",0},
	{"jmp_if_true",4},
	{"jmp_if_false",4},
	{"jmp",4},
	{"push_try",-4},
	{"pop_try",0},
	{"append",0},
};
#endif
#endif
