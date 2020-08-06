#ifndef TOOL_H
#define TOOL_H
#include<stdint.h>
#include"fakedef.h"
#define OUTOFMEMORY 1
#define WRONGENV 2
typedef struct RawString
{
	int len;
	char * str;
} rawString;
int isHexNum(const char*);
int isOctNum(const char*);
int isDouble(const char*);
int isNum(const char*);
char* getListFromFile(FILE*);
static char* subGetList(FILE*);
char* getStringFromList(const char*);
int power(int,int);
rawString getStringBetweenMarks(const char*,intpr*);
void printRawString(const char*,FILE*);
void printRawChar(char,FILE*);
char* doubleToString(double);
double stringToDouble(const char*);
char* intToString(long);
int64_t stringToInt(const char*);
char stringToChar(const char*);
void errors(int);
pair* newPair(int,pair*);
cptr* newCptr(int,pair*);
atom* newAtom(int type,const char*,pair*);
intpr* newIntpr(const char*,FILE*);
void freeIntpr(intpr*);
void freeAtom(atom*);
int destoryCptr(cptr*);
env* newEnv(env*);
void destroyEnv(env*);
int cptrcmp(const cptr*,const cptr*);
int deleteCptr(cptr*);
int copyCptr(cptr*,const cptr*);
void replace(cptr*,const cptr*);
cptr* nextCptr(const cptr*);
cptr* prevCptr(const cptr*);
#endif
