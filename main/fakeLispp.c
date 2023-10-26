#include<fakeLisp/utils.h>
#include<fakeLisp/codegen.h>
#include<fakeLisp/vm.h>
#include<fakeLisp/common.h>
#include<stdio.h>
#include<string.h>
#ifdef _WIN32
#include<direct.h>
#include<io.h>
#include<process.h>
#else
#include<unistd.h>
#endif

static void loadLib(FILE*,size_t* pnum,FklCodegenLib** libs,FklSymbolTable*);

static void print_compiler_macros(FklCodegenMacro* head,const FklSymbolTable* pst,FILE* fp);

static void print_reader_macros(const FklHashTable* named_prod_groups,const FklSymbolTable* pst,FILE* fp);

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
		if(!fklIsAccessibleRegFile(filename))
		{
			perror(filename);
			fklDestroyCwd();
			return EXIT_FAILURE;
		}
		char* cwd=fklSysgetcwd();
		fklSetCwd(cwd);
		free(cwd);
		FILE* fp=fopen(argv[1],"rb");
		if(fp==NULL)
		{
			perror(filename);
			fklDestroyCwd();
			return EXIT_FAILURE;
		}
		FklSymbolTable table;
		fklInitSymbolTable(&table);
		fklLoadSymbolTable(fp,&table);
		fklDestroyFuncPrototypes(fklLoadFuncPrototypes(fklGetBuiltinSymbolNum(),fp));
		char* rp=fklRealpath(filename);
		fklSetMainFileRealPath(rp);
		free(rp);

		FklByteCodelnt* bcl=fklLoadByteCodelnt(fp);
		fklPrintByteCodelnt(bcl,stdout,&table);
		fputc('\n',stdout);
		fklDestroyByteCodelnt(bcl);
		FklCodegenLib* libs=NULL;
		size_t num=0;
		loadLib(fp,&num,&libs,&table);
		for(size_t i=0;i<num;i++)
		{
			FklCodegenLib* cur=&libs[i];
			fputc('\n',stdout);
			printf("lib %"PRT64U":\n",i+1);
			switch(cur->type)
			{
				case FKL_CODEGEN_LIB_SCRIPT:
					fklPrintByteCodelnt(cur->bcl,stdout,&table);
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
		fklUninitSymbolTable(&table);
	}
	else if(fklIsPrecompileFile(filename))
	{
		if(!fklIsAccessibleRegFile(filename))
		{
			perror(filename);
			fklDestroyCwd();
			return EXIT_FAILURE;
		}
		char* cwd=fklSysgetcwd();
		fklSetCwd(cwd);
		free(cwd);
		FILE* fp=fopen(argv[1],"rb");
		if(fp==NULL)
		{
			perror(filename);
			fklDestroyCwd();
			return EXIT_FAILURE;
		}
		FklSymbolTable gst;
		fklInitSymbolTable(&gst);

		FklFuncPrototypes pts;
		fklInitFuncPrototypes(&pts,0);

		FklFuncPrototypes macro_pts;
		fklInitFuncPrototypes(&macro_pts,0);

		FklPtrStack scriptLibStack;
		fklInitPtrStack(&scriptLibStack,8,16);

		FklPtrStack macroScriptLibStack;
		fklInitPtrStack(&macroScriptLibStack,8,16);

		FklCodegenOuterCtx ctx;
		fklInitCodegenOuterCtxExceptPattern(&ctx);

		char* rp=fklRealpath(filename);
		char* errorStr=NULL;
		int load_result=fklLoadPreCompile(&pts
				,&macro_pts
				,&scriptLibStack
				,&macroScriptLibStack
				,&gst
				,&ctx
				,rp
				,fp
				,&errorStr);
		free(rp);
		fclose(fp);
		int exit_state=0;
		if(load_result)
		{
			if(errorStr)
			{
				fprintf(stderr,"%s\n",errorStr);
				free(errorStr);
			}
			exit_state=1;
			goto exit;
		}

		FklSymbolTable* pst=&ctx.public_symbol_table;
		uint32_t num=scriptLibStack.top;
		for(size_t i=0;i<num;i++)
		{
			FklCodegenLib* cur=scriptLibStack.base[i];
			fputc('\n',stdout);
			printf("lib %"PRT64U":\n",i+1);
			switch(cur->type)
			{
				case FKL_CODEGEN_LIB_SCRIPT:
					fklPrintByteCodelnt(cur->bcl,stdout,&gst);
					if(cur->head)
						print_compiler_macros(cur->head,pst,stdout);
					if(cur->named_prod_groups.t)
						print_reader_macros(&cur->named_prod_groups
								,pst
								,stdout);
					break;
				case FKL_CODEGEN_LIB_DLL:
					fputs(cur->rp,stdout);
					break;
			}
			fputc('\n',stdout);
		}

		if(macroScriptLibStack.top>0)
		{
			fputc('\n',stdout);
			puts("macro loaded libs:\n");
			num=macroScriptLibStack.top;
			for(size_t i=0;i<num;i++)
			{
				FklCodegenLib* cur=macroScriptLibStack.base[i];
				putchar('\n');
				printf("lib %"PRT64U":\n",i+1);
				switch(cur->type)
				{
					case FKL_CODEGEN_LIB_SCRIPT:
						fklPrintByteCodelnt(cur->bcl,stdout,pst);
						if(cur->head)
							print_compiler_macros(cur->head,pst,stdout);
						if(cur->named_prod_groups.t)
							print_reader_macros(&cur->named_prod_groups
									,pst
									,stdout);
						break;
					case FKL_CODEGEN_LIB_DLL:
						fputs(cur->rp,stdout);
						break;
				}
				fputc('\n',stdout);
			}
		}
exit:
		fklUninitFuncPrototypes(&pts);
		fklUninitFuncPrototypes(&macro_pts);

		while(!fklIsPtrStackEmpty(&scriptLibStack))
			fklDestroyCodegenLib(fklPopPtrStack(&scriptLibStack));
		fklUninitPtrStack(&scriptLibStack);
		while(!fklIsPtrStackEmpty(&macroScriptLibStack))
			fklDestroyCodegenLib(fklPopPtrStack(&macroScriptLibStack));
		fklUninitPtrStack(&macroScriptLibStack);

		fklUninitSymbolTable(&gst);
		fklUninitSymbolTable(&ctx.public_symbol_table);
		fklDestroyCwd();
		fklDestroyMainFileRealPath();
		return exit_state;
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
			uv_lib_t lib={0};
			uint64_t len=0;
			uint64_t typelen=strlen(FKL_DLL_FILE_TYPE);
			fread(&len,sizeof(uint64_t),1,fp);
			char* rp=(char*)calloc(sizeof(char),len+typelen+1);
			FKL_ASSERT(rp);
			fread(rp,len,1,fp);
			strcat(rp,FKL_DLL_FILE_TYPE);
			fklInitCodegenDllLib(&libs[i],rp,lib,NULL,dll_init,table);
		}
	}
}

static void print_compiler_macros(FklCodegenMacro* head,const FklSymbolTable* pst,FILE* fp)
{
	fputs("\ncompiler macros:\n",fp);
	for(;head;head=head->next)
	{
		fputs("pattern:\n",fp);
		fklPrintNastNode(head->origin_exp,fp,pst);
		fputc('\n',fp);
		fklPrintByteCodelnt(head->bcl,fp,pst);
		fputc('\n',fp);
	}
}

static void print_reader_macros(const FklHashTable* ht,const FklSymbolTable* pst,FILE* fp)
{
	fputs("\n\nreader macros:\n",fp);
	for(FklHashTableItem* l=ht->first;l;l=l->next)
	{
		FklGrammerProductionGroup* group=(FklGrammerProductionGroup*)l->data;
		fputs("group name:",fp);
		fklPrintRawSymbol(fklGetSymbolWithId(group->id,pst)->symbol,fp);
		if(group->ignore_printing.top)
		{
			fputs("\nignores:\n",fp);
			uint32_t top=group->ignore_printing.top;
			void** base=group->ignore_printing.base;
			for(uint32_t i=0;i<top;i++)
			{
				fklPrintNastNode(base[i],fp,pst);
				fputc('\n',fp);
			}
		}
		if(group->prod_printing.top)
		{
			fputs("\nprods:\n\n",fp);
			uint32_t top=group->prod_printing.top;
			void** base=group->prod_printing.base;
			for(uint32_t i=0;i<top;i++)
			{
				FklCodegenProdPrinting* p=base[i];
				if(p->sid)
					fklPrintRawSymbol(fklGetSymbolWithId(p->sid,pst)->symbol,fp);
				else
					fputs("()",fp);
				fputc(' ',fp);
				fklPrintNastNode(p->vec,fp,pst);
				fputc(' ',fp);
				fputs(p->add_extra?"start":"not-start",fp);
				fputc(' ',fp);
				fputs("\naction: ",fp);
				if(p->type==FKL_CODEGEN_PROD_CUSTOM)
				{
					fputs("custom\n",fp);
					fklPrintByteCodelnt(p->bcl,fp,pst);
				}
				else
				{
					fputs(p->type==FKL_CODEGEN_PROD_SIMPLE?"simple\n":"builtin\n",fp);
					fklPrintNastNode(p->forth,fp,pst);
				}
				fputc('\n',fp);
			}
		}
		fputc('\n',fp);
	}
}
