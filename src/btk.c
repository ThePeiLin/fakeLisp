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
		RAISE_BUILTIN_ERROR("btk.getch",TOOMANYARG,topComStack(exe->rstack),exe);
	SET_RETURN("FAKE_getch",MAKE_VM_CHR(getch()),stack);
}

void FAKE_sleep(FakeVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	VMrunnable* r=topComStack(exe->rstack);
	VMvalue* second=GET_VAL(popVMstack(stack));
	if(!second)
		RAISE_BUILTIN_ERROR("btk.sleep",TOOFEWARG,r,exe);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("btk.sleep",TOOMANYARG,r,exe);
	if(!IS_IN32(second))
		RAISE_BUILTIN_ERROR("btk.sleep",WRONGARG,r,exe);
	releaseSource(pGClock);
	SET_RETURN("FAKE_sleep",MAKE_VM_IN32(sleep(GET_IN32(second))),stack);
	lockSource(pGClock);
	stack->tp+=1;
}

void FAKE_usleep(FakeVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	VMvalue* second=GET_VAL(popVMstack(stack));
	VMrunnable* r=topComStack(exe->rstack);
	if(!second)
		RAISE_BUILTIN_ERROR("btk.usleep",TOOFEWARG,r,exe);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("btk.usleep",TOOMANYARG,r,exe);
	if(!IS_IN32(second))
		RAISE_BUILTIN_ERROR("btk.usleep",WRONGARG,r,exe);
	releaseSource(pGClock);
#ifdef _WIN32
		Sleep(GET_IN32(second));
#else
		usleep(GET_IN32(second));
#endif
	lockSource(pGClock);
	SET_RETURN("FAKE_usleep",second,stack);
	stack->tp+=1;
}

void FAKE_exit(FakeVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	VMvalue* exitCode=GET_VAL(popVMstack(stack));
	lockSource(pGClock);
	int a[2]={0,2};
	VMrunnable* r=topComStack(exe->rstack);
	if(exe->VMid==-1)
		RAISE_BUILTIN_ERROR("btk.exit",STACKERROR,r,exe);
	if(exitCode==NULL)
		exe->callback(a);
	if(!IS_IN32(exitCode))
		RAISE_BUILTIN_ERROR("btk.exit",WRONGARG,r,exe);
	a[0]=GET_IN32(exitCode);
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
	VMvalue*  lim=GET_VAL(popVMstack(stack));
	VMrunnable* r=topComStack(exe->rstack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("btk.rand",TOOMANYARG,r,exe);
	if(lim&&!IS_IN32(lim))
		RAISE_BUILTIN_ERROR("btk.rand",WRONGARG,r,exe);
	SET_RETURN("FAKE_rand",MAKE_VM_IN32(rand()%((lim==NULL||!IS_IN32(lim))?RAND_MAX:GET_IN32(lim))),stack);
}

void FAKE_getTime(FakeVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("btk.getTime",TOOMANYARG,topComStack(exe->rstack),exe);
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
	sprintf(trueTime,"%s-%s-%s_%s_%s_%s",year,mon,day,hour,min,sec);
	free(sec);
	free(min);
	free(hour);
	free(day);
	free(mon);
	free(year);
	VMvalue* tmpVMvalue=newVMvalue(STR,trueTime,exe->heap);
	SET_RETURN("FAKE_getTime",tmpVMvalue,stack);
}

void FAKE_removeFile(FakeVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	VMvalue* name=popVMstack(stack);
	VMrunnable* r=topComStack(exe->rstack);
	if(!name)
		RAISE_BUILTIN_ERROR("btk.removeFile",TOOFEWARG,r,exe);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("btk.removeFile",TOOMANYARG,r,exe);
	if(!IS_STR(name))
		RAISE_BUILTIN_ERROR("btk.removeFile",WRONGARG,r,exe);
	SET_RETURN("FAKE_removeFile",MAKE_VM_IN32(remove(name->u.str)),stack);
}

void FAKE_setVMChanlBufferSize(FakeVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	VMvalue* chan=popVMstack(stack);
	VMvalue* size=popVMstack(stack);
	VMrunnable* r=topComStack(exe->rstack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("btk.setVMChanlBufferSize",TOOMANYARG,r,exe);
	if(size==NULL||chan==NULL)
		RAISE_BUILTIN_ERROR("btk.setVMChanlBufferSize",TOOFEWARG,r,exe);
	if(!IS_IN32(size)||!IS_CHAN(chan))
		RAISE_BUILTIN_ERROR("btk.setVMChanlBufferSize",WRONGARG,r,exe);
	chan->u.chan->max=GET_IN32(size);
	SET_RETURN("FAKE_setVMChanlBufferSize",chan,stack);
}

void FAKE_isEndOfFile(FakeVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	VMvalue* fp=popVMstack(stack);
	VMrunnable* r=topComStack(exe->rstack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("btk.isEndOfFile",TOOMANYARG,r,exe);
	if(fp==NULL)
		RAISE_BUILTIN_ERROR("btk.isEndOfFile",TOOFEWARG,r,exe);
	if(!IS_FP(fp))
		RAISE_BUILTIN_ERROR("btk.isEndOfFile",WRONGARG,r,exe);
	SET_RETURN("FAKE_isEndOfFile",feof(fp->u.fp->fp)
			?VM_TRUE
			:VM_NIL
			,stack);
}
#ifdef __cplusplus
}
#endif
