#include<fakeLisp/vm.h>
#include<stdlib.h>

static int os_system(FKL_CPROC_ARGL)
{
	static const char Pname[]="os.system";
	FKL_DECL_AND_CHECK_ARG(cmd,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(cmd,FKL_IS_STR,Pname,exe);
	const FklString* cmd_str=FKL_VM_STR(cmd);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMint(system(cmd_str->str),exe));
	return 0;
}

static int os_time(FKL_CPROC_ARGL)
{
	static const char Pname[]="os.time";
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMint((int64_t)time(NULL),exe));
	return 0;
}

static int os_clock(FKL_CPROC_ARGL)
{
	static const char Pname[]="os.clock";
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMint((int64_t)clock(),exe));
	return 0;
}

static int os_date(FKL_CPROC_ARGL)
{
	static const char Pname[]="os.date";
	FklVMvalue* arg1=FKL_VM_POP_ARG(exe);
	FklVMvalue* arg2=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	if(arg1==NULL&&arg2==NULL)
	{
		time_t stamp=time(NULL);
		const struct tm* tblock=localtime(&stamp);

		FklStringBuffer buf;
		fklInitStringBuffer(&buf);

		fklStringBufferPrintf(&buf,"%04u-%02u-%02u_%02u_%02u_%02u"
				,tblock->tm_year+1900
				,tblock->tm_mon+1
				,tblock->tm_mday
				,tblock->tm_hour
				,tblock->tm_min
				,tblock->tm_sec);

		FklVMvalue* tmpVMvalue=fklCreateVMvalueStr(exe,fklCreateString(buf.index,buf.buf));
		fklUninitStringBuffer(&buf);

		FKL_VM_PUSH_VALUE(exe,tmpVMvalue);
	}
	else if(arg1&&arg2)
	{
		if(FKL_IS_STR(arg1)&&fklIsVMint(arg2))
		{
			FklVMvalue* fmt=arg1;
			FklVMvalue* timestamp=arg2;
			FklStringBuffer buf;
			fklInitStringBuffer(&buf);
			int64_t stamp=fklGetInt(timestamp);
			const struct tm* tblock=localtime(&stamp);
			const char* format=FKL_VM_STR(fmt)->str;
			size_t n=0;
			while(!(n=strftime(buf.buf,buf.size,format,tblock)))
				fklStringBufferReverse(&buf,buf.size*2);
			fklStringBufferReverse(&buf,n+1);
			buf.index=n;

			FklVMvalue* tmpVMvalue=fklCreateVMvalueStr(exe,fklCreateString(buf.index,buf.buf));
			fklUninitStringBuffer(&buf);
			FKL_VM_PUSH_VALUE(exe,tmpVMvalue);
		}
		else
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	}
	else
	{
		if(FKL_IS_STR(arg1))
		{
			FklVMvalue* fmt=arg1;
			FklStringBuffer buf;
			fklInitStringBuffer(&buf);
			time_t stamp=time(NULL);
			const struct tm* tblock=localtime(&stamp);
			const char* format=FKL_VM_STR(fmt)->str;
			size_t n=0;
			while(!(n=strftime(buf.buf,buf.size,format,tblock)))
				fklStringBufferReverse(&buf,buf.size*2);
			fklStringBufferReverse(&buf,n+1);
			buf.index=n;

			FklVMvalue* tmpVMvalue=fklCreateVMvalueStr(exe,fklCreateString(buf.index,buf.buf));
			fklUninitStringBuffer(&buf);
			FKL_VM_PUSH_VALUE(exe,tmpVMvalue);
		}
		else if(fklIsVMint(arg1))
		{
			static const char default_fmt[]="%Y-%m-%d_%H_%M_%S";
			FklVMvalue* timestamp=arg1;
			FklStringBuffer buf;
			fklInitStringBuffer(&buf);
			int64_t stamp=fklGetInt(timestamp);
			const struct tm* tblock=localtime(&stamp);
			size_t n=0;
			while(!(n=strftime(buf.buf,buf.size,default_fmt,tblock)))
				fklStringBufferReverse(&buf,buf.size*2);
			fklStringBufferReverse(&buf,n+1);
			buf.index=n;

			FklVMvalue* tmpVMvalue=fklCreateVMvalueStr(exe,fklCreateString(buf.index,buf.buf));
			fklUninitStringBuffer(&buf);
			FKL_VM_PUSH_VALUE(exe,tmpVMvalue);
		}
		else
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	}
	return 0;
}

static int os_remove(FKL_CPROC_ARGL)
{
	static const char Pname[]="os.remove";
	FKL_DECL_AND_CHECK_ARG(name,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(name,FKL_IS_STR,Pname,exe);
	const FklString* name_str=FKL_VM_STR(name);
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(remove(name_str->str)));
	return 0;
}

static int os_rename(FKL_CPROC_ARGL)
{
	static const char Pname[]="os.rename";
	FKL_DECL_AND_CHECK_ARG2(old_name,new_name,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(old_name,FKL_IS_STR,Pname,exe);
	FKL_CHECK_TYPE(new_name,FKL_IS_STR,Pname,exe);
	const FklString* old_name_str=FKL_VM_STR(old_name);
	const FklString* new_name_str=FKL_VM_STR(new_name);
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(rename(old_name_str->str,new_name_str->str)));
	return 0;
}

static int os_chdir(FKL_CPROC_ARGL)
{
	static const char Pname[]="os.chdir";
	FKL_DECL_AND_CHECK_ARG(dir,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(dir,FKL_IS_STR,Pname,exe);
	FklString* dirstr=FKL_VM_STR(dir);
	int r=fklChdir(dirstr->str);
	if(r)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR(Pname,dirstr->str,0,FKL_ERR_FILEFAILURE,exe);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int os_getcwd(FKL_CPROC_ARGL)
{
	static const char Pname[]="os.getcwd";
	FKL_CHECK_REST_ARG(exe,Pname);
	char* cwd=fklSysgetcwd();
	FklVMvalue* s=fklCreateVMvalueStr(exe,fklCreateStringFromCstr(cwd));
	free(cwd);
	FKL_VM_PUSH_VALUE(exe,s);
	return 0;
}

static int os_getenv(FKL_CPROC_ARGL)
{
	static const char Pname[]="os.getenv";
	FKL_DECL_AND_CHECK_ARG(name,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(name,FKL_IS_STR,Pname,exe);
	const FklString* name_str=FKL_VM_STR(name);
	char* env_var=getenv(name_str->str);
	FKL_VM_PUSH_VALUE(exe,env_var?fklCreateVMvalueStr(exe,fklCreateStringFromCstr(env_var)):FKL_VM_NIL);
	return 0;
}

static int os_setenv(FKL_CPROC_ARGL)
{
	static const char Pname[]="os.setenv";
	FKL_DECL_AND_CHECK_ARG(name,exe,Pname);
	FklVMvalue* value=FKL_VM_POP_ARG(exe);
	FklVMvalue* overwrite=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(name,FKL_IS_STR,Pname,exe);
	const FklString* name_str=FKL_VM_STR(name);
	if(value)
	{
		FKL_CHECK_TYPE(value,FKL_IS_STR,Pname,exe);
		int o=(overwrite&&overwrite!=FKL_VM_NIL)?1:0;
		if(setenv(name_str->str,FKL_VM_STR(value)->str,o))
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALID_VALUE,exe);
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	}
	else
	{
		if(unsetenv(name_str->str))
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALID_VALUE,exe);
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	}
	return 0;
}

struct SymFunc
{
	const char* sym;
	FklVMcFunc f;
}exports_and_func[]=
{
	{"system",   os_system,   },
	{"remove",   os_remove,   },
	{"rename",   os_rename,   },
	{"clock",    os_clock,    },
	{"time",     os_time,     },
	{"date",     os_date,     },
	{"chdir",    os_chdir,    },
	{"getcwd",   os_getcwd,   },
	{"getenv",   os_getenv,   },
	{"setenv",   os_setenv,   },
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
	*count=EXPORT_NUM;
	FklVMvalue** loc=(FklVMvalue**)malloc(sizeof(FklVMvalue*)*EXPORT_NUM);
	FKL_ASSERT(loc);
	for(size_t i=0;i<EXPORT_NUM;i++)
	{
		FklSid_t id=fklVMaddSymbolCstr(exe->gc,exports_and_func[i].sym)->id;
		FklVMcFunc func=exports_and_func[i].f;
		FklVMvalue* dlproc=fklCreateVMvalueCproc(exe,func,dll,NULL,id);
		loc[i]=dlproc;
	}
	return loc;
}
