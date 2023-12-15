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

static int math_rad(FKL_CPROC_ARGL)
{
	static const char Pname[]="math.rad";
	FKL_DECL_AND_CHECK_ARG(num,Pname);
	FKL_CHECK_TYPE(num,fklIsVMnumber,Pname,exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueF64(exe,fklGetDouble(num)*(M_PI/180.0)));
	return 0;
}

static int math_deg(FKL_CPROC_ARGL)
{
	static const char Pname[]="math.deg";
	FKL_DECL_AND_CHECK_ARG(num,Pname);
	FKL_CHECK_TYPE(num,fklIsVMnumber,Pname,exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueF64(exe,fklGetDouble(num)*(180.0/M_PI)));
	return 0;
}

#define SINGLE_ARG_MATH_FUNC(NAME,ERROR_NAME,FUNC) static int math_##NAME(FKL_CPROC_ARGL)\
{\
	static const char Pname[]="math."#ERROR_NAME;\
	FKL_DECL_AND_CHECK_ARG(num,Pname);\
	FKL_CHECK_TYPE(num,fklIsVMnumber,Pname,exe);\
	FKL_CHECK_REST_ARG(exe,Pname);\
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueF64(exe,FUNC(fklGetDouble(num))));\
	return 0;\
}

SINGLE_ARG_MATH_FUNC(sqrt,sqrt,sqrt);
SINGLE_ARG_MATH_FUNC(cbrt,cbrt,cbrt);

SINGLE_ARG_MATH_FUNC(sin,sin,sin);
SINGLE_ARG_MATH_FUNC(cos,cos,cos);
SINGLE_ARG_MATH_FUNC(tan,tan,tan);

SINGLE_ARG_MATH_FUNC(sinh,sinh,sinh);
SINGLE_ARG_MATH_FUNC(cosh,cosh,cosh);
SINGLE_ARG_MATH_FUNC(tanh,tanh,tanh);

SINGLE_ARG_MATH_FUNC(asin,asin,asin);
SINGLE_ARG_MATH_FUNC(acos,acos,acos);
SINGLE_ARG_MATH_FUNC(atan,atan,atan);

SINGLE_ARG_MATH_FUNC(asinh,asinh,asinh);
SINGLE_ARG_MATH_FUNC(acosh,acosh,acosh);
SINGLE_ARG_MATH_FUNC(atanh,atanh,atanh);

SINGLE_ARG_MATH_FUNC(ceil,ceil,ceil);
SINGLE_ARG_MATH_FUNC(floor,floor,floor);
SINGLE_ARG_MATH_FUNC(round,round,round);
SINGLE_ARG_MATH_FUNC(trunc,trunc,trunc);

SINGLE_ARG_MATH_FUNC(exp,exp,exp);
SINGLE_ARG_MATH_FUNC(exp2,exp2,exp2);
SINGLE_ARG_MATH_FUNC(expm1,expm1,expm1);

SINGLE_ARG_MATH_FUNC(log2,log2,log2);
SINGLE_ARG_MATH_FUNC(log10,log10,log10);
SINGLE_ARG_MATH_FUNC(log1p,log1p,log1p);

SINGLE_ARG_MATH_FUNC(erf,erf,erf);
SINGLE_ARG_MATH_FUNC(erfc,erfc,erfc);
SINGLE_ARG_MATH_FUNC(gamma,gamma,tgamma);
SINGLE_ARG_MATH_FUNC(lgamma,lgamma,lgamma);

#undef SINGLE_ARG_MATH_FUNC

static int math_modf(FKL_CPROC_ARGL)
{
	static const char Pname[]="math.modf";
	FKL_DECL_AND_CHECK_ARG(num,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(num,fklIsVMnumber,Pname,exe);
	double car=0.0;
	double cdr=modf(fklGetDouble(num),&car);
	FKL_VM_PUSH_VALUE(exe
			,fklCreateVMvaluePair(exe
				,fklCreateVMvalueF64(exe,car)
				,fklCreateVMvalueF64(exe,cdr)));
	return 0;
}

static int math_frexp(FKL_CPROC_ARGL)
{
	static const char Pname[]="math.frexp";
	FKL_DECL_AND_CHECK_ARG(num,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(num,fklIsVMnumber,Pname,exe);
	int32_t cdr=0;
	double car=frexp(fklGetDouble(num),&cdr);
	FKL_VM_PUSH_VALUE(exe
			,fklCreateVMvaluePair(exe
				,fklCreateVMvalueF64(exe,car)
				,FKL_MAKE_VM_FIX(cdr)));
	return 0;
}

static int math_log(FKL_CPROC_ARGL)
{
	static const char Pname[]="math.log";
	FKL_DECL_AND_CHECK_ARG(x,Pname);
	FklVMvalue* base=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(x,fklIsVMnumber,Pname,exe);
	if(base)
	{
		FKL_CHECK_TYPE(base,fklIsVMnumber,Pname,exe);
		double b=fklGetDouble(base);
		if(!islessgreater(b,2.0))
			FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueF64(exe,log2(fklGetDouble(x))));
		else if(!islessgreater(b,10.0))
			FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueF64(exe,log10(fklGetDouble(x))));
		else
			FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueF64(exe,log(fklGetDouble(x))/log(b)));
	}
	else
		FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueF64(exe,log(fklGetDouble(x))));
	return 0;
}

#define DOUBLE_ARG_MATH_FUNC(NAME,ERROR_NAME,FUNC) static int math_##NAME(FKL_CPROC_ARGL)\
{\
	static const char Pname[]="math."#ERROR_NAME;\
	FKL_DECL_AND_CHECK_ARG2(x,y,Pname);\
	FKL_CHECK_REST_ARG(exe,Pname);\
	FKL_CHECK_TYPE(x,fklIsVMnumber,Pname,exe);\
	FKL_CHECK_TYPE(y,fklIsVMnumber,Pname,exe);\
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueF64(exe,FUNC(fklGetDouble(x),fklGetDouble(y))));\
	return 0;\
}

DOUBLE_ARG_MATH_FUNC(remainder,remainder,remainder);
DOUBLE_ARG_MATH_FUNC(hypot,hypot,hypot);
DOUBLE_ARG_MATH_FUNC(atan2,atan2,atan2);
DOUBLE_ARG_MATH_FUNC(pow,pow,pow);
DOUBLE_ARG_MATH_FUNC(ldexp,ldexp,ldexp);
DOUBLE_ARG_MATH_FUNC(nextafter,nextafter,nextafter);
DOUBLE_ARG_MATH_FUNC(copysign,copysign,copysign);

#undef DOUBLE_ARG_MATH_FUNC

#define PREDICATE_FUNC(NAME,ERROR_NAME,FUNC) static int math_##NAME(FKL_CPROC_ARGL)\
{\
	static const char Pname[]="math."ERROR_NAME;\
	FKL_DECL_AND_CHECK_ARG(val,Pname);\
	FKL_CHECK_REST_ARG(exe,Pname);\
	FKL_CHECK_TYPE(val,fklIsVMnumber,Pname,exe);\
	FKL_VM_PUSH_VALUE(exe,FUNC(fklGetDouble(val))\
			?FKL_VM_TRUE\
			:FKL_VM_NIL);\
	return 0;\
}

PREDICATE_FUNC(signbit,"signbit",signbit);
PREDICATE_FUNC(nan_p,"nan?",isnan);
PREDICATE_FUNC(finite_p,"finite?",isfinite);
PREDICATE_FUNC(inf_p,"inf?",isinf);
PREDICATE_FUNC(normal_p,"normal?",isnormal);

#undef PREDICATE_FUNC

static int math_unordered_p(FKL_CPROC_ARGL)
{
	static const char Pname[]="math.unordered?";
	FKL_DECL_AND_CHECK_ARG2(x,y,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(x,fklIsVMnumber,Pname,exe);
	FKL_CHECK_TYPE(y,fklIsVMnumber,Pname,exe);
	FKL_VM_PUSH_VALUE(exe,isunordered(fklGetDouble(x),fklGetDouble(y))
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

struct SymFunc
{
	const char* sym;
	FklVMcFunc f;
}exports_and_func[]=
{
	{"even?",      math_even_p,      },
	{"odd?",       math_odd_p,       },

	{"srand",      math_srand,       },
	{"rand",       math_rand,        },

	{"sqrt",       math_sqrt,        },
	{"cbrt",       math_cbrt,        },
	{"pow",        math_pow,         },
	{"hypot",      math_hypot,       },

	{"abs",        math_abs,         },
	{"remainder",  math_remainder,   },

	{"rad",        math_rad,         },
	{"deg",        math_deg,         },

	{"asin",       math_asin,        },
	{"acos",       math_acos,        },
	{"atan",       math_atan,        },
	{"atan2",      math_atan2,       },

	{"asinh",      math_asinh,       },
	{"acosh",      math_acosh,       },
	{"atanh",      math_atanh,       },

	{"sinh",       math_sinh,        },
	{"cosh",       math_cosh,        },
	{"tanh",       math_tanh,        },

	{"sin",        math_sin,         },
	{"cos",        math_cos,         },
	{"tan",        math_tan,         },

	{"ceil",       math_ceil,        },
	{"floor",      math_floor,       },
	{"trunc",      math_trunc,       },
	{"round",      math_round,       },

	{"frexp",      math_frexp,       },
	{"ldexp",      math_ldexp,       },
	{"modf",       math_modf,        },
	{"nextafter",  math_nextafter,   },
	{"copysign",   math_copysign,    },

	{"log",        math_log,         },
	{"log2",       math_log2,        },
	{"log10",      math_log10,       },
	{"log1p",      math_log1p,       },

	{"exp",        math_exp,         },
	{"exp2",       math_exp2,        },
	{"expm1",      math_expm1,       },

	{"erf",        math_erf,         },
	{"erfc",       math_erfc,        },
	{"gamma",      math_gamma,       },
	{"lgamma",     math_lgamma,      },

	{"signbit",    math_signbit,     },
	{"nan?",       math_nan_p,       },
	{"finite?",    math_finite_p,    },
	{"inf?",       math_inf_p,       },
	{"normal?",    math_normal_p,    },
	{"unordered?", math_unordered_p, },

	{"NAN",        NULL,             },
	{"HUGE",       NULL,             },
	{"E",          NULL,             },
	{"PI",         NULL,             },
	{"PI/2",       NULL,             },
	{"PI/4",       NULL,             },
	{"1/PI",       NULL,             },
	{"2/PI",       NULL,             },
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
	size_t i=0;
	for(;i<EXPORT_NUM;i++)
	{
		FklVMcFunc func=exports_and_func[i].f;
		if(func==NULL)
			break;
		FklSid_t id=fklAddSymbolCstr(exports_and_func[i].sym,table)->id;
		FklVMvalue* dlproc=fklCreateVMvalueCproc(exe,func,dll,NULL,id);
		loc[i]=dlproc;
	}

	loc[i++]=fklCreateVMvalueF64(exe,NAN);
	loc[i++]=fklCreateVMvalueF64(exe,HUGE_VAL);
	loc[i++]=fklCreateVMvalueF64(exe,M_E);
	loc[i++]=fklCreateVMvalueF64(exe,M_PI);
	loc[i++]=fklCreateVMvalueF64(exe,M_PI_2);
	loc[i++]=fklCreateVMvalueF64(exe,M_PI_4);
	loc[i++]=fklCreateVMvalueF64(exe,M_1_PI);
	loc[i++]=fklCreateVMvalueF64(exe,M_2_PI);
	return loc;
}
