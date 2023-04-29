#include<fakeLisp/opcode.h>
#include<string.h>
static struct
{
	char* name;
	int len;
}opcodeInfor[]=
{
	{"dummy",                0,  },
	{"push-nil",             0,  },
	{"push-pair",            0,  },
	{"push-i32",             4,  },
	{"push-i64",             8,  },
	{"push-char",            1,  },
	{"push-f64",             8,  },
	{"push-str",             -1, },
	{"push-sym",             8,  },
	{"dup",                  0,  },
	{"push-proc",            -1, },
	{"drop",                 0,  },
	{"pop-arg",              4,  },
	{"pop-rest-arg",         4,  },
	{"set-bp",               0,  },
	{"call",                 0,  },
	{"res-bp",               0,  },
	{"jmp-if-true",          8,  },
	{"jmp-if-false",         8,  },
	{"jmp",                  8,  },
	{"list-append",          0,  },
	{"push-vector",          8,  },
	{"tail-call",            0,  },
	{"push-big-int",         -1, },
	{"push-box",             0,  },
	{"push-bytevector",      -1, },
	{"push-hashtable-eq",    8,  },
	{"push-hashtable-eqv",   8,  },
	{"push-hashtable-equal", 8,  },
	{"push-list-0",          0,  },
	{"push-list",            8,  },
	{"push-vector-0",        0,  },
	{"list-push",            0,  },
	{"import",               8,  },
	{"push-0",               0,  },
	{"push-1",               0,  },
	{"push-i8",              1,  },
	{"push-i16",             2,  },
	{"push-i64-big",         8,  },
	{"get-loc",              4,  },
	{"put-loc",              4,  },
	{"get-var-ref",          4,  },
	{"put-var-ref",          4,  },
	{"export",               4,  },
	{"load-lib",             4,  },
	{"load-dll",             4,  },
	{"true",                 0,  },
	{"not",                  0,  },
	{"eq",                   0,  },
	{"eqv",                  0,  },
	{"equal",                0,  },

	{"eqn",                  0,  },
	{"eqn3",                 0,  },

	{"gt",                   0,  },
	{"gt3",                  0,  },

	{"lt",                   0,  },
	{"lt3",                  0,  },

	{"ge",                   0,  },
	{"ge3",                  0,  },

	{"le",                   0,  },
	{"le3",                  0,  },

	{"inc",                  0,  },
	{"dec",                  0,  },
	{"add",                  0,  },
	{"sub",                  0,  },
	{"mul",                  0,  },
	{"div",                  0,  },
	{"idiv",                 0,  },
	{"mod",                  0,  },
	{"add1",                 0,  },
	{"mul1",                 0,  },
	{"neg",                  0,  },
	{"rec",                  0,  },
	{"add3",                 0,  },
	{"sub3",                 0,  },
	{"mul3",                 0,  },
	{"div3",                 0,  },
	{"idiv3",                0,  },

	{"push-car",             0,  },
	{"push-cdr",             0,  },
	{"cons",                 0,  },
	{"nth",                  0,  },
	{"vec-ref",              0,  },
	{"str-ref",              0,  },
	{"box",                  0,  },
	{"unbox",                0,  },
	{"box0",                 0,  },

	//{"close-put-loc",        4,  },
	//{"close-pop-arg",        4,  },
	//{"close-import",         8,  },

	{"close-ref",            8,  },
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
	for(FklOpcode i=0;i<FKL_OP_LAST_OPCODE;i++)
		if(!strcmp(opcodeInfor[i].name,str))
			return i;
	return 0;
}
