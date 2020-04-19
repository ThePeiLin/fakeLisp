#ifndef TOOL_H
#define TOOL_H
#include"fakedef.h"
#define OUTOFMEMORY 1
#define WRONGENV 2
typedef struct RawString
{
	int len;
	char * str;
} rawString;
char* getListFromFile(FILE*);
static char* subGetList(FILE*);
char* getStringFromList(const char*);
int power(int,int);
rawString getStringBetweenMarks(const char*);
void printRawString(const char*,FILE*);
char* floatToString(double);
double stringToFloat(const char*);
char* intToString(long);
long stringToInt(const char*);
void errors(int);
cell* createCell(cell*);
cptr* createCptr(cell*);
atom* createAtom(int type,const char*,cell*);
int destoryCptr(cptr*);
int cptrcmp(const cptr*,const cptr*);
int deleteCptr(cptr*);
int copyCptr(cptr*,const cptr*);
void replace(cptr*,const cptr*);
#endif
