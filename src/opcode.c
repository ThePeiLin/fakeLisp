#include<fakeLisp/opcode.h>
#include<string.h>

static struct
{
	char* name;
	FklOpcodeMode mode;
}opcodeInfo[FKL_OP_LAST_OPCODE]=
{
	{"dummy",             FKL_OP_MODE_I,        },
	{"push-nil",          FKL_OP_MODE_I,        },
	{"push-pair",         FKL_OP_MODE_I,        },
	{"push-i24",          FKL_OP_MODE_IsC,      },
	{"push-i64f",         FKL_OP_MODE_IuB,      },
	{"push-i64f-c",       FKL_OP_MODE_IuC,      },
	{"push-i64f-x",       FKL_OP_MODE_IuBB,     },
	{"push-char",         FKL_OP_MODE_IuB,      },
	{"push-f64",          FKL_OP_MODE_IuB,      },
	{"push-f64-c",        FKL_OP_MODE_IuC,      },
	{"push-f64-x",        FKL_OP_MODE_IuBB,     },
	{"push-str",          FKL_OP_MODE_IuB,      },
	{"push-str-c",        FKL_OP_MODE_IuC,      },
	{"push-str-x",        FKL_OP_MODE_IuBB,     },
	{"push-sym",          FKL_OP_MODE_IuB,      },
	{"push-sym-c",        FKL_OP_MODE_IuC,      },
	{"push-sym-x",        FKL_OP_MODE_IuBB,     },
	{"dup",               FKL_OP_MODE_I,        },
	{"push-proc",         FKL_OP_MODE_IuAuB,    },
	{"push-proc-x",       FKL_OP_MODE_IuCuC,    },
	{"push-proc-xx",      FKL_OP_MODE_IuCAuBB,  },
	{"push-proc-xxx",     FKL_OP_MODE_IuCAuBCC, },
	{"drop",              FKL_OP_MODE_I,        },
	{"pop-arg",           FKL_OP_MODE_IuB,      },
	{"pop-arg-c",         FKL_OP_MODE_IuC,      },
	{"pop-arg-x",         FKL_OP_MODE_IuBB,     },
	{"pop-rest-arg",      FKL_OP_MODE_IuB,      },
	{"pop-rest-arg-c",    FKL_OP_MODE_IuC,      },
	{"pop-rest-arg-x",    FKL_OP_MODE_IuBB,     },
	{"set-bp",            FKL_OP_MODE_I,        },
	{"call",              FKL_OP_MODE_I,        },
	{"res-bp",            FKL_OP_MODE_I,        },
	{"jmp-if-true",       FKL_OP_MODE_IsB,      },
	{"jmp-if-true-c",     FKL_OP_MODE_IsC,      },
	{"jmp-if-true-x",     FKL_OP_MODE_IsBB,     },
	{"jmp-if-true-xx",    FKL_OP_MODE_IsCCB,    },
	{"jmp-if-false",      FKL_OP_MODE_IsB,      },
	{"jmp-if-false-c",    FKL_OP_MODE_IsC,      },
	{"jmp-if-false-x",    FKL_OP_MODE_IsBB,     },
	{"jmp-if-false-xx",   FKL_OP_MODE_IsCCB,    },
	{"jmp",               FKL_OP_MODE_IsB,      },
	{"jmp-c",             FKL_OP_MODE_IsC,      },
	{"jmp-x",             FKL_OP_MODE_IsBB,     },
	{"jmp-xx",            FKL_OP_MODE_IsCCB,    },
	{"list-append",       FKL_OP_MODE_I,        },
	{"push-vec",          FKL_OP_MODE_IuB,      },
	{"push-vec-c",        FKL_OP_MODE_IuC,      },
	{"push-vec-x",        FKL_OP_MODE_IuBB,     },
	{"push-vec-xx",       FKL_OP_MODE_IuCCB,    },
	{"tail-call",         FKL_OP_MODE_I,        },
	{"push-bi",           FKL_OP_MODE_IuB,      },
	{"push-bi-c",         FKL_OP_MODE_IuC,      },
	{"push-bi-x",         FKL_OP_MODE_IuBB,     },
	{"push-box",          FKL_OP_MODE_I,        },
	{"push-bvec",         FKL_OP_MODE_IuB,      },
	{"push-bvec-c",       FKL_OP_MODE_IuC,      },
	{"push-bvec-x",       FKL_OP_MODE_IuBB,     },
	{"push-hasheq",       FKL_OP_MODE_IuB,      },
	{"push-hasheq-c",     FKL_OP_MODE_IuC,      },
	{"push-hasheq-x",     FKL_OP_MODE_IuBB,     },
	{"push-hasheq-xx",    FKL_OP_MODE_IuCCB,    },
	{"push-hasheqv",      FKL_OP_MODE_IuB,      },
	{"push-hasheqv-c",    FKL_OP_MODE_IuC,      },
	{"push-hasheqv-x",    FKL_OP_MODE_IuBB,     },
	{"push-hasheqv-xx",   FKL_OP_MODE_IuCCB,    },
	{"push-hashequal",    FKL_OP_MODE_IuB,      },
	{"push-hashequal-c",  FKL_OP_MODE_IuC,      },
	{"push-hashequal-x",  FKL_OP_MODE_IuBB,     },
	{"push-hashequal-xx", FKL_OP_MODE_IuCCB,    },
	{"push-list-0",       FKL_OP_MODE_I,        },
	{"push-list",         FKL_OP_MODE_IuB,      },
	{"push-list-c",       FKL_OP_MODE_IuC,      },
	{"push-list-x",       FKL_OP_MODE_IuBB,     },
	{"push-list-xx",      FKL_OP_MODE_IuCCB,    },
	{"push-vec-0",        FKL_OP_MODE_I,        },
	{"list-push",         FKL_OP_MODE_I,        },
	{"import",            FKL_OP_MODE_IuAuB,    },
	{"import-x",          FKL_OP_MODE_IuCuC,    },
	{"import-xx",         FKL_OP_MODE_IuCAuBB,  },
	{"push-0",            FKL_OP_MODE_I,        },
	{"push-1",            FKL_OP_MODE_I,        },
	{"push-i8",           FKL_OP_MODE_IsA,      },
	{"push-i16",          FKL_OP_MODE_IsB,      },
	{"push-i64b",         FKL_OP_MODE_IuB,      },
	{"push-i64b-c",       FKL_OP_MODE_IuC,      },
	{"push-i64b-x",       FKL_OP_MODE_IuBB,     },
	{"get-loc",           FKL_OP_MODE_IuB,      },
	{"get-loc-c",         FKL_OP_MODE_IuC,      },
	{"get-loc-x",         FKL_OP_MODE_IuBB,     },
	{"put-loc",           FKL_OP_MODE_IuB,      },
	{"put-loc-c",         FKL_OP_MODE_IuC,      },
	{"put-loc-x",         FKL_OP_MODE_IuBB,     },
	{"get-var-ref",       FKL_OP_MODE_IuB,      },
	{"get-var-ref-c",     FKL_OP_MODE_IuC,      },
	{"get-var-ref-x",     FKL_OP_MODE_IuBB,     },
	{"put-var-ref",       FKL_OP_MODE_IuB,      },
	{"put-var-ref-c",     FKL_OP_MODE_IuC,      },
	{"put-var-ref-x",     FKL_OP_MODE_IuBB,     },
	{"export",            FKL_OP_MODE_IuB,      },
	{"export-c",          FKL_OP_MODE_IuC,      },
	{"export-x",          FKL_OP_MODE_IuBB,     },
	{"load-lib",          FKL_OP_MODE_IuB,      },
	{"load-lib-c",        FKL_OP_MODE_IuC,      },
	{"load-lib-x",        FKL_OP_MODE_IuBB,     },
	{"load-dll",          FKL_OP_MODE_IuB,      },
	{"load-dll-c",        FKL_OP_MODE_IuC,      },
	{"load-dll-x",        FKL_OP_MODE_IuBB,     },
	{"true",              FKL_OP_MODE_I,        },
	{"not",               FKL_OP_MODE_I,        },
	{"eq",                FKL_OP_MODE_I,        },
	{"eqv",               FKL_OP_MODE_I,        },
	{"equal",             FKL_OP_MODE_I,        },

	{"eqn",               FKL_OP_MODE_I,        },
	{"eqn3",              FKL_OP_MODE_I,        },

	{"gt",                FKL_OP_MODE_I,        },
	{"gt3",               FKL_OP_MODE_I,        },

	{"lt",                FKL_OP_MODE_I,        },
	{"lt3",               FKL_OP_MODE_I,        },

	{"ge",                FKL_OP_MODE_I,        },
	{"ge3",               FKL_OP_MODE_I,        },

	{"le",                FKL_OP_MODE_I,        },
	{"le3",               FKL_OP_MODE_I,        },

	{"inc",               FKL_OP_MODE_I,        },
	{"dec",               FKL_OP_MODE_I,        },
	{"add",               FKL_OP_MODE_I,        },
	{"sub",               FKL_OP_MODE_I,        },
	{"mul",               FKL_OP_MODE_I,        },
	{"div",               FKL_OP_MODE_I,        },
	{"idiv",              FKL_OP_MODE_I,        },
	{"mod",               FKL_OP_MODE_I,        },
	{"add1",              FKL_OP_MODE_I,        },
	{"mul1",              FKL_OP_MODE_I,        },
	{"neg",               FKL_OP_MODE_I,        },
	{"rec",               FKL_OP_MODE_I,        },
	{"add3",              FKL_OP_MODE_I,        },
	{"sub3",              FKL_OP_MODE_I,        },
	{"mul3",              FKL_OP_MODE_I,        },
	{"div3",              FKL_OP_MODE_I,        },
	{"idiv3",             FKL_OP_MODE_I,        },

	{"push-car",          FKL_OP_MODE_I,        },
	{"push-cdr",          FKL_OP_MODE_I,        },
	{"cons",              FKL_OP_MODE_I,        },
	{"nth",               FKL_OP_MODE_I,        },
	{"vec-ref",           FKL_OP_MODE_I,        },
	{"str-ref",           FKL_OP_MODE_I,        },
	{"box",               FKL_OP_MODE_I,        },
	{"unbox",             FKL_OP_MODE_I,        },
	{"box0",              FKL_OP_MODE_I,        },

	{"close-ref",         FKL_OP_MODE_IuAuB,    },
	{"close-ref-x",       FKL_OP_MODE_IuCuC,    },
	{"close-ref-xx",      FKL_OP_MODE_IuCAuBB,  },

	{"ret",               FKL_OP_MODE_I,        },
	{"export-to",         FKL_OP_MODE_IuAuB,    },
	{"export-to-x",       FKL_OP_MODE_IuCuC,    },
	{"export-to-xx",      FKL_OP_MODE_IuCAuBB,  },
	{"res-bp-tp",         FKL_OP_MODE_I,        },
	{"atom",              FKL_OP_MODE_I,        },

	{"extra-arg",         FKL_OP_MODE_IxAxB,    },

	{"push-dvec",         FKL_OP_MODE_IuB,      },
	{"push-dvec-c",       FKL_OP_MODE_IuC,      },
	{"push-dvec-x",       FKL_OP_MODE_IuBB,     },
	{"push-dvec-xx",      FKL_OP_MODE_IuCCB,    },
	{"push-dvec-0",       FKL_OP_MODE_I,        },
	{"dvec-ref",          FKL_OP_MODE_I,        },
	{"dvec-first",        FKL_OP_MODE_I,        },
	{"dvec-last",         FKL_OP_MODE_I,        },
	{"vec-first",         FKL_OP_MODE_I,        },
	{"vec-last",          FKL_OP_MODE_I,        },

	{"pop-loc",           FKL_OP_MODE_IuB,      },
	{"pop-loc-c",         FKL_OP_MODE_IuC,      },
	{"pop-loc-x",         FKL_OP_MODE_IuBB,     },

	{"car-set",           FKL_OP_MODE_I,        },
	{"cdr-set",           FKL_OP_MODE_I,        },
	{"box-set",           FKL_OP_MODE_I,        },

	{"vec-set",           FKL_OP_MODE_I,        },
	{"dvec-set",          FKL_OP_MODE_I,        },

	{"hash-ref-2",        FKL_OP_MODE_I,        },
	{"hash-ref-3",        FKL_OP_MODE_I,        },
	{"hash-set",          FKL_OP_MODE_I,        },

	{"inc-loc",           FKL_OP_MODE_IuB,      },

	{"dec-loc",           FKL_OP_MODE_IuB,      },

	{"call-loc",          FKL_OP_MODE_IuB,      },
	{"tail-call-loc",     FKL_OP_MODE_IuB,      },

	{"call-var-ref",      FKL_OP_MODE_IuB,      },
	{"tail-call-var-ref", FKL_OP_MODE_IuB,      },

	{"call-vec",          FKL_OP_MODE_I,        },
	{"tail-call-vec",     FKL_OP_MODE_I,        },

	{"call-car",          FKL_OP_MODE_I,        },
	{"tail-call-car",     FKL_OP_MODE_I,        },

	{"call-cdr",          FKL_OP_MODE_I,        },
	{"tail-call-cdr",     FKL_OP_MODE_I,        },

	{"ret-if-true",       FKL_OP_MODE_I,        },
	{"ret-if-false",      FKL_OP_MODE_I,        },
	{"mov-loc",           FKL_OP_MODE_IuAuB,    },
	{"mov-var-ref",       FKL_OP_MODE_IuAuB,    },
};

const char* fklGetOpcodeName(FklOpcode opcode)
{
	return opcodeInfo[opcode].name;
}

FklOpcodeMode fklGetOpcodeMode(FklOpcode opcode)
{
	return opcodeInfo[opcode].mode;
}

FklOpcode fklFindOpcode(const char* str)
{
	for(FklOpcode i=0;i<FKL_OP_LAST_OPCODE;i++)
		if(!strcmp(opcodeInfo[i].name,str))
			return i;
	return 0;
}

int fklGetOpcodeModeLen(FklOpcode op)
{
	FklOpcodeMode mode=opcodeInfo[op].mode;
	switch(mode)
	{
		case FKL_OP_MODE_I:
		case FKL_OP_MODE_IsA:
		case FKL_OP_MODE_IuB:
		case FKL_OP_MODE_IsB:
		case FKL_OP_MODE_IuC:
		case FKL_OP_MODE_IsC:
		case FKL_OP_MODE_IuAuB:
		case FKL_OP_MODE_IxAxB:
			return 1;
			break;

		case FKL_OP_MODE_IuBB:
		case FKL_OP_MODE_IsBB:
		case FKL_OP_MODE_IuCuC:
			return 2;
			break;

		case FKL_OP_MODE_IuCCB:
		case FKL_OP_MODE_IsCCB:
		case FKL_OP_MODE_IuCAuBB:
			return 3;
			break;

		case FKL_OP_MODE_IuCAuBCC:
			return 4;
			break;
	}
	return 0;
}

