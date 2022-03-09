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
		RAISE_BUILTIN_ERROR("btk.getch",FKL_TOOMANYARG,fklTopPtrStack(exe->rstack),exe);
	SET_RETURN("FKL_getch",MAKE_VM_CHR(getch()),stack);
}

void FKL_sleep(FklVM* exe,pthread_rwlock_t* pGClock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	FklVMvalue* second=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(!second)
		RAISE_BUILTIN_ERROR("btk.sleep",FKL_TOOFEWARG,r,exe);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("btk.sleep",FKL_TOOMANYARG,r,exe);
	if(!IS_I32(second))
		RAISE_BUILTIN_ERROR("btk.sleep",FKL_WRONGARG,r,exe);
	fklReleaseSource(pGClock);
	SET_RETURN("FKL_sleep",MAKE_VM_I32(sleep(GET_I32(second))),stack);
	fklLockSource(pGClock);
	stack->tp+=1;
}

void FKL_usleep(FklVM* exe,pthread_rwlock_t* pGClock)
{
	FklVMstack* stack=exe->stack;
	FklVMvalue* second=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	if(!second)
		RAISE_BUILTIN_ERROR("btk.usleep",FKL_TOOFEWARG,r,exe);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("btk.usleep",FKL_TOOMANYARG,r,exe);
	if(!IS_I32(second))
		RAISE_BUILTIN_ERROR("btk.usleep",FKL_WRONGARG,r,exe);
	fklReleaseSource(pGClock);
#ifdef _WIN32
		Sleep(GET_I32(second));
#else
		usleep(GET_I32(second));
#endif
	fklLockSource(pGClock);
	SET_RETURN("FKL_usleep",second,stack);
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
		RAISE_BUILTIN_ERROR("btk.rand",FKL_TOOMANYARG,r,exe);
	if(lim&&!IS_I32(lim))
		RAISE_BUILTIN_ERROR("btk.rand",FKL_WRONGARG,r,exe);
	SET_RETURN("FKL_rand",MAKE_VM_I32(rand()%((lim==NULL||!IS_I32(lim))?RAND_MAX:GET_I32(lim))),stack);
}

void FKL_getTime(FklVM* exe,pthread_rwlock_t* pGClock)
{
	FklVMstack* stack=exe->stack;
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("btk.getTime",FKL_TOOMANYARG,fklTopPtrStack(exe->rstack),exe);
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
	FKL_ASSERT(trueTime,"FKL_getTime",__FILE__,__LINE__);
	sprintf(trueTime,"%s-%s-%s_%s_%s_%s",year,mon,day,hour,min,sec);
	free(sec);
	free(min);
	free(hour);
	free(day);
	free(mon);
	free(year);
	FklVMvalue* tmpVMvalue=fklNewVMvalue(FKL_STR,trueTime,exe->heap);
	SET_RETURN("FKL_getTime",tmpVMvalue,stack);
}

void FKL_removeFile(FklVM* exe,pthread_rwlock_t* pGClock)
{
	FklVMstack* stack=exe->stack;
	FklVMvalue* name=fklPopVMstack(stack);
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	if(!name)
		RAISE_BUILTIN_ERROR("btk.removeFile",FKL_TOOFEWARG,r,exe);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("btk.removeFile",FKL_TOOMANYARG,r,exe);
	if(!IS_STR(name))
		RAISE_BUILTIN_ERROR("btk.removeFile",FKL_WRONGARG,r,exe);
	SET_RETURN("FKL_removeFile",MAKE_VM_I32(remove(name->u.str)),stack);
}

void FKL_setVMChanlBufferSize(FklVM* exe,pthread_rwlock_t* pGClock)
{
	FklVMstack* stack=exe->stack;
	FklVMvalue* chan=fklPopVMstack(stack);
	FklVMvalue* size=fklPopVMstack(stack);
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("btk.setVMChanlBufferSize",FKL_TOOMANYARG,r,exe);
	if(size==NULL||chan==NULL)
		RAISE_BUILTIN_ERROR("btk.setVMChanlBufferSize",FKL_TOOFEWARG,r,exe);
	if(!IS_I32(size)||!IS_CHAN(chan))
		RAISE_BUILTIN_ERROR("btk.setVMChanlBufferSize",FKL_WRONGARG,r,exe);
	chan->u.chan->max=GET_I32(size);
	SET_RETURN("FKL_setVMChanlBufferSize",chan,stack);
}

void FKL_isEndOfFile(FklVM* exe,pthread_rwlock_t* pGClock)
{
	FklVMstack* stack=exe->stack;
	FklVMvalue* fp=fklPopVMstack(stack);
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("btk.isEndOfFile",FKL_TOOMANYARG,r,exe);
	if(fp==NULL)
		RAISE_BUILTIN_ERROR("btk.isEndOfFile",FKL_TOOFEWARG,r,exe);
	if(!IS_FP(fp))
		RAISE_BUILTIN_ERROR("btk.isEndOfFile",FKL_WRONGARG,r,exe);
	SET_RETURN("FKL_isEndOfFile",feof(fp->u.fp)
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,stack);
}
#ifdef __cplusplus
}
#endif
