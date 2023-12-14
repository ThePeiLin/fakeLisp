#include<fakeLisp/vm.h>
#include<math.h>

static int math_srand(FKL_CPROC_ARGL)
{
	static const char Pname[]="math.srand";
	FKL_DECL_AND_CHECK_ARG(s,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(s,fklIsVMint,Pname,exe);
	srand(fklGetInt(s));
	FKL_VM_PUSH_VALUE(exe,s);
	return 0;
}

static int math_rand(FKL_CPROC_ARGL)
{
	static const char Pname[]="math.rand";
	FklVMvalue*  lim=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	if(lim&&!fklIsVMint(lim))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(rand()%((lim==NULL)?RAND_MAX:fklGetInt(lim))));
	return 0;
}

static int math_sqrt(FKL_CPROC_ARGL)
{
	static const char Pname[]="math.sqrt";
	FKL_DECL_AND_CHECK_ARG(num,Pname);
	FKL_CHECK_TYPE(num,fklIsVMnumber,Pname,exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueF64(exe,sqrt(fklGetDouble(num))));
	return 0;
}

static int math_abs(FKL_CPROC_ARGL)
{
	static const char Pname[]="math.abs";
	FKL_DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,fklIsVMnumber,Pname,exe);
	if(FKL_IS_F64(obj))
		FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueF64(exe,fabs(FKL_VM_F64(obj))));
	else
	{
		if(FKL_IS_FIX(obj))
		{
			int64_t i=llabs(FKL_GET_FIX(obj));
			if(i>FKL_FIX_INT_MAX)
				FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueBigIntWithI64(exe,i));
			else
				FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(i));
		}
		else
		{
			FklVMvalue* r=fklCreateVMvalueBigInt(exe,FKL_VM_BI(obj));
			FKL_VM_BI(r)->neg=0;
			FKL_VM_PUSH_VALUE(exe,r);
		}
	}
	return 0;
}

static int math_odd_p(FKL_CPROC_ARGL)
{
	static const char Pname[]="math.odd?";
	FKL_DECL_AND_CHECK_ARG(val,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(val,fklIsVMint,Pname,exe);
	int r=0;
	if(FKL_IS_FIX(val))
		r=FKL_GET_FIX(val)%2;
	else
		r=fklIsBigIntOdd(FKL_VM_BI(val));
	if(r)
		FKL_VM_PUSH_VALUE(exe,FKL_VM_TRUE);
	else
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int math_even_p(FKL_CPROC_ARGL)
{
	static const char Pname[]="math.even?";
	FKL_DECL_AND_CHECK_ARG(val,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(val,fklIsVMint,Pname,exe);
	int r=0;
	if(FKL_IS_FIX(val))
		r=FKL_GET_FIX(val)%2==0;
	else
		r=fklIsBigIntEven(FKL_VM_BI(val));
	if(r)
		FKL_VM_PUSH_VALUE(exe,FKL_VM_TRUE);
	else
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int math_E(FKL_CPROC_ARGL)
{
	static const char Pname[]="math.E";
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueF64(exe,M_E));
	return 0;
}

static int math_PI(FKL_CPROC_ARGL)
{
	static const char Pname[]="math.PI";
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueF64(exe,M_PI));
	return 0;
}

static int math_PI_2(FKL_CPROC_ARGL)
{
	static const char Pname[]="math.PI/2";
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueF64(exe,M_PI_2));
	return 0;
}

static int math_PI_4(FKL_CPROC_ARGL)
{
	static const char Pname[]="math.PI/4";
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueF64(exe,M_PI_4));
	return 0;
}

static int math_1_PI(FKL_CPROC_ARGL)
{
	static const char Pname[]="math.1/PI";
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueF64(exe,M_1_PI));
	return 0;
}

static int math_2_PI(FKL_CPROC_ARGL)
{
	static const char Pname[]="math.2/PI";
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueF64(exe,M_2_PI));
	return 0;
}

static int math_rad(FKL_CPROC_ARGL)
{
	static const char Pname[]="math.rad";
	FKL_DECL_AND_CHECK_ARG(num,Pname);
	FKL_CHECK_TYPE(num,fklIsVMnumber,Pname,exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueF64(exe,fklGetDouble(num)*(M_PI/180.0)));
	return 0;
}

static int math_sin(FKL_CPROC_ARGL)
{
	static const char Pname[]="math.sin";
	FKL_DECL_AND_CHECK_ARG(num,Pname);
	FKL_CHECK_TYPE(num,fklIsVMnumber,Pname,exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueF64(exe,sin(fklGetDouble(num))));
	return 0;
}

static int math_cos(FKL_CPROC_ARGL)
{
	static const char Pname[]="math.cos";
	FKL_DECL_AND_CHECK_ARG(num,Pname);
	FKL_CHECK_TYPE(num,fklIsVMnumber,Pname,exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueF64(exe,cos(fklGetDouble(num))));
	return 0;
}

static int math_tan(FKL_CPROC_ARGL)
{
	static const char Pname[]="math.tan";
	FKL_DECL_AND_CHECK_ARG(num,Pname);
	FKL_CHECK_TYPE(num,fklIsVMnumber,Pname,exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueF64(exe,tan(fklGetDouble(num))));
	return 0;
}

static int math_asin(FKL_CPROC_ARGL)
{
	static const char Pname[]="math.asin";
	FKL_DECL_AND_CHECK_ARG(num,Pname);
	FKL_CHECK_TYPE(num,fklIsVMnumber,Pname,exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueF64(exe,asin(fklGetDouble(num))));
	return 0;
}

static int math_acos(FKL_CPROC_ARGL)
{
	static const char Pname[]="math.acos";
	FKL_DECL_AND_CHECK_ARG(num,Pname);
	FKL_CHECK_TYPE(num,fklIsVMnumber,Pname,exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueF64(exe,acos(fklGetDouble(num))));
	return 0;
}

static int math_atan(FKL_CPROC_ARGL)
{
	static const char Pname[]="math.atan";
	FKL_DECL_AND_CHECK_ARG(num,Pname);
	FKL_CHECK_TYPE(num,fklIsVMnumber,Pname,exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueF64(exe,atan(fklGetDouble(num))));
	return 0;
}

static int math_ceil(FKL_CPROC_ARGL)
{
	static const char Pname[]="math.ceil";
	FKL_DECL_AND_CHECK_ARG(num,Pname);
	FKL_CHECK_TYPE(num,fklIsVMnumber,Pname,exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueF64(exe,ceil(fklGetDouble(num))));
	return 0;
}

static int math_floor(FKL_CPROC_ARGL)
{
	static const char Pname[]="math.floor";
	FKL_DECL_AND_CHECK_ARG(num,Pname);
	FKL_CHECK_TYPE(num,fklIsVMnumber,Pname,exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueF64(exe,floor(fklGetDouble(num))));
	return 0;
}

static int math_round(FKL_CPROC_ARGL)
{
	static const char Pname[]="math.round";
	FKL_DECL_AND_CHECK_ARG(num,Pname);
	FKL_CHECK_TYPE(num,fklIsVMnumber,Pname,exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueF64(exe,round(fklGetDouble(num))));
	return 0;
}

static int math_trunc(FKL_CPROC_ARGL)
{
	static const char Pname[]="math.round";
	FKL_DECL_AND_CHECK_ARG(num,Pname);
	FKL_CHECK_TYPE(num,fklIsVMnumber,Pname,exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueF64(exe,trunc(fklGetDouble(num))));
	return 0;
}

struct SymFunc
{
	const char* sym;
	FklVMcFunc f;
}exports_and_func[]=
{
	{"even?", math_even_p, },
	{"odd?",  math_odd_p,  },

	{"srand", math_srand,  },
	{"rand",  math_rand,   },

	{"sqrt",  math_sqrt,   },
	{"abs",   math_abs,    },

	{"rad",   math_rad,    },
	// {"deg",   math_deg,    },

	{"asin",  math_asin,   },
	{"acos",  math_acos,   },
	{"atan",  math_atan,   },

	{"sin",   math_sin,    },
	{"cos",   math_cos,    },
	{"tan",   math_tan,    },

	{"ceil",  math_ceil,   },
	{"floor", math_floor,  },
	{"trunc", math_trunc,  },
	{"round", math_round,  },

	{"E",     math_E,      },
	{"PI",    math_PI,     },
	{"PI/2",  math_PI_2,   },
	{"PI/4",  math_PI_4,   },
	{"1/PI",  math_1_PI,   },
	{"2/PI",  math_2_PI,   },
};

static const size_t EXPORT_NUM=sizeof(exports_and_func)/sizeof(struct SymFunc);

FKL_DLL_EXPORT void _fklExportSymbolInit(FKL_CODEGEN_DLL_LIB_INIT_EXPORT_FUNC_ARGS)
{
	*num=EXPORT_NUM;
	FklSid_t* symbols=(FklSid_t*)malloc(sizeof(FklSid_t)*EXPORT_NUM);
	FKL_ASSERT(symbols);
	for(size_t i=0;i<EXPORT_NUM;i++)
		symbols[i]=fklAddSymbolCstr(exports_and_func[i].sym,st)->id;
	*exports=symbols;
}

FKL_DLL_EXPORT FklVMvalue** _fklImportInit(FKL_IMPORT_DLL_INIT_FUNC_ARGS)
{
	FklSymbolTable* table=exe->symbolTable;
	*count=EXPORT_NUM;
	FklVMvalue** loc=(FklVMvalue**)malloc(sizeof(FklVMvalue*)*EXPORT_NUM);
	FKL_ASSERT(loc);
	for(size_t i=0;i<EXPORT_NUM;i++)
	{
		FklSid_t id=fklAddSymbolCstr(exports_and_func[i].sym,table)->id;
		FklVMcFunc func=exports_and_func[i].f;
		FklVMvalue* dlproc=fklCreateVMvalueCproc(exe,func,dll,NULL,id);
		loc[i]=dlproc;
	}
	return loc;
}
