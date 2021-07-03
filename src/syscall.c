#include"common.h"
#include"syscall.h"
#include"VMtool.h"
#include"reader.h"

extern const char* builtInSymbolList[NUMOFBUILTINSYMBOL];
extern const char* builtInErrorType[19];
void SYS_file(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMheap* heap=exe->heap;
	VMvalue* filename=getArg(stack);
	VMvalue* mode=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!mode||!filename)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	if(filename->type!=STR||mode->type!=STR)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	FILE* file=fopen(filename->u.str->str,mode->u.str->str);
	VMvalue* obj=NULL;
	if(!file)
		obj=newNilValue(heap);
	else
		obj=newVMvalue(FP,newVMfp(file),heap,1);
	SET_RETURN("SYS_file",obj,stack);
}

void SYS_read(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* file=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!file)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	if(file->type!=FP)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	FILE* tmpFile=file->u.fp->fp;
	int unexpectEOF=0;
	char* prev=NULL;
	char* tmpString=readInPattern(tmpFile,&prev,&unexpectEOF);
	if(prev)
		free(prev);
	if(unexpectEOF)
	{
		free(tmpString);
		RAISE_BUILTIN_ERROR(UNEXPECTEOF,runnable,exe);
	}
	Intpr* tmpIntpr=newTmpIntpr(NULL,tmpFile);
	AST_cptr* tmpCptr=baseCreateTree(tmpString,tmpIntpr);
	VMvalue* tmp=NULL;
	if(tmpCptr==NULL)
		tmp=newNilValue(exe->heap);
	else
		tmp=castCptrVMvalue(tmpCptr,exe->heap);
	SET_RETURN("SYS_read",tmp,stack);
	free(tmpIntpr);
	free(tmpString);
	deleteCptr(tmpCptr);
	free(tmpCptr);
}

void SYS_getb(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* size=getArg(stack);
	VMvalue* file=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!file||!size)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	if(file->type!=FP||size->type!=IN32)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	FILE* fp=file->u.fp->fp;
	uint8_t* str=(uint8_t*)malloc(sizeof(uint8_t)*(*size->u.in32));
	FAKE_ASSERT(str,"B_getb",__FILE__,__LINE__);
	int32_t realRead=0;
	realRead=fread(str,sizeof(uint8_t),*size->u.in32,fp);
	if(!realRead)
	{
		free(str);
		SET_RETURN("SYS_getb",newNilValue(exe->heap),stack);
	}
	else
	{
		str=(uint8_t*)realloc(str,sizeof(uint8_t)*realRead);
		FAKE_ASSERT(str,"B_getb",__FILE__,__LINE__);
		VMvalue* tmpByts=newVMvalue(BYTS,NULL,exe->heap,1);
		tmpByts->u.byts=newEmptyByteArry();
		tmpByts->u.byts->size=*size->u.in32;
		tmpByts->u.byts->str=str;
		SET_RETURN("SYS_getb",tmpByts,stack);
	}
}

void SYS_write(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* obj=getArg(stack);
	VMvalue* file=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!file||!obj)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	if(file->type!=FP)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	FILE* objFile=file->u.fp->fp;
	CRL* head=NULL;
	writeVMvalue(obj,objFile,&head);
	SET_RETURN("SYS_write",obj,stack);
	while(head)
	{
		CRL* prev=head;
		head=head->next;
		free(prev);
	}
}

void SYS_putb(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* bt=getArg(stack);
	VMvalue* file=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!file||!bt)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	if(file->type!=FP||bt->type!=BYTS)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	FILE* objFile=file->u.fp->fp;
	fwrite(bt->u.byts->str,sizeof(uint8_t),bt->u.byts->size,objFile);
	SET_RETURN("SYS_putb",bt,stack);
}

void SYS_princ(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* obj=getArg(stack);
	VMvalue* file=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!file||!obj)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	if(file->type!=FP)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	FILE* objFile=file->u.fp->fp;
	CRL* head=NULL;
	princVMvalue(obj,objFile,&head);
	while(head)
	{
		CRL* prev=head;
		head=head->next;
		free(prev);
	}
	SET_RETURN("SYS_princ",obj,stack);
}
