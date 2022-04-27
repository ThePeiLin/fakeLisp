#include<fakeLisp/fklni.h>
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
#define ARGL FklVM* exe
void FKL_getch(ARGL)
{
	FKL_NI_BEGIN(exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("btk.getch",FKL_TOOMANYARG,fklTopPtrStack(exe->rstack),exe);
	fklNiReturn(FKL_MAKE_VM_CHR(getch()),&ap,stack);
	fklNiEnd(&ap,stack);
}

void FKL_sleep(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	FklVMvalue* second=fklNiGetArg(&ap,stack);
	if(!second)
		FKL_RAISE_BUILTIN_ERROR("btk.sleep",FKL_TOOFEWARG,r,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("btk.sleep",FKL_TOOMANYARG,r,exe);
	if(!FKL_IS_I32(second))
		FKL_RAISE_BUILTIN_ERROR("btk.sleep",FKL_WRONGARG,r,exe);
	fklNiReturn(FKL_MAKE_VM_I32(sleep(FKL_GET_I32(second))),&ap,stack);
	fklNiEnd(&ap,stack);
}

void FKL_usleep(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* second=fklNiGetArg(&ap,stack);
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	if(!second)
		FKL_RAISE_BUILTIN_ERROR("btk.usleep",FKL_TOOFEWARG,r,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("btk.usleep",FKL_TOOMANYARG,r,exe);
	if(!FKL_IS_I32(second))
		FKL_RAISE_BUILTIN_ERROR("btk.usleep",FKL_WRONGARG,r,exe);
#ifdef _WIN32
		Sleep(FKL_GET_I32(second));
#else
		usleep(FKL_GET_I32(second));
#endif
	fklNiReturn(second,&ap,stack);
	fklNiEnd(&ap,stack);
}

void FKL_srand(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	FklVMvalue* s=fklNiGetArg(&ap,stack);
    if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("btk.srand",FKL_TOOMANYARG,r,exe);
	if(!fklIsInt(s))
		FKL_RAISE_BUILTIN_ERROR("btk.srand",FKL_WRONGARG,r,exe);
    srand(fklGetInt(s));
    fklNiReturn(s,&ap,stack);
	fklNiEnd(&ap,stack);
}

void FKL_rand(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue*  lim=fklNiGetArg(&ap,stack);
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("btk.rand",FKL_TOOMANYARG,r,exe);
	if(lim&&!FKL_IS_I32(lim))
		FKL_RAISE_BUILTIN_ERROR("btk.rand",FKL_WRONGARG,r,exe);
	fklNiReturn(FKL_MAKE_VM_I32(rand()%((lim==NULL||!FKL_IS_I32(lim))?RAND_MAX:FKL_GET_I32(lim))),&ap,stack);
	fklNiEnd(&ap,stack);
}

void FKL_getTime(ARGL)
{
	FKL_NI_BEGIN(exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("btk.getTime",FKL_TOOMANYARG,fklTopPtrStack(exe->rstack),exe);
	time_t timer=time(NULL);
	struct tm* tblock=NULL;
	tblock=localtime(&timer);
	char sec[4]={0};
	char min[4]={0};
	char hour[4]={0};
	char day[4]={0};
	char mon[4]={0};
	char year[10]={0};
	snprintf(sec,4,"%u",tblock->tm_sec);
	snprintf(min,4,"%u",tblock->tm_min);
	snprintf(hour,4,"%u",tblock->tm_hour);
	snprintf(day,4,"%u",tblock->tm_mday);
	snprintf(mon,4,"%u",tblock->tm_mon+1);
	snprintf(year,10,"%u",tblock->tm_year+1900);
	uint32_t timeLen=strlen(year)+strlen(mon)+strlen(day)+strlen(hour)+strlen(min)+strlen(sec)+5+1;
	char* trueTime=(char*)malloc(sizeof(char)*timeLen);
	FKL_ASSERT(trueTime,__func__);
	sprintf(trueTime,"%s-%s-%s_%s_%s_%s",year,mon,day,hour,min,sec);
	FklVMstr* str=fklNewVMstr(timeLen-1,trueTime);
	FklVMvalue* tmpVMvalue=fklNiNewVMvalue(FKL_STR,str,stack,exe->heap);
	free(trueTime);
	fklNiReturn(tmpVMvalue,&ap,stack);
	fklNiEnd(&ap,stack);
}

void FKL_removeFile(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* name=fklPopVMstack(stack);
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	if(!name)
		FKL_RAISE_BUILTIN_ERROR("btk.removeFile",FKL_TOOFEWARG,r,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("btk.removeFile",FKL_TOOMANYARG,r,exe);
	if(!FKL_IS_STR(name))
		FKL_RAISE_BUILTIN_ERROR("btk.removeFile",FKL_WRONGARG,r,exe);
	char* str=fklVMstrToCstr(name->u.str);
	fklNiReturn(FKL_MAKE_VM_I32(remove(str)),&ap,stack);
	fklNiEnd(&ap,stack);
	free(name);
}

void FKL_setChanlBufferSize(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* chan=fklPopVMstack(stack);
	FklVMvalue* size=fklPopVMstack(stack);
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("btk.setChanlBufferSize",FKL_TOOMANYARG,r,exe);
	if(size==NULL||chan==NULL)
		FKL_RAISE_BUILTIN_ERROR("btk.setChanlBufferSize",FKL_TOOFEWARG,r,exe);
	if(!FKL_IS_I32(size)||!FKL_IS_CHAN(chan))
		FKL_RAISE_BUILTIN_ERROR("btk.setChanlBufferSize",FKL_WRONGARG,r,exe);
	chan->u.chan->max=FKL_GET_I32(size);
	fklNiReturn(chan,&ap,stack);
	fklNiEnd(&ap,stack);
}

void FKL_time(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("btk.time",FKL_TOOMANYARG,r,exe);
	fklNiReturn(fklMakeVMint((int64_t)time(NULL),stack,exe->heap),&ap,stack);
	fklNiEnd(&ap,stack);
}

#ifdef __cplusplus
}
#endif
