#include<fakeLisp/compiler.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/syntax.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/vm.h>
#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>

int main(int argc,char** argv)
{
	if(argc<2)
	{
		fprintf(stderr,"error:Not input file!\n");
		return EXIT_FAILURE;
	}
	char* filename=argv[1];
	char* cwd=getcwd(NULL,0);
	fklSetCwd(cwd);
	free(cwd);
	FILE* fp=(argc>1)?fopen(argv[1],"r"):stdin;
	if(fp==NULL)
	{
		perror(filename);
		fklFreeCwd();
		return EXIT_FAILURE;
	}
	if(argc==1||fklIsscript(filename))
	{
		char* rp=fklRealpath(filename);
		FklInterpreter* inter=fklNewIntpr(rp,fp,NULL,NULL);
		fklInitGlobKeyWord(inter->glob);
		fklAddSymbolToGlob(argv[1]);
		int state;
		FklByteCodelnt* mainByteCode=fklCompileFile(inter,&state);
		fklPrintByteCodelnt(mainByteCode,stderr);
		fklInitVMargs(argc,argv);
		if(mainByteCode==NULL)
		{
			free(rp);
			fklFreeIntpr(inter);
			fklUninitPreprocess();
			fklFreeCwd();
			fklFreeGlobSymbolTable();
			return state;
		}
		fklPrintUndefinedSymbol(mainByteCode);
		char* outputname=(char*)malloc(sizeof(char)*(strlen(rp)+2));
		strcpy(outputname,rp);
		strcat(outputname,"c");
		FILE* outfp=fopen(outputname,"wb");
		if(!outfp)
		{
			fprintf(stderr,"%s:Can't create byte code file!",outputname);
			fklFreeCwd();
			fklFreeIntpr(inter);
			fklUninitPreprocess();
			fklFreeCwd();
			fklFreeGlobSymbolTable();
			return EXIT_FAILURE;
		}
		inter->lnt->list=mainByteCode->l;
		inter->lnt->num=mainByteCode->ls;
		fklWriteGlobSymbolTable(outfp);
		fklWriteLineNumberTable(inter->lnt,outfp);
		uint64_t sizeOfMain=mainByteCode->bc->size;
		uint8_t* code=mainByteCode->bc->code;
		fwrite(&sizeOfMain,sizeof(mainByteCode->bc->size),1,outfp);
		fwrite(code,sizeof(uint8_t),sizeOfMain,outfp);
		fklFreeByteCode(mainByteCode->bc);
		free(mainByteCode);
		fclose(outfp);
		fklFreeIntpr(inter);
		fklUninitPreprocess();
		fklFreeCwd();
		fklFreeGlobSymbolTable();
		free(outputname);
		free(rp);
	}
	else
	{
		fprintf(stderr,"error: It is not a correct file.\n");
		fclose(fp);
		fklFreeCwd();
		return EXIT_FAILURE;
	}
	return 0;
}
