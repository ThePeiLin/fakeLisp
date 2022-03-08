#include<fakeLisp/compiler.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/syntax.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/vmrun.h>
#include<string.h>
#include<stdio.h>
#include<stdlib.h>

int main(int argc,char** argv)
{
	if(argc<2)
	{
		fprintf(stderr,"error:Not input file!\n");
		return EXIT_FAILURE;
	}
	char* filename=argv[1];
#ifdef WFKL_I32
	char* intprPath=_fullpath(NULL,argv[0],0);
#else
	char* intprPath=realpath(argv[0],0);
#endif
	char* t=fklGetDir(intprPath);
	free(intprPath);
	fklSetInterpreterPath(t);
	free(t);
	FILE* fp=(argc>1)?fopen(argv[1],"r"):stdin;
	if(fp==NULL)
	{
		perror(filename);
		fklFreeInterpeterPath();
		return EXIT_FAILURE;
	}
	if(argc==1||fklIsscript(filename))
	{
#ifdef _WIN32
		char* rp=_fullpath(NULL,filename,0);
#else
		char* rp=realpath(filename,0);
#endif
		FklInterpreter* inter=fklNewIntpr(((fp==stdin)?"stdin":argv[1]),fp,NULL,NULL,NULL);
		fklInitGlobKeyWord(inter->glob);
		fklInitNativeDefTypes(inter->deftypes);
		fklAddSymbolToGlob(argv[1]);
		int state;
		FklByteCodelnt* mainByteCode=fklCompileFile(inter,1,&state);
		if(mainByteCode==NULL)
		{
			free(rp);
			fklFreeIntpr(inter);
			fklUnInitPreprocess();
			fklFreeInterpeterPath();
			fklFreeGlobSymbolTable();
			return state;
		}
		//fklPrintByteCodelnt(mainByteCode,inter->table,stderr);
		char* outputname=(char*)malloc(sizeof(char)*(strlen(rp)+2));
		strcpy(outputname,rp);
		strcat(outputname,"c");
		FILE* outfp=fopen(outputname,"wb");
		if(!outfp)
		{
			fprintf(stderr,"%s:Can't create byte code file!",outputname);
			fklFreeInterpeterPath();
			return 1;
		}
		inter->lnt->list=mainByteCode->l;
		inter->lnt->num=mainByteCode->ls;
		fklWriteGlobSymbolTable(outfp);
		fklWriteLineNumberTable(inter->lnt,outfp);
		fklWriteTypeList(outfp);
		int32_t sizeOfMain=mainByteCode->bc->size;
		uint8_t* code=mainByteCode->bc->code;
		fwrite(&sizeOfMain,sizeof(sizeOfMain),1,outfp);
		fwrite(code,sizeof(char),sizeOfMain,outfp);
		fklFreeByteCode(mainByteCode->bc);
		free(mainByteCode);
		fclose(outfp);
		fklFreeIntpr(inter);
		fklUnInitPreprocess();
		free(outputname);
		free(rp);
	}
	else
	{
		fprintf(stderr,"error: It is not a correct file.\n");
		fclose(fp);
		fklFreeInterpeterPath();
		return EXIT_FAILURE;
	}
	fklFreeInterpeterPath();
	fklFreeAllSharedObj();
	return 0;
}
