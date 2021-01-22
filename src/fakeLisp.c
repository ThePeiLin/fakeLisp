#include"fakeLisp.h"
#include"reader.h"
#include"tool.h"
#include"VMtool.h"
#include"syntax.h"
#include"compiler.h"
#include"fakeVM.h"
#include"opcode.h"
#include"ast.h"
#include<stdio.h>
#include<stdlib.h>
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
	char* filename=argv[1];
	if(argc==1||isscript(filename))
	{
		FILE* fp=(argc>1)?fopen(argv[1],"r"):stdin;
		if(fp==NULL)
		{
			perror(filename);
			return EXIT_FAILURE;
		}
		Intpr* inter=newIntpr(((fp==stdin)?"stdin":argv[1]),fp,NULL);
		if(fp==stdin)
			runIntpr(inter);
		else
		{
			initPreprocess();
			VMenv* globEnv=newVMenv(0,NULL);
			ByteCode* mainByteCode=compileFile(inter);
			ByteCode* rawProcList=castRawproc(NULL,inter->procs);
			FakeVM* anotherVM=newFakeVM(mainByteCode,rawProcList);
			freeByteCode(mainByteCode);
			anotherVM->tid=pthread_self();
			anotherVM->mainproc->localenv=globEnv;
			anotherVM->mainproc->code->localenv=globEnv;
			anotherVM->modules=inter->modules;
			anotherVM->callback=errorCallBack;
			initGlobEnv(globEnv,anotherVM->heap);
			if(!setjmp(buf))
			{
				runFakeVM(anotherVM);
				joinAllThread();
				free(rawProcList);
				freeIntpr(inter);
				anotherVM->modules=NULL;
				unInitPreprocess();
				freeVMheap(anotherVM->heap);
				freeAllVMs();
			}
			else
			{
				deleteCallChain(anotherVM);
				joinAllThread();
				free(rawProcList);
				freeIntpr(inter);
				anotherVM->modules=NULL;
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
		ByteCode* RawProcess=loadRawproc(fp,&num);
		ByteCode* mainprocess=loadByteCode(fp);
		//printByteCode(mainprocess,stdout);
		FakeVM* anotherVM=newFakeVM(mainprocess,RawProcess);
		VMheap* heap=anotherVM->heap;
		freeByteCode(mainprocess);
		loadAllModules(fp,&anotherVM->modules);
		fclose(fp);
		VMenv* globEnv=newVMenv(0,NULL);
		anotherVM->mainproc->localenv=globEnv;
		anotherVM->mainproc->code->localenv=globEnv;
		anotherVM->callback=errorCallBack;
		initGlobEnv(globEnv,anotherVM->heap);
		if(!setjmp(buf))
		{
			runFakeVM(anotherVM);
			joinAllThread();
			freeAllVMs();
			freeVMheap(heap);
			freeRawProc(RawProcess,num);
		}
		else
		{
			deleteCallChain(anotherVM);
			joinAllThread();
			freeAllVMs();
			freeVMheap(heap);
			freeRawProc(RawProcess,num);
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
	VMenv* globEnv=newVMenv(0,NULL);
	anotherVM->mainproc->localenv=globEnv;
	anotherVM->tid=pthread_self();
	anotherVM->callback=errorCallBack;
	initGlobEnv(globEnv,anotherVM->heap);
	ByteCode* rawProcList=NULL;
	char* prev=NULL;
	for(;!e;)
	{
		AST_cptr* begin=NULL;
		if(inter->file==stdin)printf(">>>");
		StringMatchPattern* tmpPattern=NULL;
		char* list=readInPattern(inter->file,&tmpPattern,&prev);
		//	printf("%s\n==================\n",list);
		ErrorStatus status={0,NULL};
		begin=createTree(list,inter,tmpPattern);
		//	printList(begin,stderr);
		//	printf("\n==============\n");
		int ch=getc(inter->file);
		if(ch==EOF)
		{
			if(!list)
			{
				free(list);
				break;
			}
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
				anotherVM->modules=inter->modules;
			}
			else
			{
				//	printList(begin,stdout);
				//	putchar('\n');
				ByteCode* tmpByteCode=compile(begin,inter->glob,inter,&status);
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
				else
				{
					//printByteCode(tmpByteCode,stderr);
					rawProcList=castRawproc(rawProcList,inter->procs);
					anotherVM->procs=rawProcList;
					VMcode* tmp=newVMcode(tmpByteCode);
					tmp->localenv=globEnv;
					anotherVM->mainproc->code=tmp;
					anotherVM->mainproc->localenv=globEnv;
					anotherVM->mainproc->cp=0;
					anotherVM->curproc=anotherVM->mainproc;
					if(!(e=setjmp(buf)))
					{
						runFakeVM(anotherVM);
						VMstack* stack=anotherVM->stack;
						if(inter->file==stdin&&stack->tp!=0)
						{
							printf("]=>");
							printAllStack(stack,stdout);
						}
						//fprintf(stderr,"======\n");
						//fprintf(stderr,"stack->tp=%d\n",stack->tp);
						//printAllStack(stack,stderr);
						stack->tp=0;
						freeByteCode(tmpByteCode);
						tmp->localenv=NULL;
						freeVMcode(tmp);
						anotherVM->mainproc->code=NULL;
					}
					else
					{
						VMstack* stack=anotherVM->stack;
						stack->tp=0;
						stack->bp=0;
						freeByteCode(tmpByteCode);
						tmp->localenv=NULL;
						freeVMcode(tmp);
						anotherVM->mainproc->code=NULL;
						deleteCallChain(anotherVM);
					}
				}
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
	anotherVM->modules=NULL;
	unInitPreprocess();
	freeVMheap(anotherVM->heap);
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
