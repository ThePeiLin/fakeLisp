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
	FAKE_PUSH_PROC,
	FAKE_POP,
	FAKE_POP_VAR,
	FAKE_POP_CAR,
	FAKE_POP_CDR,
	FAKE_ADD,
	FAKE_SUB,
	FAKE_MUL,
	FAKE_DIV,
	FAKE_MOD,
	FAKE_ATOM,
	FAKE_NULL,
	FAKE_INIT_PROC,
	FAKE_SET_BOUND,
	FAKE_RES_BOUND,
	FAKE_EXIT,
	FAKE_OPEN,
	FAKE_CLOSE,
	FAKE_EQ,
	FAKE_NE,
	FAKE_GT,
	FAKE_LT,
	FAKE_GE,
	FAKE_LE,
	FAKE_LOG_NOT,
	FAKE_JMP_IF_TURE,
	FAKE_JMP_IF_FALSE,
	FAKE_JMP,
	FAKE_IN,
	FAKE_OUT,
	FAKE_GO,
	FAKE_RUN,
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
	{"push_proc",4},
	{"push_pop",0},
	{"push_pop_var",4},
	{"push_pop_car",0},
	{"push_pop_cdr",0},
	{"add",0}
};

#endif
