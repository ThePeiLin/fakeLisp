#include<fakeLisp/reader.h>
#include<fakeLisp/syntax.h>
#include<fakeLisp/compiler.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/parser.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/ast.h>
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

void runRepl(FklInterpreter*);
void runRepl1(FklCodegen*);
FklByteCode* loadByteCode(FILE*);
void loadSymbolTable(FILE*);
FklLineNumberTable* loadLineNumberTable(FILE*);
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
	if(argc==1||fklIsscript(filename))
	{
		FILE* fp=(argc>1)?fopen(argv[1],"r"):stdin;
		if(fp==NULL)
		{
			perror(filename);
			fklFreeCwd();
			return EXIT_FAILURE;
		}
		FklInterpreter* inter=NULL;
		if(filename)
			fklAddSymbolToGlobCstr(filename);
		fklInitVMargs(argc,argv);
		if(fp==stdin)
		{
			fklSetMainFileRealPathWithCwd();
//			inter=fklNewIntpr(NULL,fp,NULL,NULL);
//			fklInitGlobKeyWord(inter->glob);
//			runRepl(inter);
			fklInitCodegen();
			fklInitLexer();
			FklCodegen codegen={
				.filename=NULL,
				.realpath=NULL,
				.curDir=getcwd(NULL,0),
				.file=stdin,
				.curline=1,
				.globalEnv=fklNewCodegenEnv(NULL),
				.globalSymTable=fklGetGlobSymbolTable(),
				.fid=0,
				.prev=NULL};
			runRepl1(&codegen);
		}
		else
		{
			char* rp=fklRealpath(filename);
			fklSetMainFileRealPath(rp);
			int state;
			inter=fklNewIntpr(rp,fp,NULL,NULL);
			fklInitGlobKeyWord(inter->glob);
			free(rp);
			FklByteCodelnt* mainByteCode=fklCompileFile(inter,&state);
			if(mainByteCode==NULL)
			{
				fklFreeIntpr(inter);
				fklUninitPreprocess();
				fklFreeGlobSymbolTable();
				fklFreeMainFileRealPath();
				fklFreeCwd();
				return state;
			}
			chdir(fklGetCwd());
			fklPrintUndefinedSymbol(mainByteCode);
			inter->lnt->num=mainByteCode->ls;
			inter->lnt->list=mainByteCode->l;
			FklVM* anotherVM=fklNewVM(mainByteCode->bc);
			FklVMvalue* globEnv=fklNewVMvalueNoGC(FKL_TYPE_ENV,fklNewGlobVMenv(FKL_VM_NIL,anotherVM->gc),anotherVM->gc);
			fklFreeByteCode(mainByteCode->bc);
			free(mainByteCode);
			FklVMframe* mainframe=anotherVM->frames;
			mainframe->localenv=globEnv;
			anotherVM->callback=errorCallBack;
			anotherVM->lnt=inter->lnt;
//			fklInitGlobEnv(globEnv->u.env,anotherVM->gc);
			if(setjmp(buf)==0)
			{
				fklRunVM(anotherVM);
				fklWaitGC(anotherVM->gc);
				fklJoinAllThread(NULL);
				fklFreeIntpr(inter);
				fklUninitPreprocess();
				fklFreeVMgc(anotherVM->gc);
				fklFreeGlobSymbolTable();
				fklFreeAllVMs();
			}
			else
			{
				fklDeleteCallChain(anotherVM);
				fklCancelAllThread();
				fklFreeIntpr(inter);
				fklUninitPreprocess();
				fklFreeVMgc(anotherVM->gc);
				fklFreeAllVMs();
				fklFreeMainFileRealPath();
				fklFreeCwd();
				fklFreeGlobSymbolTable();
				return exitState;
			}
		}
	}
	else if(fklIscode(filename))
	{
		FILE* fp=fopen(argv[1],"rb");
		if(fp==NULL)
		{
			perror(filename);
			fklFreeCwd();
			return EXIT_FAILURE;
		}
		loadSymbolTable(fp);
		char* rp=fklRealpath(filename);
		fklSetMainFileRealPath(rp);
		free(rp);
		FklLineNumberTable* lnt=loadLineNumberTable(fp);
		FklByteCode* mainCode=loadByteCode(fp);
		FklVM* anotherVM=fklNewVM(mainCode);
		FklVMgc* gc=anotherVM->gc;
		fklFreeByteCode(mainCode);
		fclose(fp);
		FklVMframe* mainframe=anotherVM->frames;
		FklVMvalue* globEnv=fklNewVMvalueNoGC(FKL_TYPE_ENV,fklNewGlobVMenv(FKL_VM_NIL,anotherVM->gc),anotherVM->gc);
		mainframe->localenv=globEnv;
		anotherVM->callback=errorCallBack;
		anotherVM->lnt=lnt;
//		fklInitGlobEnv(globEnv->u.env,anotherVM->gc);
		if(!setjmp(buf))
		{
			fklRunVM(anotherVM);
			fklJoinAllThread(NULL);
			fklFreeVMgc(gc);
			fklFreeGlobSymbolTable();
			fklFreeAllVMs();
			fklFreeLineNumberTable(lnt);
		}
		else
		{
			fklDeleteCallChain(anotherVM);
			fklCancelAllThread();
			fklFreeAllVMs();
			fklFreeVMgc(gc);
			fklFreeGlobSymbolTable();
			fklFreeLineNumberTable(lnt);
			fklFreeCwd();
			return exitState;
		}
	}
	else
	{
		fprintf(stderr,"%s: It is not a correct file.\n",filename);
		fklFreeCwd();
		return EXIT_FAILURE;
	}
	fklFreeMainFileRealPath();
	fklFreeCwd();
	return exitState;
}

void runRepl(FklInterpreter* inter)
{
	int e=0;
	FklVM* anotherVM=fklNewVM(NULL);
	FklVMvalue* globEnv=fklNewVMvalueNoGC(FKL_TYPE_ENV,fklNewGlobVMenv(FKL_VM_NIL,anotherVM->gc),anotherVM->gc);
	anotherVM->callback=errorCallBack;
	anotherVM->lnt=inter->lnt;
	FklByteCode* rawProcList=NULL;
	FklPtrStack* tokenStack=fklNewPtrStack(32,16);
	char* prev=NULL;
	size_t prevSize=0;
	int32_t bs=0;
	for(;e<2;)
	{
		FklAstCptr* begin=NULL;
		if(inter->file==stdin)printf(">>>");
		if(prev)
			fwrite(prev,sizeof(char),prevSize,stdout);
		int unexpectEOF=0;
		size_t size=0;
		char* list=fklReadInStringPattern(inter->file,&prev,&size,&prevSize,inter->curline,&unexpectEOF,tokenStack,NULL);
		FklErrorState state={0,NULL};
		if(unexpectEOF)
		{
			switch(unexpectEOF)
			{
					case 1:
						fprintf(stderr,"error of reader:Unexpect EOF at line %d\n",inter->curline);
						break;
					case 2:
						fprintf(stderr,"error of reader:Invalid expression at line %d\n",inter->curline);
						break;
				}
			free(list);
			list=NULL;
			continue;
		}
		begin=fklCreateAstWithTokens(tokenStack,inter->filename,inter->glob);
		inter->curline+=fklCountChar(list,'\n',size);
		while(!fklIsPtrStackEmpty(tokenStack))
			fklFreeToken(fklPopPtrStack(tokenStack));
		if(fklIsAllSpaceBufSize(list,size))
		{
			free(list);
			break;
		}
		if(begin!=NULL)
		{
			FklByteCodelnt* tmpByteCode=fklCompile(begin,inter->glob,inter,&state);
			if(state.state!=0)
			{
				fklPrintCompileError(state.place,state.state,inter);
				fklDeleteCptr(state.place);
				if(inter->file!=stdin)
				{
					fklDeleteCptr(begin);
					fklFreeGlobSymbolTable();
					exit(0);
				}
			}
			else if(tmpByteCode)
			{
				fklLntCat(inter->lnt,bs,tmpByteCode->l,tmpByteCode->ls);
				FklByteCode byteCodeOfVM={anotherVM->size,anotherVM->code};
				fklCodeCat(&byteCodeOfVM,tmpByteCode->bc);
				anotherVM->code=byteCodeOfVM.code;
				anotherVM->size=byteCodeOfVM.size;
				FklVMproc* tmp=fklNewVMproc(bs,tmpByteCode->bc->size);
				bs+=tmpByteCode->bc->size;
				fklFreeByteCodelnt(tmpByteCode);
				tmp->prevEnv=NULL;
				FklVMframe* mainframe=fklNewVMframe(tmp,anotherVM->frames);
				mainframe->localenv=globEnv;
				anotherVM->frames=mainframe;
				if(!(e=setjmp(buf)))
				{
					fklRunVM(anotherVM);
					FklVMstack* stack=anotherVM->stack;
					if(inter->file==stdin&&stack->tp!=0)
					{
						printf(";=>");
						fklDBG_printVMstack(stack,stdout,0);
					}
					fklWaitGC(anotherVM->gc);
					free(anotherVM->frames);
					anotherVM->frames=NULL;
					stack->tp=0;
					fklFreeVMproc(tmp);
				}
				else
				{
					if(e>=2&&prev)
						free(prev);
					FklVMstack* stack=anotherVM->stack;
					stack->tp=0;
					stack->bp=0;
					tmp->prevEnv=NULL;
					fklFreeVMproc(tmp);
					fklDeleteCallChain(anotherVM);
				}
			}
			free(list);
			list=NULL;
			fklDeleteCptr(begin);
			free(begin);
			begin=NULL;
		}
		else
		{
			fprintf(stderr,"error of reader:Invalid expression at line %d\n",inter->curline);
			if(list!=NULL)
				free(list);
		}
	}
	fklJoinAllThread(NULL);
	fklFreePtrStack(tokenStack);
	free(rawProcList);
	fklFreeIntpr(inter);
	fklUninitPreprocess();
	fklFreeVMgc(anotherVM->gc);
	fklFreeAllVMs();
	fklFreeGlobSymbolTable();
}

void runRepl1(FklCodegen* codegen)
{
	int e=0;
	FklVM* anotherVM=fklNewVM(NULL);
	FklVMvalue* globEnv=fklNewVMvalueNoGC(FKL_TYPE_ENV,fklNewGlobVMenv(FKL_VM_NIL,anotherVM->gc),anotherVM->gc);
	anotherVM->callback=errorCallBack;
	FklByteCode* rawProcList=NULL;
	FklPtrStack* tokenStack=fklNewPtrStack(32,16);
	FklLineNumberTable* globalLnt=fklNewLineNumTable();
	char* prev=NULL;
	size_t prevSize=0;
	int32_t bs=0;
	for(;e<2;)
	{
		FklNastNode* begin=NULL;
		printf(">>>");
		if(prev)
			fwrite(prev,sizeof(char),prevSize,stdout);
		int unexpectEOF=0;
		size_t size=0;
		char* list=fklReadInStringPattern(codegen->file,&prev,&size,&prevSize,codegen->curline,&unexpectEOF,tokenStack,NULL);
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
		begin=fklNewNastNodeFromTokenStack(tokenStack);
		codegen->curline+=fklCountChar(list,'\n',size);
		while(!fklIsPtrStackEmpty(tokenStack))
			fklFreeToken(fklPopPtrStack(tokenStack));
		if(fklIsAllSpaceBufSize(list,size))
		{
			free(list);
			break;
		}
		if(begin!=NULL)
		{
			FklByteCodelnt* tmpByteCode=fklGenExpressionCode(begin,codegen->globalEnv,codegen);
			if(tmpByteCode)
			{
				fklLntCat(globalLnt,bs,tmpByteCode->l,tmpByteCode->ls);
				FklByteCode byteCodeOfVM={anotherVM->size,anotherVM->code};
				fklCodeCat(&byteCodeOfVM,tmpByteCode->bc);
				anotherVM->code=byteCodeOfVM.code;
				anotherVM->size=byteCodeOfVM.size;
				FklVMproc* tmp=fklNewVMproc(bs,tmpByteCode->bc->size);
				bs+=tmpByteCode->bc->size;
				fklFreeByteCodelnt(tmpByteCode);
				tmp->prevEnv=NULL;
				FklVMframe* mainframe=fklNewVMframe(tmp,anotherVM->frames);
				mainframe->localenv=globEnv;
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
					fklFreeVMproc(tmp);
				}
				else
				{
					if(e>=2&&prev)
						free(prev);
					FklVMstack* stack=anotherVM->stack;
					stack->tp=0;
					stack->bp=0;
					tmp->prevEnv=NULL;
					fklFreeVMproc(tmp);
					fklDeleteCallChain(anotherVM);
				}
			}
			free(list);
			list=NULL;
			free(begin);
			begin=NULL;
		}
		else
		{
			fprintf(stderr,"error of reader:Invalid expression at line %ld\n",codegen->curline);
			if(list!=NULL)
				free(list);
		}
	}
	fklJoinAllThread(NULL);
	fklFreePtrStack(tokenStack);
	free(rawProcList);
	fklUninitPreprocess();
	fklFreeVMgc(anotherVM->gc);
	fklFreeAllVMs();
	fklFreeGlobSymbolTable();
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
		FklString* buf=fklNewString(len,NULL);
		fread(buf->str,len,1,fp);
		fklAddSymbolToGlob(buf);
		free(buf);
	}
}

FklLineNumberTable* loadLineNumberTable(FILE* fp)
{
	uint32_t size=0;
	uint32_t i=0;
	fread(&size,sizeof(uint32_t),1,fp);
	FklLineNumTabNode** list=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*size);
	FKL_ASSERT(list);
	for(;i<size;i++)
	{
		FklSid_t fid=0;
		uint64_t scp=0;
		uint64_t cpc=0;
		uint32_t line=0;
		fread(&fid,sizeof(fid),1,fp);
		fread(&scp,sizeof(scp),1,fp);
		fread(&cpc,sizeof(cpc),1,fp);
		fread(&line,sizeof(line),1,fp);
		FklLineNumTabNode* n=fklNewLineNumTabNode(fid,scp,cpc,line);
		list[i]=n;
	}
	FklLineNumberTable* lnt=fklNewLineNumTable();
	lnt->list=list;
	lnt->num=size;
	return lnt;
}
