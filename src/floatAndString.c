#include<stdlib.h>
#include<stdio.h>
#include<string.h>
char* floatToString(double num)
{
	int i;
	char NumString[sizeof(double)*2+3];
	sprintf(NumString,"%lf",num);
	int lenOfNum=strlen(NumString)+1;
	char* tmp=(char*)malloc(lenOfNum*sizeof(char));
	for(i=0;i<lenOfNum;i++)*(tmp+i)=NumString[i];
	return tmp;
}
double stringToFloat(char* str)
{
	double tmp;
	sscanf(str,"%lf",&tmp);
	return tmp;
}
