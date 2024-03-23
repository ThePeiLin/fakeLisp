#include<fakeLisp/utils.h>
#include<fakeLisp/codegen.h>
#include<fakeLisp/vm.h>
#include<fakeLisp/common.h>
#include<fakeLisp/builtin.h>
#include<argtable3.h>
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

struct arg_lit* help;
struct arg_file* files;
struct arg_end* end;

int main(int argc,char** argv)
{
	int exitState=0;
	const char* progname=argv[0];
	void* argtable[]=
	{
		help=arg_lit0("h","help","display this help and exit"),
		files=arg_filen(NULL,NULL,NULL,1,argc+2,"files"),
		end=arg_end(20),
	};

	int nerrors=arg_parse(argc,argv,argtable);

	if(help->count>0)
	{
		printf("Usage: %s",progname);
		arg_print_syntaxv(stdout,argtable,"\n");
		arg_print_glossary_gnu(stdout,argtable);
		goto exit;
	}

	if(nerrors)
	{
		arg_print_errors(stdout,end,progname);
		printf("Try '%s --help' for more informaction.\n",progname);
		exitState=EXIT_FAILURE;
		goto exit;
	}

	for(int i=0;i<files->count;i++)
	{
		const char* filename=files->filename[i];
		const char* extension=files->extension[i];

		if(!strcmp(extension,FKL_BYTECODE_FILE_EXTENSION))
		{
			FILE* fp=fopen(filename,"rb");
			if(fp==NULL)
			{
				perror(filename);
				exitState=EXIT_FAILURE;
				goto exit;
			}
			FklSymbolTable table;
			fklInitSymbolTable(&table);
			fklLoadSymbolTable(fp,&table);
			fklDestroyFuncPrototypes(fklLoadFuncPrototypes(fp));
			char* rp=fklRealpath(filename);
			free(rp);

			FklByteCodelnt* bcl=fklLoadByteCodelnt(fp);
			printf("file: %s\n\n",filename);
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
				printf("lib %"FKL_PRT64U":\n",i+1);
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
			puts("\nglobal symbol table:");
			fklPrintSymbolTable(&table,stdout);

			free(libs);
			fclose(fp);
			fklUninitSymbolTable(&table);
		}
		else if(!strcmp(extension,FKL_PRE_COMPILE_FILE_EXTENSION))
		{
			char* cwd=fklSysgetcwd();
			free(cwd);
			FILE* fp=fopen(filename,"rb");
			if(fp==NULL)
			{
				perror(filename);
				exitState=EXIT_FAILURE;
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
			if(load_result)
			{
				if(errorStr)
				{
					fprintf(stderr,"%s\n",errorStr);
					free(errorStr);
				}
				else
					fprintf(stderr,"%s: load failed\n",filename);
				exitState=EXIT_FAILURE;
				goto precompile_exit;
			}

			printf("file: %s\n\n",filename);
			FklSymbolTable* pst=&ctx.public_symbol_table;
			uint32_t num=scriptLibStack.top;
			for(size_t i=0;i<num;i++)
			{
				FklCodegenLib* cur=scriptLibStack.base[i];
				printf("lib %"FKL_PRT64U":\n",i+1);
				switch(cur->type)
				{
					case FKL_CODEGEN_LIB_SCRIPT:
						fklPrintByteCodelnt(cur->bcl,stdout,&gst);
						fputc('\n',stdout);
						if(cur->head)
							print_compiler_macros(cur->head,pst,stdout);
						if(cur->named_prod_groups.t)
							print_reader_macros(&cur->named_prod_groups
									,pst
									,stdout);
						if(!cur->head&&!cur->named_prod_groups.t)
							fputc('\n',stdout);
						break;
					case FKL_CODEGEN_LIB_DLL:
						fputs(cur->rp,stdout);
						fputs("\n\n",stdout);
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
					printf("lib %"FKL_PRT64U":\n",i+1);
					switch(cur->type)
					{
						case FKL_CODEGEN_LIB_SCRIPT:
							fklPrintByteCodelnt(cur->bcl,stdout,pst);
							fputc('\n',stdout);
							if(cur->head)
								print_compiler_macros(cur->head,pst,stdout);
							if(cur->named_prod_groups.t)
								print_reader_macros(&cur->named_prod_groups
										,pst
										,stdout);
							if(!cur->head&&!cur->named_prod_groups.t)
								fputc('\n',stdout);
							break;
						case FKL_CODEGEN_LIB_DLL:
							fputs(cur->rp,stdout);
							fputs("\n\n",stdout);
							break;
					}
					fputc('\n',stdout);
				}
			}
			puts("\nglobal symbol table:");
			fklPrintSymbolTable(&gst,stdout);

			puts("\npublic symbol table:");
			fklPrintSymbolTable(&ctx.public_symbol_table,stdout);

precompile_exit:
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

			if(exitState)
				break;
		}
		else
		{
			fprintf(stderr,"%s: Not a correct file!\n",filename);
			break;
		}
	}

exit:
	arg_freetable(argtable,sizeof(argtable)/sizeof(argtable[0]));
	return exitState;
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
		fputs("\n\n",fp);
	}
}

static void print_reader_macros(const FklHashTable* ht,const FklSymbolTable* pst,FILE* fp)
{
	fputs("\nreader macros:\n",fp);
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
