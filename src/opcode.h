#ifndef OPCODE_H
#define OPCODE_H
enum opcode
{
	FAKE_PUSH_NIL=1,
	FAKE_PUSH_PAIR,//it push an empty pair,
	FAKE_PUSH_INT,
	FAKE_PUSH_CHR,
	FAKE_PUSH_DBL,
	FAKE_PUSH_STR,
	FAKE_PUSH_SYM,
	FAKE_PUSH_BYTE,
	FAKE_PUSH_VAR,
	FAKE_PUSH_ENV_VAR,
	FAKE_PUSH_TOP,
	FAKE_PUSH_PROC,
	FAKE_POP,
	FAKE_POP_VAR,
	FAKE_POP_ARG,
	FAKE_POP_REST_ARG,
	FAKE_POP_CAR,
	FAKE_POP_CDR,
	FAKE_POP_REF,
	FAKE_POP_ENV,
	FAKE_SWAP,
	FAKE_SET_TP,
	FAKE_SET_BP,
	FAKE_INVOKE,
	FAKE_RES_TP,
	FAKE_POP_TP,
	FAKE_RES_BP,
	FAKE_JMP_IF_TRUE,
	FAKE_JMP_IF_FALSE,
	FAKE_JMP,
	FAKE_PUSH_TRY,
	FAKE_POP_TRY,
	FAKE_APPEND,
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
	{"push_int",4},
	{"push_chr",1},
	{"push_dbl",8},
	{"push_str",-1},
	{"push_sym",4},
	{"push_byte",-2},
	{"push_var",4},
	{"push_env_var",0},
	{"push_top",0},
	{"push_proc",-2},
	{"pop",0},
	{"pop_var",-3},
	{"pop_arg",4},
	{"pop_rest_arg",4},
	{"pop_car",0},
	{"pop_cdr",0},
	{"pop_ref",0},
	{"pop_env",4},
	{"swap",0},
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
