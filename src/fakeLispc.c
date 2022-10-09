#include<fakeLisp/opcode.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/vm.h>
#include<fakeLisp/codegen.h>
#include<fakeLisp/lexer.h>
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
		fklInitCodegen();
		fklInitLexer();
		char* cwd=getcwd(NULL,0);
		fklSetCwd(cwd);
		free(cwd);
		chdir(fklGetCwd());
		for(int j=0;j<buf.gl_pathc;j++)
		{
			char* filename=buf.gl_pathv[j];
			FILE* fp=fopen(filename,"r");
			if(fp==NULL)
			{
				perror(filename);
				fklFreeCwd();
				globfree(&buf);
				fklUninitCodegen();
				return EXIT_FAILURE;
			}
			if(fklIsscript(filename))
			{
				if(access(filename,R_OK))
				{
					perror(filename);
					fklFreeCwd();
					fklUninitCodegen();
					return EXIT_FAILURE;
				}
				fklAddSymbolToGlobCstr(filename);
				FklCodegen codegen={.fid=0,};
				char* rp=fklRealpath(filename);
				fklSetMainFileRealPath(rp);
				chdir(fklGetMainFileRealPath());
				fklInitGlobalCodegener(&codegen,rp,NULL,fklNewSymbolTable(),0);
				FklByteCodelnt* mainByteCode=fklGenExpressionCodeWithFp(fp,&codegen);
				fklInitVMargs(argc,argv);
				if(mainByteCode==NULL)
				{
					free(rp);
					fklUninitCodegener(&codegen);
					fklUninitCodegen();
					fklFreeGlobSymbolTable();
					fklFreeMainFileRealPath();
					fklFreeCwd();
					globfree(&buf);
					fklUninitCodegen();
					return 1;
				}
				FklSymbolTable* globalSymbolTable=fklExchangeGlobSymbolTable(codegen.globalSymTable);
				fklFreeSymbolTable(globalSymbolTable);
				fklCodegenPrintUndefinedSymbol(mainByteCode,codegen.globalSymTable);
				char* outputname=(char*)malloc(sizeof(char)*(strlen(rp)+2));
				strcpy(outputname,rp);
				strcat(outputname,"c");
				free(rp);
				FILE* outfp=fopen(outputname,"wb");
				if(!outfp)
				{
					fprintf(stderr,"%s:Can't create byte code file!",outputname);
					fklUninitCodegener(&codegen);
					fklUninitCodegen();
					fklFreeGlobSymbolTable();
					fklFreeMainFileRealPath();
					fklFreeCwd();
					globfree(&buf);
					fklUninitCodegen();
					return EXIT_FAILURE;
				}
				FklLineNumberTable globalLnt={mainByteCode->ls,mainByteCode->l};
				fklWriteGlobSymbolTable(outfp);
				fklWriteLineNumberTable(&globalLnt,outfp);
				uint64_t sizeOfMain=mainByteCode->bc->size;
				uint8_t* code=mainByteCode->bc->code;
				fwrite(&sizeOfMain,sizeof(mainByteCode->bc->size),1,outfp);
				fwrite(code,sizeof(uint8_t),sizeOfMain,outfp);
				fklFreeByteCodeAndLnt(mainByteCode);
				fclose(outfp);
				fklFreeMainFileRealPath();
				fklFreeCwd();
				fklUninitCodegener(&codegen);
				fklFreeGlobSymbolTable();
				free(outputname);
			}
			else
			{
				fprintf(stderr,"error: It is not a correct file.\n");
				fclose(fp);
				fklFreeCwd();
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
