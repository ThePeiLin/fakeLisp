#ifndef LINE_H
#define LINE_H
#include"fakedef.h"

LineNumberTable* newLineNumTable();
LineNumTabNode* newLineNumNode(int32_t scp,int32_t cpc,char* file);
#endif
