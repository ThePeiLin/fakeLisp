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
static char* getAtomFromFile(FILE*);
static char* getStringAfterMark(FILE*);
static char* subGetList(FILE*);
char* getStringFromList(const char*);
char* getStringAfterBackslash(const char*);
int power(int,int);
rawString getStringBetweenMarks(const char*,intpr*);
void printRawString(const char*,FILE*);
void printRawChar(char,FILE*);
char* doubleToString(double);
double stringToDouble(const char*);
char* intToString(long);
int32_t stringToInt(const char*);
char stringToChar(const char*);
void errors(int,const char*,int);
Cptr* createTree(const char*,intpr*);
void exError(const Cptr*,int,intpr*);
void printList(const Cptr*,FILE*);
ANS_pair* newPair(int,ANS_pair*);
Cptr* newCptr(int,ANS_pair*);
ANS_atom* newAtom(int type,const char*,ANS_pair*);
compDef* addCompDef(compEnv*,const char*);
rawproc* newRawProc(int32_t);
rawproc* addRawProc(byteCode*,intpr*);
compEnv* newCompEnv(compEnv*);
void destroyCompEnv(compEnv*);
compDef* findCompDef(const char*,compEnv*);
intpr* newIntpr(const char*,FILE*);
int isscript(const char*);
int iscode(const char*);
char* copyStr(const char*);
void freeIntpr(intpr*);
void freeAtom(ANS_atom*);
int destoryCptr(Cptr*);
env* newEnv(env*);
void destroyEnv(env*);
int Cptrcmp(const Cptr*,const Cptr*);
int deleteCptr(Cptr*);
int copyCptr(Cptr*,const Cptr*);
void replace(Cptr*,const Cptr*);
Cptr* nextCptr(const Cptr*);
Cptr* prevCptr(const Cptr*);
Cptr* getLast(const Cptr*);
Cptr* getFirst(const Cptr*);
byteCode* createByteCode(unsigned int);
byteCode* codeCat(const byteCode*,const byteCode*);
void initCompEnv(compEnv*);
byteCode* copyByteCode(const byteCode*);
void freeByteCode(byteCode*);
void printByteCode(const byteCode*,FILE*);
#endif
