#include<stdio.h>
#include<string.h>
#include"fakeLispc.h"
#include"tool.h"
#include"compiler.h"
#include"opcode.h"
#include"preprocess.h"
#include"syntax.h"
#include"form.h"
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
		initEvalution();
		int i;
		intpr* inter=newIntpr(((fp==stdin)?"stdin":argv[1]),fp);
		ByteCode* mainByteCode=compileFile(inter);
	//	printByteCode(mainByteCode);
		char* outputname=(char*)malloc(sizeof(char)*(strlen(filename)+2));
		strcpy(outputname,filename);
		strcat(outputname,"c");
		int32_t numOfRawproc=(inter->procs==NULL)?0:inter->procs->count+1;
		FILE* outfp=fopen(outputname,"w");
		fwrite(&numOfRawproc,sizeof(numOfRawproc),1,outfp);
		ByteCode* rawProcList=castRawproc(inter->procs);
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
		fclose(outfp);
		fclose(fp);
	}
	else
	{
		fprintf(stderr,"error: It is not a correct file.\n");
		fclose(fp);
		return EXIT_FAILURE;
	}
	return 0;
}

ByteCode* compileFile(intpr* inter)
{
	initPreprocess(inter);
	char ch;
	ByteCode* tmp=createByteCode(0);
	ByteCode* pop=createByteCode(1);
	pop->code[0]=FAKE_POP;
	while((ch=getc(inter->file))!=EOF)
	{
		ungetc(ch,inter->file);
		ANS_cptr* begin=NULL;
		char* list=getListFromFile(inter->file);
		if(list==NULL)continue;
		ErrorStatus status={0,NULL};
		begin=createTree(list,inter);
		if(begin!=NULL)
		{
			if(isPreprocess(begin))
			{
				status=eval(begin,NULL);
				if(status.status!=0)
				{
					exError(status.place,status.status,inter);
					deleteCptr(status.place);
					deleteCptr(begin);
					exit(EXIT_FAILURE);
				}
			}
			else
			{
				ByteCode* tmpByteCode=compile(begin,inter->glob,inter,&status);
				if(status.status!=0)
				{
					exError(status.place,status.status,inter);
					deleteCptr(status.place);
					freeByteCode(tmp);
					exit(EXIT_FAILURE);
				}
				ByteCode* tmp1=codeCat(tmpByteCode,pop);
				ByteCode* beFree=tmp;
				tmp=codeCat(tmp,tmp1);
				freeByteCode(tmp1);
				freeByteCode(tmpByteCode);
				freeByteCode(beFree);
				free(list);
				list=NULL;
				deleteCptr(begin);
				free(begin);
			}
		}
	}
	return tmp;
}

ByteCode* castRawproc(RawProc* procs)
{
	if(procs==NULL)return NULL;
	else
	{
		ByteCode* tmp=(ByteCode*)malloc(sizeof(ByteCode)*(procs->count+1));
		if(tmp==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
		RawProc* curRawproc=procs;
		while(curRawproc!=NULL)
		{
			tmp[curRawproc->count]=*curRawproc->proc;
			curRawproc=curRawproc->next;
		}
		return tmp;
	}
}

void initEvalution()
{
	addFunc("quote",N_quote);
	addFunc("car",N_car);
	addFunc("cdr",N_cdr);
	addFunc("cons",N_cons);
	addFunc("eq",N_eq);
	addFunc("ANS_atom",N_ANS_atom);
	addFunc("null",N_null);
	addFunc("cond",N_cond);
	addFunc("and",N_and);
	addFunc("or",N_or);
	addFunc("not",N_not);
	addFunc("define",N_define);
	addFunc("lambda",N_lambda);
	addFunc("list",N_list);
	addFunc("defmacro",N_defmacro);
//	addFunc("undef",N_undef);
	addFunc("add",N_add);
	addFunc("sub",N_sub);
	addFunc("mul",N_mul);
	addFunc("div",N_div);
	addFunc("mod",N_mod);
	addFunc("append",N_append);
	addFunc("extend",N_extend);
	addFunc("print",N_print);
}
