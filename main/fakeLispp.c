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

static void loadLib(FILE*,size_t* pnum,FklCodegenLib** libs,FklSymbolTable*);

static inline FklLineNumberTableItem* loadLineNumberTable(uint32_t* psize,FILE* fp)
{
	uint32_t size=0;
	uint32_t i=0;
	fread(&size,sizeof(uint32_t),1,fp);
	FklLineNumberTableItem* list=(FklLineNumberTableItem*)malloc(sizeof(FklLineNumberTableItem)*size);
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
	*psize=size;
	return list;
}

int main(int argc,char** argv)
{
	if(argc<2)
	{
		fprintf(stderr,"error:Not input file!\n");
		return EXIT_FAILURE;
	}
	char* filename=argv[1];
	if(fklIsByteCodeFile(filename))
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
		fklLoadSymbolTable(fp,table);
		fklDestroyFuncPrototypes(fklLoadFuncPrototypes(fklGetBuiltinSymbolNum(),fp));
		char* rp=fklRealpath(filename);
		fklSetMainFileRealPath(rp);
		free(rp);

		FklByteCodelnt* bcl=fklLoadByteCodelnt(fp);
		fklPrintByteCodelnt(bcl,stdout,table);
		fputc('\n',stdout);
		fklDestroyByteCodelnt(bcl);
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
					fklPrintByteCodelnt(cur->bcl,stdout,table);
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

static void dll_init(FKL_CODEGEN_DLL_LIB_INIT_EXPORT_FUNC_ARGS)
{
}

static void loadLib(FILE* fp,size_t* pnum,FklCodegenLib** plibs,FklSymbolTable* table)
{
	fread(pnum,sizeof(uint64_t),1,fp);
	size_t num=*pnum;
	FklCodegenLib* libs=(FklCodegenLib*)malloc(sizeof(FklCodegenInfo)*num);
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
			FklByteCode* bc=fklLoadByteCode(fp);
			bcl->bc=bc;
			fklInitCodegenScriptLib(&libs[i]
					,NULL
					,bcl
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
			fklInitCodegenDllLib(&libs[i],rp,NULL,NULL,dll_init,table);
		}
	}
}
