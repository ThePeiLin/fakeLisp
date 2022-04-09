#include<fakeLisp/vm.h>
#include<fakeLisp/utils.h>
#include<stdio.h>
#include<stdlib.h>
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
void FKL_getch(FklVM* exe,pthread_rwlock_t* pGClock)
{
	FklVMstack* stack=exe->stack;
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("btk.getch",FKL_TOOMANYARG,fklTopPtrStack(exe->rstack),exe);
	FKL_SET_RETURN("FKL_getch",FKL_MAKE_VM_I8(getch()),stack);
}

void FKL_sleep(FklVM* exe,pthread_rwlock_t* pGClock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	FklVMvalue* second=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(!second)
		FKL_RAISE_BUILTIN_ERROR("btk.sleep",FKL_TOOFEWARG,r,exe);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("btk.sleep",FKL_TOOMANYARG,r,exe);
	if(!FKL_IS_I32(second))
		FKL_RAISE_BUILTIN_ERROR("btk.sleep",FKL_WRONGARG,r,exe);
	fklReleaseGC(pGClock);
	FKL_SET_RETURN("FKL_sleep",FKL_MAKE_VM_I32(sleep(FKL_GET_I32(second))),stack);
	fklLockGC(pGClock);
	stack->tp+=1;
}

void FKL_usleep(FklVM* exe,pthread_rwlock_t* pGClock)
{
	FklVMstack* stack=exe->stack;
	FklVMvalue* second=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	if(!second)
		FKL_RAISE_BUILTIN_ERROR("btk.usleep",FKL_TOOFEWARG,r,exe);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("btk.usleep",FKL_TOOMANYARG,r,exe);
	if(!FKL_IS_I32(second))
		FKL_RAISE_BUILTIN_ERROR("btk.usleep",FKL_WRONGARG,r,exe);
	fklReleaseGC(pGClock);
#ifdef _WIN32
		Sleep(FKL_GET_I32(second));
#else
		usleep(FKL_GET_I32(second));
#endif
	fklLockGC(pGClock);
	FKL_SET_RETURN("FKL_usleep",second,stack);
	stack->tp+=1;
}

void FKL_rand(FklVM* exe,pthread_rwlock_t* pGClock)
{
	static int hasSrand=0;
	if(!hasSrand)
	{
		srand((unsigned)time(NULL));
		hasSrand=1;
	}
	FklVMstack* stack=exe->stack;
	FklVMvalue*  lim=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("btk.rand",FKL_TOOMANYARG,r,exe);
	if(lim&&!FKL_IS_I32(lim))
		FKL_RAISE_BUILTIN_ERROR("btk.rand",FKL_WRONGARG,r,exe);
	FKL_SET_RETURN("FKL_rand",FKL_MAKE_VM_I32(rand()%((lim==NULL||!FKL_IS_I32(lim))?RAND_MAX:FKL_GET_I32(lim))),stack);
}

void FKL_getTime(FklVM* exe,pthread_rwlock_t* pGClock)
{
	FklVMstack* stack=exe->stack;
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("btk.getTime",FKL_TOOMANYARG,fklTopPtrStack(exe->rstack),exe);
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
	FKL_ASSERT(trueTime,"FKL_getTime");
	sprintf(trueTime,"%s-%s-%s_%s_%s_%s",year,mon,day,hour,min,sec);
	free(sec);
	free(min);
	free(hour);
	free(day);
	free(mon);
	free(year);
	FklVMvalue* tmpVMvalue=fklNewVMvalue(FKL_STR,trueTime,exe->heap);
	FKL_SET_RETURN("FKL_getTime",tmpVMvalue,stack);
}

void FKL_removeFile(FklVM* exe,pthread_rwlock_t* pGClock)
{
	FklVMstack* stack=exe->stack;
	FklVMvalue* name=fklPopVMstack(stack);
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	if(!name)
		FKL_RAISE_BUILTIN_ERROR("btk.removeFile",FKL_TOOFEWARG,r,exe);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("btk.removeFile",FKL_TOOMANYARG,r,exe);
	if(!FKL_IS_STR(name))
		FKL_RAISE_BUILTIN_ERROR("btk.removeFile",FKL_WRONGARG,r,exe);
	FKL_SET_RETURN("FKL_removeFile",FKL_MAKE_VM_I32(remove(name->u.str)),stack);
}

void FKL_setVMChanlBufferSize(FklVM* exe,pthread_rwlock_t* pGClock)
{
	FklVMstack* stack=exe->stack;
	FklVMvalue* chan=fklPopVMstack(stack);
	FklVMvalue* size=fklPopVMstack(stack);
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("btk.setVMChanlBufferSize",FKL_TOOMANYARG,r,exe);
	if(size==NULL||chan==NULL)
		FKL_RAISE_BUILTIN_ERROR("btk.setVMChanlBufferSize",FKL_TOOFEWARG,r,exe);
	if(!FKL_IS_I32(size)||!FKL_IS_CHAN(chan))
		FKL_RAISE_BUILTIN_ERROR("btk.setVMChanlBufferSize",FKL_WRONGARG,r,exe);
	chan->u.chan->max=FKL_GET_I32(size);
	FKL_SET_RETURN("FKL_setVMChanlBufferSize",chan,stack);
}

void FKL_isEndOfFile(FklVM* exe,pthread_rwlock_t* pGClock)
{
	FklVMstack* stack=exe->stack;
	FklVMvalue* fp=fklPopVMstack(stack);
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("btk.isEndOfFile",FKL_TOOMANYARG,r,exe);
	if(fp==NULL)
		FKL_RAISE_BUILTIN_ERROR("btk.isEndOfFile",FKL_TOOFEWARG,r,exe);
	if(!FKL_IS_FP(fp))
		FKL_RAISE_BUILTIN_ERROR("btk.isEndOfFile",FKL_WRONGARG,r,exe);
	FKL_SET_RETURN("FKL_isEndOfFile",feof(fp->u.fp->fp)
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,stack);
}
#ifdef __cplusplus
}
#endif
