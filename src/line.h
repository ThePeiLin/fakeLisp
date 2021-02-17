#ifndef LINE_H
#define LINE_H
#include"fakedef.h"

LineNumberTable* newLineNumTable();
LineNumTabNode* newLineNumNode(int32_t id,int32_t fid,int32_t scp,int32_t cpc,int32_t line);
LineNumTabNode* addLineNumNode(LineNumTabNode*,LineNumberTable*);
LineNumTabNode* findLineNumTabNode(int32_t id,int32_t cp,LineNumberTable*);
void LineNumTabNodeAppd(LineNumTabNode*,LineNumTabNode*);
void freeLineNumTabNode(LineNumTabNode*);
void freeLineNumberTable(LineNumberTable*);
#endif
