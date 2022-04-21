#include<fakeLisp/opcode.h>
#include<string.h>
static struct
{
	char* name;
	int len;
}opcodeInfor[]=
{
	{"dummy",0},
	{"push_nil",0},
	{"push_pair",0},
	{"push_i32",4},
	{"push_i64",8},
	{"push_char",1},
	{"push_f64",8},
	{"push_str",-1},
	{"push_sym",8},
	{"push_var",8},
	{"push_top",0},
	{"push_proc",-2},
	{"pop",0},
	{"pop_var",-3},
	{"pop_arg",8},
	{"pop_rest_arg",8},
	{"pop_car",0},
	{"pop_cdr",0},
	{"pop_ref",0},
	{"set_tp",0},
	{"set_bp",0},
	{"invoke",0},
	{"res_tp",0},
	{"pop_tp",0},
	{"res_bp",0},
	{"jmp_if_true",8},
	{"jmp_if_false",8},
	{"jmp",8},
	{"push_try",-4},
	{"pop_try",0},
	{"append",0},
	{"push_vector",8},
	{"push_r_env",0},
	{"pop_r_env",0},
};

const char* fklGetOpcodeName(FklOpcode opcode)
{
	return opcodeInfor[opcode].name;
}

int fklGetOpcodeArgLen(FklOpcode opcode)
{
	return opcodeInfor[opcode].len;
}

FklOpcode fklFindOpcode(const char* str)
{
	FklOpcode i=0;
	for(;i<FKL_LAST_OPCODE;i++)
		if(!strcmp(opcodeInfor[i].name,str))
			return i;
	return 0;
}
