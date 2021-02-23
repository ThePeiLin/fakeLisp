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
	FAKE_PUSH_CHAN,
	FAKE_PUSH_VAR,
	FAKE_PUSH_CAR,
	FAKE_PUSH_CDR,
	FAKE_PUSH_TOP,
	FAKE_PUSH_PROC,
	FAKE_PUSH_MOD_PROC,
	FAKE_PUSH_LIST_ARG,
	FAKE_POP,
	FAKE_POP_VAR,
	FAKE_POP_REST_VAR,
	FAKE_POP_CAR,
	FAKE_POP_CDR,
	FAKE_POP_REF,
	FAKE_PACK_CC,
	FAKE_CALL_PROC,
	FAKE_END_PROC,
	FAKE_SET_TP,
	FAKE_SET_BP,
	FAKE_INVOKE,
	FAKE_RES_TP,
	FAKE_POP_TP,
	FAKE_RES_BP,
	FAKE_JMP_IF_TURE,
	FAKE_JMP_IF_FALSE,
	FAKE_JMP,
	FAKE_ADD,
	FAKE_SUB,
	FAKE_MUL,
	FAKE_DIV,
	FAKE_REM,
	FAKE_ATOM,
	FAKE_NULL,
	FAKE_TYPE_OF,
	FAKE_CAST_TO_CHR,
	FAKE_CAST_TO_INT,
	FAKE_CAST_TO_DBL,
	FAKE_CAST_TO_STR,
	FAKE_CAST_TO_SYM,
	FAKE_CAST_TO_BYTE,
	FAKE_NTH,
	FAKE_LENGTH,
	FAKE_APPD,
	FAKE_OPEN,
	FAKE_CLOSE,
	FAKE_EQ,
	FAKE_EQN,
	FAKE_EQUAL,
	FAKE_GT,
	FAKE_GE,
	FAKE_LT,
	FAKE_LE,
	FAKE_NOT,
	FAKE_READ,
	FAKE_READB,
	FAKE_WRITE,
	FAKE_WRITEB,
	FAKE_PRINC,
	FAKE_GO,
	FAKE_SEND,
	FAKE_RECV,
};

typedef struct
{
	char* codeName;
	int len;
}codeinfor;

#ifdef USE_CODE_NAME
static codeinfor codeName[]=
{
	{"dummy",0},
	{"push_nil",0},
	{"push_pair",0},
	{"push_int",4},
	{"push_chr",1},
	{"push_dbl",8},
	{"push_str",-1},
	{"push_sym",-1},
	{"push_byte",-2},
	{"push_chan",4},
	{"push_var",4},
	{"push_car",0},
	{"push_cdr",0},
	{"push_top",0},
	{"push_proc",4},
	{"push_mod_proc",-1},
	{"push_list_arg",0},
	{"pop",0},
	{"pop_var",-3},
	{"pop_rest_var",-3},
	{"pop_car",0},
	{"pop_cdr",0},
	{"pop_ref",0},
	{"pack_cc",0},
	{"call_proc",-1},
	{"end_proc",0},
	{"set_tp",0},
	{"set_bp",0},
	{"invoke",0},
	{"res_tp",0},
	{"pop_tp",0},
	{"res_bp",0},
	{"jmp_if_ture",4},
	{"jmp_if_false",4},
	{"jmp",4},
	{"add",0},
	{"sub",0},
	{"mul",0},
	{"div",0},
	{"rem",0},
	{"atom",0},
	{"null",0},
	{"type_of",0},
	{"cast_to_chr",0},
	{"cast_to_int",0},
	{"cast_to_dbl",0},
	{"cast_to_str",0},
	{"cast_to_sym",0},
	{"cast_to_byte",0},
	{"nth",0},
	{"length",0},
	{"appd",0},
	{"open",0},
	{"close",0},
	{"eq",0},
	{"eqn",0},
	{"equal",0},
	{"gt",0},
	{"ge",0},
	{"lt",0},
	{"le",0},
	{"not",0},
	{"read",0},
	{"readb",0},
	{"write",0},
	{"writeb",0},
	{"princ",0},
	{"go",0},
	{"send",0},
	{"recv",0},
};
#endif
#endif
