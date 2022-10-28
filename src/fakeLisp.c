#include<fakeLisp/reader.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/parser.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/vm.h>
#include<fakeLisp/lexer.h>
#include<fakeLisp/codegen.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include<pthread.h>
#include<setjmp.h>
#ifdef _WIN32
#include<io.h>
#include<process.h>
#else
#include<unistd.h>
#endif

void runRepl(FklCodegen*,const FklSid_t*);
FklByteCode* loadByteCode(FILE*);
void loadSymbolTable(FILE*);
void loadLineNumberTable(FILE*,FklLineNumTabNode** plist,size_t* pnum);
static jmp_buf buf;
static int exitState=0;

static void errorCallBack(void* a)
{
	int* i=(int*)a;
	exitState=255;
	longjmp(buf,i[(sizeof(void*)*2)/sizeof(int)]);
}

int main(int argc,char** argv)
{
	char* filename=(argc>1)?argv[1]:NULL;
	char* cwd=getcwd(NULL,0);
	fklSetCwd(cwd);
	free(cwd);
	if(!filename)
	{
		fklInitVMargs(argc,argv);
		fklSetMainFileRealPathWithCwd();
		FklCodegen codegen={.fid=0,};
		const FklSid_t* builtInHeadSymbolTable=fklInitCodegen();
		//fklInitLexer();
		fklInitGlobalCodegener(&codegen,NULL,NULL,fklGetGlobSymbolTable(),0);
		runRepl(&codegen,builtInHeadSymbolTable);
		codegen.globalSymTable=NULL;
		FklPtrStack* loadedLibStack=codegen.loadedLibStack;
		while(!fklIsPtrStackEmpty(loadedLibStack))
		{
			FklCodegenLib* lib=fklPopPtrStack(loadedLibStack);
			free(lib->rp);
			free(lib);
		}
		fklUninitCodegener(&codegen);
		fklUninitCodegen();
	}
	else if(fklIsscript(filename))
	{
		if(access(filename,R_OK))
		{
			perror(filename);
			fklDestroyCwd();
			return EXIT_FAILURE;
		}
		FILE* fp=fopen(filename,"r");
		if(fp==NULL)
		{
			perror(filename);
			fklDestroyCwd();
			return EXIT_FAILURE;
		}
		fklAddSymbolToGlobCstr(filename);
		fklInitCodegen();
		//fklInitLexer();
		FklCodegen codegen={.fid=0,};
		char* rp=fklRealpath(filename);
		fklSetMainFileRealPath(rp);
		fklInitGlobalCodegener(&codegen,rp,NULL,fklCreateSymbolTable(),0);
		free(rp);
		FklByteCodelnt* mainByteCode=fklGenExpressionCodeWithFp(fp,&codegen);
		if(mainByteCode==NULL)
		{
			fklUninitCodegener(&codegen);
			fklUninitCodegen();
			fklDestroyGlobSymbolTable();
			fklDestroyMainFileRealPath();
			fklDestroyCwd();
			return 1;
		}
		FklSymbolTable* globalSymbolTable=fklExchangeGlobSymbolTable(codegen.globalSymTable);
		fklDestroySymbolTable(globalSymbolTable);
		chdir(fklGetCwd());
		fklCodegenPrintUndefinedSymbol(mainByteCode,codegen.globalSymTable);
		FklVM* anotherVM=fklCreateVM(mainByteCode,NULL,NULL);
		FklVMvalue* globEnv=fklCreateVMvalueNoGC(FKL_TYPE_ENV,fklCreateGlobVMenv(FKL_VM_NIL,anotherVM->gc),anotherVM->gc);
		FklPtrStack* loadedLibStack=codegen.loadedLibStack;
		anotherVM->libNum=codegen.loadedLibStack->top;
		anotherVM->libs=(FklVMlib*)malloc(sizeof(FklVMlib)*loadedLibStack->top);
		FKL_ASSERT(anotherVM->libs);
		while(!fklIsPtrStackEmpty(loadedLibStack))
		{
			FklCodegenLib* cur=fklPopPtrStack(loadedLibStack);
			FklVMvalue* codeObj=fklCreateVMvalueNoGC(FKL_TYPE_CODE_OBJ,cur->bcl,anotherVM->gc);
			FklVMvalue* proc=fklCreateVMvalueNoGC(FKL_TYPE_PROC,fklCreateVMproc(0,cur->bcl->bc->size,codeObj,anotherVM->gc),anotherVM->gc);
			fklSetRef(&proc->u.proc->prevEnv,globEnv,anotherVM->gc);
			fklInitVMlib(&anotherVM->libs[loadedLibStack->top],cur->exportNum,cur->exports,proc);
			free(cur->rp);
			free(cur);
		}
		FklVMvalue* mainEnv=fklCreateVMvalueNoGC(FKL_TYPE_ENV,fklCreateVMenv(globEnv,anotherVM->gc),anotherVM->gc);
		FklVMframe* mainframe=anotherVM->frames;
		mainframe->localenv=mainEnv;
		anotherVM->callback=errorCallBack;
		if(setjmp(buf)==0)
		{
			fklRunVM(anotherVM);
			fklWaitGC(anotherVM->gc);
			fklJoinAllThread(anotherVM);
			fklDestroyVMgc(anotherVM->gc);
			fklUninitCodegener(&codegen);
			fklDestroyGlobSymbolTable();
			fklUninitCodegen();
			fklDestroyAllVMs(anotherVM);
		}
		else
		{
			fklDeleteCallChain(anotherVM);
			fklCancelAllThread();
			fklDestroyVMgc(anotherVM->gc);
			fklDestroyAllVMs(anotherVM);
			fklDestroyMainFileRealPath();
			fklDestroyCwd();
			fklUninitCodegener(&codegen);
			fklDestroyGlobSymbolTable();
			fklUninitCodegen();
			return exitState;
		}
	}
	else if(fklIscode(filename))
	{
		if(access(filename,R_OK))
		{
			perror(filename);
			fklDestroyCwd();
			return EXIT_FAILURE;
		}
		FILE* fp=fopen(argv[1],"rb");
		if(fp==NULL)
		{
			perror(filename);
			fklDestroyCwd();
			return EXIT_FAILURE;
		}
		loadSymbolTable(fp);
		char* rp=fklRealpath(filename);
		fklSetMainFileRealPath(rp);
		free(rp);
		FklByteCodelnt* mainCodelnt=fklCreateByteCodelnt(NULL);
		loadLineNumberTable(fp,&mainCodelnt->l,&mainCodelnt->ls);
		FklByteCode* mainCode=loadByteCode(fp);
		mainCodelnt->bc=mainCode;
		FklVM* anotherVM=fklCreateVM(mainCodelnt,NULL,NULL);
		FklVMgc* gc=anotherVM->gc;
		fclose(fp);
		FklVMframe* mainframe=anotherVM->frames;
		FklVMvalue* globEnv=fklCreateVMvalueNoGC(FKL_TYPE_ENV,fklCreateGlobVMenv(FKL_VM_NIL,anotherVM->gc),anotherVM->gc);
		FklVMvalue* mainEnv=fklCreateVMvalueNoGC(FKL_TYPE_ENV,fklCreateVMenv(globEnv,anotherVM->gc),anotherVM->gc);
		mainframe->localenv=mainEnv;
		anotherVM->callback=errorCallBack;
		if(!setjmp(buf))
		{
			fklRunVM(anotherVM);
			fklJoinAllThread(anotherVM);
			fklDestroyVMgc(gc);
			fklDestroyGlobSymbolTable();
			fklDestroyAllVMs(anotherVM);
		}
		else
		{
			fklDeleteCallChain(anotherVM);
			fklCancelAllThread();
			fklDestroyAllVMs(anotherVM);
			fklDestroyVMgc(gc);
			fklDestroyGlobSymbolTable();
			fklDestroyCwd();
			return exitState;
		}
	}
	else
	{
		fprintf(stderr,"%s: It is not a correct file.\n",filename);
		fklDestroyCwd();
		return EXIT_FAILURE;
	}
	fklDestroyMainFileRealPath();
	fklDestroyCwd();
	return exitState;
}

void runRepl(FklCodegen* codegen,const FklSid_t* builtInHeadSymbolTable)
{
	int e=0;
	FklVM* anotherVM=fklCreateVM(NULL,NULL,NULL);
	FklVMvalue* globEnv=fklCreateVMvalueNoGC(FKL_TYPE_ENV,fklCreateGlobVMenv(FKL_VM_NIL,anotherVM->gc),anotherVM->gc);
	anotherVM->callback=errorCallBack;
	FklByteCode* rawProcList=NULL;
	FklPtrStack* tokenStack=fklCreatePtrStack(32,16);
	FklLineNumberTable* globalLnt=fklCreateLineNumTable();
	anotherVM->codeObj=fklCreateVMvalueNoGC(FKL_TYPE_CODE_OBJ,fklCreateByteCodelnt(fklCreateByteCode(0)),anotherVM->gc);
	char* prev=NULL;
	size_t prevSize=0;
	int32_t bs=0;
	FklVMvalue* mainEnv=fklCreateSaveVMvalue(FKL_TYPE_ENV,fklCreateVMenv(globEnv,anotherVM->gc));
	FklCodegenEnv* mainCodegenEnv=fklCreateCodegenEnv(codegen->globalEnv);
	mainCodegenEnv->refcount=1;
	size_t libNum=codegen->loadedLibStack->top;
	for(;e<2;)
	{
		FklNastNode* begin=NULL;
		printf(">>>");
		if(prev)
			fwrite(prev,sizeof(char),prevSize,stdout);
		int unexpectEOF=0;
		size_t size=0;
		char* list=fklReadInStringPattern(stdin,&prev,&size,&prevSize,codegen->curline,&unexpectEOF,tokenStack,NULL);
		if(unexpectEOF)
		{
			switch(unexpectEOF)
			{
				case 1:
					fprintf(stderr,"error of reader:Unexpect EOF at line %lu\n",codegen->curline);
					break;
				case 2:
					fprintf(stderr,"error of reader:Invalid expression at line %lu\n",codegen->curline);
					break;
			}
			free(list);
			list=NULL;
			continue;
		}
		size_t errorLine=0;
		begin=fklCreateNastNodeFromTokenStack(tokenStack,&errorLine,builtInHeadSymbolTable);
		codegen->curline+=fklCountChar(list,'\n',size);
		free(list);
		if(fklIsPtrStackEmpty(tokenStack))
			break;
		while(!fklIsPtrStackEmpty(tokenStack))
			fklDestroyToken(fklPopPtrStack(tokenStack));
		if(!begin)
			fprintf(stderr,"error of reader:Invalid expression at line %lu\n",errorLine);
		else
		{
			fklMakeNastNodeRef(begin);
			FklByteCodelnt* tmpByteCode=fklGenExpressionCode(begin,mainCodegenEnv,codegen);
			if(tmpByteCode)
			{
				size_t unloadlibNum=codegen->loadedLibStack->top-libNum;
				if(unloadlibNum)
				{
					libNum+=unloadlibNum;
					FklVMlib* nlibs=(FklVMlib*)malloc(sizeof(FklVMlib)*libNum);
					FKL_ASSERT(nlibs);
					memcpy(nlibs,anotherVM->libs,sizeof(FklVMlib)*anotherVM->libNum);
					for(size_t i=anotherVM->libNum;i<libNum;i++)
					{
						FklVMlib* curVMlib=&nlibs[i];
						FklCodegenLib* curCGlib=codegen->loadedLibStack->base[i];
						FklVMvalue* codeObj=fklCreateVMvalueNoGC(FKL_TYPE_CODE_OBJ,curCGlib->bcl,anotherVM->gc);
						FklVMvalue* proc=fklCreateVMvalueNoGC(FKL_TYPE_PROC,fklCreateVMproc(0,curCGlib->bcl->bc->size,codeObj,anotherVM->gc),anotherVM->gc);
						fklSetRef(&proc->u.proc->prevEnv,globEnv,anotherVM->gc);
						fklInitVMlib(curVMlib,curCGlib->exportNum,curCGlib->exports,proc);
					}
					FklVMlib* prev=anotherVM->libs;
					anotherVM->libs=nlibs;
					anotherVM->libNum=libNum;
					free(prev);
				}
				fklCodeLntCat(anotherVM->codeObj->u.code,tmpByteCode);
				FklVMproc* tmp=fklCreateVMproc(bs,tmpByteCode->bc->size,anotherVM->codeObj,anotherVM->gc);
				bs+=tmpByteCode->bc->size;
				fklDestroyByteCodelnt(tmpByteCode);
				tmp->prevEnv=NULL;
				FklVMframe* mainframe=fklCreateVMframeWithProc(tmp,anotherVM->frames);
				mainframe->localenv=mainEnv;
				anotherVM->frames=mainframe;
				if(!(e=setjmp(buf)))
				{
					fklRunVM(anotherVM);
					FklVMstack* stack=anotherVM->stack;
					if(stack->tp!=0)
					{
						printf(";=>");
						fklDBG_printVMstack(stack,stdout,0);
					}
					fklWaitGC(anotherVM->gc);
					free(anotherVM->frames);
					anotherVM->frames=NULL;
					stack->tp=0;
					fklDestroyVMproc(tmp);
				}
				else
				{
					if(e>=2&&prev)
						free(prev);
					FklVMstack* stack=anotherVM->stack;
					stack->tp=0;
					stack->bp=0;
					tmp->prevEnv=NULL;
					fklDestroyVMproc(tmp);
					fklDeleteCallChain(anotherVM);
				}
			}
			fklDestroyNastNode(begin);
			begin=NULL;
		}
	}
	fklDestroyCodegenEnv(mainCodegenEnv);
	fklDestroyLineNumberTable(globalLnt);
	fklJoinAllThread(anotherVM);
	fklDestroyPtrStack(tokenStack);
	free(rawProcList);
	fklAddToGCNoGC(mainEnv,anotherVM->gc);
	fklDestroyVMgc(anotherVM->gc);
	fklDestroyAllVMs(anotherVM);
	fklDestroyGlobSymbolTable();
}

FklByteCode* loadByteCode(FILE* fp)
{
	uint64_t size=0;
	fread(&size,sizeof(uint64_t),1,fp);
	FklByteCode* tmp=(FklByteCode*)malloc(sizeof(FklByteCode));
	FKL_ASSERT(tmp);
	tmp->size=size;
	tmp->code=(uint8_t*)malloc(sizeof(uint8_t)*size);
	FKL_ASSERT(tmp->code);
	fread(tmp->code,size,1,fp);
	return tmp;
}

void loadSymbolTable(FILE* fp)
{
	uint64_t size=0;
	fread(&size,sizeof(size),1,fp);
	for(uint64_t i=0;i<size;i++)
	{
		uint64_t len=0;
		fread(&len,sizeof(len),1,fp);
		FklString* buf=fklCreateString(len,NULL);
		fread(buf->str,len,1,fp);
		fklAddSymbolToGlob(buf);
		free(buf);
	}
}

void loadLineNumberTable(FILE* fp,FklLineNumTabNode** plist,size_t* pnum)
{
	size_t size=0;
	fread(&size,sizeof(uint32_t),1,fp);
	FklLineNumTabNode* list=(FklLineNumTabNode*)malloc(sizeof(FklLineNumTabNode)*size);
	FKL_ASSERT(list);
	for(size_t i=0;i<size;i++)
	{
		FklSid_t fid=0;
		uint64_t scp=0;
		uint64_t cpc=0;
		uint32_t line=0;
		fread(&fid,sizeof(fid),1,fp);
		fread(&scp,sizeof(scp),1,fp);
		fread(&cpc,sizeof(cpc),1,fp);
		fread(&line,sizeof(line),1,fp);
		fklInitLineNumTabNode(&list[i],fid,scp,cpc,line);
	}
	*plist=list;
	*pnum=size;
}
