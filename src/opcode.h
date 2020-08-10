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
	FAKE_PUSH_STACK,
	FAKE_PUSH_STATIC,
	FAKE_PUSH_CAR,
	FAKE_PUSH_CDR,
	FAKE_POP,
	FAKE_POP_STATIC,
	FAKE_POP_NIL_STATIC,
	FAKE_POP_STACK,
	FAKE_POP_NIL_STACK,
	FAKE_POP_CAR,
	FAKE_POP_CDR,
	FAKE_ADD,
	FAKE_SUB,
	FAKE_MUL,
	FAKE_DIV,
	FAKE_MOD,
	FAKE_ATOM,
	FAKE_NULL,
	FAKE_INIT_ENV,
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
	FAKE_LOG_AND,
	FAKE_JMP_IF_TURE,
	FAKE_JMP_IF_FALSE,
	FAKE_GOTO,
	FAKE_IN,
	FAKE_OUT,
	FAKE_GO,
	FAKE_RUN,
	FAKE_SEND,
	FAKE_ACCEPT
};
#endif
