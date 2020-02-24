#ifndef TOOL_H
#define TOOL_H
#include"fakedef.h"
#define OUTOFMEMORY 1
typedef struct RawString
{
	int len;
	char * str;
} rawString;
int power(int,int);
rawString getStringBetweenMarks(const char*);
void printRawString(const char*,FILE*);
char* floatToString(double);
double stringToFloat(const char*);
char* intToString(long);
long stringToInt(const char*);
void errors(int);
#endif
