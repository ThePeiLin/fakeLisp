#include<fakeLisp/opcode.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/vm.h>
#include<fakeLisp/codegen.h>
#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#ifdef _WIN32
#include<io.h>
#include<process.h>
#else
#include<unistd.h>
#include<glob.h>
#endif

static inline int compile(const char* filename,int argc,char* argv[],FklCodegenOuterCtx* outer_ctx)
{
	FklSymbolTable* pst=&outer_ctx->public_symbol_table;
	fklAddSymbolCstr(filename,pst);
	FILE* fp=fopen(filename,"r");
	FklCodegen codegen={.fid=0,};
	char* rp=fklRealpath(filename);
	fklSetMainFileRealPath(rp);
	chdir(fklGetMainFileRealPath());
	fklInitGlobalCodegener(&codegen,rp,fklCreateSymbolTable(),0,outer_ctx);
	FklByteCodelnt* mainByteCode=fklGenExpressionCodeWithFp(fp,&codegen);
	fklInitVMargs(argc,argv);
	if(mainByteCode==NULL)
	{
		free(rp);
		fklDestroyFuncPrototypes(codegen.pts);
		fklUninitCodegener(&codegen);
		fklDestroyMainFileRealPath();
		return EXIT_FAILURE;
	}
	fklUpdatePrototype(codegen.pts,codegen.globalEnv,codegen.globalSymTable,pst);
	fklPrintUndefinedRef(codegen.globalEnv,codegen.globalSymTable,pst);
	FklPtrStack* loadedLibStack=codegen.loadedLibStack;
	char* outputname=(char*)malloc(sizeof(char)*(strlen(rp)+2));
	strcpy(outputname,rp);
	strcat(outputname,"c");
	free(rp);
	FILE* outfp=fopen(outputname,"wb");
	if(!outfp)
	{
		fprintf(stderr,"%s:Can't create byte code file!",outputname);
		fklDestroyFuncPrototypes(codegen.pts);
		fklUninitCodegener(&codegen);
		fklDestroyMainFileRealPath();
		return EXIT_FAILURE;
	}
	fklWriteSymbolTable(codegen.globalSymTable,outfp);
	fklWriteLineNumberTable(mainByteCode->l,mainByteCode->ls,outfp);
	uint64_t sizeOfMain=mainByteCode->bc->len;
	uint8_t* code=mainByteCode->bc->code;
	fwrite(&sizeOfMain,sizeof(mainByteCode->bc->len),1,outfp);
	fwrite(code,sizeOfMain,1,outfp);
	fklWriteFuncPrototypes(codegen.pts,outfp);
	uint64_t num=loadedLibStack->top;
	fwrite(&num,sizeof(uint64_t),1,outfp);
	for(size_t i=0;i<num;i++)
	{
		FklCodegenLib* lib=loadedLibStack->base[i];
		fwrite(&lib->type,sizeof(char),1,outfp);
		if(lib->type==FKL_CODEGEN_LIB_SCRIPT)
		{
			fwrite(&lib->prototypeId,sizeof(lib->prototypeId),1,outfp);
			FklByteCodelnt* bcl=lib->bcl;
			fklWriteLineNumberTable(bcl->l,bcl->ls,outfp);
			uint64_t libSize=bcl->bc->len;
			uint8_t* libCode=bcl->bc->code;
			fwrite(&libSize,sizeof(uint64_t),1,outfp);
			fwrite(libCode,libSize,1,outfp);
		}
		else
		{
			const char* rp=lib->rp;
			uint64_t typelen=strlen(FKL_DLL_FILE_TYPE);
			uint64_t len=strlen(rp)-typelen;
			fwrite(&len,sizeof(uint64_t),1,outfp);
			fwrite(rp,len,1,outfp);
		}
	}
	fklDestroyFuncPrototypes(codegen.pts);
	fklDestroyByteCodelnt(mainByteCode);
	fclose(outfp);
	fklDestroyMainFileRealPath();
	fklDestroyCwd();
	fklUninitCodegener(&codegen);
	free(outputname);
	return 0;
}

int main(int argc,char** argv)
{
	if(argc<2)
	{
		fprintf(stderr,"error:Not input file!\n");
		return EXIT_FAILURE;
	}
	FklCodegenOuterCtx outer_ctx;
	fklInitCodegenOuterCtx(&outer_ctx);
	for(int i=1;i<argc;i++)
	{
		const char* pattern=argv[i];
		glob_t buf;
		glob(pattern,GLOB_NOSORT,NULL,&buf);
		if(buf.gl_pathc>0)
		{
			char* cwd=getcwd(NULL,0);
			fklSetCwd(cwd);
			free(cwd);
			chdir(fklGetCwd());
			for(size_t j=0;j<buf.gl_pathc;j++)
			{
				char* filename=buf.gl_pathv[j];
				if(fklIsscript(filename)&&fklIsAccessableRegFile(filename))
				{
					if(!fklIsAccessableRegFile(filename))
					{
						perror(filename);
						fklDestroyCwd();
						fklUninitCodegenOuterCtx(&outer_ctx);
						return EXIT_FAILURE;
					}
					if(compile(filename,argc,argv,&outer_ctx))
					{
						fklUninitCodegenOuterCtx(&outer_ctx);
						fklDestroyMainFileRealPath();
						fklDestroyCwd();
						globfree(&buf);
						return EXIT_FAILURE;
					}
				}
				else
				{

					FklStringBuffer buffer;
					fklInitStringBuffer(&buffer);
					fklStringBufferConcatWithCstr(&buffer,filename);
					fklStringBufferConcatWithCstr(&buffer,FKL_PATH_SEPARATOR_STR);
					fklStringBufferConcatWithCstr(&buffer,"main.fkl");

					if(!fklIsAccessableRegFile(buffer.buf)||compile(buffer.buf,argc,argv,&outer_ctx))
					{
						fklDestroyMainFileRealPath();
						fprintf(stderr,"error: It is not a correct file.\n");
						fklDestroyCwd();
						globfree(&buf);
						fklUninitCodegenOuterCtx(&outer_ctx);
						return EXIT_FAILURE;
					}
					fklUninitStringBuffer(&buffer);
				}
			}
			fklDestroyCwd();
			globfree(&buf);
		}
		fklUninitCodegenOuterCtx(&outer_ctx);
	}
	return 0;
}
