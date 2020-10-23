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
ANS_cptr* createTree(const char*,intpr*);
void exError(const ANS_cptr*,int,intpr*);
void printList(const ANS_cptr*,FILE*);
ANS_pair* newPair(int,ANS_pair*);
ANS_cptr* newCptr(int,ANS_pair*);
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
int destoryCptr(ANS_cptr*);
env* newEnv(env*);
void destroyEnv(env*);
int ANS_cptrcmp(const ANS_cptr*,const ANS_cptr*);
int deleteCptr(ANS_cptr*);
int copyCptr(ANS_cptr*,const ANS_cptr*);
void replace(ANS_cptr*,const ANS_cptr*);
ANS_cptr* nextCptr(const ANS_cptr*);
ANS_cptr* prevCptr(const ANS_cptr*);
ANS_cptr* getLast(const ANS_cptr*);
ANS_cptr* getFirst(const ANS_cptr*);
byteCode* createByteCode(unsigned int);
byteCode* codeCat(const byteCode*,const byteCode*);
void initCompEnv(compEnv*);
byteCode* copyByteCode(const byteCode*);
void freeByteCode(byteCode*);
void printByteCode(const byteCode*,FILE*);
#endif
