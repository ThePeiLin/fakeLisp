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

int main(int argc,char** argv)
{
	if(argc<2)
	{
		fprintf(stderr,"error:Not input file!\n");
		return EXIT_FAILURE;
	}
	for(int i=1;i<argc;i++)
	{
		char* pattern=argv[i];
		glob_t buf;
		glob(pattern,GLOB_NOSORT,NULL,&buf);
		FklSymbolTable* table=fklCreateSymbolTable();
		fklInitCodegen(table);
		//fklInitLexer();
		char* cwd=getcwd(NULL,0);
		fklSetCwd(cwd);
		free(cwd);
		chdir(fklGetCwd());
		for(size_t j=0;j<buf.gl_pathc;j++)
		{
			char* filename=buf.gl_pathv[j];
			FILE* fp=fopen(filename,"r");
			if(fp==NULL)
			{
				perror(filename);
				fklDestroyCwd();
				globfree(&buf);
				fklUninitCodegen();
				return EXIT_FAILURE;
			}
			if(fklIsscript(filename))
			{
				if(!fklIsAccessableRegFile(filename))
				{
					perror(filename);
					fklDestroyCwd();
					fklUninitCodegen();
					return EXIT_FAILURE;
				}
				fklAddSymbolCstr(filename,table);
				FklCodegen codegen={.fid=0,};
				char* rp=fklRealpath(filename);
				fklSetMainFileRealPath(rp);
				chdir(fklGetMainFileRealPath());
				fklInitGlobalCodegener(&codegen,rp,fklCreateSymbolTable(),table,0);
				FklByteCodelnt* mainByteCode=fklGenExpressionCodeWithFp(fp,&codegen);
				fklInitVMargs(argc,argv);
				if(mainByteCode==NULL)
				{
					free(rp);
					fklDestroyFuncPrototypes(codegen.pts);
					fklUninitCodegener(&codegen);
					fklUninitCodegen();
					fklDestroyMainFileRealPath();
					fklDestroyCwd();
					globfree(&buf);
					fklUninitCodegen();
					return 1;
				}
				fklUpdatePrototype(codegen.pts,codegen.globalEnv,codegen.globalSymTable,codegen.publicSymbolTable);
				fklPrintUndefinedRef(codegen.globalEnv,codegen.globalSymTable,codegen.publicSymbolTable);
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
					fklUninitCodegen();
					fklDestroyMainFileRealPath();
					fklDestroyCwd();
					globfree(&buf);
					fklUninitCodegen();
					return EXIT_FAILURE;
				}
				FklLineNumberTable globalLnt={mainByteCode->ls,mainByteCode->l};
				fklWriteSymbolTable(codegen.globalSymTable,outfp);
				fklWriteLineNumberTable(&globalLnt,outfp);
				uint64_t sizeOfMain=mainByteCode->bc->size;
				uint8_t* code=mainByteCode->bc->code;
				fwrite(&sizeOfMain,sizeof(mainByteCode->bc->size),1,outfp);
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
						FklByteCodelnt* bcl=lib->u.bcl;
						FklLineNumberTable tmpLnt={bcl->ls,bcl->l};
						fklWriteLineNumberTable(&tmpLnt,outfp);
						uint64_t libSize=bcl->bc->size;
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
			}
			else
			{
				fprintf(stderr,"error: It is not a correct file.\n");
				fclose(fp);
				fklDestroyCwd();
				globfree(&buf);
				fklUninitCodegen();
				return EXIT_FAILURE;
			}
		}
		globfree(&buf);
		fklUninitCodegen();
	}
	return 0;
}
