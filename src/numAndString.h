#ifndef _FLOAT_AND_STRING_H_
#define _FLOAT_AND_STRING_H_
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
#endif
