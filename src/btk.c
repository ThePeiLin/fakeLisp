#include<fakeLisp/common.h>
#include<fakeLisp/VMtool.h>
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
void FAKE_getch(FklVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("btk.getch",TOOMANYARG,fklTopComStack(exe->rstack),exe);
	SET_RETURN("FAKE_getch",MAKE_VM_CHR(getch()),stack);
}

void FAKE_sleep(FklVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	VMrunnable* r=fklTopComStack(exe->rstack);
	VMvalue* second=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(!second)
		RAISE_BUILTIN_ERROR("btk.sleep",TOOFEWARG,r,exe);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("btk.sleep",TOOMANYARG,r,exe);
	if(!IS_IN32(second))
		RAISE_BUILTIN_ERROR("btk.sleep",WRONGARG,r,exe);
	fklReleaseSource(pGClock);
	SET_RETURN("FAKE_sleep",MAKE_VM_IN32(sleep(GET_IN32(second))),stack);
	fklLockSource(pGClock);
	stack->tp+=1;
}

void FAKE_usleep(FklVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	VMvalue* second=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	VMrunnable* r=fklTopComStack(exe->rstack);
	if(!second)
		RAISE_BUILTIN_ERROR("btk.usleep",TOOFEWARG,r,exe);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("btk.usleep",TOOMANYARG,r,exe);
	if(!IS_IN32(second))
		RAISE_BUILTIN_ERROR("btk.usleep",WRONGARG,r,exe);
	fklReleaseSource(pGClock);
#ifdef _WIN32
		Sleep(GET_IN32(second));
#else
		usleep(GET_IN32(second));
#endif
	fklLockSource(pGClock);
	SET_RETURN("FAKE_usleep",second,stack);
	stack->tp+=1;
}

void FAKE_rand(FklVM* exe,pthread_rwlock_t* pGClock)
{
	static int hasSrand=0;
	if(!hasSrand)
	{
		srand((unsigned)time(NULL));
		hasSrand=1;
	}
	VMstack* stack=exe->stack;
	VMvalue*  lim=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	VMrunnable* r=fklTopComStack(exe->rstack);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("btk.rand",TOOMANYARG,r,exe);
	if(lim&&!IS_IN32(lim))
		RAISE_BUILTIN_ERROR("btk.rand",WRONGARG,r,exe);
	SET_RETURN("FAKE_rand",MAKE_VM_IN32(rand()%((lim==NULL||!IS_IN32(lim))?RAND_MAX:GET_IN32(lim))),stack);
}

void FAKE_getTime(FklVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("btk.getTime",TOOMANYARG,fklTopComStack(exe->rstack),exe);
	time_t timer=time(NULL);
	struct tm* tblock=NULL;
	tblock=localtime(&timer);
	char* sec=fklIntToString(tblock->tm_sec);
	char* min=fklIntToString(tblock->tm_min);
	char* hour=fklIntToString(tblock->tm_hour);
	char* day=fklIntToString(tblock->tm_mday);
	char* mon=fklIntToString(tblock->tm_mon+1);
	char* year=fklIntToString(tblock->tm_year+1900);
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
	VMvalue* tmpVMvalue=fklNewVMvalue(STR,trueTime,exe->heap);
	SET_RETURN("FAKE_getTime",tmpVMvalue,stack);
}

void FAKE_removeFile(FklVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	VMvalue* name=fklPopVMstack(stack);
	VMrunnable* r=fklTopComStack(exe->rstack);
	if(!name)
		RAISE_BUILTIN_ERROR("btk.removeFile",TOOFEWARG,r,exe);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("btk.removeFile",TOOMANYARG,r,exe);
	if(!IS_STR(name))
		RAISE_BUILTIN_ERROR("btk.removeFile",WRONGARG,r,exe);
	SET_RETURN("FAKE_removeFile",MAKE_VM_IN32(remove(name->u.str)),stack);
}

void FAKE_setVMChanlBufferSize(FklVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	VMvalue* chan=fklPopVMstack(stack);
	VMvalue* size=fklPopVMstack(stack);
	VMrunnable* r=fklTopComStack(exe->rstack);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("btk.setVMChanlBufferSize",TOOMANYARG,r,exe);
	if(size==NULL||chan==NULL)
		RAISE_BUILTIN_ERROR("btk.setVMChanlBufferSize",TOOFEWARG,r,exe);
	if(!IS_IN32(size)||!IS_CHAN(chan))
		RAISE_BUILTIN_ERROR("btk.setVMChanlBufferSize",WRONGARG,r,exe);
	chan->u.chan->max=GET_IN32(size);
	SET_RETURN("FAKE_setVMChanlBufferSize",chan,stack);
}

void FAKE_isEndOfFile(FklVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	VMvalue* fp=fklPopVMstack(stack);
	VMrunnable* r=fklTopComStack(exe->rstack);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("btk.isEndOfFile",TOOMANYARG,r,exe);
	if(fp==NULL)
		RAISE_BUILTIN_ERROR("btk.isEndOfFile",TOOFEWARG,r,exe);
	if(!IS_FP(fp))
		RAISE_BUILTIN_ERROR("btk.isEndOfFile",WRONGARG,r,exe);
	SET_RETURN("FAKE_isEndOfFile",feof(fp->u.fp)
			?VM_TRUE
			:VM_NIL
			,stack);
}
#ifdef __cplusplus
}
#endif
