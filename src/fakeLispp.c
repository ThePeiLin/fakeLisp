#include<fakeLisp/utils.h>
#include<fakeLisp/codegen.h>
#include<fakeLisp/vm.h>
#include<stdio.h>
#include<string.h>
#ifdef _WIN32
#include<io.h>
#include<process.h>
#else
#include<unistd.h>
#endif

static FklByteCode* loadByteCode(FILE*);
static FklLineNumberTable* loadLineNumberTable(FILE*);
static void loadLib(FILE*,size_t* pnum,FklCodegenLib** libs,FklSymbolTable*);

FklByteCode* loadByteCode(FILE* fp)
{
	uint64_t size=0;
	fread(&size,sizeof(uint64_t),1,fp);
	FklByteCode* tmp=(FklByteCode*)malloc(sizeof(FklByteCode));
	FKL_ASSERT(tmp);
	tmp->size=size;
	tmp->code=(uint8_t*)malloc(sizeof(uint8_t)*size);
	FKL_ASSERT(tmp->code||!size);
	fread(tmp->code,size,1,fp);
	return tmp;
}

static void loadSymbolTable(FILE* fp,FklSymbolTable* table)
{
	uint64_t size=0;
	fread(&size,sizeof(size),1,fp);
	for(uint64_t i=0;i<size;i++)
	{
		uint64_t len=0;
		fread(&len,sizeof(len),1,fp);
		FklString* buf=fklCreateString(len,NULL);
		fread(buf->str,len,1,fp);
		fklAddSymbol(buf,table);
		free(buf);
	}
}

FklLineNumberTable* loadLineNumberTable(FILE* fp)
{
	uint32_t size=0;
	uint32_t i=0;
	fread(&size,sizeof(uint32_t),1,fp);
	FklLineNumTabNode* list=(FklLineNumTabNode*)malloc(sizeof(FklLineNumTabNode)*size);
	FKL_ASSERT(list||!size);
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
		if(!fklIsAccessableRegFile(filename))
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
		FklSymbolTable* table=fklCreateSymbolTable();
		loadSymbolTable(fp,table);
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
		fklPrintByteCodelnt(&bytecodelnt,stdout,table);
		fputc('\n',stdout);
		fklDestroyByteCode(mainCode);
		FklCodegenLib* libs=NULL;
		size_t num=0;
		loadLib(fp,&num,&libs,table);
		for(size_t i=0;i<num;i++)
		{
			FklCodegenLib* cur=&libs[i];
			fputc('\n',stdout);
			printf("lib %lu:\n",i+1);
			switch(cur->type)
			{
				case FKL_CODEGEN_LIB_SCRIPT:
					fklPrintByteCodelnt(cur->u.bcl,stdout,table);
					break;
				case FKL_CODEGEN_LIB_DLL:
					fputs(cur->rp,stdout);
					break;
			}
			fputc('\n',stdout);
			fklUninitCodegenLib(cur);
		}
		free(libs);
		fclose(fp);
		fklDestroyCwd();
		fklDestroyLineNumberTable(lnt);
		fklDestroyMainFileRealPath();
		fklDestroySymbolTable(table);
	}
	else
	{
		fprintf(stderr,"error:Not a correct file!\n");
		return EXIT_FAILURE;
	}
	return 0;
}

static void dll_init(size_t* _,FklSid_t** __,FklSymbolTable* ___)
{
}

static void loadLib(FILE* fp,size_t* pnum,FklCodegenLib** plibs,FklSymbolTable* table)
{
	fread(pnum,sizeof(uint64_t),1,fp);
	size_t num=*pnum;
	FklCodegenLib* libs=(FklCodegenLib*)malloc(sizeof(FklCodegen)*num);
	FKL_ASSERT(libs||!num);
	*plibs=libs;
	for(size_t i=0;i<num;i++)
	{
		FklCodegenLibType libType=FKL_CODEGEN_LIB_SCRIPT;
		fread(&libType,sizeof(char),1,fp);
		if(libType==FKL_CODEGEN_LIB_SCRIPT)
		{
			uint32_t protoId=0;
			fread(&protoId,sizeof(protoId),1,fp);
			FklByteCodelnt* bcl=fklCreateByteCodelnt(NULL);
			fklLoadLineNumberTable(fp,&bcl->l,&bcl->ls);
			FklByteCode* bc=loadByteCode(fp);
			bcl->bc=bc;
			fklInitCodegenScriptLib(&libs[i]
					,NULL
					,bcl
					,0
					,NULL
					,NULL
					,NULL
					,NULL
					,NULL
					,NULL);
			libs[i].prototypeId=protoId;
		}
		else
		{
			uint64_t len=0;
			uint64_t typelen=strlen(FKL_DLL_FILE_TYPE);
			fread(&len,sizeof(uint64_t),1,fp);
			char* rp=(char*)calloc(sizeof(char),len+typelen+1);
			FKL_ASSERT(rp);
			fread(rp,len,1,fp);
			strcat(rp,FKL_DLL_FILE_TYPE);
			fklInitCodegenDllLib(&libs[i],rp,NULL,table,dll_init);
		}
	}
}
