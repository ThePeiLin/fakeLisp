#ifndef _FLOAT_AND_STRING_H_
#define _FLOAT_AND_STRING_H_
typedef struct RawString
{
	int len;
	char * str;
} rawString;
int power(int,int);
rawString getStringBetweenMarks(const char*);
char* floatToString(double);
double stringToFloat(const char*);
char* intToString(long);
long stringToInt(const char*);
#endif
