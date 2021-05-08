#include"common.h"
#include"compiler.h"
#include"opcode.h"
#include"syntax.h"
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
	FILE* fp=(argc>1)?fopen(argv[1],"r"):stdin;
	if(fp==NULL)
	{
		perror(filename);
		return EXIT_FAILURE;
	}
	if(argc==1||isscript(filename))
	{
#ifdef _WIN32
		char* rp=_fullpath(NULL,filename,0);
#else
		char* rp=realpath(filename,0);
#endif
		//changeWorkPath(filename);
		initPreprocess();
		int i;
		Intpr* inter=newIntpr(((fp==stdin)?"stdin":argv[1]),fp,NULL,NULL,NULL);
		SymTabNode* node=newSymTabNode(argv[1]);
		addSymTabNode(node,inter->table);
		ByteCode* fix=newByteCode(0);
		int status;
		ByteCodelnt* mainByteCode=compileFile(inter,1,fix,&status);
		if(mainByteCode==NULL)
		{
			free(rp);
			freeIntpr(inter);
			freeByteCode(fix);
			unInitPreprocess();
			return status;
		}
		//printByteCodelnt(mainByteCode,inter->table,stderr);
		char* outputname=(char*)malloc(sizeof(char)*(strlen(rp)+2));
		strcpy(outputname,rp);
		strcat(outputname,"c");
		FILE* outfp=fopen(outputname,"wb");
		if(!outfp)
		{
			fprintf(stderr,"%s:Can't create byte code file!",outputname);
			return 1;
		}
		reCodeCat(fix,mainByteCode->bc);
		mainByteCode->l[0]->cpc+=fix->size;
		INCREASE_ALL_SCP(mainByteCode->l+1,mainByteCode->ls-1,fix->size);
		freeByteCode(fix);
		addLineNumTabId(mainByteCode->l,mainByteCode->ls,0,inter->lnt);
		//printByteCode(mainByteCode,stderr);
		writeSymbolTable(inter->table,outfp);
		writeLineNumberTable(inter->lnt,outfp);
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
		int32_t sizeOfMain=mainByteCode->bc->size;
		char* code=mainByteCode->bc->code;
		fwrite(&sizeOfMain,sizeof(sizeOfMain),1,outfp);
		fwrite(code,sizeof(char),sizeOfMain,outfp);
		freeByteCode(mainByteCode->bc);
		free(mainByteCode);
		free(rawProcList);
		fclose(outfp);
		freeIntpr(inter);
		unInitPreprocess();
		free(outputname);
		free(rp);
	}
	else
	{
		fprintf(stderr,"error: It is not a correct file.\n");
		fclose(fp);
		return EXIT_FAILURE;
	}
	return 0;
}
