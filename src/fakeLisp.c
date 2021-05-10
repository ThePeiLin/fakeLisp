#include"fakeLisp.h"
#include"reader.h"
#include"common.h"
#include"VMtool.h"
#include"syntax.h"
#include"compiler.h"
#include"fakeVM.h"
#include"opcode.h"
#include"ast.h"
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<ctype.h>
#include<pthread.h>
#include<setjmp.h>
static jmp_buf buf;
static int exitStatus=0;

void errorCallBack(void* a)
{
	int* i=(int*)a;
	exitStatus=i[0];
	longjmp(buf,i[1]);
}

int main(int argc,char** argv)
{
	char* filename=(argc>1)?argv[1]:NULL;
	if(argc==1||isscript(filename))
	{
		FILE* fp=(argc>1)?fopen(argv[1],"r"):stdin;
		if(fp==NULL)
		{
			perror(filename);
			return EXIT_FAILURE;
		}
		Intpr* inter=newIntpr(((fp==stdin)?"stdin":argv[1]),fp,NULL,NULL,NULL);
		if(fp==stdin)
		{
			SymTabNode* node=newSymTabNode("stdin");
			addSymTabNode(node,inter->table);
			runIntpr(inter);
		}
		else
		{
#ifdef _WIN32
			char* rp=_fullpath(NULL,filename,0);
#else
			char* rp=realpath(filename,0);
#endif
			char* workpath=getDir(rp);
			free(rp);
			initPreprocess();
			ByteCode* fix=newByteCode(0);
			int status;
			SymTabNode* node=newSymTabNode(filename);
			addSymTabNode(node,inter->table);
			ByteCodelnt* mainByteCode=compileFile(inter,1,fix,&status);
			if(mainByteCode==NULL)
			{
				free(workpath);
				freeIntpr(inter);
				freeByteCode(fix);
				unInitPreprocess();
				return status;
			}
			reCodeCat(fix,mainByteCode->bc);
			mainByteCode->l[0]->cpc+=fix->size;
			INCREASE_ALL_SCP(mainByteCode->l+1,mainByteCode->ls-1,fix->size);
			freeByteCode(fix);
			addLineNumTabId(mainByteCode->l,mainByteCode->ls,0,inter->lnt);
			VMenv* globEnv=newVMenv(NULL);
			ByteCode* rawProcList=castRawproc(NULL,inter->procs);
			FakeVM* anotherVM=newFakeVM(mainByteCode->bc,rawProcList);
			freeByteCode(mainByteCode->bc);
			free(mainByteCode);
			anotherVM->argc=argc-1;
			anotherVM->argv=argv+1;
			anotherVM->tid=pthread_self();
			anotherVM->mainproc->localenv=globEnv;
			anotherVM->mainproc->code->prevEnv=NULL;
			anotherVM->callback=errorCallBack;
			anotherVM->table=inter->table;
			anotherVM->lnt=inter->lnt;
			initGlobEnv(globEnv,anotherVM->heap,inter->table);
			chdir(workpath);
			free(workpath);
			if(setjmp(buf)==0)
			{
				runFakeVM(anotherVM);
				joinAllThread();
				free(rawProcList);
				freeIntpr(inter);
				unInitPreprocess();
				freeVMheap(anotherVM->heap);
				anotherVM->mainproc=NULL;
				freeAllVMs();
			}
			else
			{
				deleteCallChain(anotherVM);
				joinAllThread();
				free(rawProcList);
				freeIntpr(inter);
				unInitPreprocess();
				freeVMheap(anotherVM->heap);
				freeAllVMs();
				return exitStatus;
			}
		}
	}
	else if(iscode(filename))
	{
		FILE* fp=fopen(argv[1],"rb");
		if(fp==NULL)
		{
			perror(filename);
			return EXIT_FAILURE;
		}
		int32_t num=0;
		changeWorkPath(filename);
		SymbolTable* table=loadSymbolTable(fp);
		LineNumberTable* lnt=loadLineNumberTable(fp);
		ByteCode* RawProcess=loadRawproc(fp,&num);
		ByteCode* mainprocess=loadByteCode(fp);
		FakeVM* anotherVM=newFakeVM(mainprocess,RawProcess);
		VMheap* heap=anotherVM->heap;
		freeByteCode(mainprocess);
		fclose(fp);
		VMenv* globEnv=newVMenv(NULL);
		anotherVM->argc=argc-1;
		anotherVM->argv=argv+1;
		anotherVM->table=table;
		anotherVM->mainproc->localenv=globEnv;
		anotherVM->mainproc->code->prevEnv=NULL;
		anotherVM->callback=errorCallBack;
		anotherVM->lnt=lnt;
		initGlobEnv(globEnv,anotherVM->heap,table);
		if(!setjmp(buf))
		{
			runFakeVM(anotherVM);
			joinAllThread();
			freeVMheap(heap);
			freeRawProc(RawProcess,num);
			freeSymbolTable(table);
			anotherVM->mainproc=NULL;
			freeAllVMs();
			freeLineNumberTable(lnt);
		}
		else
		{
			deleteCallChain(anotherVM);
			joinAllThread();
			freeAllVMs();
			freeVMheap(heap);
			freeRawProc(RawProcess,num);
			freeSymbolTable(table);
			freeLineNumberTable(lnt);
			return exitStatus;
		}
	}
	else
	{
		fprintf(stderr,"%s: It is not a correct file.\n",filename);
		return EXIT_FAILURE;
	}
	return exitStatus;
}

void runIntpr(Intpr* inter)
{
	initPreprocess();
	int e=0;
	FakeVM* anotherVM=newFakeVM(NULL,NULL);
	VMenv* globEnv=newVMenv(NULL);
	anotherVM->table=inter->table;
	anotherVM->tid=pthread_self();
	anotherVM->callback=errorCallBack;
	anotherVM->lnt=inter->lnt;
	initGlobEnv(globEnv,anotherVM->heap,inter->table);
	ByteCode* rawProcList=NULL;
	char* prev=NULL;
	for(;e<2;)
	{
		AST_cptr* begin=NULL;
		if(inter->file==stdin)printf(">>>");
		StringMatchPattern* tmpPattern=NULL;
		char* list=readInPattern(inter->file,&tmpPattern,&prev);
		ErrorStatus status={0,NULL};
		begin=createTree(list,inter,tmpPattern);
		int ch=getc(inter->file);
		if(!begin&&(list&&!(isAllSpace(list)&&ch==EOF)))
		{
			fprintf(stderr,"In file \"%s\",line %d\n",inter->filename,inter->curline);
			if(list&&!isAllSpace(list))
				fprintf(stderr,"%s:Invalid expression here.\n",list);
			else
				fprintf(stderr,"Can't create a valid object.\n");
			free(list);
			list=NULL;
			continue;
		}
		if(ch==EOF)
		{
			if(list)
				free(list);
			break;
		}
		else if(ch!='\n')
			ungetc(ch,inter->file);
		if(begin!=NULL)
		{
			if(isPreprocess(begin))
			{
				status=eval(begin,NULL,inter);
				if(status.status!=0)
				{
					exError(status.place,status.status,inter);
					deleteCptr(status.place);
					if(inter->file!=stdin)
					{
						deleteCptr(begin);
						exit(0);
					}
				}
			}
			else
			{
				ByteCode* fix=newByteCode(0);
				ByteCodelnt* tmpByteCode=compile(begin,inter->glob,inter,&status,!isLambdaExpression(begin),fix);
				if(status.status!=0)
				{
					exError(status.place,status.status,inter);
					deleteCptr(status.place);
					if(inter->file!=stdin)
					{
						deleteCptr(begin);
						exit(0);
					}
				}
				else if(tmpByteCode)
				{
					reCodeCat(fix,tmpByteCode->bc);
					tmpByteCode->l[0]->cpc+=fix->size;
					INCREASE_ALL_SCP(tmpByteCode->l+1,tmpByteCode->ls-1,fix->size);
					addLineNumTabId(tmpByteCode->l,tmpByteCode->ls,0,inter->lnt);
					rawProcList=castRawproc(rawProcList,inter->procs);
					anotherVM->procs=rawProcList;
					VMcode* tmp=newVMcode(tmpByteCode->bc,0);
					freeByteCode(tmpByteCode->bc);
					free(tmpByteCode);
					tmp->prevEnv=NULL;
					anotherVM->mainproc=newFakeProcess(tmp,NULL);
					anotherVM->mainproc->localenv=globEnv;
					anotherVM->curproc=anotherVM->mainproc;
					globEnv->refcount+=1;
					if(!(e=setjmp(buf)))
					{
						runFakeVM(anotherVM);
						VMstack* stack=anotherVM->stack;
						if(inter->file==stdin&&stack->tp!=0)
						{
							printf(";=>");
							DBG_printVMstack(stack,stdout,0);
						}
						stack->tp=0;
						freeVMcode(tmp);
					}
					else
					{
						VMstack* stack=anotherVM->stack;
						stack->tp=0;
						stack->bp=0;
						tmp=anotherVM->mainproc->code;
						tmp->prevEnv=NULL;
						freeVMcode(tmp);
						deleteCallChain(anotherVM);
					}
				}
				freeByteCode(fix);
			}
			free(list);
			list=NULL;
			deleteCptr(begin);
			free(begin);
		}
		else
		{
			if(list!=NULL)
				free(list);
		}
		if(ch=='\n')
			ungetc(ch,inter->file);
	}
	freeVMenv(globEnv);
	joinAllThread();
	free(rawProcList);
	freeIntpr(inter);
	unInitPreprocess();
	freeVMheap(anotherVM->heap);
	anotherVM->mainproc=NULL;
	freeAllVMs();
}

ByteCode* loadRawproc(FILE* fp,int32_t* renum)
{
	int32_t num=0;
	int i=0;
	fread(&num,sizeof(int32_t),1,fp);
	*renum=num;
	ByteCode* tmp=(ByteCode*)malloc(sizeof(ByteCode)*num);
	if(tmp==NULL)
	{
		fprintf(stderr,"In file \"%s\",line %d\n",__FILE__,__LINE__);
		errors("loadRawproc",__FILE__,__LINE__);
	}
	for(;i<num;i++)
	{
		int32_t size=0;
		fread(&size,sizeof(int32_t),1,fp);
		tmp[i].size=size;
		tmp[i].code=(char*)malloc(sizeof(char)*size);
		if(tmp[i].code==NULL)
		{
			fprintf(stderr,"In file \"%s\",line %d\n",__FILE__,__LINE__);
			errors("loadRawproc",__FILE__,__LINE__);
		}
		int32_t j=0;
		for(;j<size;j++)
			tmp[i].code[j]=getc(fp);
	}
	return tmp;
}

ByteCode* loadByteCode(FILE* fp)
{
	int32_t size=0;
	int32_t i=0;
	fread(&size,sizeof(int32_t),1,fp);
	ByteCode* tmp=(ByteCode*)malloc(sizeof(ByteCode));
	if(tmp==NULL)
		errors("loadByteCode",__FILE__,__LINE__);
	tmp->size=size;
	tmp->code=(char*)malloc(sizeof(char)*size);
	if(tmp->code==NULL)
		errors("loadByteCode",__FILE__,__LINE__);
	for(;i<size;i++)
		tmp->code[i]=getc(fp);
	return tmp;
}

SymbolTable* loadSymbolTable(FILE* fp)
{
	int32_t size=0;
	int32_t i=0;
	SymbolTable* tmp=newSymbolTable();
	fread(&size,sizeof(int32_t),1,fp);
	for(;i<size;i++)
	{
		char* symbol=getStringFromFile(fp);
		SymTabNode* node=newSymTabNode(symbol);
		addSymTabNode(node,tmp);
		free(symbol);
	}
	return tmp;
}

LineNumberTable* loadLineNumberTable(FILE* fp)
{
	int32_t size=0;
	int32_t i=0;
	LineNumberTable* lnt=newLineNumTable();
	fread(&size,sizeof(int32_t),1,fp);
	lnt->size=size;
	lnt->list=(LineNumTabId**)malloc(sizeof(LineNumTabId*)*size);
	if(!lnt->list)
		errors("loadLineNumberTable",__FILE__,__LINE__);
	for(;i<lnt->size;i++)
	{
		int32_t j=0;
		int32_t id=0;
		int32_t size=0;
		LineNumTabId* idl=NULL;
		fread(&id,sizeof(id),1,fp);
		idl=newLineNumTabId(id);
		lnt->list[i]=idl;
		fread(&size,sizeof(size),1,fp);
		idl->size=size;
		idl->list=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*size);
		if(!idl->list)
			errors("loadLineNumberTable",__FILE__,__LINE__);
		for(;j<size;j++)
		{
			int32_t fid=0;
			int32_t scp=0;
			int32_t cpc=0;
			int32_t line=0;
			fread(&fid,sizeof(fid),1,fp);
			fread(&scp,sizeof(scp),1,fp);
			fread(&cpc,sizeof(cpc),1,fp);
			fread(&line,sizeof(line),1,fp);
			LineNumTabNode* n=newLineNumTabNode(fid,scp,cpc,line);
			idl->list[j]=n;
		}
	}
	return lnt;
}
