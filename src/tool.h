#ifndef TOOL_H
#define TOOL_H
#include<stdint.h>
#include"fakedef.h"
#define OUTOFMEMORY 1
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
int32_t stringToInt(const char*);
char stringToChar(const char*);
void errors(int);
cptr* createTree(const char*,intpr*);
void exError(const cptr*,int,intpr*);
void printList(const cptr*,FILE*);
pair* newPair(int,pair*);
cptr* newCptr(int,pair*);
atom* newAtom(int type,const char*,pair*);
compDef* addCompDef(compEnv*,const char*);
rawproc* newRawProc(int32_t);
rawproc* addRawProc(byteCode*,intpr*);
compEnv* newCompEnv(compEnv*);
compDef* findCompDef(const char*,compEnv*);
void destroyCompEnv(compEnv*);
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
byteCode* createByteCode(unsigned int);
byteCode* codeCat(const byteCode*,const byteCode*);
void initCompEnv(compEnv*);
byteCode* copyByteCode(const byteCode*);
void freeByteCode(byteCode*);
#endif
