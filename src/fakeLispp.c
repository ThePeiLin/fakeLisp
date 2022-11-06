#include<fakeLisp/reader.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/parser.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/codegen.h>
#include<fakeLisp/vm.h>
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

static FklByteCode* loadByteCode(FILE*);
static void loadSymbolTable(FILE*);
static FklLineNumberTable* loadLineNumberTable(FILE*);
static void loadLib(FILE*,size_t* pnum,FklCodegenLib** libs);

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

FklLineNumberTable* loadLineNumberTable(FILE* fp)
{
	uint32_t size=0;
	uint32_t i=0;
	fread(&size,sizeof(uint32_t),1,fp);
	FklLineNumTabNode* list=(FklLineNumTabNode*)malloc(sizeof(FklLineNumTabNode)*size);
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
		fklInitLineNumTabNode(&list[i],fid,scp,cpc,line);
	}
	FklLineNumberTable* lnt=fklCreateLineNumTable();
	lnt->list=list;
	lnt->num=size;
	return lnt;
}

int main(int argc,char** argv)
{
	if(argc<2)
	{
		fprintf(stderr,"error:Not input file!\n");
		return EXIT_FAILURE;
	}
	char* filename=argv[1];
	if(fklIscode(filename))
	{
		if(!fklIsAccessableScriptFile(filename))
		{
			perror(filename);
			fklDestroyCwd();
			return EXIT_FAILURE;
		}
		char* cwd=getcwd(NULL,0);
		fklSetCwd(cwd);
		free(cwd);
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
		FklLineNumberTable* lnt=loadLineNumberTable(fp);
		FklByteCode* mainCode=loadByteCode(fp);
		FklByteCodelnt bytecodelnt=
		{
			.ls=lnt->num,
			.l=lnt->list,
			.bc=mainCode,
		};
		fklPrintByteCodelnt(&bytecodelnt,stdout,NULL);
		fputc('\n',stdout);
		fklDestroyByteCode(mainCode);
		FklCodegenLib* libs=NULL;
		size_t num=0;
		loadLib(fp,&num,&libs);
		for(size_t i=0;i<num;i++)
		{
			FklCodegenLib* cur=&libs[i];
			fputc('\n',stdout);
			printf("lib %lu:\n",i+1);
			printf("exports %lu\n",cur->exportNum);
			for(size_t j=0;j<cur->exportNum;j++)
			{
				FklSid_t id=cur->exports[j];
				fklPrintRawSymbol(fklGetGlobSymbolWithId(id)->symbol,stdout);
				fputc('\n',stdout);
			}
			fklPrintByteCodelnt(cur->bcl,stdout,NULL);
			fputc('\n',stdout);
		}
		fclose(fp);
		fklDestroyCwd();
		fklDestroyGlobSymbolTable();
		fklDestroyLineNumberTable(lnt);
		fklDestroyMainFileRealPath();
	}
	else
	{
		fprintf(stderr,"error:Not a correct file!\n");
		return EXIT_FAILURE;
	}
	return 0;
}

static void loadLib(FILE* fp,size_t* pnum,FklCodegenLib** plibs)
{
	fread(pnum,sizeof(uint64_t),1,fp);
	size_t num=*pnum;
	FklCodegenLib* libs=(FklCodegenLib*)malloc(sizeof(FklCodegen)*num);
	FKL_ASSERT(libs);
	*plibs=libs;
	for(size_t i=0;i<num;i++)
	{
		uint64_t exportNum=0;
		fread(&exportNum,sizeof(uint64_t),1,fp);
		FklSid_t* exports=(FklSid_t*)malloc(sizeof(FklSid_t)*exportNum);
		FKL_ASSERT(exports);
		fread(exports,sizeof(FklSid_t),exportNum,fp);
		FklByteCodelnt* bcl=fklCreateByteCodelnt(NULL);
		fklLoadLineNumberTable(fp,&bcl->l,&bcl->ls);
		FklByteCode* bc=loadByteCode(fp);
		bcl->bc=bc;
		fklInitCodegenLib(&libs[i],NULL,bcl,exportNum,exports);
	}
}
