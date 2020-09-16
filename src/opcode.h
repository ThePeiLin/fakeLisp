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
	FAKE_PUSH_VAR,
	FAKE_PUSH_CAR,
	FAKE_PUSH_CDR,
	FAKE_PUSH_TOP,
	FAKE_PUSH_PROC,
	FAKE_POP,
	FAKE_POP_VAR,
	FAKE_POP_REST_VAR,
	FAKE_POP_CAR,
	FAKE_POP_CDR,
	FAKE_INIT_PROC,
	FAKE_END_PROC,
	FAKE_SET_BP,
	FAKE_INVOKE,
	FAKE_RES_BP,
	FAKE_JMP_IF_TURE,
	FAKE_JMP_IF_FALSE,
	FAKE_JMP,
	FAKE_ADD,
	FAKE_SUB,
	FAKE_MUL,
	FAKE_DIV,
	FAKE_MOD,
	FAKE_ATOM,
	FAKE_NULL,
	FAKE_CAST_TO_CHR,
	FAKE_CAST_TO_INT,
	FAKE_CAST_TO_DBL,
	FAKE_CAST_TO_STR,
	FAKE_CAST_TO_SYM,
	FAKE_IS_CHR,
	FAKE_IS_INT,
	FAKE_IS_DBL,
	FAKE_IS_STR,
	FAKE_IS_SYM,
	FAKE_IS_PRC,
	FAKE_GET_CHR_STR,
	FAKE_STR_LEN,
	FAKE_STR_CAT,
	FAKE_OPEN,
	FAKE_CLOSE,
	FAKE_EQ,
	FAKE_GT,
	FAKE_GE,
	FAKE_LT,
	FAKE_LE,
	FAKE_NOT,
	FAKE_GETC,
	FAKE_UNGETC,
	FAKE_WRITE,
	FAKE_TELL,
	FAKE_SEEK,
	FAKE_REWIND,
	FAKE_GO,
	FAKE_WAIT,
	FAKE_SEND,
	FAKE_ACCEPT
};

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
	{"push_sym",-1},
	{"push_var",4},
	{"push_car",0},
	{"push_cdr",0},
	{"push_top",0},
	{"push_proc",4},
	{"pop",0},
	{"pop_var",4},
	{"pop_rest_var",4},
	{"pop_car",0},
	{"pop_cdr",0},
	{"init_proc",4},
	{"end_proc",0},
	{"set_bp",0},
	{"invoke",0},
	{"res_bp",0},
	{"jmp_if_ture",4},
	{"jmp_if_false",4},
	{"jmp",4},
	{"add",0},
	{"sub",0},
	{"mul",0},
	{"div",0},
	{"mod",0},
	{"atom",0},
	{"null",0},
	{"cast_to_chr",0},
	{"cast_to_int",0},
	{"cast_to_dbl",0},
	{"cast_to_str",0},
	{"cast_to_sym",0},
	{"is_chr",0},
	{"is_int",0},
	{"is_dbl",0},
	{"is_str",0},
	{"is_sym",0},
	{"is_prc",0},
	{"get_chr_str",0},
	{"str_len",0},
	{"str_cat",0},
	{"open",0},
	{"close",0},
	{"eq",0},
	{"gt",0},
	{"ge",0},
	{"lt",0},
	{"le",0},
	{"not",0},
	{"getc",0},
	{"ungetc",0},
	{"write",0},
	{"tell",0},
	{"seek",0},
	{"rewind",0},
	{"go",0},
	{"wait",0},
	{"send",0},
	{"accept",0}
};

#endif
