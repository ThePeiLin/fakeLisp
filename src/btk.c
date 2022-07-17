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
#include<unistd.h>
#endif
#include<time.h>
#include<string.h>
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
void btk_getch(ARGL)
{
	FKL_NI_BEGIN(exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("btk.getch",FKL_TOOMANYARG,exe->rhead,exe);
	fklNiReturn(FKL_MAKE_VM_CHR(getch()),&ap,stack);
	fklNiEnd(&ap,stack);
}

void btk_sleep(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* r=exe->rhead;
	FklVMvalue* second=fklNiGetArg(&ap,stack);
	if(!second)
		FKL_RAISE_BUILTIN_ERROR_CSTR("btk.sleep",FKL_TOOFEWARG,r,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("btk.sleep",FKL_TOOMANYARG,r,exe);
	FKL_NI_CHECK_TYPE(second,fklIsInt,"btk.sleep",r,exe);
	fklNiReturn(FKL_MAKE_VM_I32(sleep(fklGetInt(second))),&ap,stack);
	fklNiEnd(&ap,stack);
}

void btk_usleep(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* second=fklNiGetArg(&ap,stack);
	FklVMrunnable* r=exe->rhead;
	if(!second)
		FKL_RAISE_BUILTIN_ERROR_CSTR("btk.usleep",FKL_TOOFEWARG,r,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("btk.usleep",FKL_TOOMANYARG,r,exe);
	FKL_NI_CHECK_TYPE(second,fklIsInt,"btk.usleep",r,exe);
#ifdef _WIN32
		Sleep(fklGetInt(second));
#else
		usleep(fklGetInt(second));
#endif
	fklNiReturn(second,&ap,stack);
	fklNiEnd(&ap,stack);
}

void btk_srand(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* r=exe->rhead;
	FklVMvalue* s=fklNiGetArg(&ap,stack);
    if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("btk.srand",FKL_TOOMANYARG,r,exe);
	FKL_NI_CHECK_TYPE(s,fklIsInt,"btk.srand",r,exe);
    srand(fklGetInt(s));
    fklNiReturn(s,&ap,stack);
	fklNiEnd(&ap,stack);
}

void btk_rand(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue*  lim=fklNiGetArg(&ap,stack);
	FklVMrunnable* r=exe->rhead;
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("btk.rand",FKL_TOOMANYARG,r,exe);
	if(lim&&!fklIsInt(lim))
		FKL_RAISE_BUILTIN_ERROR_CSTR("btk.rand",FKL_WRONGARG,r,exe);
	fklNiReturn(FKL_MAKE_VM_I32(rand()%((lim==NULL)?RAND_MAX:fklGetInt(lim))),&ap,stack);
	fklNiEnd(&ap,stack);
}

void btk_get_time(ARGL)
{
	FKL_NI_BEGIN(exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("btk.get_time",FKL_TOOMANYARG,exe->rhead,exe);
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
	FKL_ASSERT(trueTime);
	sprintf(trueTime,"%s-%s-%s_%s_%s_%s",year,mon,day,hour,min,sec);
	FklString* str=fklNewString(timeLen-1,trueTime);
	FklVMvalue* tmpVMvalue=fklNewVMvalueToStack(FKL_STR,str,stack,exe->heap);
	free(trueTime);
	fklNiReturn(tmpVMvalue,&ap,stack);
	fklNiEnd(&ap,stack);
}

void btk_remove_file(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* name=fklPopVMstack(stack);
	FklVMrunnable* r=exe->rhead;
	if(!name)
		FKL_RAISE_BUILTIN_ERROR_CSTR("btk.remove_file",FKL_TOOFEWARG,r,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("btk.remove_file",FKL_TOOMANYARG,r,exe);
	FKL_NI_CHECK_TYPE(name,FKL_IS_STR,"btk.remove_file",r,exe);
	char* str=fklStringToCstr(name->u.str);
	fklNiReturn(FKL_MAKE_VM_I32(remove(str)),&ap,stack);
	fklNiEnd(&ap,stack);
	free(name);
}

void btk_time(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* r=exe->rhead;
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("btk.time",FKL_TOOMANYARG,r,exe);
	fklNiReturn(fklMakeVMint((int64_t)time(NULL),stack,exe->heap),&ap,stack);
	fklNiEnd(&ap,stack);
}

#ifdef __cplusplus
}
#endif
