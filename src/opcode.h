#ifndef OPCODE_H
#define OPCODE_H
enum opcode
{
	FAKE_PUSH_0=1,
	FAKE_PUSH_NUM,
	FAKE_PUSH_STR,
	FAKE_PUSH_SYM,
	FAKE_PUSH_NUM_STACK,
	FAKE_PUSH_STR_STACK,
	FAKE_PUSH_SYM_STACK,
	FAKE_PUSH_NUM_STATIC,
	FAKE_PUSH_STR_STATIC,
	FAKE_PUSH_SYM_STATIC,
	FAKE_POP_NUM,
	FAKE_POP_STR,
	FAKE_POP_SYM,
	FAKE_POP_NUM_STATIC,
	FAKE_POP_STR_STATIC,
	FAKE_POP_SYM_STATIC,
	FAKE_POP_NUM_STACK,
	FAKE_POP_STR_STACK,
	FAKE_POP_SYM_STACK,
	FAKE_PUSH_NIL,
	FAKE_PUSH_NIL_STACK,
	FAKE_PUSH_NIL_STATIC,
	FAKE_POP_NIL,
	FAKE_POP_NIL_STATIC,
	FAKE_POP_NIL_STACK,
	FAKE_PUSH_LIST,
	FAKE_PUSH_LIST_STACK,
	FAKE_PUSH_LIST_STATIC,
	FAKE_POP_LIST,
	FAKE_POP_LIST_STATIC,
	FAKE_POP_LIST_STACK,
	FAKE_PUSH_CELL,
	FAKE_POP_CELL,
	FAKE_POP_CELL_STACK,
	FAKE_PUSH_FUNCTION,
	FAKE_POP_FUNCTION,
	FAKE_CAST_NUM_STR,
	FAKE_CAST_STR_NUM,
	FAKE_ADD_NUM,
	FAKE_ADD_STR,
	FAKE_SUB,
	FAKE_MUL,
	FAKE_DIV,
	FAKE_MOD,
	FAKE_CONS,
	FAKE_CAR,
	FAKE_CDR,
	FAKE_LAMBDA,
	FAKE_ATOM,
	FAKE_NULL,
	FAKE_INVOLVE,
	FAKE_RET,
	FAKE_EXIT,
	FAKE_OPEN,
	FAKE_CLOSE,
	FAKE_ZIP,
	FAKE_EXPAND,
	FAKE_EQ_NUM,
	FAKE_EQ_STR,
	FAKE_EQ_CELL,
	FAKE_NE_NUM,
	FAKE_NE_STR,
	FAKE_GT_NUM,
	FAKE_LT_NUM,
	FAKE_GE_NUM,
	FAKE_LE_NUM,
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
