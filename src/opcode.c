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
	{"push-var",             8,  },
	{"push-top",             0,  },
	{"push-proc",            -2, },
	{"pop",                  0,  },
	{"pop-var",              -3, },
	{"pop-arg",              8,  },
	{"pop-rest-arg",         8,  },
	{"set-tp",               0,  },
	{"set-bp",               0,  },
	{"call",                 0,  },
	{"res-tp",               0,  },
	{"pop-tp",               0,  },
	{"res-bp",               0,  },
	{"jmp-if-true",          8,  },
	{"jmp-if-false",         8,  },
	{"jmp",                  8,  },
	{"push-try",             -4, },
	{"pop-try",              0,  },
	{"list-append",          0,  },
	{"push-vector",          8,  },
	{"push-r-env",           0,  },
	{"pop-r-env",            0,  },
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
