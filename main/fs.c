#include<fakeLisp/vm.h>

static int fs_facce_p(FKL_CPROC_ARGL)
{
	static const char Pname[]="fs.facce?";
	FKL_DECL_AND_CHECK_ARG(filename,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(filename,FKL_IS_STR,Pname,exe);
	FKL_VM_PUSH_VALUE(exe,fklIsAccessibleRegFile(FKL_VM_STR(filename)->str)
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int fs_freg_p(FKL_CPROC_ARGL)
{
	static const char Pname[]="fs.freg?";
	FKL_DECL_AND_CHECK_ARG(filename,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(filename,FKL_IS_STR,Pname,exe);
	FKL_VM_PUSH_VALUE(exe
			,fklIsAccessibleRegFile(FKL_VM_STR(filename)->str)
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int fs_fdir_p(FKL_CPROC_ARGL)
{
	static const char Pname[]="fs.fdir?";
	FKL_DECL_AND_CHECK_ARG(filename,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(filename,FKL_IS_STR,Pname,exe);
	FKL_VM_PUSH_VALUE(exe
			,fklIsAccessibleDirectory(FKL_VM_STR(filename)->str)
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int fs_freopen(FKL_CPROC_ARGL)
{
	static const char Pname[]="fs.freopen";
	FKL_DECL_AND_CHECK_ARG2(stream,filename,Pname);
	FklVMvalue* mode=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	if(!FKL_IS_FP(stream)||!FKL_IS_STR(filename)||(mode&&!FKL_IS_STR(mode)))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklVMfp* vfp=FKL_VM_FP(stream);
	FklString* filenameStr=FKL_VM_STR(filename);
	const char* modeStr=mode?FKL_VM_STR(mode)->str:"r";
	FILE* fp=freopen(filenameStr->str,modeStr,vfp->fp);
	if(!fp)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR(Pname,filenameStr->str,0,FKL_ERR_FILEFAILURE,exe);
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
	static const char Pname[]="fs.realpath";
	FKL_DECL_AND_CHECK_ARG(filename,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(filename,FKL_IS_STR,Pname,exe);
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
	static const char Pname[]="fs.relpath";
	FKL_DECL_AND_CHECK_ARG(path,Pname);
	FklVMvalue* start=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(path,FKL_IS_STR,Pname,exe);

	if(start)
		FKL_CHECK_TYPE(start,FKL_IS_STR,Pname,exe);

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
