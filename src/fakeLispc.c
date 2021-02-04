#include"common.h"
#include"compiler.h"
#include"opcode.h"
#include"syntax.h"
#include<string.h>
#include<stdio.h>

int main(int argc,char** argv)
{
	if(argc<2)
	{
		fprintf(stderr,"error:Not input file!\n");
		return EXIT_FAILURE;
	}
	char* filename=argv[1];
	FILE* fp=(argc>1)?fopen(argv[1],"r"):stdin;
	if(fp==NULL)
	{
		perror(filename);
		return EXIT_FAILURE;
	}
	if(argc==1||isscript(filename))
	{
		char* outputname=(char*)malloc(sizeof(char)*(strlen(filename)+2));
		strcpy(outputname,filename);
		strcat(outputname,"c");
		FILE* outfp=fopen(outputname,"wb");
		//changeWorkPath(filename);
		initPreprocess();
		int i;
		Intpr* inter=newIntpr(((fp==stdin)?"stdin":argv[1]),fp,NULL,NULL);
		ByteCode* mainByteCode=compileFile(inter);
		//printByteCode(mainByteCode,stderr);
		writeSymbolTable(inter->table,outfp);
		int32_t numOfRawproc=(inter->procs==NULL)?0:inter->procs->count+1;
		fwrite(&numOfRawproc,sizeof(numOfRawproc),1,outfp);
		ByteCode* rawProcList=castRawproc(NULL,inter->procs);
		for(i=0;i<numOfRawproc;i++)
		{
			int32_t size=rawProcList[i].size;
			char* code=rawProcList[i].code;
			fwrite(&size,sizeof(size),1,outfp);
			fwrite(code,sizeof(char),size,outfp);
		}
		int32_t sizeOfMain=mainByteCode->size;
		char* code=mainByteCode->code;
		fwrite(&sizeOfMain,sizeof(sizeOfMain),1,outfp);
		fwrite(code,sizeof(char),sizeOfMain,outfp);
		writeAllDll(inter,outfp);
		freeByteCode(mainByteCode);
		free(rawProcList);
		fclose(outfp);
		freeIntpr(inter);
		unInitPreprocess();
		free(outputname);
	}
	else
	{
		fprintf(stderr,"error: It is not a correct file.\n");
		fclose(fp);
		return EXIT_FAILURE;
	}
	return 0;
}
