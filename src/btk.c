#include"common.h"
#include"VMtool.h"
#include<stdio.h>
#include<pthread.h>
#ifdef _WIN32
#include<conio.h>
#include<windows.h>
#else
#include<termios.h>
#endif
#include<time.h>
#include<string.h>
#include<unistd.h>
#include<limits.h>
#ifdef __cplusplus
extern "C"
{
#endif
#ifndef _WIN32

extern const char* builtInErrorType[NUMOFBUILTINERRORTYPE];

int getch()
{
	struct termios oldt,newt;
	int ch;
	tcgetattr(STDIN_FILENO,&oldt);
	newt=oldt;
	newt.c_lflag &=~(ICANON|ECHO);
	tcsetattr(STDIN_FILENO,TCSANOW,&newt);
	ch=getchar();
	tcsetattr(STDIN_FILENO,TCSANOW,&oldt);
	return ch;
}
#endif
void FAKE_getch(FakeVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,topComStack(exe->rstack),exe);
	char ch=getch();
	SET_RETURN("FAKE_getch",newVMvalue(CHR,&ch,exe->heap,1),stack);
}

void FAKE_sleep(FakeVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	VMrunnable* r=topComStack(exe->rstack);
	VMvalue* second=getArg(stack);
	if(!second)
		RAISE_BUILTIN_ERROR(TOOFEWARG,r,exe);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,r,exe);
	if(second->type!=IN32)
		RAISE_BUILTIN_ERROR(WRONGARG,r,exe);
	int32_t s=*second->u.in32;
	releaseSource(pGClock);
	s=sleep(s);
	lockSource(pGClock);
	stack->values[stack->tp]=newVMvalue(IN32,&s,exe->heap,1);
	stack->tp+=1;
}

void FAKE_usleep(FakeVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	VMvalue* second=getArg(stack);
	VMrunnable* r=topComStack(exe->rstack);
	if(!second)
		RAISE_BUILTIN_ERROR(TOOFEWARG,r,exe);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,r,exe);
	if(second->type!=IN32)
		RAISE_BUILTIN_ERROR(WRONGARG,r,exe);
	int32_t s=*second->u.in32;
	releaseSource(pGClock);
#ifdef _WIN32
		Sleep(s);
#else
		usleep(s);
#endif
	if(stack->tp>=stack->size)
	{
		stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size+64));
		FAKE_ASSERT(stack->values,"FAKE_usleep",__FILE__,__LINE__);
		stack->size+=64;
	}
	lockSource(pGClock);
	stack->values[stack->tp]=newVMvalue(IN32,&s,exe->heap,1);
	stack->tp+=1;
}

void FAKE_exit(FakeVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	VMvalue* exitCode=getArg(stack);
	lockSource(pGClock);
	int a[2]={0,2};
	VMrunnable* r=topComStack(exe->rstack);
	if(exe->VMid==-1)
		RAISE_BUILTIN_ERROR(STACKERROR,r,exe);
	if(exitCode==NULL)
		exe->callback(a);
	if(exitCode->type!=IN32)
		RAISE_BUILTIN_ERROR(WRONGARG,r,exe);
	int32_t num=*exitCode->u.in32;
	a[0]=num;
	exe->callback(a);
}

void FAKE_rand(FakeVM* exe,pthread_rwlock_t* pGClock)
{
	static int hasSrand=0;
	if(!hasSrand)
	{
		srand((unsigned)time(NULL));
		hasSrand=1;
	}
	VMstack* stack=exe->stack;
	VMvalue*  lim=getArg(stack);
	VMrunnable* r=topComStack(exe->rstack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,r,exe);
	if(lim&&lim->type!=IN32)
		RAISE_BUILTIN_ERROR(WRONGARG,r,exe);
	int32_t limit=(lim==NULL||*lim->u.in32==0)?RAND_MAX:*lim->u.in32;
	int32_t result=rand()%limit;
	VMvalue* toReturn=newVMvalue(IN32,&result,exe->heap,1);
	SET_RETURN("FAKE_rand",toReturn,stack);
}

void FAKE_getTime(FakeVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,topComStack(exe->rstack),exe);
	time_t timer=time(NULL);
	struct tm* tblock=NULL;
	tblock=localtime(&timer);
	char* sec=intToString(tblock->tm_sec);
	char* min=intToString(tblock->tm_min);
	char* hour=intToString(tblock->tm_hour);
	char* day=intToString(tblock->tm_mday);
	char* mon=intToString(tblock->tm_mon+1);
	char* year=intToString(tblock->tm_year+1900);
	int32_t timeLen=strlen(year)+strlen(mon)+strlen(day)+strlen(hour)+strlen(min)+strlen(sec)+5+1;
	char* trueTime=(char*)malloc(sizeof(char)*timeLen);
	FAKE_ASSERT(trueTime,"FAKE_getTime",__FILE__,__LINE__);
	snprintf(trueTime,timeLen,"%s-%s-%s_%s_%s_%s",year,mon,day,hour,min,sec);
	free(sec);
	free(min);
	free(hour);
	free(day);
	free(mon);
	free(year);
	VMstr* tmpStr=newVMstr(trueTime);
	VMvalue* tmpVMvalue=newVMvalue(STR,tmpStr,exe->heap,1);
	SET_RETURN("FAKE_getTime",tmpVMvalue,stack);
	free(trueTime);
}

void FAKE_removeFile(FakeVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	VMvalue* name=getArg(stack);
	VMrunnable* r=topComStack(exe->rstack);
	if(!name)
		RAISE_BUILTIN_ERROR(TOOFEWARG,r,exe);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,r,exe);
	if(name->type!=STR)
		RAISE_BUILTIN_ERROR(WRONGARG,r,exe);
	int32_t i=remove(name->u.str->str);
	VMvalue* toReturn=newVMvalue(IN32,&i,exe->heap,1);
	SET_RETURN("FAKE_removeFile",toReturn,stack);
}

void FAKE_setVMChanlBufferSize(FakeVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	VMvalue* chan=getArg(stack);
	VMvalue* size=getArg(stack);
	VMrunnable* r=topComStack(exe->rstack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,r,exe);
	if(size==NULL||chan==NULL)
		RAISE_BUILTIN_ERROR(TOOFEWARG,r,exe);
	if(size->type!=IN32||chan->type!=CHAN)
		RAISE_BUILTIN_ERROR(WRONGARG,r,exe);
	chan->u.chan->max=*size->u.in32;
	SET_RETURN("FAKE_setVMChanlBufferSize",chan,stack);
}

void FAKE_isEndOfFile(FakeVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	VMvalue* fp=getArg(stack);
	VMvalue* t=NULL;
	VMrunnable* r=topComStack(exe->rstack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,r,exe);
	if(fp==NULL)
		RAISE_BUILTIN_ERROR(TOOFEWARG,r,exe);
	if(fp->type!=FP)
		RAISE_BUILTIN_ERROR(WRONGARG,r,exe);
	if(feof(fp->u.fp->fp))
		t=newTrueValue(exe->heap);
	else
		t=newNilValue(exe->heap);
	SET_RETURN("FAKE_isEndOfFile",t,stack);
}
#ifdef __cplusplus
}
#endif
