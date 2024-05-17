#include<fakeLisp/vm.h>
#include<stdlib.h>
#include<time.h>

static int os_system(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(cmd,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(cmd,FKL_IS_STR,exe);
	const FklString* cmd_str=FKL_VM_STR(cmd);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMint(system(cmd_str->str),exe));
	return 0;
}

static int os_time(FKL_CPROC_ARGL)
{
	FKL_CHECK_REST_ARG(exe);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMint((int64_t)time(NULL),exe));
	return 0;
}

static int os_clock(FKL_CPROC_ARGL)
{
	FKL_CHECK_REST_ARG(exe);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMint((int64_t)clock(),exe));
	return 0;
}

static int os_date(FKL_CPROC_ARGL)
{
	FklVMvalue* arg1=FKL_VM_POP_ARG(exe);
	FklVMvalue* arg2=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
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
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
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
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	}
	return 0;
}

static int os_remove(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(name,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(name,FKL_IS_STR,exe);
	const FklString* name_str=FKL_VM_STR(name);
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(remove(name_str->str)));
	return 0;
}

static int os_rename(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(old_name,new_name,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(old_name,FKL_IS_STR,exe);
	FKL_CHECK_TYPE(new_name,FKL_IS_STR,exe);
	const FklString* old_name_str=FKL_VM_STR(old_name);
	const FklString* new_name_str=FKL_VM_STR(new_name);
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(rename(old_name_str->str,new_name_str->str)));
	return 0;
}

static int os_chdir(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(dir,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(dir,FKL_IS_STR,exe);
	int r=fklChdir(FKL_VM_STR(dir)->str);
	if(r)
		FKL_RAISE_BUILTIN_ERROR_FMT(FKL_ERR_FILEFAILURE,exe,"Failed to Change dir to: %s",dir);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int os_getcwd(FKL_CPROC_ARGL)
{
	FKL_CHECK_REST_ARG(exe);
	char* cwd=fklSysgetcwd();
	FklVMvalue* s=fklCreateVMvalueStr(exe,fklCreateStringFromCstr(cwd));
	free(cwd);
	FKL_VM_PUSH_VALUE(exe,s);
	return 0;
}

static int os_getenv(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(name,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(name,FKL_IS_STR,exe);
	const FklString* name_str=FKL_VM_STR(name);
	char* env_var=getenv(name_str->str);
	FKL_VM_PUSH_VALUE(exe,env_var?fklCreateVMvalueStr(exe,fklCreateStringFromCstr(env_var)):FKL_VM_NIL);
	return 0;
}

#ifdef WIN32
int setenv(const char* name, const char* value, int overwrite)
{
	if(!overwrite)
	{
		size_t envsize=0;
		int errcode=getenv_s(&envsize,NULL,0,name);
		if (errcode||envsize)return errcode;
	}
	return _putenv_s(name,value);
}

int unsetenv(const char* name)
{
	return _putenv_s(name,"");
}
#endif

static int os_setenv(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(name,exe);
	FklVMvalue* value=FKL_VM_POP_ARG(exe);
	FklVMvalue* overwrite=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(name,FKL_IS_STR,exe);
	const FklString* name_str=FKL_VM_STR(name);
	if(value)
	{
		FKL_CHECK_TYPE(value,FKL_IS_STR,exe);
		int o=(overwrite&&overwrite!=FKL_VM_NIL)?1:0;
		if(setenv(name_str->str,FKL_VM_STR(value)->str,o))
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE,exe);
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	}
	else
	{
		if(unsetenv(name_str->str))
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE,exe);
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

FKL_DLL_EXPORT FklSid_t* _fklExportSymbolInit(FKL_CODEGEN_DLL_LIB_INIT_EXPORT_FUNC_ARGS)
{
	*num=EXPORT_NUM;
	FklSid_t* symbols=(FklSid_t*)malloc(sizeof(FklSid_t)*EXPORT_NUM);
	FKL_ASSERT(symbols);
	for(size_t i=0;i<EXPORT_NUM;i++)
		symbols[i]=fklAddSymbolCstr(exports_and_func[i].sym,st)->id;
	return symbols;
}

FKL_DLL_EXPORT FklVMvalue** _fklImportInit(FKL_IMPORT_DLL_INIT_FUNC_ARGS)
{
	*count=EXPORT_NUM;
	FklVMvalue** loc=(FklVMvalue**)malloc(sizeof(FklVMvalue*)*EXPORT_NUM);
	FKL_ASSERT(loc);
	fklVMacquireSt(exe->gc);
	FklSymbolTable* st=exe->gc->st;
	for(size_t i=0;i<EXPORT_NUM;i++)
	{
		FklSid_t id=fklAddSymbolCstr(exports_and_func[i].sym,st)->id;
		FklVMcFunc func=exports_and_func[i].f;
		FklVMvalue* dlproc=fklCreateVMvalueCproc(exe,func,dll,NULL,id);
		loc[i]=dlproc;
	}
	fklVMreleaseSt(exe->gc);
	return loc;
}
