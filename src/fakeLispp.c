#include<fakeLisp/reader.h>
#include<fakeLisp/syntax.h>
#include<fakeLisp/compiler.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/parser.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/ast.h>
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

FklByteCode* loadByteCode(FILE*);
void loadSymbolTable(FILE*);
FklLineNumberTable* loadLineNumberTable(FILE*);

FklByteCode* loadByteCode(FILE* fp)
{
	uint64_t size=0;
	fread(&size,sizeof(uint64_t),1,fp);
	FklByteCode* tmp=(FklByteCode*)malloc(sizeof(FklByteCode));
	FKL_ASSERT(tmp,__func__);
	tmp->size=size;
	tmp->code=(uint8_t*)malloc(sizeof(uint8_t)*size);
	FKL_ASSERT(tmp->code,__func__);
	fread(tmp->code,size,1,fp);
	return tmp;
}

void loadSymbolTable(FILE* fp)
{
	uint64_t size=0;
	fread(&size,sizeof(size),1,fp);
	for(uint64_t i=0;i<size;i++)
	{
		char* symbol=fklGetStringFromFile(fp);
		fklAddSymbolToGlob(symbol);
		free(symbol);
	}
}

FklLineNumberTable* loadLineNumberTable(FILE* fp)
{
	uint32_t size=0;
	uint32_t i=0;
	fread(&size,sizeof(uint32_t),1,fp);
	FklLineNumTabNode** list=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*size);
	FKL_ASSERT(list,__func__);
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
		char* cwd=getcwd(NULL,0);
		fklSetCwd(cwd);
		free(cwd);
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
		FklByteCodelnt bytecodelnt=
		{
			.ls=lnt->num,
			.l=lnt->list,
			.bc=mainCode,
		};
		fklPrintByteCodelnt(&bytecodelnt,stdout);
		fklFreeByteCode(mainCode);
		fclose(fp);
		fklFreeCwd();
		fklFreeGlobSymbolTable();
		fklFreeLineNumberTable(lnt);
		fklFreeMainFileRealPath();
	}
	else
	{
		fprintf(stderr,"error:Not a correct file!\n");
		return EXIT_FAILURE;
	}
	return 0;
}