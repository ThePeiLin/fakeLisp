#include<fakeLisp/vm.h>

static int fs_facce_p(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(filename,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(filename,FKL_IS_STR,exe);
	FKL_VM_PUSH_VALUE(exe,fklIsAccessibleRegFile(FKL_VM_STR(filename)->str)
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int fs_freg_p(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(filename,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(filename,FKL_IS_STR,exe);
	FKL_VM_PUSH_VALUE(exe
			,fklIsAccessibleRegFile(FKL_VM_STR(filename)->str)
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int fs_fdir_p(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(filename,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(filename,FKL_IS_STR,exe);
	FKL_VM_PUSH_VALUE(exe
			,fklIsAccessibleDirectory(FKL_VM_STR(filename)->str)
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int fs_freopen(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(stream,filename,exe);
	FklVMvalue* mode=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	if(!FKL_IS_FP(stream)||!FKL_IS_STR(filename)||(mode&&!FKL_IS_STR(mode)))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklVMfp* vfp=FKL_VM_FP(stream);
	FklString* filenameStr=FKL_VM_STR(filename);
	const char* modeStr=mode?FKL_VM_STR(mode)->str:"r";
	FILE* fp=freopen(filenameStr->str,modeStr,vfp->fp);
	if(!fp)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR(filenameStr->str,0,FKL_ERR_FILEFAILURE,exe);
	else
	{
		vfp->fp=fp;
		vfp->rw=fklGetVMfpRwFromCstr(modeStr);
	}
	FKL_VM_PUSH_VALUE(exe,stream);
	return 0;
}

static int fs_realpath(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(filename,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(filename,FKL_IS_STR,exe);
	char* rp=fklRealpath(FKL_VM_STR(filename)->str);
	if(rp)
	{
		FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueStr(exe,fklCreateStringFromCstr(rp)));
		free(rp);
	}
	else
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int fs_relpath(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(path,exe);
	FklVMvalue* start=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(path,FKL_IS_STR,exe);

	if(start)
		FKL_CHECK_TYPE(start,FKL_IS_STR,exe);

	char* path_rp=fklRealpath(FKL_VM_STR(path)->str);
	if(!path_rp)
	{
		path_rp=fklStrCat(fklSysgetcwd(),FKL_PATH_SEPARATOR_STR);
		path_rp=fklStrCat(path_rp,FKL_VM_STR(path)->str);
	}

	char* start_rp=start?fklRealpath(FKL_VM_STR(start)->str):fklRealpath(".");
	if(!start_rp)
	{
		start_rp=fklStrCat(fklSysgetcwd(),FKL_PATH_SEPARATOR_STR);
		start_rp=fklStrCat(start_rp,FKL_VM_STR(start)->str);
	}

	char* relpath_cstr=fklRelpath(start_rp
			?start_rp
			:FKL_VM_STR(start)->str
			,path_rp
			?path_rp
			:FKL_VM_STR(path)->str);

	if(relpath_cstr)
	{
		FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueStr(exe,fklCreateStringFromCstr(relpath_cstr)));
		free(relpath_cstr);
	}
	else
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	free(start_rp);
	free(path_rp);
	return 0;
}

static int fs_mkdir(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(filename,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(filename,FKL_IS_STR,exe);
	char* filename_str=FKL_VM_STR(filename)->str;
	if(fklMkdir(filename_str))
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR(filename_str,0,FKL_ERR_FILEFAILURE,exe);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static inline int isVMfpWritable(const FklVMvalue* fp)
{
	return FKL_VM_FP(fp)->rw&FKL_VM_FP_W_MASK;
}

#define CHECK_FP_WRITABLE(V,E) if(!isVMfpWritable(V))\
	FKL_RAISE_BUILTIN_ERROR(FKL_ERR_UNSUPPORTED_OP,E)

#define CHECK_FP_OPEN(V,E) if(!FKL_VM_FP(V)->fp)\
	FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,E)

static int fs_fprint(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(f,exe);
	FKL_CHECK_TYPE(f,FKL_IS_FP,exe);
	CHECK_FP_OPEN(f,exe);
	CHECK_FP_WRITABLE(f,exe);
	FklVMvalue* obj=FKL_VM_POP_ARG(exe);
	FklVMvalue* r=FKL_VM_NIL;
	FILE* fp=FKL_VM_FP(f)->fp;
	for(;obj;r=obj,obj=FKL_VM_POP_ARG(exe))
		fklPrincVMvalue(obj,fp,exe->gc);
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int fs_fprintln(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(f,exe);
	FKL_CHECK_TYPE(f,FKL_IS_FP,exe);
	CHECK_FP_OPEN(f,exe);
	CHECK_FP_WRITABLE(f,exe);
	FklVMvalue* obj=FKL_VM_POP_ARG(exe);
	FklVMvalue* r=FKL_VM_NIL;
	FILE* fp=FKL_VM_FP(f)->fp;
	for(;obj;r=obj,obj=FKL_VM_POP_ARG(exe))
		fklPrincVMvalue(obj,fp,exe->gc);
	fputc('\n',stdout);
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int fs_fprin1(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(f,exe);
	FKL_CHECK_TYPE(f,FKL_IS_FP,exe);
	CHECK_FP_OPEN(f,exe);
	CHECK_FP_WRITABLE(f,exe);

	FklVMvalue* obj=FKL_VM_POP_ARG(exe);
	FklVMvalue* r=FKL_VM_NIL;
	FILE* fp=FKL_VM_FP(f)->fp;
	for(;obj;r=obj,obj=FKL_VM_POP_ARG(exe))
		fklPrin1VMvalue(obj,fp,exe->gc);
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int fs_fprin1n(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(f,exe);
	FKL_CHECK_TYPE(f,FKL_IS_FP,exe);
	CHECK_FP_OPEN(f,exe);
	CHECK_FP_WRITABLE(f,exe);

	FklVMvalue* obj=FKL_VM_POP_ARG(exe);
	FklVMvalue* r=FKL_VM_NIL;
	FILE* fp=FKL_VM_FP(f)->fp;
	for(;obj;r=obj,obj=FKL_VM_POP_ARG(exe))
		fklPrin1VMvalue(obj,fp,exe->gc);
	fputc('\n',stdout);
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int fs_fwrite(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(f,exe);
	FKL_CHECK_TYPE(f,FKL_IS_FP,exe);
	CHECK_FP_OPEN(f,exe);
	CHECK_FP_WRITABLE(f,exe);

	FILE* fp=FKL_VM_FP(f)->fp;
	FklVMvalue* obj=FKL_VM_POP_ARG(exe);
	FklVMvalue* r=FKL_VM_NIL;
	for(;obj;r=obj,obj=FKL_VM_POP_ARG(exe))
	{
		if(FKL_IS_STR(obj))
		{
			FklString* str=FKL_VM_STR(obj);
			fwrite(str->str,str->size,1,fp);
		}
		else if(FKL_IS_BYTEVECTOR(obj))
		{
			FklBytevector* bvec=FKL_VM_BVEC(obj);
			fwrite(bvec->ptr,bvec->size,1,fp);
		}
		else if(FKL_IS_USERDATA(obj)&&fklIsWritableUd(FKL_VM_UD(obj)))
			fklWriteVMud(FKL_VM_UD(obj),fp);
		else
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	}
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int fs_fflush(FKL_CPROC_ARGL)
{
	FklVMvalue* fp=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	if(fp)
	{
		FKL_CHECK_TYPE(fp,FKL_IS_FP,exe);
		CHECK_FP_OPEN(fp,exe);
		if(fflush(FKL_VM_FP(fp)->fp))
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE,exe);
	}
	else if(fflush(NULL))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int fs_fclerr(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(fp,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(fp,FKL_IS_FP,exe);
	CHECK_FP_OPEN(fp,exe);
	clearerr(FKL_VM_FP(fp)->fp);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int fs_fprintf(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(fp,fmt_obj,exe);
	FKL_CHECK_TYPE(fp,FKL_IS_FP,exe);
	FKL_CHECK_TYPE(fmt_obj,FKL_IS_STR,exe);
	CHECK_FP_OPEN(fp,exe);
	CHECK_FP_WRITABLE(fp,exe);

	uint64_t len=0;
	FklBuiltinErrorType err_type=fklVMprintf(exe
			,FKL_VM_FP(fp)->fp
			,FKL_VM_STR(fmt_obj)
			,&len);
	if(err_type)
		FKL_RAISE_BUILTIN_ERROR(err_type,exe);

	FKL_CHECK_REST_ARG(exe);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMuint(len,exe));
	return 0;
}

static int fs_fileno(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(vfp,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(vfp,FKL_IS_FP,exe);
	FklVMfp* fp=FKL_VM_FP(vfp);
	int fileno=fklVMfpFileno(fp);
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(fileno));
	return 0;
}

struct SymFunc
{
	const char* sym;
	FklVMcFunc f;
}exports_and_func[]=
{
	{"facce?",   fs_facce_p,  },
	{"freg?",    fs_freg_p,   },
	{"fdir?",    fs_fdir_p,   },
	{"freopen",  fs_freopen,  },
	{"realpath", fs_realpath, },
	{"relpath",  fs_relpath,  },
	{"mkdir",    fs_mkdir,    },
	{"fprint",   fs_fprint,   },
	{"fprintln", fs_fprintln, },
	{"fprin1",   fs_fprin1,   },
	{"fprin1n",  fs_fprin1n,  },
	{"fwrite",   fs_fwrite,   },
	{"fflush",   fs_fflush,   },
	{"fclerr",   fs_fclerr,   },
	{"fprintf",  fs_fprintf,  },
	{"fileno",  fs_fileno,  },
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
